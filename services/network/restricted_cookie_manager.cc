// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/restricted_cookie_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"  // for FALLTHROUGH;
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/isolation_info.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_util.h"
#include "services/network/cookie_settings.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "url/gurl.h"

namespace network {

namespace {

net::CookieOptions MakeOptionsForSet(
    mojom::RestrictedCookieManagerRole role,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const net::IsolationInfo& isolation_info,
    const CookieSettings* cookie_settings,
    const net::CookieAccessDelegate* cookie_access_delegate) {
  net::CookieOptions options;
  bool force_ignore_site_for_cookies =
      cookie_settings->ShouldIgnoreSameSiteRestrictions(
          url, site_for_cookies.RepresentativeUrl());
  if (role == mojom::RestrictedCookieManagerRole::SCRIPT) {
    options.set_exclude_httponly();  // Default, but make it explicit here.
    options.set_same_site_cookie_context(
        net::cookie_util::ComputeSameSiteContextForScriptSet(
            url, site_for_cookies, force_ignore_site_for_cookies));
  } else {
    // mojom::RestrictedCookieManagerRole::NETWORK
    options.set_include_httponly();
    options.set_same_site_cookie_context(
        net::cookie_util::ComputeSameSiteContextForSubresource(
            url, site_for_cookies, force_ignore_site_for_cookies));
  }
  net::SchemefulSite request_site(url);
  options.set_same_party_cookie_context_type(
      net::cookie_util::ComputeSamePartyContext(request_site, isolation_info,
                                                cookie_access_delegate));
  if (isolation_info.party_context().has_value()) {
    // Count the top-frame site since it's not in the party_context.
    options.set_full_party_context_size(isolation_info.party_context()->size() +
                                        1);
  }
  bool is_in_nontrivial_first_party_set =
      cookie_access_delegate &&
      cookie_access_delegate->IsInNontrivialFirstPartySet(request_site);
  options.set_is_in_nontrivial_first_party_set(
      is_in_nontrivial_first_party_set);

  return options;
}

net::CookieOptions MakeOptionsForGet(
    mojom::RestrictedCookieManagerRole role,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const net::IsolationInfo& isolation_info,
    const CookieSettings* cookie_settings,
    const net::CookieAccessDelegate* cookie_access_delegate) {
  // TODO(https://crbug.com/925311): Wire initiator here.
  net::CookieOptions options;
  bool force_ignore_site_for_cookies =
      cookie_settings->ShouldIgnoreSameSiteRestrictions(
          url, site_for_cookies.RepresentativeUrl());
  if (role == mojom::RestrictedCookieManagerRole::SCRIPT) {
    options.set_exclude_httponly();  // Default, but make it explicit here.
    options.set_same_site_cookie_context(
        net::cookie_util::ComputeSameSiteContextForScriptGet(
            url, site_for_cookies, base::nullopt /*initiator*/,
            force_ignore_site_for_cookies));
  } else {
    // mojom::RestrictedCookieManagerRole::NETWORK
    options.set_include_httponly();
    options.set_same_site_cookie_context(
        net::cookie_util::ComputeSameSiteContextForSubresource(
            url, site_for_cookies, force_ignore_site_for_cookies));
  }
  net::SchemefulSite request_site(url);
  options.set_same_party_cookie_context_type(
      net::cookie_util::ComputeSamePartyContext(request_site, isolation_info,
                                                cookie_access_delegate));
  if (isolation_info.party_context().has_value()) {
    // Count the top-frame site since it's not in the party_context.
    options.set_full_party_context_size(isolation_info.party_context()->size() +
                                        1);
  }
  bool is_in_nontrivial_first_party_set =
      cookie_access_delegate &&
      cookie_access_delegate->IsInNontrivialFirstPartySet(request_site);
  options.set_is_in_nontrivial_first_party_set(
      is_in_nontrivial_first_party_set);

  return options;
}

}  // namespace

class RestrictedCookieManager::Listener : public base::LinkNode<Listener> {
 public:
  Listener(net::CookieStore* cookie_store,
           const RestrictedCookieManager* restricted_cookie_manager,
           const GURL& url,
           const net::SiteForCookies& site_for_cookies,
           const url::Origin& top_frame_origin,
           net::CookieOptions options,
           mojo::PendingRemote<mojom::CookieChangeListener> mojo_listener)
      : cookie_store_(cookie_store),
        restricted_cookie_manager_(restricted_cookie_manager),
        url_(url),
        site_for_cookies_(site_for_cookies),
        top_frame_origin_(top_frame_origin),
        options_(options),
        mojo_listener_(std::move(mojo_listener)) {
    // TODO(pwnall): add a constructor w/options to net::CookieChangeDispatcher.
    cookie_store_subscription_ =
        cookie_store->GetChangeDispatcher().AddCallbackForUrl(
            url, base::BindRepeating(
                     &Listener::OnCookieChange,
                     // Safe because net::CookieChangeDispatcher guarantees that
                     // the callback will stop being called immediately after we
                     // remove the subscription, and the cookie store lives on
                     // the same thread as we do.
                     base::Unretained(this)));
  }

