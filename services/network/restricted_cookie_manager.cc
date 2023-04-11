// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/restricted_cookie_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/compiler_specific.h"  // for [[fallthrough]];
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/optional_util.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/site_for_cookies.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "services/network/cookie_settings.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "url/gurl.h"

namespace network {

namespace {

// How often to call CookieObserveer.OnCookiesAccessed. This value was picked
// because it reduces calls by up to 90% on slow Android devices while not
// adding a user-perceptible delay.
constexpr base::TimeDelta kCookiesAccessedTimeout = base::Milliseconds(100);

// TODO(cfredric): the `force_ignore_top_frame_party` param being false prevents
// `document.cookie` access for same-party scripts embedded in an extension
// frame. It would be better if we allowed that similarly to how we allow
// SameParty cookies for requests in same-party contexts embedded in top-level
// extension frames.
const bool kForceIgnoreTopFrameParty = false;

net::CookieOptions MakeOptionsForSet(
    mojom::RestrictedCookieManagerRole role,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const net::IsolationInfo& isolation_info,
    const CookieSettings& cookie_settings,
    const net::FirstPartySetMetadata& first_party_set_metadata) {
  net::CookieOptions options;
  bool force_ignore_site_for_cookies =
      cookie_settings.ShouldIgnoreSameSiteRestrictions(url, site_for_cookies);
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
  options.set_same_party_context(first_party_set_metadata.context());
  if (isolation_info.party_context().has_value()) {
    // Count the top-frame site since it's not in the party_context.
    options.set_full_party_context_size(isolation_info.party_context()->size() +
                                        1);
  }
  options.set_is_in_nontrivial_first_party_set(
      first_party_set_metadata.frame_entry().has_value());

  return options;
}

net::CookieOptions MakeOptionsForGet(
    mojom::RestrictedCookieManagerRole role,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const net::IsolationInfo& isolation_info,
    const CookieSettings& cookie_settings,
    const net::FirstPartySetMetadata& first_party_set_metadata) {
  // TODO(https://crbug.com/925311): Wire initiator here.
  net::CookieOptions options;
  bool force_ignore_site_for_cookies =
      cookie_settings.ShouldIgnoreSameSiteRestrictions(url, site_for_cookies);
  if (role == mojom::RestrictedCookieManagerRole::SCRIPT) {
    options.set_exclude_httponly();  // Default, but make it explicit here.
    options.set_same_site_cookie_context(
        net::cookie_util::ComputeSameSiteContextForScriptGet(
            url, site_for_cookies, absl::nullopt /*initiator*/,
            force_ignore_site_for_cookies));
  } else {
    // mojom::RestrictedCookieManagerRole::NETWORK
    options.set_include_httponly();
    options.set_same_site_cookie_context(
        net::cookie_util::ComputeSameSiteContextForSubresource(
            url, site_for_cookies, force_ignore_site_for_cookies));
  }
  options.set_same_party_context(first_party_set_metadata.context());
  if (isolation_info.party_context().has_value()) {
    // Count the top-frame site since it's not in the party_context.
    options.set_full_party_context_size(isolation_info.party_context()->size() +
                                        1);
  }
  options.set_is_in_nontrivial_first_party_set(
      first_party_set_metadata.frame_entry().has_value());

  return options;
}

}  // namespace

RestrictedCookieManager::UmaMetricsUpdater::UmaMetricsUpdater() = default;
RestrictedCookieManager::UmaMetricsUpdater::~UmaMetricsUpdater() = default;

// static
void RestrictedCookieManager::ComputeFirstPartySetMetadata(
    const url::Origin& origin,
    const net::CookieStore* cookie_store,
    const net::IsolationInfo& isolation_info,
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback) {
  std::pair<base::OnceCallback<void(net::FirstPartySetMetadata)>,
            base::OnceCallback<void(net::FirstPartySetMetadata)>>
      callbacks = base::SplitOnceCallback(std::move(callback));
  absl::optional<net::FirstPartySetMetadata> metadata =
      net::cookie_util::ComputeFirstPartySetMetadataMaybeAsync(
          /*request_site=*/net::SchemefulSite(origin), isolation_info,
          cookie_store->cookie_access_delegate(), kForceIgnoreTopFrameParty,
          std::move(callbacks.first));
  if (metadata.has_value())
    std::move(callbacks.second).Run(std::move(metadata.value()));
}

bool CookieWithAccessResultComparer::operator()(
    const net::CookieWithAccessResult& cookie_with_access_result1,
    const net::CookieWithAccessResult& cookie_with_access_result2) const {
  // Compare just the cookie portion of the CookieWithAccessResults so a cookie
  // only ever has one entry in the map. For a given cookie we want to send a
  // new access notification whenever its access results change. If we keyed off
  // of both the cookie and its current access result, if a cookie shifted from
  // "allowed" to "blocked" the cookie would wind up with two entries in the
  // map. If the cookie then shifted back to "allowed" we wouldn't send a new
  // notification because cookie/allowed already existed in the map. In the case
  // of a cookie shifting from "allowed" to "blocked,"
  // SkipAccessNotificationForCookieItem() checks the access result. If the
  // cookie exists in the map but its status is "allowed" we evict the old
  // entry.
  return cookie_with_access_result1.cookie < cookie_with_access_result2.cookie;
}

CookieAccesses* RestrictedCookieManager::GetCookieAccessesForURLAndSite(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies) {
  std::unique_ptr<CookieAccesses>& entry =
      recent_cookie_accesses_[std::make_pair(url, site_for_cookies)];
  if (!entry) {
    entry = std::make_unique<CookieAccesses>();
  }

  return entry.get();
}

bool RestrictedCookieManager::SkipAccessNotificationForCookieItem(
    CookieAccesses* cookie_accesses,
    const net::CookieWithAccessResult& cookie_item) {
  DCHECK(cookie_accesses);

  // Have we sent information about this cookie to the |cookie_observer_|
  // before?
  std::set<net::CookieWithAccessResult>::iterator existing_slot =
      cookie_accesses->find(cookie_item);

  // If this is the first time seeing this cookie make a note and don't skip
  // the notification.
  if (existing_slot == cookie_accesses->end()) {
    // Don't store more than a max number of cookies, in the interest of
    // limiting memory consumption.
    const int kMaxCookieCount = 32;
    if (cookie_accesses->size() == kMaxCookieCount) {
      cookie_accesses->clear();
    }
    cookie_accesses->insert(cookie_item);

    return false;
  }

  // If the cookie and its access result are unchanged since we last updated
  // the |cookie_observer_|, skip notifying the |cookie_observer_| again.
  if (existing_slot->cookie.HasEquivalentDataMembers(cookie_item.cookie) &&
      existing_slot->access_result == cookie_item.access_result) {
    return true;
  }

  // The cookie's access result has changed - update it in our record of what
  // we've sent to the |cookie_observer_|. It's safe to update the existing
  // entry in the set because the access_result field does not determine the
  // CookieWithAccessResult's location in the set.
  const_cast<net::CookieWithAccessResult&>(*existing_slot).access_result =
      cookie_item.access_result;

  // Don't skip notifying the |cookie_observer_| of the change.
  return false;
}

class RestrictedCookieManager::Listener : public base::LinkNode<Listener> {
 public:
  Listener(net::CookieStore* cookie_store,
           const RestrictedCookieManager* restricted_cookie_manager,
           const GURL& url,
           const net::SiteForCookies& site_for_cookies,
           const url::Origin& top_frame_origin,
           bool has_storage_access,
           const absl::optional<net::CookiePartitionKey>& cookie_partition_key,
           net::CookieOptions options,
           mojo::PendingRemote<mojom::CookieChangeListener> mojo_listener,
           bool same_party_attribute_enabled)
      : cookie_store_(cookie_store),
        restricted_cookie_manager_(restricted_cookie_manager),
        url_(url),
        site_for_cookies_(site_for_cookies),
        top_frame_origin_(top_frame_origin),
        has_storage_access_(has_storage_access),
        options_(options),
        mojo_listener_(std::move(mojo_listener)),
        same_party_attribute_enabled_(same_party_attribute_enabled) {
    // TODO(pwnall): add a constructor w/options to net::CookieChangeDispatcher.
    cookie_store_subscription_ =
        cookie_store->GetChangeDispatcher().AddCallbackForUrl(
            url, cookie_partition_key,
            base::BindRepeating(
                &Listener::OnCookieChange,
                // Safe because net::CookieChangeDispatcher guarantees that
                // the callback will stop being called immediately after we
                // remove the subscription, and the cookie store lives on
                // the same thread as we do.
                base::Unretained(this)));
  }

