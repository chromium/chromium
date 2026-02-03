// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/pervasive_resources/shared_resource_checker.h"

#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "components/content_settings/core/common/cookie_settings_base.h"
#include "components/url_pattern/simple_url_pattern_matcher.h"
#include "net/base/load_flags.h"
#include "services/network/pervasive_resources/shared_resource_checker_patterns.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/zstd/src/lib/zstd.h"
#include "url/gurl.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_FUCHSIA)
#include "services/network/pervasive_resources/shared_resource_checker_patterns.h"
#endif  // !BUILDFLAG(IS_FUCHSIA)

// Each pattern can only be used by up to 2 URLs in a given hour (allowing for
// a resource to update once per hour).
// This is to reduce the risk of abusing a given pattern to use for
// fingerprinting.
// See "URL pattern matches are sticky" in the design doc:
// https://docs.google.com/document/d/1xaoF9iSOojrlPrHZaKIJMK4iRZKA3AD6pQvbSy4ueUQ/edit?tab=t.0#bookmark=id.j15h26m9sd4
static const size_t kMaxMatches = 2;
static const int64_t kMatchWindowSeconds = 1 * base::Time::kSecondsPerHour;
static constexpr base::TimeDelta kUserGestureTimeout = base::Minutes(10);

namespace network {

// This class encapsulates the matching logic and restrictions for a single
// URLPattern (limiting the number of URLs allowed within a given period).
class SharedResourceChecker::PatternEntry {
 public:
  ~PatternEntry() = default;
  PatternEntry(const std::string& pattern, const GURL& base_url) {
    auto pattern_create_result =
        url_pattern::SimpleUrlPatternMatcher::Create(pattern, &base_url);
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
    base::TimeTicks now = base::TimeTicks::Now();

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
    base::TimeTicks last_used;
  };
  std::list<UrlMatch> url_matches_;
  std::unique_ptr<url_pattern::SimpleUrlPatternMatcher> url_pattern_;
};

SharedResourceChecker::SharedResourceChecker(
    const content_settings::CookieSettingsBase& cookie_settings)
    : cookie_settings_(cookie_settings) {
#if !BUILDFLAG(IS_FUCHSIA)
  enabled_ = base::FeatureList::IsEnabled(
      features::kCacheSharingForPervasiveResources);
#endif  // !BUILDFLAG(IS_FUCHSIA)
}

SharedResourceChecker::~SharedResourceChecker() = default;

void SharedResourceChecker::LoadPervasivePatterns(
    const uint8_t* compressed_patterns,
    size_t compressed_patterns_size,
    const base::Time::Exploded& expiration) {
  loaded_ = true;
  patterns_.clear();

  base::Time patterns_expiration;
  if (!base::Time::FromUTCExploded(expiration, &patterns_expiration)) {
    return;
  }

  base::Time now = base::Time::Now();
  if (now > patterns_expiration) {
    return;
  }

  // Decompress the zstd-compressed list
  std::string patterns;
  auto uncompressed_buff_size =
      ZSTD_getFrameContentSize(compressed_patterns, compressed_patterns_size);
  if (uncompressed_buff_size == ZSTD_CONTENTSIZE_ERROR ||
      uncompressed_buff_size == ZSTD_CONTENTSIZE_UNKNOWN) {
    return;
  }
  size_t actual_compressed_size = ZSTD_findFrameCompressedSize(
      compressed_patterns, compressed_patterns_size);
  if (ZSTD_isError(actual_compressed_size) ||
      actual_compressed_size > compressed_patterns_size) {
    return;
  }
  patterns.resize(uncompressed_buff_size);
  size_t uncompressed_size =
      ZSTD_decompress(patterns.data(), patterns.size(), compressed_patterns,
                      compressed_patterns_size);
  if (ZSTD_isError(uncompressed_size)) {
    return;
  }

  // Build the origin-indexed list of URL Patterns.
  std::vector<std::string> lines = base::SplitString(
      patterns, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const std::string& entry : lines) {
    GURL pattern_as_url(entry);
    CHECK(pattern_as_url.is_valid());

    std::unique_ptr<PatternEntry> pattern =
        std::make_unique<PatternEntry>(entry, pattern_as_url);
    CHECK(pattern->is_valid());

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

bool SharedResourceChecker::IsSharedResource(
    const ResourceRequest& request,
    const std::optional<url::Origin>& top_frame_origin,
    base::optional_ref<const net::CookiePartitionKey> cookie_partition_key) {
  if (!enabled_) {
    return false;
  }

  // Keep track of the last time each document origin had a request with a
  // user gesture or a top-level navigation.
  bool had_gesture_or_navigation =
      UpdateGestureAndNavigationTracking(request, top_frame_origin);

  // Make sure there are no cache-impacting load flags set.
  if (request.load_flags &
      (net::LOAD_VALIDATE_CACHE | net::LOAD_BYPASS_CACHE |
       net::LOAD_SKIP_CACHE_VALIDATION | net::LOAD_ONLY_FROM_CACHE |
       net::LOAD_DISABLE_CACHE)) {
    return false;
  }

  // Only allow script, style and dictionary destinations.
  if (request.destination != mojom::RequestDestination::kScript &&
      request.destination != mojom::RequestDestination::kStyle &&
      request.destination != mojom::RequestDestination::kDictionary) {
    return false;
  }

  // Do not support URLs with query parameters.
  if (request.url.has_query()) {
    return false;
  }

  // Do not allow requests where the top-level document origin hasn't had
  // a recent request from a user gesture or a main document navigation.
  if (!had_gesture_or_navigation &&
      !HadRecentGestureOrNavigation(top_frame_origin)) {
    return false;
  }

  // Disabled if third-party cookies are disabled.
  if (!cookie_settings_->IsFullCookieAccessAllowed(
          request.url, request.site_for_cookies, top_frame_origin,
          net::CookieSettingOverrides(), cookie_partition_key)) {
    return false;
  }

#if !BUILDFLAG(IS_FUCHSIA)
  if (!loaded_) {
    LoadPervasivePatterns(internal::kPervasivePatternsZstd,
                          sizeof(internal::kPervasivePatternsZstd),
                          internal::kPervasivePatternsExpiration);
  }
#endif  // !BUILDFLAG(IS_FUCHSIA)

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

bool SharedResourceChecker::UpdateGestureAndNavigationTracking(
    const ResourceRequest& request,
    const std::optional<url::Origin>& top_frame_origin) {
  // Clear the tracking map any time it has been more than the user gesture
  // timeout. This is more efficient than pruning individual expired values.
  base::TimeTicks now = base::TimeTicks::Now();
  if (!last_gesture_or_navigation_.is_null() &&
      now - last_gesture_or_navigation_ > kUserGestureTimeout) {
    last_document_gesture_or_navigation_.clear();
  }

  if (top_frame_origin &&
      (request.has_user_gesture ||
       (request.is_outermost_main_frame &&
        request.destination == mojom::RequestDestination::kDocument))) {
    last_gesture_or_navigation_ = now;
    last_document_gesture_or_navigation_[*top_frame_origin] = now;
    return true;
  }

  return false;
}

bool SharedResourceChecker::HadRecentGestureOrNavigation(
    const std::optional<url::Origin>& top_frame_origin) const {
  if (!top_frame_origin) {
    return false;
  }
  auto it = last_document_gesture_or_navigation_.find(*top_frame_origin);
  if (it == last_document_gesture_or_navigation_.end()) {
    return false;
  }
  base::TimeTicks now = base::TimeTicks::Now();
  if (now - it->second <= kUserGestureTimeout) {
    return true;
  }
  return false;
}

}  // namespace network
