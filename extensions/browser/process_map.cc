// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/process_map.h"

#include <string>
#include <tuple>

#include "base/containers/contains.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/browser/process_map_factory.h"
#include "extensions/browser/script_injection_tracker.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/context_type.mojom.h"

namespace extensions {

namespace {

// Returns true if `process_id` is associated with a WebUI process.
bool ProcessHasWebUIBindings(int process_id) {
  // TODO(crbug.com/1055656): HasWebUIBindings does not always return true for
  // WebUIs. This should be changed to use something else.
  return content::ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      process_id);
}

// Returns true if `process_id` is associated with a webview owned by the
// extension with the specified `extension_id`.
bool IsWebViewProcessForExtension(int process_id,
                                  const ExtensionId& extension_id) {
  WebViewRendererState* web_view_state = WebViewRendererState::GetInstance();
  if (!web_view_state->IsGuest(process_id)) {
    return false;
  }

  std::string webview_owner;
  int owner_process_id = -1;
  bool found_info = web_view_state->GetOwnerInfo(process_id, &owner_process_id,
                                                 &webview_owner);
  return found_info && webview_owner == extension_id;
}

}  // namespace

// ProcessMap
ProcessMap::ProcessMap() = default;

ProcessMap::~ProcessMap() = default;

// static
ProcessMap* ProcessMap::Get(content::BrowserContext* browser_context) {
  return ProcessMapFactory::GetForBrowserContext(browser_context);
}

bool ProcessMap::Insert(const ExtensionId& extension_id, int process_id) {
  return items_.emplace(extension_id, process_id).second;
}

int ProcessMap::RemoveAllFromProcess(int process_id) {
  return std::erase_if(
      items_, [&](const auto& item) { return item.second == process_id; });
}

bool ProcessMap::Contains(const ExtensionId& extension_id,
                          int process_id) const {
  return items_.contains({extension_id, process_id});
}

bool ProcessMap::Contains(int process_id) const {
  return base::Contains(items_, process_id, &Item::second);
}

std::set<ExtensionId> ProcessMap::GetExtensionsInProcess(
    int process_id_in) const {
  std::set<ExtensionId> result;
  for (const auto& [extension_id, process_id] : items_) {
    if (process_id == process_id_in) {
      result.insert(extension_id);
    }
  }
  return result;
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
      // TODO(crbug.com/1186557): This could be better by looking at
      // ScriptInjectionTracker, as we do for user scripts below.
      return !!extension;
    case mojom::ContextType::kUserScript:
      return extension &&
             ScriptInjectionTracker::DidProcessRunUserScriptFromExtension(
                 process, extension->id());
    case mojom::ContextType::kLockscreenExtension:
      // Lock screen contexts are essentially blessed contexts that run on the
      // lock screen profile. We don't run component hosted apps there, so no
      // need to allow those.
      return is_lock_screen_context_ && extension &&
             !extension->is_hosted_app() &&
             Contains(extension->id(), process_id);
    case mojom::ContextType::kPrivilegedWebPage:
      // A blessed web page is a (non-component) hosted app process.
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

  // TODO(crbug.com/1055168): Move this into the !extension if statement below
  // or document why we want to return WEBUI_CONTEXT for content scripts in
  // WebUIs.
  if (ProcessHasWebUIBindings(process_id)) {
    return mojom::ContextType::kWebUi;
  }

  if (!extension) {
    // Note that blob/filesystem schemes associated with an inner URL of
    // chrome-untrusted will be considered regular pages.
    if (url && url->SchemeIs(content::kChromeUIUntrustedScheme))
      return mojom::ContextType::kUntrustedWebUi;

    return mojom::ContextType::kWebPage;
  }

  if (!Contains(extension->id(), process_id)) {
    // If the process map doesn't contain the process, it might be an extension
    // frame in a webview.
    // We (deliberately) don't add webview-hosted frames to the process map and
    // don't classify them as BLESSED_EXTENSION_CONTEXTs.
    if (url && extension->origin().IsSameOriginWith(*url) &&
        IsWebViewProcessForExtension(process_id, extension->id())) {
      // Yep, it's an extension frame in a webview.
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

  // TODO(https://crbug.com/1339382): Currently, offscreen document contexts
  // are misclassified as BLESSED_EXTENSION_CONTEXTs. This is not ideal
  // because there is a mismatch between the browser and the renderer), but it's
  // not a security issue because, while offscreen documents have fewer
  // capabilities, this is an API distinction, and not a security enforcement.
  // Offscreen documents run in the same process as the rest of the extension
  // and can message the extension, so could easily - though indirectly -
  // access all the same features.
  // Even so, we should fix this to properly classify offscreen documents (and
  // this would be a problem if offscreen documents ever have access to APIs
  // that BLESSED_EXTENSION_CONTEXTs don't).

  return is_lock_screen_context_ ? mojom::ContextType::kLockscreenExtension
                                 : mojom::ContextType::kPrivilegedExtension;
}

}  // namespace extensions
