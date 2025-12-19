// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_service_impl.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/process.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/features.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "net/device_bound_sessions/jwk_utils.h"
#include "net/device_bound_sessions/registration_request_param.h"
#include "net/device_bound_sessions/session_store.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace net::device_bound_sessions {

namespace {

// Parameters for the signing quota. We currently allow 6 signings in 9
// minutes per site. Reasoning:
// 1. This allows sites to refresh on average every 5 minutes, accounting for
//    proactive refreshes 2 minutes before expiry, and with some error tolerance
//    (e.g. a failed refresh or user cookie clearing) and tolerance for new
//    registration signings.
// 2. It's 6:9 instead of 2:3 to allow small bursts of login activity + new
//    registrations.
// 3. The spec notes that user agents should include quotas on registration
//    attempts to prevent identity linking for federated sessions.
constexpr size_t kSigningQuota = 6;
constexpr base::TimeDelta kSigningQuotaInterval = base::Minutes(9);

// The delay between when the session service is loaded and the garbage
// collection is started. This is delayed to not slow down the startup of the
// browser.
constexpr base::TimeDelta kGarbageCollectionDelay = base::Minutes(2);

bool SessionMatchesFilter(
    const SchemefulSite& site,
    const Session& session,
    std::optional<base::Time> created_after_time,
    std::optional<base::Time> created_before_time,
    base::RepeatingCallback<bool(const url::Origin&, const net::SchemefulSite&)>
        origin_and_site_matcher) {
  if (created_before_time && *created_before_time < session.creation_date()) {
    return false;
  }

  if (created_after_time && *created_after_time > session.creation_date()) {
    return false;
  }

  if (!origin_and_site_matcher.is_null() &&
      !origin_and_site_matcher.Run(session.origin(), site)) {
    return false;
  }

  return true;
}

class DebugHeaderBuilder {
 public:
  void AddSkippedSession(SessionKey key, RefreshResult result) {
    structured_headers::Item item;
    switch (result) {
      case RefreshResult::kRefreshed:
      case RefreshResult::kFatalError:
        return;
      case RefreshResult::kInitializedService:
        NOTREACHED();
      case RefreshResult::kUnreachable:
        item = structured_headers::Item("unreachable",
                                        structured_headers::Item::kTokenType);
        break;
      case RefreshResult::kServerError:
        item = structured_headers::Item("server_error",
                                        structured_headers::Item::kTokenType);
        break;
      case RefreshResult::kRefreshQuotaExceeded:
      case RefreshResult::kSigningQuotaExceeded:
        item = structured_headers::Item("quota_exceeded",
                                        structured_headers::Item::kTokenType);
        break;
    }

    structured_headers::Parameters params = {
        {"session_identifier", structured_headers::Item(key.id.value())}};
    skipped_sessions_.emplace_back(std::move(item), std::move(params));
  }

  std::optional<std::string> Build() {
    if (skipped_sessions_.empty()) {
      return std::nullopt;
    }

    return structured_headers::SerializeList(std::move(skipped_sessions_));
  }

 private:
  structured_headers::List skipped_sessions_;
};

bool IsProactiveRefreshCandidate(
    Session& existing_session,
    const Session& new_session,
    const CookieAndLineAccessResultList& maybe_stored_cookies) {
  // Get the shortest lifetime of a bound cookie set by the current
  // refresh request. This assumes:
  // 1. The current refresh sets all bound cookies
  // 2. The proactive refresh would have set the same lifetimes
  // These assumptions are good enough for histogram logging, but likely
  // not true for all sites.
  base::Time current_time = base::Time::Now();
  base::TimeDelta minimum_lifetime = base::TimeDelta::Max();
  for (const CookieCraving& cookie_craving : new_session.cookies()) {
    for (const CookieAndLineWithAccessResult& cookie_and_line :
         maybe_stored_cookies) {
      if (cookie_and_line.cookie.has_value() &&
          cookie_craving.IsSatisfiedBy(cookie_and_line.cookie.value())) {
        minimum_lifetime =
            std::min(minimum_lifetime,
                     cookie_and_line.cookie->ExpiryDate() - current_time);
      }
    }
  }

  base::UmaHistogramLongTimes100(
      "Net.DeviceBoundSessions.MinimumBoundCookieLifetime", minimum_lifetime);

  std::optional<base::Time> last_proactive_refresh_opportunity =
      existing_session.TakeLastProactiveRefreshOpportunity();

  if (!last_proactive_refresh_opportunity.has_value()) {
    return false;
  }

  return minimum_lifetime >= current_time - *last_proactive_refresh_opportunity;
}

void LogProactiveRefreshAttempt(
    SessionServiceImpl::ProactiveRefreshAttempt attempt) {
  base::UmaHistogramEnumeration(
      "Net.DeviceBoundSessions.ProactiveRefreshAttempt", attempt);
}

}  // namespace

DeferredURLRequest::DeferredURLRequest(
    SessionService::RefreshCompleteCallback callback)
    : callback(std::move(callback)) {}

DeferredURLRequest::DeferredURLRequest(DeferredURLRequest&& other) noexcept =
    default;

DeferredURLRequest& DeferredURLRequest::operator=(
    DeferredURLRequest&& other) noexcept = default;

DeferredURLRequest::~DeferredURLRequest() = default;

SessionServiceImpl::SessionServiceImpl(
    unexportable_keys::UnexportableKeyService& key_service,
    const URLRequestContext* request_context,
    SessionStore* store)
    : pending_initialization_(!!store),
      key_service_(key_service),
      context_(request_context),
      session_store_(store) {
  ignore_refresh_quota_ = !features::kDeviceBoundSessionsRefreshQuota.Get();
  CHECK(context_);
}

SessionServiceImpl::~SessionServiceImpl() = default;

void SessionServiceImpl::LoadSessionsAsync() {
  if (!session_store_) {
    return;
  }
  session_store_->LoadSessions(base::BindOnce(
      &SessionServiceImpl::OnLoadSessionsComplete, weak_factory_.GetWeakPtr()));

  // Schedule a task for original profiles to obtain all keys that were
  // created for this profile in the past, including all OTR profiles.
  if (base::FeatureList::IsEnabled(
          unexportable_keys::kUnexportableKeyDeletion)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SessionServiceImpl::StartGarbageCollection,
                       weak_factory_.GetWeakPtr()),
        kGarbageCollectionDelay);
  }
}

