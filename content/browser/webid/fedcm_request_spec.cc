// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/fedcm_request_spec.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/webid_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/webid/federated_identity_permission_context_delegate.h"
#include "mojo/public/cpp/bindings/clone_traits.h"

namespace content::webid {

// static
scoped_refptr<FedCmRequestSpec> FedCmRequestSpec::Build(
    RenderFrameHost* rfh,
    FederatedIdentityPermissionContextDelegate* permission_delegate,
    blink::mojom::IdentityProviderGetParametersPtr idp_get_params,
    MediationRequirement requirement,
    NavigationHandle* navigation_handle,
    const GURL& intercepted_url,
    bool force_allow_redirect_to_for_testing) {
  CHECK(navigation_handle == nullptr || !intercepted_url.is_empty());
  CHECK(idp_get_params);

  if (IsIdPRegistrationEnabled()) {
    bool has_registration_provider = false;
    for (const auto& provider : idp_get_params->providers) {
      if (provider->config->from_idp_registration_api) {
        has_registration_provider = true;
        break;
      }
    }

    if (has_registration_provider) {
      std::vector<GURL> registered_config_urls =
          permission_delegate->GetRegisteredIdPs();
      // TODO(crbug.com/40252825): we insert the registered IdPs to
      // the list of IdPs in a reverse chronological order:
      // first IdPs to be registered goes first. It is not clear
      // yet what's the right order, but this seems like a reasonable
      // starting point.
      std::ranges::reverse(registered_config_urls);

      std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> result;
      for (auto& provider : idp_get_params->providers) {
        if (!provider->config->from_idp_registration_api) {
          result.emplace_back(std::move(provider));
          continue;
        }
        for (const auto& config_url : registered_config_urls) {
          blink::mojom::IdentityProviderRequestOptionsPtr idp =
              provider->Clone();
          // Keep `from_idp_registration_api` so it is clear this is a
          // registered provider.
          idp->config->config_url = config_url;
          result.emplace_back(std::move(idp));
        }
      }
      // TODO(crbug.com/40252825): Consider removing duplicate
      // IdPs in case they were present in the registry as well
      // as added individually.
      idp_get_params->providers = std::move(result);
    }
  }

  const bool can_accept_redirect_to =
      force_allow_redirect_to_for_testing ||
      ((IsNavigationInterceptionEnabled() || HasEmbedderLoginRequest(rfh)) &&
       navigation_handle != nullptr);

  const bool had_transient_user_activation =
      (navigation_handle &&
       DidNavigationHandleHaveActivation(navigation_handle)) ||
      (rfh && rfh->HasTransientUserActivation());

  return base::WrapRefCounted(new FedCmRequestSpec(
      std::move(idp_get_params), requirement, had_transient_user_activation,
      can_accept_redirect_to, intercepted_url));
}

FedCmRequestSpec::FedCmRequestSpec() = default;

FedCmRequestSpec::FedCmRequestSpec(
    blink::mojom::IdentityProviderGetParametersPtr idp_get_params,
    MediationRequirement requirement,
    bool had_transient_user_activation,
    bool can_accept_redirect_to,
    const GURL& intercepted_url)
    : providers_(std::move(idp_get_params->providers)),
      mediation_requirement_(requirement),
      rp_mode_(idp_get_params->mode),
      rp_context_(idp_get_params->context),
      had_transient_user_activation_(had_transient_user_activation),
      can_accept_redirect_to_(can_accept_redirect_to),
      intercepted_url_(intercepted_url) {
  ExtractIdpOrderAndNonceSets();
}

FedCmRequestSpec::~FedCmRequestSpec() = default;

void FedCmRequestSpec::ExtractIdpOrderAndNonceSets() {
  for (const auto& idp_ptr : providers_) {
    idp_order_.push_back(idp_ptr->config->config_url);

    if (!idp_ptr->nonce.empty()) {
      idps_with_nonce_.insert(idp_ptr->config->config_url);

      bool has_nonce_in_params = false;
      if (idp_ptr->params_json) {
        std::optional<base::Value> params = base::JSONReader::Read(
            *idp_ptr->params_json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
        if (params && params->is_dict()) {
          if (params->GetDict().contains("nonce")) {
            has_nonce_in_params = true;
          }
        }
      }
      if (!has_nonce_in_params) {
        idps_with_nonce_outside_params_only_.insert(
            idp_ptr->config->config_url);
      }
    }
  }
}

}  // namespace content::webid
