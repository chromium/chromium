// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cookie_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_util.h"
#include "net/url_request/url_request_context.h"
#include "services/network/cookie_access_delegate_impl.h"
#include "services/network/session_cleanup_cookie_store.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

using CookieDeletionInfo = net::CookieDeletionInfo;
using CookieDeleteSessionControl = net::CookieDeletionInfo::SessionControl;

namespace network {

namespace {

using CookiePartitionKeyPairCallback = base::OnceCallback<void(
    std::pair<bool, absl::optional<net::CookiePartitionKey>>)>;

bool g_crash_on_get_cookie_list = false;

}  // namespace

CookieManager::ListenerRegistration::ListenerRegistration() = default;

CookieManager::ListenerRegistration::~ListenerRegistration() = default;

void CookieManager::ListenerRegistration::DispatchCookieStoreChange(
    const net::CookieChangeInfo& change) {
  listener->OnCookieChange(change);
}

CookieManager::CookieManager(
    net::URLRequestContext* url_request_context,
    FirstPartySetsAccessDelegate* const first_party_sets_access_delegate,
    scoped_refptr<SessionCleanupCookieStore> session_cleanup_cookie_store,
    mojom::CookieManagerParamsPtr params)
    : cookie_store_(url_request_context->cookie_store()),
      session_cleanup_cookie_store_(std::move(session_cleanup_cookie_store)) {
  mojom::CookieAccessDelegateType cookie_access_delegate_type =
      mojom::CookieAccessDelegateType::USE_CONTENT_SETTINGS;
  if (params) {
    ConfigureCookieSettings(*params, &cookie_settings_);
    cookie_access_delegate_type = params->cookie_access_delegate_type;
    // Don't wait for callback, the work happens synchronously.
    AllowFileSchemeCookies(params->allow_file_scheme_cookies,
                           base::DoNothing());
  }
  cookie_store_->SetCookieAccessDelegate(
      std::make_unique<CookieAccessDelegateImpl>(
          cookie_access_delegate_type, first_party_sets_access_delegate,
          &cookie_settings_));
}

CookieManager::~CookieManager() {
  if (session_cleanup_cookie_store_) {
    session_cleanup_cookie_store_->DeleteSessionCookies(
        cookie_settings_.CreateDeleteCookieOnExitPredicate());
  }
  // Make sure we destroy the CookieStore's CookieAccessDelegate, because it
  // holds a pointer to this CookieManager's CookieSettings, which is about to
  // be destroyed.
  cookie_store_->SetCookieAccessDelegate(nullptr);
}

void CookieManager::AddReceiver(
    mojo::PendingReceiver<mojom::CookieManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void CookieManager::GetAllCookies(GetAllCookiesCallback callback) {
  cookie_store_->GetAllCookiesAsync(std::move(callback));
}

void CookieManager::GetAllCookiesWithAccessSemantics(
    GetAllCookiesWithAccessSemanticsCallback callback) {
  cookie_store_->GetAllCookiesWithAccessSemanticsAsync(std::move(callback));
}

void CookieManager::GetCookieList(
    const GURL& url,
    const net::CookieOptions& cookie_options,
    const net::CookiePartitionKeyCollection& cookie_partition_key_collection,
    GetCookieListCallback callback) {
#if !BUILDFLAG(IS_IOS)
  if (g_crash_on_get_cookie_list)
    base::Process::TerminateCurrentProcessImmediately(1);
#endif

  cookie_store_->GetCookieListWithOptionsAsync(url, cookie_options,
                                               cookie_partition_key_collection,
                                               std::move(callback));
}

void CookieManager::SetCanonicalCookie(const net::CanonicalCookie& cookie,
                                       const GURL& source_url,
                                       const net::CookieOptions& cookie_options,
                                       SetCanonicalCookieCallback callback) {
  const absl::optional<net::CookiePartitionKey>& cookie_partition_key =
      cookie.PartitionKey();

  auto cookie_ptr = std::make_unique<net::CanonicalCookie>(cookie);
  base::Time adjusted_expiry_date =
      net::CanonicalCookie::ValidateAndAdjustExpiryDate(cookie.ExpiryDate(),
                                                        cookie.CreationDate());
  if (adjusted_expiry_date != cookie.ExpiryDate() || !cookie_partition_key) {
    cookie_ptr = net::CanonicalCookie::FromStorage(
        cookie.Name(), cookie.Value(), cookie.Domain(), cookie.Path(),
        cookie.CreationDate(), adjusted_expiry_date, cookie.LastAccessDate(),
        cookie.LastUpdateDate(), cookie.IsSecure(), cookie.IsHttpOnly(),
        cookie.SameSite(), cookie.Priority(), cookie.IsSameParty(),
        cookie_partition_key, cookie.SourceScheme(), cookie.SourcePort());
    if (!cookie_ptr) {
      std::move(callback).Run(
          net::CookieAccessResult(net::CookieInclusionStatus(
              net::CookieInclusionStatus::ExclusionReason::
                  EXCLUDE_FAILURE_TO_STORE)));
      return;
    }
  }
  DCHECK(cookie_ptr->IsCanonical());
  cookie_store_->SetCanonicalCookieAsync(std::move(cookie_ptr), source_url,
                                         cookie_options, std::move(callback));
}

void CookieManager::DeleteCanonicalCookie(
    const net::CanonicalCookie& cookie,
    DeleteCanonicalCookieCallback callback) {
  cookie_store_->DeleteCanonicalCookieAsync(
      cookie, base::BindOnce([](uint32_t num_deleted) {
                return num_deleted > 0;
              }).Then(std::move(callback)));
}

void CookieManager::SetContentSettings(
    const ContentSettingsForOneType& settings) {
  cookie_settings_.set_content_settings(settings);
}

void CookieManager::DeleteCookies(mojom::CookieDeletionFilterPtr filter,
                                  DeleteCookiesCallback callback) {
  cookie_store_->DeleteAllMatchingInfoAsync(
      DeletionFilterToInfo(std::move(filter)), std::move(callback));
}

void CookieManager::DeleteSessionOnlyCookies(
    DeleteSessionOnlyCookiesCallback callback) {
  auto delete_cookie_predicate =
      cookie_settings_.CreateDeleteCookieOnExitPredicate();
  if (!delete_cookie_predicate) {
    std::move(callback).Run(0);
    return;
  }

  cookie_store_->DeleteMatchingCookiesAsync(
      base::BindRepeating(
          [](const DeleteCookiePredicate& predicate,
             const net::CanonicalCookie& cookie) {
            return predicate.Run(cookie.Domain(), cookie.IsSecure());
          },
          std::move(delete_cookie_predicate)),
      std::move(callback));
}

void CookieManager::AddCookieChangeListener(
    const GURL& url,
    const absl::optional<std::string>& name,
    mojo::PendingRemote<mojom::CookieChangeListener> listener) {
  auto listener_registration = std::make_unique<ListenerRegistration>();
  listener_registration->listener.Bind(std::move(listener));

  auto cookie_change_callback = base::BindRepeating(
      &CookieManager::ListenerRegistration::DispatchCookieStoreChange,
      // base::Unretained is safe as destruction of the
      // ListenerRegistration will also destroy the
      // CookieChangedSubscription, unregistering the callback.
      base::Unretained(listener_registration.get()));

  if (name) {
    // TODO(https://crbug.com/1225444): Include the correct cookie partition
    // key when attaching cookie change listeners to service workers.
    listener_registration->subscription =
        cookie_store_->GetChangeDispatcher().AddCallbackForCookie(
            url, *name, absl::nullopt, std::move(cookie_change_callback));
  } else {
    // TODO(https://crbug.com/1225444): Include the correct cookie partition
    // key when attaching cookie change listeners to service workers.
    listener_registration->subscription =
        cookie_store_->GetChangeDispatcher().AddCallbackForUrl(
            url, absl::nullopt, std::move(cookie_change_callback));
  }

  listener_registration->listener.set_disconnect_handler(
      base::BindOnce(&CookieManager::RemoveChangeListener,
                     // base::Unretained is safe as destruction of the
                     // CookieManager will also destroy the
                     // notifications_registered list (which this object will be
                     // inserted into, below), which will destroy the
                     // listener, rendering this callback moot.
                     base::Unretained(this),
                     // base::Unretained is safe as destruction of the
                     // ListenerRegistration will also destroy the
                     // CookieChangedSubscription, unregistering the callback.
                     base::Unretained(listener_registration.get())));

  listener_registrations_.push_back(std::move(listener_registration));
}

void CookieManager::AddGlobalChangeListener(
    mojo::PendingRemote<mojom::CookieChangeListener> listener) {
  auto listener_registration = std::make_unique<ListenerRegistration>();
  listener_registration->listener.Bind(std::move(listener));

  listener_registration->subscription =
      cookie_store_->GetChangeDispatcher().AddCallbackForAllChanges(
          base::BindRepeating(
              &CookieManager::ListenerRegistration::DispatchCookieStoreChange,
              // base::Unretained is safe as destruction of the
              // ListenerRegistration will also destroy the
              // CookieChangedSubscription, unregistering the callback.
              base::Unretained(listener_registration.get())));

  listener_registration->listener.set_disconnect_handler(
      base::BindOnce(&CookieManager::RemoveChangeListener,
                     // base::Unretained is safe as destruction of the
                     // CookieManager will also destroy the
                     // notifications_registered list (which this object will be
                     // inserted into, below), which will destroy the
                     // listener, rendering this callback moot.
                     base::Unretained(this),
                     // base::Unretained is safe as destruction of the
                     // ListenerRegistration will also destroy the
                     // CookieChangedSubscription, unregistering the callback.
                     base::Unretained(listener_registration.get())));

  listener_registrations_.push_back(std::move(listener_registration));
}

void CookieManager::RemoveChangeListener(ListenerRegistration* registration) {
  for (auto it = listener_registrations_.begin();
       it != listener_registrations_.end(); ++it) {
    if (it->get() == registration) {
      // It isn't expected this will be a common enough operation for
      // the performance of std::vector::erase() to matter.
      listener_registrations_.erase(it);
      return;
    }
  }
  // A broken connection error should never be raised for an unknown pipe.
  NOTREACHED();
}

void CookieManager::CloneInterface(
    mojo::PendingReceiver<mojom::CookieManager> new_interface) {
  AddReceiver(std::move(new_interface));
}

void CookieManager::FlushCookieStore(FlushCookieStoreCallback callback) {
  // Flushes the backing store (if any) to disk.
  cookie_store_->FlushStore(std::move(callback));
}

void CookieManager::AllowFileSchemeCookies(
    bool allow,
    AllowFileSchemeCookiesCallback callback) {
  std::vector<std::string> cookieable_schemes(
      net::CookieMonster::kDefaultCookieableSchemes,
      net::CookieMonster::kDefaultCookieableSchemes +
          net::CookieMonster::kDefaultCookieableSchemesCount);
  if (allow) {
    cookieable_schemes.push_back(url::kFileScheme);
  }
  cookie_store_->SetCookieableSchemes(cookieable_schemes, std::move(callback));
}

void CookieManager::SetForceKeepSessionState() {
  cookie_store_->SetForceKeepSessionState();
}

void CookieManager::BlockThirdPartyCookies(bool block) {
  cookie_settings_.set_block_third_party_cookies(block);
}

void CookieManager::SetContentSettingsForLegacyCookieAccess(
    const ContentSettingsForOneType& settings) {
  cookie_settings_.set_content_settings_for_legacy_cookie_access(settings);
}

void CookieManager::SetStorageAccessGrantSettings(
    const ContentSettingsForOneType& settings,
    SetStorageAccessGrantSettingsCallback callback) {
  cookie_settings_.set_storage_access_grants(settings);

  // Signal our storage update is complete.
  std::move(callback).Run();
}

// static
void CookieManager::ConfigureCookieSettings(
    const network::mojom::CookieManagerParams& params,
    CookieSettings* out) {
  out->set_block_third_party_cookies(params.block_third_party_cookies);
  out->set_content_settings(params.settings);
  out->set_secure_origin_cookies_allowed_schemes(
      params.secure_origin_cookies_allowed_schemes);
  out->set_matching_scheme_cookies_allowed_schemes(
      params.matching_scheme_cookies_allowed_schemes);
  out->set_third_party_cookies_allowed_schemes(
      params.third_party_cookies_allowed_schemes);
  out->set_content_settings_for_legacy_cookie_access(
      params.settings_for_legacy_cookie_access);
  out->set_storage_access_grants(params.settings_for_storage_access);
}

void CookieManager::CrashOnGetCookieList() {
  g_crash_on_get_cookie_list = true;
}

CookieDeletionInfo DeletionFilterToInfo(mojom::CookieDeletionFilterPtr filter) {
  CookieDeletionInfo delete_info;

  if (filter->created_after_time.has_value() &&
      !filter->created_after_time.value().is_null()) {
    delete_info.creation_range.SetStart(filter->created_after_time.value());
  }
  if (filter->created_before_time.has_value() &&
      !filter->created_before_time.value().is_null()) {
    delete_info.creation_range.SetEnd(filter->created_before_time.value());
  }
  delete_info.name = std::move(filter->cookie_name);
  delete_info.url = std::move(filter->url);
  delete_info.host = std::move(filter->host_name);

  switch (filter->session_control) {
    case mojom::CookieDeletionSessionControl::IGNORE_CONTROL:
      delete_info.session_control = CookieDeleteSessionControl::IGNORE_CONTROL;
      break;
    case mojom::CookieDeletionSessionControl::SESSION_COOKIES:
      delete_info.session_control = CookieDeleteSessionControl::SESSION_COOKIES;
      break;
    case mojom::CookieDeletionSessionControl::PERSISTENT_COOKIES:
      delete_info.session_control =
          CookieDeleteSessionControl::PERSISTENT_COOKIES;
      break;
  }

  if (filter->including_domains.has_value()) {
    delete_info.domains_and_ips_to_delete.insert(
        filter->including_domains.value().begin(),
        filter->including_domains.value().end());
  }
  if (filter->excluding_domains.has_value()) {
    delete_info.domains_and_ips_to_ignore.insert(
        filter->excluding_domains.value().begin(),
        filter->excluding_domains.value().end());
  }

  delete_info.cookie_partition_key_collection =
      filter->cookie_partition_key_collection
          ? *filter->cookie_partition_key_collection
          : net::CookiePartitionKeyCollection::ContainsAll();

  return delete_info;
}

}  // namespace network
