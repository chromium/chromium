// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/process_map.h"

#include <string>
#include <tuple>

#include "base/containers/contains.h"
#include "base/containers/map_util.h"
#include "base/types/optional_util.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map_factory.h"
#include "extensions/browser/script_injection_tracker.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "pdf/buildflags.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#endif

#if BUILDFLAG(ENABLE_PDF)
#include "extensions/common/constants.h"
#include "pdf/pdf_features.h"
#endif

namespace extensions {

namespace {

// Returns true if `process_id` is associated with a WebUI process.
bool ProcessHasWebUIBindings(int process_id) {
  // TODO(crbug.com/40676401): HasWebUIBindings does not always return true for
  // WebUIs. This should be changed to use something else.
  return content::ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      process_id);
}

// Returns true if `process_id` is associated with a webview owned by the
// extension with the specified `extension_id`.
bool IsWebViewProcessForExtension(int process_id,
                                  const ExtensionId& extension_id) {
#if BUILDFLAG(ENABLE_GUEST_VIEW)
  WebViewRendererState* web_view_state = WebViewRendererState::GetInstance();
  if (!web_view_state->IsGuest(process_id)) {
    return false;
  }

  std::string webview_owner;
  int owner_process_id = -1;
  bool found_info = web_view_state->GetOwnerInfo(process_id, &owner_process_id,
                                                 &webview_owner);
  return found_info && webview_owner == extension_id;
#else
  return false;
#endif
}

}  // namespace

