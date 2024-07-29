// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/common/api_key_request_util.h"

#include "base/version_info/channel.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/resource_request.h"

namespace {

// See https://cloud.google.com/apis/docs/system-parameters
constexpr char kApiKeyHeaderName[] = "X-Goog-Api-Key";

}  // namespace

namespace google_apis {

void AddDefaultAPIKeyToRequest(network::ResourceRequest& request,
                               version_info::Channel channel) {
  AddAPIKeyToRequest(request, GetAPIKey(channel));
}

void AddAPIKeyToRequest(network::ResourceRequest& request,
                        const std::string& api_key) {
  // TODO(b/355544759): check that no Authorization header is present.
  // TODO(b/355544759): check that the API isn't present as a URL query param.
  // Don't use CHECK for validation, to make migrating to this API less risky.
  if (api_key.empty()) {
    DLOG(FATAL) << "API key cannot be empty.";
    return;
  }
  DLOG_ASSERT(!request.headers.HasHeader(kApiKeyHeaderName))
      << "API key already present on the request.";

  request.headers.SetHeader(kApiKeyHeaderName, api_key);
}

}  // namespace google_apis
