// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/throttling/throttling_controller.h"

#include <algorithm>
#include <memory>

#include "base/no_destructor.h"
#include "components/url_pattern/simple_url_pattern_matcher.h"
#include "net/http/http_request_info.h"
#include "services/network/throttling/network_conditions.h"
#include "services/network/throttling/scoped_throttling_token.h"
#include "services/network/throttling/throttling_network_interceptor.h"
#if BUILDFLAG(IS_P2P_ENABLED)
#include "services/network/throttling/throttling_p2p_network_interceptor.h"
#endif

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
    const base::UnguessableToken& throttling_client_id,
    std::vector<MatchedNetworkConditions> conditions) {
  instance().SetNetworkConditions(throttling_profile_id, throttling_client_id,
                                  std::move(conditions));
}

// static
ThrottlingNetworkInterceptor* ThrottlingController::GetInterceptor(
    uint32_t net_log_source_id,
    const GURL& url) {
  return instance().FindInterceptor(net_log_source_id, url);
}

#if BUILDFLAG(IS_P2P_ENABLED)
// static
ThrottlingP2PNetworkInterceptor* ThrottlingController::GetP2PInterceptor(
    uint32_t net_log_source_id) {
  return instance().FindP2PInterceptor(net_log_source_id);
}
#endif

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

std::optional<base::UnguessableToken> ThrottlingController::GetProfileID(
    uint32_t net_log_source_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = net_log_source_profile_map_.find(net_log_source_id);
  if (it == net_log_source_profile_map_.end()) {
    return std::nullopt;
  }
  return it->second;
}

ThrottlingController::InterceptorMatcher::InterceptorMatcher(
    NetworkConditions conditions)
    : conditions(std::move(conditions)) {
  interceptor = std::make_unique<ThrottlingNetworkInterceptor>();
  interceptor->UpdateConditions(this->conditions);
}

ThrottlingController::InterceptorMatcher::~InterceptorMatcher() {
  // May have been moved out
  if (interceptor) {
    interceptor->UpdateConditions({});
  }
}

ThrottlingController::InterceptorMatcher::InterceptorMatcher(
    InterceptorMatcher&&) = default;
ThrottlingController::InterceptorMatcher&
ThrottlingController::InterceptorMatcher::operator=(InterceptorMatcher&&) =
    default;

ThrottlingController::ThrottlingProfile::ThrottlingProfile(
    ThrottlingController::ThrottlingProfile&&) = default;
ThrottlingController::ThrottlingProfile&
ThrottlingController::ThrottlingProfile::operator=(
    ThrottlingController::ThrottlingProfile&&) = default;

ThrottlingController::ThrottlingProfile::ThrottlingProfile() = default;
ThrottlingController::ThrottlingProfile::~ThrottlingProfile() = default;

namespace {

template <typename IterT>
IterT FindConditions(IterT begin,
                     IterT end,
                     const NetworkConditions& network_conditions) {
  return std::find_if(begin, end, [&network_conditions](auto&& matcher) {
    return matcher.conditions == network_conditions;
  });
}

}  // namespace

void ThrottlingController::ThrottlingProfile::SetNetworkConditions(
    const base::UnguessableToken& throttling_client_id,
    std::vector<MatchedNetworkConditions> conditions) {
  auto it = std::find_if(
      client_matchers_.begin(), client_matchers_.end(),
      [&](const auto& pair) { return pair.first == throttling_client_id; });

  if (conditions.empty()) {
    if (it != client_matchers_.end()) {
      client_matchers_.erase(it);
    }
    return;
  }

  std::vector<InterceptorMatcher>* matchers_ptr;
  std::vector<InterceptorMatcher> old_matchers;
  // client_matchers could be a map that maintains insertion order.
  // Currently, no such map container is available in Chromium so we are using
  // a vector to maintain the insertion order of the throttling profiles.
  if (it != client_matchers_.end()) {
    matchers_ptr = &it->second;
    std::swap(old_matchers, *matchers_ptr);
  } else {
    client_matchers_.emplace_back(throttling_client_id,
                                  std::vector<InterceptorMatcher>());
    matchers_ptr = &client_matchers_.back().second;
  }

  auto& matchers_ = *matchers_ptr;

  // This has quadratic (#conditions * #matchers_) complexity, but we expect
  // clients to create relatively small numbers of different conditions at the
  // same time. If this grows too large we need to build maps from the
  // conditions here.
  for (auto& [pattern, network_conditions] : conditions) {
    std::unique_ptr<url_pattern::SimpleUrlPatternMatcher> pattern_matcher;
    if (!pattern.empty()) {
      auto maybe_pattern_matcher = url_pattern::SimpleUrlPatternMatcher::Create(
          pattern, /*base_url=*/nullptr);
      if (!maybe_pattern_matcher.has_value()) {
        continue;
      }
      pattern_matcher = std::move(*maybe_pattern_matcher);
      CHECK(pattern_matcher);
    }

    if (auto old_entry = FindConditions(old_matchers.begin(),
                                        old_matchers.end(), network_conditions);
        old_entry != old_matchers.end()) {
      matchers_.emplace_back(std::move(*old_entry));
      old_matchers.erase(old_entry);

      matchers_.back().patterns.clear();
      matchers_.back().patterns.emplace_back(std::move(pattern),
                                             std::move(pattern_matcher));
    } else if (auto new_entry = FindConditions(
                   matchers_.begin(), matchers_.end(), network_conditions);
               new_entry != matchers_.end()) {
      new_entry->patterns.emplace_back(std::move(pattern),
                                       std::move(pattern_matcher));
    } else {
      matchers_.emplace_back(std::move(network_conditions));
      matchers_.back().patterns.emplace_back(std::move(pattern),
                                             std::move(pattern_matcher));
    }
  }
}

