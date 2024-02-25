// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/extension_downloader_delegate.h"

#include "base/version.h"
#include "net/base/net_errors.h"

namespace extensions {

ExtensionDownloaderDelegate::PingResult::PingResult() : did_ping(false) {}

ExtensionDownloaderDelegate::PingResult::~PingResult() = default;

ExtensionDownloaderDelegate::FailureData::FailureData()
    : network_error_code(0), fetch_tries(0) {}

// static
ExtensionDownloaderDelegate::FailureData
ExtensionDownloaderDelegate::FailureData::CreateFromNetworkResponse(
    int net_error,
    int response_code,
    int failure_count) {
  return ExtensionDownloaderDelegate::FailureData(
      -net_error,
      (net_error == net::Error::ERR_HTTP_RESPONSE_CODE_FAILURE &&
       response_code > 0)
          ? std::optional<int>(response_code)
          : std::nullopt,
      failure_count);
}

ExtensionDownloaderDelegate::FailureData::FailureData(
    const FailureData& other) = default;
ExtensionDownloaderDelegate::FailureData::FailureData(const int net_error_code,
                                                      const int fetch_attempts)
    : network_error_code(net_error_code), fetch_tries(fetch_attempts) {}

ExtensionDownloaderDelegate::FailureData::FailureData(
    const int net_error_code,
    const std::optional<int> response,
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

void ExtensionDownloaderDelegate::OnExtensionUpdateFound(
    const ExtensionId& id,
    const std::set<int>& request_ids,
    const base::Version& version) {}

void ExtensionDownloaderDelegate::OnExtensionDownloadCacheStatusRetrieved(
    const ExtensionId& id,
    CacheStatus cache_status) {}

void ExtensionDownloaderDelegate::OnExtensionDownloadFailed(
    const ExtensionId& id,
    Error error,
    const PingResult& ping_result,
    const std::set<int>& request_id,
    const FailureData& data) {}

void ExtensionDownloaderDelegate::OnExtensionDownloadRetry(
    const ExtensionId& id,
    const FailureData& data) {}

void ExtensionDownloaderDelegate::OnExtensionDownloadRetryForTests() {}

bool ExtensionDownloaderDelegate::GetPingDataForExtension(
    const ExtensionId& id,
    DownloadPingData* ping) {
  return false;
}

ExtensionDownloaderDelegate::RequestRollbackResult
ExtensionDownloaderDelegate::RequestRollback(const ExtensionId& id) {
  return RequestRollbackResult::kDisallowed;
}

}  // namespace extensions