  Listener(const Listener&) = delete;
  Listener& operator=(const Listener&) = delete;

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
        net::cookie_util::GetSamePartyStatus(change.cookie, options_,
                                             same_party_attribute_enabled_);

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
    if (!restricted_cookie_manager_->cookie_settings().IsCookieAccessible(
            change.cookie, url_, site_for_cookies_, top_frame_origin_,
            restricted_cookie_manager_->GetCookieSettingOverrides(
                has_storage_access_))) {
      return;
    }

    mojo_listener_->OnCookieChange(change);
  }

  // Expected to outlive |restricted_cookie_manager_| which outlives this.
  raw_ptr<const net::CookieStore> cookie_store_;

  // The CookieChangeDispatcher subscription used by this listener.
  std::unique_ptr<net::CookieChangeSubscription> cookie_store_subscription_;

  // Raw pointer usage is safe because RestrictedCookieManager owns this
  // instance and is guaranteed to outlive it.
  const raw_ptr<const RestrictedCookieManager> restricted_cookie_manager_;

  // The URL whose cookies this listener is interested in.
  const GURL url_;

  // Site context in which we're used; used to determine if a cookie is accessed
  // in a third-party context.
  const net::SiteForCookies site_for_cookies_;

  // Site context in which we're used; used to check content settings.
  const url::Origin top_frame_origin_;

