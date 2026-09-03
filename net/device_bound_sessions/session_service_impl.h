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
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/optional_ref.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "net/base/net_export.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/site_for_cookies.h"
#include "net/device_bound_sessions/refresh_result.h"
#include "net/device_bound_sessions/registration_fetcher.h"
#include "net/device_bound_sessions/registration_fetcher_param.h"
#include "net/device_bound_sessions/registration_request_param.h"
#include "net/device_bound_sessions/session.h"
#include "net/device_bound_sessions/session_key.h"
#include "net/device_bound_sessions/session_service.h"
#include "net/device_bound_sessions/session_store.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace net {
class URLRequest;
class URLRequestContext;
class SchemefulSite;
}  // namespace net

namespace unexportable_keys {
class UnexportableKeyService;
}

namespace net::device_bound_sessions {

struct DeferredURLRequest {
  // A weak pointer to the deferred request. Stored to allow resiliently
  // selecting a new triggering request if the original triggering request is
  // canceled during asynchronous key restoration.
  base::WeakPtr<URLRequest> request;
  base::ElapsedTimer timer;
  SessionService::RefreshCompleteCallback callback;
  bool triggered_refresh = false;
};

class NET_EXPORT SessionServiceImpl : public SessionService {
 public:
  // The maximum number of pre-provisioned keys per Identity Provider. Declared
  // here for use in tests.
  static constexpr size_t kMaxPreProvisionedKeysPerIdentityProvider = 10;

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
    // kSigningQuota = 5,  // no longer used
    kBackoff = 6,
    kMaxValue = kBackoff,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:DeviceBoundSessionProactiveRefreshAttempt)

  // Parameters required to initiate a session registration. It's shared by
  // all registration flows (standalone, federated, and SSO).
  struct RegistrationParams {
    SessionService::OnAccessCallback access_callback;
    RegistrationFetcherParam fetcher_param;
    net::IsolationInfo isolation_info;
    net::SiteForCookies site_for_cookies;
    net::NetLogWithSource net_log;
    std::optional<url::Origin> original_request_initiator;
  };

  SessionServiceImpl(unexportable_keys::UnexportableKeyService& key_service,
                     const URLRequestContext* request_context,
                     SessionStore* store,
                     const std::vector<SchemefulSite>& restricted_sites,
                     CookieAccessCallback has_cookie_access_cb,
                     SelectClientCertificateHandler client_cert_handler);
  ~SessionServiceImpl() override;

  // Loads saved session data from disk if a `SessionStore` object is provided
  // during construction. Otherwise, it is a no-op.
  void LoadSessionsAsync();

  void RegisterBoundSession(
      OnAccessCallback on_access_callback,
      RegistrationFetcherParam fetcher_param,
      const IsolationInfo& isolation_info,
      const net::SiteForCookies& site_for_cookies,
      const NetLogWithSource& net_log,
      const std::optional<url::Origin>& original_request_initiator) override;

  bool AddPreProvisionedKey(
      const url::Origin& rp_origin,
      std::string_view provider_key,
      const GURL& provider_url,
      unexportable_keys::UnexportableSigningKeyId key_id) override;

  SessionErrorOr<unexportable_keys::UnexportableSigningKeyId>
  FindPreProvisionedKey(const ProviderRegistrationParams& provider_params,
                        base::optional_ref<const url::Origin>
                            original_request_initiator) override;

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
  void GetAllSessionDisplaysAsync(
      base::OnceCallback<void(const std::vector<SessionDisplay>&)> callback)
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
  base::CallbackListSubscription AddEventObserver(
      OnEventCallback callback) override;
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
  void HandleResponseHeaders(
      DbscRequest& request,
      HttpResponseHeaders* headers,
      const FirstPartySetMetadata& first_party_set_metadata) override;
  void SelectClientCertificate(
      const GURL& url,
      scoped_refptr<SSLCertRequestInfo> cert_info,
      SelectClientCertificateCallback callback) override;
  void PrewarmSessionsForUrl(const GURL& url,
                             PrewarmCallback callback) override;

  // The `SessionService` implementation has a const-qualified accessor
  // for sessions. This overload allows for non-const access as well.
  Session* GetSession(const SessionKey& session_key);

 private:
  friend class SessionServiceImplWithStoreTest;

