// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_service.h"

#include <memory>

#include "net/base/features.h"
#include "net/device_bound_sessions/registration_fetcher.h"
#include "net/device_bound_sessions/session.h"
#include "net/device_bound_sessions/session_service_impl.h"
#include "net/device_bound_sessions/unexportable_key_service_factory.h"
#include "net/url_request/url_request_context.h"

namespace net::device_bound_sessions {

SessionService::DeferralParams::DeferralParams()
    : is_pending_initialization(true), session_id(std::nullopt) {}
SessionService::DeferralParams::DeferralParams(Session::Id session_id)
    : is_pending_initialization(false), session_id(std::move(session_id)) {}
SessionService::DeferralParams::~DeferralParams() = default;

SessionService::DeferralParams::DeferralParams(
    const SessionService::DeferralParams&) = default;
SessionService::DeferralParams& SessionService::DeferralParams::operator=(
    const SessionService::DeferralParams&) = default;

SessionService::DeferralParams::DeferralParams(
    SessionService::DeferralParams&&) = default;
SessionService::DeferralParams& SessionService::DeferralParams::operator=(
    SessionService::DeferralParams&&) = default;

std::unique_ptr<SessionService> SessionService::Create(
    const URLRequestContext* request_context) {
#if BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)
  unexportable_keys::UnexportableKeyService* service;
  if (request_context->unexportable_key_service()) {
    service = request_context->unexportable_key_service();
  } else {
    service = UnexportableKeyServiceFactory::GetInstance()->GetShared();
  }
  if (!service) {
    return nullptr;
  }

  SessionStore* session_store = request_context->device_bound_session_store();
  auto session_service = std::make_unique<SessionServiceImpl>(
      *service, request_context, session_store);
  // Loads saved sessions if `session_store` is not null.
  session_service->LoadSessionsAsync();
  return session_service;
#else
  return nullptr;
#endif
}

void SessionService::HandleResponseHeaders(
    DbscRequest& request,
    HttpResponseHeaders* headers,
    const FirstPartySetMetadata& first_party_set_metadata) {
  const auto& request_url = request.url();

  // If response header Sec-Session-Registration is present and configured
  // appropriately, trigger a registration request per header value to attempt
  // to create a new session.
  if (request.allows_device_bound_session_registration() ||
      !features::kDeviceBoundSessionsRequireOriginTrialTokens.Get()) {
    std::vector<device_bound_sessions::RegistrationFetcherParam> params =
        device_bound_sessions::RegistrationFetcherParam::CreateIfValid(
            request_url, headers);
    for (auto& param : params) {
      RegisterBoundSession(request.device_bound_session_access_callback(),
                           std::move(param), request.isolation_info(),
                           request.net_log(), request.initiator());
    }
  }

  // If response header Sec-Session-Challenge is present and configured
  // appropriately, for each header value, store the challenge in advance for
  // the next relevant refresh request that gets triggered. This is to help
  // avoid a round-trip for when the next refresh request is required.
  std::vector<device_bound_sessions::SessionChallengeParam> challenge_params =
      device_bound_sessions::SessionChallengeParam::CreateIfValid(request_url,
                                                                  headers);
  for (auto& param : challenge_params) {
    SetChallengeForBoundSession(request.device_bound_session_access_callback(),
                                request, first_party_set_metadata,
                                std::move(param));
  }
}

}  // namespace net::device_bound_sessions
