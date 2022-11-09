// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_script_tracker.h"

#include <map>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/typed_macros.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/guest_view/web_view/web_view_content_script_manager.h"
#include "extensions/browser/url_loader_factory_manager.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/constants.h"
#include "extensions/common/content_script_injection_url_getter.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/trace_util.h"
#include "extensions/common/user_script.h"

using perfetto::protos::pbzero::ChromeTrackEvent;

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
//    by ReadyToCommit races associated with DocumentUserData.
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
      auto owned_self =
          base::WrapUnique(new RenderProcessHostUserData(process));
      self = owned_self.get();
      process.SetUserData(kUserDataKey, std::move(owned_self));
    }

    DCHECK(self);
    return *self;
  }

  // base::SupportsUserData::Data override:
  ~RenderProcessHostUserData() override {
    TRACE_EVENT_END("extensions", perfetto::Track::FromPointer(this),
                    ChromeTrackEvent::kRenderProcessHost, *process_);
  }

  bool HasContentScript(const ExtensionId& extension_id) const {
    return base::Contains(content_scripts_, extension_id);
  }

  void AddContentScript(const ExtensionId& extension_id) {
    TRACE_EVENT_INSTANT(
        "extensions",
        "ContentScriptTracker::RenderProcessHostUserData::AddContentScript",
        ChromeTrackEvent::kRenderProcessHost, *process_,
        ChromeTrackEvent::kChromeExtensionId,
        ExtensionIdForTracing(extension_id));
    content_scripts_.insert(extension_id);
  }

  void AddFrame(content::RenderFrameHost* frame) { frames_.insert(frame); }
  void RemoveFrame(content::RenderFrameHost* frame) { frames_.erase(frame); }
  const std::set<content::RenderFrameHost*>& frames() const { return frames_; }

  const ExtensionIdSet& content_scripts() const { return content_scripts_; }

 private:
  explicit RenderProcessHostUserData(content::RenderProcessHost& process)
      : process_(process) {
    TRACE_EVENT_BEGIN("extensions",
                      "ContentScriptTracker::RenderProcessHostUserData",
                      perfetto::Track::FromPointer(this),
                      ChromeTrackEvent::kRenderProcessHost, *process_);
  }

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

  // Only used for tracing.
  const raw_ref<content::RenderProcessHost> process_;
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
    // Non primary pages(e.g. fenced frame, prerendered page, bfcache, and
    // portals) can't look at the opener, and WebContents::GetOpener returns the
    // opener on the primary frame tree. Thus, GetOpener should be called when
    // |frame_| is a primary main frame.
    if (!parent_or_opener && frame_->IsInPrimaryMainFrame()) {
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

  GURL GetUrl() const override {
    if (frame_->GetLastCommittedURL().is_empty()) {
      // It's possible for URL to be empty when `frame_` is on the initial empty
      // document. TODO(https://crbug.com/1197308): Consider making  `frame_`'s
      // document's URL about:blank instead of empty in that case.
      return GURL(url::kAboutBlankURL);
    }
    return frame_->GetLastCommittedURL();
  }

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
  const raw_ptr<content::RenderFrameHost> frame_;
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
  content::RenderProcessHost& process = *frame->GetProcess();
  const ExtensionId& extension_id = user_script.extension_id();

  // ContentScriptTracker only needs to track Javascript content scripts (e.g.
  // doesn't track CSS-only injections).
  if (user_script.js_scripts().empty()) {
    TRACE_EVENT_INSTANT(
        "extensions",
        "ContentScriptTracker/DoesContentScriptMatch=false(non-js)",
        ChromeTrackEvent::kRenderProcessHost, process,
        ChromeTrackEvent::kChromeExtensionId,
        ExtensionIdForTracing(extension_id));
    return false;
  }

  GURL effective_url = GetEffectiveDocumentURL(
      frame, url, user_script.match_origin_as_fallback());
  if (user_script.url_patterns().MatchesSecurityOrigin(effective_url)) {
    TRACE_EVENT_INSTANT("extensions",
                        "ContentScriptTracker/DoesContentScriptMatch=true",
                        ChromeTrackEvent::kRenderProcessHost, process,
                        ChromeTrackEvent::kChromeExtensionId,
                        ExtensionIdForTracing(extension_id));
    return true;
  } else {
    TRACE_EVENT_INSTANT(
        "extensions",
        "ContentScriptTracker/DoesContentScriptMatch=false(mismatch)",
        ChromeTrackEvent::kRenderProcessHost, process,
        ChromeTrackEvent::kChromeExtensionId,
        ExtensionIdForTracing(extension_id));
    return false;
  }
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

bool DoContentScriptsMatch(const UserScriptList& content_script_list,
                           content::RenderFrameHost* frame,
                           const GURL& url) {
  return base::ranges::any_of(
      content_script_list.begin(), content_script_list.end(),
      [frame, &url](const std::unique_ptr<UserScript>& script) {
        return DoesContentScriptMatch(*script, frame, url);
      });
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
  TRACE_EVENT("extensions", "ContentScriptTracker/DoContentScriptsMatch",
              ChromeTrackEvent::kRenderProcessHost, *frame->GetProcess(),
              ChromeTrackEvent::kChromeExtensionId,
              ExtensionIdForTracing(extension.id()));
  content::RenderProcessHost& process = *frame->GetProcess();

  auto* guest = guest_view::GuestViewBase::FromRenderFrameHost(frame);
  if (guest) {
    // Return true if `extension` is an owner of `guest` and it registered
    // content scripts using the `webview.addContentScripts` API.
    GURL owner_site_url = guest->GetOwnerSiteURL();
    if (owner_site_url.SchemeIs(kExtensionScheme) &&
        owner_site_url.host_piece() == extension.id()) {
      WebViewContentScriptManager* script_manager =
          WebViewContentScriptManager::Get(frame->GetBrowserContext());
      int embedder_process_id = guest->owner_web_contents()
                                    ->GetPrimaryMainFrame()
                                    ->GetProcess()
                                    ->GetID();
      std::set<std::string> script_ids = script_manager->GetContentScriptIDSet(
          embedder_process_id, guest->view_instance_id());

      // Note - more granular checks (e.g. against URL patterns) are desirable
      // for performance (to avoid creating unnecessary URLLoaderFactory via
      // URLLoaderFactoryManager), but not necessarily for security (because
      // there are anyway no OOPIFs inside the webView process -
      // https://crbug.com/614463).  At the same time, more granular checks are
      // difficult to achieve, because the UserScript objects are not retained
      // (i.e. only UserScriptIDs are available) by WebViewContentScriptManager.
      if (!script_ids.empty()) {
        TRACE_EVENT_INSTANT(
            "extensions",
            "ContentScriptTracker/DoContentScriptsMatch=true(guest)",
            ChromeTrackEvent::kRenderProcessHost, process,
            ChromeTrackEvent::kChromeExtensionId,
            ExtensionIdForTracing(extension.id()));
        return true;
      }
    }
  }

  if (!guest || PermissionsData::CanExecuteScriptEverywhere(
                    extension.id(), extension.location())) {
    // Return true if manifest-declared content scripts match.
    const UserScriptList& manifest_scripts =
        ContentScriptsInfo::GetContentScripts(&extension);
    if (DoContentScriptsMatch(manifest_scripts, frame, url)) {
      TRACE_EVENT_INSTANT(
          "extensions",
          "ContentScriptTracker/DoContentScriptsMatch=true(manifest)",
          ChromeTrackEvent::kRenderProcessHost, process,
          ChromeTrackEvent::kChromeExtensionId,
          ExtensionIdForTracing(extension.id()));
      return true;
    }

    // Return true if dynamic content scripts match.  Note that `manager` can be
    // null for some unit tests which do not initialize the ExtensionSystem.
    UserScriptManager* manager =
        ExtensionSystem::Get(frame->GetProcess()->GetBrowserContext())
            ->user_script_manager();
    if (manager) {
      const UserScriptList& dynamic_scripts =
          manager->GetUserScriptLoaderForExtension(extension.id())
              ->GetLoadedDynamicScripts();
      if (DoContentScriptsMatch(dynamic_scripts, frame, url)) {
        TRACE_EVENT_INSTANT(
            "extensions",
            "ContentScriptTracker/DoContentScriptsMatch=true(dynamic)",
            ChromeTrackEvent::kRenderProcessHost, process,
            ChromeTrackEvent::kChromeExtensionId,
            ExtensionIdForTracing(extension.id()));
        return true;
      }
    }
  }

  // Otherwise, no content script from `extension` can run in `frame` at `url`.
  TRACE_EVENT_INSTANT("extensions",
                      "ContentScriptTracker/DoContentScriptsMatch=false",
                      ChromeTrackEvent::kRenderProcessHost, process,
                      ChromeTrackEvent::kChromeExtensionId,
                      ExtensionIdForTracing(extension.id()));
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

void StoreExtensionsInjectingContentScripts(
    const std::vector<const Extension*>& extensions_injecting_content_scripts,
    content::RenderProcessHost& process) {
  // Store `extensions_injecting_content_scripts` in `process_data`.
  // ContentScriptTracker never removes entries from this set - once a renderer
  // process gains an ability to talk on behalf of a content script, it retains
  // this ability forever.  Note that the `process_data` will be destroyed
  // together with the RenderProcessHost (see also a comment inside
  // RenderProcessHostUserData::GetOrCreate).
  auto& process_data = RenderProcessHostUserData::GetOrCreate(process);
  for (const Extension* extension : extensions_injecting_content_scripts)
    process_data.AddContentScript(extension->id());
}

}  // namespace

// static
ExtensionIdSet ContentScriptTracker::GetExtensionsThatRanScriptsInProcess(
    const content::RenderProcessHost& process) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const auto* process_data = RenderProcessHostUserData::Get(process);
  if (!process_data)
    return {};

  return process_data->content_scripts();
}

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

  content::RenderProcessHost& process =
      *navigation->GetRenderFrameHost()->GetProcess();
  TRACE_EVENT("extensions", "ContentScriptTracker::ReadyToCommitNavigation",
              ChromeTrackEvent::kRenderProcessHost, process);

  // Need to call StoreExtensionsInjectingContentScripts at
  // ReadyToCommitNavigation time to deal with a (hypothetical, not confirmed by
  // tests) race condition where Browser process sends Commit IPC and then
  // immediately disables the extension.  In this scenario, the renderer may run
  // some content scripts, even though at DidCommit time the Browser will see
  // that the extension has been disabled.
  std::vector<const Extension*> extensions_injecting_content_scripts =
      GetExtensionsInjectingContentScripts(navigation);
  StoreExtensionsInjectingContentScripts(extensions_injecting_content_scripts,
                                         process);

  // Notify URLLoaderFactoryManager - this needs to happen at
  // ReadyToCommitNavigation time (i.e. before constructing a URLLoaderFactory
  // that will be sent to the Renderer in a Commit IPC).
  URLLoaderFactoryManager::WillInjectContentScriptsWhenNavigationCommits(
      base::PassKey<ContentScriptTracker>(), navigation,
      extensions_injecting_content_scripts);
}