  enum class RefreshTrigger {
    // Refresh due to a request missing a bound cookie.
    kMissingCookie,
    // Proactive refresh due to a soon-to-expire bound cookie.
    kProactive,
  };

  // The refresh outcome and earliest next refresh time for a single session
  // evaluated during `PrewarmSessionsForUrl`.
  struct PrewarmResult {
    RefreshResult result = RefreshResult::kRefreshed;
    base::Time earliest_next_refresh_time = base::Time::Max();
  };

  // State tracking an in-flight proactive refresh for a session, including
  // callbacks waiting for it to finish and a timer for metrics.
  struct ProactiveRefresh {
    std::vector<base::OnceCallback<void(PrewarmResult)>> completion_callbacks;
    base::ElapsedTimer timer;
  };

  // Parameters required to initiate a session refresh. Encapsulates trigger
  // cause, isolation context, site for cookies, priority, and net logging.
  struct RefreshParams {
    RefreshTrigger trigger = RefreshTrigger::kProactive;
    net::IsolationInfo isolation_info;
    net::SiteForCookies site_for_cookies;
    std::optional<url::Origin> initiator;
    unexportable_keys::BackgroundTaskPriority priority =
        unexportable_keys::BackgroundTaskPriority::kBestEffort;
    SessionService::OnAccessCallback access_callback;
    net::NetLogWithSource net_log;
  };

  // The key is the site (eTLD+1) of the session's origin and the
  // session id.
  // NOTE: This map needs to be ordered, and thus is not a hash map.
  using SessionsMap = std::map<SessionKey, std::unique_ptr<Session>>;
  using DeferredRequestsMap =
      absl::flat_hash_map<SessionKey,
                          absl::InlinedVector<DeferredURLRequest, 1>>;
  using ProactiveRefreshMap = absl::flat_hash_map<SessionKey, ProactiveRefresh>;
  using LatestSignedRefreshChallengesMap =
      absl::flat_hash_map<SessionKey, SignedRefreshChallenge>;

  struct PreProvisionedKeyEntry {
    GURL provider_url;
    net::SchemefulSite provider_site;
    url::Origin rp_origin;
    std::string provider_key;
    unexportable_keys::UnexportableSigningKeyId key_id;
  };

  struct Observer {
    GURL url;
    base::RepeatingCallback<void(const SessionAccess&)> callback;
  };

  using ObserverSet = absl::flat_hash_set<std::unique_ptr<Observer>>;

  using FetchStarter = base::OnceCallback<void(
      RegistrationFetcher*,
      RegistrationRequestParam,
      RegistrationFetcher::RegistrationCompleteCallback)>;

  // The type of registration for a bound session. Used for histograms.
  // LINT.IfChange(RegistrationType)
  enum class RegistrationType {
    kStandalone = 0,
    kFederated = 1,
    kSingleSignOn = 2,
    kMaxValue = kSingleSignOn,
  };

  template <typename Sink>
  friend void AbslStringify(Sink& sink, RegistrationType type) {
    switch (type) {
      case RegistrationType::kStandalone:
        sink.Append("Standalone");
        return;
      case RegistrationType::kFederated:
        sink.Append("Federated");
        return;
      case RegistrationType::kSingleSignOn:
        sink.Append("SingleSignOn");
        return;
    }
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/net/histograms.xml:RegistrationType)

  void OnLoadSessionsComplete(SessionsMap sessions);

  void OnRegistrationComplete(OnAccessCallback on_access_callback,
                              GURL endpoint,
                              RegistrationType registration_type_for_histograms,
                              RegistrationFetcher* fetcher,
                              RegistrationResult registration_result);
  void OnRefreshRequestCompletion(RefreshTrigger trigger,
                                  OnAccessCallback on_access_callback,
                                  SessionKey session_key,
                                  RegistrationFetcher* fetcher,
                                  RegistrationResult registration_result);

  void StartGarbageCollection();
  void OnGetAllKeysForGarbageCollection(
      unexportable_keys::ServiceErrorOr<
          std::vector<unexportable_keys::UnexportableSigningKeyId>>
          all_key_ids_or_error);
  void DoGarbageCollection(
      std::vector<unexportable_keys::UnexportableSigningKeyId> all_key_ids);

