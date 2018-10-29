// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mock_external_provider.h"

#include <memory>

#include "base/version.h"
#include "extensions/browser/external_install_info.h"
#include "extensions/common/extension.h"

namespace extensions {

MockExternalProvider::MockExternalProvider(VisitorInterface* visitor,
                                           Manifest::Location location)
    : location_(location), visitor_(visitor), visit_count_(0) {}

MockExternalProvider::~MockExternalProvider() {}

void MockExternalProvider::UpdateOrAddExtension(const ExtensionId& id,
                                                const std::string& version_str,
                                                const base::FilePath& path) {
  auto info = std::make_unique<ExternalInstallInfoFile>(
      id, base::Version(version_str), path, location_, Extension::NO_FLAGS,
      false, false);
  UpdateOrAddExtension(std::move(info));
}

void MockExternalProvider::UpdateOrAddExtension(
    std::unique_ptr<ExternalInstallInfoFile> info) {
  const std::string& id = info->extension_id;
  CHECK(url_extension_map_.find(id) == url_extension_map_.end());
  file_extension_map_[id] = std::move(info);
}

void MockExternalProvider::UpdateOrAddExtension(
    std::unique_ptr<ExternalInstallInfoUpdateUrl> info) {
  const std::string& id = info->extension_id;
  CHECK(file_extension_map_.find(id) == file_extension_map_.end());
  url_extension_map_[id] = std::move(info);
}

void MockExternalProvider::RemoveExtension(const ExtensionId& id) {
  file_extension_map_.erase(id);
  url_extension_map_.erase(id);
}

void MockExternalProvider::VisitRegisteredExtension() {
  visit_count_++;
  for (const auto& extension_kv : file_extension_map_)
    visitor_->OnExternalExtensionFileFound(*extension_kv.second);
  for (const auto& extension_kv : url_extension_map_)
    visitor_->OnExternalExtensionUpdateUrlFound(*extension_kv.second,
                                                true /* is_initial_load */);
  visitor_->OnExternalProviderReady(this);
}

bool MockExternalProvider::HasExtension(const std::string& id) const {
  return file_extension_map_.find(id) != file_extension_map_.end() ||
         url_extension_map_.find(id) != url_extension_map_.end();
}

bool MockExternalProvider::GetExtensionDetails(
    const std::string& id,
    Manifest::Location* location,
    std::unique_ptr<base::Version>* version) const {
  auto it1 = file_extension_map_.find(id);
  auto it2 = url_extension_map_.find(id);

  // |id| can't be on both |file_extension_map_| and |url_extension_map_|.
  if (it1 == file_extension_map_.end() && it2 == url_extension_map_.end())
    return false;

  // Only ExternalInstallInfoFile has version.
  if (version && it1 != file_extension_map_.end())
    version->reset(new base::Version(it1->second->version));

  if (location)
    *location = location_;

  return true;
}

bool MockExternalProvider::IsReady() const {
  return true;
}

}  // namespace extensions
