// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_DEVICE_BOUND_SESSION_REGISTRATION_FETCHER_PARAM_H_
#define NET_DEVICE_BOUND_SESSIONS_DEVICE_BOUND_SESSION_REGISTRATION_FETCHER_PARAM_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "crypto/signature_verifier.h"
#include "net/base/net_export.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "url/gurl.h"

namespace net {

// Class to parse Sec-Session-Registration header.
// See explainer for details:
// https://github.com/WICG/dbsc/blob/main/README.md#start-session
class NET_EXPORT DeviceBoundSessionRegistrationFetcherParam {
 public:
  DeviceBoundSessionRegistrationFetcherParam(
      DeviceBoundSessionRegistrationFetcherParam&& other);
  DeviceBoundSessionRegistrationFetcherParam& operator=(
      DeviceBoundSessionRegistrationFetcherParam&& other) noexcept;

  // Disabled to make accidental copies compile errors.
  DeviceBoundSessionRegistrationFetcherParam(
      const DeviceBoundSessionRegistrationFetcherParam& other) = delete;
  DeviceBoundSessionRegistrationFetcherParam& operator=(
      const DeviceBoundSessionRegistrationFetcherParam&) = delete;
  ~DeviceBoundSessionRegistrationFetcherParam();

  // Returns a vector of valid instances.
  // TODO(chlily): Get IsolationInfo from the request as well
  static std::vector<DeviceBoundSessionRegistrationFetcherParam> CreateIfValid(
      const GURL& request_url,
      const HttpResponseHeaders* headers);

  const GURL& registration_endpoint() const { return registration_endpoint_; }

  base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
  supported_algos() const {
    return supported_algos_;
  }

  const std::string& challenge() const { return challenge_; }

 private:
  DeviceBoundSessionRegistrationFetcherParam(
      GURL registration_endpoint,
      std::vector<crypto::SignatureVerifier::SignatureAlgorithm>
          supported_algos,
      std::string challenge);
  static std::optional<DeviceBoundSessionRegistrationFetcherParam> ParseItem(
      const GURL& request_url,
      structured_headers::Item item,
      structured_headers::Parameters params);

  // TODO(chlily): Store last-updated time and last-updated isolationinfo as
  // needed.
  GURL registration_endpoint_;
  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> supported_algos_;
  std::string challenge_;
};

}  // namespace net

#endif  // NET_DEVICE_BOUND_SESSIONS_DEVICE_BOUND_SESSION_REGISTRATION_FETCHER_PARAM_H_
