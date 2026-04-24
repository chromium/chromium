// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_FILTERING_DETAILS_URL_GENERATOR_H_
#define NET_DNS_FILTERING_DETAILS_URL_GENERATOR_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "net/base/net_export.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace net {

// Utility class for generating user-facing URLs for DNS Filtering Details.
// Based on Version 2 of the Public Resolver Errors draft:
// https://datatracker.ietf.org/doc/draft-nottingham-public-resolver-errors/02/
class NET_EXPORT FilteringDetailsUrlGenerator {
 public:
  struct RegistryEntry {
    std::string url_template;
    raw_ptr<const base::Feature> feature = nullptr;
  };

  using FilteringDetailsRegistry =
      absl::flat_hash_map<std::string, RegistryEntry>;

  static const FilteringDetailsUrlGenerator& GetInstance();

  static FilteringDetailsUrlGenerator CreateForTesting(
      const FilteringDetailsRegistry& registry);
  static void SetInstanceForTesting(
      const FilteringDetailsUrlGenerator* instance);

  ~FilteringDetailsUrlGenerator();

  // Delete the copy constructor and assignment operator since this class is
  // primarily a singleton, but allow move construction and assignment for
  // CreateForTesting.
  FilteringDetailsUrlGenerator(const FilteringDetailsUrlGenerator&) = delete;
  FilteringDetailsUrlGenerator& operator=(const FilteringDetailsUrlGenerator&) =
      delete;
  FilteringDetailsUrlGenerator(FilteringDetailsUrlGenerator&&);
  FilteringDetailsUrlGenerator& operator=(FilteringDetailsUrlGenerator&&);

  std::optional<std::string> GenerateUrl(std::string_view db,
                                         std::string_view id) const;

  const FilteringDetailsRegistry& GetRegistryForTesting() const {
    return registry_;
  }

 private:
  friend class base::NoDestructor<FilteringDetailsUrlGenerator>;

  explicit FilteringDetailsUrlGenerator(
      const FilteringDetailsRegistry& registry);

  FilteringDetailsRegistry registry_;
};

}  // namespace net

#endif  // NET_DNS_FILTERING_DETAILS_URL_GENERATOR_H_
