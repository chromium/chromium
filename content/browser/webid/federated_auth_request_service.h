// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_SERVICE_H_
#define CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_SERVICE_H_

#include <memory>
#include <vector>

#include "content/common/content_export.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

namespace content {

class FederatedAuthRequestImpl;
class RenderFrameHostImpl;

// FederatedAuthRequestService handles mojo connections from the renderer to
// fulfill WebID-related requests.
//
// In practice, it is owned and managed by a RenderFrameHost. It accomplishes
// that via subclassing DocumentService, which observes the lifecycle of a
// RenderFrameHost and manages its own memory.
// Create() creates a self-managed instance of FederatedAuthRequestService and
// binds it to the receiver.
class CONTENT_EXPORT FederatedAuthRequestService
    : public DocumentService<blink::mojom::FederatedAuthRequest> {
 public:
  static void Create(RenderFrameHostImpl*,
                     mojo::PendingReceiver<blink::mojom::FederatedAuthRequest>);

  FederatedAuthRequestService(
      RenderFrameHostImpl*,
      mojo::PendingReceiver<blink::mojom::FederatedAuthRequest>);

  FederatedAuthRequestService(const FederatedAuthRequestService&) = delete;
  FederatedAuthRequestService& operator=(const FederatedAuthRequestService&) =
      delete;

  ~FederatedAuthRequestService() override;

  // blink::mojom::FederatedAuthRequest:
  void RequestIdToken(const GURL& provider,
                      const std::string& client_id,
                      const std::string& nonce,
                      bool prefer_auto_sign_in,
                      RequestIdTokenCallback) override;
  void CancelTokenRequest() override;
  void Revoke(const GURL& provider,
              const std::string& client_id,
              const std::string& account_id,
              RevokeCallback callback) override;
  void Logout(const GURL& provider,
              const std::string& account_id,
              LogoutCallback callback) override;
  void LogoutRps(std::vector<blink::mojom::LogoutRpsRequestPtr> logout_requests,
                 LogoutRpsCallback) override;

  FederatedAuthRequestImpl* GetImplForTesting() { return impl_.get(); }

 private:
  std::unique_ptr<FederatedAuthRequestImpl> impl_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_SERVICE_H_
