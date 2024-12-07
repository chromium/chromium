// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_service_impl.h"

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
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

DeferredURLRequest::DeferredURLRequest(
    const URLRequest* request,
    SessionService::RefreshCompleteCallback restart_callback,
    SessionService::RefreshCompleteCallback continue_callback)
    : request(request),
      restart_callback(std::move(restart_callback)),
      continue_callback(std::move(continue_callback)) {}

DeferredURLRequest::DeferredURLRequest(DeferredURLRequest&& other) noexcept =
    default;

DeferredURLRequest& DeferredURLRequest::operator=(
    DeferredURLRequest&& other) noexcept = default;

DeferredURLRequest::~DeferredURLRequest() = default;

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
  pending_initialization_ = true;
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
  pending_initialization_ = false;

  std::vector<base::OnceClosure> queued_operations =
      std::move(queued_operations_);
  for (base::OnceClosure& closure : queued_operations) {
    std::move(closure).Run();
  }
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

  AddSession(site, std::move(session));
}

std::pair<SessionServiceImpl::SessionsMap::iterator,
          SessionServiceImpl::SessionsMap::iterator>
SessionServiceImpl::GetSessionsForSite(const SchemefulSite& site) {
  const auto now = base::Time::Now();
  auto [begin, end] = unpartitioned_sessions_.equal_range(site);
  for (auto it = begin; it != end;) {
    if (now >= it->second->expiry_date()) {
      it = DeleteSessionInternal(site, it);
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

// Actually send the refresh request, for now continue with sending the deferred
// request right away.
void SessionServiceImpl::DeferRequestForRefresh(
    URLRequest* request,
    Session::Id session_id,
    RefreshCompleteCallback restart_callback,
    RefreshCompleteCallback continue_callback) {
  CHECK(restart_callback);
  CHECK(continue_callback);
  CHECK(request);
  bool needs_refresh = false;
  // For the first deferring request, create a new vector and add the request.
  auto [it, inserted] = deferred_requests_.try_emplace(session_id);
  if (inserted) {
    needs_refresh = true;
  }
  // Add the request to the deferred list.
  it->second.emplace_back(request, std::move(restart_callback),
                          std::move(continue_callback));

  SchemefulSite site(request->url());
  auto* session = GetSession(site, session_id);
  if (!session) {
    // If we can't find the session, clear the session_id key in the map and
    // continue all related requests.
    UnblockDeferredRequests(session_id, /*is_cookie_refreshed=*/false);
    return;
  }
  // Notify the request that it has been deferred for refreshed cookies.
  NotifySessionAccess(request->device_bound_session_access_callback(), site,
                      *session);
  // Do refresh the session.
  if (needs_refresh) {
    const Session::KeyIdOrError& key_id = session->unexportable_key_id();
    if (!key_id.has_value()) {
      UnblockDeferredRequests(session_id, /*is_cookie_refreshed=*/false);
      return;
    }
    auto callback =
        base::BindOnce(&SessionServiceImpl::OnRefreshRequestCompletion,
                       weak_factory_.GetWeakPtr(), std::move(site), session_id);
    RegistrationFetcher::StartFetchWithExistingKey(
        RegistrationRequestParam::Create(*session), key_service_.get(),
        context_.get(), request->isolation_info(), std::move(callback),
        *key_id);
  }
}

void SessionServiceImpl::OnRefreshRequestCompletion(
    SchemefulSite site,
    Session::Id session_id,
    std::optional<RegistrationFetcher::RegistrationCompleteParams>
        refresh_result) {
  // Refresh succeeded:
  // 1. update the session by adding a new session and deleting the old one
  // 2. restart the deferred requests.
  // TODO(crbug.com/353766139): check if add/delete update will cause some race,
  // for example, if the the old session_id is still in use while deleting it.
  // Is it service's responsibility to keep the session_id same with the one in
  // received JSON which parsed as result_result->params?
  if (refresh_result) {
    auto new_session = Session::CreateIfValid(std::move(refresh_result->params),
                                              refresh_result->url);
    if (new_session) {
      new_session->set_unexportable_key_id(std::move(refresh_result->key_id));
      // Delete old session.
      DeleteSession(site, session_id);
      // Add the new session.
      AddSession(SchemefulSite(url::Origin::Create(refresh_result->url)),
                 std::move(new_session));
      // The session has been refreshed, restart the request.
      UnblockDeferredRequests(session_id, /*is_cookie_refreshed=*/true);
      return;
    }
  }

  // Refresh failed:
  // 1. Clear the existing session which initiated the refresh flow.
  // 2. continue all deferred requests.
  // TODO(crbug.com/353766139): Do we need a retry mechanism?
  DeleteSession(site, session_id);
  UnblockDeferredRequests(session_id, /*is_cookie_refreshed=*/false);
}

// Continue or restart all deferred requests for the session and remove the
// session_id key in the map.
void SessionServiceImpl::UnblockDeferredRequests(const Session::Id& session_id,
                                                 bool is_cookie_refreshed) {
  auto it = deferred_requests_.find(session_id);
  if (it == deferred_requests_.end()) {
    return;
  }

  auto requests = std::move(it->second);
  deferred_requests_.erase(it);

  for (auto& request : requests) {
    if (is_cookie_refreshed) {
      std::move(request.restart_callback).Run();
    } else {
      std::move(request.continue_callback).Run();
    }
  }
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

void SessionServiceImpl::GetAllSessionsAsync(
    base::OnceCallback<void(const std::vector<SessionKey>&)> callback) {
  if (pending_initialization_) {
    queued_operations_.push_back(base::BindOnce(
        &SessionServiceImpl::GetAllSessionsAsync,
        // `base::Unretained` is safe because the callback is stored in
        // `queued_operations_`, which is owned by `this`.
        base::Unretained(this), std::move(callback)));
  } else {
    std::vector<SessionKey> sessions =
        base::ToVector(unpartitioned_sessions_, [](const auto& pair) {
          const auto& [site, session] = pair;
          return SessionKey(site, session->id());
        });
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(sessions)));
  }
}

Session* SessionServiceImpl::GetSession(const SchemefulSite& site,
                                        const Session::Id& session_id) const {
  // Intentionally do not use `GetSessionsForSite` here so we do not
  // modify the session during testing.
  auto range = unpartitioned_sessions_.equal_range(site);
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second->id() == session_id) {
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

void SessionServiceImpl::DeleteSession(const SchemefulSite& site,
                                       const Session::Id& id) {
  auto range = unpartitioned_sessions_.equal_range(site);
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second->id() == id) {
      std::ignore = DeleteSessionInternal(site, it);
      return;
    }
  }
}

SessionServiceImpl::SessionsMap::iterator
SessionServiceImpl::DeleteSessionInternal(
    const SchemefulSite& site,
    SessionServiceImpl::SessionsMap::iterator it) {
  if (session_store_) {
    session_store_->DeleteSession(site, it->second->id());
  }

  // TODO(crbug.com/353774923): Clear BFCache entries for this session.
  return unpartitioned_sessions_.erase(it);
}

}  // namespace net::device_bound_sessions
