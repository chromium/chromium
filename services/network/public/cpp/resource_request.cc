// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/resource_request.h"

#include "base/strings/string_number_conversions.h"
#include "base/trace_event/typed_macros.h"
#include "base/types/optional_util.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/load_flags.h"
#include "net/log/net_log_source.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/trust_token_access_observer.mojom.h"
#include "services/network/public/mojom/url_request.mojom.h"
#include "services/network/public/mojom/web_bundle_handle.mojom.h"

namespace network {

namespace {

mojo::PendingRemote<mojom::CookieAccessObserver> Clone(
    mojo::PendingRemote<mojom::CookieAccessObserver>* observer) {
  if (!*observer) {
    return mojo::NullRemote();
  }
  mojo::Remote<mojom::CookieAccessObserver> remote(std::move(*observer));
  mojo::PendingRemote<mojom::CookieAccessObserver> new_remote;
  remote->Clone(new_remote.InitWithNewPipeAndPassReceiver());
  *observer = remote.Unbind();
  return new_remote;
}

mojo::PendingRemote<mojom::TrustTokenAccessObserver> Clone(
    mojo::PendingRemote<mojom::TrustTokenAccessObserver>* observer) {
  if (!*observer) {
    return mojo::NullRemote();
  }
  mojo::Remote<mojom::TrustTokenAccessObserver> remote(std::move(*observer));
  mojo::PendingRemote<mojom::TrustTokenAccessObserver> new_remote;
  remote->Clone(new_remote.InitWithNewPipeAndPassReceiver());
  *observer = remote.Unbind();
  return new_remote;
}

mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver> Clone(
    mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>* observer) {
  if (!*observer) {
    return mojo::NullRemote();
  }
  mojo::Remote<mojom::URLLoaderNetworkServiceObserver> remote(
      std::move(*observer));
  mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver> new_remote;
  remote->Clone(new_remote.InitWithNewPipeAndPassReceiver());
  *observer = remote.Unbind();
  return new_remote;
}

mojo::PendingRemote<mojom::DevToolsObserver> Clone(
    mojo::PendingRemote<mojom::DevToolsObserver>* observer) {
  if (!*observer) {
    return mojo::NullRemote();
  }
  mojo::Remote<mojom::DevToolsObserver> remote(std::move(*observer));
  mojo::PendingRemote<mojom::DevToolsObserver> new_remote;
  remote->Clone(new_remote.InitWithNewPipeAndPassReceiver());
  *observer = remote.Unbind();
  return new_remote;
}

mojo::PendingRemote<mojom::AcceptCHFrameObserver> Clone(
    mojo::PendingRemote<mojom::AcceptCHFrameObserver>& observer) {
  if (!observer) {
    return mojo::NullRemote();
  }
  mojo::Remote<mojom::AcceptCHFrameObserver> remote(std::move(observer));
  mojo::PendingRemote<mojom::AcceptCHFrameObserver> new_remote;
  remote->Clone(new_remote.InitWithNewPipeAndPassReceiver());
  observer = remote.Unbind();
  return new_remote;
}

mojo::PendingRemote<mojom::SharedDictionaryAccessObserver> Clone(
    mojo::PendingRemote<mojom::SharedDictionaryAccessObserver>& observer) {
  if (!observer) {
    return mojo::NullRemote();
  }
  mojo::Remote<mojom::SharedDictionaryAccessObserver> remote(
      std::move(observer));
  mojo::PendingRemote<mojom::SharedDictionaryAccessObserver> new_remote;
  remote->Clone(new_remote.InitWithNewPipeAndPassReceiver());
  observer = remote.Unbind();
  return new_remote;
}

// Returns true iff either holds true:
//
//  - both |lhs| and |rhs| are nullopt, or
//  - neither is nullopt and they both contain equal values
//
bool OptionalTrustedParamsEqualsForTesting(
    const std::optional<ResourceRequest::TrustedParams>& lhs,
    const std::optional<ResourceRequest::TrustedParams>& rhs) {
  return (!lhs && !rhs) || (lhs && rhs && lhs->EqualsForTesting(*rhs));
}

bool OptionalWebBundleTokenParamsEqualsForTesting(  // IN-TEST
    const std::optional<ResourceRequest::WebBundleTokenParams>& lhs,
    const std::optional<ResourceRequest::WebBundleTokenParams>& rhs) {
  return (!lhs && !rhs) ||
         (lhs && rhs && lhs->EqualsForTesting(*rhs));  // IN-TEST
}

bool OptionalNetLogInfoEqualsForTesting(
    const std::optional<net::NetLogSource>& lhs,
    const std::optional<net::NetLogSource>& rhs) {
  bool equal_members = lhs && rhs && lhs.value() == rhs.value();
  return (!lhs && !rhs) || equal_members;
}

base::debug::CrashKeyString* GetRequestUrlCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "request_url", base::debug::CrashKeySize::Size256);
  return crash_key;
}

base::debug::CrashKeyString* GetRequestInitiatorCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "request_initiator", base::debug::CrashKeySize::Size64);
  return crash_key;
}

base::debug::CrashKeyString* GetRequestResourceTypeCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "request_resource_type", base::debug::CrashKeySize::Size32);
  return crash_key;
}

}  // namespace

ResourceRequest::TrustedParams::TrustedParams() = default;
ResourceRequest::TrustedParams::~TrustedParams() = default;

ResourceRequest::TrustedParams::TrustedParams(const TrustedParams& other) {
  *this = other;
}

