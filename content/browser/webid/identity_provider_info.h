// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_IDENTITY_PROVIDER_INFO
#define CONTENT_BROWSER_WEBID_IDENTITY_PROVIDER_INFO

#include <optional>

#include "content/browser/webid/idp_network_request_manager.h"
#include "content/public/browser/webid/identity_request_dialog_controller.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

namespace gfx {
class Image;
}

namespace content::webid {

using IdentityProviderDataPtr = scoped_refptr<content::IdentityProviderData>;

// Class representing the information about an identity provider. Populated
// while fetching.
class IdentityProviderInfo {
 public:
  IdentityProviderInfo(const blink::mojom::IdentityProviderRequestOptionsPtr&,
                       IdpNetworkRequestManager::Endpoints,
                       IdentityProviderMetadata,
                       blink::mojom::RpContext rp_context,
                       blink::mojom::RpMode rp_mode,
                       std::optional<blink::mojom::Format> format);
  ~IdentityProviderInfo();
  IdentityProviderInfo(const IdentityProviderInfo&);

  blink::mojom::IdentityProviderRequestOptionsPtr provider;
  IdpNetworkRequestManager::Endpoints endpoints;
  IdentityProviderMetadata metadata;
  bool has_failing_idp_signin_status{false};
  blink::mojom::RpContext rp_context{blink::mojom::RpContext::kSignIn};
  blink::mojom::RpMode rp_mode{blink::mojom::RpMode::kPassive};
  std::optional<blink::mojom::Format> format;
  IdentityProviderDataPtr data;
  gfx::Image decoded_idp_brand_icon;
  bool client_is_third_party_to_top_frame_origin{false};
};

}  // namespace content::webid

#endif  // CONTENT_BROWSER_WEBID_IDENTITY_PROVIDER_INFO
