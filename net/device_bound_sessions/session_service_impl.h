// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_SESSION_SERVICE_IMPL_H_
#define NET_DEVICE_BOUND_SESSIONS_SESSION_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"
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

class NET_EXPORT SessionServiceImpl : public SessionService {
 public:
  SessionServiceImpl(unexportable_keys::UnexportableKeyService& key_service,
                     const URLRequestContext* request_context,
                     SessionStore* store);
  ~SessionServiceImpl() override;

  // Loads saved session data from disk if a `SessionStore` object is provided
  // during construction. Otherwise, it is a no-op.
  void LoadSessionsAsync();

  void RegisterBoundSession(OnAccessCallback on_access_callback,
                            RegistrationFetcherParam registration_params,
                            const IsolationInfo& isolation_info) override;

  std::optional<Session::Id> GetAnySessionRequiringDeferral(
      URLRequest* request) override;

  void DeferRequestForRefresh(
      URLRequest* request,
      Session::Id session_id,
      RefreshCompleteCallback restart_callback,
      RefreshCompleteCallback continue_callback) override;

  void SetChallengeForBoundSession(OnAccessCallback on_access_callback,
                                   const GURL& request_url,
                                   const SessionChallengeParam& param) override;

  Session* GetSessionForTesting(const SchemefulSite& site,
                                const std::string& session_id) const;

 private:
  friend class SessionServiceImplWithStoreTest;

  // The key is the site (eTLD+1) of the session's origin.
  using SessionsMap = std::multimap<SchemefulSite, std::unique_ptr<Session>>;

  void OnLoadSessionsComplete(SessionsMap sessions);

  void OnRegistrationComplete(
      OnAccessCallback on_access_callback,
      std::optional<RegistrationFetcher::RegistrationCompleteParams> params);

  void StartSessionRefresh(
      const Session& session,
      const IsolationInfo& isolation_info,
      // TODO(crbug.com/353764893): Replace this callback placeholder.
      OnAccessCallback on_access_callback);
  void AddSession(const SchemefulSite& site, std::unique_ptr<Session> session);
  void ClearSession(const SchemefulSite& site, const Session::Id& id);

  // Get all the unexpired sessions for a given site. This also removes
  // expired sessions for the site and extends the TTL of used sessions.
  std::pair<SessionsMap::iterator, SessionsMap::iterator> GetSessionsForSite(
      const SchemefulSite& site);

  // Remove a session from the session map. It also clears the session from
  // `session_store_` and any BFCache entries.
  // Return the iterator to the next session in the map.
  [[nodiscard]] SessionsMap::iterator ClearSessionInternal(
      const SchemefulSite& site,
      SessionsMap::iterator it);

  const raw_ref<unexportable_keys::UnexportableKeyService> key_service_;
  raw_ptr<const URLRequestContext> context_;
  raw_ptr<SessionStore> session_store_ = nullptr;

  // Storage is similar to how CookieMonster stores its cookies.
  SessionsMap unpartitioned_sessions_;

  base::WeakPtrFactory<SessionServiceImpl> weak_factory_{this};
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_SERVICE_IMPL_H_
