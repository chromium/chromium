// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FEDERATED_AUTH_RESPONSE_IMPL_H_
#define CONTENT_BROWSER_WEBID_FEDERATED_AUTH_RESPONSE_IMPL_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_service_base.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/webid/federated_auth_response.mojom.h"

namespace content {

class RenderFrameHost;

// FederatedAuthResponseImpl handles mojo connections from the renderer to
// fulfill WebID-related response by an IDP.
class FederatedAuthResponseImpl
    : public FrameServiceBase<blink::mojom::FederatedAuthResponse> {
 public:
  // Creates a self-managed instance of FederatedAuthResponseImpl and binds it
  // to the receiver.
  static void Create(
      RenderFrameHost*,
      mojo::PendingReceiver<blink::mojom::FederatedAuthResponse>);

  FederatedAuthResponseImpl(const FederatedAuthResponseImpl&) = delete;
  FederatedAuthResponseImpl& operator=(const FederatedAuthResponseImpl&) =
      delete;
  ~FederatedAuthResponseImpl() override;

  void ProvideIdToken(const std::string& id_token,
                      ProvideIdTokenCallback) override;

 private:
  FederatedAuthResponseImpl(
      RenderFrameHost*,
      mojo::PendingReceiver<blink::mojom::FederatedAuthResponse>);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FEDERATED_AUTH_RESPONSE_IMPL_H_