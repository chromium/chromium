// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/script_injection_tracker.h"

#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ref.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/typed_macros.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/browser_frame_context_data.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/url_loader_factory_manager.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/constants.h"
#include "extensions/common/content_script_injection_url_getter.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/trace_util.h"
#include "extensions/common/user_script.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "components/guest_view/browser/guest_view_base.h"
#include "extensions/browser/guest_view/web_view/web_view_content_script_manager.h"
#endif

using perfetto::protos::pbzero::ChromeTrackEvent;

namespace extensions {

namespace {

// Helper for lazily attaching ExtensionIdSet to a RenderProcessHost.  Used to
// track the set of extensions which have injected a JS script into a
// RenderProcessHost.
//
// We track script injection per-RenderProcessHost:
// 1. This matches the real security boundary that Site Isolation uses (the
//    boundary of OS processes) and follows the precedent of
//    content::ChildProcessSecurityPolicy.
// 2. This robustly handles initial empty documents (see the *InitialEmptyDoc*
//    tests in //script_injection_tracker_browsertest.cc) and isn't impacted
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
      // to purge or destroy the set from within ScriptInjectionTracker).
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

  bool HasScript(ScriptInjectionTracker::ScriptType script_type,
                 const ExtensionId& extension_id) const {
    return base::Contains(GetScripts(script_type), extension_id);
  }

  void AddScript(ScriptInjectionTracker::ScriptType script_type,
                 const ExtensionId& extension_id) {
    TRACE_EVENT_INSTANT(
        "extensions",
        "ScriptInjectionTracker::RenderProcessHostUserData::AddScript",
        ChromeTrackEvent::kRenderProcessHost, *process_,
        ChromeTrackEvent::kChromeExtensionId,
        ExtensionIdForTracing(extension_id));
    GetScripts(script_type).insert(extension_id);
  }

  const ExtensionIdSet& content_scripts() const { return content_scripts_; }
  const ExtensionIdSet& user_scripts() const { return user_scripts_; }

 private:
  explicit RenderProcessHostUserData(content::RenderProcessHost& process)
      : process_(process) {
    TRACE_EVENT_BEGIN("extensions",
                      "ScriptInjectionTracker::RenderProcessHostUserData",
                      perfetto::Track::FromPointer(this),
                      ChromeTrackEvent::kRenderProcessHost, *process_);
  }

  const ExtensionIdSet& GetScripts(
      ScriptInjectionTracker::ScriptType script_type) const {
    switch (script_type) {
      case ScriptInjectionTracker::ScriptType::kContentScript:
        return content_scripts_;
      case ScriptInjectionTracker::ScriptType::kUserScript:
        return user_scripts_;
    }
  }
  ExtensionIdSet& GetScripts(ScriptInjectionTracker::ScriptType script_type) {
    return const_cast<ExtensionIdSet&>(
        const_cast<const RenderProcessHostUserData*>(this)->GetScripts(
            script_type));
  }

  static const char* kUserDataKey;

  // The sets of extension ids that have *ever* injected a content script or
  // user script into this particular renderer process.  This is the core data
  // maintained by the ScriptInjectionTracker.
  ExtensionIdSet content_scripts_;
  ExtensionIdSet user_scripts_;

