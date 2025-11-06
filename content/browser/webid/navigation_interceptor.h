// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_NAVIGATION_INTERCEPTOR_H_
#define CONTENT_BROWSER_WEBID_NAVIGATION_INTERCEPTOR_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/weak_document_ptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/structured_headers.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

namespace content {

class NavigationThrottleRegistry;
class RenderFrameHost;

namespace webid {

// The NavigationInterceptor enables Identity Providers to control
// navigations to their endpoints by cancelling it and replacing it
// with an inline FedCM flow instead.
class CONTENT_EXPORT NavigationInterceptor
    : public content::NavigationThrottle {
 public:
  class RequestBuilder {
   public:
    ~RequestBuilder() = default;
    CONTENT_EXPORT std::optional<
        std::vector<blink::mojom::IdentityProviderGetParametersPtr>>
    Build(const net::structured_headers::Dictionary& dictionary);
  };

  class ResponseBuilder {
   public:
    ResponseBuilder() = default;
    CONTENT_EXPORT std::optional<content::NavigationController::LoadURLParams>
    Build(const base::Value& response);
  };

  using RequestServiceBuilder =
      base::RepeatingCallback<mojo::Remote<blink::mojom::FederatedAuthRequest>(
          content::RenderFrameHost* rfh)>;

  explicit NavigationInterceptor(NavigationThrottleRegistry& registry);
  NavigationInterceptor(NavigationThrottleRegistry& registry,
                        RequestServiceBuilder service_builder);
  ~NavigationInterceptor() override;

  NavigationInterceptor(const NavigationInterceptor&) = delete;
  NavigationInterceptor& operator=(const NavigationInterceptor&) = delete;

  // content::NavigationThrottle overrides:
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

  static void MaybeCreateAndAdd(NavigationThrottleRegistry& registry);

 private:
  void OnHeaderParsed(
      content::WeakDocumentPtr doc_ptr,
      base::expected<net::structured_headers::Dictionary, std::string> result);
  void OnTokenResponse(
      blink::mojom::RequestTokenStatus status,
      const std::optional<GURL>& selected_identity_provider_config_url,
      std::optional<base::Value> token,
      blink::mojom::TokenErrorPtr error,
      bool is_auto_selected);

  RequestServiceBuilder service_builder_;
  mojo::Remote<blink::mojom::FederatedAuthRequest> remote_;
  base::WeakPtrFactory<NavigationInterceptor> weak_ptr_factory_{this};
};

}  // namespace webid
}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_NAVIGATION_INTERCEPTOR_H_