  // Whether the Listener has storage access. Note that if a listener is created
  // from a document that has not called `document.requestStorageAccess()`, and
  // the script later calls `document.requestStorageAccess()` to obtain storage
  // access, this listener's state will not be updated.
  const bool has_storage_access_;

  // CanonicalCookie::IncludeForRequestURL options for this listener's interest.
  const net::CookieOptions options_;

  mojo::Remote<mojom::CookieChangeListener> mojo_listener_;

  bool same_party_attribute_enabled_;

  SEQUENCE_CHECKER(sequence_checker_);
};

RestrictedCookieManager::RestrictedCookieManager(
    const mojom::RestrictedCookieManagerRole role,
    net::CookieStore* cookie_store,
    const CookieSettings& cookie_settings,
    const url::Origin& origin,
    const net::IsolationInfo& isolation_info,
    const net::CookieSettingOverrides& cookie_setting_overrides,
    mojo::PendingRemote<mojom::CookieAccessObserver> cookie_observer,
    net::FirstPartySetMetadata first_party_set_metadata,
    UmaMetricsUpdater* metrics_updater)
    : role_(role),
      cookie_store_(cookie_store),
      cookie_settings_(cookie_settings),
      cookie_setting_overrides_(cookie_setting_overrides),
      origin_(origin),
      isolation_info_(isolation_info),
      cookie_observer_(std::move(cookie_observer)),
      first_party_set_metadata_(std::move(first_party_set_metadata)),
      cookie_partition_key_(net::CookiePartitionKey::FromNetworkIsolationKey(
          isolation_info.network_isolation_key())),
      cookie_partition_key_collection_(
          net::CookiePartitionKeyCollection::FromOptional(
              cookie_partition_key_)),
      same_party_attribute_enabled_(base::FeatureList::IsEnabled(
          net::features::kSamePartyAttributeEnabled)),
      receiver_(this),
      metrics_updater_(metrics_updater),
      cookies_access_timer_(
          FROM_HERE,
          kCookiesAccessedTimeout,
          base::BindRepeating(&RestrictedCookieManager::CallCookiesAccessed,
                              base::Unretained(this))) {
  DCHECK(cookie_store);
  DCHECK(!cookie_setting_overrides_.Has(
      net::CookieSettingOverride::kStorageAccessGrantEligible));
  if (role == mojom::RestrictedCookieManagerRole::SCRIPT) {
      CHECK(origin_.IsSameOriginWith(isolation_info_.frame_origin().value()));
  }
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

void RestrictedCookieManager::OverrideIsolationInfoForTesting(
    const net::IsolationInfo& new_isolation_info) {
  base::RunLoop run_loop;
  isolation_info_ = new_isolation_info;

  ComputeFirstPartySetMetadata(
      origin_, cookie_store_, isolation_info_,
      base::BindOnce(
          &RestrictedCookieManager::OnGotFirstPartySetMetadataForTesting,
          weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()));
  run_loop.Run();
}

void RestrictedCookieManager::OnGotFirstPartySetMetadataForTesting(
    base::OnceClosure done_closure,
    net::FirstPartySetMetadata first_party_set_metadata) {
  first_party_set_metadata_ = std::move(first_party_set_metadata);
  cookie_partition_key_ = net::CookiePartitionKey::FromNetworkIsolationKey(
      isolation_info_.network_isolation_key());
  cookie_partition_key_collection_ =
      net::CookiePartitionKeyCollection::FromOptional(cookie_partition_key_);
  std::move(done_closure).Run();
}

bool RestrictedCookieManager::IsPartitionedCookiesEnabled() const {
  return cookie_partition_key_.has_value();
}

void RestrictedCookieManager::GetAllForUrl(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    bool has_storage_access,
    mojom::CookieManagerGetOptionsPtr options,
    GetAllForUrlCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ValidateAccessToCookiesAt(url, site_for_cookies, top_frame_origin)) {
    std::move(callback).Run({});
    return;
  }

  // TODO(morlovich): Try to validate site_for_cookies as well.

  net::CookieOptions net_options =
      MakeOptionsForGet(role_, url, site_for_cookies, isolation_info_,
                        cookie_settings(), first_party_set_metadata_);
  // TODO(https://crbug.com/977040): remove set_return_excluded_cookies() once
  // removing deprecation warnings.
  net_options.set_return_excluded_cookies();

  cookie_store_->GetCookieListWithOptionsAsync(
      url, net_options, cookie_partition_key_collection_,
      base::BindOnce(&RestrictedCookieManager::CookieListToGetAllForUrlCallback,
                     weak_ptr_factory_.GetWeakPtr(), url, site_for_cookies,
                     top_frame_origin, has_storage_access, net_options,
                     std::move(options), std::move(callback)));
}