  // Only used for tracing.
  const raw_ref<content::RenderProcessHost> process_;
};

const char* RenderProcessHostUserData::kUserDataKey =
    "ScriptInjectionTracker's data";

std::vector<const UserScript*> GetVectorFromScriptList(
    const UserScriptList& scripts) {
  std::vector<const UserScript*> result;
  result.reserve(scripts.size());
  for (const auto& script : scripts) {
    result.push_back(script.get());
  }
  return result;
}

// Returns all the loaded dynamic scripts with `source` of `extension_id` on
// `frame`.
std::vector<const UserScript*> GetLoadedDynamicScripts(
    const ExtensionId& extension_id,
    UserScript::Source source,
    content::RenderProcessHost& process) {
  // `manager` can be null for some unit tests which do not initialize the
  // ExtensionSystem.
  UserScriptManager* manager =
      ExtensionSystem::Get(process.GetBrowserContext())->user_script_manager();
  if (!manager) {
    CHECK_IS_TEST();
    return std::vector<const UserScript*>();
  }

  const UserScriptList& loaded_dynamic_scripts =
      manager->GetUserScriptLoaderForExtension(extension_id)
          ->GetLoadedDynamicScripts();

  std::vector<const UserScript*> scripts;
  for (auto& loaded_script : loaded_dynamic_scripts) {
    if (loaded_script->GetSource() == source) {
      scripts.push_back(loaded_script.get());
    }
  }
  return scripts;
}

// This function approximates ScriptContext::GetEffectiveDocumentURLForInjection
// from the renderer side.
GURL GetEffectiveDocumentURL(
    content::RenderFrameHost* frame,
    const GURL& document_url,
    MatchOriginAsFallbackBehavior match_origin_as_fallback) {
  // This is a simplification to avoid calling
  // `BrowserFrameContextData::CanAccess` which is unable to replicate all of
  // WebSecurityOrigin::CanAccess checks (e.g. universal access or file
  // exceptions tracked on the renderer side).  This is okay, because our only
  // caller (DoesContentScriptMatch()) expects false positives.
  constexpr bool kAllowInaccessibleParents = true;

  return ContentScriptInjectionUrlGetter::Get(
      BrowserFrameContextData(frame), document_url, match_origin_as_fallback,
      kAllowInaccessibleParents);
}

// Returns whether the extension's scripts can run on `frame`.
bool CanExtensionScriptsAffectFrame(content::RenderFrameHost& frame,
                                    const Extension& extension) {
#if BUILDFLAG(ENABLE_GUEST_VIEW)
  // Most extension's scripts won't run on webviews. The only ones that may are
  // those from extensions that can execute script everywhere.
  auto* guest = guest_view::GuestViewBase::FromRenderFrameHost(&frame);
  return !guest || PermissionsData::CanExecuteScriptEverywhere(
                       extension.id(), extension.location());
#else
  return true;
#endif
}

// Returns whether `extension` will inject any of `scripts` JavaScript content
// into the `frame` / `url`. Note that this function ignores CSS content
// scripts. This function approximates a subset of checks from
// UserScriptSet::GetInjectionForScript (which runs in the renderer process).
// Unlike the renderer version, the code below doesn't consider ability to
// create an injection host, nor the results of
// ScriptInjector::CanExecuteOnFrame, nor the path of `url_patterns`.
// Additionally the `effective_url` calculations are also only an approximation.
// This is okay, because the top-level doc comment for ScriptInjectionTracker
// documents that false positives are expected and why they are okay.
bool DoesScriptMatch(const Extension& extension,
                     const UserScript& script,
                     content::RenderFrameHost& frame,
                     const GURL& url) {
  // ScriptInjectionTracker only needs to track Javascript content scripts (e.g.
  // doesn't track CSS-only injections).
  if (script.js_scripts().empty()) {
    return false;
  }

  GURL effective_url =
      GetEffectiveDocumentURL(&frame, url, script.match_origin_as_fallback());
  auto* web_contents = content::WebContents::FromRenderFrameHost(&frame);
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();

  // Script can inject if the extension has tab permissions for the url.
  if (extension.permissions_data()->HasTabPermissionsForSecurityOrigin(
          tab_id, effective_url)) {
    return true;
  }

  // Dynamic scripts can only inject when the extension has host permissions for
  // the url.
  auto script_source = script.GetSource();
  if ((script_source == UserScript::Source::kDynamicContentScript ||
       script_source == UserScript::Source::kDynamicUserScript) &&
      !extension.permissions_data()->HasHostPermission(effective_url)) {
    return false;
  }

  return script.url_patterns().MatchesSecurityOrigin(effective_url);
}

void HandleProgrammaticScriptInjection(
    base::PassKey<ScriptInjectionTracker> pass_key,
    ScriptInjectionTracker::ScriptType script_type,
    content::RenderFrameHost* frame,
    const Extension& extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Store `extension.id()` in `process_data`.  ScriptInjectionTracker never
  // removes entries from this set - once a renderer process gains an ability to
  // talk on behalf of a content script, it retains this ability forever.  Note
  // that the `process_data` will be destroyed together with the
  // RenderProcessHost (see also a comment inside
  // RenderProcessHostUserData::GetOrCreate).
  auto& process_data =
      RenderProcessHostUserData::GetOrCreate(*frame->GetProcess());
  process_data.AddScript(script_type, extension.id());

  URLLoaderFactoryManager::WillProgrammaticallyInjectContentScript(
      pass_key, frame, extension);
}

// Returns whether ``extension` will inject any of `scripts` JavaScript content
// into the `frame` / `url`.
bool DoScriptsMatch(const Extension& extension,
                    const std::vector<const UserScript*>& scripts,
                    content::RenderFrameHost& frame,
                    const GURL& url) {
  return base::ranges::any_of(
      scripts.begin(), scripts.end(),
      [&extension, &frame, &url](const UserScript* script) {
        return DoesScriptMatch(extension, *script, frame, url);
      });
}

// Returns whether an `extension` can inject JavaScript web view scripts into
// the `frame` / `url`.
bool DoWebViewScripstMatch(const Extension& extension,
                           content::RenderFrameHost& frame) {
#if BUILDFLAG(ENABLE_GUEST_VIEW)
  content::RenderProcessHost& process = *frame.GetProcess();
  TRACE_EVENT("extensions", "ScriptInjectionTracker/DoWebViewScripstMatch",
              ChromeTrackEvent::kRenderProcessHost, process,
              ChromeTrackEvent::kChromeExtensionId,
              ExtensionIdForTracing(extension.id()));

  auto* guest = guest_view::GuestViewBase::FromRenderFrameHost(&frame);
  if (!guest) {
    // Not a guest; no webview scripts.
    return false;
  }

  // Return true if `extension` is an owner of `guest` and it registered
  // content scripts using the `webview.addContentScripts` API.
  GURL owner_site_url = guest->GetOwnerSiteURL();
  if (owner_site_url.SchemeIs(kExtensionScheme) &&
      owner_site_url.host_piece() == extension.id()) {
    WebViewContentScriptManager* script_manager =
        WebViewContentScriptManager::Get(frame.GetBrowserContext());
    int embedder_process_id = guest->owner_rfh()->GetProcess()->GetID();
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
      return true;
    }
  }
#endif