void SessionServiceImpl::RegisterBoundSession(
    OnAccessCallback on_access_callback,
    RegistrationFetcherParam registration_params,
    const IsolationInfo& isolation_info,
    const NetLogWithSource& net_log,
    const std::optional<url::Origin>& original_request_initiator) {
  if (registration_params.provider_session_id().has_value()) {
    if (!base::FeatureList::IsEnabled(
            features::kDeviceBoundSessionsFederatedRegistration)) {
      // Simply ignore headers with a provider_session_id if the flag
      // isn't enabled.
      return;
    }

    // RegistrationFetcherParam::ParseItem ensures that all three of
    // these are present.
    GURL provider_url = *registration_params.provider_url();
    Session::Id provider_session_id =
        *registration_params.provider_session_id();
    std::string provider_key_thumbprint = *registration_params.provider_key();
    GetFederatedProviderSessionIfValid(
        std::move(provider_url), std::move(provider_session_id),
        std::move(provider_key_thumbprint), on_access_callback,
        base::BindOnce(&SessionServiceImpl::RegisterBoundSessionInternal,
                       weak_factory_.GetWeakPtr(), on_access_callback,
                       std::move(registration_params),
                       std::move(isolation_info), std::move(net_log),
                       std::move(original_request_initiator)));
    return;
  }

  RegisterBoundSessionInternal(std::move(on_access_callback),
                               std::move(registration_params),
                               std::move(isolation_info), std::move(net_log),
                               std::move(original_request_initiator),
                               /*federated_provider_session=*/nullptr);
}

void SessionServiceImpl::RegisterBoundSessionInternal(
    OnAccessCallback on_access_callback,
    RegistrationFetcherParam registration_params,
    const IsolationInfo& isolation_info,
    const NetLogWithSource& net_log,
    const std::optional<url::Origin>& original_request_initiator,
    base::expected<Session*, SessionError> federated_provider_session) {
  bool is_google_subdomain_for_histograms = IsSubdomainOf(
      registration_params.registration_endpoint().host(), "google.com");

  // A federated session was attempted but had an error.
  if (!federated_provider_session.has_value()) {
    OnRegistrationComplete(
        std::move(on_access_callback), is_google_subdomain_for_histograms,
        /*is_federated_registration_for_histograms=*/true,
        /*fetcher=*/nullptr,
        RegistrationResult(std::move(federated_provider_session.error())));
    return;
  }

  if (*federated_provider_session) {
    SessionKey provider_session_key{
        SchemefulSite(*registration_params.provider_url()),
        *registration_params.provider_session_id()};
    NotifySessionAccess(on_access_callback, SessionAccess::AccessType::kUpdate,
                        provider_session_key, **federated_provider_session);
  }

  net::NetLogSource net_log_source_for_registration = net::NetLogSource(
      net::NetLogSourceType::URL_REQUEST, net::NetLog::Get()->NextID());
  net_log.AddEventReferencingSource(
      net::NetLogEventType::DBSC_REGISTRATION_REQUEST,
      net_log_source_for_registration);

  const auto supported_algos = registration_params.supported_algos();
  std::optional<GURL> provider_url = registration_params.provider_url();
  RegistrationRequestParam request_params =
      RegistrationRequestParam::CreateForRegistration(
          std::move(registration_params));
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          request_params, *this, key_service_.get(), context_.get(),
          isolation_info, net_log_source_for_registration,
          original_request_initiator);
  RegistrationFetcher* fetcher_raw = fetcher.get();
  registration_fetchers_.insert(std::move(fetcher));

  auto callback = base::BindOnce(
      &SessionServiceImpl::OnRegistrationComplete, weak_factory_.GetWeakPtr(),
      std::move(on_access_callback), is_google_subdomain_for_histograms,
      /*is_federated_registration_for_histograms=*/federated_provider_session !=
          nullptr);
  if (*federated_provider_session) {
    fetcher_raw->StartFetchWithFederatedKey(
        request_params, *(*federated_provider_session)->unexportable_key_id(),
        *provider_url, std::move(callback));
    // `fetcher_raw` may be deleted.
  } else {
    fetcher_raw->StartCreateTokenAndFetch(request_params, supported_algos,
                                          std::move(callback));
    // `fetcher_raw` may be deleted.
  }
}

void SessionServiceImpl::GetFederatedProviderSessionIfValid(
    GURL provider_url,
    Session::Id provider_session_id,
    std::string provider_key_thumbprint,
    OnAccessCallback on_access_callback,
    base::OnceCallback<void(base::expected<Session*, SessionError>)> callback) {
  // This is a federated session registration.
  if (!provider_url.is_valid() || url::Origin::Create(provider_url).opaque()) {
    std::move(callback).Run(base::unexpected(
        SessionError(SessionError::kInvalidFederatedSessionUrl)));
    return;
  }

  SessionKey provider_session_key{SchemefulSite(provider_url),
                                  provider_session_id};
  Session* provider_session = GetSession(provider_session_key);
  if (!provider_session) {
    std::move(callback).Run(base::unexpected(SessionError(
        SessionError::kInvalidFederatedSessionProviderSessionMissing)));
    return;
  }

  if (url::Origin::Create(provider_url) != provider_session->origin()) {
    std::move(callback).Run(base::unexpected(SessionError(
        SessionError::kInvalidFederatedSessionWrongProviderOrigin)));
    return;
  }

  if (!provider_session->unexportable_key_id().has_value()) {
    RestoreSessionKey(
        provider_session_key, on_access_callback,
        base::BindOnce(&SessionServiceImpl::CheckFederatedProviderKey,
                       weak_factory_.GetWeakPtr(), provider_session_key,
                       std::move(provider_key_thumbprint),
                       std::move(callback)));
    return;
  }

  CheckFederatedProviderKey(
      std::move(provider_session_key), std::move(provider_key_thumbprint),
      std::move(callback), *provider_session->unexportable_key_id());
}

