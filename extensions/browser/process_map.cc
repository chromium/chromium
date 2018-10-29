// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/process_map.h"

#include <tuple>

#include "content/public/browser/child_process_security_policy.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map_factory.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"

namespace extensions {

// Item
struct ProcessMap::Item {
  Item() : process_id(0), site_instance_id(0) {
  }

  // Purposely implicit constructor needed on older gcc's. See:
  // http://codereview.chromium.org/8769022/
  explicit Item(const ProcessMap::Item& other)
      : extension_id(other.extension_id),
        process_id(other.process_id),
        site_instance_id(other.site_instance_id) {
  }

  Item(const std::string& extension_id, int process_id,
       int site_instance_id)
      : extension_id(extension_id),
        process_id(process_id),
        site_instance_id(site_instance_id) {
  }

  ~Item() {
  }

  bool operator<(const ProcessMap::Item& other) const {
    return std::tie(extension_id, process_id, site_instance_id) <
           std::tie(other.extension_id, other.process_id,
                    other.site_instance_id);
  }

  std::string extension_id;
  int process_id;
  int site_instance_id;
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

bool ProcessMap::Insert(const std::string& extension_id, int process_id,
                        int site_instance_id) {
  return items_.insert(Item(extension_id, process_id, site_instance_id)).second;
}

bool ProcessMap::Remove(const std::string& extension_id, int process_id,
                        int site_instance_id) {
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
    int process_id) const {
  // WARNING: This logic must match ScriptContextSet::ClassifyJavaScriptContext,
  // as much as possible.

  if (content::ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
          process_id)) {
    return Feature::WEBUI_CONTEXT;
  }

  if (!extension) {
    return Feature::WEB_PAGE_CONTEXT;
  }

  if (!Contains(extension->id(), process_id)) {
    // This could equally be UNBLESSED_EXTENSION_CONTEXT, but we don't record
    // which processes have extension frames in them.
    // TODO(kalman): Investigate this.
    return Feature::CONTENT_SCRIPT_CONTEXT;
  }

  if (extension->is_hosted_app() &&
      extension->location() != Manifest::COMPONENT) {
    return Feature::BLESSED_WEB_PAGE_CONTEXT;
  }

  return is_lock_screen_context_ ? Feature::LOCK_SCREEN_EXTENSION_CONTEXT
                                 : Feature::BLESSED_EXTENSION_CONTEXT;
}

}  // namespace extensions
