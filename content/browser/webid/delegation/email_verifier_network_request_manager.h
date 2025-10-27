// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_DELEGATION_EMAIL_VERIFIER_NETWORK_REQUEST_MANAGER_H_
#define CONTENT_BROWSER_WEBID_DELEGATION_EMAIL_VERIFIER_NETWORK_REQUEST_MANAGER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/webid/network_request_manager.h"
#include "content/common/content_export.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class RenderFrameHostImpl;

namespace webid {

// Manages network requests for fetching email verification well-known file and
// token.
class CONTENT_EXPORT EmailVerifierNetworkRequestManager
    : public NetworkRequestManager {
 public:
  struct CONTENT_EXPORT WellKnown {
    WellKnown();
    ~WellKnown();
    WellKnown(const WellKnown&);
    GURL issuance_endpoint;
  };

  struct CONTENT_EXPORT TokenResult {
    TokenResult();
    ~TokenResult();
    TokenResult(const TokenResult&) = delete;
    TokenResult& operator=(const TokenResult&) = delete;
    TokenResult(TokenResult&&);
    TokenResult& operator=(TokenResult&&) = default;

    std::optional<base::Value> token;
  };

  using FetchWellKnownCallback =
      base::OnceCallback<void(FetchStatus, WellKnown)>;
  using TokenRequestCallback =
      base::OnceCallback<void(FetchStatus, TokenResult&&)>;

  static std::unique_ptr<EmailVerifierNetworkRequestManager> Create(
      RenderFrameHostImpl* host);

  EmailVerifierNetworkRequestManager(
      const url::Origin& relying_party_origin,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      network::mojom::ClientSecurityStatePtr client_security_state,
      content::FrameTreeNodeId frame_tree_node_id);
  ~EmailVerifierNetworkRequestManager() override;

  EmailVerifierNetworkRequestManager(
      const EmailVerifierNetworkRequestManager&) = delete;
  EmailVerifierNetworkRequestManager& operator=(
      const EmailVerifierNetworkRequestManager&) = delete;

  // Fetches the well-known file for email verification.
  virtual void FetchWellKnown(const GURL& provider, FetchWellKnownCallback);

  virtual void SendTokenRequest(const GURL& token_url,
                                const std::string& url_encoded_post_data,
                                TokenRequestCallback callback);

 private:
  // NetworkRequestManager.
  net::NetworkTrafficAnnotationTag CreateTrafficAnnotation() override;

  using DownloadCallback =
      base::OnceCallback<void(std::unique_ptr<std::string> response_body,
                              int response_code,
                              const std::string& mime_type,
                              bool cors_error)>;

  base::WeakPtrFactory<EmailVerifierNetworkRequestManager> weak_ptr_factory_{
      this};
};

}  // namespace webid
}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_DELEGATION_EMAIL_VERIFIER_NETWORK_REQUEST_MANAGER_H_
