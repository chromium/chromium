// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_SIMPLE_FEATURE_H_
#define EXTENSIONS_COMMON_FEATURES_SIMPLE_FEATURE_H_

#include <stddef.h>

#include <initializer_list>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/values.h"
#include "components/version_info/version_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/common/manifest.h"

namespace extensions {

class FeatureProviderTest;
class ExtensionAPITest;

class SimpleFeature : public Feature {
 public:
  // Used by tests to override the cached --whitelisted-extension-id.
  // NOTE: Not thread-safe! This is because it sets extension id on global
  // singleton during its construction and destruction.
  class ScopedThreadUnsafeAllowlistForTest {
   public:
    explicit ScopedThreadUnsafeAllowlistForTest(const std::string& id);
    ~ScopedThreadUnsafeAllowlistForTest();

   private:
    std::string previous_id_;

    DISALLOW_COPY_AND_ASSIGN(ScopedThreadUnsafeAllowlistForTest);
  };

  SimpleFeature();
  ~SimpleFeature() override;

  Availability IsAvailableToContext(const Extension* extension,
                                    Context context) const {
    return IsAvailableToContext(extension, context, GURL());
  }
  Availability IsAvailableToContext(const Extension* extension,
                                    Context context,
                                    Platform platform) const {
    return IsAvailableToContext(extension, context, GURL(), platform);
  }
  Availability IsAvailableToContext(const Extension* extension,
                                    Context context,
                                    const GURL& url) const {
    return IsAvailableToContext(extension, context, url, GetCurrentPlatform());
  }

  // extension::Feature:
  Availability IsAvailableToManifest(const HashedExtensionId& hashed_id,
                                     Manifest::Type type,
                                     Manifest::Location location,
                                     int manifest_version,
                                     Platform platform) const override;
  Availability IsAvailableToContext(const Extension* extension,
                                    Context context,
                                    const GURL& url,
                                    Platform platform) const override;
  Availability IsAvailableToEnvironment() const override;
  bool IsInternal() const override;
  bool IsIdInBlocklist(const HashedExtensionId& hashed_id) const override;
  bool IsIdInAllowlist(const HashedExtensionId& hashed_id) const override;

  static bool IsIdInArray(const std::string& extension_id,
                          const char* const array[],
                          size_t array_length);

  // Similar to Manifest::Location, these are the classes of locations
  // supported in feature files. These should only be used in this class and in
  // generated files.
  enum Location {
    COMPONENT_LOCATION,
    EXTERNAL_COMPONENT_LOCATION,
    POLICY_LOCATION,
    UNPACKED_LOCATION,
  };

  // Setters used by generated code to create the feature.
  // NOTE: These setters use base::StringPiece and std::initalizer_list rather
  // than std::string and std::vector for binary size reasons. Using STL types
  // directly in the header means that code that doesn't already have that exact
  // type ends up triggering many implicit conversions which are all inlined.
  void set_blocklist(std::initializer_list<const char* const> blocklist);
  void set_channel(version_info::Channel channel) { channel_ = channel; }
  void set_command_line_switch(base::StringPiece command_line_switch);
  void set_component_extensions_auto_granted(bool granted) {
    component_extensions_auto_granted_ = granted;
  }
  void set_contexts(std::initializer_list<Context> contexts);
  void set_dependencies(std::initializer_list<const char* const> dependencies);
  void set_extension_types(std::initializer_list<Manifest::Type> types);
  void set_session_types(std::initializer_list<FeatureSessionType> types);
  void set_internal(bool is_internal) { is_internal_ = is_internal; }
  void set_disallow_for_service_workers(bool disallow) {
    disallow_for_service_workers_ = disallow;
  }
  void set_location(Location location) { location_ = location; }
  // set_matches() is an exception to pass-by-value since we construct an
  // URLPatternSet from the vector of strings.
  // TODO(devlin): Pass in an URLPatternSet directly.
  void set_matches(std::initializer_list<const char* const> matches);
  void set_max_manifest_version(int max_manifest_version) {
    max_manifest_version_ = max_manifest_version;
  }
  void set_min_manifest_version(int min_manifest_version) {
    min_manifest_version_ = min_manifest_version;
  }
  void set_noparent(bool no_parent) { no_parent_ = no_parent; }
  void set_platforms(std::initializer_list<Platform> platforms);
  void set_allowlist(std::initializer_list<const char* const> allowlist);

