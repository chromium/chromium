// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/extension_downloader_delegate.h"

#include "base/logging.h"
#include "base/version.h"

namespace extensions {

ExtensionDownloaderDelegate::PingResult::PingResult() : did_ping(false) {
}

ExtensionDownloaderDelegate::PingResult::~PingResult() {
}

ExtensionDownloaderDelegate::~ExtensionDownloaderDelegate() {
}

void ExtensionDownloaderDelegate::OnExtensionDownloadStageChanged(
    const ExtensionId& id,
    Stage stage) {}

void ExtensionDownloaderDelegate::OnExtensionDownloadCacheStatusRetrieved(
    const ExtensionId& id,
    CacheStatus cache_status) {}

void ExtensionDownloaderDelegate::OnExtensionDownloadFailed(
    const ExtensionId& id,
    Error error,
    const PingResult& ping_result,
    const std::set<int>& request_id) {}

void ExtensionDownloaderDelegate::OnExtensionDownloadRetryForTests() {}

bool ExtensionDownloaderDelegate::GetPingDataForExtension(
    const ExtensionId& id,
    ManifestFetchData::PingData* ping) {
  return false;
}

std::string ExtensionDownloaderDelegate::GetUpdateUrlData(
    const ExtensionId& id) {
  return std::string();
}

}  // namespace extensions
