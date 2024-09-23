// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_DEVTOOLS_PROXY_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_DEVTOOLS_PROXY_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/unguessable_token.h"
#include "content/browser/web_package/signed_exchange_error.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

class GURL;

namespace base {
class UnguessableToken;
}  // namespace base

namespace net {
class SSLInfo;
class X509Certificate;
}  // namespace net

namespace network {
struct ResourceRequest;
struct URLLoaderCompletionStatus;
}  // namespace network

namespace content {
class SignedExchangeEnvelope;

// SignedExchangeDevToolsProxy lives on the IO thread and sends messages to
// DevTools via the UI thread to show signed exchange related information.
class CONTENT_EXPORT SignedExchangeDevToolsProxy {
 public:
  // When the signed exchange request is a navigation request,
  // |devtools_navigation_token| can be used to find the matching request in
  // DevTools. But when the signed exchange request is a prefetch request, the
  // browser process doesn't know the request id used in DevTools. So DevTools
  // looks up the inflight requests using |outer_request_url| to find the
  // matching request.
  SignedExchangeDevToolsProxy(
      const GURL& outer_request_url,
      network::mojom::URLResponseHeadPtr outer_response_head,
      FrameTreeNodeId frame_tree_node_id,
      std::optional<const base::UnguessableToken> devtools_navigation_token,
      bool report_raw_headers);

  SignedExchangeDevToolsProxy(const SignedExchangeDevToolsProxy&) = delete;
  SignedExchangeDevToolsProxy& operator=(const SignedExchangeDevToolsProxy&) =
      delete;

  ~SignedExchangeDevToolsProxy();

  void ReportError(
      const std::string& message,
      std::optional<SignedExchangeError::FieldIndexPair> error_field);

  void CertificateRequestSent(const base::UnguessableToken& request_id,
                              const network::ResourceRequest& request);
  void CertificateResponseReceived(const base::UnguessableToken& request_id,
                                   const GURL& url,
                                   const network::mojom::URLResponseHead& head);
  void CertificateRequestCompleted(
      const base::UnguessableToken& request_id,
      const network::URLLoaderCompletionStatus& status);

  void OnSignedExchangeReceived(
      const std::optional<SignedExchangeEnvelope>& envelope,
      const scoped_refptr<net::X509Certificate>& certificate,
      const std::optional<net::SSLInfo>& ssl_info);

 private:
  const GURL outer_request_url_;
  const network::mojom::URLResponseHeadPtr outer_response_;
  const FrameTreeNodeId frame_tree_node_id_;
  const std::optional<const base::UnguessableToken> devtools_navigation_token_;
  const bool devtools_enabled_;
  std::vector<SignedExchangeError> errors_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_DEVTOOLS_PROXY_H_
