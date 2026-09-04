// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_SIMPLE_FEATURE_H_
#define EXTENSIONS_COMMON_FEATURES_SIMPLE_FEATURE_H_

#include <stddef.h>

#include <array>
#include <initializer_list>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "components/version_info/channel.h"
#include "extensions/common/context_data.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/context_type.mojom-forward.h"
#include "extensions/common/mojom/feature_session_type.mojom.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"

namespace extensions {

class FeatureProviderTest;
class ExtensionAPITest;
class ComplexFeature;
struct SimpleFeatureData;

// A retained view of a statically stored array. Its consteval constructors
// enforce the lifetime without changing base::span's representation.
template <typename T>
class StaticSpan {
 public:
  template <size_t N>
  explicit consteval StaticSpan(const T (&arr)[N]) : span_(arr) {}
  template <size_t N>
  explicit consteval StaticSpan(const std::array<T, N>& arr) : span_(arr) {}
  consteval StaticSpan() = default;

  constexpr base::span<const T> span() const { return span_; }

 private:
  // Safe because construction requires static storage.
  RAW_PTR_EXCLUSION base::span<const T> span_;
};

enum class SimpleFeatureLocation {
  kComponent,
  kExternalComponent,
  kPolicy,
  kUnpacked,
};

// Immutable configuration for a simple feature. Generated descriptors
// initialize this with designated initializers, so the member order must match
// SIMPLE_FEATURE_CONFIG_FIELD_ORDER in
// tools/json_schema_compiler/feature_compiler.py.
struct SimpleFeatureConfig {
  StaticSpan<std::string_view> blocklist;
  StaticSpan<std::string_view> allowlist;
  StaticSpan<std::string_view> dependencies;
  StaticSpan<Manifest::Type> extension_types;
  StaticSpan<mojom::FeatureSessionType> session_types;
  std::optional<StaticSpan<mojom::ContextType>> contexts;
  StaticSpan<Feature::Platform> platforms;
  StaticSpan<std::string_view> match_patterns;
  std::optional<SimpleFeatureLocation> location;
  std::optional<int> min_manifest_version;
  std::optional<int> max_manifest_version;
  StaticCString command_line_switch;
  StaticCString feature_flag;
  std::optional<version_info::Channel> channel;
  bool component_extensions_auto_granted = true;
  bool is_internal = false;
  bool requires_delegated_availability_check = false;
  bool developer_mode_only = false;
  bool disallow_for_service_workers = false;
};

class SimpleFeature : public Feature {
 public:
  // Used by tests to override the cached --allowlisted-extension-id.
  // NOTE: Not thread-safe! This is because it sets extension id on global
  // singleton during its construction and destruction.
  class ScopedThreadUnsafeAllowlistForTest {
   public:
    explicit ScopedThreadUnsafeAllowlistForTest(const std::string& id);
    explicit ScopedThreadUnsafeAllowlistForTest(
        const std::vector<std::string>& ids);

    static std::unique_ptr<ScopedThreadUnsafeAllowlistForTest>
    CreateFromCommaSeparated(const std::string& comma_separated_ids);

    ScopedThreadUnsafeAllowlistForTest(
        const ScopedThreadUnsafeAllowlistForTest&) = delete;
    ScopedThreadUnsafeAllowlistForTest& operator=(
        const ScopedThreadUnsafeAllowlistForTest&) = delete;

    ~ScopedThreadUnsafeAllowlistForTest();

   private:
    std::vector<std::string> previous_ids_;
  };

  explicit SimpleFeature(StaticFeatureData<SimpleFeatureData> data);

  SimpleFeature(const SimpleFeature&) = delete;
  SimpleFeature& operator=(const SimpleFeature&) = delete;

  ~SimpleFeature() override;

  Availability IsAvailableToContext(const Extension* extension,
                                    mojom::ContextType context,
                                    int context_id,
                                    const ContextData& context_data) const {
    return IsAvailableToContext(extension, context, GURL(), context_id,
                                context_data);
  }
  Availability IsAvailableToContext(const Extension* extension,
                                    mojom::ContextType context,
                                    Platform platform,
                                    int context_id,
                                    const ContextData& context_data) const {
    return IsAvailableToContextImpl(extension, context, GURL(), platform,
                                    context_id, true, context_data);
  }
  Availability IsAvailableToContext(const Extension* extension,
                                    mojom::ContextType context,
                                    const GURL& url,
                                    int context_id,
                                    const ContextData& context_data) const {
    return IsAvailableToContextImpl(extension, context, url,
                                    GetCurrentPlatform(), context_id, true,
                                    context_data);
  }
  Availability IsAvailableToContext(const Extension* extension,
                                    mojom::ContextType context,
                                    const GURL& url,
                                    Platform platform,
                                    int context_id,
                                    const ContextData& context_data) const {
    return IsAvailableToContextImpl(extension, context, url, platform,
                                    context_id, true, context_data);
  }

  // extension::Feature:
  Availability IsAvailableToManifest(const HashedExtensionId& hashed_id,
                                     Manifest::Type type,
                                     mojom::ManifestLocation location,
                                     int manifest_version,
                                     Platform platform,
                                     int context_id) const override;
  Availability IsAvailableToEnvironment(int context_id) const override;
  bool IsInternal() const override;
  bool IsIdInBlocklist(const HashedExtensionId& hashed_id) const override;
  bool IsIdInAllowlist(const HashedExtensionId& hashed_id) const override;
  bool RequiresDelegatedAvailabilityCheck() const override;
  void SetDelegatedAvailabilityCheckHandler(
      DelegatedAvailabilityCheckHandler handler) override;
  bool HasDelegatedAvailabilityCheckHandler() const override;