  void AddSession(const SchemefulSite& site,
                  std::unique_ptr<Session> session,
                  SessionStore::SaveSessionMode mode =
                      SessionStore::SaveSessionMode::kNewSession);

  // Continue or restart all deferred requests and complete any proactive
  // refresh requests waiting for the session, removing the session key from
  // both maps.
  void UnblockWaitingRequests(
      const SessionKey& session_key,
      RefreshResult result,
      std::optional<net::device_bound_sessions::SessionError> fetch_error =
          std::nullopt,
      std::optional<SessionDisplay> new_session_display = std::nullopt,
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
      RegistrationResult result,
      SchemefulSite site);

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
          void(std::optional<unexportable_keys::UnexportableSigningKeyId>)>
          callback);

  // Callback after unwrapping a session key. `on_access_callback` is
  // used to notify the browser that this request led to usage of a
  // session. If restoring the key succeeds, calls `callback` with
  // the key. If restoring the key fails, deletes the session and calls
  // `callback` with `std::nullopt`.
  void OnSessionKeyRestored(
      const SessionKey& session_key,
      OnAccessCallback on_access_callback,
      base::OnceCallback<void(
          std::optional<unexportable_keys::UnexportableSigningKeyId>)> callback,
      Session::KeyIdOrError key_id_or_error);

  // Helper function for starting a refresh.
  void StartSessionRefresh(const SessionKey& session_key, RefreshParams params);

  void RefreshSessionInternal(
      RefreshParams params,
      const SessionKey& session_key,
      std::optional<unexportable_keys::UnexportableSigningKeyId> key_id);

  // Add a header to `request` indicating which sessions should have
  // applied, but did not due to error conditions.
  void AddDebugHeader(const DbscRequest& request);

  // Removes `fetcher` from the set of active fetchers. If `fetcher` is
  // null, does nothing.
  void RemoveFetcher(RegistrationFetcher* fetcher);

  // Asynchronously get the federated provider session specified by
  // `provider_params`, if allowed.
  void GetFederatedProviderSessionIfValid(
      ProviderRegistrationParams provider_params,
      OnAccessCallback on_access_callback,
      base::OnceCallback<void(SessionErrorOr<Session*>)> callback);

  // Helper for getting the federated provider session. Checks that the
  // key thumbprints maps and runs `callback` with the resulting
  // provider session or an error on mismatch.
  void CheckFederatedProviderKey(
      SessionKey provider_session_key,
      std::string provider_key_thumbprint,
      base::OnceCallback<void(SessionErrorOr<Session*>)> callback,
      std::optional<unexportable_keys::UnexportableSigningKeyId> provider_key);

  void OnAddSessionKeyRestored(
      const SchemefulSite& site,
      SessionParams params,
      base::OnceCallback<void(SessionError::ErrorType)> callback,
      unexportable_keys::ServiceErrorOr<
          unexportable_keys::UnexportableSigningKeyId> key_or_error);

  base::expected<std::unique_ptr<Session>, SessionError::ErrorType>
  CreateSessionFromUnexportableKey(
      SessionParams params,
      unexportable_keys::ServiceErrorOr<
          unexportable_keys::UnexportableSigningKeyId> key_or_error);

  // If `minimum_cookie_lifetime` is small enough and there are no
  // pending refreshes for `session_key`, start a proactive refresh.
  void MaybeStartProactiveRefresh(
      SessionService::OnAccessCallback per_request_callback,
      DbscRequest& request,
      const SessionKey& session_key,
      base::TimeDelta minimum_cookie_lifetime);

  // Continuation of `PrewarmSessionsForUrl` after retrieving cookies from the
  // cookie store. Checks each matching session's cookie lifetimes and starts
  // proactive refreshes for any soon-to-expire sessions.
  void OnGetCookiesForPrewarm(const GURL& url,
                              std::vector<SessionKey> matching_sessions,
                              PrewarmCallback callback,
                              const CookieAccessResultList& cookies,
                              const CookieAccessResultList& excluded_cookies);

  // Completes in-flight proactive refresh requests for `session_key`. If the
  // refresh succeeded, fetches cookies to determine
  // `earliest_next_refresh_time` before invoking `callbacks`.
  void CompleteProactiveRefresh(
      const SessionKey& session_key,
      RefreshResult result,
      std::vector<base::OnceCallback<void(PrewarmResult)>> callbacks);

  // Continuation of `UnblockWaitingRequests` for proactive refresh requests
  // after retrieving cookies from the cookie store. Computes
  // `earliest_next_refresh_time` and invokes waiting callbacks.
  void OnGetCookiesAfterProactiveRefresh(
      const SessionKey& session_key,
      std::vector<base::OnceCallback<void(PrewarmResult)>> callbacks,
      RefreshResult refresh_result,
      const CookieAccessResultList& cookies,
      const CookieAccessResultList& excluded_cookies);

  // Barrier callback invoked once all sessions matching a prewarm request have
  // completed their evaluation and any needed proactive refreshes.
  void OnAllPrewarmSessionsDone(PrewarmCallback callback,
                                std::vector<PrewarmResult> session_results);

  void StartRegistration(RegistrationParams registration_params,
                         RegistrationType registration_type_for_histograms,
                         FetchStarter fetch_starter);

  void RegisterStandaloneBoundSession(RegistrationParams registration_params);

  void RegisterSingleSignOnBoundSession(
      RegistrationParams registration_params,
      unexportable_keys::UnexportableSigningKeyId pre_provisioned_key);

  void RegisterFederatedBoundSession(
      RegistrationParams registration_params,
      SessionErrorOr<Session*> federated_provider_session);

  ChallengeResult SetChallengeForBoundSessionInternal(
      OnAccessCallback on_access_callback,
      DbscRequest& request,
      const FirstPartySetMetadata& first_party_set_metadata,
      const SessionChallengeParam& param);

  // Helper to notify event listeners about an event only if they exist.
  void NotifyIfEventCallbackListeners(
      base::FunctionRef<SessionEvent()> event_creator);

  // Helper function to check if a pre-provisioned key created by
  // `provider_url` can be added to the in-memory store.
  //
  // It checks if IdP origin has access to its cookies from a third-party
  // context and the number of existing pre-provisioned keys for the IdP.
  bool CanAddPreProvisionedKey(const GURL& provider_url,
                               const url::Origin& rp_origin);

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
  // List of sites that are restricted from starting Device Bound
  // Session Credential sessions unless
  // `kDeviceBoundSessionRestrictedSites` is enabled.
  std::vector<SchemefulSite> restricted_sites_;
  const SelectClientCertificateHandler client_cert_handler_;

  // When true, the signing quota is not enforced. This is only ever set to
  // true for testing purposes.
  bool ignore_signing_quota_ = false;

  // Deferred requests are stored by session key.
  DeferredRequestsMap deferred_requests_;

  // Proactive refresh requests, stored by session key.
  ProactiveRefreshMap proactive_requests_;

  // Storage is similar to how CookieMonster stores its cookies.
  SessionsMap unpartitioned_sessions_;

  // In-memory store of pre-provisioned keys.
  std::vector<PreProvisionedKeyEntry> pre_provisioned_keys_;

  // All observers of sessions.
  absl::flat_hash_map<net::SchemefulSite, ObserverSet> observers_by_site_;

  // Observers for DBSC events. Used for DevTools.
  base::RepeatingCallbackList<void(const SessionEvent&)> event_callbacks_;

  // Per-site record of the most recent refresh result. This is used
  // for histograms.
  absl::flat_hash_map<net::SchemefulSite, SessionError> refresh_last_result_;

  // Per-site session signing quota. In order to be robust across
  // session parameter changes, we enforce signing quota for a site.
  // This is updated whenever a site triggers signing.
  //
  // NOTE: We use `base::Time` instead of `base::TimeTicks` because
  // `base::TimeTicks` pauses during system sleep on macOS
  // (crbug.com/489704854), which would prevent the quota from decaying
  // overnight.
  absl::flat_hash_map<net::SchemefulSite, std::vector<base::Time>>
      signing_times_;

  // The latest signed challenges per session.
  LatestSignedRefreshChallengesMap latest_signed_refresh_challenges_;

  // Holds all currently live registration fetchers.
  absl::flat_hash_set<std::unique_ptr<RegistrationFetcher>>
      registration_fetchers_;

  // Callback to check if storage access is allowed.
  CookieAccessCallback has_cookie_access_cb_;

  base::WeakPtrFactory<SessionServiceImpl> weak_factory_{this};
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_SERVICE_IMPL_H_
