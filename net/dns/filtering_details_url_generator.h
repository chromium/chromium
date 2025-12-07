// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_FILTERING_DETAILS_URL_GENERATOR_H_
#define NET_DNS_FILTERING_DETAILS_URL_GENERATOR_H_

#include <optional>
#include <string>
#include <string_view>

#include "net/base/net_export.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace net {

// Utility class for generating user-facing URLs for DNS Filtering Details.
// Based on Version 2 of the Public Resolver Errors draft:
// https://datatracker.ietf.org/doc/draft-nottingham-public-resolver-errors/02/
class NET_EXPORT FilteringDetailsUrlGenerator {
 public:
  explicit FilteringDetailsUrlGenerator(
      const absl::flat_hash_map<std::string, std::string>& registry);
  ~FilteringDetailsUrlGenerator();

  std::optional<std::string> GenerateUrl(std::string_view db,
                                         std::string_view id);

 private:
  // TODO(crbug.com/396483553): Populate this registry with at least one
  // provider before enabling feature flag `kDnsFilteringDetails`.
  absl::flat_hash_map<std::string, std::string> registry_;
};

}  // namespace net

#endif  // NET_DNS_FILTERING_DETAILS_URL_GENERATOR_H_