  // Similar to mojom::ManifestLocation, these are the classes of locations
  // supported in feature files. These should only be used in this class and in
  // generated files.
  using Location = SimpleFeatureLocation;

 protected:
  explicit SimpleFeature(const SimpleFeatureData* data);

  // Accessors used by subclasses in feature verification.
  base::span<const std::string_view> blocklist() const;
  base::span<const std::string_view> allowlist() const;
  base::span<const Manifest::Type> extension_types() const;
  base::span<const Platform> platforms() const;
  std::optional<base::span<const mojom::ContextType>> contexts() const;
  base::span<const std::string_view> dependencies() const;
  std::optional<version_info::Channel> channel() const;
  std::optional<Location> location() const;
  std::optional<int> min_manifest_version() const;
  std::optional<int> max_manifest_version() const;
  std::optional<std::string_view> command_line_switch() const;
  bool component_extensions_auto_granted() const;
  base::span<const std::string_view> match_patterns() const;

  std::string GetAvailabilityMessage(
      AvailabilityResult result,
      Manifest::Type type,
      const GURL& url,
      mojom::ContextType context,
      version_info::Channel channel,
      mojom::FeatureSessionType session_type) const;

  // Handy utilities which construct the correct availability message.
  Availability CreateAvailability(AvailabilityResult result) const;
  Availability CreateAvailability(AvailabilityResult result,
                                  Manifest::Type type) const;
  Availability CreateAvailability(AvailabilityResult result,
                                  const GURL& url) const;
  Availability CreateAvailability(AvailabilityResult result,
                                  mojom::ContextType context) const;
  Availability CreateAvailability(AvailabilityResult result,
                                  version_info::Channel channel) const;
  Availability CreateAvailability(AvailabilityResult result,
                                  mojom::FeatureSessionType session_type) const;

  Availability IsAvailableToContextImpl(
      const Extension* extension,
      mojom::ContextType context,
      const GURL& url,
      Platform platform,
      int context_id,
      bool check_developer_mode,
      const ContextData& context_data) const override;

 private:
  friend class ComplexFeature;
  friend struct FeatureComparator;
  FRIEND_TEST_ALL_PREFIXES(FeatureProviderTest, ManifestFeatureTypes);
  FRIEND_TEST_ALL_PREFIXES(FeatureProviderTest, PermissionFeatureTypes);
  FRIEND_TEST_ALL_PREFIXES(ExtensionAPITest, DefaultConfigurationFeatures);
  FRIEND_TEST_ALL_PREFIXES(FeaturesGenerationTest, FeaturesTest);

  // Holds String to Enum value mappings.
  struct Mappings;

  static Feature::Availability IsAvailableToContextForBind(
      const Extension* extension,
      mojom::ContextType context,
      const GURL& url,
      Feature::Platform platform,
      int context_id,
      const ContextData* context_data,
      const Feature* feature);

  static bool IsIdInList(const HashedExtensionId& hashed_id,
                         base::span<const std::string_view> list);

  bool MatchesManifestLocation(mojom::ManifestLocation manifest_location) const;

  // Checks if the feature is allowed in a session of type `session_type`
  // (based on session type feature restrictions).
  bool MatchesSessionTypes(mojom::FeatureSessionType session_type) const;

  Availability CheckDependencies(
      const base::RepeatingCallback<Availability(const Feature*)>& checker)
      const;

  static bool IsValidExtensionId(const ExtensionId& extension_id);
  static bool IsValidHashedExtensionId(const HashedExtensionId& hashed_id);

  base::span<const mojom::FeatureSessionType> session_types() const;
  StaticCString command_line_switch_data() const;
  StaticCString feature_flag() const;
  bool developer_mode_only() const;
  bool disallow_for_service_workers() const;

  // Returns the availability of the feature with respect to the basic
  // environment Chrome is running in.
  Availability GetEnvironmentAvailability(
      Platform platform,
      version_info::Channel channel,
      mojom::FeatureSessionType session_type,
      int context_id,
      bool check_developer_mode) const;

  // Returns the availability of the feature with respect to a given extension's
  // properties.
  Availability GetManifestAvailability(const HashedExtensionId& hashed_id,
                                       Manifest::Type type,
                                       mojom::ManifestLocation location,
                                       int manifest_version) const;

  // Returns the availability of the feature with respect to a given context.
  Availability GetContextAvailability(mojom::ContextType context,
                                      const GURL& url,
                                      bool is_for_service_worker) const;

  // Returns the result of running the installed delegated availability check
  // handler.
  Availability RunDelegatedAvailabilityCheck(
      const Extension* extension,
      mojom::ContextType context,
      const GURL& url,
      Platform platform,
      int context_id,
      bool check_developer_mode,
      const ContextData& context_data) const;

  bool MatchesURL(const GURL& url) const;

  // Immutable configuration, owned by whoever constructed this feature. For
  // generated features this is static storage; tests own their own copy.
  RAW_PTR_EXCLUSION const SimpleFeatureConfig* simple_feature_config_;

  // If set and the feature needs to be overridden, this is the handler used
  // to perform the override availability check.
  DelegatedAvailabilityCheckHandler delegated_availability_check_handler_;
};

struct SimpleFeatureData {
  FeatureData feature;
  SimpleFeatureConfig config;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_SIMPLE_FEATURE_H_