void SessionServiceImpl::CheckFederatedProviderKey(
    SessionKey provider_session_key,
    std::string provider_key_thumbprint,
    base::OnceCallback<void(base::expected<Session*, SessionError>)> callback,
    std::optional<unexportable_keys::UnexportableKeyId> provider_key) {
  if (!provider_key) {
    // Failed to restore provider key.
    std::move(callback).Run(base::unexpected(SessionError(
        SessionError::kInvalidFederatedSessionProviderFailedToRestoreKey)));
    return;
  }

  Session* provider_session = GetSession(provider_session_key);
  if (!provider_session) {
    // Provider session not found, fail the registration.
    std::move(callback).Run(base::unexpected(SessionError(
        SessionError::kInvalidFederatedSessionProviderSessionMissing)));
    return;
  }

  unexportable_keys::ServiceErrorOr<
      crypto::SignatureVerifier::SignatureAlgorithm>
      algorithm =
          key_service_->GetAlgorithm(*provider_session->unexportable_key_id());
  if (!algorithm.has_value()) {
    std::move(callback).Run(
        base::unexpected(SessionError(SessionError::kInvalidFederatedKey)));
    return;
  }

  unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> pub_key =
      key_service_->GetSubjectPublicKeyInfo(
          *provider_session->unexportable_key_id());
  if (!pub_key.has_value()) {
    std::move(callback).Run(
        base::unexpected(SessionError(SessionError::kInvalidFederatedKey)));
    return;
  }

  std::string thumbprint = CreateJwkThumbprint(*algorithm, *pub_key);
  if (thumbprint != provider_key_thumbprint) {
    std::move(callback).Run(base::unexpected(
        SessionError(SessionError::kFederatedKeyThumbprintMismatch)));
    return;
  }

  std::move(callback).Run(provider_session);
}

SessionServiceImpl::Observer::Observer(
    const GURL& url,
    base::RepeatingCallback<void(const SessionAccess&)> callback)
    : url(url), callback(callback) {}
SessionServiceImpl::Observer::~Observer() = default;

void SessionServiceImpl::OnLoadSessionsComplete(
    SessionStore::SessionsMap sessions) {
  unpartitioned_sessions_.merge(sessions);
  pending_initialization_ = false;

  std::vector<base::OnceClosure> queued_operations =
      std::move(queued_operations_);
  for (base::OnceClosure& closure : queued_operations) {
    std::move(closure).Run();
  }

  base::UmaHistogramCounts1000(
      "Net.DeviceBoundSessions.RequestsDeferredForInitialization",
      requests_before_initialization_);
}

void SessionServiceImpl::StartGarbageCollection() {
  key_service_->GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      base::BindOnce(&SessionServiceImpl::OnGetAllKeysForGarbageCollection,
                     weak_factory_.GetWeakPtr()));
}

void SessionServiceImpl::OnGetAllKeysForGarbageCollection(
    unexportable_keys::ServiceErrorOr<
        std::vector<unexportable_keys::UnexportableKeyId>>
        all_key_ids_or_error) {
  if (!all_key_ids_or_error.has_value() || all_key_ids_or_error->empty()) {
    return;
  }

  base::OnceClosure do_garbage_collection = base::BindOnce(
      &SessionServiceImpl::DoGarbageCollection, weak_factory_.GetWeakPtr(),
      std::move(*all_key_ids_or_error));

  if (pending_initialization_) {
    queued_operations_.push_back(std::move(do_garbage_collection));
  } else {
    std::move(do_garbage_collection).Run();
  }
}

void SessionServiceImpl::DoGarbageCollection(
    std::vector<unexportable_keys::UnexportableKeyId> all_key_ids) {
  const size_t key_count = all_key_ids.size();
  base::UmaHistogramCounts100(
      "Crypto.UnexportableKeys.GarbageCollection.DeviceBoundSessions."
      "TotalKeyCount",
      key_count);

  absl::flat_hash_set<unexportable_keys::UnexportableKeyId> known_key_ids;
  for (const auto& [_, session] : unpartitioned_sessions_) {
    if (Session::KeyIdOrError key_id_or_error = session->unexportable_key_id();
        key_id_or_error.has_value()) {
      known_key_ids.insert(*key_id_or_error);
    }
  }

  // Remove all keys that are still used, or were created after the process
  // started.
  std::erase_if(all_key_ids, [&](unexportable_keys::UnexportableKeyId key_id) {
    return known_key_ids.contains(key_id) ||
           key_service_->GetCreationTime(key_id).value_or(base::Time::Now()) >=
               base::Process::Current().CreationTime();
  });

  base::UmaHistogramCounts100(
      "Crypto.UnexportableKeys.GarbageCollection.DeviceBoundSessions."
      "UsedKeyCount",
      key_count - all_key_ids.size());

  base::UmaHistogramCounts100(
      "Crypto.UnexportableKeys.GarbageCollection.DeviceBoundSessions."
      "ObsoleteKeyCount",
      all_key_ids.size());

  const auto barrier_callback =
      base::BarrierCallback<unexportable_keys::ServiceErrorOr<void>>(
          all_key_ids.size(),
          base::BindOnce([](std::vector<unexportable_keys::ServiceErrorOr<void>>
                                results) {
            base::UmaHistogramCounts100(
                "Crypto.UnexportableKeys.GarbageCollection.DeviceBoundSessions."
                "ObsoleteKeyDeletionCount",
                std::ranges::count_if(
                    results, [](auto result) { return result.has_value(); }));
          }));

  // Delete all remaining keys.
  std::ranges::for_each(
      all_key_ids, [&](unexportable_keys::UnexportableKeyId unknown_key_id) {
        key_service_->DeleteKeySlowlyAsync(
            unknown_key_id,
            unexportable_keys::BackgroundTaskPriority::kBestEffort,
            barrier_callback);
      });
}

void SessionServiceImpl::OnRegistrationComplete(
    OnAccessCallback on_access_callback,
    bool is_google_subdomain_for_histograms,
    bool is_federated_registration_for_histograms,
    RegistrationFetcher* fetcher,
    RegistrationResult registration_result) {
  if (is_google_subdomain_for_histograms) {
    base::UmaHistogramBoolean(
        "Net.DeviceBoundSessions.GoogleRegistrationIsFromStandard", true);
  }
  SessionError::ErrorType result = OnRegistrationCompleteInternal(
      std::move(on_access_callback), fetcher, std::move(registration_result));
  base::UmaHistogramEnumeration("Net.DeviceBoundSessions.RegistrationResult",
                                result);
  if (is_federated_registration_for_histograms) {
    base::UmaHistogramEnumeration(
        "Net.DeviceBoundSessions.RegistrationResult.Federated", result);
  } else {
    base::UmaHistogramEnumeration(
        "Net.DeviceBoundSessions.RegistrationResult.Standalone", result);
  }
}

