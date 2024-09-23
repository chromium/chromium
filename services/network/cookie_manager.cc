// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/network/cookie_manager.h"

#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/process/process.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
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
#include "services/network/tpcd/metadata/manager.h"
#include "url/gurl.h"

using CookieDeletionInfo = net::CookieDeletionInfo;
using CookieDeleteSessionControl = net::CookieDeletionInfo::SessionControl;

namespace network {

namespace {

using CookiePartitionKeyPairCallback = base::OnceCallback<void(
    std::pair<bool, std::optional<net::CookiePartitionKey>>)>;

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
    mojom::CookieManagerParamsPtr params,
    network::tpcd::metadata::Manager* tpcd_metadata_manager)
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
    cookie_settings_.set_tpcd_metadata_manager(tpcd_metadata_manager);
  }
  cookie_store_->SetCookieAccessDelegate(
      std::make_unique<CookieAccessDelegateImpl>(
          cookie_access_delegate_type, first_party_sets_access_delegate,
          &cookie_settings_));
}

CookieManager::~CookieManager() {
  // The cookie manager will go away which means potentially clearing cookies if
  // policy calls for it. This can be important for background mode for which
  // renderers might stay active.
  OnSettingsWillChange();

  if (session_cleanup_cookie_store_) {
    session_cleanup_cookie_store_->DeleteSessionCookies(
        cookie_settings_.CreateDeleteCookieOnExitPredicate());
  }
  // Make sure we destroy the CookieStore's CookieAccessDelegate, because it
  // holds a pointer to this CookieManager's CookieSettings, which is about to
  // be destroyed.
  cookie_store_->SetCookieAccessDelegate(nullptr);
}

void CookieManager::AddSettingsWillChangeCallback(
    SettingsChangeCallback callback) {
  CHECK(!settings_will_change_callback_);
  settings_will_change_callback_ = callback;
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
  const std::optional<net::CookiePartitionKey>& cookie_partition_key =
      cookie.PartitionKey();

  auto cookie_ptr = std::make_unique<net::CanonicalCookie>(cookie);
  base::Time adjusted_expiry_date =
      net::CanonicalCookie::ValidateAndAdjustExpiryDate(
          cookie.ExpiryDate(), cookie.CreationDate(), cookie.SourceScheme());
  if (adjusted_expiry_date != cookie.ExpiryDate() || !cookie_partition_key) {
    cookie_ptr = net::CanonicalCookie::FromStorage(
        cookie.Name(), cookie.Value(), cookie.Domain(), cookie.Path(),
        cookie.CreationDate(), adjusted_expiry_date, cookie.LastAccessDate(),
        cookie.LastUpdateDate(), cookie.SecureAttribute(), cookie.IsHttpOnly(),
        cookie.SameSite(), cookie.Priority(), cookie_partition_key,
        cookie.SourceScheme(), cookie.SourcePort(), cookie.SourceType());
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
    ContentSettingsType content_settings_type,
    const ContentSettingsForOneType& settings,
    SetContentSettingsCallback callback) {
  OnSettingsWillChange();
  cookie_settings_.set_content_settings(content_settings_type, settings);
  if (callback) {
    std::move(callback).Run();
  }
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
            return predicate.Run(cookie.Domain(), cookie.SourceScheme());
          },
          std::move(delete_cookie_predicate)),
      std::move(callback));
}

void CookieManager::DeleteStaleSessionOnlyCookies(
    DeleteStaleSessionOnlyCookiesCallback callback) {
  cookie_store_->DeleteMatchingCookiesAsync(
      base::BindRepeating([](const net::CanonicalCookie& cookie) {
        // We do not delete persistent cookies.
        if (cookie.IsPersistent()) {
          return false;
        }

        // We only examine the newer date between the last read/write time.
        const base::Time last_accessed_or_updated =
            cookie.LastAccessDate() > cookie.LastUpdateDate()
                ? cookie.LastAccessDate()
                : cookie.LastUpdateDate();

        // Without timing data we cannot delete the cookie.
        if (last_accessed_or_updated.is_null()) {
          return false;
        }

        // Delete cookies that haven't been accessed or updated in 7 days.
        // See crbug.com/40285083 for more info.
        return (base::Time::Now() - last_accessed_or_updated) > base::Days(7);
      }),
      std::move(callback));
}

