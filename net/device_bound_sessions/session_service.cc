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

namespace net::device_bound_sessions {

std::unique_ptr<SessionService> SessionService::Create(
    const URLRequestContext* request_context) {
  unexportable_keys::UnexportableKeyService* service =
      UnexportableKeyServiceFactory::GetInstance()->GetShared();
  if (!service) {
    return nullptr;
  }

  return std::make_unique<SessionServiceImpl>(*service, request_context);
}

}  // namespace net::device_bound_sessions
