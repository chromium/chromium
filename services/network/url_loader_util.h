// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_URL_LOADER_UTIL_H_
#define SERVICES_NETWORK_URL_LOADER_UTIL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/files/file.h"
#include "net/base/isolation_info.h"
#include "net/cookies/cookie_setting_override.h"

namespace base {
class SequencedTaskRunner;
}  // namespace  base

namespace net {
class HttpRequestHeaders;
class HttpResponseInfo;
class UploadDataStream;
class URLRequest;
}  // namespace  net

namespace network {

namespace mojom {
enum class RequestDestination;
}  // namespace mojom

struct ResourceRequest;
class ResourceRequestBody;

namespace url_loader_util {

// Creates a net::UploadDataStream from the passed `body` and `opened_files`.
// `file_task_runner` will be used for reading file elements in the `body`.
std::unique_ptr<net::UploadDataStream> CreateUploadDataStream(
    ResourceRequestBody* body,
    std::vector<base::File>& opened_files,
    base::SequencedTaskRunner* file_task_runner);

// Computes the CookieSettingOverrides to use for a given `ResourceRequest`.
// May also emit to histograms.
COMPONENT_EXPORT(NETWORK_SERVICE)
net::CookieSettingOverrides CalculateCookieSettingOverrides(
    net::CookieSettingOverrides factory_overrides,
    net::CookieSettingOverrides devtools_overrides,
    const ResourceRequest& request,
    bool emit_metrics);

// Determines the IsolationInfo for a request, checking sources in priority:
// 1. `factory_isolation_info` (if non-empty).
// 2. `request.trusted_params->isolation_info` (if non-empty).
// 3. Auto-created based on `request.url` origin if
//    `automatically_assign_isolation_info` is true.
// Returns `std::nullopt` if none of the above apply.
std::optional<net::IsolationInfo> GetIsolationInfo(
    const net::IsolationInfo& factory_isolation_info,
    bool automatically_assign_isolation_info,
    const ResourceRequest& request);

// Retrieves the Cookie header from either `cors_exempt_headers` or `headers`.
std::string GetCookiesFromHeaders(
    const net::HttpRequestHeaders& headers,
    const net::HttpRequestHeaders& cors_exempt_headers);

// Records UMA histograms for request sizes and categorizes them.
void RecordURLLoaderRequestMetrics(const net::URLRequest& url_request,
                                   size_t raw_request_line_size,
                                   size_t raw_request_headers_size);

// Records UMA metrics related to shared dictionary usage for non-cached
// responses.
void MaybeRecordSharedDictionaryUsedResponseMetrics(
    int error_code,
    network::mojom::RequestDestination destination,
    const net::HttpResponseInfo& response_info,
    bool shared_dictionary_allowed_check_passed);

}  // namespace url_loader_util
}  // namespace network

#endif  // SERVICES_NETWORK_URL_LOADER_UTIL_H_
