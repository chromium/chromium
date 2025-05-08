// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_RESOURCE_CHECKER_H_
#define SERVICES_NETWORK_SHARED_RESOURCE_CHECKER_H_

#include <list>
#include <map>
#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ref.h"
#include "base/types/optional_ref.h"
#include "net/cookies/cookie_partition_key.h"
#include "url/origin.h"

namespace content_settings {
class CookieSettingsBase;
}

namespace network {
struct ResourceRequest;

// This class is attached to NetworkContext and is responsible for determining
// if a given request is for a well-known pervasive script when
// network::features::kCacheSharingForPervasiveScripts is enabled.
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
  const raw_ref<const content_settings::CookieSettingsBase> cookie_settings_;
  bool enabled_ = false;

  // URLPatterns for matching URLs for shared static resource.
  // The patterns are stored in lists, indexed by origin.
  typedef std::list<std::unique_ptr<PatternEntry>> UrlPatternList;
  std::map<url::Origin, std::unique_ptr<UrlPatternList>> patterns_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_RESOURCE_CHECKER_H_