std::ranges::subrange<SessionServiceImpl::SessionsMap::iterator>
SessionServiceImpl::GetSessionsForSite(const SchemefulSite& site) {
  const auto now = base::Time::Now();
  // Session keys are sorted by site, then identifier. So the first
  // element not less than (`site`, "") is the first session for this
  // site.
  auto it =
      unpartitioned_sessions_.lower_bound(SessionKey{site, Session::Id("")});
  while (it != unpartitioned_sessions_.end() && it->first.site == site) {
    auto curit = it;
    ++it;

    if (now >= curit->second->expiry_date()) {
      // Since this deletion is not due to a request, we do not need to
      // provide a per-request callback here.
      DeleteSessionAndNotifyInternal(DeletionReason::kExpired, curit,
                                     base::NullCallback());
    } else {
      curit->second->RecordAccess();
    }
  }

  return std::ranges::subrange<SessionsMap::iterator>(
      unpartitioned_sessions_.lower_bound(SessionKey{site, Session::Id("")}),
      it);
}

std::optional<SessionService::DeferralParams> SessionServiceImpl::ShouldDefer(
    DbscRequest& request,
    HttpRequestHeaders* extra_headers,
    const FirstPartySetMetadata& first_party_set_metadata) {
  if (pending_initialization_) {
    return DeferralParams();
  }

  if (request.device_bound_session_usage() < SessionUsage::kNoUsage) {
    request.set_device_bound_session_usage(SessionUsage::kNoUsage);
  }

  SchemefulSite site(request.url());
  DebugHeaderBuilder debug_header_builder;
  const base::flat_map<SessionKey, RefreshResult>& previous_deferrals =
      request.device_bound_session_deferrals();
  for (const auto& [_, session] : GetSessionsForSite(site)) {
    if (!session->IsInScope(request)) {
      continue;
    }

    SessionKey session_key{site, session->id()};
    base::TimeDelta minimum_lifetime =
        session->MinimumBoundCookieLifetime(request, first_party_set_metadata);
    if (minimum_lifetime.is_zero()) {
      auto previous_deferrals_it = previous_deferrals.find(session_key);
      if (previous_deferrals_it != previous_deferrals.end()) {
        debug_header_builder.AddSkippedSession(previous_deferrals_it->first,
                                               previous_deferrals_it->second);
        continue;
      }

      NotifySessionAccess(request.device_bound_session_access_callback(),
                          SessionAccess::AccessType::kUpdate, session_key,
                          *session);
      return DeferralParams(session->id());
    }

    MaybeStartProactiveRefresh(request.device_bound_session_access_callback(),
                               request, session_key, minimum_lifetime);
  }

  std::optional<std::string> debug_header = debug_header_builder.Build();
  if (debug_header.has_value()) {
    extra_headers->SetHeader("Secure-Session-Skipped", *debug_header);
  }

  return std::nullopt;
}

void SessionServiceImpl::DeferRequestForRefresh(
    DbscRequest& request,
    DeferralParams deferral,
    RefreshCompleteCallback callback) {
  CHECK(callback);

  if (deferral.is_pending_initialization) {
    CHECK(pending_initialization_);
    requests_before_initialization_++;
    // Due to the need to recompute `first_party_set_metadata`, we always
    // restart the request after initialization completes.
    queued_operations_.push_back(base::BindOnce(
        std::move(callback), RefreshResult::kInitializedService));
    return;
  }

  SessionKey session_key{SchemefulSite(request.url()), *deferral.session_id};
  // For the first deferring request, create a new vector and add the request.
  auto [it, inserted] = deferred_requests_.try_emplace(session_key);
  // Add the request callback to the deferred list.
  it->second.emplace_back(std::move(callback));

  auto* session = GetSession(session_key);
  CHECK(session, base::NotFatalUntil::M147);
  // TODO(crbug.com/417770933): Remove this block.
  if (!session) {
    // If we can't find the session, clear the `session_key` in the map
    // and continue all related requests. We can call this a fatal error
    // because the session has already been deleted.
    UnblockDeferredRequests(session_key, RefreshResult::kFatalError);
    return;
  }
  // Notify the request that it has been deferred for refreshed cookies.
  NotifySessionAccess(request.device_bound_session_access_callback(),
                      SessionAccess::AccessType::kUpdate, session_key,
                      *session);
  if (!inserted) {
    return;
  }
  if (proactive_requests_.find(session_key) != proactive_requests_.end()) {
    return;
  }

  if (RefreshQuotaExceeded(session_key.site)) {
    UnblockDeferredRequests(session_key, RefreshResult::kRefreshQuotaExceeded);
    return;
  }

  if (session->ShouldBackoff()) {
    UnblockDeferredRequests(session_key, RefreshResult::kUnreachable);
    return;
  }

  const Session::KeyIdOrError& key_id = session->unexportable_key_id();
  if (!key_id.has_value()) {
    if (key_id.error() == unexportable_keys::ServiceError::kKeyNotReady) {
      RestoreSessionKey(
          session_key, request.device_bound_session_access_callback(),
          base::BindOnce(&SessionServiceImpl::RefreshSessionInternal,
                         weak_factory_.GetWeakPtr(),
                         RefreshTrigger::kMissingCookie, request.GetWeakPtr(),
                         session_key));
    } else {
      UnblockDeferredRequests(session_key, RefreshResult::kFatalError);
      DeleteSessionAndNotify(DeletionReason::kFailedToRestoreKey, session_key,
                             request.device_bound_session_access_callback());
    }

    return;
  }

  RefreshSessionInternal(RefreshTrigger::kMissingCookie, request.GetWeakPtr(),
                         session_key, *key_id);
}

