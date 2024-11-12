// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_service_impl.h"

#include "base/functional/bind.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "net/base/schemeful_site.h"
#include "net/device_bound_sessions/registration_request_param.h"
#include "net/device_bound_sessions/session_store.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

namespace net::device_bound_sessions {

namespace {

void NotifySessionAccess(SessionService::OnAccessCallback callback,
                         const SchemefulSite& site,
                         const Session& session) {
  if (callback.is_null()) {
    return;
  }

  callback.Run({site, session.id()});
}

}  // namespace

SessionServiceImpl::SessionServiceImpl(
    unexportable_keys::UnexportableKeyService& key_service,
    const URLRequestContext* request_context,
    SessionStore* store)
    : key_service_(key_service),
      context_(request_context),
      session_store_(store) {
  CHECK(context_);
}

SessionServiceImpl::~SessionServiceImpl() = default;

void SessionServiceImpl::LoadSessionsAsync() {
  if (!session_store_) {
    return;
  }
  session_store_->LoadSessions(base::BindOnce(
      &SessionServiceImpl::OnLoadSessionsComplete, weak_factory_.GetWeakPtr()));
}

void SessionServiceImpl::RegisterBoundSession(
    OnAccessCallback on_access_callback,
    RegistrationFetcherParam registration_params,
    const IsolationInfo& isolation_info) {
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(registration_params), key_service_.get(), context_.get(),
      isolation_info,
      base::BindOnce(&SessionServiceImpl::OnRegistrationComplete,
                     weak_factory_.GetWeakPtr(),
                     std::move(on_access_callback)));
}

void SessionServiceImpl::OnLoadSessionsComplete(
    SessionStore::SessionsMap sessions) {
  unpartitioned_sessions_.merge(sessions);
}

void SessionServiceImpl::OnRegistrationComplete(
    OnAccessCallback on_access_callback,
    std::optional<RegistrationFetcher::RegistrationCompleteParams> params) {
  if (!params) {
    return;
  }

  auto session = Session::CreateIfValid(std::move(params->params), params->url);
  if (!session) {
    return;
  }
  session->set_unexportable_key_id(std::move(params->key_id));

  const SchemefulSite site(url::Origin::Create(params->url));
  NotifySessionAccess(on_access_callback, site, *session);

  // Clear the existing session which initiated the registration.
  if (params->referral_session_identifier) {
    ClearSession(site,
                 Session::Id(std::move(*params->referral_session_identifier)));
  }
  AddSession(site, std::move(session));
}

std::pair<SessionServiceImpl::SessionsMap::iterator,
          SessionServiceImpl::SessionsMap::iterator>
SessionServiceImpl::GetSessionsForSite(const SchemefulSite& site) {
  const auto now = base::Time::Now();
  auto [begin, end] = unpartitioned_sessions_.equal_range(site);
  for (auto it = begin; it != end;) {
    if (now >= it->second->expiry_date()) {
      it = ClearSessionInternal(site, it);
    } else {
      it->second->RecordAccess();
      it++;
    }
  }

  return unpartitioned_sessions_.equal_range(site);
}

std::optional<Session::Id> SessionServiceImpl::GetAnySessionRequiringDeferral(
    URLRequest* request) {
  SchemefulSite site(request->url());
  auto range = GetSessionsForSite(site);
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second->ShouldDeferRequest(request)) {
      NotifySessionAccess(request->device_bound_session_access_callback(), site,
                          *it->second);
      return it->second->id();
    }
  }

  return std::nullopt;
}

// TODO(kristianm): Actually send the refresh request, for now continue
// with sending the deferred request right away.
void SessionServiceImpl::DeferRequestForRefresh(
    URLRequest* request,
    Session::Id session_id,
    RefreshCompleteCallback restart_callback,
    RefreshCompleteCallback continue_callback) {
  CHECK(restart_callback);
  CHECK(continue_callback);
  std::move(continue_callback).Run();
}

void SessionServiceImpl::SetChallengeForBoundSession(
    OnAccessCallback on_access_callback,
    const GURL& request_url,
    const SessionChallengeParam& param) {
  if (!param.session_id()) {
    return;
  }

  SchemefulSite site(request_url);
  auto range = GetSessionsForSite(site);
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second->id().value() == param.session_id()) {
      NotifySessionAccess(on_access_callback, site, *it->second);
      it->second->set_cached_challenge(param.challenge());
      return;
    }
  }
}

Session* SessionServiceImpl::GetSessionForTesting(
    const SchemefulSite& site,
    const std::string& session_id) const {
  // Intentionally do not use `GetSessionsForSite` here so we do not
  // modify the session during testing.
  auto range = unpartitioned_sessions_.equal_range(site);
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second->id().value() == session_id) {
      return it->second.get();
    }
  }

  return nullptr;
}

void SessionServiceImpl::AddSession(const SchemefulSite& site,
                                    std::unique_ptr<Session> session) {
  if (session_store_) {
    session_store_->SaveSession(site, *session);
  }
  // TODO(crbug.com/353774923): Enforce unique session ids per site.
  unpartitioned_sessions_.emplace(site, std::move(session));
}

void SessionServiceImpl::ClearSession(const SchemefulSite& site,
                                      const Session::Id& id) {
  auto range = unpartitioned_sessions_.equal_range(site);
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second->id() == id) {
      it = ClearSessionInternal(site, it);
      return;
    }
  }
}

SessionServiceImpl::SessionsMap::iterator
SessionServiceImpl::ClearSessionInternal(
    const SchemefulSite& site,
    SessionServiceImpl::SessionsMap::iterator it) {
  if (session_store_) {
    session_store_->DeleteSession(site, it->second->id());
  }

  // TODO(crbug.com/353774923): Clear BFCache entries for this session.
  return unpartitioned_sessions_.erase(it);
}

void SessionServiceImpl::StartSessionRefresh(
    const Session& session,
    const IsolationInfo& isolation_info,
    OnAccessCallback on_access_callback) {
  const Session::KeyIdOrError& key_id = session.unexportable_key_id();
  if (!key_id.has_value()) {
    return;
  }

  auto request_params = RegistrationRequestParam::Create(session);
  RegistrationFetcher::StartFetchWithExistingKey(
      std::move(request_params), key_service_.get(), context_.get(),
      isolation_info,
      base::BindOnce(&SessionServiceImpl::OnRegistrationComplete,
                     weak_factory_.GetWeakPtr(), std::move(on_access_callback)),
      *key_id);
}

}  // namespace net::device_bound_sessions
