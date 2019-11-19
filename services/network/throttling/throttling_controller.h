// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_THROTTLING_THROTTLING_NETWORK_CONTROLLER_H_
#define SERVICES_NETWORK_THROTTLING_THROTTLING_NETWORK_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/unguessable_token.h"

namespace network {

class NetworkConditions;
class ScopedThrottlingToken;
class ThrottlingNetworkInterceptor;

// ThrottlingController manages interceptors identified by NetLog source ID and
// profile ID and their throttling conditions.
class COMPONENT_EXPORT(NETWORK_SERVICE) ThrottlingController {
 public:
  // Applies network emulation configuration.
  static void SetConditions(const base::UnguessableToken& throttling_profile_id,
                            std::unique_ptr<NetworkConditions>);

  // Returns the interceptor for the NetLog source ID.
  static ThrottlingNetworkInterceptor* GetInterceptor(
      uint32_t net_log_source_id);

 private:
  friend class ScopedThrottlingToken;

  // TODO(https://crbug.com/960874): Debugging code to try and shed some light
  // on why the owned maps are invalid.
  enum class Liveness : uint32_t {
    kAlive = 0xCA11AB13,
    kDead = 0xDEADBEEF,
  };

  ThrottlingController();
  ~ThrottlingController();

  // Registers the profile ID for the NetLog source. This is called from
  // ScopedThrottlingToken.
  static void RegisterProfileIDForNetLogSource(
      uint32_t net_log_source_id,
      const base::UnguessableToken& throttling_profile_id);

  // Unregister the NetLog source. This is called from ScopedThrottlingToken.
  static void UnregisterNetLogSource(uint32_t net_log_source_id);

  // Returns whether there is an interceptor for the profile ID. This is called
  // from ScopedThrottlingToken.
  static bool HasInterceptor(
      const base::UnguessableToken& throttling_profile_id);

  void Register(uint32_t net_log_source_id,
                const base::UnguessableToken& throttling_profile_id);
  void Unregister(uint32_t net_log_source_id);

  base::Optional<base::UnguessableToken> GetProfileID(
      uint32_t net_log_source_id);

  void SetNetworkConditions(const base::UnguessableToken& throttling_profile_id,
                            std::unique_ptr<NetworkConditions> conditions);

  ThrottlingNetworkInterceptor* FindInterceptor(uint32_t net_log_source_id);

  // TODO(https://crbug.com/960874): Debugging code.
  void CheckValidThread();

  static ThrottlingController* instance_;

  using InterceptorMap =
      std::map<base::UnguessableToken,
               std::unique_ptr<ThrottlingNetworkInterceptor>>;
  using NetLogSourceProfileMap =
      std::map<uint32_t /* net_log_source_id */,
               base::UnguessableToken /* throttling_profile_id */>;

  // TODO(https://crbug.com/960874): Debugging code.
  Liveness liveness_ = Liveness::kAlive;
  InterceptorMap interceptors_;
  NetLogSourceProfileMap net_log_source_profile_map_;
  base::ThreadCheckerImpl thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ThrottlingController);
};

}  // namespace network

#endif  // SERVICES_NETWORK_THROTTLING_THROTTLING_NETWORK_CONTROLLER_H_
