// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/process_map.h"

#include <tuple>

#include "content/public/browser/child_process_security_policy.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map_factory.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"

namespace extensions {

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

  ~Item() {
  }

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
ProcessMap::ProcessMap() {
}

ProcessMap::~ProcessMap() {
}

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

Feature::Context ProcessMap::GetMostLikelyContextType(
    const Extension* extension,
    int process_id,
    const GURL* url) const {
  // WARNING: This logic must match ScriptContextSet::ClassifyJavaScriptContext,
  // as much as possible.

  // TODO(crbug.com/1055168): Move this into the !extension if statement below
  // or document why we want to return WEBUI_CONTEXT for content scripts in
  // WebUIs.
  // TODO(crbug.com/1055656): HasWebUIBindings does not always return true for
  // WebUIs. This should be changed to use something else.
  if (content::ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
          process_id)) {
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
    // This could equally be UNBLESSED_EXTENSION_CONTEXT, but we don't record
    // which processes have extension frames in them.
    // TODO(kalman): Investigate this.
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