void RestrictedCookieManager::CookieListToGetAllForUrlCallback(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    bool has_storage_access,
    const net::CookieOptions& net_options,
    mojom::CookieManagerGetOptionsPtr options,
    GetAllForUrlCallback callback,
    const net::CookieAccessResultList& cookie_list,
    const net::CookieAccessResultList& excluded_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  net::CookieAccessResultList maybe_included_cookies = cookie_list;
  net::CookieAccessResultList excluded_cookies = excluded_list;
  cookie_settings().AnnotateAndMoveUserBlockedCookies(
      url, site_for_cookies, &top_frame_origin, first_party_set_metadata_,
      GetCookieSettingOverrides(has_storage_access), maybe_included_cookies,
      excluded_cookies);

  std::vector<net::CookieWithAccessResult> result;
  std::vector<mojom::CookieOrLineWithAccessResultPtr>
      on_cookies_accessed_result;

  CookieAccesses* cookie_accesses =
      GetCookieAccessesForURLAndSite(url, site_for_cookies);

  if (!maybe_included_cookies.empty())
    result.reserve(maybe_included_cookies.size());
  mojom::CookieMatchType match_type = options->match_type;
  const std::string& match_name = options->name;
  for (const net::CookieWithAccessResult& cookie_item :
       maybe_included_cookies) {
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

    if (access_result.status.IsInclude()) {
      result.push_back(cookie_item);
    }
  }

  auto notify_observer = [&]() {
    if (cookie_observer_ && !on_cookies_accessed_result.empty()) {
      OnCookiesAccessed(mojom::CookieAccessDetails::New(
          mojom::CookieAccessDetails::Type::kRead, url, site_for_cookies,
          std::move(on_cookies_accessed_result), absl::nullopt));
    }
  };

  if (!maybe_included_cookies.empty() && IsPartitionedCookiesEnabled()) {
    UMA_HISTOGRAM_COUNTS_100(
        "Net.RestrictedCookieManager.PartitionedCookiesInScript",
        base::ranges::count_if(result,
                               [](const net::CookieWithAccessResult& c) {
                                 return c.cookie.IsPartitioned();
                               }));
  }

  std::move(callback).Run(result);

  // TODO(https://crbug.com/977040): Stop reporting accesses of cookies with
  // warning reasons once samesite tightening up is rolled out.
  for (const auto& cookie_and_access_result : excluded_cookies) {
    if (!cookie_and_access_result.access_result.status.ShouldWarn() &&
        !cookie_and_access_result.access_result.status
             .ExcludedByUserPreferences()) {
      continue;
    }

    // Skip sending a notification about this cookie access?
    if (SkipAccessNotificationForCookieItem(cookie_accesses,
                                            cookie_and_access_result)) {
      continue;
    }

    on_cookies_accessed_result.push_back(
        mojom::CookieOrLineWithAccessResult::New(
            mojom::CookieOrLine::NewCookie(cookie_and_access_result.cookie),
            cookie_and_access_result.access_result));
  }

  for (auto& cookie : result) {
    // Skip sending a notification about this cookie access?
    if (SkipAccessNotificationForCookieItem(cookie_accesses, cookie)) {
      continue;
    }

    on_cookies_accessed_result.push_back(
        mojom::CookieOrLineWithAccessResult::New(
            mojom::CookieOrLine::NewCookie(cookie.cookie),
            cookie.access_result));
  }

  notify_observer();
}