void CookieManager::AddCookieChangeListener(
    const GURL& url,
    const std::optional<std::string>& name,
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
    // TODO(crbug.com/40188414): Include the correct cookie partition
    // key when attaching cookie change listeners to service workers.
    listener_registration->subscription =
        cookie_store_->GetChangeDispatcher().AddCallbackForCookie(
            url, *name, std::nullopt, std::move(cookie_change_callback));
  } else {
    // TODO(crbug.com/40188414): Include the correct cookie partition
    // key when attaching cookie change listeners to service workers.
    listener_registration->subscription =
        cookie_store_->GetChangeDispatcher().AddCallbackForUrl(
            url, std::nullopt, std::move(cookie_change_callback));
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
  NOTREACHED_IN_MIGRATION();
}

void CookieManager::CloneInterface(
    mojo::PendingReceiver<mojom::CookieManager> new_interface) {
  AddReceiver(std::move(new_interface));
}

void CookieManager::SetPreCommitCallbackDelayForTesting(base::TimeDelta delay) {
  session_cleanup_cookie_store_->SetBeforeCommitCallback(base::BindRepeating(
      [](base::TimeDelta delay) { base::PlatformThread::Sleep(delay); },
      delay));
}

void CookieManager::FlushCookieStore(FlushCookieStoreCallback callback) {
  // Flushes the backing store (if any) to disk.
  cookie_store_->FlushStore(std::move(callback));
}

void CookieManager::AllowFileSchemeCookies(
    bool allow,
    AllowFileSchemeCookiesCallback callback) {
  OnSettingsWillChange();

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
  OnSettingsWillChange();
  cookie_store_->SetForceKeepSessionState();
}

void CookieManager::BlockThirdPartyCookies(bool block) {
  OnSettingsWillChange();
  cookie_settings_.set_block_third_party_cookies(block);
}

void CookieManager::SetMitigationsEnabledFor3pcd(bool enable) {
  OnSettingsWillChange();
  cookie_settings_.set_mitigations_enabled_for_3pcd(enable);
}

void CookieManager::SetTrackingProtectionEnabledFor3pcd(bool enable) {
  OnSettingsWillChange();
  cookie_settings_.set_tracking_protection_enabled_for_3pcd(enable);
}

void CookieManager::OnSettingsWillChange() {
  if (settings_will_change_callback_) {
    settings_will_change_callback_.Run();
  }
}

// static
void CookieManager::ConfigureCookieSettings(
    const network::mojom::CookieManagerParams& params,
    CookieSettings* out) {
  out->set_block_third_party_cookies(params.block_third_party_cookies);
  out->set_mitigations_enabled_for_3pcd(params.mitigations_enabled_for_3pcd);
  out->set_tracking_protection_enabled_for_3pcd(
      params.tracking_protection_enabled_for_3pcd);
  out->set_secure_origin_cookies_allowed_schemes(
      params.secure_origin_cookies_allowed_schemes);
  out->set_matching_scheme_cookies_allowed_schemes(
      params.matching_scheme_cookies_allowed_schemes);
  out->set_third_party_cookies_allowed_schemes(
      params.third_party_cookies_allowed_schemes);
  for (const auto& [type, settings] : params.content_settings) {
    out->set_content_settings(type, settings);
  }
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
    delete_info.domains_and_ips_to_delete.emplace(
        filter->including_domains.value().begin(),
        filter->including_domains.value().end());
  }
  if (filter->excluding_domains.has_value()) {
    delete_info.domains_and_ips_to_ignore.emplace(
        filter->excluding_domains.value().begin(),
        filter->excluding_domains.value().end());
  }

  delete_info.cookie_partition_key_collection =
      filter->cookie_partition_key_collection
          ? *filter->cookie_partition_key_collection
          : net::CookiePartitionKeyCollection::ContainsAll();

  delete_info.partitioned_state_only = filter->partitioned_state_only;

  return delete_info;
}

}  // namespace network
