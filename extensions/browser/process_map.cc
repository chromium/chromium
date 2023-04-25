// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/process_map.h"

#include <tuple>

#include "content/public/browser/child_process_security_policy.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/browser/process_map_factory.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"

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

// Item
struct ProcessMap::Item {
  Item(const std::string& extension_id,
       int process_id,
       content::SiteInstanceId site_instance_id)
      : extension_id(extension_id),
        process_id(process_id),
        site_instance_id(site_instance_id) {}

  Item(const Item&) = delete;
  Item& operator=(const Item&) = delete;

  ~Item() = default;

  Item(ProcessMap::Item&&) = default;
  Item& operator=(ProcessMap::Item&&) = default;

  bool operator<(const ProcessMap::Item& other) const {
    return std::tie(extension_id, process_id, site_instance_id) <
           std::tie(other.extension_id, other.process_id,
                    other.site_instance_id);
  }

  std::string extension_id;
  int process_id = 0;
  content::SiteInstanceId site_instance_id;
};


// ProcessMap
ProcessMap::ProcessMap() = default;

ProcessMap::~ProcessMap() = default;

// static
ProcessMap* ProcessMap::Get(content::BrowserContext* browser_context) {
  return ProcessMapFactory::GetForBrowserContext(browser_context);
}

bool ProcessMap::Insert(const std::string& extension_id,
                        int process_id,
                        content::SiteInstanceId site_instance_id) {
  return items_.insert(Item(extension_id, process_id, site_instance_id)).second;
}

bool ProcessMap::Remove(const std::string& extension_id,
                        int process_id,
                        content::SiteInstanceId site_instance_id) {
  return items_.erase(Item(extension_id, process_id, site_instance_id)) > 0;
}

int ProcessMap::RemoveAllFromProcess(int process_id) {
  int result = 0;
  for (auto iter = items_.begin(); iter != items_.end();) {
    if (iter->process_id == process_id) {
      items_.erase(iter++);
      ++result;
    } else {
      ++iter;
    }
  }
  return result;
}

bool ProcessMap::Contains(const std::string& extension_id,
                          int process_id) const {
  for (auto iter = items_.cbegin(); iter != items_.cend(); ++iter) {
    if (iter->process_id == process_id && iter->extension_id == extension_id)
      return true;
  }
  return false;
}

bool ProcessMap::Contains(int process_id) const {
  for (auto iter = items_.cbegin(); iter != items_.cend(); ++iter) {
    if (iter->process_id == process_id)
      return true;
  }
  return false;
}

std::set<std::string> ProcessMap::GetExtensionsInProcess(int process_id) const {
  std::set<std::string> result;
  for (auto iter = items_.cbegin(); iter != items_.cend(); ++iter) {
    if (iter->process_id == process_id)
      result.insert(iter->extension_id);
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

bool ProcessMap::CanProcessHostContextType(const Extension* extension,
                                           int process_id,
                                           Feature::Context context_type) {
  switch (context_type) {
    case Feature::UNSPECIFIED_CONTEXT:
      // We never consider unspecified contexts valid. Even though they would be
      // permissionless, they should never be able to make a request to the
      // browser.
      return false;
    case Feature::OFFSCREEN_EXTENSION_CONTEXT:
    case Feature::BLESSED_EXTENSION_CONTEXT:
      // Offscreen documents run in the main extension process, so both of these
      // require a privileged extension process.
      return extension && IsPrivilegedExtensionProcess(*extension, process_id);
    case Feature::UNBLESSED_EXTENSION_CONTEXT:
      return extension &&
             IsWebViewProcessForExtension(process_id, extension->id());
    case Feature::CONTENT_SCRIPT_CONTEXT:
    case Feature::USER_SCRIPT_CONTEXT:
      // Currently, we assume any process can host a content script or user
      // script.
      // TODO(crbug.com/1186557): This could be better by looking at
      // ContentScriptTracker for (which also means hooking user scripts up to
      // ContentScriptTracker).
      return !!extension;
    case Feature::LOCK_SCREEN_EXTENSION_CONTEXT:
      // Lock screen contexts are essentially blessed contexts that run on the
      // lock screen profile. We don't run component hosted apps there, so no
      // need to allow those.
      return is_lock_screen_context_ && extension &&
             !extension->is_hosted_app() &&
             Contains(extension->id(), process_id);
    case Feature::BLESSED_WEB_PAGE_CONTEXT:
      // A blessed web page is a (non-component) hosted app process.
      return extension && extension->is_hosted_app() &&
             extension->location() != mojom::ManifestLocation::kComponent &&
             Contains(extension->id(), process_id);
    case Feature::WEBUI_UNTRUSTED_CONTEXT:
      // Unfortunately, we have no way of checking if a *process* can host
      // untrusted webui contexts. Callers should look at (ideally, the
      // browser-verified) origin.
      [[fallthrough]];
    case Feature::WEB_PAGE_CONTEXT:
      // Any context not associated with an extension, not running in an
      // extension process, and without webui bindings can be considered a
      // web page process.
      return !extension && !Contains(process_id) &&
             !ProcessHasWebUIBindings(process_id);
    case Feature::WEBUI_CONTEXT:
      // Don't consider extensions in webui (like content scripts) to be
      // webui.
      return !extension && ProcessHasWebUIBindings(process_id);
  }
}

Feature::Context ProcessMap::GetMostLikelyContextType(
    const Extension* extension,
    int process_id,
    const GURL* url) const {
  // WARNING: This logic must match ScriptContextSet::ClassifyJavaScriptContext,
  // as much as possible.

  // TODO(crbug.com/1055168): Move this into the !extension if statement below
  // or document why we want to return WEBUI_CONTEXT for content scripts in
  // WebUIs.
  if (ProcessHasWebUIBindings(process_id)) {
    return Feature::WEBUI_CONTEXT;
  }

  if (!extension) {
    // Note that blob/filesystem schemes associated with an inner URL of
    // chrome-untrusted will be considered regular pages.
    if (url && url->SchemeIs(content::kChromeUIUntrustedScheme))
      return Feature::WEBUI_UNTRUSTED_CONTEXT;

    return Feature::WEB_PAGE_CONTEXT;
  }

  if (!Contains(extension->id(), process_id)) {
    // If the process map doesn't contain the process, it might be an extension
    // frame in a webview.
    // We (deliberately) don't add webview-hosted frames to the process map and
    // don't classify them as BLESSED_EXTENSION_CONTEXTs.
    if (url && extension->origin().IsSameOriginWith(*url) &&
        IsWebViewProcessForExtension(process_id, extension->id())) {
      // Yep, it's an extension frame in a webview.
      return Feature::UNBLESSED_EXTENSION_CONTEXT;
    }

    // Otherwise, it's a content script (the context in which an extension can
    // run in an unassociated, non-webview process).
    return Feature::CONTENT_SCRIPT_CONTEXT;
  }

  if (extension->is_hosted_app() &&
      extension->location() != mojom::ManifestLocation::kComponent) {
    return Feature::BLESSED_WEB_PAGE_CONTEXT;
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

  return is_lock_screen_context_ ? Feature::LOCK_SCREEN_EXTENSION_CONTEXT
                                 : Feature::BLESSED_EXTENSION_CONTEXT;
}

}  // namespace extensions
