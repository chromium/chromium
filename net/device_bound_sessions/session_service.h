// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_SESSION_SERVICE_H_
#define NET_DEVICE_BOUND_SESSIONS_SESSION_SERVICE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "net/base/net_export.h"
#include "net/device_bound_sessions/deletion_reason.h"
#include "net/device_bound_sessions/registration_fetcher_param.h"
#include "net/device_bound_sessions/session.h"
#include "net/device_bound_sessions/session_access.h"
#include "net/device_bound_sessions/session_challenge_param.h"
#include "net/device_bound_sessions/session_key.h"
#include "net/log/net_log_with_source.h"

namespace net {
class FirstPartySetMetadata;
class IsolationInfo;
class URLRequestContext;
class HttpRequestHeaders;
}  // namespace net

namespace net::device_bound_sessions {

// Main class for Device Bound Session Credentials (DBSC).
// Full information can be found at https://github.com/WICG/dbsc
class NET_EXPORT SessionService {
 public:
  using OnAccessCallback = base::RepeatingCallback<void(const SessionAccess&)>;
  using RefreshCompleteCallback = base::OnceCallback<void(RefreshResult)>;

  // Indicates the reason for deferring. Exactly one of
  // `is_pending_initialization` or `session_id` will be truthy.
  struct NET_EXPORT DeferralParams {
    // Construct with `is_pending_initialization` set to true.
    DeferralParams();

    // Constructor with `session_id` having a value
    explicit DeferralParams(Session::Id session_id);
    ~DeferralParams();

    DeferralParams(const DeferralParams&);
    DeferralParams& operator=(const DeferralParams&);

    DeferralParams(DeferralParams&&);
    DeferralParams& operator=(DeferralParams&&);

    // Set to true when we defer due to missing initialization.
    bool is_pending_initialization;

    // If `is_pending_initialization` is false, we're deferring due to
    // missing credentials on this session.
    std::optional<Session::Id> session_id;
  };

  // Stores a signed refresh challenge as well as the inputs used for the
  // signing. This is an optimization to avoid redundant resigning, which is
  // slow + resource-intensive, and could also cause issues like triggering the
  // signing quota unnecessarily.
  struct NET_EXPORT SignedRefreshChallenge {
    // The signed challenge that was cached.
    std::string signed_challenge;
    // The challenge used to generate `signed_challenge`.
    std::string challenge;
    // The key_id used to generate `signed_challenge`.
    unexportable_keys::UnexportableKeyId key_id;
  };

  // Returns nullptr if unexportable key provider is not supported by the
  // platform or the device.
  static std::unique_ptr<SessionService> Create(
      const URLRequestContext* request_context);

  SessionService(const SessionService&) = delete;
  SessionService& operator=(const SessionService&) = delete;

  virtual ~SessionService() = default;

  // Called to register a new session after getting a
  // Secure-Session-Registration header. Registration parameters to be used for
  // creating the registration request. Isolation info to be used for
  // registration request, this should be the same as was used for the response
  // with the Secure-Session-Registration header. `net_log` is the log
  // corresponding to the request receiving the Secure-Session-Registration
  // header. 'original_request_initiator` was the initiator for the request that
  // received the Secure-Session-Registration header.
  virtual void RegisterBoundSession(
      OnAccessCallback on_access_callback,
      RegistrationFetcherParam registration_params,
      const IsolationInfo& isolation_info,
      const NetLogWithSource& net_log,
      const std::optional<url::Origin>& original_request_initiator) = 0;

  // Check if a request should be deferred due to the session cookie being
  // missing. This should only be called once the request has the correct
  // cookies added to the request.
  // If multiple sessions needs to be refreshed for this request,
  // any of them can be returned.
  // Returns a `DeferralParams` setting `is_pending_initialization` if
  // the request should be deferred while waiting for initialization, a
  // `DeferralParams` containing the session id if the request should be
  // deferred due to a session, and returns std::nullopt if the request
  // does not need to be deferred.
  // If sessions are skipped without deferring, they will be added to
  // the Secure-Session-Skipped header in `extra_headers`.
  virtual std::optional<DeferralParams> ShouldDefer(
      DbscRequest& request,
      HttpRequestHeaders* extra_headers,
      const FirstPartySetMetadata& first_party_set_metadata) = 0;

