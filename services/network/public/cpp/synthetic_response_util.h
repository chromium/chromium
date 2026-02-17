// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SYNTHETIC_RESPONSE_UTIL_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SYNTHETIC_RESPONSE_UTIL_H_

#include <vector>

#include "base/component_export.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace network {

// Returns true if the URLLoader should perform a fallback on response start.
// TODO(crbug.com/447039330): This is temporary for the SyntheticResponse
// experiment and will be removed after standardization.
COMPONENT_EXPORT(NETWORK_CPP)
bool CheckHeaderConsistencyForSyntheticResponse(
    const net::HttpResponseHeaders& actual_headers,
    const net::HttpResponseHeaders& expected_headers);

COMPONENT_EXPORT(NETWORK_CPP)
bool CheckHeaderConsistencyForSyntheticResponseForTesting(
    const net::HttpResponseHeaders& actual_headers,
    const net::HttpResponseHeaders& expected_headers,
    const std::vector<std::string>& ignored_headers);

struct WriteSyntheticResponseFallbackResult {
  MojoResult result;
  size_t bytes_written;
};

// Writes the hardcoded fallback body to the provided data pipe.
// TODO(crbug.com/447039330): This is temporary for the SyntheticResponse
// experiment and will be removed after standardization.
COMPONENT_EXPORT(NETWORK_CPP)
WriteSyntheticResponseFallbackResult WriteSyntheticResponseFallbackBody(
    mojo::ScopedDataPipeProducerHandle& response_body_stream);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SYNTHETIC_RESPONSE_UTIL_H_
