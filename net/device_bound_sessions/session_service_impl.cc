// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_service_impl.h"

#include "base/functional/bind.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "net/base/schemeful_site.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

namespace net::device_bound_sessions {

SessionServiceImpl::SessionServiceImpl(
    unexportable_keys::UnexportableKeyService& key_service,
    const URLRequestContext* request_context)
    : key_service_(key_service), context_(request_context) {
  CHECK(context_);
}

SessionServiceImpl::~SessionServiceImpl() = default;

void SessionServiceImpl::RegisterBoundSession(
    RegistrationFetcherParam registration_params,
    const IsolationInfo& isolation_info) {
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(registration_params), key_service_.get(), context_.get(),
      isolation_info,
      base::BindOnce(&SessionServiceImpl::OnRegistrationComplete,
                     weak_factory_.GetWeakPtr()));
}

void SessionServiceImpl::OnRegistrationComplete(
    std::optional<RegistrationFetcher::RegistrationCompleteParams> params) {
  if (!params) {
    return;
  }

  auto session = Session::CreateIfValid(std::move(params->params), params->url);
  if (session) {
    unpartitioned_sessions_.insert(std::make_pair(
        SchemefulSite(url::Origin::Create(params->url)), std::move(session)));
  }
  // Call Session::CreateIfValid(params). This callback will also need to take
  // the original request's info (in order to store the IsolationInfo etc).
  // Add the created session to the appropriate map, overwriting any existing
  // one that has the same SessionId.
}

std::optional<Session::Id> SessionServiceImpl::GetAnySessionRequiringDeferral(
    URLRequest* request) {
  SchemefulSite site(request->url());
  auto range = unpartitioned_sessions_.equal_range(site);
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second->ShouldDeferRequest(request)) {
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
    const GURL& request_url,
    const SessionChallengeParam& param) {
  if (!param.session_id()) {
    return;
  }

  SchemefulSite site(request_url);
  auto range = unpartitioned_sessions_.equal_range(site);
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second->id().value() == param.session_id()) {
      it->second->set_cached_challenge(param.challenge());
      return;
    }
  }
}

const Session* SessionServiceImpl::GetSessionForTesting(
    const SchemefulSite& site,
    const std::string& session_id) const {
  auto range = unpartitioned_sessions_.equal_range(site);
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second->id().value() == session_id) {
      return it->second.get();
    }
  }

  return nullptr;
}

}  // namespace net::device_bound_sessions