  ~Listener() { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

  mojo::Remote<mojom::CookieChangeListener>& mojo_listener() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return mojo_listener_;
  }

 private:
  // net::CookieChangeDispatcher callback.
  void OnCookieChange(const net::CookieChangeInfo& change) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    bool delegate_treats_url_as_trustworthy =
        cookie_store_->cookie_access_delegate() &&
        cookie_store_->cookie_access_delegate()->ShouldTreatUrlAsTrustworthy(
            url_);

    // CookieChangeDispatcher doesn't check for inclusion against `options_`, so
    // we need to double-check that.
    net::CookieSamePartyStatus same_party_status =
        net::cookie_util::GetSamePartyStatus(change.cookie, options_);

    if (!change.cookie
             .IncludeForRequestURL(
                 url_, options_,
                 net::CookieAccessParams{change.access_result.access_semantics,
                                         delegate_treats_url_as_trustworthy,
                                         same_party_status})
             .status.IsInclude()) {
      return;
    }

    // When a user blocks a site's access to cookies, the existing cookies are
    // not deleted. This check prevents the site from observing their cookies
    // being deleted at a later time, which can happen due to eviction or due to
    // the user explicitly deleting all cookies.
    if (!restricted_cookie_manager_->cookie_settings()->IsCookieAccessAllowed(
            url_, site_for_cookies_.RepresentativeUrl(), top_frame_origin_)) {
      return;
    }

    mojo_listener_->OnCookieChange(change);
  }

  // Expected to outlive |restricted_cookie_manager_| which outlives this.
  const net::CookieStore* cookie_store_;

  // The CookieChangeDispatcher subscription used by this listener.
  std::unique_ptr<net::CookieChangeSubscription> cookie_store_subscription_;

  // Raw pointer usage is safe because RestrictedCookieManager owns this
  // instance and is guaranteed to outlive it.
  const RestrictedCookieManager* const restricted_cookie_manager_;

  // The URL whose cookies this listener is interested in.
  const GURL url_;

  // Site context in which we're used; used to determine if a cookie is accessed
  // in a third-party context.
  const net::SiteForCookies site_for_cookies_;

  // Site context in which we're used; used to check content settings.
  const url::Origin top_frame_origin_;

  // CanonicalCookie::IncludeForRequestURL options for this listener's interest.
  const net::CookieOptions options_;

  mojo::Remote<mojom::CookieChangeListener> mojo_listener_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(Listener);
};

RestrictedCookieManager::RestrictedCookieManager(
    const mojom::RestrictedCookieManagerRole role,
    net::CookieStore* cookie_store,
    const CookieSettings* cookie_settings,
    const url::Origin& origin,
    const net::IsolationInfo& isolation_info,
    mojo::PendingRemote<mojom::CookieAccessObserver> cookie_observer)
    : role_(role),
      cookie_store_(cookie_store),
      cookie_settings_(cookie_settings),
      origin_(origin),
      site_for_cookies_(isolation_info.site_for_cookies()),
      top_frame_origin_(isolation_info.top_frame_origin().value()),
      isolation_info_(isolation_info),
      cookie_observer_(std::move(cookie_observer)) {
  DCHECK(cookie_store);
  CHECK(origin_ == isolation_info_.frame_origin().value() ||
        role_ != mojom::RestrictedCookieManagerRole::SCRIPT);
}

RestrictedCookieManager::~RestrictedCookieManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::LinkNode<Listener>* node = listeners_.head();
  while (node != listeners_.end()) {
    Listener* listener_reference = node->value();
    node = node->next();
    // The entire list is going away, no need to remove nodes from it.
    delete listener_reference;
  }
}

