// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_script_tracker.h"

#include <algorithm>
#include <map>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/url_loader_factory_manager.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/constants.h"
#include "extensions/common/content_script_injection_url_getter.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/user_script.h"

namespace extensions {

namespace {

// Helper for lazily attaching ExtensionIdSet to a RenderProcessHost.  Used to
// track the set of extensions which have injected a JS content script into a
// RenderProcessHost.
//
// We track content script injection per-RenderProcessHost:
// 1. This matches the real security boundary that Site Isolation uses (the
//    boundary of OS processes) and follows the precedent of
//    content::ChildProcessSecurityPolicy.
// 2. This robustly handles initial empty documents (see the *InitialEmptyDoc*
//    tests in //content_script_tracker_browsertest.cc) and isn't impacted
//    by ReadyToCommit races associated with RenderDocumentHostUserData.
// For more information see:
// https://docs.google.com/document/d/1MFprp2ss2r9RNamJ7Jxva1bvRZvec3rzGceDGoJ6vW0/edit#
class RenderProcessHostUserData : public base::SupportsUserData::Data {
 public:
  static const RenderProcessHostUserData* Get(
      const content::RenderProcessHost& process) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    return static_cast<RenderProcessHostUserData*>(
        process.GetUserData(kUserDataKey));
  }

  static RenderProcessHostUserData& GetOrCreate(
      content::RenderProcessHost& process) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    auto* self = static_cast<RenderProcessHostUserData*>(
        process.GetUserData(kUserDataKey));

    if (!self) {
      // Create a new RenderProcessHostUserData if needed.  The ownership is
      // passed to the `process` (i.e. the new RenderProcessHostUserData will be
      // destroyed at the same time as the `process` - this is why we don't need
      // to purge or destroy the set from within ContentScriptTracker).
      auto owned_self = base::WrapUnique(new RenderProcessHostUserData);
      self = owned_self.get();
      process.SetUserData(kUserDataKey, std::move(owned_self));
    }

    DCHECK(self);
    return *self;
  }

  // base::SupportsUserData::Data override:
  ~RenderProcessHostUserData() override = default;

  bool HasContentScript(const ExtensionId& extension_id) const {
    return base::Contains(content_scripts_, extension_id);
  }

  void AddContentScript(const ExtensionId& extension_id) {
    content_scripts_.insert(extension_id);
  }

  void AddFrame(content::RenderFrameHost* frame) { frames_.insert(frame); }
  void RemoveFrame(content::RenderFrameHost* frame) { frames_.erase(frame); }
  const std::set<content::RenderFrameHost*>& frames() const { return frames_; }

 private:
  RenderProcessHostUserData() = default;

  static const char* kUserDataKey;

  // Set of extensions ids that have *ever* injected a content script into this
  // particular renderer process.  This is the core data maintained by the
  // ContentScriptTracker.
  ExtensionIdSet content_scripts_;

  // Set of frames that are *currently* hosted in this particular renderer
  // process.  This is mostly used just to get GetLastCommittedURL of these
  // frames so that when a new extension is loaded, then ContentScriptTracker
  // can know where content scripts may be injected.
  std::set<content::RenderFrameHost*> frames_;
};

const char* RenderProcessHostUserData::kUserDataKey =
    "ContentScriptTracker's data";

