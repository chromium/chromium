// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/extension_downloader_delegate.h"

#include "base/version.h"

namespace extensions {

ExtensionDownloaderDelegate::PingResult::PingResult() : did_ping(false) {}

ExtensionDownloaderDelegate::PingResult::~PingResult() = default;

ExtensionDownloaderDelegate::FailureData::FailureData()
    : network_error_code(0), fetch_tries(0) {}
ExtensionDownloaderDelegate::FailureData::FailureData(
    const FailureData& other) = default;
ExtensionDownloaderDelegate::FailureData::FailureData(const int net_error_code,
                                                      const int fetch_attempts)
    : network_error_code(net_error_code), fetch_tries(fetch_attempts) {}

ExtensionDownloaderDelegate::FailureData::FailureData(
    const int net_error_code,
    const base::Optional<int> response,
    const int fetch_attempts)
    : network_error_code(net_error_code),
      response_code(response),
      fetch_tries(fetch_attempts) {}

ExtensionDownloaderDelegate::FailureData::FailureData(
    ManifestInvalidError manifest_invalid_error)
    : manifest_invalid_error(manifest_invalid_error) {}

ExtensionDownloaderDelegate::FailureData::FailureData(
    ManifestInvalidError manifest_invalid_error,
    const std::string& app_status_error)
    : manifest_invalid_error(manifest_invalid_error),
      app_status_error(app_status_error) {}

ExtensionDownloaderDelegate::FailureData::FailureData(
    const std::string& additional_info)
    : additional_info(additional_info) {}

ExtensionDownloaderDelegate::FailureData::~FailureData() = default;

ExtensionDownloaderDelegate::~ExtensionDownloaderDelegate() = default;

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
    const std::set<int>& request_id,
    const FailureData& data) {}

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
