// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_SESSION_SERVICE_IMPL_H_
#define NET_DEVICE_BOUND_SESSIONS_SESSION_SERVICE_IMPL_H_

#include <map>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"
#include "net/device_bound_sessions/registration_fetcher.h"
#include "net/device_bound_sessions/registration_fetcher_param.h"
#include "net/device_bound_sessions/session.h"
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

class NET_EXPORT SessionServiceImpl : public SessionService {
 public:
  SessionServiceImpl(unexportable_keys::UnexportableKeyService& key_service,
                     const URLRequestContext* request_context);
  ~SessionServiceImpl() override;

  void RegisterBoundSession(RegistrationFetcherParam registration_params,
                            const IsolationInfo& isolation_info) override;

  std::optional<Session::Id> GetAnySessionRequiringDeferral(
      URLRequest* request) override;

  void DeferRequestForRefresh(
      URLRequest* request,
      Session::Id session_id,
      RefreshCompleteCallback restart_callback,
      RefreshCompleteCallback continue_callback) override;

  void SetChallengeForBoundSession(const GURL& request_url,
                                   const SessionChallengeParam& param) override;

  const Session* GetSessionForTesting(const SchemefulSite& site,
                                      const std::string& session_id) const;

 private:
  // The key is the site (eTLD+1) of the session's origin.
  using SessionsMap = std::multimap<SchemefulSite, std::unique_ptr<Session>>;

  void OnRegistrationComplete(
      std::optional<RegistrationFetcher::RegistrationCompleteParams> params);

  const raw_ref<unexportable_keys::UnexportableKeyService> key_service_;
  raw_ptr<const URLRequestContext> context_;

  // Storage is similar to how CookieMonster stores its cookies.
  SessionsMap unpartitioned_sessions_;

  base::WeakPtrFactory<SessionServiceImpl> weak_factory_{this};
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_SERVICE_IMPL_H_
