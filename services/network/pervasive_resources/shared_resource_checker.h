// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PERVASIVE_RESOURCES_SHARED_RESOURCE_CHECKER_H_
#define SERVICES_NETWORK_PERVASIVE_RESOURCES_SHARED_RESOURCE_CHECKER_H_

#include <list>
#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "net/cookies/cookie_partition_key.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "url/origin.h"

namespace content_settings {
class CookieSettingsBase;
}

namespace network {
struct ResourceRequest;

// This class is attached to NetworkContext and is responsible for determining
// if a given request is for a well-known pervasive script when
// network::features::kCacheSharingForPervasiveResources is enabled.
// See https://chromestatus.com/feature/5202380930678784
class COMPONENT_EXPORT(NETWORK_SERVICE) SharedResourceChecker {
 public:
  class PatternEntry;

  ~SharedResourceChecker();
  SharedResourceChecker(
      const content_settings::CookieSettingsBase& cookie_settings);
  SharedResourceChecker(const SharedResourceChecker&) = delete;
  SharedResourceChecker& operator=(const SharedResourceChecker&) = delete;

  // Check to see if the given request is for a well-known pervasive static
  // resource (which will use a shared HTTP cache).
  //
  // On a match, this will update the PatternEntry to keep track of the
  // last time a given URL matched a pattern and will limit it to
  // two URLs per pattern (with a one-hour expiration for a given match).
  // This is to reduce the risk of abusing a pattern to be used for
  // fingerprinting.
  //
  // See https://chromestatus.com/feature/5202380930678784
  bool IsSharedResource(
      const ResourceRequest& request,
      const std::optional<url::Origin>& top_frame_origin,
      base::optional_ref<const net::CookiePartitionKey> cookie_partition_key);

 private:
  friend class SharedResourceCheckerTest;

  // Load the zstd-compressed list of pervasive resources. This is done
  // automatically in the constructor with the static list but is exposed so
  // that testing can use test-specific lists.
  void LoadPervasivePatterns(const uint8_t* compressed_patterns,
                             size_t compressed_patterns_size,
                             const base::Time::Exploded& expiration);

  // Keep track of the last time each top-level document origin had a request
  // that had an associated user gesture or a main document navigation
  // (and prune stale entries from the tracking map). Returns true if the
  // origin updated the timestamp so the origin won't need to be checked
  // again for this request.
  bool UpdateGestureAndNavigationTracking(
      const ResourceRequest& request,
      const std::optional<url::Origin>& top_frame_origin);

  // Check if the provided origin has had a request with an associated user
  // gesture or a main document navigation within the timeout window.
  bool HadRecentGestureOrNavigation(
      const std::optional<url::Origin>& top_frame_origin) const;

  const raw_ref<const content_settings::CookieSettingsBase> cookie_settings_;
  bool enabled_ = false;
  bool loaded_ = false;

  // Processed URLPatterns for matching URLs for shared static resource.
  // The patterns are stored in lists, indexed by origin.
  typedef std::list<std::unique_ptr<PatternEntry>> UrlPatternList;
  absl::flat_hash_map<url::Origin, std::unique_ptr<UrlPatternList>> patterns_;

  // Keep track of the last time each top-level site had a request triggered
  // by user gesture or document navigation (to prevent background attacks at
  // sniffing the single-keyed cache).
  base::TimeTicks last_gesture_or_navigation_;
  absl::flat_hash_map<url::Origin, base::TimeTicks>
      last_document_gesture_or_navigation_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PERVASIVE_RESOURCES_SHARED_RESOURCE_CHECKER_H_