void SessionServiceImpl::OnRefreshRequestCompletion(
    RefreshTrigger trigger,
    OnAccessCallback on_access_callback,
    SessionKey session_key,
    RegistrationFetcher* fetcher,
    RegistrationResult registration_result) {
  SessionError::ErrorType result = OnRefreshRequestCompletionInternal(
      std::move(on_access_callback), session_key, fetcher,
      std::move(registration_result));

  Session* session = GetSession(session_key);
  if (session) {
    session->InformOfRefreshResult(
        /*was_proactive=*/trigger == RefreshTrigger::kProactive, result);
  }

  std::string histogram_base = "Net.DeviceBoundSessions.RefreshResult";
  std::string suffix;
  switch (trigger) {
    case RefreshTrigger::kProactive:
      suffix = ".Proactive";
      break;
    case RefreshTrigger::kMissingCookie:
      suffix = ".MissingCookie";
      break;
  }
  base::UmaHistogramEnumeration(histogram_base, result);
  base::UmaHistogramEnumeration(histogram_base + suffix, result);
}

// Continue or restart all deferred requests for the session and remove the
// session key in the map.
void SessionServiceImpl::UnblockDeferredRequests(
    const SessionKey& session_key,
    RefreshResult result,
    std::optional<bool> is_proactive_refresh_candidate,
    std::optional<base::TimeDelta> minimum_proactive_refresh_threshold) {
  if (auto it = proactive_requests_.find(session_key);
      it != proactive_requests_.end()) {
    base::UmaHistogramTimes("Net.DeviceBoundSessions.ProactiveRefreshDuration",
                            it->second.Elapsed());
    proactive_requests_.erase(it);
  }

  auto it = deferred_requests_.find(session_key);
  if (it == deferred_requests_.end()) {
    return;
  }

  auto requests = std::move(it->second);
  deferred_requests_.erase(it);

  base::UmaHistogramCounts100("Net.DeviceBoundSessions.RequestDeferredCount",
                              requests.size());

  if (is_proactive_refresh_candidate.has_value() &&
      minimum_proactive_refresh_threshold.has_value()) {
    base::UmaHistogramLongTimes100(
        "Net.DeviceBoundSessions.MinimumProactiveRefreshThreshold",
        *minimum_proactive_refresh_threshold);
    if (*is_proactive_refresh_candidate) {
      base::UmaHistogramLongTimes100(
          "Net.DeviceBoundSessions.MinimumProactiveRefreshThreshold.Success",
          *minimum_proactive_refresh_threshold);
    } else {
      base::UmaHistogramLongTimes100(
          "Net.DeviceBoundSessions.MinimumProactiveRefreshThreshold.Failure",
          *minimum_proactive_refresh_threshold);
    }

    if (*is_proactive_refresh_candidate) {
      if (*minimum_proactive_refresh_threshold <= base::Seconds(30)) {
        base::UmaHistogramCounts100(
            "Net.DeviceBoundSessions.ProactiveRefreshCandidateDeferredCount."
            "ThirtySeconds",
            requests.size());
        for (auto& request : requests) {
          base::UmaHistogramTimes(
              "Net.DeviceBoundSessions."
              "ProactiveRefreshCandidateRequestDeferredDuration.ThirtySeconds",
              request.timer.Elapsed());
        }
      }

      if (*minimum_proactive_refresh_threshold <= base::Minutes(1)) {
        base::UmaHistogramCounts100(
            "Net.DeviceBoundSessions.ProactiveRefreshCandidateDeferredCount."
            "OneMinute",
            requests.size());
        for (auto& request : requests) {
          base::UmaHistogramTimes(
              "Net.DeviceBoundSessions."
              "ProactiveRefreshCandidateRequestDeferredDuration.OneMinute",
              request.timer.Elapsed());
        }
      }

      if (*minimum_proactive_refresh_threshold <= base::Minutes(2)) {
        base::UmaHistogramCounts100(
            "Net.DeviceBoundSessions.ProactiveRefreshCandidateDeferredCount."
            "TwoMinutes",
            requests.size());
        for (auto& request : requests) {
          base::UmaHistogramTimes(
              "Net.DeviceBoundSessions."
              "ProactiveRefreshCandidateRequestDeferredDuration.TwoMinutes",
              request.timer.Elapsed());
        }
      }
    }
  }

  for (auto& request : requests) {
    base::UmaHistogramTimes("Net.DeviceBoundSessions.RequestDeferredDuration",
                            request.timer.Elapsed());
    base::UmaHistogramEnumeration("Net.DeviceBoundSessions.DeferralResult",
                                  result);
    if (request.timer.Elapsed() <= base::Milliseconds(1)) {
      base::UmaHistogramEnumeration(
          "Net.DeviceBoundSessions.DeferralResult.Instant", result);
    } else {
      base::UmaHistogramEnumeration(
          "Net.DeviceBoundSessions.DeferralResult.Slow", result);
    }
    std::move(request.callback).Run(result);
  }
}

void SessionServiceImpl::SetChallengeForBoundSession(
    OnAccessCallback on_access_callback,
    DbscRequest& request,
    const FirstPartySetMetadata& first_party_set_metadata,
    const SessionChallengeParam& param) {
  if (!param.session_id()) {
    return;
  }

  SessionKey session_key{SchemefulSite(request.url()),
                         Session::Id(*param.session_id())};
  Session* session = GetSession(session_key);
  if (!session) {
    return;
  }

  if (!session->CanSetBoundCookie(request, first_party_set_metadata)) {
    return;
  }

  NotifySessionAccess(on_access_callback, SessionAccess::AccessType::kUpdate,
                      session_key, *session);
  session->set_cached_challenge(param.challenge());
}

void SessionServiceImpl::GetAllSessionsAsync(
    base::OnceCallback<void(const std::vector<SessionKey>&)> callback) {
  if (pending_initialization_) {
    queued_operations_.push_back(base::BindOnce(
        &SessionServiceImpl::GetAllSessionsAsync,
        // `base::Unretained` is safe because the callback is stored in
        // `queued_operations_`, which is owned by `this`.
        base::Unretained(this), std::move(callback)));
  } else {
    std::vector<SessionKey> sessions = base::ToVector(
        unpartitioned_sessions_, [](const auto& pair) { return pair.first; });
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(sessions)));
  }
}

void SessionServiceImpl::DeleteSessionAndNotify(
    DeletionReason reason,
    const SessionKey& session_key,
    SessionService::OnAccessCallback per_request_callback) {
  auto it = unpartitioned_sessions_.find(session_key);
  if (it == unpartitioned_sessions_.end()) {
    return;
  }

  DeleteSessionAndNotifyInternal(reason, it, per_request_callback);
}

