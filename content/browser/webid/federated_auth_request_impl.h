// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_IMPL_H_
#define CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_IMPL_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_service_base.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

namespace content {

class RenderFrameHost;

// FederatedAuthRequestImpl handles mojo connections from the renderer to
// fulfill WebID-related requests.
//
// In practice, it is owned and managed by a RenderFrameHost. It accomplishes
// that via subclassing FrameServiceBase, which observes the lifecycle of a
// RenderFrameHost and manages it own memory.
// Create() creates a self-managed instance of FederatedAuthRequestImpl and
// binds it to the receiver.
class CONTENT_EXPORT FederatedAuthRequestImpl
    : public FrameServiceBase<blink::mojom::FederatedAuthRequest> {
 public:
  static void Create(RenderFrameHost*,
                     mojo::PendingReceiver<blink::mojom::FederatedAuthRequest>);

  ~FederatedAuthRequestImpl() override;

  FederatedAuthRequestImpl(const FederatedAuthRequestImpl&) = delete;
  FederatedAuthRequestImpl& operator=(const FederatedAuthRequestImpl&) = delete;

  // blink::mojom::FederatedAuthRequest:
  void RequestIdToken(const ::GURL& provider, RequestIdTokenCallback) override;

 private:
  FederatedAuthRequestImpl(
      RenderFrameHost*,
      mojo::PendingReceiver<blink::mojom::FederatedAuthRequest>);

  base::WeakPtrFactory<FederatedAuthRequestImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_IMPL_H_