  return false;
}

// Returns whether an `extension` can inject JavaScript static content scripts
// into the `frame` / `url`.  The `url` might be either the last committed URL
// of `frame` or the target of a ReadyToCommit navigation in `frame`.
bool DoStaticContentScriptsMatch(const Extension& extension,
                                 content::RenderFrameHost& frame,
                                 const GURL& url) {
  content::RenderProcessHost& process = *frame.GetProcess();
  TRACE_EVENT("extensions", "ScriptInjectionTracker/DoStaticContentScriptMatch",
              ChromeTrackEvent::kRenderProcessHost, process,
              ChromeTrackEvent::kChromeExtensionId,
              ExtensionIdForTracing(extension.id()));

  if (!CanExtensionScriptsAffectFrame(frame, extension)) {
    return false;
  }

  std::vector<const UserScript*> static_content_scripts =
      GetVectorFromScriptList(
          ContentScriptsInfo::GetContentScripts(&extension));
  return DoScriptsMatch(extension, static_content_scripts, frame, url);
}

// Returns whether an `extension` can inject JavaScript dynamic content scripts
// into the `frame` / `url`.  The `url` might be either the last committed
// URL of `frame` or the target of a ReadyToCommit navigation in `frame`.
bool DoDynamicContentScriptsMatch(const Extension& extension,
                                  content::RenderFrameHost& frame,
                                  const GURL& url) {
  content::RenderProcessHost& process = *frame.GetProcess();
  TRACE_EVENT("extensions",
              "ScriptInjectionTracker/DoDynamicContentScriptsMatch",
              ChromeTrackEvent::kRenderProcessHost, process,
              ChromeTrackEvent::kChromeExtensionId,
              ExtensionIdForTracing(extension.id()));

  if (!CanExtensionScriptsAffectFrame(frame, extension)) {
    return false;
  }

  std::vector<const UserScript*> dynamic_user_scripts = GetLoadedDynamicScripts(
      extension.id(), UserScript::Source::kDynamicContentScript, process);
  return DoScriptsMatch(extension, dynamic_user_scripts, frame, url);
}

// Returns whether an `extension` can inject JavaScript dynamic user scripts
// into the `frame` / `url`.  The `url` might be either the last committed URL
// of `frame` or the target of a ReadyToCommit navigation in `frame`.
bool DoUserScriptsMatch(const Extension& extension,
                        content::RenderFrameHost& frame,
                        const GURL& url) {
  content::RenderProcessHost& process = *frame.GetProcess();
  TRACE_EVENT("extensions", "ScriptInjectionTracker/DoUserScriptsMatch",
              ChromeTrackEvent::kRenderProcessHost, process,
              ChromeTrackEvent::kChromeExtensionId,
              ExtensionIdForTracing(extension.id()));

  if (!CanExtensionScriptsAffectFrame(frame, extension)) {
    return false;
  }

  std::vector<const UserScript*> dynamic_user_scripts = GetLoadedDynamicScripts(
      extension.id(), UserScript::Source::kDynamicUserScript, process);
  return DoScriptsMatch(extension, dynamic_user_scripts, frame, url);
}

