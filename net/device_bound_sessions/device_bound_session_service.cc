// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/device_bound_session_service.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "net/base/features.h"
#include "net/device_bound_sessions/registration_fetcher.h"
#include "net/device_bound_sessions/unexportable_key_service_factory.h"

namespace net {

namespace {
class DeviceBoundSessionServiceImpl : public DeviceBoundSessionService {
 public:
  DeviceBoundSessionServiceImpl(
      unexportable_keys::UnexportableKeyService& key_service,
      const URLRequestContext* request_context)
      : key_service_(key_service), context_(request_context) {
    CHECK(context_);
  }

  void RegisterBoundSession(
      DeviceBoundSessionRegistrationFetcherParam registration_params,
      const IsolationInfo& isolation_info) override {
    RegistrationFetcher::StartCreateTokenAndFetch(
        std::move(registration_params), key_service_.get(), context_.get(),
        isolation_info,
        base::BindOnce(&DeviceBoundSessionServiceImpl::OnRegistrationComplete,
                       weak_factory_.GetWeakPtr()));
  }

  // TODO(kristianm): Parse the registration params and create a session
  // in the service.
  void OnRegistrationComplete(std::optional<DeviceBoundSessionParams> params) {}

 private:
  const raw_ref<unexportable_keys::UnexportableKeyService> key_service_;
  raw_ptr<const URLRequestContext> context_;
  base::WeakPtrFactory<DeviceBoundSessionServiceImpl> weak_factory_{this};
};
}  // namespace

std::unique_ptr<DeviceBoundSessionService> DeviceBoundSessionService::Create(
    const URLRequestContext* request_context) {
  unexportable_keys::UnexportableKeyService* service =
      UnexportableKeyServiceFactory::GetInstance()->GetShared();
  if (!service) {
    return nullptr;
  }

  return std::make_unique<DeviceBoundSessionServiceImpl>(*service,
                                                         request_context);
}

}  // namespace net