ResourceRequest::TrustedParams& ResourceRequest::TrustedParams::operator=(
    const TrustedParams& other) {
  TRACE_EVENT("loading", "ResourceRequest::TrustedParams.copy");
  isolation_info = other.isolation_info;
  disable_secure_dns = other.disable_secure_dns;
  has_user_activation = other.has_user_activation;
  allow_cookies_from_browser = other.allow_cookies_from_browser;
  include_request_cookies_with_response =
      other.include_request_cookies_with_response;
  cookie_observer =
      Clone(&const_cast<mojo::PendingRemote<mojom::CookieAccessObserver>&>(
          other.cookie_observer));
  trust_token_observer =
      Clone(&const_cast<mojo::PendingRemote<mojom::TrustTokenAccessObserver>&>(
          other.trust_token_observer));
  url_loader_network_observer = Clone(
      &const_cast<mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>&>(
          other.url_loader_network_observer));
  devtools_observer =
      Clone(&const_cast<mojo::PendingRemote<mojom::DevToolsObserver>&>(
          other.devtools_observer));
  client_security_state = other.client_security_state.Clone();
  accept_ch_frame_observer =
      Clone(const_cast<mojo::PendingRemote<mojom::AcceptCHFrameObserver>&>(
          other.accept_ch_frame_observer));
  shared_dictionary_observer = Clone(
      const_cast<mojo::PendingRemote<mojom::SharedDictionaryAccessObserver>&>(
          other.shared_dictionary_observer));
  return *this;
}

ResourceRequest::TrustedParams::TrustedParams(TrustedParams&& other) = default;
ResourceRequest::TrustedParams& ResourceRequest::TrustedParams::operator=(
    TrustedParams&& other) = default;

bool ResourceRequest::TrustedParams::EqualsForTesting(
    const TrustedParams& other) const {
  return isolation_info.IsEqualForTesting(other.isolation_info) &&
         disable_secure_dns == other.disable_secure_dns &&
         has_user_activation == other.has_user_activation &&
         allow_cookies_from_browser == other.allow_cookies_from_browser &&
         include_request_cookies_with_response ==
             other.include_request_cookies_with_response &&
         client_security_state == other.client_security_state;
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

ResourceRequest::ResourceRequest() = default;
ResourceRequest::ResourceRequest(const ResourceRequest& request) {
  TRACE_EVENT("loading", "ResourceRequest::ResourceRequest.copy_constructor");
  *this = request;
}
ResourceRequest& ResourceRequest::operator=(const ResourceRequest& other) =
    default;
ResourceRequest::ResourceRequest(ResourceRequest&& other) = default;
ResourceRequest& ResourceRequest::operator=(ResourceRequest&& other) = default;
ResourceRequest::~ResourceRequest() = default;

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
         priority_incremental == request.priority_incremental &&
         devtools_stack_id == request.devtools_stack_id &&
         cors_preflight_policy == request.cors_preflight_policy &&
         originated_from_service_worker ==
             request.originated_from_service_worker &&
         skip_service_worker == request.skip_service_worker &&
         mode == request.mode &&
         required_ip_address_space == request.required_ip_address_space &&
         credentials_mode == request.credentials_mode &&
         redirect_mode == request.redirect_mode &&
         fetch_integrity == request.fetch_integrity &&
         destination == request.destination &&
         request_body == request.request_body &&
         keepalive == request.keepalive &&
         shared_storage_writable_eligible ==
             request.shared_storage_writable_eligible &&
         has_user_gesture == request.has_user_gesture &&
         enable_load_timing == request.enable_load_timing &&
         enable_upload_progress == request.enable_upload_progress &&
         do_not_prompt_for_login == request.do_not_prompt_for_login &&
         is_outermost_main_frame == request.is_outermost_main_frame &&
         transition_type == request.transition_type &&
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
         is_fetch_like_api == request.is_fetch_like_api &&
         is_fetch_later_api == request.is_fetch_later_api &&
         is_favicon == request.is_favicon &&
         recursive_prefetch_token == request.recursive_prefetch_token &&
         OptionalTrustedParamsEqualsForTesting(trusted_params,
                                               request.trusted_params) &&
         devtools_accepted_stream_types ==
             request.devtools_accepted_stream_types &&
         trust_token_params == request.trust_token_params &&
         OptionalWebBundleTokenParamsEqualsForTesting(  // IN-TEST
             web_bundle_token_params, request.web_bundle_token_params) &&
         OptionalNetLogInfoEqualsForTesting(net_log_create_info,
                                            request.net_log_create_info) &&
         OptionalNetLogInfoEqualsForTesting(net_log_reference_info,
                                            request.net_log_reference_info) &&
         target_ip_address_space == request.target_ip_address_space &&
         shared_dictionary_writer_enabled ==
             request.shared_dictionary_writer_enabled;
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
  NOTREACHED_IN_MIGRATION();
  return net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
}

namespace debug {

ScopedResourceRequestCrashKeys::ScopedResourceRequestCrashKeys(
    const network::ResourceRequest& request)
    : url_(GetRequestUrlCrashKey(), request.url.possibly_invalid_spec()),
      request_initiator_(GetRequestInitiatorCrashKey(),
                         base::OptionalToPtr(request.request_initiator)),
      resource_type_(GetRequestResourceTypeCrashKey(),
                     base::NumberToString(request.resource_type)) {}

ScopedResourceRequestCrashKeys::~ScopedResourceRequestCrashKeys() = default;

}  // namespace debug
}  // namespace network