// Returns all the extensions injecting content scripts into the `frame` /
// `url`.
std::vector<const Extension*> GetExtensionsInjectingContentScripts(
    const ExtensionSet& extensions,
    content::RenderFrameHost& frame,
    const GURL& url) {
  std::vector<const Extension*> extensions_injecting_scripts;
  for (const auto& it : extensions) {
    const Extension& extension = *it;
    if (DoWebViewScripstMatch(extension, frame) ||
        DoStaticContentScriptsMatch(extension, frame, url) ||
        DoDynamicContentScriptsMatch(extension, frame, url)) {
      extensions_injecting_scripts.push_back(&extension);
    }
  }

  return extensions_injecting_scripts;
}

// Adds all scripts from `extension` that matches the `process` renderers to the
// process data.
void AddMatchingScriptsToProcess(const Extension& extension,
                                 content::RenderProcessHost& process) {
  bool any_frame_matches_content_scripts = false;
  bool any_frame_matches_user_scripts = false;
  process.ForEachRenderFrameHost([&any_frame_matches_content_scripts,
                                  &any_frame_matches_user_scripts,
                                  &extension](content::RenderFrameHost* frame) {
    const GURL& url = frame->GetLastCommittedURL();
    if (!any_frame_matches_content_scripts) {
      any_frame_matches_content_scripts =
          DoWebViewScripstMatch(extension, *frame) ||
          DoStaticContentScriptsMatch(extension, *frame, url) ||
          DoDynamicContentScriptsMatch(extension, *frame, url);
    }
    if (!any_frame_matches_user_scripts) {
      any_frame_matches_user_scripts =
          DoUserScriptsMatch(extension, *frame, url);
    }
  });

  if (any_frame_matches_content_scripts || any_frame_matches_user_scripts) {
    auto& process_data = RenderProcessHostUserData::GetOrCreate(process);
    if (any_frame_matches_content_scripts) {
      process_data.AddScript(ScriptInjectionTracker::ScriptType::kContentScript,
                             extension.id());
    }
    if (any_frame_matches_user_scripts) {
      process_data.AddScript(ScriptInjectionTracker::ScriptType::kUserScript,
                             extension.id());
    }
  }
}

// Returns all the extensions injecting user scripts into the `frame` / `url`.
std::vector<const Extension*> GetExtensionsInjectingUserScripts(
    const ExtensionSet& extensions,
    content::RenderFrameHost& frame,
    const GURL& url) {
  std::vector<const Extension*> extensions_injecting_scripts;
  for (const auto& it : extensions) {
    const Extension& extension = *it;
    if (DoUserScriptsMatch(extension, frame, url)) {
      extensions_injecting_scripts.push_back(&extension);
    }
  }

  return extensions_injecting_scripts;
}

