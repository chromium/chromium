// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_KEEP_ALIVE_ATTRIBUTION_REQUEST_HELPER_H_
#define CONTENT_BROWSER_LOADER_KEEP_ALIVE_ATTRIBUTION_REQUEST_HELPER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/browser/attribution_reporting/attribution_background_registrations_id.h"
#include "content/browser/attribution_reporting/attribution_suitable_context.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/attribution_reporting_runtime_features.h"
#include "services/network/public/mojom/attribution.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace network {
class TriggerVerification;
}  // namespace network

namespace content {

class AttributionDataHostManager;

// Handles Attribution Reporting API
// (https://github.com/WICG/attribution-reporting-api) background source or
// trigger registrations. It is meant to be optionally hooked to a
// `KeepAliveURLLoader` instance. This enables registrations to be successfully
// processed even when the renderer which initiated the request is no longer
// active.
//
// A helper instance can handle a single request chain.
class CONTENT_EXPORT KeepAliveAttributionRequestHelper {
 public:
  // Creates a `KeepAliveAttributionRequestHelper` instance when the request is
  // eligible for attribution.
  static std::unique_ptr<KeepAliveAttributionRequestHelper> CreateIfNeeded(
      network::mojom::AttributionReportingEligibility,
      const GURL& request_url,
      const absl::optional<base::UnguessableToken>& attribution_src_token,
      const absl::optional<std::string>& devtools_request_id,
      network::AttributionReportingRuntimeFeatures,
      const AttributionSuitableContext&);

  ~KeepAliveAttributionRequestHelper();

  // non-copyable and non-movable
  KeepAliveAttributionRequestHelper(const KeepAliveAttributionRequestHelper&) =
      delete;
  KeepAliveAttributionRequestHelper& operator=(
      const KeepAliveAttributionRequestHelper&) = delete;

  void OnReceiveRedirect(const net::HttpResponseHeaders* headers,
                         const std::vector<network::TriggerVerification>&,
                         const GURL& redirect_url);
  void OnReceiveResponse(const net::HttpResponseHeaders* headers,
                         const std::vector<network::TriggerVerification>&);

 private:
  friend class KeepAliveAttributionRequestHelperTestPeer;

  KeepAliveAttributionRequestHelper(
      BackgroundRegistrationsId,
      AttributionDataHostManager*,
      const GURL& reporting_url,
      network::AttributionReportingRuntimeFeatures runtime_features);

  BackgroundRegistrationsId id_;

  base::WeakPtr<AttributionDataHostManager> attribution_data_host_manager_;

  network::AttributionReportingRuntimeFeatures runtime_features_;

  // Reporting url of the ongoing request, it is updated on redirection. The url
  // might be suitable or not, if it is not, when receiving a response, it will
  // be ignored.
  GURL reporting_url_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_KEEP_ALIVE_ATTRIBUTION_REQUEST_HELPER_H_
