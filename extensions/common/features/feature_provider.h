// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_FEATURE_PROVIDER_H_
#define EXTENSIONS_COMMON_FEATURES_FEATURE_PROVIDER_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace extensions {

class Feature;

// Note: Binding code (specifically native_extension_bindings_system.cc) relies
// on this being a sorted map.
using FeatureMap = std::map<std::string, std::unique_ptr<const Feature>>;

// Implemented by classes that can vend features.
class FeatureProvider {
 public:
  FeatureProvider();

  FeatureProvider(const FeatureProvider&) = delete;
  FeatureProvider& operator=(const FeatureProvider&) = delete;

  virtual ~FeatureProvider();

  // Gets a FeatureProvider for a specific type, like "permission".
  static const FeatureProvider* GetByName(const std::string& name);

  // Directly access the common FeatureProvider types.
  // Each is equivalent to GetByName('featuretype').
  static const FeatureProvider* GetAPIFeatures();
  static const FeatureProvider* GetManifestFeatures();
  static const FeatureProvider* GetPermissionFeatures();
  static const FeatureProvider* GetBehaviorFeatures();

  // Directly get Features from the common FeatureProvider types.
  // Each is equivalent to GetByName('featuretype')->GetFeature(name).
  // NOTE: These functions may return |nullptr| in case corresponding JSON file
  // got corrupted.
  static const Feature* GetAPIFeature(const std::string& name);
  static const Feature* GetManifestFeature(const std::string& name);
  static const Feature* GetPermissionFeature(const std::string& name);
  static const Feature* GetBehaviorFeature(const std::string& name);

  // Returns the feature with the specified name.
  const Feature* GetFeature(const std::string& name) const;

  // Returns the parent feature of |feature|, or null if there isn't one.
  const Feature* GetParent(const Feature& feature) const;

  // Returns the features inside the |parent| namespace, recursively.
  std::vector<const Feature*> GetChildren(const Feature& parent) const;

  // Returns a map containing all features described by this instance.
  // TODO(devlin): Rename this to be features().
  const FeatureMap& GetAllFeatures() const;

  void AddFeature(std::string_view name, std::unique_ptr<Feature> feature);

  // Takes ownership. Used in preference to unique_ptr variant to reduce size
  // of generated code.
  void AddFeature(std::string_view name, Feature* feature);

 private:
  FeatureMap features_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_FEATURE_PROVIDER_H_