void RestrictedCookieManager::GetAllForUrl(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    mojom::CookieManagerGetOptionsPtr options,
    GetAllForUrlCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ValidateAccessToCookiesAt(url, site_for_cookies, top_frame_origin)) {
    std::move(callback).Run({});
    return;
  }

  // TODO(morlovich): Try to validate site_for_cookies as well.

  net::CookieOptions net_options = MakeOptionsForGet(
      role_, url, site_for_cookies, isolation_info_, cookie_settings(),
      cookie_store_->cookie_access_delegate());
  // TODO(https://crbug.com/977040): remove set_return_excluded_cookies() once
  //                                 removing deprecation warnings.
  net_options.set_return_excluded_cookies();

  cookie_store_->GetCookieListWithOptionsAsync(
      url, net_options,
      base::BindOnce(&RestrictedCookieManager::CookieListToGetAllForUrlCallback,
                     weak_ptr_factory_.GetWeakPtr(), url, site_for_cookies,
                     top_frame_origin, net_options, std::move(options),
                     std::move(callback)));
}

void RestrictedCookieManager::CookieListToGetAllForUrlCallback(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    const net::CookieOptions& net_options,
    mojom::CookieManagerGetOptionsPtr options,
    GetAllForUrlCallback callback,
    const net::CookieAccessResultList& cookie_list,
    const net::CookieAccessResultList& excluded_cookies) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool blocked = !cookie_settings_->IsCookieAccessAllowed(
      url, site_for_cookies.RepresentativeUrl(), top_frame_origin);

  std::vector<net::CookieWithAccessResult> result;
  std::vector<mojom::CookieOrLineWithAccessResultPtr>
      on_cookies_accessed_result;

  // TODO(https://crbug.com/977040): Remove once samesite tightening up is
  // rolled out.
  for (const auto& cookie_and_access_result : excluded_cookies) {
    if (cookie_and_access_result.access_result.status.ShouldWarn()) {
      on_cookies_accessed_result.push_back(
          mojom::CookieOrLineWithAccessResult::New(
              mojom::CookieOrLine::NewCookie(cookie_and_access_result.cookie),
              cookie_and_access_result.access_result));
    }
  }

  if (!blocked)
    result.reserve(cookie_list.size());
  mojom::CookieMatchType match_type = options->match_type;
  const std::string& match_name = options->name;
  // TODO(https://crbug.com/993843): Use the statuses passed in |cookie_list|.
  for (const net::CookieWithAccessResult& cookie_item : cookie_list) {
    const net::CanonicalCookie& cookie = cookie_item.cookie;
    net::CookieAccessResult access_result = cookie_item.access_result;
    const std::string& cookie_name = cookie.Name();

    if (match_type == mojom::CookieMatchType::EQUALS) {
      if (cookie_name != match_name)
        continue;
    } else if (match_type == mojom::CookieMatchType::STARTS_WITH) {
      if (!base::StartsWith(cookie_name, match_name,
                            base::CompareCase::SENSITIVE)) {
        continue;
      }
    } else {
      NOTREACHED();
    }

    if (blocked) {
      access_result.status.AddExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES);
    } else {
      result.push_back(cookie_item);
    }
    on_cookies_accessed_result.push_back(
        mojom::CookieOrLineWithAccessResult::New(
            mojom::CookieOrLine::NewCookie(cookie), access_result));
  }

  if (cookie_observer_) {
    cookie_observer_->OnCookiesAccessed(mojom::CookieAccessDetails::New(
        mojom::CookieAccessDetails::Type::kRead, url, site_for_cookies,
        std::move(on_cookies_accessed_result), base::nullopt));
  }

  if (blocked) {
    DCHECK(result.empty());
    std::move(callback).Run({});
    return;
  }

  std::move(callback).Run(std::move(result));
}

