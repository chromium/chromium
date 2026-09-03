// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_SESSION_SERVICE_H_
#define NET_DEVICE_BOUND_SESSIONS_SESSION_SERVICE_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "net/base/net_export.h"
#include "net/device_bound_sessions/cookie_access_check_params.h"
#include "net/device_bound_sessions/deletion_reason.h"
#include "net/device_bound_sessions/refresh_result.h"
#include "net/device_bound_sessions/registration_fetcher_param.h"
#include "net/device_bound_sessions/session.h"
#include "net/device_bound_sessions/session_access.h"
#include "net/device_bound_sessions/session_challenge_param.h"
#include "net/device_bound_sessions/session_display.h"
#include "net/device_bound_sessions/session_error.h"
#include "net/device_bound_sessions/session_event.h"
#include "net/device_bound_sessions/session_key.h"
#include "net/log/net_log_with_source.h"

namespace net {
class FirstPartySetMetadata;
class IsolationInfo;
class SiteForCookies;
class URLRequestContext;
class HttpRequestHeaders;
class SSLCertRequestInfo;
class X509Certificate;
class SSLPrivateKey;
}  // namespace net

namespace net::device_bound_sessions {

// Result of prewarming DBSC sessions for a URL.
struct NET_EXPORT SessionPrewarmResult {
  std::vector<RefreshResult> results;
  base::Time earliest_next_refresh_time = base::Time::Max();

  bool operator==(const SessionPrewarmResult&) const = default;
};

// Callback invoked when a client certificate selection has finished.
// `cert` and `key` are the selected certificate and its private key. Both are
// null if no certificate was selected or the request was cancelled.
// `cancel` is true if the request should be aborted (e.g. user cancelled the
// prompt), or false if it should continue (either with a certificate or without
// one).
using SelectClientCertificateCallback =
    base::OnceCallback<void(scoped_refptr<X509Certificate> cert,
                            scoped_refptr<SSLPrivateKey> key,
                            bool cancel)>;

// Handler invoked by the SessionService to select a client certificate for a
// Device Bound session request (registration or refresh).
// When the certificate selection is complete, the handler must run the
// provided `callback`.
using SelectClientCertificateHandler =
    base::RepeatingCallback<void(const GURL& url,
                                 scoped_refptr<SSLCertRequestInfo> cert_info,
                                 SelectClientCertificateCallback callback)>;

// Main class for Device Bound Session Credentials (DBSC).
// Full information can be found at https://github.com/WICG/dbsc
class NET_EXPORT SessionService {
 public:
  using OnAccessCallback = base::RepeatingCallback<void(const SessionAccess&)>;
  using OnEventCallback = base::RepeatingCallback<void(const SessionEvent&)>;
  using RefreshCompleteCallback = base::OnceCallback<void(RefreshResult)>;
  using CookieAccessCallback =
      base::RepeatingCallback<bool(const CookieAccessCheckParams&)>;
  using PrewarmCallback = base::OnceCallback<void(SessionPrewarmResult)>;

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
    unexportable_keys::UnexportableSigningKeyId key_id;
  };

  // Returns nullptr if unexportable key provider is not supported by the
  // platform or the device.
  static std::unique_ptr<SessionService> Create(
      const URLRequestContext* request_context,
      const std::vector<SchemefulSite>& restricted_sites,
      SelectClientCertificateHandler client_cert_handler,
      CookieAccessCallback has_cookie_access_cb = base::NullCallback());

  SessionService(const SessionService&) = delete;
  SessionService& operator=(const SessionService&) = delete;

  virtual ~SessionService() = default;

  // Called to register a new session after getting a
  // Sec-Session-Registration header.
  // - `on_access_callback`: Callback invoked when a session is successfully
  //   created. Not invoked if registration fails.
  // - `registration_params`: Parameters parsed from the
  //   Sec-Session-Registration header used for creating the registration
  //   request.
  //
  // The following parameters configure the registration request and they should
  // correspond to the request that received the Sec-Session-Registration
  // header:
  // - `isolation_info`
  // - `site_for_cookies`
  // - `net_log`
  // - `original_request_initiator`
  virtual void RegisterBoundSession(
      OnAccessCallback on_access_callback,
      RegistrationFetcherParam registration_params,
      const IsolationInfo& isolation_info,
      const net::SiteForCookies& site_for_cookies,
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

  // Get all sessions and return a list of display sessions. If sessions
  // have not yet been loaded from disk, defer until completely initialized.
  virtual void GetAllSessionDisplaysAsync(
      base::OnceCallback<void(const std::vector<SessionDisplay>&)>
          callback) = 0;

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

  // Adds a pre-provisioned key to an in-memory storage.
  //
  // Returns `true` if the key was successfully added, `false` otherwise.
  //
  // A key insertion can fail if the key already exists, i.e. same rp_origin,
  // provider_key, and provider_url.
  //
  // A key insertion can fail if there are more than
  // `kMaxPreProvisionedKeysPerIdentityProvider` keys per Identity Provider
  // site.
  //
  // A key insertion can fail if the Identity Provider site does not have access
  // to its cookies from a third-party context.
  virtual bool AddPreProvisionedKey(
      const url::Origin& rp_origin,
      std::string_view provider_key,
      const GURL& provider_url,
      unexportable_keys::UnexportableSigningKeyId key_id) = 0;

  // Find a pre-provisioned key that matches the parameters.
  virtual SessionErrorOr<unexportable_keys::UnexportableSigningKeyId>
  FindPreProvisionedKey(
      const ProviderRegistrationParams& provider_params,
      base::optional_ref<const url::Origin> original_request_initiator) = 0;

  // Add an observer for session changes that include `url`. `callback`
  // will only be notified until the destruction of the returned
  // `ScopedClosureRunner`.
  virtual base::ScopedClosureRunner AddObserver(
      const GURL& url,
      base::RepeatingCallback<void(const SessionAccess&)> callback) = 0;

  // Add an observer for DBSC events. This is used for DevTools.
  virtual base::CallbackListSubscription AddEventObserver(
      OnEventCallback callback) = 0;

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
  virtual void HandleResponseHeaders(
      DbscRequest& request,
      HttpResponseHeaders* headers,
      const FirstPartySetMetadata& first_party_set_metadata) = 0;

  virtual void SelectClientCertificate(
      const GURL& url,
      scoped_refptr<SSLCertRequestInfo> cert_info,
      SelectClientCertificateCallback callback) = 0;

  // Evaluates all DBSC sessions matching `url` to determine if their required
  // cookies are missing or expiring soon. If a refresh is needed, initiates
  // background proactive refreshes and invokes `callback` asynchronously when
  // all concurrent evaluations and refreshes complete, passing a
  // `SessionPrewarmResult` containing a vector of `RefreshResult` outcomes and
  // an absolute `base::Time` timestamp for `earliest_next_refresh_time`. If
  // `url` is invalid or no matching sessions are found, `callback` is invoked
  // asynchronously with an empty `SessionPrewarmResult`.
  virtual void PrewarmSessionsForUrl(const GURL& url,
                                     PrewarmCallback callback) = 0;

 protected:
  SessionService() = default;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_SERVICE_H_
