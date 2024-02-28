// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_REQUEST_IMPL_H_
#define CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_REQUEST_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/digital_identity_provider.h"
#include "content/public/browser/document_service.h"
#include "third_party/blink/public/mojom/webid/digital_identity_request.mojom.h"
#include "url/gurl.h"

namespace content {

class DigitalIdentityProvider;
class RenderFrameHost;

// DigitalIdentityRequestImpl handles mojo connections from the renderer to
// fulfill digital identity requests.
//
// In practice, it is owned and managed by a RenderFrameHost. It accomplishes
// that via subclassing DocumentService, which observes the lifecycle of a
// RenderFrameHost and manages its own memory.
// Create() creates a self-managed instance of DigitalIdentityRequestImpl and
// binds it to the receiver.
class CONTENT_EXPORT DigitalIdentityRequestImpl
    : public DocumentService<blink::mojom::DigitalIdentityRequest> {
 public:
  static void Create(
      RenderFrameHost&,
      mojo::PendingReceiver<blink::mojom::DigitalIdentityRequest>);

  DigitalIdentityRequestImpl(const DigitalIdentityRequestImpl&) = delete;
  DigitalIdentityRequestImpl& operator=(const DigitalIdentityRequestImpl&) =
      delete;

  ~DigitalIdentityRequestImpl() override;

  // blink::mojom::DigitalIdentityRequest:
  void Request(blink::mojom::DigitalCredentialProviderPtr provider,
               RequestCallback) override;
  void Abort() override;

 private:
  DigitalIdentityRequestImpl(
      RenderFrameHost&,
      mojo::PendingReceiver<blink::mojom::DigitalIdentityRequest>);

  // Infers one of [kError, kSuccess] for RequestDigitalIdentityStatus based on
  // `status_for_metrics`.
  void CompleteRequest(
      const std::string& response,
      DigitalIdentityProvider::RequestStatusForMetrics status_for_metrics);

  void CompleteRequestWithStatus(
      blink::mojom::RequestDigitalIdentityStatus status,
      const std::string& response,
      DigitalIdentityProvider::RequestStatusForMetrics status_for_metrics);

  std::unique_ptr<DigitalIdentityProvider> CreateProvider();

  std::unique_ptr<DigitalIdentityProvider> provider_;
  RequestCallback callback_;

  base::WeakPtrFactory<DigitalIdentityRequestImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_REQUEST_IMPL_H_