class RenderFrameHostAdapter
    : public ContentScriptInjectionUrlGetter::FrameAdapter {
 public:
  explicit RenderFrameHostAdapter(content::RenderFrameHost* frame)
      : frame_(frame) {}

  ~RenderFrameHostAdapter() override = default;

  std::unique_ptr<FrameAdapter> Clone() const override {
    return std::make_unique<RenderFrameHostAdapter>(frame_);
  }

  std::unique_ptr<FrameAdapter> GetLocalParentOrOpener() const override {
    content::RenderFrameHost* parent_or_opener = frame_->GetParent();
    if (!parent_or_opener) {
      parent_or_opener =
          content::WebContents::FromRenderFrameHost(frame_)->GetOpener();
    }
    if (!parent_or_opener)
      return nullptr;

    // Renderer-side WebLocalFrameAdapter only considers local frames.
    // Comparing processes is robust way to replicate such renderer-side checks,
    // because out caller (DoesContentScriptMatch) accepts false positives.
    // This comparison might be less accurate (e.g. give more false positives)
    // than SiteInstance comparison, but comparing processes should be robust
    // and stable as SiteInstanceGroup refactoring proceeds.
    if (parent_or_opener->GetProcess() != frame_->GetProcess())
      return nullptr;

    return std::make_unique<RenderFrameHostAdapter>(parent_or_opener);
  }

  GURL GetUrl() const override { return frame_->GetLastCommittedURL(); }

  url::Origin GetOrigin() const override {
    return frame_->GetLastCommittedOrigin();
  }

  bool CanAccess(const url::Origin& target) const override {
    // CanAccess should not be called - see the comment for
    // kAllowInaccessibleParents in GetEffectiveDocumentURL below.
    NOTREACHED();
    return true;
  }

  bool CanAccess(const FrameAdapter& target) const override {
    // CanAccess should not be called - see the comment for
    // kAllowInaccessibleParents in GetEffectiveDocumentURL below.
    NOTREACHED();
    return true;
  }

  uintptr_t GetId() const override { return frame_->GetRoutingID(); }

 private:
  content::RenderFrameHost* const frame_;
};

// This function approximates ScriptContext::GetEffectiveDocumentURLForInjection
// from the renderer side.
GURL GetEffectiveDocumentURL(
    content::RenderFrameHost* frame,
    const GURL& document_url,
    MatchOriginAsFallbackBehavior match_origin_as_fallback) {
  // This is a simplification to avoid calling
  // `RenderFrameHostAdapter::CanAccess` which is unable to replicate all of
  // WebSecurityOrigin::CanAccess checks (e.g. universal access or file
  // exceptions tracked on the renderer side).  This is okay, because our only
  // caller (DoesContentScriptMatch()) expects false positives.
  constexpr bool kAllowInaccessibleParents = true;

  return ContentScriptInjectionUrlGetter::Get(
      RenderFrameHostAdapter(frame), document_url, match_origin_as_fallback,
      kAllowInaccessibleParents);
}

// If `user_script` will inject JavaScript content script into the target of
// `navigation`, then DoesContentScriptMatch returns true.  Otherwise it may
// return either true or false.  Note that this function ignores CSS content
// scripts.
//
// This function approximates a subset of checks from
// UserScriptSet::GetInjectionForScript (which runs in the renderer process).
// Unlike the renderer version, the code below doesn't consider ability to
// create an injection host, nor the results of
// ScriptInjector::CanExecuteOnFrame, nor the path of `url_patterns`.
// Additionally the `effective_url` calculations are also only an approximation.
// This is okay, because the top-level doc comment for ContentScriptTracker
// documents that false positives are expected and why they are okay.
bool DoesContentScriptMatch(const UserScript& user_script,
                            content::RenderFrameHost* frame,
                            const GURL& url) {
  // ContentScriptTracker only needs to track Javascript content scripts (e.g.
  // doesn't track CSS-only injections).
  if (user_script.js_scripts().empty())
    return false;

  GURL effective_url = GetEffectiveDocumentURL(
      frame, url, user_script.match_origin_as_fallback());
  return user_script.url_patterns().MatchesSecurityOrigin(effective_url);
}

void HandleProgrammaticContentScriptInjection(
    base::PassKey<ContentScriptTracker> pass_key,
    content::RenderFrameHost* frame,
    const Extension& extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Store `extension.id()` in `process_data`.  ContentScriptTracker never
  // removes entries from this set - once a renderer process gains an ability to
  // talk on behalf of a content script, it retains this ability forever.  Note
  // that the `process_data` will be destroyed together with the
  // RenderProcessHost (see also a comment inside
  // RenderProcessHostUserData::GetOrCreate).
  auto& process_data =
      RenderProcessHostUserData::GetOrCreate(*frame->GetProcess());
  process_data.AddContentScript(extension.id());

  URLLoaderFactoryManager::WillProgrammaticallyInjectContentScript(
      pass_key, frame, extension);
}

