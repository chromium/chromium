// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/single_sign_on_key_manager.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/types/expected.h"
#include "net/device_bound_sessions/cookie_access_check_params.h"
#include "net/device_bound_sessions/registration_fetcher_param.h"
#include "net/device_bound_sessions/session.h"
#include "net/device_bound_sessions/session_error.h"
#include "url/origin.h"

namespace net::device_bound_sessions {

namespace {

bool CanAccessPreProvisionedKey(
    const SessionService::CookieAccessCallback& cookie_access_cb,
    const url::Origin& provider_origin,
    const url::Origin& rp_origin) {
  return cookie_access_cb &&
         cookie_access_cb.Run({.provider_origin{provider_origin},
                               .relying_party_origin{rp_origin}});
}

}  // namespace

SingleSignOnKeyManager::SingleSignOnKeyManager(
    SessionService::CookieAccessCallback has_cookie_access_cb)
    : has_cookie_access_cb_(std::move(has_cookie_access_cb)) {}

SingleSignOnKeyManager::~SingleSignOnKeyManager() = default;

bool SingleSignOnKeyManager::AddPreProvisionedKey(
    const url::Origin& rp_origin,
    std::string_view provider_key,
    const GURL& provider_url,
    unexportable_keys::UnexportableSigningKeyId key_id) {
  if (!CanAddPreProvisionedKey(provider_url, rp_origin)) {
    return false;
  }

  auto existing_key_it =
      std::ranges::find_if(pre_provisioned_keys_, [&](const auto& pk) {
        return pk.provider_url == provider_url && pk.rp_origin == rp_origin &&
               pk.provider_key == provider_key;
      });
  if (existing_key_it != pre_provisioned_keys_.end()) {
    return false;
  }

  pre_provisioned_keys_.push_back(
      {.provider_url{provider_url},
       .provider_site{net::SchemefulSite(provider_url)},
       .rp_origin{rp_origin},
       .provider_key{std::string(provider_key)},
       .key_id{key_id}});
  return true;
}

SessionErrorOr<unexportable_keys::UnexportableSigningKeyId>
SingleSignOnKeyManager::FindPreProvisionedKey(
    const ProviderRegistrationParams& provider_params,
    base::optional_ref<const url::Origin> initiator) const {
  if (!initiator) {
    return base::unexpected(
        SessionError::kInvalidPreProvisionedKeyInitiatorMissing);
  }

  if (!CanAccessPreProvisionedKey(
          has_cookie_access_cb_,
          url::Origin::Create(provider_params.provider_url), *initiator)) {
    return base::unexpected(SessionError::kPreProvisionedKeyAccessNotGranted);
  }

  auto key_it =
      std::ranges::find_if(pre_provisioned_keys_, [&](const auto& pk) {
        return pk.provider_url == provider_params.provider_url &&
               pk.provider_key == provider_params.provider_key &&
               pk.rp_origin == *initiator;
      });
  if (key_it == pre_provisioned_keys_.end()) {
    return base::unexpected(SessionError::kPreProvisionedKeyNotFound);
  }

  return key_it->key_id;
}

void SingleSignOnKeyManager::ConsumeKeyForSession(const Session& session) {
  if (session.unexportable_key_id().has_value()) {
    std::erase_if(pre_provisioned_keys_, [&](const PreProvisionedKeyEntry& pk) {
      return pk.key_id == session.unexportable_key_id();
    });
  }
}

void SingleSignOnKeyManager::ClearPreProvisionedKeys(
    const SessionService::OriginAndSiteMatcher& matcher) {
  if (!matcher) {
    pre_provisioned_keys_.clear();
  } else {
    std::erase_if(
        pre_provisioned_keys_, [&](const PreProvisionedKeyEntry& key) {
          // We only delete a pre-provisioned key if the origin and site
          // matches the Relying Party's origin and site because the key is
          // considered RP's data, not IdP's.
          return matcher.Run(key.rp_origin, net::SchemefulSite(key.rp_origin));
        });
  }
}

bool SingleSignOnKeyManager::CanAddPreProvisionedKey(
    const GURL& provider_url,
    const url::Origin& rp_origin) const {
  if (!CanAccessPreProvisionedKey(has_cookie_access_cb_,
                                  url::Origin::Create(provider_url),
                                  rp_origin)) {
    return false;
  }

  // If we haven't reached the max keys per Identity Provider overall, we can
  // add it right away.
  if (pre_provisioned_keys_.size() <
      kMaxPreProvisionedKeysPerIdentityProvider) {
    return true;
  }

  return static_cast<size_t>(std::ranges::count(
             pre_provisioned_keys_, net::SchemefulSite(provider_url),
             &PreProvisionedKeyEntry::provider_site)) <
         kMaxPreProvisionedKeysPerIdentityProvider;
}

}  // namespace net::device_bound_sessions
