// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_SESSION_SERVICE_IMPL_H_
#define NET_DEVICE_BOUND_SESSIONS_SESSION_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "net/base/net_export.h"
#include "net/device_bound_sessions/refresh_result.h"
#include "net/device_bound_sessions/registration_fetcher.h"
#include "net/device_bound_sessions/registration_fetcher_param.h"
#include "net/device_bound_sessions/session.h"
#include "net/device_bound_sessions/session_key.h"
#include "net/device_bound_sessions/session_service.h"

namespace net {
class URLRequest;
class URLRequestContext;
class SchemefulSite;
}  // namespace net

namespace unexportable_keys {
class UnexportableKeyService;
}

namespace net::device_bound_sessions {

class SessionStore;

struct DeferredURLRequest {
  explicit DeferredURLRequest(SessionService::RefreshCompleteCallback callback);
  DeferredURLRequest(DeferredURLRequest&& other) noexcept;

  DeferredURLRequest& operator=(DeferredURLRequest&& other) noexcept;

  ~DeferredURLRequest();

  base::ElapsedTimer timer;
  SessionService::RefreshCompleteCallback callback;
};

class NET_EXPORT SessionServiceImpl : public SessionService {
 public:
  // Result of attempting to start a proactive refresh. This enum only
  // covers reasons we don't start the refresh despite a cookie expiring
  // soon.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(ProactiveRefreshAttempt)
  enum class ProactiveRefreshAttempt {
    kExistingDeferringRefresh = 0,
    kExistingProactiveRefresh = 1,
    kMissingKey = 2,
    kAttempted = 3,
    kPreviousFailedProactiveRefresh = 4,
    kSigningQuota = 5,
    kBackoff = 6,
    kMaxValue = kBackoff,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:DeviceBoundSessionProactiveRefreshAttempt)

  SessionServiceImpl(unexportable_keys::UnexportableKeyService& key_service,
                     const URLRequestContext* request_context,
                     SessionStore* store);
  ~SessionServiceImpl() override;

  // Loads saved session data from disk if a `SessionStore` object is provided
  // during construction. Otherwise, it is a no-op.
  void LoadSessionsAsync();

  void RegisterBoundSession(
      OnAccessCallback on_access_callback,
      RegistrationFetcherParam registration_params,
      const IsolationInfo& isolation_info,
      const NetLogWithSource& net_log,
      const std::optional<url::Origin>& original_request_initiator) override;

  std::optional<DeferralParams> ShouldDefer(
      DbscRequest& request,
      HttpRequestHeaders* extra_headers,
      const FirstPartySetMetadata& first_party_set_metadata) override;

  void DeferRequestForRefresh(DbscRequest& request,
                              DeferralParams deferral,
                              RefreshCompleteCallback callback) override;

  void SetChallengeForBoundSession(
      OnAccessCallback on_access_callback,
      DbscRequest& request,
      const FirstPartySetMetadata& first_party_set_metadata,
      const SessionChallengeParam& param) override;

  void GetAllSessionsAsync(
      base::OnceCallback<void(const std::vector<SessionKey>&)> callback)
      override;
  void DeleteSessionAndNotify(
      DeletionReason reason,
      const SessionKey& session_key,
      SessionService::OnAccessCallback per_request_callback) override;
  void DeleteAllSessions(
      DeletionReason reason,
      std::optional<base::Time> created_after_time,
      std::optional<base::Time> created_before_time,
      base::RepeatingCallback<bool(const url::Origin&,
                                   const net::SchemefulSite&)>
          origin_and_site_matcher,
      base::OnceClosure completion_callback) override;
  base::ScopedClosureRunner AddObserver(
      const GURL& url,
      base::RepeatingCallback<void(const SessionAccess&)> callback) override;
  const Session* GetSession(const SessionKey& session_key) const override;
  void AddSession(
      const SchemefulSite& site,
      SessionParams params,
      base::span<const uint8_t> wrapped_key,
      base::OnceCallback<void(SessionError::ErrorType)> callback) override;
  const SignedRefreshChallenge* GetLatestSignedRefreshChallenge(
      const SessionKey& session_key) override;
  void SetLatestSignedRefreshChallenge(
      SessionKey session_key,
      SignedRefreshChallenge signed_refresh_challenge) override;
  bool SigningQuotaExceeded(const SchemefulSite& site) override;
  void AddSigningOccurrence(const SchemefulSite& site) override;

  // The `SessionService` implementation has a const-qualified accessor
  // for sessions. This overload allows for non-const access as well.
  Session* GetSession(const SessionKey& session_key);

 private:
  friend class SessionServiceImplWithStoreTest;

