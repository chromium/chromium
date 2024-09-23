// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/private_network_access_util.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/common/features.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/ip_address_space_util.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace content {
namespace {

using Policy = network::mojom::PrivateNetworkRequestPolicy;
using AddressSpace = network::mojom::IPAddressSpace;
using RequestContext = PrivateNetworkRequestContext;

// Represents the state of feature flags for a given `RequestContext`.
enum class FeatureState {
  kDisabled,
  kWarningOnly,
  kEnabled,
};

FeatureState FeatureStateForContext(RequestContext request_context) {
  switch (request_context) {
    case RequestContext::kSubresource:
      return FeatureState::kEnabled;
    case RequestContext::kWorker:
      if (!base::FeatureList::IsEnabled(
              features::kPrivateNetworkAccessForWorkers)) {
        return FeatureState::kDisabled;
      }

      if (base::FeatureList::IsEnabled(
              features::kPrivateNetworkAccessForWorkersWarningOnly)) {
        return FeatureState::kWarningOnly;
      }

      return FeatureState::kEnabled;
    case RequestContext::kNavigation:
      if (!base::FeatureList::IsEnabled(
              features::kPrivateNetworkAccessForNavigations)) {
        return FeatureState::kDisabled;
      }

      if (base::FeatureList::IsEnabled(
              features::kPrivateNetworkAccessForNavigationsWarningOnly)) {
        return FeatureState::kWarningOnly;
      }

      return FeatureState::kEnabled;
  }
}

}  // namespace

Policy DerivePrivateNetworkRequestPolicy(
    const PolicyContainerPolicies& policies,
    RequestContext private_network_request_context) {
  return DerivePrivateNetworkRequestPolicy(policies.ip_address_space,
                                           policies.is_web_secure_context,
                                           private_network_request_context);
}

Policy DerivePolicyForNonSecureContext(AddressSpace ip_address_space) {
  switch (ip_address_space) {
    case AddressSpace::kUnknown:
      // Requests from the `unknown` address space are controlled separately
      // because it is unclear why they happen in the first place. The goal is
      // to reduce instances of this happening before enabling this feature.
      return base::FeatureList::IsEnabled(
                 features::kBlockInsecurePrivateNetworkRequestsFromUnknown)
                 ? Policy::kBlock
                 : Policy::kAllow;
    case AddressSpace::kPrivate:
      // Requests from the non secure contexts in the `private` address space
      // to localhost are blocked only if the right feature is enabled.
      // This is controlled separately because private network websites face
      // additional hurdles compared to public websites. See crbug.com/1234044.
      return base::FeatureList::IsEnabled(
                 features::kBlockInsecurePrivateNetworkRequestsFromPrivate)
                 ? Policy::kBlock
                 : Policy::kWarn;
    case AddressSpace::kPublic:
    case AddressSpace::kLocal:
      // Private network requests from non secure contexts are blocked if the
      // secure context restriction is enabled in general.
      //
      // NOTE: We also set this when `ip_address_space` is `kLocal`, but that
      // has no effect. Indeed, requests initiated from the local address space
      // are never considered private network requests - they cannot target
      // more-private address spaces.
      return base::FeatureList::IsEnabled(
                 features::kBlockInsecurePrivateNetworkRequests)
                 ? Policy::kBlock
                 : Policy::kWarn;
  }
}

Policy DerivePolicyForSecureContext(AddressSpace ip_address_space) {
  // The goal is to eliminate occurrences of this case as much as possible,
  // before removing this special case.
  if (ip_address_space == AddressSpace::kUnknown) {
    return Policy::kAllow;
  }

  if (base::FeatureList::IsEnabled(
          features::kPrivateNetworkAccessRespectPreflightResults)) {
    return Policy::kPreflightBlock;
  }

  if (base::FeatureList::IsEnabled(
          features::kPrivateNetworkAccessSendPreflights)) {
    return Policy::kPreflightWarn;
  }

  return Policy::kAllow;
}

Policy ApplyFeatureStateToPolicy(FeatureState feature_state, Policy policy) {
  switch (feature_state) {
    // Feature disabled: allow all requests.
    case FeatureState::kDisabled:
      return Policy::kAllow;

    // Feature enabled in warning-only mode. Downgrade `k*Block` to `k*Warn`.
    case FeatureState::kWarningOnly:
      switch (policy) {
        case Policy::kBlock:
          return Policy::kWarn;
        case Policy::kPreflightBlock:
          return Policy::kPreflightWarn;
        default:
          return policy;
      }

    // Fully enabled. Use `policy` as is.
    case FeatureState::kEnabled:
      return policy;
  }
}

