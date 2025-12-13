// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_THROTTLING_THROTTLING_CONTROLLER_H_
#define SERVICES_NETWORK_THROTTLING_THROTTLING_CONTROLLER_H_

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/no_destructor.h"
#include "base/threading/thread_checker.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/url_pattern/simple_url_pattern_matcher.h"
#include "services/network/public/cpp/network_service_buildflags.h"
#include "services/network/throttling/network_conditions.h"

namespace network {

class NetworkConditions;
class ScopedThrottlingToken;
class ThrottlingNetworkInterceptor;
class ThrottlingP2PNetworkInterceptor;

struct COMPONENT_EXPORT(NETWORK_SERVICE) MatchedNetworkConditions {
  std::string url_pattern;
  NetworkConditions conditions;
};

// ThrottlingController manages interceptors identified by NetLog source ID and
// profile ID and their throttling conditions.
class COMPONENT_EXPORT(NETWORK_SERVICE) ThrottlingController {
 public:
  ThrottlingController(const ThrottlingController&) = delete;
  ThrottlingController& operator=(const ThrottlingController&) = delete;

  // Applies network emulation configuration. The configuration is passed in
  // |conditions| as vector of pairs of a URL pattern and the
  // |NetworkConditions| to apply to requests matching that pattern. Patterns
  // are defined using the URLPattern API pattern language. An empty URL pattern
  // applies to all requests that don't match another pattern. If multiple such
  // global conditions are passed, only the last one takes effect.
  static void SetConditions(const base::UnguessableToken& throttling_profile_id,
                            std::vector<MatchedNetworkConditions> conditions);

  // Returns the interceptor for the NetLog source ID and url. The |url| is
  // matched against the patterns provided in |SetConditions|.
  static ThrottlingNetworkInterceptor* GetInterceptor(
      uint32_t net_log_source_id,
      const GURL& url);

#if BUILDFLAG(IS_P2P_ENABLED)
  static ThrottlingP2PNetworkInterceptor* GetP2PInterceptor(
      uint32_t net_log_source_id);
#endif

 private:
  friend class ScopedThrottlingToken;
  friend class base::NoDestructor<ThrottlingController>;
  friend class ThrottlingControllerTestHelper;

  struct InterceptorMatcher {
    explicit InterceptorMatcher(NetworkConditions conditions);
    ~InterceptorMatcher();
    InterceptorMatcher(InterceptorMatcher&&);
    InterceptorMatcher& operator=(InterceptorMatcher&&);

    using Pattern =
        std::pair<std::string,
                  std::unique_ptr<url_pattern::SimpleUrlPatternMatcher>>;
    std::vector<Pattern> patterns;
    std::unique_ptr<ThrottlingNetworkInterceptor> interceptor;
    NetworkConditions conditions;
  };

  class COMPONENT_EXPORT(NETWORK_SERVICE) ThrottlingProfile {
   public:
    ThrottlingProfile();
    ~ThrottlingProfile();
    ThrottlingProfile(ThrottlingProfile&&);
    ThrottlingProfile& operator=(ThrottlingProfile&&);

    void SetNetworkConditions(std::vector<MatchedNetworkConditions> conditions);
    ThrottlingNetworkInterceptor* FindInterceptor(const GURL& url) const;

    size_t matcher_count() const { return matchers_.size(); }

   private:
    std::vector<InterceptorMatcher> matchers_;
  };

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

  void SetNetworkConditions(
      const base::UnguessableToken& throttling_profile_id,
      std::vector<MatchedNetworkConditions> matched_conditions);

  ThrottlingNetworkInterceptor* FindInterceptor(uint32_t net_log_source_id,
                                                const GURL& url);

  using InterceptorMap = std::map<base::UnguessableToken, ThrottlingProfile>;
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
