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

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/raw_span.h"
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

// A retained view of a statically stored array. Its consteval constructors
// enforce the lifetime without changing base::span's representation.
template <typename T>
class StaticSpan {
 public:
  template <size_t N>
  explicit consteval StaticSpan(const T (&arr)[N]) : span_(arr) {}
  template <size_t N>
  explicit consteval StaticSpan(const std::array<T, N>& arr)
      : span_(arr) {}
  consteval StaticSpan() = default;

  constexpr base::span<const T> span() const { return span_; }

 private:
  RAW_PTR_EXCLUSION base::span<const T> span_;
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

  SimpleFeature();

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
  enum class Location {
    kComponent,
    kExternalComponent,
    kPolicy,
    kUnpacked,
  };

  void set_blocklist(StaticSpan<std::string_view> blocklist);
  void set_blocklist(std::initializer_list<std::string_view> blocklist) =
      delete;
  void set_channel(version_info::Channel channel) { channel_ = channel; }
  void set_command_line_switch(std::string_view command_line_switch);
  void set_component_extensions_auto_granted(bool granted) {
    component_extensions_auto_granted_ = granted;
  }
  void set_contexts(std::initializer_list<mojom::ContextType> contexts);
  void set_dependencies(StaticSpan<std::string_view> dependencies);
  void set_dependencies(std::initializer_list<std::string_view> dependencies) =
      delete;
  void set_extension_types(std::initializer_list<Manifest::Type> types);
  void set_feature_flag(std::string_view feature_flag);
  void set_session_types(
      std::initializer_list<mojom::FeatureSessionType> types);
  void set_internal(bool is_internal) { is_internal_ = is_internal; }
  void set_requires_delegated_availability_check(
      bool requires_delegated_availability_check) {
    requires_delegated_availability_check_ =
        requires_delegated_availability_check;
  }
  void set_developer_mode_only(bool is_developer_mode_only) {
    developer_mode_only_ = is_developer_mode_only;
  }
  void set_disallow_for_service_workers(bool disallow) {
    disallow_for_service_workers_ = disallow;
  }
  void set_location(Location location) { location_ = location; }
  void set_matches(StaticSpan<std::string_view> matches);
  void set_matches(std::initializer_list<std::string_view> matches) = delete;
  void set_max_manifest_version(int max_manifest_version) {
    max_manifest_version_ = max_manifest_version;
  }
  void set_min_manifest_version(int min_manifest_version) {
    min_manifest_version_ = min_manifest_version;
  }
  void set_noparent(bool no_parent) { no_parent_ = no_parent; }
  void set_platforms(std::initializer_list<Platform> platforms);
  void set_allowlist(StaticSpan<std::string_view> allowlist);
  void set_allowlist(std::initializer_list<std::string_view> allowlist) =
      delete;

 protected:
  // Accessors used by subclasses in feature verification.
  base::span<const std::string_view> blocklist() const { return blocklist_; }
  base::span<const std::string_view> allowlist() const { return allowlist_; }
  const std::vector<Manifest::Type>& extension_types() const {
    return extension_types_;
  }
  const std::vector<Platform>& platforms() const { return platforms_; }
  const std::optional<std::vector<mojom::ContextType>>& contexts() const {
    return contexts_;
  }
  base::span<const std::string_view> dependencies() const {
    return dependencies_;
  }
  const std::optional<version_info::Channel> channel() const {
    return channel_;
  }
  const std::optional<Location> location() const { return location_; }
  const std::optional<int> min_manifest_version() const {
    return min_manifest_version_;
  }
  const std::optional<int> max_manifest_version() const {
    return max_manifest_version_;
  }
  const std::optional<std::string>& command_line_switch() const {
    return command_line_switch_;
  }
  bool component_extensions_auto_granted() const {
    return component_extensions_auto_granted_;
  }
  base::span<const std::string_view> match_patterns() const LIFETIME_BOUND {
    return match_patterns_;
  }

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

  // Returns true if `url` matches any of `match_patterns_`.
  bool MatchesURL(const GURL& url) const;

  // For clarity and consistency, we handle the default value of each of these
  // members the same way: it matches everything. It is up to the higher level
  // code that reads Features out of static data to validate that data and set
  // sensible defaults.
  base::raw_span<const std::string_view> blocklist_;
  base::raw_span<const std::string_view> allowlist_;
  base::raw_span<const std::string_view> dependencies_;
  std::vector<Manifest::Type> extension_types_;
  std::vector<mojom::FeatureSessionType> session_types_;
  std::optional<std::vector<mojom::ContextType>> contexts_;
  std::vector<Platform> platforms_;
  // The feature's URL match patterns, parsed into a transient URLPattern on
  // demand. Generated features provide statically allocated strings.
  base::raw_span<const std::string_view> match_patterns_;

  std::optional<Location> location_;
  std::optional<int> min_manifest_version_;
  std::optional<int> max_manifest_version_;
  std::optional<std::string> command_line_switch_;
  std::optional<std::string> feature_flag_;
  std::optional<version_info::Channel> channel_;
  // Whether to ignore channel-based restrictions (such as because the user has
  // enabled experimental extension APIs). Note: this is lazily calculated, and
  // then cached.
  mutable std::optional<bool> ignore_channel_;

  // If set and the feature needs to be overridden, this is the handler used
  // to perform the override availability check.
  DelegatedAvailabilityCheckHandler delegated_availability_check_handler_;

  // Whether access to the feature is automatically granted to component
  // extensions. This defaults to true to maintain backward compatibility and
  // the expectation that component extensions are trusted.
  bool component_extensions_auto_granted_{true};
  bool is_internal_;
  bool requires_delegated_availability_check_{false};
  bool developer_mode_only_{false};
  bool disallow_for_service_workers_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_SIMPLE_FEATURE_H_