// If `extension`'s manifest declares that it may inject JavaScript content
// script into the `frame` / `url`, then DoContentScriptsMatch returns true.
// Otherwise it may return either true or false.
//
// Note that the `url` might be either 1) the last committed URL of `frame` or
// 2) the target of a ReadyToCommit navigation in `frame`.
//
// Note that this method ignores CSS content scripts.
bool DoContentScriptsMatch(const Extension& extension,
                           content::RenderFrameHost* frame,
                           const GURL& url) {
  const UserScriptList& manifest_scripts =
      ContentScriptsInfo::GetContentScripts(&extension);

  auto does_script_match = [frame,
                            &url](const std::unique_ptr<UserScript>& script) {
    return DoesContentScriptMatch(*script, frame, url);
  };

  if (base::ranges::any_of(manifest_scripts.begin(), manifest_scripts.end(),
                           does_script_match)) {
    return true;
  }

  // `manager` can be null for some unit tests which do not initialize the
  // ExtensionSystem.
  UserScriptManager* manager =
      ExtensionSystem::Get(frame->GetProcess()->GetBrowserContext())
          ->user_script_manager();
  if (manager) {
    const UserScriptList& dynamic_scripts =
        manager->GetUserScriptLoaderForExtension(extension.id())
            ->GetLoadedDynamicScripts();
    return base::ranges::any_of(dynamic_scripts.begin(), dynamic_scripts.end(),
                                does_script_match);
  }

  return false;
}

std::vector<const Extension*> GetExtensionsInjectingContentScripts(
    content::NavigationHandle* navigation) {
  content::RenderFrameHost* frame = navigation->GetRenderFrameHost();
  const GURL& url = navigation->GetURL();

  std::vector<const Extension*> extensions_injecting_content_scripts;
  const ExtensionRegistry* registry =
      ExtensionRegistry::Get(frame->GetProcess()->GetBrowserContext());
  DCHECK(registry);  // This method shouldn't be called during shutdown.
  for (const auto& it : registry->enabled_extensions()) {
    const Extension& extension = *it;
    if (!DoContentScriptsMatch(extension, frame, url))
      continue;

    extensions_injecting_content_scripts.push_back(&extension);
  }

  return extensions_injecting_content_scripts;
}

const Extension* FindExtensionByHostId(content::BrowserContext* browser_context,
                                       const mojom::HostID& host_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  switch (host_id.type) {
    case mojom::HostID::HostType::kWebUi:
      // ContentScriptTracker only tracks extensions.
      return nullptr;
    case mojom::HostID::HostType::kExtensions:
      break;
  }

  const ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context);
  DCHECK(registry);  // WillExecuteCode and WillUpdateContentScriptsInRenderer
                     // shouldn't happen during shutdown.

  const Extension* extension =
      registry->enabled_extensions().GetByID(host_id.id);

  return extension;
}

}  // namespace

// static
bool ContentScriptTracker::DidProcessRunContentScriptFromExtension(
    const content::RenderProcessHost& process,
    const ExtensionId& extension_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!extension_id.empty());

  // Check if we've been notified about the content script injection via
  // ReadyToCommitNavigation or WillExecuteCode methods.
  const auto* process_data = RenderProcessHostUserData::Get(process);
  if (!process_data)
    return false;

  return process_data->HasContentScript(extension_id);
}

// static
void ContentScriptTracker::ReadyToCommitNavigation(
    base::PassKey<ExtensionWebContentsObserver> pass_key,
    content::NavigationHandle* navigation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Store `extensions_injecting_content_scripts` in
  // `process_data`.  ContentScriptTracker never removes entries
  // from this set - once a renderer process gains an ability to talk on behalf
  // of a content script, it retains this ability forever.  Note that the
  // `process_data`
  // will be destroyed together with the RenderProcessHost (see also a comment
  // inside RenderProcessHostUserData::GetOrCreate).
  std::vector<const Extension*> extensions_injecting_content_scripts =
      GetExtensionsInjectingContentScripts(navigation);
  auto& process_data = RenderProcessHostUserData::GetOrCreate(
      *navigation->GetRenderFrameHost()->GetProcess());
  for (const Extension* extension : extensions_injecting_content_scripts)
    process_data.AddContentScript(extension->id());

  URLLoaderFactoryManager::WillInjectContentScriptsWhenNavigationCommits(
      base::PassKey<ContentScriptTracker>(), navigation,
      extensions_injecting_content_scripts);
}