void RestrictedCookieManager::SetCanonicalCookie(
    const net::CanonicalCookie& cookie,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    SetCanonicalCookieCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!ValidateAccessToCookiesAt(url, site_for_cookies, top_frame_origin,
                                 &cookie)) {
    std::move(callback).Run(false);
    return;
  }

  // TODO(morlovich): Try to validate site_for_cookies as well.
  bool blocked = !cookie_settings_->IsCookieAccessAllowed(
      url, site_for_cookies.RepresentativeUrl(), top_frame_origin);

  net::CookieInclusionStatus status;
  if (blocked)
    status.AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES);

  // Don't allow URLs with leading dots like https://.some-weird-domain.com
  // This probably never happens.
  if (!net::cookie_util::DomainIsHostOnly(url.host()))
    status.AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN);

  if (!status.IsInclude()) {
    if (cookie_observer_) {
      std::vector<network::mojom::CookieOrLineWithAccessResultPtr>
          result_with_access_result;
      result_with_access_result.push_back(
          mojom::CookieOrLineWithAccessResult::New(
              mojom::CookieOrLine::NewCookie(cookie),
              net::CookieAccessResult(status)));
      cookie_observer_->OnCookiesAccessed(mojom::CookieAccessDetails::New(
          mojom::CookieAccessDetails::Type::kChange, url, site_for_cookies,
          std::move(result_with_access_result), base::nullopt));
    }
    std::move(callback).Run(false);
    return;
  }

  // TODO(pwnall): Validate the CanonicalCookie fields.

  // Update the creation and last access times.
  base::Time now = base::Time::NowFromSystemTime();
  // TODO(http://crbug.com/1024053): Log metrics
  const GURL& origin_url = origin_.GetURL();
  net::CookieSourceScheme source_scheme =
      GURL::SchemeIsCryptographic(origin_.scheme())
          ? net::CookieSourceScheme::kSecure
          : net::CookieSourceScheme::kNonSecure;
  auto sanitized_cookie = net::CanonicalCookie::FromStorage(
      cookie.Name(), cookie.Value(), cookie.Domain(), cookie.Path(), now,
      cookie.ExpiryDate(), now, cookie.IsSecure(), cookie.IsHttpOnly(),
      cookie.SameSite(), cookie.Priority(), cookie.IsSameParty(), source_scheme,
      origin_.port());
  DCHECK(sanitized_cookie);
  net::CanonicalCookie cookie_copy = *sanitized_cookie;

  net::CookieOptions options = MakeOptionsForSet(
      role_, url, site_for_cookies, isolation_info_, cookie_settings(),
      cookie_store_->cookie_access_delegate());

  cookie_store_->SetCanonicalCookieAsync(
      std::move(sanitized_cookie), origin_url, options,
      base::BindOnce(&RestrictedCookieManager::SetCanonicalCookieResult,
                     weak_ptr_factory_.GetWeakPtr(), url, site_for_cookies,
                     cookie_copy, options, std::move(callback)));
}

void RestrictedCookieManager::SetCanonicalCookieResult(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const net::CanonicalCookie& cookie,
    const net::CookieOptions& net_options,
    SetCanonicalCookieCallback user_callback,
    net::CookieAccessResult access_result) {
  // TODO(https://crbug.com/977040): Only report pure INCLUDE once samesite
  // tightening up is rolled out.
  DCHECK(!access_result.status.HasExclusionReason(
      net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES));

  if (access_result.status.IsInclude() || access_result.status.ShouldWarn()) {
    if (cookie_observer_) {
      std::vector<mojom::CookieOrLineWithAccessResultPtr> notify;
      notify.push_back(mojom::CookieOrLineWithAccessResult::New(
          mojom::CookieOrLine::NewCookie(cookie), access_result));
      cookie_observer_->OnCookiesAccessed(mojom::CookieAccessDetails::New(
          mojom::CookieAccessDetails::Type::kChange, url, site_for_cookies,
          std::move(notify), base::nullopt));
    }
  }
  std::move(user_callback).Run(access_result.status.IsInclude());
}

void RestrictedCookieManager::AddChangeListener(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    mojo::PendingRemote<mojom::CookieChangeListener> mojo_listener,
    AddChangeListenerCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!ValidateAccessToCookiesAt(url, site_for_cookies, top_frame_origin)) {
    std::move(callback).Run();
    return;
  }

  net::CookieOptions net_options = MakeOptionsForGet(
      role_, url, site_for_cookies, isolation_info_, cookie_settings(),
      cookie_store_->cookie_access_delegate());
  auto listener = std::make_unique<Listener>(
      cookie_store_, this, url, site_for_cookies, top_frame_origin, net_options,
      std::move(mojo_listener));

  listener->mojo_listener().set_disconnect_handler(
      base::BindOnce(&RestrictedCookieManager::RemoveChangeListener,
                     weak_ptr_factory_.GetWeakPtr(),
                     // Safe because this owns the listener, so the listener is
                     // guaranteed to be alive for as long as the weak pointer
                     // above resolves.
                     base::Unretained(listener.get())));

  // The linked list takes over the Listener ownership.
  listeners_.Append(listener.release());
  std::move(callback).Run();
}