void RestrictedCookieManager::SetCanonicalCookie(
    const net::CanonicalCookie& cookie,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    bool has_storage_access,
    net::CookieInclusionStatus status,
    SetCanonicalCookieCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Don't allow a status that has an exclusion reason as they should have
  // already been taken care of on the renderer side.
  if (!status.IsInclude()) {
    mojo::ReportBadMessage(
        "RestrictedCookieManager: unexpected cookie inclusion status");
    std::move(callback).Run(false);
    return;
  }
  if (!ValidateAccessToCookiesAt(url, site_for_cookies, top_frame_origin,
                                 &cookie)) {
    std::move(callback).Run(false);
    return;
  }

  // TODO(morlovich): Try to validate site_for_cookies as well.
  bool blocked = !cookie_settings_->IsCookieAccessible(
      cookie, url, site_for_cookies, top_frame_origin,
      GetCookieSettingOverrides(has_storage_access));

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
      OnCookiesAccessed(mojom::CookieAccessDetails::New(
          mojom::CookieAccessDetails::Type::kChange, url, site_for_cookies,
          std::move(result_with_access_result), absl::nullopt));
    }
    std::move(callback).Run(false);
    return;
  }

  // TODO(pwnall): Validate the CanonicalCookie fields.

  // Update the creation and last access times.
  // Note: This used to be a call to NowFromSystemTime, but this caused
  // inconsistency with the expiration date, which was capped checking
  // against Now. If any issues crop up related to this change please
  // contact the owners of http://crbug.com/1335859.
  base::Time now = base::Time::Now();
  // TODO(http://crbug.com/1024053): Log metrics
  const GURL& origin_url = origin_.GetURL();
  net::CookieSourceScheme source_scheme =
      GURL::SchemeIsCryptographic(origin_.scheme())
          ? net::CookieSourceScheme::kSecure
          : net::CookieSourceScheme::kNonSecure;

  // If the renderer's cookie has a partition key that was not created using
  // CookiePartitionKey::FromScript, then the cookie's partition key should be
  // equal to RestrictedCookieManager's partition key.
  absl::optional<net::CookiePartitionKey> cookie_partition_key =
      cookie.PartitionKey();

  // If the `cookie_partition_key_` has a nonce then force all cookie writes to
  // be in the nonce based partition even if the cookie was not set with the
  // Partitioned attribute.
  if (net::CookiePartitionKey::HasNonce(cookie_partition_key_)) {
    cookie_partition_key = cookie_partition_key_;
  }
  if (cookie_partition_key) {
    // RestrictedCookieManager having a null partition key strictly implies the
    // feature is disabled. If that is the case, we treat the cookie as
    // unpartitioned.
    if (!cookie_partition_key_) {
      cookie_partition_key = absl::nullopt;
    } else {
      bool cookie_partition_key_ok =
          cookie_partition_key->from_script() ||
          cookie_partition_key.value() == cookie_partition_key_.value();
      UMA_HISTOGRAM_BOOLEAN("Net.RestrictedCookieManager.CookiePartitionKeyOK",
                            cookie_partition_key_ok);
      if (!cookie_partition_key_ok) {
        mojo::ReportBadMessage(
            "RestrictedCookieManager: unexpected cookie partition key");
        std::move(callback).Run(false);
        return;
      }
      if (cookie_partition_key->from_script()) {
        cookie_partition_key = cookie_partition_key_;
      }
    }
  }

  if (IsPartitionedCookiesEnabled()) {
    UMA_HISTOGRAM_BOOLEAN("Net.RestrictedCookieManager.SetPartitionedCookie",
                          cookie_partition_key.has_value());
  }

  std::unique_ptr<net::CanonicalCookie> sanitized_cookie =
      net::CanonicalCookie::FromStorage(
          cookie.Name(), cookie.Value(), cookie.Domain(), cookie.Path(), now,
          cookie.ExpiryDate(), now, now, cookie.IsSecure(), cookie.IsHttpOnly(),
          cookie.SameSite(), cookie.Priority(), cookie.IsSameParty(),
          cookie_partition_key, source_scheme, origin_.port());
  DCHECK(sanitized_cookie);
  // FromStorage() uses a less strict version of IsCanonical(), we need to check
  // the stricter version as well here.
  if (!sanitized_cookie->IsCanonical()) {
    std::move(callback).Run(false);
    return;
  }

  net::CanonicalCookie cookie_copy = *sanitized_cookie;
  net::CookieOptions options =
      MakeOptionsForSet(role_, url, site_for_cookies, isolation_info_,
                        cookie_settings(), first_party_set_metadata_);

  net::CookieAccessResult cookie_access_result(status);
  cookie_store_->SetCanonicalCookieAsync(
      std::move(sanitized_cookie), origin_url, options,
      base::BindOnce(&RestrictedCookieManager::SetCanonicalCookieResult,
                     weak_ptr_factory_.GetWeakPtr(), url, site_for_cookies,
                     cookie_copy, options, std::move(callback)),
      cookie_access_result);
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
      OnCookiesAccessed(mojom::CookieAccessDetails::New(
          mojom::CookieAccessDetails::Type::kChange, url, site_for_cookies,
          std::move(notify), absl::nullopt));
    }
  }
  std::move(user_callback).Run(access_result.status.IsInclude());
}