  // The key is the site (eTLD+1) of the session's origin and the
  // session id.
  using SessionsMap = std::map<SessionKey, std::unique_ptr<Session>>;
  using DeferredRequestsMap =
      std::map<SessionKey, absl::InlinedVector<DeferredURLRequest, 1>>;
  using ProactiveRefreshMap = std::map<SessionKey, base::ElapsedTimer>;
  using LatestSignedRefreshChallengesMap =
      std::map<SessionKey, SignedRefreshChallenge>;

  struct Observer {
    Observer(const GURL& url,
             base::RepeatingCallback<void(const SessionAccess&)> callback);

    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    ~Observer();

    GURL url;
    base::RepeatingCallback<void(const SessionAccess&)> callback;
  };

  using ObserverSet =
      std::set<std::unique_ptr<Observer>, base::UniquePtrComparator>;

  enum class RefreshTrigger {
    // Refresh due to a request missing a bound cookie.
    kMissingCookie,
    // Proactive refresh due to a soon-to-expire bound cookie.
    kProactive,
  };

  void OnLoadSessionsComplete(SessionsMap sessions);

  void OnRegistrationComplete(OnAccessCallback on_access_callback,
                              bool is_google_subdomain_for_histograms,
                              bool is_federated_registration_for_histograms,
                              RegistrationFetcher* fetcher,
                              RegistrationResult result);
  void OnRefreshRequestCompletion(RefreshTrigger trigger,
                                  OnAccessCallback on_access_callback,
                                  SessionKey session_key,
                                  RegistrationFetcher* fetcher,
                                  RegistrationResult result);

  void StartGarbageCollection();
  void OnGetAllKeysForGarbageCollection(
      unexportable_keys::ServiceErrorOr<
          std::vector<unexportable_keys::UnexportableKeyId>>
          all_key_ids_or_error);
  void DoGarbageCollection(
      std::vector<unexportable_keys::UnexportableKeyId> all_key_ids);

  void AddSession(const SchemefulSite& site, std::unique_ptr<Session> session);
  void UnblockDeferredRequests(
      const SessionKey& session_key,
      RefreshResult result,
      std::optional<bool> is_proactive_refresh_candidate = std::nullopt,
      std::optional<base::TimeDelta> minimum_proactive_refresh_threshold =
          std::nullopt);

  // Get all the unexpired sessions for a given site. This also removes
  // expired sessions for the site and extends the TTL of used sessions.
  std::ranges::subrange<SessionsMap::iterator> GetSessionsForSite(
      const SchemefulSite& site);

  // Remove a session from the session map. It also clears the session
  // from `session_store_` and notifies any observers (including
  // `per_request_callback`) about the termination.
  void DeleteSessionAndNotifyInternal(
      DeletionReason reason,
      SessionsMap::iterator it,
      SessionService::OnAccessCallback per_request_callback);

  // Notify all observers about an access to a session. Will update
  // `per_request_callback` unconditionally, and any observers in
  // `observers_` which have a URL in the scope of `session`.
  void NotifySessionAccess(
      SessionService::OnAccessCallback per_request_callback,
      SessionAccess::AccessType access_type,
      const SessionKey& session_key,
      const Session& session);

  // Remove an observer by site and pointer.
  void RemoveObserver(net::SchemefulSite site, Observer* observer);

  // Helper function encapsulating the processing of registration
  SessionError::ErrorType OnRegistrationCompleteInternal(
      OnAccessCallback on_access_callback,
      RegistrationFetcher* fetcher,
      RegistrationResult result);

  // Helper function encapsulating the processing of refresh
  SessionError::ErrorType OnRefreshRequestCompletionInternal(
      OnAccessCallback on_access_callback,
      const SessionKey& session_key,
      RegistrationFetcher* fetcher,
      RegistrationResult result);

  // Helper for restoring the key for the session identified by
  // `session_key`. If restoring the key succeeds, calls `callback` with
  // the key. If restoring the key fails, deletes the session and calls
  // `callback` with `std::nullopt`.
  void RestoreSessionKey(
      const SessionKey& session_key,
      OnAccessCallback on_access_callback,
      base::OnceCallback<
          void(std::optional<unexportable_keys::UnexportableKeyId>)> callback);

  // Callback after unwrapping a session key. `on_access_callback` is
  // used to notify the browser that this request led to usage of a
  // session. If restoring the key succeeds, calls `callback` with
  // the key. If restoring the key fails, deletes the session and calls
  // `callback` with `std::nullopt`.
  void OnSessionKeyRestored(
      const SessionKey& session_key,
      OnAccessCallback on_access_callback,
      base::OnceCallback<
          void(std::optional<unexportable_keys::UnexportableKeyId>)> callback,
      Session::KeyIdOrError key_id_or_error);

