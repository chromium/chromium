// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FEDCM_REQUEST_SPEC_H_
#define CONTENT_BROWSER_WEBID_FEDCM_REQUEST_SPEC_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_manager.mojom.h"
#include "third_party/blink/public/mojom/webid/federated_request.mojom.h"
#include "url/gurl.h"

namespace content {
class FederatedIdentityPermissionContextDelegate;
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

namespace content::webid {

// Encapsulates the immutable input parameters, ambient browser security
// context, and pre-computed lookup tables for a single FedCM token request.
class CONTENT_EXPORT FedCmRequestSpec
    : public base::RefCounted<FedCmRequestSpec> {
 public:
  using MediationRequirement =
      ::password_manager::CredentialMediationRequirement;

  static scoped_refptr<FedCmRequestSpec> Build(
      RenderFrameHost* rfh,
      FederatedIdentityPermissionContextDelegate* permission_delegate,
      blink::mojom::IdentityProviderGetParametersPtr idp_get_params,
      MediationRequirement requirement,
      NavigationHandle* navigation_handle = nullptr,
      const GURL& intercepted_url = GURL(),
      bool force_allow_redirect_to_for_testing = false);

  FedCmRequestSpec();
  FedCmRequestSpec(const FedCmRequestSpec&) = delete;
  FedCmRequestSpec& operator=(const FedCmRequestSpec&) = delete;

  blink::mojom::RpMode rp_mode() const { return rp_mode_; }

  MediationRequirement mediation_requirement() const {
    return mediation_requirement_;
  }

  blink::mojom::RpContext rp_context() const { return rp_context_; }

  bool had_transient_user_activation() const {
    return had_transient_user_activation_;
  }
  bool can_accept_redirect_to() const { return can_accept_redirect_to_; }
  const GURL& intercepted_url() const { return intercepted_url_; }
  const std::vector<GURL>& idp_order() const { return idp_order_; }
  const base::flat_set<GURL>& idps_with_nonce() const {
    return idps_with_nonce_;
  }
  const base::flat_set<GURL>& idps_with_nonce_outside_params_only() const {
    return idps_with_nonce_outside_params_only_;
  }
  const std::vector<blink::mojom::IdentityProviderRequestOptionsPtr>&
  providers() const {
    return providers_;
  }

 private:
  friend class base::RefCounted<FedCmRequestSpec>;
  ~FedCmRequestSpec();

  FedCmRequestSpec(
      blink::mojom::IdentityProviderGetParametersPtr idp_get_params,
      MediationRequirement requirement,
      bool had_transient_user_activation,
      bool can_accept_redirect_to,
      const GURL& intercepted_url);

  void ExtractIdpOrderAndNonceSets();

  // Immutable request inputs:
  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> providers_;
  MediationRequirement mediation_requirement_{MediationRequirement::kOptional};
  blink::mojom::RpMode rp_mode_{blink::mojom::RpMode::kPassive};
  blink::mojom::RpContext rp_context_{blink::mojom::RpContext::kSignIn};

  // The active flow requires user activation to be kicked off. We'd also need
  // this information along the way. e.g. showing pop-up window when accounts
  // fetch is failed. However, the function `HasTransientUserActivation` may
  // return false at that time because the network requests may be very slow
  // such that the previous user gesture is expired. Therefore we store the
  // information to use it during the entire the active flow.
  bool had_transient_user_activation_{false};

  // Whether this Request can make top level redirections, available
  // currently only for interception-initiated requests.
  bool can_accept_redirect_to_{false};

  // Stores the URL that we intercepted. This will be used as the referrer when
  // loading the redirect target so that to the RP this looks like the load was
  // initiated by the IDP.
  GURL intercepted_url_;

  // Pre-computed lookup helpers:
  // List of config URLs of IDPs in the same order as the providers specified in
  // the navigator.credentials.get call.
  std::vector<GURL> idp_order_;
  base::flat_set<GURL> idps_with_nonce_;
  base::flat_set<GURL> idps_with_nonce_outside_params_only_;
};

}  // namespace content::webid

#endif  // CONTENT_BROWSER_WEBID_FEDCM_REQUEST_SPEC_H_