void RecordUkm(content::NavigationHandle* navigation,
               int extensions_injecting_content_script_count) {
  using PermissionID = extensions::mojom::APIPermissionID;
  const ExtensionSet& enabled_extensions =
      ExtensionRegistry::Get(
          navigation->GetRenderFrameHost()->GetProcess()->GetBrowserContext())
          ->enabled_extensions();
  int enabled_extension_count = 0;
  int enabled_extension_count_has_host_permissions = 0;
  int web_request_permission_count = 0;
  int web_request_auth_provider_permission_count = 0;
  int web_request_blocking_permission_count = 0;
  int declarative_net_request_permission_count = 0;
  int declarative_net_request_feedback_permission_count = 0;
  int declarative_net_request_with_host_access_permission_count = 0;
  int declarative_web_request_permission_count = 0;
  for (const scoped_refptr<const Extension>& extension : enabled_extensions) {
    if (!extension->is_extension()) {
      continue;
    }
    // Ignore component extensions.
    if (Manifest::IsComponentLocation(extension->location())) {
      continue;
    }
    enabled_extension_count++;
    const PermissionsData* permissions = extension->permissions_data();
    if (!permissions) {
      continue;
    }
    if (!permissions->HasHostPermission(navigation->GetURL())) {
      continue;
    }
    enabled_extension_count_has_host_permissions++;
    if (permissions->HasAPIPermission(PermissionID::kWebRequest)) {
      web_request_permission_count++;
    }
    if (permissions->HasAPIPermission(PermissionID::kWebRequestAuthProvider)) {
      web_request_auth_provider_permission_count++;
    }
    if (permissions->HasAPIPermission(PermissionID::kWebRequestBlocking)) {
      web_request_blocking_permission_count++;
    }
    if (permissions->HasAPIPermission(PermissionID::kDeclarativeNetRequest)) {
      declarative_net_request_permission_count++;
    }
    if (permissions->HasAPIPermission(
            PermissionID::kDeclarativeNetRequestFeedback)) {
      declarative_net_request_feedback_permission_count++;
    }
    if (permissions->HasAPIPermission(
            PermissionID::kDeclarativeNetRequestWithHostAccess)) {
      declarative_net_request_with_host_access_permission_count++;
    }
    if (permissions->HasAPIPermission(PermissionID::kDeclarativeWebRequest)) {
      declarative_web_request_permission_count++;
    }
  }

  const double kBucketSpacing = 2;
  ukm::builders::Extensions_OnNavigation(navigation->GetNextPageUkmSourceId())
      .SetEnabledExtensionCount(
          ukm::GetExponentialBucketMin(enabled_extension_count, kBucketSpacing))
      .SetEnabledExtensionCount_InjectContentScript(
          ukm::GetExponentialBucketMin(
              extensions_injecting_content_script_count, kBucketSpacing))
      .SetEnabledExtensionCount_HaveHostPermissions(
          ukm::GetExponentialBucketMin(
              enabled_extension_count_has_host_permissions, kBucketSpacing))
      .SetWebRequestPermissionCount(ukm::GetExponentialBucketMin(
          web_request_permission_count, kBucketSpacing))
      .SetWebRequestAuthProviderPermissionCount(ukm::GetExponentialBucketMin(
          web_request_auth_provider_permission_count, kBucketSpacing))
      .SetWebRequestBlockingPermissionCount(ukm::GetExponentialBucketMin(
          web_request_blocking_permission_count, kBucketSpacing))
      .SetDeclarativeNetRequestPermissionCount(ukm::GetExponentialBucketMin(
          declarative_net_request_permission_count, kBucketSpacing))
      .SetDeclarativeNetRequestFeedbackPermissionCount(
          ukm::GetExponentialBucketMin(
              declarative_net_request_feedback_permission_count,
              kBucketSpacing))
      .SetDeclarativeNetRequestWithHostAccessPermissionCount(
          ukm::GetExponentialBucketMin(
              declarative_net_request_with_host_access_permission_count,
              kBucketSpacing))
      .SetDeclarativeWebRequestPermissionCount(ukm::GetExponentialBucketMin(
          declarative_web_request_permission_count, kBucketSpacing))
      .Record(ukm::UkmRecorder::Get());
}

const Extension* FindExtensionByHostId(content::BrowserContext* browser_context,
                                       const mojom::HostID& host_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  switch (host_id.type) {
    // TODO(cmp): Investigate whether Controlled Frame support is needed in
    // ScriptInjectionTracker.
    case mojom::HostID::HostType::kControlledFrameEmbedder:
    case mojom::HostID::HostType::kWebUi:
      // ScriptInjectionTracker only tracks extensions.
      return nullptr;
    case mojom::HostID::HostType::kExtensions:
      break;
  }

  const ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context);
  DCHECK(registry);  // WillExecuteCode and DidUpdateScriptsInRenderer
                     // shouldn't happen during shutdown.

  const Extension* extension =
      registry->enabled_extensions().GetByID(host_id.id);

  return extension;
}

// Stores extensions injecting scripts with `script_type` in `process` data.
void StoreExtensionsInjectingScripts(
    const std::vector<const Extension*>& extensions,
    ScriptInjectionTracker::ScriptType script_type,
    content::RenderProcessHost& process) {
  // ScriptInjectionTracker never removes entries from this set - once a
  // renderer process gains an ability to talk on behalf of a content script,
  // it retains this ability forever.  Note that the `process_data` will be
  // destroyed together with the RenderProcessHost (see also a comment inside
  // RenderProcessHostUserData::GetOrCreate).
  auto& process_data = RenderProcessHostUserData::GetOrCreate(process);
  for (const Extension* extension : extensions) {
    process_data.AddScript(script_type, extension->id());
  }
}

bool DidProcessRunScriptFromExtension(
    ScriptInjectionTracker::ScriptType script_type,
    const content::RenderProcessHost& process,
    const ExtensionId& extension_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!extension_id.empty());

  // Check if we've been notified about the content script injection via
  // ReadyToCommitNavigation or WillExecuteCode methods.
  const auto* process_data = RenderProcessHostUserData::Get(process);
  if (!process_data) {
    return false;
  }

  return process_data->HasScript(script_type, extension_id);
}

}  // namespace