// ProcessMap
ProcessMap::ProcessMap(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

ProcessMap::~ProcessMap() = default;

void ProcessMap::Shutdown() {
  browser_context_ = nullptr;
}

// static
ProcessMap* ProcessMap::Get(content::BrowserContext* browser_context) {
  return ProcessMapFactory::GetForBrowserContext(browser_context);
}

bool ProcessMap::Insert(const ExtensionId& extension_id, int process_id) {
  return items_.emplace(process_id, extension_id).second;
}

int ProcessMap::Remove(int process_id) {
  return items_.erase(process_id);
}

bool ProcessMap::Contains(const ExtensionId& extension_id_in,
                          int process_id) const {
  auto* extension_id = base::FindOrNull(items_, process_id);
  return extension_id && *extension_id == extension_id_in;
}

bool ProcessMap::Contains(int process_id) const {
  return base::Contains(items_, process_id);
}

const Extension* ProcessMap::GetEnabledExtensionByProcessID(
    int process_id) const {
  auto* extension_id = base::FindOrNull(items_, process_id);
  return extension_id ? ExtensionRegistry::Get(browser_context_)
                            ->enabled_extensions()
                            .GetByID(*extension_id)
                      : nullptr;
}

std::optional<ExtensionId> ProcessMap::GetExtensionIdForProcess(
    int process_id) const {
  return base::OptionalFromPtr(base::FindOrNull(items_, process_id));
}

bool ProcessMap::IsPrivilegedExtensionProcess(const Extension& extension,
                                              int process_id) {
  return Contains(extension.id(), process_id) &&
         // Hosted apps aren't considered privileged extension processes...
         (!extension.is_hosted_app() ||
          // ... Unless they're component hosted apps, like the webstore.
          // TODO(https://crbug/1429667): We can clean this up when we remove
          // special handling of component hosted apps.
          extension.location() == mojom::ManifestLocation::kComponent) &&
         // Lock screen contexts are not the same as privileged extension
         // processes.
         !is_lock_screen_context_;
}

bool ProcessMap::CanProcessHostContextType(
    const Extension* extension,
    const content::RenderProcessHost& process,
    mojom::ContextType context_type) {
  const int process_id = process.GetID();
  switch (context_type) {
    case mojom::ContextType::kUnspecified:
      // We never consider unspecified contexts valid. Even though they would be
      // permissionless, they should never be able to make a request to the
      // browser.
      return false;
    case mojom::ContextType::kOffscreenExtension:
    case mojom::ContextType::kPrivilegedExtension:
      // Offscreen documents run in the main extension process, so both of these
      // require a privileged extension process.
      return extension && IsPrivilegedExtensionProcess(*extension, process_id);
    case mojom::ContextType::kUnprivilegedExtension:
      return extension &&
             IsWebViewProcessForExtension(process_id, extension->id());
    case mojom::ContextType::kContentScript:
      // Currently, we assume any process can host a content script.
      // TODO(crbug.com/40055126): This could be better by looking at
      // ScriptInjectionTracker, as we do for user scripts below.
      return !!extension;
    case mojom::ContextType::kUserScript:
      return extension &&
             ScriptInjectionTracker::DidProcessRunUserScriptFromExtension(
                 process, extension->id());
    case mojom::ContextType::kLockscreenExtension:
      // Lock screen contexts are essentially privileged contexts that run on
      // the lock screen profile. We don't run component hosted apps there, so
      // no need to allow those.
      return is_lock_screen_context_ && extension &&
             !extension->is_hosted_app() &&
             Contains(extension->id(), process_id);
    case mojom::ContextType::kPrivilegedWebPage:
      // A privileged web page is a (non-component) hosted app process.
      return extension && extension->is_hosted_app() &&
             extension->location() != mojom::ManifestLocation::kComponent &&
             Contains(extension->id(), process_id);
    case mojom::ContextType::kUntrustedWebUi:
      // Unfortunately, we have no way of checking if a *process* can host
      // untrusted webui contexts. Callers should look at (ideally, the
      // browser-verified) origin.
      [[fallthrough]];
    case mojom::ContextType::kWebPage:
      // Any context not associated with an extension, not running in an
      // extension process, and without webui bindings can be considered a
      // web page process.
      return !extension && !Contains(process_id) &&
             !ProcessHasWebUIBindings(process_id);
    case mojom::ContextType::kWebUi:
      // Don't consider extensions in webui (like content scripts) to be
      // webui.
      return !extension && ProcessHasWebUIBindings(process_id);
  }
}

mojom::ContextType ProcessMap::GetMostLikelyContextType(
    const Extension* extension,
    int process_id,
    const GURL* url) const {
  // WARNING: This logic must match ScriptContextSet::ClassifyJavaScriptContext,
  // as much as possible.

  // TODO(crbug.com/40676105): Move this into the !extension if statement below
  // or document why we want to return WEBUI_CONTEXT for content scripts in
  // WebUIs.
  if (ProcessHasWebUIBindings(process_id)) {
    return mojom::ContextType::kWebUi;
  }

  if (!extension) {
    // Note that blob/filesystem schemes associated with an inner URL of
    // chrome-untrusted will be considered regular pages.
    if (url && url->SchemeIs(content::kChromeUIUntrustedScheme)) {
      return mojom::ContextType::kUntrustedWebUi;
    }

    return mojom::ContextType::kWebPage;
  }

  const ExtensionId& extension_id = extension->id();
  if (!Contains(extension_id, process_id)) {
    // If the process map doesn't contain the process, it might be an extension
    // frame in a webview.
    // We (deliberately) don't add webview-hosted frames to the process map and
    // don't classify them as kPrivilegedExtension contexts.
    if (url && extension->origin().IsSameOriginWith(*url) &&
        IsWebViewProcessForExtension(process_id, extension->id())) {
      // Yep, it's an extension frame in a webview.
#if BUILDFLAG(ENABLE_PDF)
      // The PDF Viewer extension is an exception, since webviews need to be
      // able to load the PDF Viewer. The PDF extension needs a
      // kPrivilegedExtension context to load, so the PDF extension frame is
      // added to the process map and shouldn't reach here.
      if (chrome_pdf::features::IsOopifPdfEnabled()) {
        CHECK_NE(extension_id, extension_misc::kPdfExtensionId);
      }
#endif  // BUILDFLAG(ENABLE_PDF)

      return mojom::ContextType::kUnprivilegedExtension;
    }

    // Otherwise, it's a content script (the context in which an extension can
    // run in an unassociated, non-webview process).
    return mojom::ContextType::kContentScript;
  }

  if (extension->is_hosted_app() &&
      extension->location() != mojom::ManifestLocation::kComponent) {
    return mojom::ContextType::kPrivilegedWebPage;
  }

  // TODO(crbug.com/40849649): Currently, offscreen document contexts
  // are misclassified as kPrivilegedExtension contexts. This is not ideal
  // because there is a mismatch between the browser and the renderer), but it's
  // not a security issue because, while offscreen documents have fewer
  // capabilities, this is an API distinction, and not a security enforcement.
  // Offscreen documents run in the same process as the rest of the extension
  // and can message the extension, so could easily - though indirectly -
  // access all the same features.
  // Even so, we should fix this to properly classify offscreen documents (and
  // this would be a problem if offscreen documents ever have access to APIs
  // that kPrivilegedExtension contexts don't).

  return is_lock_screen_context_ ? mojom::ContextType::kLockscreenExtension
                                 : mojom::ContextType::kPrivilegedExtension;
}

}  // namespace extensions
