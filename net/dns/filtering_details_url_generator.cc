// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/filtering_details_url_generator.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "base/no_destructor.h"
#include "net/third_party/uri_template/uri_template.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace net {

namespace {
const FilteringDetailsUrlGenerator* g_instance_for_testing = nullptr;

// Note: use BASE_FEATURE as a suffix for this macro name because some tooling
// identifies feature definitions by searching for that string, so if our macro
// matches the standard one we have a better chance of our feature flags being
// detected by those tools.
#define MAKE_STATIC_STORAGE_BASE_FEATURE(feature_name, feature_state) \
  ([]() {                                                             \
    static BASE_FEATURE(feature_name, feature_state);                 \
    return &feature_name;                                             \
  })()

const FilteringDetailsUrlGenerator::FilteringDetailsRegistry&
GetBuiltInRegistry() {
  static const base::NoDestructor<
      FilteringDetailsUrlGenerator::FilteringDetailsRegistry>
      kDefaultFilteringDetailsRegistry(
          FilteringDetailsUrlGenerator::FilteringDetailsRegistry{
              {"lumen",
               FilteringDetailsUrlGenerator::RegistryEntry{
                   .url_template = "https://lumendatabase.com/notices/{id}",
                   .feature = MAKE_STATIC_STORAGE_BASE_FEATURE(
                       kFilteringDetailsLumen,
                       base::FEATURE_ENABLED_BY_DEFAULT)}}});
  return *kDefaultFilteringDetailsRegistry;
}

#undef MAKE_STATIC_STORAGE_BASE_FEATURE

}  // namespace

// static
const FilteringDetailsUrlGenerator&
FilteringDetailsUrlGenerator::GetInstance() {
  if (g_instance_for_testing) {
    return *g_instance_for_testing;
  }
  static const base::NoDestructor<FilteringDetailsUrlGenerator> instance(
      GetBuiltInRegistry());
  return *instance;
}

// static
void FilteringDetailsUrlGenerator::SetInstanceForTesting(
    const FilteringDetailsUrlGenerator* instance) {
  if (instance != nullptr) {
    CHECK_EQ(g_instance_for_testing, nullptr);
  }
  g_instance_for_testing = instance;
}

FilteringDetailsUrlGenerator FilteringDetailsUrlGenerator::CreateForTesting(
    const FilteringDetailsRegistry& registry) {
  return FilteringDetailsUrlGenerator(registry);
}

FilteringDetailsUrlGenerator::FilteringDetailsUrlGenerator(
    const FilteringDetailsRegistry& registry)
    : registry_(registry) {}

FilteringDetailsUrlGenerator::~FilteringDetailsUrlGenerator() = default;

FilteringDetailsUrlGenerator::FilteringDetailsUrlGenerator(
    FilteringDetailsUrlGenerator&&) = default;

FilteringDetailsUrlGenerator& FilteringDetailsUrlGenerator::operator=(
    FilteringDetailsUrlGenerator&&) = default;

std::optional<std::string> FilteringDetailsUrlGenerator::GenerateUrl(
    std::string_view db,
    std::string_view id) const {
  auto it = registry_.find(db);
  if (it == registry_.end()) {
    return std::nullopt;
  }
  if (it->second.feature &&
      !base::FeatureList::IsEnabled(*it->second.feature)) {
    return std::nullopt;
  }
  std::unordered_map<std::string, std::string> params = {
      {"id", std::string(id)}};
  std::string url;
  bool success = uri_template::Expand(it->second.url_template, params, &url);
  return success ? std::optional<std::string>(std::move(url)) : std::nullopt;
}

}  // namespace net