// static
void ContentScriptTracker::RenderFrameCreated(
    base::PassKey<ExtensionWebContentsObserver> pass_key,
    content::RenderFrameHost* frame) {
  auto& process_data =
      RenderProcessHostUserData::GetOrCreate(*frame->GetProcess());
  process_data.AddFrame(frame);
}

// static
void ContentScriptTracker::RenderFrameDeleted(
    base::PassKey<ExtensionWebContentsObserver> pass_key,
    content::RenderFrameHost* frame) {
  auto& process_data =
      RenderProcessHostUserData::GetOrCreate(*frame->GetProcess());
  process_data.RemoveFrame(frame);
}

// static
void ContentScriptTracker::ReadyToCommitNavigationWithGuestViewContentScripts(
    base::PassKey<WebViewGuest> pass_key,
    content::WebContents* outer_web_contents,
    content::NavigationHandle* navigation) {
  // Only Chrome Apps and Extensions can inject content scripts.  OTOH,
  // <webview> tag can be used by Chrome Apps and/or WebUI pages.  Do nothing
  // for WebUI and only continue when the `outer_web_contents` is a Chrome App.
  url::Origin outer_origin =
      outer_web_contents->GetMainFrame()->GetLastCommittedOrigin();
  if (outer_origin.scheme() != kExtensionScheme)
    return;
  ExtensionId app_id = outer_origin.host();

  // Store `extension_id` in `content_scripts_for_process`.
  // ContentScriptTracker never removes entries from this set - once a renderer
  // process gains an ability to talk on behalf of a content script, it retains
  // this ability forever.  Note that the set will be destroyed together with
  // the RenderProcessHost (see also a comment inside
  // ContentScriptsSet::GetOrCreate).
  //
  // TODO(lukasza): false positives in ContentScriptTracker are okay, but
  // ideally we would only populate `content_scripts_for_process` below if
  // content script URL patterns actually match the target URL of the
  // navigation.
  content::RenderProcessHost* inner_process =
      navigation->GetRenderFrameHost()->GetProcess();
  auto& process_data = RenderProcessHostUserData::GetOrCreate(*inner_process);
  process_data.AddContentScript(app_id);
}

// static
void ContentScriptTracker::WillExecuteCode(
    base::PassKey<ScriptExecutor> pass_key,
    content::RenderFrameHost* frame,
    const mojom::HostID& host_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const Extension* extension =
      FindExtensionByHostId(frame->GetProcess()->GetBrowserContext(), host_id);
  if (!extension)
    return;

  HandleProgrammaticContentScriptInjection(PassKey(), frame, *extension);
}

// static
void ContentScriptTracker::WillExecuteCode(
    base::PassKey<RequestContentScript> pass_key,
    content::RenderFrameHost* frame,
    const Extension& extension) {
  HandleProgrammaticContentScriptInjection(PassKey(), frame, extension);
}

// static
void ContentScriptTracker::WillUpdateContentScriptsInRenderer(
    base::PassKey<UserScriptLoader> pass_key,
    const mojom::HostID& host_id,
    content::RenderProcessHost& process) {
  const Extension* extension =
      FindExtensionByHostId(process.GetBrowserContext(), host_id);
  if (!extension)
    return;

  auto& process_data = RenderProcessHostUserData::GetOrCreate(process);
  const std::set<content::RenderFrameHost*>& frames_in_process =
      process_data.frames();
  bool any_frame_matches_content_scripts =
      std::any_of(frames_in_process.begin(), frames_in_process.end(),
                  [extension](content::RenderFrameHost* frame) {
                    return DoContentScriptsMatch(*extension, frame,
                                                 frame->GetLastCommittedURL());
                  });
  if (any_frame_matches_content_scripts)
    process_data.AddContentScript(extension->id());
}

// static
bool ContentScriptTracker::DoContentScriptsMatchForTesting(
    const Extension& extension,
    content::RenderFrameHost* frame,
    const GURL& url) {
  return DoContentScriptsMatch(extension, frame, url);
}

}  // namespace extensions