// static
ExtensionIdSet
ScriptInjectionTracker::GetExtensionsThatRanContentScriptsInProcess(
    const content::RenderProcessHost& process) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const auto* process_data = RenderProcessHostUserData::Get(process);
  if (!process_data) {
    return {};
  }

  return process_data->content_scripts();
}

// static
bool ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
    const content::RenderProcessHost& process,
    const ExtensionId& extension_id) {
  return DidProcessRunScriptFromExtension(ScriptType::kContentScript, process,
                                          extension_id);
}

// static
bool ScriptInjectionTracker::DidProcessRunUserScriptFromExtension(
    const content::RenderProcessHost& process,
    const ExtensionId& extension_id) {
  return DidProcessRunScriptFromExtension(ScriptType::kUserScript, process,
                                          extension_id);
}

// static
void ScriptInjectionTracker::ReadyToCommitNavigation(
    base::PassKey<ExtensionWebContentsObserver> pass_key,
    content::NavigationHandle* navigation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderFrameHost& frame = *navigation->GetRenderFrameHost();
  content::RenderProcessHost& process = *frame.GetProcess();
  TRACE_EVENT("extensions", "ScriptInjectionTracker::ReadyToCommitNavigation",
              ChromeTrackEvent::kRenderProcessHost, process);

  const GURL& url = navigation->GetURL();
  const ExtensionRegistry* registry =
      ExtensionRegistry::Get(process.GetBrowserContext());
  DCHECK(registry);  // This method shouldn't be called during shutdown.
  const ExtensionSet& extensions = registry->enabled_extensions();

  // Need to call StoreExtensionsInjectingScripts at ReadyToCommitNavigation
  // time to deal with a (hypothetical, not confirmed by tests) race condition
  // where Browser process sends Commit IPC and then immediately disables the
  // extension.  In this scenario, the renderer may run some content scripts,
  // even though at DidCommit time the Browser will see that the extension has
  // been disabled.
  std::vector<const Extension*> extensions_injecting_content_scripts =
      GetExtensionsInjectingContentScripts(extensions, frame, url);
  std::vector<const Extension*> extensions_injecting_user_scripts =
      GetExtensionsInjectingUserScripts(extensions, frame, url);
  StoreExtensionsInjectingScripts(
      extensions_injecting_content_scripts,
      ScriptInjectionTracker::ScriptType::kContentScript, process);
  StoreExtensionsInjectingScripts(
      extensions_injecting_user_scripts,
      ScriptInjectionTracker::ScriptType::kUserScript, process);

  // Notify URLLoaderFactoryManager for both user and content scripts. This
  // needs to happen at ReadyToCommitNavigation time (i.e. before constructing a
  // URLLoaderFactory that will be sent to the Renderer in a Commit IPC).
  // TODO(crbug.com/40286422): This should only use webview scripts, since it's
  // not needed for all extensions.
  extensions_injecting_content_scripts.reserve(
      extensions_injecting_content_scripts.size() +
      extensions_injecting_user_scripts.size());
  extensions_injecting_content_scripts.insert(
      extensions_injecting_content_scripts.end(),
      extensions_injecting_user_scripts.begin(),
      extensions_injecting_user_scripts.end());
  URLLoaderFactoryManager::WillInjectContentScriptsWhenNavigationCommits(
      base::PassKey<ScriptInjectionTracker>(), navigation,
      extensions_injecting_content_scripts);
}

// static
void ScriptInjectionTracker::DidFinishNavigation(
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

  content::RenderFrameHost& frame = *navigation->GetRenderFrameHost();
  content::RenderProcessHost& process = *frame.GetProcess();
  TRACE_EVENT("extensions", "ScriptInjectionTracker::DidFinishNavigation",
              ChromeTrackEvent::kRenderProcessHost, process);

  const GURL& url = navigation->GetURL();
  const ExtensionRegistry* registry =
      ExtensionRegistry::Get(process.GetBrowserContext());
  DCHECK(registry);  // This method shouldn't be called during shutdown.
  const ExtensionSet& extensions = registry->enabled_extensions();

  // Calling StoreExtensionsInjectingScripts in response to DidCommit IPC is
  // required for correct handling of the race condition from
  // https://crbug.com/1312125.
  std::vector<const Extension*> extensions_injecting_content_scripts =
      GetExtensionsInjectingContentScripts(extensions, frame, url);
  std::vector<const Extension*> extensions_injecting_user_scripts =
      GetExtensionsInjectingUserScripts(extensions, frame, url);
  StoreExtensionsInjectingScripts(
      extensions_injecting_content_scripts,
      ScriptInjectionTracker::ScriptType::kContentScript, process);
  StoreExtensionsInjectingScripts(
      extensions_injecting_user_scripts,
      ScriptInjectionTracker::ScriptType::kUserScript, process);

  int num_extensions_injecting_scripts =
      extensions_injecting_content_scripts.size() +
      extensions_injecting_user_scripts.size();
  RecordUkm(navigation, num_extensions_injecting_scripts);
}

