// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_THROTTLING_THROTTLING_CONTROLLER_H_
#define SERVICES_NETWORK_THROTTLING_THROTTLING_CONTROLLER_H_

#include <map>
#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/no_destructor.h"
#include "base/threading/thread_checker.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "services/network/public/cpp/network_service_buildflags.h"

namespace network {

class NetworkConditions;
class ScopedThrottlingToken;
class ThrottlingNetworkInterceptor;
class ThrottlingP2PNetworkInterceptor;

// ThrottlingController manages interceptors identified by NetLog source ID and
// profile ID and their throttling conditions.
class COMPONENT_EXPORT(NETWORK_SERVICE) ThrottlingController {
 public:
  ThrottlingController(const ThrottlingController&) = delete;
  ThrottlingController& operator=(const ThrottlingController&) = delete;

  // Applies network emulation configuration.
  static void SetConditions(const base::UnguessableToken& throttling_profile_id,
                            std::unique_ptr<NetworkConditions>);

  // Returns the interceptor for the NetLog source ID.
  static ThrottlingNetworkInterceptor* GetInterceptor(
      uint32_t net_log_source_id);

#if BUILDFLAG(IS_P2P_ENABLED)
  static ThrottlingP2PNetworkInterceptor* GetP2PInterceptor(
      uint32_t net_log_source_id);
#endif

 private:
  friend class ScopedThrottlingToken;
  friend class base::NoDestructor<ThrottlingController>;

  ThrottlingController();
  ~ThrottlingController();

  static ThrottlingController& instance();

  // Registers the profile ID for the NetLog source. This is called from
  // ScopedThrottlingToken.
  static void RegisterProfileIDForNetLogSource(
      uint32_t net_log_source_id,
      const base::UnguessableToken& throttling_profile_id);

  // Unregister the NetLog source. This is called from ScopedThrottlingToken.
  static void UnregisterNetLogSource(uint32_t net_log_source_id);

  void Register(uint32_t net_log_source_id,
                const base::UnguessableToken& throttling_profile_id);
  void Unregister(uint32_t net_log_source_id);

  std::optional<base::UnguessableToken> GetProfileID(
      uint32_t net_log_source_id);

  void SetNetworkConditions(const base::UnguessableToken& throttling_profile_id,
                            std::unique_ptr<NetworkConditions> conditions);

  ThrottlingNetworkInterceptor* FindInterceptor(uint32_t net_log_source_id);

  using InterceptorMap =
      std::map<base::UnguessableToken,
               std::unique_ptr<ThrottlingNetworkInterceptor>>;
  using NetLogSourceProfileMap =
      std::map<uint32_t /* net_log_source_id */,
               base::UnguessableToken /* throttling_profile_id */>;

  InterceptorMap interceptors_;
  NetLogSourceProfileMap net_log_source_profile_map_;

#if BUILDFLAG(IS_P2P_ENABLED)
  using P2PInterceptorMap =
      std::map<base::UnguessableToken,
               std::unique_ptr<ThrottlingP2PNetworkInterceptor>>;

  ThrottlingP2PNetworkInterceptor* FindP2PInterceptor(
      uint32_t net_log_source_id);

  P2PInterceptorMap p2p_interceptors_;
#endif

  THREAD_CHECKER(thread_checker_);
};

}  // namespace network

#endif  // SERVICES_NETWORK_THROTTLING_THROTTLING_CONTROLLER_H_
