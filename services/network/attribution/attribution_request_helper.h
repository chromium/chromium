// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_REQUEST_HELPER_H_
#define SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_REQUEST_HELPER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/attribution.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

namespace net {

class URLRequest;
struct RedirectInfo;

}  // namespace net

namespace network {

struct ResourceRequest;

// Class AttributionRequestHelper handles attribution-reporting-api related
// operations (https://github.com/WICG/attribution-reporting-api) that must
// happen in the network service. It is meant to be optionally hooked to a
// url_loader instance.
class AttributionRequestHelper {
 public:
  // Creates an AttributionRequestHelper instance if needed.
  //
  // Currently it never is needed. The class is kept for future work.
  static std::unique_ptr<AttributionRequestHelper> CreateIfNeeded(
      mojom::AttributionReportingEligibility);

  // Test method which allows to instantiate an AttributionRequestHelper.
  static std::unique_ptr<AttributionRequestHelper> CreateForTesting();

  ~AttributionRequestHelper();

  AttributionRequestHelper(const AttributionRequestHelper&) = delete;
  AttributionRequestHelper& operator=(const AttributionRequestHelper&) = delete;

  // no-op
  void Begin(net::URLRequest& request, base::OnceClosure done);

  // no-op
  void OnReceiveRedirect(
      net::URLRequest& request,
      mojom::URLResponseHeadPtr response,
      const net::RedirectInfo& redirect_info,
      base::OnceCallback<void(mojom::URLResponseHeadPtr response)> done);

  // no-op
  void Finalize(mojom::URLResponseHead& response, base::OnceClosure done);

 private:
  explicit AttributionRequestHelper();
};

// Computes the Attribution Reporting request headers on attribution eligible
// requests. See https://github.com/WICG/attribution-reporting-api.
net::HttpRequestHeaders ComputeAttributionReportingHeaders(
    const ResourceRequest&);

}  // namespace network

#endif  // SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_REQUEST_HELPER_H_