// static
void ScriptInjectionTracker::WillExecuteCode(
    base::PassKey<ScriptExecutor> pass_key,
    ScriptType script_type,
    content::RenderFrameHost* frame,
    const mojom::HostID& host_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderProcessHost& process = *frame->GetProcess();
  TRACE_EVENT("extensions", "ScriptInjectionTracker::WillExecuteCode/1",
              ChromeTrackEvent::kRenderProcessHost, process,
              ChromeTrackEvent::kChromeExtensionId,
              ExtensionIdForTracing(host_id.id));

  const Extension* extension =
      FindExtensionByHostId(process.GetBrowserContext(), host_id);
  if (!extension) {
    return;
  }

  HandleProgrammaticScriptInjection(PassKey(), script_type, frame, *extension);
}

// static
void ScriptInjectionTracker::WillExecuteCode(
    base::PassKey<RequestContentScript> pass_key,
    content::RenderFrameHost* frame,
    const Extension& extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  TRACE_EVENT("extensions", "ScriptInjectionTracker::WillExecuteCode/2",
              ChromeTrackEvent::kRenderProcessHost, *frame->GetProcess(),
              ChromeTrackEvent::kChromeExtensionId,
              ExtensionIdForTracing(extension.id()));

  // Declarative content scripts are only ever of a kContentScript type and
  // never handle user scripts.
  HandleProgrammaticScriptInjection(PassKey(), ScriptType::kContentScript,
                                    frame, extension);
}

// static
void ScriptInjectionTracker::WillGrantActiveTab(
    base::PassKey<ActiveTabPermissionGranter> pass_key,
    const Extension& extension,
    content::RenderProcessHost& process) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  AddMatchingScriptsToProcess(extension, process);
}

// static
void ScriptInjectionTracker::DidUpdateScriptsInRenderer(
    base::PassKey<UserScriptLoader> pass_key,
    const mojom::HostID& host_id,
    content::RenderProcessHost& process) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  TRACE_EVENT(
      "extensions", "ScriptInjectionTracker::DidUpdateScriptsInRenderer",
      ChromeTrackEvent::kRenderProcessHost, process,
      ChromeTrackEvent::kChromeExtensionId, ExtensionIdForTracing(host_id.id));

  scoped_refptr<const Extension> extension =
      FindExtensionByHostId(process.GetBrowserContext(), host_id);
  if (!extension) {
    return;
  }

  AddMatchingScriptsToProcess(*extension, process);
}

// static
void ScriptInjectionTracker::DidUpdatePermissionsInRenderer(
    base::PassKey<PermissionsUpdater> pass_key,
    const Extension& extension,
    content::RenderProcessHost& process) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  AddMatchingScriptsToProcess(extension, process);
}

// static
bool ScriptInjectionTracker::DoStaticContentScriptsMatchForTesting(
    const Extension& extension,
    content::RenderFrameHost* frame,
    const GURL& url) {
  return DoStaticContentScriptsMatch(extension, *frame, url);
}

namespace debug {

namespace {

base::debug::CrashKeyString* GetRegistryStatusCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "extension_registry_status", base::debug::CrashKeySize::Size256);
  return crash_key;
}

std::string GetRegistryStatusValue(const ExtensionId& extension_id,
                                   content::BrowserContext& browser_context) {
  std::string result = "status=";
  ExtensionRegistry* registry = ExtensionRegistry::Get(&browser_context);
  if (registry->enabled_extensions().Contains(extension_id)) {
    result += "enabled,";
  }
  if (registry->disabled_extensions().Contains(extension_id)) {
    result += "disabled,";
  }
  if (registry->terminated_extensions().Contains(extension_id)) {
    result += "terminated,";
  }
  if (registry->blocklisted_extensions().Contains(extension_id)) {
    result += "blocklisted,";
  }
  if (registry->blocked_extensions().Contains(extension_id)) {
    result += "blocked,";
  }
  if (registry->ready_extensions().Contains(extension_id)) {
    result += "ready,";
  }
  return result;
}

base::debug::CrashKeyString* GetIsIncognitoCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "is_incognito", base::debug::CrashKeySize::Size32);
  return crash_key;
}