const Session* SessionServiceImpl::GetSession(
    const SessionKey& session_key) const {
  auto it = unpartitioned_sessions_.find(session_key);
  if (it != unpartitioned_sessions_.end()) {
    return it->second.get();
  }
  return nullptr;
}

Session* SessionServiceImpl::GetSession(const SessionKey& session_key) {
  return const_cast<Session*>(std::as_const(*this).GetSession(session_key));
}

void SessionServiceImpl::AddSession(
    const SchemefulSite& site,
    SessionParams params,
    base::span<const uint8_t> wrapped_key,
    base::OnceCallback<void(SessionError::ErrorType)> callback) {
  key_service_->FromWrappedSigningKeySlowlyAsync(
      wrapped_key, unexportable_keys::BackgroundTaskPriority::kBestEffort,
      base::BindOnce(&SessionServiceImpl::OnAddSessionKeyRestored,
                     weak_factory_.GetWeakPtr(), site, std::move(params),
                     std::move(callback)));
}

const SessionService::SignedRefreshChallenge*
SessionServiceImpl::GetLatestSignedRefreshChallenge(
    const SessionKey& session_key) {
  auto signed_challenge_it =
      latest_signed_refresh_challenges_.find(session_key);
  if (signed_challenge_it == latest_signed_refresh_challenges_.end()) {
    return nullptr;
  }
  return &signed_challenge_it->second;
}

void SessionServiceImpl::SetLatestSignedRefreshChallenge(
    SessionKey session_key,
    SessionService::SignedRefreshChallenge signed_refresh_challenge) {
  latest_signed_refresh_challenges_[std::move(session_key)] =
      std::move(signed_refresh_challenge);
}

void SessionServiceImpl::OnAddSessionKeyRestored(
    const SchemefulSite& site,
    SessionParams params,
    base::OnceCallback<void(SessionError::ErrorType)> callback,
    unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
        key_or_error) {
  if (!key_or_error.has_value()) {
    std::move(callback).Run(SessionError::kFailedToUnwrapKey);
    return;
  }

  params.key_id = *key_or_error;

  base::expected<std::unique_ptr<net::device_bound_sessions::Session>,
                 net::device_bound_sessions::SessionError>
      session_or_error =
          net::device_bound_sessions::Session::CreateIfValid(params);

  if (!session_or_error.has_value()) {
    std::move(callback).Run(session_or_error.error().type);
    return;
  }

  NotifySessionAccess(base::NullCallback(),
                      SessionAccess::AccessType::kCreation,
                      SessionKey{site, session_or_error.value()->id()},
                      *session_or_error.value());

  AddSession(site, std::move(session_or_error.value()));
  std::move(callback).Run(SessionError::kSuccess);
}

void SessionServiceImpl::AddSession(const SchemefulSite& site,
                                    std::unique_ptr<Session> session) {
  if (session_store_) {
    session_store_->SaveSession(site, *session);
  }

  unpartitioned_sessions_[SessionKey{site, session->id()}] = std::move(session);
}

void SessionServiceImpl::DeleteAllSessions(
    DeletionReason reason,
    std::optional<base::Time> created_after_time,
    std::optional<base::Time> created_before_time,
    base::RepeatingCallback<bool(const url::Origin&, const net::SchemefulSite&)>
        origin_and_site_matcher,
    base::OnceClosure completion_callback) {
  for (auto it = unpartitioned_sessions_.begin();
       it != unpartitioned_sessions_.end();) {
    auto curit = it;
    ++it;

    if (SessionMatchesFilter(curit->first.site, *curit->second,
                             created_after_time, created_before_time,
                             origin_and_site_matcher)) {
      DeleteSessionAndNotifyInternal(reason, curit, base::NullCallback());
    }
  }

  std::move(completion_callback).Run();
}

base::ScopedClosureRunner SessionServiceImpl::AddObserver(
    const GURL& url,
    base::RepeatingCallback<void(const SessionAccess&)> callback) {
  auto observer = std::make_unique<Observer>(url, callback);
  base::ScopedClosureRunner subscription(base::BindOnce(
      &SessionServiceImpl::RemoveObserver, weak_factory_.GetWeakPtr(),
      net::SchemefulSite(url), observer.get()));
  observers_by_site_[net::SchemefulSite(url)].insert(std::move(observer));
  return subscription;
}

void SessionServiceImpl::DeleteSessionAndNotifyInternal(
    DeletionReason reason,
    SessionServiceImpl::SessionsMap::iterator it,
    SessionService::OnAccessCallback per_request_callback) {
  base::UmaHistogramEnumeration("Net.DeviceBoundSessions.DeletionReason",
                                reason);

  if (session_store_) {
    session_store_->DeleteSession(it->first);
  }

  NotifySessionAccess(per_request_callback,
                      SessionAccess::AccessType::kTermination, it->first,
                      *it->second);

  unpartitioned_sessions_.erase(it);
}

void SessionServiceImpl::NotifySessionAccess(
    SessionService::OnAccessCallback per_request_callback,
    SessionAccess::AccessType access_type,
    const SessionKey& session_key,
    const Session& session) {
  SessionAccess access{access_type, session_key};

  if (access_type == SessionAccess::AccessType::kTermination) {
    access.cookies.reserve(session.cookies().size());
    for (const CookieCraving& cookie : session.cookies()) {
      access.cookies.push_back(cookie.Name());
    }
  }

  if (per_request_callback) {
    per_request_callback.Run(access);
  }

  auto observers_it = observers_by_site_.find(session_key.site);
  if (observers_it == observers_by_site_.end()) {
    return;
  }

  for (const auto& observer : observers_it->second) {
    if (session.IncludesUrl(observer->url)) {
      observer->callback.Run(access);
    }
  }
}

void SessionServiceImpl::RemoveObserver(net::SchemefulSite site,
                                        Observer* observer) {
  auto observers_it = observers_by_site_.find(site);
  if (observers_it == observers_by_site_.end()) {
    return;
  }

  ObserverSet& observers = observers_it->second;

  auto it = observers.find(observer);
  if (it == observers.end()) {
    return;
  }

  observers.erase(it);

  if (observers.empty()) {
    observers_by_site_.erase(observers_it);
  }
}

