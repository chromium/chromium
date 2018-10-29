// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/runtime_data.h"

#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/background_info.h"

namespace extensions {

RuntimeData::RuntimeData(ExtensionRegistry* registry) : registry_(registry) {
  registry_->AddObserver(this);
}

RuntimeData::~RuntimeData() {
  registry_->RemoveObserver(this);
}

bool RuntimeData::IsBackgroundPageReady(const Extension* extension) const {
  if (!BackgroundInfo::HasPersistentBackgroundPage(extension))
    return true;
  return HasFlag(extension->id(), BACKGROUND_PAGE_READY);
}

void RuntimeData::SetBackgroundPageReady(const std::string& extension_id,
                                         bool value) {
  SetFlag(extension_id, BACKGROUND_PAGE_READY, value);
}

bool RuntimeData::IsBeingUpgraded(const std::string& extension_id) const {
  return HasFlag(extension_id, BEING_UPGRADED);
}

void RuntimeData::SetBeingUpgraded(const std::string& extension_id,
                                   bool value) {
  SetFlag(extension_id, BEING_UPGRADED, value);
}

bool RuntimeData::HasExtensionForTesting(
    const std::string& extension_id) const {
  return extension_flags_.find(extension_id) != extension_flags_.end();
}

void RuntimeData::ClearAll() {
  extension_flags_.clear();
}

void RuntimeData::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                      const Extension* extension,
                                      UnloadedExtensionReason reason) {
  auto iter = extension_flags_.find(extension->id());
  if (iter != extension_flags_.end())
    iter->second = iter->second & kPersistAcrossUnloadMask;
}

bool RuntimeData::HasFlag(const std::string& extension_id,
                          RuntimeFlag flag) const {
  auto it = extension_flags_.find(extension_id);
  if (it == extension_flags_.end())
    return false;
  return !!(it->second & flag);
}

void RuntimeData::SetFlag(const std::string& extension_id,
                          RuntimeFlag flag,
                          bool value) {
  if (value)
    extension_flags_[extension_id] |= flag;
  else
    extension_flags_[extension_id] &= ~flag;
}

}  // namespace extensions
