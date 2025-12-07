// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_REPORTER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_REPORTER_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "content/browser/web_package/signed_exchange_error.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "net/base/ip_address.h"
#include "net/base/network_isolation_key.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

namespace content {

// SignedExchangeReporter sends a signed exchange report for distributor when
// the distributor of signed exchange has set Network Error Logging (NEL) policy
// using HTTP header.
class CONTENT_EXPORT SignedExchangeReporter {
 public:
  static std::unique_ptr<SignedExchangeReporter> MaybeCreate(
      const GURL& outer_url,
      const std::string& referrer,
      const network::mojom::URLResponseHead& response,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      FrameTreeNodeId frame_tree_node_id);

  SignedExchangeReporter(const SignedExchangeReporter&) = delete;
  SignedExchangeReporter& operator=(const SignedExchangeReporter&) = delete;

  ~SignedExchangeReporter();

  void set_cert_server_ip_address(const net::IPAddress& cert_server_ip_address);
  void set_inner_url(const GURL& inner_url);
  void set_cert_url(const GURL& cert_url);

  // Queues the signed exchange report. This method must be called at the last
  // method to be called on |this|, and must be called only once.
  void ReportLoadResultAndFinish(SignedExchangeLoadResult result);

  void ReportHeaderIntegrityMismatch();

 private:
  SignedExchangeReporter(
      const GURL& outer_url,
      const std::string& referrer,
      const network::mojom::URLResponseHead& response,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      FrameTreeNodeId frame_tree_node_id);

  network::mojom::SignedExchangeReportPtr report_;
  const base::TimeTicks request_start_;
  const net::NetworkAnonymizationKey network_anonymization_key_;
  const FrameTreeNodeId frame_tree_node_id_;
  net::IPAddress cert_server_ip_address_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_REPORTER_H_
