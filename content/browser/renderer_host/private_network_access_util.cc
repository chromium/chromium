// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/private_network_access_util.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/renderer_host/policy_container_host.h"
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

using Policy = network::mojom::PrivateNetworkRequestPolicy;
using AddressSpace = network::mojom::IPAddressSpace;

Policy DerivePrivateNetworkRequestPolicy(
    const PolicyContainerPolicies& policies,
    PrivateNetworkRequestContext private_network_request_context) {
  return DerivePrivateNetworkRequestPolicy(policies.ip_address_space,
                                           policies.is_web_secure_context,
                                           private_network_request_context);
}

Policy DerivePolicyForNonSecureContext(
    AddressSpace ip_address_space,
    PrivateNetworkRequestContext private_network_request_context) {
  bool warning_only = private_network_request_context ==
                          PrivateNetworkRequestContext::kWorker &&
                      base::FeatureList::IsEnabled(
                          features::kPrivateNetworkAccessForWorkersWarningOnly);
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
      return !warning_only &&
                     base::FeatureList::IsEnabled(
                         features::
                             kBlockInsecurePrivateNetworkRequestsFromPrivate)
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
      return !warning_only &&
                     base::FeatureList::IsEnabled(
                         features::kBlockInsecurePrivateNetworkRequests)
                 ? Policy::kBlock
                 : Policy::kWarn;
  }
}

Policy DerivePolicyForSecureContext(
    AddressSpace ip_address_space,
    PrivateNetworkRequestContext private_network_request_context) {
  bool warning_only = private_network_request_context ==
                          PrivateNetworkRequestContext::kWorker &&
                      base::FeatureList::IsEnabled(
                          features::kPrivateNetworkAccessForWorkersWarningOnly);
  // The goal is to eliminate occurrences of this case as much as possible,
  // before removing this special case.
  if (ip_address_space == AddressSpace::kUnknown) {
    return Policy::kAllow;
  }

  if (base::FeatureList::IsEnabled(
          features::kPrivateNetworkAccessRespectPreflightResults)) {
    return warning_only ? Policy::kPreflightWarn : Policy::kPreflightBlock;
  }

  if (base::FeatureList::IsEnabled(
          features::kPrivateNetworkAccessSendPreflights)) {
    return Policy::kPreflightWarn;
  }

  return Policy::kAllow;
}

Policy DerivePrivateNetworkRequestPolicy(
    AddressSpace ip_address_space,
    bool is_web_secure_context,
    PrivateNetworkRequestContext private_network_request_context) {
  // Disable PNA checks entirely when running with `--disable-web-security`.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebSecurity)) {
    return Policy::kAllow;
  }

  return is_web_secure_context
             ? DerivePolicyForSecureContext(ip_address_space,
                                            private_network_request_context)
             : DerivePolicyForNonSecureContext(ip_address_space,
                                               private_network_request_context);
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
    if (url.SchemeIs(scheme))
      return AddressSpace::kLocal;
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
  absl::optional<network::CalculateClientAddressSpaceParams> params =
      absl::nullopt;
  if (response_head) {
    params.emplace(response_head->url_list_via_service_worker,
                   response_head->parsed_headers,
                   response_head->remote_endpoint);
  }
  AddressSpace computed_ip_address_space =
      network::CalculateClientAddressSpace(url, params);

  if (computed_ip_address_space != AddressSpace::kUnknown) {
    return computed_ip_address_space;
  }

  return IPAddressSpaceForSpecialScheme(url, client);
}

}  // namespace content