 protected:
  // Accessors used by subclasses in feature verification.
  const std::vector<std::string>& blocklist() const { return blocklist_; }
  const std::vector<std::string>& allowlist() const { return allowlist_; }
  const std::vector<Manifest::Type>& extension_types() const {
    return extension_types_;
  }
  const std::vector<Platform>& platforms() const { return platforms_; }
  const std::vector<Context>& contexts() const { return contexts_; }
  const std::vector<std::string>& dependencies() const { return dependencies_; }
  const base::Optional<version_info::Channel> channel() const {
    return channel_;
  }
  const base::Optional<Location> location() const { return location_; }
  const base::Optional<int> min_manifest_version() const {
    return min_manifest_version_;
  }
  const base::Optional<int> max_manifest_version() const {
    return max_manifest_version_;
  }
  const base::Optional<std::string>& command_line_switch() const {
    return command_line_switch_;
  }
  bool component_extensions_auto_granted() const {
    return component_extensions_auto_granted_;
  }
  const URLPatternSet& matches() const { return matches_; }

  std::string GetAvailabilityMessage(AvailabilityResult result,
                                     Manifest::Type type,
                                     const GURL& url,
                                     Context context,
                                     version_info::Channel channel,
                                     FeatureSessionType session_type) const;

  // Handy utilities which construct the correct availability message.
  Availability CreateAvailability(AvailabilityResult result) const;
  Availability CreateAvailability(AvailabilityResult result,
                                  Manifest::Type type) const;
  Availability CreateAvailability(AvailabilityResult result,
                                  const GURL& url) const;
  Availability CreateAvailability(AvailabilityResult result,
                                  Context context) const;
  Availability CreateAvailability(AvailabilityResult result,
                                  version_info::Channel channel) const;
  Availability CreateAvailability(AvailabilityResult result,
                                  FeatureSessionType session_type) const;

 private:
  friend struct FeatureComparator;
  FRIEND_TEST_ALL_PREFIXES(FeatureProviderTest, ManifestFeatureTypes);
  FRIEND_TEST_ALL_PREFIXES(FeatureProviderTest, PermissionFeatureTypes);
  FRIEND_TEST_ALL_PREFIXES(ExtensionAPITest, DefaultConfigurationFeatures);
  FRIEND_TEST_ALL_PREFIXES(FeaturesGenerationTest, FeaturesTest);

  // Holds String to Enum value mappings.
  struct Mappings;

  static bool IsIdInList(const HashedExtensionId& hashed_id,
                         const std::vector<std::string>& list);

  bool MatchesManifestLocation(Manifest::Location manifest_location) const;

  // Checks if the feature is allowed in a session of type |session_type|
  // (based on session type feature restrictions).
  bool MatchesSessionTypes(FeatureSessionType session_type) const;

  Availability CheckDependencies(
      const base::Callback<Availability(const Feature*)>& checker) const;

  static bool IsValidExtensionId(const std::string& extension_id);
  static bool IsValidHashedExtensionId(const HashedExtensionId& hashed_id);

  // Returns the availability of the feature with respect to the basic
  // environment Chrome is running in.
  Availability GetEnvironmentAvailability(
      Platform platform,
      version_info::Channel channel,
      FeatureSessionType session_type) const;

  // Returns the availability of the feature with respect to a given extension's
  // properties.
  Availability GetManifestAvailability(const HashedExtensionId& hashed_id,
                                       Manifest::Type type,
                                       Manifest::Location location,
                                       int manifest_version) const;

  // Returns the availability of the feature with respect to a given context.
  Availability GetContextAvailability(Context context,
                                      const GURL& url,
                                      bool is_for_service_worker) const;

  // For clarity and consistency, we handle the default value of each of these
  // members the same way: it matches everything. It is up to the higher level
  // code that reads Features out of static data to validate that data and set
  // sensible defaults.
  std::vector<std::string> blocklist_;
  std::vector<std::string> allowlist_;
  std::vector<std::string> dependencies_;
  std::vector<Manifest::Type> extension_types_;
  std::vector<FeatureSessionType> session_types_;
  std::vector<Context> contexts_;
  std::vector<Platform> platforms_;
  URLPatternSet matches_;

  base::Optional<Location> location_;
  base::Optional<int> min_manifest_version_;
  base::Optional<int> max_manifest_version_;
  base::Optional<std::string> command_line_switch_;
  base::Optional<version_info::Channel> channel_;
  // Whether to ignore channel-based restrictions (such as because the user has
  // enabled experimental extension APIs). Note: this is lazily calculated, and
  // then cached.
  mutable base::Optional<bool> ignore_channel_;

  bool component_extensions_auto_granted_;
  bool is_internal_;
  bool disallow_for_service_workers_;

  DISALLOW_COPY_AND_ASSIGN(SimpleFeature);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_SIMPLE_FEATURE_H_
