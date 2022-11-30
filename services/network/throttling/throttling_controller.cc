// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/throttling/throttling_controller.h"

#include "base/no_destructor.h"
#include "net/http/http_request_info.h"
#include "services/network/throttling/network_conditions.h"
#include "services/network/throttling/scoped_throttling_token.h"
#include "services/network/throttling/throttling_network_interceptor.h"

namespace network {

ThrottlingController::ThrottlingController() = default;
ThrottlingController::~ThrottlingController() = default;

// static
ThrottlingController& ThrottlingController::instance() {
  static base::NoDestructor<ThrottlingController> instance;
  return *instance;
}

// static
void ThrottlingController::SetConditions(
    const base::UnguessableToken& throttling_profile_id,
    std::unique_ptr<NetworkConditions> conditions) {
  instance().SetNetworkConditions(throttling_profile_id, std::move(conditions));
}

// static
ThrottlingNetworkInterceptor* ThrottlingController::GetInterceptor(
    uint32_t net_log_source_id) {
  return instance().FindInterceptor(net_log_source_id);
}

// static
void ThrottlingController::RegisterProfileIDForNetLogSource(
    uint32_t net_log_source_id,
    const base::UnguessableToken& throttling_profile_id) {
  instance().Register(net_log_source_id, throttling_profile_id);
}

// static
void ThrottlingController::UnregisterNetLogSource(uint32_t net_log_source_id) {
  instance().Unregister(net_log_source_id);
}

void ThrottlingController::Register(
    uint32_t net_log_source_id,
    const base::UnguessableToken& throttling_profile_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  net_log_source_profile_map_[net_log_source_id] = throttling_profile_id;
}

void ThrottlingController::Unregister(uint32_t net_log_source_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  net_log_source_profile_map_.erase(net_log_source_id);
}

absl::optional<base::UnguessableToken> ThrottlingController::GetProfileID(
    uint32_t net_log_source_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = net_log_source_profile_map_.find(net_log_source_id);
  if (it == net_log_source_profile_map_.end())
    return absl::nullopt;
  return it->second;
}

void ThrottlingController::SetNetworkConditions(
    const base::UnguessableToken& throttling_profile_id,
    std::unique_ptr<NetworkConditions> conditions) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

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
    } else {
      it->second->UpdateConditions(std::move(conditions));
    }
  }
}

ThrottlingNetworkInterceptor* ThrottlingController::FindInterceptor(
    uint32_t net_log_source_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto source_profile_map_it =
      net_log_source_profile_map_.find(net_log_source_id);
  if (source_profile_map_it == net_log_source_profile_map_.end())
    return nullptr;
  auto it = interceptors_.find(source_profile_map_it->second);
  return it != interceptors_.end() ? it->second.get() : nullptr;
}

}  // namespace network