base::debug::CrashKeyString* GetLastCommittedOriginCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "script_frame_last_committed_origin", base::debug::CrashKeySize::Size256);
  return crash_key;
}

base::debug::CrashKeyString* GetLastCommittedUrlCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "script_frame_last_committed_url", base::debug::CrashKeySize::Size256);
  return crash_key;
}

base::debug::CrashKeyString* GetLifecycleStateCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "lifecycle_state", base::debug::CrashKeySize::Size32);
  return crash_key;
}

#if BUILDFLAG(ENABLE_GUEST_VIEW)
base::debug::CrashKeyString* GetIsGuestCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "is_guest", base::debug::CrashKeySize::Size32);
  return crash_key;
}
#endif

base::debug::CrashKeyString* GetDoWebViewScriptsMatchCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "do_web_view_scripts_match", base::debug::CrashKeySize::Size32);
  return crash_key;
}

base::debug::CrashKeyString* GetDoStaticContentScriptsMatchCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "do_static_content_scripts_match", base::debug::CrashKeySize::Size32);
  return crash_key;
}

base::debug::CrashKeyString* GetDoDynamicContentScriptsMatchCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "do_dynamic_content_scripts_match", base::debug::CrashKeySize::Size32);
  return crash_key;
}

base::debug::CrashKeyString* GetDoUserScriptsMatchCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "do_user_scripts_match", base::debug::CrashKeySize::Size32);
  return crash_key;
}

const char* BoolToCrashKeyValue(bool value) {
  return value ? "yes" : "no";
}

}  // namespace

ScopedScriptInjectionTrackerFailureCrashKeys::
    ScopedScriptInjectionTrackerFailureCrashKeys(
        content::BrowserContext& browser_context,
        const ExtensionId& extension_id)
    : registry_status_crash_key_(
          GetRegistryStatusCrashKey(),
          GetRegistryStatusValue(extension_id, browser_context)),
      is_incognito_crash_key_(
          GetIsIncognitoCrashKey(),
          BoolToCrashKeyValue(browser_context.IsOffTheRecord())) {}

ScopedScriptInjectionTrackerFailureCrashKeys::
    ScopedScriptInjectionTrackerFailureCrashKeys(
        content::RenderFrameHost& frame,
        const ExtensionId& extension_id)
    : ScopedScriptInjectionTrackerFailureCrashKeys(*frame.GetBrowserContext(),
                                                   extension_id) {
  const GURL& frame_url = frame.GetLastCommittedURL();
  last_committed_origin_crash_key_.emplace(
      GetLastCommittedOriginCrashKey(),
      frame.GetLastCommittedOrigin().GetDebugString());
  last_committed_url_crash_key_.emplace(GetLastCommittedUrlCrashKey(),
                                        frame_url.possibly_invalid_spec());
  lifecycle_state_crash_key_.emplace(
      GetLifecycleStateCrashKey(),
      base::NumberToString(static_cast<int>(frame.GetLifecycleState())));

#if BUILDFLAG(ENABLE_GUEST_VIEW)
  auto* guest = guest_view::GuestViewBase::FromRenderFrameHost(&frame);
  is_guest_crash_key_.emplace(GetIsGuestCrashKey(),
                              BoolToCrashKeyValue(!!guest));
#endif

  const ExtensionRegistry* registry =
      ExtensionRegistry::Get(frame.GetBrowserContext());
  CHECK(registry);

  const Extension* extension =
      registry->enabled_extensions().GetByID(extension_id);
  if (extension) {
    do_web_view_scripts_match_crash_key_.emplace(
        GetDoWebViewScriptsMatchCrashKey(),
        BoolToCrashKeyValue(DoWebViewScripstMatch(*extension, frame)));
    do_static_content_scripts_match_crash_key_.emplace(
        GetDoStaticContentScriptsMatchCrashKey(),
        BoolToCrashKeyValue(
            DoStaticContentScriptsMatch(*extension, frame, frame_url)));
    do_dynamic_content_scripts_match_crash_key_.emplace(
        GetDoDynamicContentScriptsMatchCrashKey(),
        BoolToCrashKeyValue(
            DoDynamicContentScriptsMatch(*extension, frame, frame_url)));
    do_user_scripts_match_crash_key_.emplace(
        GetDoUserScriptsMatchCrashKey(),
        BoolToCrashKeyValue(DoUserScriptsMatch(*extension, frame, frame_url)));
  }
}

ScopedScriptInjectionTrackerFailureCrashKeys::
    ~ScopedScriptInjectionTrackerFailureCrashKeys() = default;

}  // namespace debug
}  // namespace extensions