void RestrictedCookieManager::SetCookieFromString(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    const std::string& cookie,
    SetCookieFromStringCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  net::CookieInclusionStatus status;
  std::unique_ptr<net::CanonicalCookie> parsed_cookie =
      net::CanonicalCookie::Create(url, cookie, base::Time::Now(),
                                   base::nullopt /* server_time */, &status);
  if (!parsed_cookie) {
    if (cookie_observer_) {
      std::vector<network::mojom::CookieOrLineWithAccessResultPtr>
          result_with_access_result;
      result_with_access_result.push_back(
          mojom::CookieOrLineWithAccessResult::New(
              mojom::CookieOrLine::NewCookieString(cookie),
              net::CookieAccessResult(status)));
      cookie_observer_->OnCookiesAccessed(mojom::CookieAccessDetails::New(
          mojom::CookieAccessDetails::Type::kChange, url, site_for_cookies,
          std::move(result_with_access_result), base::nullopt));
    }
    std::move(callback).Run();
    return;
  }

  // Further checks (origin_, settings), as well as logging done by
  // SetCanonicalCookie()
  SetCanonicalCookie(
      *parsed_cookie, url, site_for_cookies, top_frame_origin,
      base::BindOnce([](SetCookieFromStringCallback user_callback,
                        bool success) { std::move(user_callback).Run(); },
                     std::move(callback)));
}

void RestrictedCookieManager::GetCookiesString(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    GetCookiesStringCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Checks done by GetAllForUrl.

  // Match everything.
  auto match_options = mojom::CookieManagerGetOptions::New();
  match_options->name = "";
  match_options->match_type = mojom::CookieMatchType::STARTS_WITH;
  GetAllForUrl(url, site_for_cookies, top_frame_origin,
               std::move(match_options),
               base::BindOnce(
                   [](GetCookiesStringCallback user_callback,
                      const std::vector<net::CookieWithAccessResult>& cookies) {
                     std::move(user_callback)
                         .Run(net::CanonicalCookie::BuildCookieLine(cookies));
                   },
                   std::move(callback)));
}

void RestrictedCookieManager::CookiesEnabledFor(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    CookiesEnabledForCallback callback) {
  if (!ValidateAccessToCookiesAt(url, site_for_cookies, top_frame_origin)) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(cookie_settings_->IsCookieAccessAllowed(
      url, site_for_cookies.RepresentativeUrl(), top_frame_origin));
}

void RestrictedCookieManager::RemoveChangeListener(Listener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  listener->RemoveFromList();
  delete listener;
}

bool RestrictedCookieManager::ValidateAccessToCookiesAt(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    const net::CanonicalCookie* cookie_being_set) {
  if (origin_.opaque()) {
    mojo::ReportBadMessage("Access is denied in this context");
    return false;
  }

  bool site_for_cookies_ok = site_for_cookies_.IsEquivalent(site_for_cookies);
  DCHECK(site_for_cookies_ok)
      << "site_for_cookies from renderer='" << site_for_cookies.ToDebugString()
      << "' from browser='" << site_for_cookies_.ToDebugString() << "';";

  bool top_frame_origin_ok = (top_frame_origin == top_frame_origin_);
  DCHECK(top_frame_origin_ok)
      << "top_frame_origin from renderer='" << top_frame_origin
      << "' from browser='" << top_frame_origin_ << "';";

  UMA_HISTOGRAM_BOOLEAN("Net.RestrictedCookieManager.SiteForCookiesOK",
                        site_for_cookies_ok);
  UMA_HISTOGRAM_BOOLEAN("Net.RestrictedCookieManager.TopFrameOriginOK",
                        top_frame_origin_ok);

  // Don't allow setting cookies on other domains. See crbug.com/996786.
  if (cookie_being_set && !cookie_being_set->IsDomainMatch(url.host())) {
    mojo::ReportBadMessage("Setting cookies on other domains is disallowed.");
    return false;
  }

  if (origin_.IsSameOriginWith(url::Origin::Create(url)))
    return true;

  if (url.IsAboutBlank() || url.IsAboutSrcdoc()) {
    // Temporary mitigation for 983090, classification improvement for parts of
    // 992587.
    static base::debug::CrashKeyString* bound_origin =
        base::debug::AllocateCrashKeyString(
            "restricted_cookie_manager_bound_origin",
            base::debug::CrashKeySize::Size256);
    base::debug::ScopedCrashKeyString scoped_key_string_bound(
        bound_origin, origin_.GetDebugString());

    static base::debug::CrashKeyString* url_origin =
        base::debug::AllocateCrashKeyString(
            "restricted_cookie_manager_url_origin",
            base::debug::CrashKeySize::Size256);
    base::debug::ScopedCrashKeyString scoped_key_string_url(
        url_origin, url::Origin::Create(url).GetDebugString());

    NOTREACHED();
    base::debug::DumpWithoutCrashing();
    return false;
  }

  mojo::ReportBadMessage("Incorrect url origin");
  return false;
}

}  // namespace network
