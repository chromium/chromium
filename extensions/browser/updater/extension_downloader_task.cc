// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/extension_downloader_task.h"

namespace extensions {

ExtensionDownloaderTask::ExtensionDownloaderTask(
    ExtensionId id,
    GURL update_url,
    mojom::ManifestLocation install_location,
    bool is_corrupt_reinstall,
    int request_id,
    DownloadFetchPriority fetch_priority,
    base::Version version,
    Manifest::Type type,
    std::string update_url_data)
    : id(std::move(id)),
      update_url(std::move(update_url)),
      install_location(install_location),
      is_corrupt_reinstall(is_corrupt_reinstall),
      request_id(request_id),
      fetch_priority(fetch_priority),
      version(std::move(version)),
      type(type),
      update_url_data(std::move(update_url_data)) {
  DCHECK(this->version.IsValid());
}

ExtensionDownloaderTask::ExtensionDownloaderTask(
    ExtensionId id,
    GURL update_url,
    mojom::ManifestLocation install_location,
    bool is_corrupt_reinstall,
    int request_id,
    DownloadFetchPriority fetch_priority)
    : id(std::move(id)),
      update_url(std::move(update_url)),
      install_location(install_location),
      is_corrupt_reinstall(is_corrupt_reinstall),
      request_id(request_id),
      fetch_priority(fetch_priority) {
  DCHECK(this->version.IsValid());
}

ExtensionDownloaderTask::ExtensionDownloaderTask(ExtensionDownloaderTask&&) =
    default;
ExtensionDownloaderTask& ExtensionDownloaderTask::operator=(
    ExtensionDownloaderTask&&) = default;
ExtensionDownloaderTask::~ExtensionDownloaderTask() = default;

void ExtensionDownloaderTask::OnStageChanged(
    ExtensionDownloaderDelegate::Stage stage) {
  if (delegate) {
    delegate->OnExtensionDownloadStageChanged(id, stage);
  }
}

}  // namespace extensions