// static
void ContentScriptTracker::DidFinishNavigation(
    base::PassKey<ExtensionWebContentsObserver> pass_key,
    content::NavigationHandle* navigation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Only consider cross-document navigations that actually commit.  (Documents
  // associated with same-document navigations should have already been
  // processed by an earlier DidFinishNavigation.  Navigations that don't
  // commit/load won't inject content scripts.  Content script injections are
  // primarily driven by URL matching and therefore failed navigations may still
  // end up injecting content scripts into the error page. Pre-rendered pages
  // already ran content scripts at the initial navigation and don't need to
  // run them again on activation.)
  if (!navigation->HasCommitted() || navigation->IsSameDocument() ||
      navigation->IsPrerenderedPageActivation()) {
    return;
  }

  content::RenderProcessHost& process =
      *navigation->GetRenderFrameHost()->GetProcess();
  TRACE_EVENT("extensions", "ContentScriptTracker::DidFinishNavigation",
              ChromeTrackEvent::kRenderProcessHost, process);

  // Calling StoreExtensionsInjectingContentScripts in response to DidCommit IPC
  // is required for correct handling of the race condition from
  // https://crbug.com/1312125.
  std::vector<const Extension*> extensions_injecting_content_scripts =
      GetExtensionsInjectingContentScripts(navigation);
  StoreExtensionsInjectingContentScripts(extensions_injecting_content_scripts,
                                         process);
}

// static
void ContentScriptTracker::RenderFrameCreated(
    base::PassKey<ExtensionWebContentsObserver> pass_key,
    content::RenderFrameHost* frame) {
  TRACE_EVENT("extensions", "ContentScriptTracker::RenderFrameCreated",
              ChromeTrackEvent::kRenderProcessHost, *frame->GetProcess());

  auto& process_data =
      RenderProcessHostUserData::GetOrCreate(*frame->GetProcess());
  process_data.AddFrame(frame);
}

// static
void ContentScriptTracker::RenderFrameDeleted(
    base::PassKey<ExtensionWebContentsObserver> pass_key,
    content::RenderFrameHost* frame) {
  TRACE_EVENT("extensions", "ContentScriptTracker::RenderFrameDeleted",
              ChromeTrackEvent::kRenderProcessHost, *frame->GetProcess());

  auto& process_data =
      RenderProcessHostUserData::GetOrCreate(*frame->GetProcess());
  process_data.RemoveFrame(frame);
}

// static
void ContentScriptTracker::WillExecuteCode(
    base::PassKey<ScriptExecutor> pass_key,
    content::RenderFrameHost* frame,
    const mojom::HostID& host_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderProcessHost& process = *frame->GetProcess();
  TRACE_EVENT("extensions", "ContentScriptTracker::WillExecuteCode/1",
              ChromeTrackEvent::kRenderProcessHost, process,
              ChromeTrackEvent::kChromeExtensionId,
              ExtensionIdForTracing(host_id.id));

  const Extension* extension =
      FindExtensionByHostId(process.GetBrowserContext(), host_id);
  if (!extension)
    return;

  HandleProgrammaticContentScriptInjection(PassKey(), frame, *extension);
}

// static
void ContentScriptTracker::WillExecuteCode(
    base::PassKey<RequestContentScript> pass_key,
    content::RenderFrameHost* frame,
    const Extension& extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  TRACE_EVENT("extensions", "ContentScriptTracker::WillExecuteCode/2",
              ChromeTrackEvent::kRenderProcessHost, *frame->GetProcess(),
              ChromeTrackEvent::kChromeExtensionId,
              ExtensionIdForTracing(extension.id()));

  HandleProgrammaticContentScriptInjection(PassKey(), frame, extension);
}

// static
void ContentScriptTracker::WillUpdateContentScriptsInRenderer(
    base::PassKey<UserScriptLoader> pass_key,
    const mojom::HostID& host_id,
    content::RenderProcessHost& process) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  TRACE_EVENT(
      "extensions", "ContentScriptTracker::WillUpdateContentScriptsInRenderer",
      ChromeTrackEvent::kRenderProcessHost, process,
      ChromeTrackEvent::kChromeExtensionId, ExtensionIdForTracing(host_id.id));

  const Extension* extension =
      FindExtensionByHostId(process.GetBrowserContext(), host_id);
  if (!extension)
    return;

  auto& process_data = RenderProcessHostUserData::GetOrCreate(process);
  const std::set<content::RenderFrameHost*>& frames_in_process =
      process_data.frames();
  bool any_frame_matches_content_scripts = base::ranges::any_of(
      frames_in_process, [extension](content::RenderFrameHost* frame) {
        return DoContentScriptsMatch(*extension, frame,
                                     frame->GetLastCommittedURL());
      });
  if (any_frame_matches_content_scripts) {
    process_data.AddContentScript(extension->id());
  } else {
    TRACE_EVENT_INSTANT(
        "extensions",
        "ContentScriptTracker::WillUpdateContentScriptsInRenderer - no matches",
        ChromeTrackEvent::kRenderProcessHost, process,
        ChromeTrackEvent::kChromeExtensionId,
        ExtensionIdForTracing(host_id.id));
  }
}

// static
bool ContentScriptTracker::DoContentScriptsMatchForTesting(
    const Extension& extension,
    content::RenderFrameHost* frame,
    const GURL& url) {
  return DoContentScriptsMatch(extension, frame, url);
}

}  // namespace extensions
