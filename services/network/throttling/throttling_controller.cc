// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/throttling/throttling_controller.h"

#include "net/http/http_request_info.h"
#include "services/network/throttling/network_conditions.h"
#include "services/network/throttling/scoped_throttling_token.h"
#include "services/network/throttling/throttling_network_interceptor.h"

namespace network {

ThrottlingController* ThrottlingController::instance_ = nullptr;

ThrottlingController::ThrottlingController() = default;
ThrottlingController::~ThrottlingController() {
  liveness_ = Liveness::kDead;
}

// static
void ThrottlingController::SetConditions(
    const base::UnguessableToken& throttling_profile_id,
    std::unique_ptr<NetworkConditions> conditions) {
  if (!instance_) {
    if (!conditions)
      return;
    instance_ = new ThrottlingController();
  }
  instance_->SetNetworkConditions(throttling_profile_id, std::move(conditions));
}

// static
ThrottlingNetworkInterceptor* ThrottlingController::GetInterceptor(
    uint32_t net_log_source_id) {
  if (!instance_)
    return nullptr;
  return instance_->FindInterceptor(net_log_source_id);
}

// static
void ThrottlingController::RegisterProfileIDForNetLogSource(
    uint32_t net_log_source_id,
    const base::UnguessableToken& throttling_profile_id) {
  if (!instance_)
    return;
  instance_->Register(net_log_source_id, throttling_profile_id);
}

// static
void ThrottlingController::UnregisterNetLogSource(uint32_t net_log_source_id) {
  if (instance_)
    instance_->Unregister(net_log_source_id);
}

// static
bool ThrottlingController::HasInterceptor(
    const base::UnguessableToken& throttling_profile_id) {
  // Null |instance_| means there is no network condition registered.
  if (!instance_)
    return false;
  instance_->CheckValidThread();
  return instance_->interceptors_.find(throttling_profile_id) !=
         instance_->interceptors_.end();
}

void ThrottlingController::Register(
    uint32_t net_log_source_id,
    const base::UnguessableToken& throttling_profile_id) {
  CheckValidThread();
  if (interceptors_.find(throttling_profile_id) == interceptors_.end())
    return;
  net_log_source_profile_map_[net_log_source_id] = throttling_profile_id;
}

void ThrottlingController::Unregister(uint32_t net_log_source_id) {
  CheckValidThread();
  net_log_source_profile_map_.erase(net_log_source_id);
}

base::Optional<base::UnguessableToken> ThrottlingController::GetProfileID(
    uint32_t net_log_source_id) {
  CheckValidThread();
  auto it = net_log_source_profile_map_.find(net_log_source_id);
  if (it == net_log_source_profile_map_.end())
    return base::nullopt;
  return it->second;
}

void ThrottlingController::SetNetworkConditions(
    const base::UnguessableToken& throttling_profile_id,
    std::unique_ptr<NetworkConditions> conditions) {
  CheckValidThread();

  auto it = interceptors_.find(throttling_profile_id);
  if (it == interceptors_.end()) {
    if (!conditions)
      return;
    std::unique_ptr<ThrottlingNetworkInterceptor> new_interceptor(
        new ThrottlingNetworkInterceptor());
    new_interceptor->UpdateConditions(std::move(conditions));
    interceptors_[throttling_profile_id] = std::move(new_interceptor);
  } else {
    if (!conditions) {
      std::unique_ptr<NetworkConditions> online_conditions(
          new NetworkConditions());
      it->second->UpdateConditions(std::move(online_conditions));
      interceptors_.erase(throttling_profile_id);
      if (interceptors_.empty()) {
        delete this;
        instance_ = nullptr;
      }
    } else {
      it->second->UpdateConditions(std::move(conditions));
    }
  }
}

static NOINLINE void CrashBecauseThrottlingControllerDeleted() {
  LOG(ERROR) << "deleted";
  CHECK(false);
}

static NOINLINE void CrashBecauseThrottlingControllerBad() {
  LOG(ERROR) << "bad";
  CHECK(false);
}

ThrottlingNetworkInterceptor* ThrottlingController::FindInterceptor(
    uint32_t net_log_source_id) {
  CheckValidThread();

  if (liveness_ == Liveness::kDead) {
    CrashBecauseThrottlingControllerDeleted();
  } else if (liveness_ != Liveness::kAlive) {
    Liveness liveness = liveness_;
    base::debug::Alias(&liveness);
    CrashBecauseThrottlingControllerBad();
  }

  auto source_profile_map_it =
      net_log_source_profile_map_.find(net_log_source_id);
  if (source_profile_map_it == net_log_source_profile_map_.end())
    return nullptr;
  auto it = interceptors_.find(source_profile_map_it->second);
  return it != interceptors_.end() ? it->second.get() : nullptr;
}

void ThrottlingController::CheckValidThread() {
  CHECK(thread_checker_.CalledOnValidThread());
}

}  // namespace network