void RestrictedCookieManager::AddChangeListener(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    bool has_storage_access,
    mojo::PendingRemote<mojom::CookieChangeListener> mojo_listener,
    AddChangeListenerCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!ValidateAccessToCookiesAt(url, site_for_cookies, top_frame_origin)) {
    std::move(callback).Run();
    return;
  }

  net::CookieOptions net_options =
      MakeOptionsForGet(role_, url, site_for_cookies, isolation_info_,
                        cookie_settings(), first_party_set_metadata_);
  auto listener = std::make_unique<Listener>(
      cookie_store_, this, url, site_for_cookies, top_frame_origin,
      has_storage_access, cookie_partition_key_, net_options,
      std::move(mojo_listener), same_party_attribute_enabled_);

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
    bool has_storage_access,
    const std::string& cookie,
    SetCookieFromStringCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool site_for_cookies_ok =
      BoundSiteForCookies().IsEquivalent(site_for_cookies);
  bool top_frame_origin_ok = top_frame_origin == BoundTopFrameOrigin();

  std::move(callback).Run(site_for_cookies_ok, top_frame_origin_ok);
  callback = base::DoNothing();

  net::CookieInclusionStatus status;
  std::unique_ptr<net::CanonicalCookie> parsed_cookie =
      net::CanonicalCookie::Create(url, cookie, base::Time::Now(),
                                   absl::nullopt /* server_time */,
                                   cookie_partition_key_, &status);
  if (!parsed_cookie) {
    if (cookie_observer_) {
      std::vector<network::mojom::CookieOrLineWithAccessResultPtr>
          result_with_access_result;
      result_with_access_result.push_back(
          mojom::CookieOrLineWithAccessResult::New(
              mojom::CookieOrLine::NewCookieString(cookie),
              net::CookieAccessResult(status)));
      OnCookiesAccessed(mojom::CookieAccessDetails::New(
          mojom::CookieAccessDetails::Type::kChange, url, site_for_cookies,
          std::move(result_with_access_result), absl::nullopt));
    }
    std::move(callback).Run(site_for_cookies_ok, top_frame_origin_ok);
    return;
  }

  // Further checks (origin_, settings), as well as logging done by
  // SetCanonicalCookie()
  SetCanonicalCookie(
      *parsed_cookie, url, site_for_cookies, top_frame_origin,
      has_storage_access, status,
      base::BindOnce([](base::OnceClosure closure,
                        bool success) { std::move(closure).Run(); },
                     // Although these values are being called outside
                     // ValidateAccessToCookiesAt, the checks done in that
                     // method are called shortly after synchronously.
                     base::BindOnce(std::move(callback), site_for_cookies_ok,
                                    top_frame_origin_ok)));
}