SessionError::ErrorType SessionServiceImpl::OnRegistrationCompleteInternal(
    OnAccessCallback on_access_callback,
    RegistrationFetcher* fetcher,
    RegistrationResult registration_result) {
  RemoveFetcher(fetcher);

  return std::move(registration_result)
      .Visit(absl::Overload(
          [&](std::unique_ptr<Session> session) {
            CHECK(session);
            const SchemefulSite site(session->origin());
            NotifySessionAccess(on_access_callback,
                                SessionAccess::AccessType::kCreation,
                                SessionKey{site, session->id()}, *session);
            AddSession(site, std::move(session));
            return SessionError::kSuccess;
          },
          [](RegistrationResult::NoSessionConfigChange)
              -> SessionError::ErrorType {
            // This should not be returned for registrations.
            NOTREACHED();
          },
          [](SessionError error) {
            // We failed to create a new session, so there's nothing to clean
            // up.
            return error.type;
          }));
}

SessionError::ErrorType SessionServiceImpl::OnRefreshRequestCompletionInternal(
    OnAccessCallback on_access_callback,
    const SessionKey& session_key,
    RegistrationFetcher* fetcher,
    RegistrationResult registration_result) {
  RemoveFetcher(fetcher);
  CookieAndLineAccessResultList stored_cookies =
      registration_result.TakeStoredCookies();

  SessionError::ErrorType result =
      std::move(registration_result)
          .Visit(absl::Overload(
              [&](std::unique_ptr<Session> new_session) {
                // If refresh succeeded:
                // 1. update the session by adding a new session, replacing the
                //    old one
                // 2. restart the deferred requests.
                CHECK(new_session);
                CHECK_EQ(new_session->id(), session_key.id);

                Session* existing_session = GetSession(session_key);
                if (!existing_session) {
                  return SessionError::kSessionDeletedDuringRefresh;
                }

                bool is_proactive_refresh_candidate =
                    IsProactiveRefreshCandidate(*existing_session, *new_session,
                                                stored_cookies);
                std::optional<base::TimeDelta> minimum_cookie_lifetime =
                    existing_session
                        ->TakeLastProactiveRefreshOpportunityMinimumCookieLifetime();
                SchemefulSite new_site(new_session->origin());
                AddSession(new_site, std::move(new_session));
                // The session has been refreshed, restart the request.
                UnblockDeferredRequests(session_key, RefreshResult::kRefreshed,
                                        is_proactive_refresh_candidate,
                                        std::move(minimum_cookie_lifetime));
                return SessionError::kSuccess;
              },
              [&](RegistrationResult::NoSessionConfigChange) {
                Session* existing_session = GetSession(session_key);
                if (!existing_session) {
                  return SessionError::kSessionDeletedDuringRefresh;
                }
                bool is_proactive_refresh_candidate =
                    IsProactiveRefreshCandidate(
                        *existing_session, *existing_session, stored_cookies);

                UnblockDeferredRequests(
                    session_key, RefreshResult::kRefreshed,
                    is_proactive_refresh_candidate,
                    existing_session
                        ->TakeLastProactiveRefreshOpportunityMinimumCookieLifetime());
                return SessionError::kSuccess;
              },
              [&](SessionError error) {
                if (std::optional<DeletionReason> deletion_reason =
                        error.GetDeletionReason();
                    deletion_reason.has_value()) {
                  DeleteSessionAndNotify(*deletion_reason, session_key,
                                         on_access_callback);
                  UnblockDeferredRequests(session_key,
                                          RefreshResult::kFatalError);
                } else {
                  RefreshResult refresh_result;
                  if (error.IsServerError()) {
                    refresh_result = RefreshResult::kServerError;
                  } else if (error.type ==
                             SessionError::kSigningQuotaExceeded) {
                    refresh_result = RefreshResult::kSigningQuotaExceeded;
                  } else {
                    refresh_result = RefreshResult::kUnreachable;
                  }
                  // Transient error, unblock the request without cookies.
                  UnblockDeferredRequests(session_key, refresh_result);
                }
                return error.type;
              }));

  refresh_last_result_.insert_or_assign(session_key.site, SessionError(result));

  return result;
}

void SessionServiceImpl::RestoreSessionKey(
    const SessionKey& session_key,
    OnAccessCallback on_access_callback,
    base::OnceCallback<
        void(std::optional<unexportable_keys::UnexportableKeyId>)> callback) {
  if (session_store_) {
    session_store_->RestoreSessionBindingKey(
        session_key, base::BindOnce(&SessionServiceImpl::OnSessionKeyRestored,
                                    weak_factory_.GetWeakPtr(), session_key,
                                    on_access_callback, std::move(callback)));
  } else {
    OnSessionKeyRestored(
        session_key, on_access_callback, std::move(callback),
        base::unexpected(unexportable_keys::ServiceError::kKeyNotReady));
  }
}

void SessionServiceImpl::OnSessionKeyRestored(
    const SessionKey& session_key,
    OnAccessCallback on_access_callback,
    base::OnceCallback<
        void(std::optional<unexportable_keys::UnexportableKeyId>)> callback,
    Session::KeyIdOrError key_id_or_error) {
  if (!key_id_or_error.has_value()) {
    UnblockDeferredRequests(session_key, RefreshResult::kFatalError);
    DeleteSessionAndNotify(DeletionReason::kFailedToUnwrapKey, session_key,
                           on_access_callback);
    std::move(callback).Run(std::nullopt);
    return;
  }

  auto* session = GetSession(session_key);
  if (!session) {
    UnblockDeferredRequests(session_key, RefreshResult::kFatalError);
    return;
  }

  session->set_unexportable_key_id(key_id_or_error);
  std::move(callback).Run(*key_id_or_error);
}