Policy DerivePrivateNetworkRequestPolicy(
    AddressSpace ip_address_space,
    bool is_web_secure_context,
    RequestContext private_network_request_context) {
  // Disable PNA checks entirely when running with `--disable-web-security`.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebSecurity)) {
    return Policy::kAllow;
  }

  FeatureState feature_state =
      FeatureStateForContext(private_network_request_context);

  Policy policy = is_web_secure_context
                      ? DerivePolicyForSecureContext(ip_address_space)
                      : DerivePolicyForNonSecureContext(ip_address_space);

  return ApplyFeatureStateToPolicy(feature_state, policy);
}

network::mojom::ClientSecurityStatePtr DeriveClientSecurityState(
    const PolicyContainerPolicies& policies,
    PrivateNetworkRequestContext private_network_request_context) {
  return network::mojom::ClientSecurityState::New(
      policies.cross_origin_embedder_policy, policies.is_web_secure_context,
      policies.ip_address_space,
      DerivePrivateNetworkRequestPolicy(policies,
                                        private_network_request_context),
      policies.document_isolation_policy);
}

// Special chrome schemes cannot directly be categorized in public/private/local
// address spaces using information from the network or the PolicyContainer. We
// have to classify them manually. In its default state an unhandled scheme will
// have an IPAddressSpace of kUnknown, which is equivalent to public.
// This means a couple of things:
// - They cannot embed anything private or local without being secure contexts
//   and triggering a CORS preflight.
// - Private Network Access does not prevent them being embedded by less private
//   content.
// - It pollutes metrics since kUnknown could also mean a missed edge case.
// To address these issues we list here a number of schemes that should be
// considered local.
// TODO(titouan): It might be better to have these schemes (and in general
// other schemes such as data: or blob:) handled directly by the URLLoaders.
// Investigate on whether this is worth doing.
AddressSpace IPAddressSpaceForSpecialScheme(const GURL& url,
                                            ContentBrowserClient* client) {
  // This only handles schemes that are known to the content/ layer.
  // List here: content/public/common/url_constants.cc.
  const char* special_content_schemes[] = {
    kChromeDevToolsScheme,
    kChromeUIScheme,
    kChromeUIUntrustedScheme,
#if BUILDFLAG(IS_CHROMEOS_ASH)
    kExternalFileScheme,
#endif
  };

  for (auto* scheme : special_content_schemes) {
    if (url.SchemeIs(scheme)) {
      return AddressSpace::kLocal;
    }
  }

  // Some of these schemes are only known to the embedder. Query the embedder
  // for these.
  if (!client) {
    return AddressSpace::kUnknown;
  }

  return client->DetermineAddressSpaceFromURL(url);
}

AddressSpace CalculateIPAddressSpace(
    const GURL& url,
    network::mojom::URLResponseHead* response_head,
    ContentBrowserClient* client) {
  // Determine the IPAddressSpace, based on the IP address and the response
  // headers received.
  std::optional<network::CalculateClientAddressSpaceParams> params =
      std::nullopt;
  if (response_head) {
    std::optional<network::mojom::IPAddressSpace> client_address_space;
    if (response_head->was_fetched_via_service_worker &&
        response_head->client_address_space !=
            network::mojom::IPAddressSpace::kUnknown) {
      client_address_space = response_head->client_address_space;
    }
    params.emplace<network::CalculateClientAddressSpaceParams>({
        .client_address_space_inherited_from_service_worker =
            client_address_space,
        .parsed_headers = &response_head->parsed_headers,
        .remote_endpoint = &response_head->remote_endpoint,
    });
  }
  AddressSpace computed_ip_address_space =
      network::CalculateClientAddressSpace(url, params);

  if (computed_ip_address_space != AddressSpace::kUnknown) {
    return computed_ip_address_space;
  }

  return IPAddressSpaceForSpecialScheme(url, client);
}

network::mojom::PrivateNetworkRequestPolicy OverrideBlockWithWarn(
    network::mojom::PrivateNetworkRequestPolicy policy) {
  switch (policy) {
    case network::mojom::PrivateNetworkRequestPolicy::kWarn:
      return network::mojom::PrivateNetworkRequestPolicy::kBlock;
    case network::mojom::PrivateNetworkRequestPolicy::kPreflightWarn:
      return network::mojom::PrivateNetworkRequestPolicy::kPreflightBlock;
    default:
      return policy;
  }
}

}  // namespace content
