// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/feature_provider.h"

#include <algorithm>
#include <functional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/feature.h"

namespace extensions {

namespace {

class FeatureProviderStatic {
 public:
  FeatureProviderStatic() {
    TRACE_EVENT0("startup",
                 "extensions::FeatureProvider::FeatureProviderStatic");

    ExtensionsClient* client = ExtensionsClient::Get();
    api_ = client->CreateFeatureProvider("api");
    manifest_ = client->CreateFeatureProvider("manifest");
    permission_ = client->CreateFeatureProvider("permission");
    behavior_ = client->CreateFeatureProvider("behavior");
  }

  FeatureProviderStatic(const FeatureProviderStatic&) = delete;
  FeatureProviderStatic& operator=(const FeatureProviderStatic&) = delete;

  const FeatureProvider& api() const { return api_; }
  const FeatureProvider& manifest() const { return manifest_; }
  const FeatureProvider& permission() const { return permission_; }
  const FeatureProvider& behavior() const { return behavior_; }

 private:
  // Held by value: there are exactly four, and they live for the life of the
  // process.
  FeatureProvider api_;
  FeatureProvider manifest_;
  FeatureProvider permission_;
  FeatureProvider behavior_;
};

const FeatureProviderStatic& GetFeatureProviderStatic() {
  static base::NoDestructor<FeatureProviderStatic> instance;
  return *instance;
}

const Feature* GetFeatureFromProvider(const FeatureProvider& provider,
                                      std::string_view provider_name,
                                      std::string_view feature_name) {
  const Feature* feature = provider.GetFeature(feature_name);
  // We should always refer to existing features, but we can't CHECK here
  // due to flaky JSONReader fails, see: crbug.com/40302033, crbug.com/41248827
  DCHECK(feature) << "Feature \"" << feature_name << "\" not found in "
                  << "FeatureProvider \"" << provider_name << "\"";
  return feature;
}

}  // namespace

FeatureProvider::FeatureProvider() = default;
FeatureProvider::FeatureProvider(FeatureProvider&&) = default;
FeatureProvider& FeatureProvider::operator=(FeatureProvider&&) = default;
FeatureProvider::~FeatureProvider() = default;

// static
const FeatureProvider* FeatureProvider::GetAPIFeatures() {
  return &GetFeatureProviderStatic().api();
}

// static
const FeatureProvider* FeatureProvider::GetManifestFeatures() {
  return &GetFeatureProviderStatic().manifest();
}

// static
const FeatureProvider* FeatureProvider::GetPermissionFeatures() {
  return &GetFeatureProviderStatic().permission();
}

// static
const FeatureProvider* FeatureProvider::GetBehaviorFeatures() {
  return &GetFeatureProviderStatic().behavior();
}

// static
const Feature* FeatureProvider::GetAPIFeature(std::string_view name) {
  return GetFeatureFromProvider(GetFeatureProviderStatic().api(), "api", name);
}

// static
const Feature* FeatureProvider::GetManifestFeature(std::string_view name) {
  return GetFeatureFromProvider(GetFeatureProviderStatic().manifest(),
                                "manifest", name);
}

// static
const Feature* FeatureProvider::GetPermissionFeature(std::string_view name) {
  return GetFeatureFromProvider(GetFeatureProviderStatic().permission(),
                                "permission", name);
}

// static
const Feature* FeatureProvider::GetBehaviorFeature(std::string_view name) {
  return GetFeatureFromProvider(GetFeatureProviderStatic().behavior(),
                                "behavior", name);
}

const Feature* FeatureProvider::GetFeature(std::string_view name) const {
  auto it =
      std::ranges::lower_bound(features_, name, std::less<>(), &Feature::name);
  return it != features_.end() && (*it)->name() == name ? *it : nullptr;
}

const Feature* FeatureProvider::GetParent(const Feature& feature) const {
  if (feature.no_parent()) {
    return nullptr;
  }

  const std::string_view name = feature.name();
  const size_t last_dot = name.rfind('.');
  if (last_dot == std::string_view::npos) {
    return nullptr;
  }
  return GetFeature(name.substr(0, last_dot));
}

// Children of a given API are named starting with parent.name()+".", which
// means they'll be contiguous in the sorted features_ registry.
std::vector<const Feature*> FeatureProvider::GetChildren(
    const Feature& parent) const {
  std::string prefix = base::StrCat({parent.name(), "."});
  auto first_child = std::ranges::lower_bound(
      features_, std::string_view(prefix), std::less<>(), &Feature::name);

  // All children have names before (parent.name() + ('.'+1)).
  ++prefix.back();
  auto after_children = std::ranges::lower_bound(
      features_, std::string_view(prefix), std::less<>(), &Feature::name);

  std::vector<const Feature*> result;
  result.reserve(std::distance(first_child, after_children));
  for (auto it = first_child; it != after_children; ++it) {
    if (!(*it)->no_parent()) {
      result.push_back(*it);
    }
  }
  return result;
}

FeatureSpan FeatureProvider::GetAllFeatures() const {
  return features_;
}

void FeatureProvider::ReserveStaticFeatures(size_t count) {
  features_.reserve(features_.size() + count);
}

void FeatureProvider::AddStaticFeatures(FeatureSpan features) {
  for (size_t i = 0; i < features.size(); ++i) {
    CHECK(features[i]);
    if (i > 0) {
      CHECK_LT(features[i - 1]->name(), features[i]->name())
          << "Feature table is not strictly ordered by name";
    }
  }

  // Merge from the back so that no temporary buffer is needed: the existing
  // features shift toward the end of the grown vector while the new features
  // are read directly from `features`.
  size_t existing = features_.size();
  size_t incoming = features.size();
  features_.resize(existing + incoming);
  size_t out = features_.size();
  while (incoming > 0) {
    const Feature* next = features[incoming - 1];
    if (existing > 0) {
      const Feature* current = features_[existing - 1];
      CHECK_NE(current->name(), next->name())
          << "Duplicate feature registration: " << next->name();
      if (next->name() < current->name()) {
        features_[--out] = current;
        --existing;
        continue;
      }
    }
    features_[--out] = next;
    --incoming;
  }
}

}  // namespace extensions