void SessionServiceImpl::RefreshSessionInternal(
    RefreshTrigger trigger,
    base::WeakPtr<URLRequest> maybe_request,
    const SessionKey& session_key,
    std::optional<unexportable_keys::UnexportableKeyId> key_id) {
  if (!maybe_request || !key_id) {
    return;
  }

  DbscRequest request(maybe_request.get());

  net::NetLogSource net_log_source_for_refresh = net::NetLogSource(
      net::NetLogSourceType::URL_REQUEST, net::NetLog::Get()->NextID());
  request.net_log().AddEventReferencingSource(
      net::NetLogEventType::DBSC_REFRESH_REQUEST, net_log_source_for_refresh);

  if (!base::FeatureList::IsEnabled(
          features::kDeviceBoundSessionSigningQuotaAndCaching)) {
    refresh_times_[session_key.site].push_back(base::TimeTicks::Now());
  }

  Session* session = GetSession(session_key);
  if (!session) {
    return;
  }

  auto registration_param =
      RegistrationRequestParam::CreateForRefresh(*session);

  auto callback = base::BindOnce(
      &SessionServiceImpl::OnRefreshRequestCompletion,
      weak_factory_.GetWeakPtr(), trigger,
      request.device_bound_session_access_callback(), session_key);
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          registration_param, *this, key_service_.get(), context_.get(),
          request.isolation_info(), net_log_source_for_refresh,
          request.initiator());
  RegistrationFetcher* fetcher_raw = fetcher.get();
  registration_fetchers_.insert(std::move(fetcher));
  fetcher_raw->StartFetchWithExistingKey(registration_param, *key_id,
                                         std::move(callback));
  // `fetcher_raw` may be deleted.
}

bool SessionServiceImpl::RefreshQuotaExceeded(const SchemefulSite& site) {
  if (base::FeatureList::IsEnabled(
          features::kDeviceBoundSessionSigningQuotaAndCaching)) {
    return false;
  }

  if (ignore_refresh_quota_) {
    return false;
  }

  auto it = refresh_times_.find(site);
  if (it == refresh_times_.end()) {
    return false;
  }

  std::erase_if(it->second, [](base::TimeTicks time) {
    return base::TimeTicks::Now() - time >= kSigningQuotaInterval;
  });

  size_t refresh_count = it->second.size();
  if (refresh_count == 0) {
    refresh_times_.erase(it);
  }

  if (auto result_it = refresh_last_result_.find(site);
      refresh_count >= kSigningQuota &&
      result_it != refresh_last_result_.end()) {
    base::UmaHistogramEnumeration(
        "Net.DeviceBoundSessions.RefreshQuotaExceededLastResult",
        result_it->second.type);
  }

  return refresh_count >= kSigningQuota;
}

bool SessionServiceImpl::SigningQuotaExceeded(const SchemefulSite& site) {
  if (!base::FeatureList::IsEnabled(
          features::kDeviceBoundSessionSigningQuotaAndCaching)) {
    return false;
  }

  // TODO(crbug.com/457803903): Rename refresh quota feature to signing quota.
  if (ignore_refresh_quota_) {
    return false;
  }

  auto it = signing_times_.find(site);
  if (it == signing_times_.end()) {
    return false;
  }

  std::erase_if(it->second, [](base::TimeTicks time) {
    return base::TimeTicks::Now() - time >= kSigningQuotaInterval;
  });

  size_t sign_count = it->second.size();
  if (sign_count == 0) {
    signing_times_.erase(it);
  }

  bool is_exceeded = sign_count >= kSigningQuota;
  if (auto result_it = refresh_last_result_.find(site);
      is_exceeded && result_it != refresh_last_result_.end()) {
    base::UmaHistogramEnumeration(
        "Net.DeviceBoundSessions.SigningQuotaExceededLastResult",
        result_it->second.type);
  }

  return is_exceeded;
}

void SessionServiceImpl::AddSigningOccurrence(const SchemefulSite& site) {
  signing_times_[site].push_back(base::TimeTicks::Now());
}

void SessionServiceImpl::RemoveFetcher(RegistrationFetcher* fetcher) {
  if (!fetcher) {
    return;
  }
  auto it = registration_fetchers_.find(fetcher);
  if (it == registration_fetchers_.end()) {
    return;
  }
  registration_fetchers_.erase(it);
}

void SessionServiceImpl::MaybeStartProactiveRefresh(
    SessionService::OnAccessCallback per_request_callback,
    DbscRequest& request,
    const SessionKey& session_key,
    base::TimeDelta minimum_cookie_lifetime) {
  if (!base::FeatureList::IsEnabled(
          features::kDeviceBoundSessionProactiveRefresh)) {
    return;
  }

  if (minimum_cookie_lifetime >
      features::kDeviceBoundSessionProactiveRefreshThreshold.Get()) {
    return;
  }

  if (deferred_requests_.find(session_key) != deferred_requests_.end()) {
    // It's not a proactive refresh if we're in the middle of a regular refresh.
    LogProactiveRefreshAttempt(
        ProactiveRefreshAttempt::kExistingDeferringRefresh);
    return;
  }

  auto* session = GetSession(session_key);
  CHECK(session);

  if (RefreshQuotaExceeded(session_key.site)) {
    LogProactiveRefreshAttempt(ProactiveRefreshAttempt::kSigningQuota);
    return;
  }

  if (session->ShouldBackoff()) {
    LogProactiveRefreshAttempt(ProactiveRefreshAttempt::kBackoff);
    return;
  }

  if (session->attempted_proactive_refresh_since_last_success()) {
    // We only do one proactive refresh attempt before a deferral. If we
    // did not do this, every refresh due to missing cookies would be
    // skipped due to the refresh quota. Instead, we allow the refresh
    // due to missing cookies, which will communicate its reason for
    // failure in the Secure-Session-Skipped header.
    LogProactiveRefreshAttempt(
        ProactiveRefreshAttempt::kPreviousFailedProactiveRefresh);
    return;
  }

  if (!session->unexportable_key_id().has_value()) {
    // TODO(crbug.com/358137054): If we're otherwise ready for a proactive
    // refresh, we could start restoring the key. This is lower priority
    // than regular proactive refresh, since some amount of startup
    // latency is unavoidable with DBSC.
    LogProactiveRefreshAttempt(ProactiveRefreshAttempt::kMissingKey);
    return;
  }

  auto [_, inserted] = proactive_requests_.try_emplace(session_key);
  if (!inserted) {
    // Do not proactively refresh if we've already started one proactive
    // refresh.
    LogProactiveRefreshAttempt(
        ProactiveRefreshAttempt::kExistingProactiveRefresh);
    return;
  }

  NotifySessionAccess(per_request_callback, SessionAccess::AccessType::kUpdate,
                      session_key, *session);
  LogProactiveRefreshAttempt(ProactiveRefreshAttempt::kAttempted);
  RefreshSessionInternal(RefreshTrigger::kProactive, request.GetWeakPtr(),
                         session_key, *session->unexportable_key_id());
}

}  // namespace net::device_bound_sessions
