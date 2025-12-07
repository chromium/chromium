// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/filtering_details_url_generator.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/no_destructor.h"
#include "net/third_party/uri_template/uri_template.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace net {

namespace {
const absl::flat_hash_map<std::string, std::string>& GetBuiltInRegistry() {
  static const base::NoDestructor<absl::flat_hash_map<std::string, std::string>>
      kDefaultFilteringDetailsRegistry(
          absl::flat_hash_map<std::string, std::string>{});
  return *kDefaultFilteringDetailsRegistry;
}
}  // namespace

FilteringDetailsUrlGenerator::FilteringDetailsUrlGenerator(
    const absl::flat_hash_map<std::string, std::string>& registry)
    : registry_(registry.empty() ? GetBuiltInRegistry() : registry) {}

FilteringDetailsUrlGenerator::~FilteringDetailsUrlGenerator() = default;

std::optional<std::string> FilteringDetailsUrlGenerator::GenerateUrl(
    std::string_view db,
    std::string_view id) {
  auto it = registry_.find(db);
  if (it == registry_.end()) {
    return std::nullopt;
  }
  std::unordered_map<std::string, std::string> params = {
      {"id", std::string(id)}};
  std::string url;
  bool success = uri_template::Expand(it->second, params, &url);
  return success ? std::optional<std::string>(url) : std::nullopt;
}

}  // namespace net
