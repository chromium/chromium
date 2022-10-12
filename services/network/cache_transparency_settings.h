// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CACHE_TRANSPARENCY_SETTINGS_H_
#define SERVICES_NETWORK_CACHE_TRANSPARENCY_SETTINGS_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace network {

// Feature configuration for Cache Transparency is expensive to calculate, so it
// is cached.
class COMPONENT_EXPORT(NETWORK_SERVICE) CacheTransparencySettings {
 public:
  CacheTransparencySettings();
  ~CacheTransparencySettings();

  CacheTransparencySettings(CacheTransparencySettings&) = delete;
  CacheTransparencySettings& operator=(const CacheTransparencySettings&) =
      delete;

  bool cache_transparency_enabled() const {
    return cache_transparency_enabled_;
  }

  bool pervasive_payloads_enabled() const {
    return pervasive_payloads_enabled_;
  }

  absl::optional<int> GetIndexForURL(const GURL& url) const;

  absl::optional<std::string> GetChecksumForURL(const GURL& url) const;

 private:
  using PervasivePayloadsMap = base::flat_map<std::string, std::string>;

  static PervasivePayloadsMap CreateMap();

  const bool cache_transparency_enabled_;
  const bool pervasive_payloads_enabled_;
  const PervasivePayloadsMap map_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_CACHE_TRANSPARENCY_SETTINGS_H_
