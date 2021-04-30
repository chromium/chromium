// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_script_tracker.h"

#include <algorithm>
#include <map>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/url_loader_factory_manager.h"
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
class ContentScriptsSet : public base::SupportsUserData::Data {
 public:
  static const ExtensionIdSet* Get(const content::RenderProcessHost& process) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    auto* self =
        static_cast<ContentScriptsSet*>(process.GetUserData(kUserDataKey));
    return self ? &self->content_scripts_ : nullptr;
  }

  static ExtensionIdSet& GetOrCreate(content::RenderProcessHost& process) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    auto* self =
        static_cast<ContentScriptsSet*>(process.GetUserData(kUserDataKey));

    if (!self) {
      auto owned_self = base::WrapUnique(new ContentScriptsSet);
      self = owned_self.get();
      process.SetUserData(kUserDataKey, std::move(owned_self));
    }

    return self->content_scripts_;
  }

  // base::SupportsUserData::Data override:
  ~ContentScriptsSet() override = default;

 private:
  ContentScriptsSet() = default;

  static const char* kUserDataKey;
  ExtensionIdSet content_scripts_;
};

const char* ContentScriptsSet::kUserDataKey = "ContentScriptTracker's data";

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
// create an injection host or the results of ScriptInjector::CanExecuteOnFrame.
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
  bool is_subframe = frame->GetParent();
  return user_script.MatchesDocument(effective_url, is_subframe);
}

void HandleProgrammaticContentScriptInjection(
    base::PassKey<ContentScriptTracker> pass_key,
    content::RenderFrameHost* frame,
    const Extension& extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ExtensionIdSet& content_scripts_for_process =
      ContentScriptsSet::GetOrCreate(*frame->GetProcess());
  content_scripts_for_process.insert(extension.id());

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
  const UserScriptList& list =
      ContentScriptsInfo::GetContentScripts(&extension);
  return std::any_of(list.begin(), list.end(),
                     [frame, &url](const std::unique_ptr<UserScript>& script) {
                       return DoesContentScriptMatch(*script, frame, url);
                     });
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

}  // namespace

// static
bool ContentScriptTracker::DidProcessRunContentScriptFromExtension(
    const content::RenderProcessHost& process,
    const ExtensionId& extension_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Check if we've been notified about the content script injection via
  // ReadyToCommitNavigation or WillExecuteCode methods.
  const ExtensionIdSet* extension_id_set = ContentScriptsSet::Get(process);
  if (!extension_id_set)
    return false;

  return base::Contains(*extension_id_set, extension_id);
}

// static
void ContentScriptTracker::ReadyToCommitNavigation(
    base::PassKey<ExtensionWebContentsObserver> pass_key,
    content::NavigationHandle* navigation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<const Extension*> extensions_injecting_content_scripts =
      GetExtensionsInjectingContentScripts(navigation);
  ExtensionIdSet& content_scripts_for_process = ContentScriptsSet::GetOrCreate(
      *navigation->GetRenderFrameHost()->GetProcess());
  for (const Extension* extension : extensions_injecting_content_scripts)
    content_scripts_for_process.insert(extension->id());

  URLLoaderFactoryManager::WillInjectContentScriptsWhenNavigationCommits(
      base::PassKey<ContentScriptTracker>(), navigation,
      extensions_injecting_content_scripts);
}

// static
void ContentScriptTracker::WillExecuteCode(
    base::PassKey<ScriptExecutor> pass_key,
    content::RenderFrameHost* frame,
    const mojom::HostID& host_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  switch (host_id.type) {
    case mojom::HostID::HostType::kWebUi:
      // This class only tracks extensions.
      return;
    case mojom::HostID::HostType::kExtensions:
      break;
  }

  const ExtensionRegistry* registry =
      ExtensionRegistry::Get(frame->GetProcess()->GetBrowserContext());
  DCHECK(registry);  // WillExecuteCode shouldn't happen during shutdown.
  const Extension* extension =
      registry->enabled_extensions().GetByID(host_id.id);
  DCHECK(extension);  // Guaranteed by the caller - see the doc comment.

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
bool ContentScriptTracker::DoContentScriptsMatchForTesting(
    const Extension& extension,
    content::RenderFrameHost* frame,
    const GURL& url) {
  return DoContentScriptsMatch(extension, frame, url);
}

}  // namespace extensions
