// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_FEATURE_PROVIDER_H_
#define EXTENSIONS_COMMON_FEATURES_FEATURE_PROVIDER_H_

#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"

namespace extensions {

class Feature;

// Binding code relies on feature names remaining sorted. The keys reference the
// static string storage that backs every feature name, so they stay valid
// independently of the lifetime of the mapped Feature objects.
using FeatureMap = base::flat_map<std::string_view, raw_ptr<const Feature>>;

// Implemented by classes that can vend features.
class FeatureProvider {
 public:
  FeatureProvider();

  FeatureProvider(const FeatureProvider&) = delete;
  FeatureProvider& operator=(const FeatureProvider&) = delete;

  virtual ~FeatureProvider();

  // Gets a FeatureProvider for a specific type, like "permission".
  static const FeatureProvider* GetByName(std::string_view name);

  // Directly access the common FeatureProvider types.
  // Each is equivalent to GetByName('featuretype').
  static const FeatureProvider* GetAPIFeatures();
  static const FeatureProvider* GetManifestFeatures();
  static const FeatureProvider* GetPermissionFeatures();
  static const FeatureProvider* GetBehaviorFeatures();

  // Directly get Features from the common FeatureProvider types.
  // Each is equivalent to GetByName('featuretype')->GetFeature(name).
  // NOTE: These functions may return `nullptr` in case corresponding JSON file
  // got corrupted.
  static const Feature* GetAPIFeature(std::string_view name);
  static const Feature* GetManifestFeature(std::string_view name);
  static const Feature* GetPermissionFeature(std::string_view name);
  static const Feature* GetBehaviorFeature(std::string_view name);

  // Returns the feature with the specified name.
  const Feature* GetFeature(std::string_view name) const;

  // Returns the parent feature of `feature`, or null if there isn't one.
  const Feature* GetParent(const Feature& feature) const;

  // Returns the features inside the `parent` namespace, recursively.
  std::vector<const Feature*> GetChildren(const Feature& parent) const;

  // Returns a map containing all features described by this instance.
  // TODO(devlin): Rename this to be features().
  const FeatureMap& GetAllFeatures() const LIFETIME_BOUND;

  // Registers features without taking ownership. Each feature must outlive
  // this provider. The span is consumed synchronously, and every pointer must
  // be non-null.
  void AddStaticFeatures(base::span<const Feature* const> features);

 private:
  FeatureMap features_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_FEATURE_PROVIDER_H_
