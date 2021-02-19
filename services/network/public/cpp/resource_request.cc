// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/resource_request.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/load_flags.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/web_bundle_handle.mojom.h"

namespace network {

namespace {

mojo::PendingRemote<mojom::CookieAccessObserver> Clone(
    mojo::PendingRemote<mojom::CookieAccessObserver>* observer) {
  if (!*observer)
    return mojo::NullRemote();
  mojo::Remote<mojom::CookieAccessObserver> remote(std::move(*observer));
  mojo::PendingRemote<mojom::CookieAccessObserver> new_remote;
  remote->Clone(new_remote.InitWithNewPipeAndPassReceiver());
  *observer = remote.Unbind();
  return new_remote;
}

mojo::PendingRemote<mojom::AuthenticationAndCertificateObserver> Clone(
    mojo::PendingRemote<mojom::AuthenticationAndCertificateObserver>*
        observer) {
  if (!*observer)
    return mojo::NullRemote();
  mojo::Remote<mojom::AuthenticationAndCertificateObserver> remote(
      std::move(*observer));
  mojo::PendingRemote<mojom::AuthenticationAndCertificateObserver> new_remote;
  remote->Clone(new_remote.InitWithNewPipeAndPassReceiver());
  *observer = remote.Unbind();
  return new_remote;
}

// Returns true iff either holds true:
//
//  - both |lhs| and |rhs| are nullopt, or
//  - neither is nullopt and they both contain equal values
//
bool OptionalTrustedParamsEqualsForTesting(
    const base::Optional<ResourceRequest::TrustedParams>& lhs,
    const base::Optional<ResourceRequest::TrustedParams>& rhs) {
  return (!lhs && !rhs) || (lhs && rhs && lhs->EqualsForTesting(*rhs));
}

bool OptionalWebBundleTokenParamsEqualsForTesting(  // IN-TEST
    const base::Optional<ResourceRequest::WebBundleTokenParams>& lhs,
    const base::Optional<ResourceRequest::WebBundleTokenParams>& rhs) {
  return (!lhs && !rhs) ||
         (lhs && rhs && lhs->EqualsForTesting(*rhs));  // IN-TEST
}

}  // namespace

ResourceRequest::TrustedParams::TrustedParams() = default;
ResourceRequest::TrustedParams::~TrustedParams() = default;

ResourceRequest::TrustedParams::TrustedParams(const TrustedParams& other) {
  *this = other;
}

ResourceRequest::TrustedParams& ResourceRequest::TrustedParams::operator=(
    const TrustedParams& other) {
  isolation_info = other.isolation_info;
  disable_secure_dns = other.disable_secure_dns;
  has_user_activation = other.has_user_activation;
  cookie_observer =
      Clone(&const_cast<mojo::PendingRemote<mojom::CookieAccessObserver>&>(
          other.cookie_observer));
  auth_cert_observer =
      Clone(&const_cast<
            mojo::PendingRemote<mojom::AuthenticationAndCertificateObserver>&>(
          other.auth_cert_observer));
  client_security_state = other.client_security_state.Clone();
  return *this;
}

bool ResourceRequest::TrustedParams::EqualsForTesting(
    const TrustedParams& trusted_params) const {
  return isolation_info.IsEqualForTesting(trusted_params.isolation_info) &&
         disable_secure_dns == trusted_params.disable_secure_dns &&
         has_user_activation == trusted_params.has_user_activation &&
         client_security_state == trusted_params.client_security_state;
}

ResourceRequest::WebBundleTokenParams::WebBundleTokenParams() = default;
ResourceRequest::WebBundleTokenParams::~WebBundleTokenParams() = default;

ResourceRequest::WebBundleTokenParams::WebBundleTokenParams(
    const WebBundleTokenParams& other) {
  *this = other;
}

ResourceRequest::WebBundleTokenParams&
ResourceRequest::WebBundleTokenParams::operator=(
    const WebBundleTokenParams& other) {
  bundle_url = other.bundle_url;
  token = other.token;
  handle = other.CloneHandle();
  render_process_id = other.render_process_id;
  return *this;
}

ResourceRequest::WebBundleTokenParams::WebBundleTokenParams(
    const GURL& bundle_url,
    const base::UnguessableToken& token,
    mojo::PendingRemote<mojom::WebBundleHandle> handle)
    : bundle_url(bundle_url), token(token), handle(std::move(handle)) {}

ResourceRequest::WebBundleTokenParams::WebBundleTokenParams(
    const GURL& bundle_url,
    const base::UnguessableToken& token,
    int32_t render_process_id)
    : bundle_url(bundle_url),
      token(token),
      render_process_id(render_process_id) {}

bool ResourceRequest::WebBundleTokenParams::EqualsForTesting(
    const WebBundleTokenParams& other) const {
  return bundle_url == other.bundle_url && token == other.token &&
         ((handle && other.handle) || (!handle && !other.handle)) &&
         render_process_id == other.render_process_id;
}

mojo::PendingRemote<mojom::WebBundleHandle>
ResourceRequest::WebBundleTokenParams::CloneHandle() const {
  if (!handle)
    return mojo::NullRemote();
  mojo::Remote<network::mojom::WebBundleHandle> remote(std::move(
      const_cast<mojo::PendingRemote<network::mojom::WebBundleHandle>&>(
          handle)));
  mojo::PendingRemote<network::mojom::WebBundleHandle> new_remote;
  remote->Clone(new_remote.InitWithNewPipeAndPassReceiver());
  const_cast<mojo::PendingRemote<network::mojom::WebBundleHandle>&>(handle) =
      remote.Unbind();
  return new_remote;
}

ResourceRequest::ResourceRequest() {}
ResourceRequest::ResourceRequest(const ResourceRequest& request) = default;
ResourceRequest::~ResourceRequest() {}

bool ResourceRequest::EqualsForTesting(const ResourceRequest& request) const {
  return method == request.method && url == request.url &&
         site_for_cookies.IsEquivalent(request.site_for_cookies) &&
         update_first_party_url_on_redirect ==
             request.update_first_party_url_on_redirect &&
         request_initiator == request.request_initiator &&
         isolated_world_origin == request.isolated_world_origin &&
         referrer == request.referrer &&
         referrer_policy == request.referrer_policy &&
         headers.ToString() == request.headers.ToString() &&
         cors_exempt_headers.ToString() ==
             request.cors_exempt_headers.ToString() &&
         load_flags == request.load_flags &&
         resource_type == request.resource_type &&
         priority == request.priority &&
         devtools_stack_id == request.devtools_stack_id &&
         should_reset_appcache == request.should_reset_appcache &&
         is_external_request == request.is_external_request &&
         cors_preflight_policy == request.cors_preflight_policy &&
         originated_from_service_worker ==
             request.originated_from_service_worker &&
         skip_service_worker == request.skip_service_worker &&
         corb_detachable == request.corb_detachable && mode == request.mode &&
         credentials_mode == request.credentials_mode &&
         redirect_mode == request.redirect_mode &&
         fetch_integrity == request.fetch_integrity &&
         destination == request.destination &&
         request_body == request.request_body &&
         keepalive == request.keepalive &&
         has_user_gesture == request.has_user_gesture &&
         enable_load_timing == request.enable_load_timing &&
         enable_upload_progress == request.enable_upload_progress &&
         do_not_prompt_for_login == request.do_not_prompt_for_login &&
         render_frame_id == request.render_frame_id &&
         is_main_frame == request.is_main_frame &&
         transition_type == request.transition_type &&
         report_raw_headers == request.report_raw_headers &&
         previews_state == request.previews_state &&
         upgrade_if_insecure == request.upgrade_if_insecure &&
         is_revalidating == request.is_revalidating &&
         throttling_profile_id == request.throttling_profile_id &&
         custom_proxy_pre_cache_headers.ToString() ==
             request.custom_proxy_pre_cache_headers.ToString() &&
         custom_proxy_post_cache_headers.ToString() ==
             request.custom_proxy_post_cache_headers.ToString() &&
         fetch_window_id == request.fetch_window_id &&
         devtools_request_id == request.devtools_request_id &&
         is_signed_exchange_prefetch_cache_enabled ==
             request.is_signed_exchange_prefetch_cache_enabled &&
         is_fetch_like_api == request.is_fetch_like_api &&
         is_favicon == request.is_favicon &&
         obey_origin_policy == request.obey_origin_policy &&
         recursive_prefetch_token == request.recursive_prefetch_token &&
         OptionalTrustedParamsEqualsForTesting(trusted_params,
                                               request.trusted_params) &&
         trust_token_params == request.trust_token_params &&
         OptionalWebBundleTokenParamsEqualsForTesting(  // IN-TEST
             web_bundle_token_params, request.web_bundle_token_params);
}

bool ResourceRequest::SendsCookies() const {
  return credentials_mode == network::mojom::CredentialsMode::kInclude;
}

bool ResourceRequest::SavesCookies() const {
  return credentials_mode == network::mojom::CredentialsMode::kInclude &&
         !(load_flags & net::LOAD_DO_NOT_SAVE_COOKIES);
}

net::ReferrerPolicy ReferrerPolicyForUrlRequest(
    mojom::ReferrerPolicy referrer_policy) {
  switch (referrer_policy) {
    case mojom::ReferrerPolicy::kAlways:
      return net::ReferrerPolicy::NEVER_CLEAR;
    case mojom::ReferrerPolicy::kNever:
      return net::ReferrerPolicy::NO_REFERRER;
    case mojom::ReferrerPolicy::kOrigin:
      return net::ReferrerPolicy::ORIGIN;
    case mojom::ReferrerPolicy::kNoReferrerWhenDowngrade:
      return net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
    case mojom::ReferrerPolicy::kOriginWhenCrossOrigin:
      return net::ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN;
    case mojom::ReferrerPolicy::kSameOrigin:
      return net::ReferrerPolicy::CLEAR_ON_TRANSITION_CROSS_ORIGIN;
    case mojom::ReferrerPolicy::kStrictOrigin:
      return net::ReferrerPolicy::
          ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
    case mojom::ReferrerPolicy::kDefault:
      CHECK(false);
      return net::ReferrerPolicy::NO_REFERRER;
    case mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin:
      return net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
  }
  NOTREACHED();
  return net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
}

}  // namespace network
