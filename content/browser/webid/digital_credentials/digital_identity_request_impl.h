// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_REQUEST_IMPL_H_
#define CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_REQUEST_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "content/common/content_export.h"
#include "content/public/browser/digital_identity_provider.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
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

  // Returns the type of interstitial to show based on the request contents.
  static std::optional<DigitalIdentityInterstitialType> ComputeInterstitialType(
      const url::Origin& rp_origin,
      const DigitalIdentityProvider* provider,
      const std::optional<std::string>& protocol,
      const data_decoder::DataDecoder::ValueOrError& request);

  DigitalIdentityRequestImpl(const DigitalIdentityRequestImpl&) = delete;
  DigitalIdentityRequestImpl& operator=(const DigitalIdentityRequestImpl&) =
      delete;

  ~DigitalIdentityRequestImpl() override;

  // blink::mojom::DigitalIdentityRequest:
  void Request(std::vector<blink::mojom::DigitalCredentialProviderPtr>
                   digital_credential_providers,
               RequestCallback) override;
  void Abort() override;

 private:
  DigitalIdentityRequestImpl(
      RenderFrameHost&,
      mojo::PendingReceiver<blink::mojom::DigitalIdentityRequest>);

  // Called when the request JSON has been parsed.
  void OnRequestJsonParsed(
      std::optional<std::string> protocol,
      base::Value request_to_send,
      data_decoder::DataDecoder::ValueOrError parsed_result);

  // Called after fetching the user's identity. Shows an interstitial if needed.
  void ShowInterstitialIfNeeded(
      bool is_only_requesting_age,
      base::expected<std::string,
                     DigitalIdentityProvider::RequestStatusForMetrics>
          response);

  // Called when the user has fulfilled the interstitial requirement. Will be
  // called immediately after OnRequestJsonParsed() if no interstitial is
  // needed.
  void OnInterstitialDone(std::optional<std::string> protocol,
                          base::Value request_to_send,
                          DigitalIdentityProvider::RequestStatusForMetrics
                              status_after_interstitial);

  // Infers blink::mojom::RequestDigitalIdentityStatus based on
  // `status_for_metrics`.
  void CompleteRequest(
      std::optional<std::string> protocol,
      const base::expected<std::string,
                           DigitalIdentityProvider::RequestStatusForMetrics>&
          status_for_metrics);

  void CompleteRequestWithError(
      DigitalIdentityProvider::RequestStatusForMetrics status_for_metrics);

  void CompleteRequestWithStatus(
      std::optional<std::string> protocol,
      blink::mojom::RequestDigitalIdentityStatus status,
      const base::expected<std::string,
                           DigitalIdentityProvider::RequestStatusForMetrics>&
          response);

  std::unique_ptr<DigitalIdentityProvider> provider_;
  RequestCallback callback_;

  // Callback which updates interstitial to inform user that the credential
  // request has been aborted.
  DigitalIdentityProvider::DigitalIdentityInterstitialAbortCallback
      update_interstitial_on_abort_callback_;

  base::WeakPtrFactory<DigitalIdentityRequestImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_REQUEST_IMPL_H_