size_t ThrottlingController::ThrottlingProfile::GetMatcherCountForTesting()
    const {
  size_t count = 0;
  for (const auto& pair : client_matchers_) {
    count += pair.second.size();
  }
  return count;
}

std::optional<NetworkConditions>
ThrottlingController::ThrottlingProfile::GetGlobalConditions() const {
  for (const auto& pair : client_matchers_) {
    for (const auto& matcher : pair.second) {
      for (const auto& pattern : matcher.patterns) {
        if (pattern.first.empty()) {
          return matcher.conditions;
        }
      }
    }
  }
  return std::nullopt;
}

ThrottlingNetworkInterceptor*
ThrottlingController::ThrottlingProfile::FindInterceptor(
    const GURL& url) const {
  for (const auto& pair : client_matchers_) {
    for (const InterceptorMatcher& matcher : pair.second) {
      for (auto& pattern : matcher.patterns) {
        if (pattern.first.empty() || pattern.second->Match(url)) {
          return matcher.interceptor.get();
        }
      }
    }
  }
  return nullptr;
}

void ThrottlingController::SetNetworkConditions(
    const base::UnguessableToken& throttling_profile_id,
    const base::UnguessableToken& throttling_client_id,
    std::vector<MatchedNetworkConditions> matched_conditions) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = interceptors_.find(throttling_profile_id);
  if (it == interceptors_.end()) {
    if (matched_conditions.empty()) {
      return;
    }
    ThrottlingProfile new_profile;
    new_profile.SetNetworkConditions(throttling_client_id, matched_conditions);
    interceptors_.emplace(throttling_profile_id, std::move(new_profile));
  } else {
    it->second.SetNetworkConditions(throttling_client_id, matched_conditions);
    if (it->second.is_empty()) {
      interceptors_.erase(throttling_profile_id);
    }
  }

#if BUILDFLAG(IS_P2P_ENABLED)
  std::optional<NetworkConditions> global_conditions;
  auto current_it = interceptors_.find(throttling_profile_id);
  if (current_it != interceptors_.end()) {
    global_conditions = current_it->second.GetGlobalConditions();
  }

  auto p2p_it = p2p_interceptors_.find(throttling_profile_id);
  if (p2p_it == p2p_interceptors_.end()) {
    if (!global_conditions.has_value()) {
      return;
    }

    std::unique_ptr<ThrottlingP2PNetworkInterceptor> new_interceptor(
        new ThrottlingP2PNetworkInterceptor());
    new_interceptor->UpdateConditions(global_conditions.value());
    p2p_interceptors_[throttling_profile_id] = std::move(new_interceptor);
  } else {
    if (!global_conditions.has_value()) {
      p2p_it->second->UpdateConditions(NetworkConditions{});
      p2p_interceptors_.erase(throttling_profile_id);
    } else {
      p2p_it->second->UpdateConditions(global_conditions.value());
    }
  }
#endif
}

ThrottlingNetworkInterceptor* ThrottlingController::FindInterceptor(
    uint32_t net_log_source_id,
    const GURL& url) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto source_profile_map_it =
      net_log_source_profile_map_.find(net_log_source_id);
  if (source_profile_map_it == net_log_source_profile_map_.end()) {
    return nullptr;
  }
  auto it = interceptors_.find(source_profile_map_it->second);
  return it != interceptors_.end() ? it->second.FindInterceptor(url) : nullptr;
}

#if BUILDFLAG(IS_P2P_ENABLED)
ThrottlingP2PNetworkInterceptor* ThrottlingController::FindP2PInterceptor(
    uint32_t net_log_source_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto source_profile_map_it =
      net_log_source_profile_map_.find(net_log_source_id);
  if (source_profile_map_it == net_log_source_profile_map_.end()) {
    return nullptr;
  }
  auto it = p2p_interceptors_.find(source_profile_map_it->second);
  return it != p2p_interceptors_.end() ? it->second.get() : nullptr;
}
#endif

}  // namespace network