void RestrictedCookieManager::GetCookiesString(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    bool has_storage_access,
    GetCookiesStringCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Checks done by GetAllForUrl

  if (metrics_updater_) {
    metrics_updater_->OnGetCookiesString();
  }

  // Match everything.
  auto match_options = mojom::CookieManagerGetOptions::New();
  match_options->name = "";
  match_options->match_type = mojom::CookieMatchType::STARTS_WITH;
  GetAllForUrl(url, site_for_cookies, top_frame_origin, has_storage_access,
               std::move(match_options),
               base::BindOnce([](const std::vector<net::CookieWithAccessResult>&
                                     cookies) {
                 return net::CanonicalCookie::BuildCookieLine(cookies);
               }).Then(std::move(callback)));
}

void RestrictedCookieManager::CookiesEnabledFor(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    bool has_storage_access,
    CookiesEnabledForCallback callback) {
  if (!ValidateAccessToCookiesAt(url, site_for_cookies, top_frame_origin)) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(cookie_settings_->IsFullCookieAccessAllowed(
      url, site_for_cookies, top_frame_origin,
      GetCookieSettingOverrides(has_storage_access)));
}

void RestrictedCookieManager::InstallReceiver(
    mojo::PendingReceiver<mojom::RestrictedCookieManager> pending_receiver,
    base::OnceClosure on_disconnect_callback) {
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(pending_receiver));
  receiver_.set_disconnect_handler(std::move(on_disconnect_callback));
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

  bool site_for_cookies_ok =
      BoundSiteForCookies().IsEquivalent(site_for_cookies);
  DCHECK(site_for_cookies_ok)
      << "site_for_cookies from renderer='" << site_for_cookies.ToDebugString()
      << "' from browser='" << BoundSiteForCookies().ToDebugString() << "';";

  bool top_frame_origin_ok = (top_frame_origin == BoundTopFrameOrigin());
  DCHECK(top_frame_origin_ok)
      << "top_frame_origin from renderer='" << top_frame_origin
      << "' from browser='" << BoundTopFrameOrigin() << "';";

  UMA_HISTOGRAM_BOOLEAN("Net.RestrictedCookieManager.SiteForCookiesOK",
                        site_for_cookies_ok);
  UMA_HISTOGRAM_BOOLEAN("Net.RestrictedCookieManager.TopFrameOriginOK",
                        top_frame_origin_ok);

  // Don't allow setting cookies on other domains. See crbug.com/996786.
  if (cookie_being_set && !cookie_being_set->IsDomainMatch(url.host())) {
    mojo::ReportBadMessage("Setting cookies on other domains is disallowed.");
    return false;
  }

  if (origin_.IsSameOriginWith(url))
    return true;

  mojo::ReportBadMessage("Incorrect url origin");
  return false;
}

net::CookieSettingOverrides RestrictedCookieManager::GetCookieSettingOverrides(
    bool has_storage_access) const {
  net::CookieSettingOverrides overrides = cookie_setting_overrides_;
  if (has_storage_access) {
    overrides.Put(net::CookieSettingOverride::kStorageAccessGrantEligible);
  }
  return overrides;
}

void RestrictedCookieManager::OnCookiesAccessed(
    mojom::CookieAccessDetailsPtr details) {
  cookie_access_details_.push_back(std::move(details));
  if (base::FeatureList::IsEnabled(features::kLessChattyNetworkService)) {
    if (!cookies_access_timer_.IsRunning()) {
      cookies_access_timer_.Reset();
    }
  } else {
    cookie_observer_->OnCookiesAccessed(std::move(cookie_access_details_));
  }
}

void RestrictedCookieManager::CallCookiesAccessed() {
  DCHECK(!cookie_access_details_.empty());
  cookie_observer_->OnCookiesAccessed(std::move(cookie_access_details_));
}

}  // namespace network