  // Defer a request and maybe refresh the corresponding session.
  // `deferral` is either the identifier of the session that is required to be
  // refreshed, or indicates the service is not completely initialized.
  // This will refresh the corresponding session if: another deferred request
  // has not already kicked off refresh, the session can be found, and the
  // associated unexportable key id is valid.
  // On completion, calls `callback`.
  virtual void DeferRequestForRefresh(DbscRequest& request,
                                      DeferralParams deferral,
                                      RefreshCompleteCallback callback) = 0;

  // Set the challenge for a bound session after getting a
  // Secure-Session-Challenge header.
  virtual void SetChallengeForBoundSession(
      OnAccessCallback on_access_callback,
      DbscRequest& request,
      const FirstPartySetMetadata& first_party_set_metadata,
      const SessionChallengeParam& param) = 0;

  // Get all sessions. If sessions have not yet been loaded from disk,
  // defer until completely initialized.
  virtual void GetAllSessionsAsync(
      base::OnceCallback<void(const std::vector<SessionKey>&)> callback) = 0;

  // Delete the session matching `session_key`, notifying
  // `per_request_callback` about any deletions.
  virtual void DeleteSessionAndNotify(
      DeletionReason reason,
      const SessionKey& session_key,
      SessionService::OnAccessCallback per_request_callback) = 0;

  // Delete all sessions that match the filtering arguments. See
  // `device_bound_sessions.mojom` for details on the filtering logic.
  virtual void DeleteAllSessions(
      DeletionReason reason,
      std::optional<base::Time> created_after_time,
      std::optional<base::Time> created_before_time,
      base::RepeatingCallback<bool(const url::Origin&,
                                   const net::SchemefulSite&)>
          origin_and_site_matcher,
      base::OnceClosure completion_callback) = 0;

  // Add an observer for session changes that include `url`. `callback`
  // will only be notified until the destruction of the returned
  // `ScopedClosureRunner`.
  virtual base::ScopedClosureRunner AddObserver(
      const GURL& url,
      base::RepeatingCallback<void(const SessionAccess&)> callback) = 0;

  // Get a session by key, or `nullptr` if no such session exists.
  virtual const Session* GetSession(const SessionKey& session_key) const = 0;

  // Adds a session to the service for the site `site` and with session
  // config from `params`. `params.key_id` is ignored in favor of
  // importing `wrapped_key`. Calls `callback` when complete with a a
  // `SessionError` indicating whether session addition was successful.
  virtual void AddSession(
      const SchemefulSite& site,
      SessionParams params,
      base::span<const uint8_t> wrapped_key,
      base::OnceCallback<void(SessionError::ErrorType)> callback) = 0;

  // Finds the latest signed refresh challenge and relevant signing context for
  // the `session_key`. If no challenge is found, returns nullptr.
  virtual const SignedRefreshChallenge* GetLatestSignedRefreshChallenge(
      const SessionKey& session_key) = 0;
  // Sets the latest signed refresh challenge and relevant signing context for
  // the `session_key`.
  virtual void SetLatestSignedRefreshChallenge(
      SessionKey session_key,
      SignedRefreshChallenge signed_refresh_challenge) = 0;

  // Whether the `site` has exceeded its signing quota.
  virtual bool SigningQuotaExceeded(const SchemefulSite& site) = 0;
  // Increments signing usage for this `site`.
  virtual void AddSigningOccurrence(const SchemefulSite& site) = 0;

  // Helper function to handle the registration and challenge headers provided
  // in `headers` on the response to `request`.
  void HandleResponseHeaders(
      DbscRequest& request,
      HttpResponseHeaders* headers,
      const FirstPartySetMetadata& first_party_set_metadata);

 protected:
  SessionService() = default;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_SERVICE_H_