  // Helper function for starting a refresh
  void RefreshSessionInternal(
      RefreshTrigger trigger,
      base::WeakPtr<URLRequest> maybe_request,
      const SessionKey& session_key,
      std::optional<unexportable_keys::UnexportableKeyId> key_id);

  // Whether the site has exceeded its refresh quota.
  bool RefreshQuotaExceeded(const SchemefulSite& site);

  // Add a header to `request` indicating which sessions should have
  // applied, but did not due to error conditions.
  void AddDebugHeader(const DbscRequest& request);

  // Removes `fetcher` from the set of active fetchers. If `fetcher` is
  // null, does nothing.
  void RemoveFetcher(RegistrationFetcher* fetcher);

  // Asynchronously get the federated provider session specified by
  // `registration_params`, if allowed.
  void GetFederatedProviderSessionIfValid(
      GURL provider_url,
      Session::Id provider_session_id,
      std::string provider_key_thumbprint,
      OnAccessCallback on_access_callback,
      base::OnceCallback<void(base::expected<Session*, SessionError>)>
          callback);

  // Helper for getting the federated provider session. Checks that the
  // key thumbprints maps and runs `callback` with the resulting
  // provider session or an error on mismatch.
  void CheckFederatedProviderKey(
      SessionKey provider_session_key,
      std::string provider_key_thumbprint,
      base::OnceCallback<void(base::expected<Session*, SessionError>)> callback,
      std::optional<unexportable_keys::UnexportableKeyId> provider_key);

  void OnAddSessionKeyRestored(
      const SchemefulSite& site,
      SessionParams params,
      base::OnceCallback<void(SessionError::ErrorType)> callback,
      unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
          key_or_error);

  // If `minimum_cookie_lifetime` is small enough and there are no
  // pending refreshes for `session_key`, start a proactive refresh.
  void MaybeStartProactiveRefresh(
      SessionService::OnAccessCallback per_request_callback,
      DbscRequest& request,
      const SessionKey& session_key,
      base::TimeDelta minimum_cookie_lifetime);

  // Helper function for common behavior from federated and regular
  // session registration.
  void RegisterBoundSessionInternal(
      OnAccessCallback on_access_callback,
      RegistrationFetcherParam registration_params,
      const IsolationInfo& isolation_info,
      const NetLogWithSource& net_log,
      const std::optional<url::Origin>& original_request_initiator,
      base::expected<Session*, SessionError> federated_provider_session);

  // Whether we are waiting on the initial load of saved sessions to
  // complete.
  bool pending_initialization_ = false;
  // Functions to call once initialization completes.
  std::vector<base::OnceClosure> queued_operations_;
  // Number of requests deferred due to pending initialization.
  size_t requests_before_initialization_ = 0;

  const raw_ref<unexportable_keys::UnexportableKeyService> key_service_;
  raw_ptr<const URLRequestContext> context_;
  raw_ptr<SessionStore> session_store_ = nullptr;

  // When true, the refresh quota is not enforced. This is only ever set to
  // true for testing purposes.
  bool ignore_refresh_quota_ = false;

  // Deferred requests are stored by session key.
  DeferredRequestsMap deferred_requests_;

  // Proactive refresh requests, stored by session key.
  ProactiveRefreshMap proactive_requests_;

  // Storage is similar to how CookieMonster stores its cookies.
  SessionsMap unpartitioned_sessions_;

  // All observers of sessions.
  std::map<net::SchemefulSite, ObserverSet> observers_by_site_;

  // Per-site session refresh quota. In order to be robust across
  // session parameter changes, we enforce refresh quota for a site.
  // This functionality is being replaced with `signing_times_`.
  std::map<net::SchemefulSite, std::vector<base::TimeTicks>> refresh_times_;

  // Per-site record of the most recent refresh result. This is used
  // for histograms.
  std::map<net::SchemefulSite, SessionError> refresh_last_result_;

  // Per-site session signing quota. In order to be robust across
  // session parameter changes, we enforce signing quota for a site.
  // This is updated whenever a site triggers signing.
  std::map<net::SchemefulSite, std::vector<base::TimeTicks>> signing_times_;

  // The latest signed challenges per session.
  LatestSignedRefreshChallengesMap latest_signed_refresh_challenges_;

  // Holds all currently live registration fetchers.
  std::set<std::unique_ptr<RegistrationFetcher>, base::UniquePtrComparator>
      registration_fetchers_;

  base::WeakPtrFactory<SessionServiceImpl> weak_factory_{this};
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_SERVICE_IMPL_H_
