// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_resource_checker.h"

#include <sstream>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "components/content_settings/core/common/cookie_settings_base.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/shared_dictionary/simple_url_pattern_matcher.h"
#include "url/gurl.h"
#include "url/origin.h"

// Each pattern can only be used by up to 2 URLs in a given hour (allowing for
// a resource to update once per hour).
// This is to reduce the risk of abusing a given pattern to use for
// fingerprinting.
// See "URL pattern matches are sticky" in the design doc:
// https://docs.google.com/document/d/1xaoF9iSOojrlPrHZaKIJMK4iRZKA3AD6pQvbSy4ueUQ/edit?tab=t.0#bookmark=id.j15h26m9sd4
static const size_t kMaxMatches = 2;
static const int64_t kMatchWindowSeconds = 1 * base::Time::kSecondsPerHour;

namespace network {

// This class encapsulates the matching logic and restrictions for a single
// URLPattern (limiting the number of URLs allowed within a given period).
class SharedResourceChecker::PatternEntry {
 public:
  ~PatternEntry() = default;
  PatternEntry(const std::string& pattern, const GURL& base_url) {
    auto pattern_create_result =
        SimpleUrlPatternMatcher::Create(pattern, base_url);
    if (pattern_create_result.has_value()) {
      url_pattern_ = std::move(pattern_create_result.value());
    }
  }
  PatternEntry(const PatternEntry&) = delete;
  PatternEntry& operator=(const PatternEntry&) = delete;

  bool is_valid() const { return static_cast<bool>(url_pattern_); }

  // Allow a given pattern to match up to two different URLs within an hour
  // (first two matches).
  bool Match(const GURL& url) {
    if (!url_pattern_ || !url_pattern_->Match(url)) {
      return false;
    }
    base::Time now = base::Time::Now();

    // See if it matches an existing URL
    for (auto& entry : url_matches_) {
      if (entry.url == url) {
        entry.last_used = now;
        return true;
      }
    }

    // Remove any stale entries.
    std::erase_if(url_matches_, [&now](const UrlMatch& entry) {
      return (now - entry.last_used).InSeconds() > kMatchWindowSeconds;
    });

    // If there is space, add it to the existing list.
    if (url_matches_.size() < kMaxMatches) {
      url_matches_.push_front({url, now});
      return true;
    }

    return false;
  }

 private:
  struct UrlMatch {
    GURL url;
    base::Time last_used;
  };
  std::list<UrlMatch> url_matches_;
  std::unique_ptr<SimpleUrlPatternMatcher> url_pattern_;
};

SharedResourceChecker::SharedResourceChecker(
    const content_settings::CookieSettingsBase& cookie_settings)
    : cookie_settings_(cookie_settings) {
  enabled_ =
      base::FeatureList::IsEnabled(features::kCacheSharingForPervasiveScripts);
  if (!enabled_) {
    return;
  }

  // Build the origin-indexed list of URL Patterns.
  std::stringstream ss(features::kPervasiveScriptURLPatterns.Get());
  std::string entry;
  while (std::getline(ss, entry)) {
    if (!entry.empty()) {
      GURL pattern_as_url(entry);
      if (pattern_as_url.is_valid()) {
        std::unique_ptr<PatternEntry> pattern =
            std::make_unique<PatternEntry>(entry, pattern_as_url);
        if (pattern->is_valid()) {
          url::Origin key = url::Origin::Create(pattern_as_url);
          auto result = patterns_.find(key);
          if (result != patterns_.end()) {
            result->second->push_back(std::move(pattern));
          } else {
            std::unique_ptr<UrlPatternList> pattern_list =
                std::make_unique<UrlPatternList>();
            pattern_list->push_back(std::move(pattern));
            patterns_.emplace(key, std::move(pattern_list));
          }
        }
      }
    }
  }
}

SharedResourceChecker::~SharedResourceChecker() = default;

bool SharedResourceChecker::IsSharedResource(
    const ResourceRequest& request,
    const std::optional<url::Origin>& top_frame_origin,
    base::optional_ref<const net::CookiePartitionKey> cookie_partition_key) {
  if (!enabled_) {
    return false;
  }

  // Only allow script destinations.
  if (request.destination != mojom::RequestDestination::kScript) {
    return false;
  }

  // Do not support URLs with query parameters.
  if (request.url.has_query()) {
    return false;
  }

  // Make sure there are no cache-impacting load flags set.
  if (request.load_flags &
      (net::LOAD_VALIDATE_CACHE | net::LOAD_BYPASS_CACHE |
       net::LOAD_SKIP_CACHE_VALIDATION | net::LOAD_ONLY_FROM_CACHE |
       net::LOAD_DISABLE_CACHE)) {
    return false;
  }

  // Disabled if third-party cookies are disabled.
  if (!cookie_settings_->IsFullCookieAccessAllowed(
          request.url, request.site_for_cookies, top_frame_origin,
          net::CookieSettingOverrides(), cookie_partition_key)) {
    return false;
  }

  // Check to see if the URL matches one of the configured patterns (indexed by
  // origin).
  auto result = patterns_.find(url::Origin::Create(request.url));
  if (result == patterns_.end()) {
    return false;
  }
  for (auto const& pattern : *result->second) {
    if (pattern->Match(request.url)) {
      return true;
    }
  }

  return false;
}

}  // namespace network
