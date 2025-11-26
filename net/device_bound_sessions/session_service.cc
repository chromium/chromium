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

}  // namespace net::device_bound_sessions
