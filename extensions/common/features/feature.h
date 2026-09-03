// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_FEATURE_H_
#define EXTENSIONS_COMMON_FEATURES_FEATURE_H_

#include <stddef.h>

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "extensions/common/context_data.h"
#include "extensions/common/hashed_extension_id.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/context_type.mojom-forward.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"

class GURL;

namespace extensions {

inline constexpr int kUnspecifiedContextId = -1;

class Extension;

// A retained pointer to immutable descriptor data. Its consteval constructor
// enforces static storage and compile-time-readable contents.
template <typename T>
class StaticFeatureData {
 public:
  template <typename U>
    requires std::is_same_v<U, const T>
  explicit consteval StaticFeatureData(U& data) : data_(&data) {
    // Read the complete descriptor during constant evaluation to reject
    // statically stored data whose contents are dynamically initialized.
    [[maybe_unused]] T validated_data = data;
  }

  constexpr const T* get() const { return data_; }
  constexpr const T* operator->() const { return data_; }

 private:
  // Safe because construction requires static storage.
  RAW_PTR_EXCLUSION const T* data_;
};

template <typename T>
StaticFeatureData(T&) -> StaticFeatureData<std::remove_const_t<T>>;

// A pointer to a static NUL-terminated string. Half the size of a
// std::string_view, which matters because a handful of features set these
// fields but every feature pays for them; the length is recovered by scanning
// on the cold paths that read them.
//
// The consteval constructor's attribute reads a character out of the array,
// which requires the contents, not just the address, to be compile-time
// constant. A bare const char* is the same size but would accept a mutable or
// dynamically initialized global.
class StaticCString {
 public:
  // Absent by default; nullptr is the sentinel, so no std::optional is needed.
  constexpr StaticCString() = default;

  template <size_t N>
  explicit consteval StaticCString(const char (&string)[N])
      ENABLE_IF_ATTR(string[N - 1u] == '\0', "requires a NUL-terminated string")
      : data_(string) {}

  constexpr bool has_value() const { return data_ != nullptr; }

  // Stops at an embedded NUL. The attribute above only constrains the final
  // byte, so a literal containing one still compiles.
  constexpr std::string_view string_view() const {
    return data_ ? std::string_view(data_) : std::string_view();
  }

 private:
  // Safe because construction requires static storage.
  RAW_PTR_EXCLUSION const char* data_ = nullptr;
};

// Immutable identity shared by every feature. Generated descriptors initialize
// this with designated initializers, so the member order must match
// FEATURE_DATA_FIELD_ORDER in tools/json_schema_compiler/feature_compiler.py.
struct FeatureData {
  std::string_view name;
  // Set by a handful of features, so these hold only a pointer rather than
  // pay for a length in every descriptor. Both are read on cold paths.
  StaticCString alias;
  StaticCString source;
  bool no_parent = false;
};

// Represents a single feature accessible to an extension developer, such as a
// top-level manifest key, a permission, or a programmatic API. A feature can
// express requirements for where it can be accessed, and supports testing
// support for those requirements. If platforms are not specified, then feature
// is available on all platforms.
//
// See //chrome/common/extensions/api/_features.md for a description of feature
// usage and types.
class Feature {
 public:
  // The platforms the feature is supported in.
  enum Platform {
    UNSPECIFIED_PLATFORM,
    CHROMEOS_PLATFORM,
    LINUX_PLATFORM,
    MACOSX_PLATFORM,
    WIN_PLATFORM,
    DESKTOP_ANDROID_PLATFORM,
  };

  // Whether a feature is available in a given situation or not, and if not,
  // why not.
  // Note: do not reorder or remove enum values because the order impacts
  // result_as_int32() used by V8ContextNativeHandler::GetAvailability().
  enum class AvailabilityResult {
    kIsAvailable,
    kNotFoundInAllowlist,
    kInvalidUrl,
    kInvalidType,
    kInvalidContext,
    kInvalidLocation,
    kInvalidPlatform,
    kInvalidMinManifestVersion,
    kInvalidMaxManifestVersion,
    kInvalidSessionType,
    kNotPresent,
    kUnsupportedChannel,
    kFoundInBlocklist,
    kMissingCommandLineSwitch,
    kFeatureFlagDisabled,
    kRequiresDeveloperMode,
    kMissingDelegatedAvailabilityCheck,
    kFailedDelegatedAvailabilityCheck
  };

  // Shorthand for delegated availability check handler function signature. The
  // function signature's arguments should contain all of the arguments passed
  // into IsAvailableToContextImpl().
  using DelegatedAvailabilityCheckHandler =
      base::RepeatingCallback<bool(const std::string& api_full_name,
                                   const Extension* extension,
                                   mojom::ContextType context,
                                   const GURL& url,
                                   Platform platform,
                                   int context_id,
                                   bool check_developer_mode,
                                   const ContextData& context_data)>;

  // Mapping Feature::name() to override function.
  using FeatureDelegatedAvailabilityCheckMap =
      std::map<std::string, DelegatedAvailabilityCheckHandler, std::less<>>;

  // Container for AvailabilityResult that also exposes a user-visible error
  // message in cases where the feature is not available.
  class Availability {
   public:
    Availability(AvailabilityResult result, std::string message)
        : result_(result), message_(std::move(message)) {}

    AvailabilityResult result() const { return result_; }
    // Used by V8ContextNativeHandler::GetAvailability().
    int32_t result_as_int32() const { return static_cast<int32_t>(result_); }
    bool is_available() const {
      return result_ == AvailabilityResult::kIsAvailable;
    }
    const std::string& message() const LIFETIME_BOUND { return message_; }

   private:
    friend class SimpleFeature;
    friend class Feature;

    // Deliberately non-const. A const `message_` cannot transfer its buffer
    // during move construction, and either const member would delete the
    // assignment operators. Availability values are returned, stored, and
    // propagated, so const would force copies or prevent moves. constexpr is
    // not an alternative because the values are produced at runtime. The class
    // is still effectively immutable, since the members are private and
    // exposed only through const accessors.
    AvailabilityResult result_;
    std::string message_;
  };

  virtual ~Feature();

  std::string_view name() const { return feature_data_->name; }
  std::string_view alias() const { return feature_data_->alias.string_view(); }
  std::string_view source() const {
    return feature_data_->source.string_view();
  }
  bool no_parent() const { return feature_data_->no_parent; }

  // Gets the platform the code is currently running on.
  static Platform GetCurrentPlatform();

  // Tests whether this is an internal API or not.
  virtual bool IsInternal() const = 0;

  // Returns if this feature's availability requires a delegated availability
  // check.
  virtual bool RequiresDelegatedAvailabilityCheck() const = 0;

  // Sets the feature availability override handler to use.
  virtual void SetDelegatedAvailabilityCheckHandler(
      DelegatedAvailabilityCheckHandler handler) = 0;

  // Returns true if the feature is available to be parsed into a new extension
  // manifest.
  Availability IsAvailableToManifest(const HashedExtensionId& hashed_id,
                                     Manifest::Type type,
                                     mojom::ManifestLocation location,
                                     int manifest_version,
                                     int context_id) const {
    return IsAvailableToManifest(hashed_id, type, location, manifest_version,
                                 GetCurrentPlatform(), context_id);
  }
  virtual Availability IsAvailableToManifest(const HashedExtensionId& hashed_id,
                                             Manifest::Type type,
                                             mojom::ManifestLocation location,
                                             int manifest_version,
                                             Platform platform,
                                             int context_id) const = 0;

  // Returns true if the feature is available to `extension`.
  Availability IsAvailableToExtension(const Extension* extension) const;

  // Returns true if the feature is available to be used in the specified
  // extension and context.
  Availability IsAvailableToContext(const Extension* extension,
                                    mojom::ContextType context,
                                    const GURL& url,
                                    int context_id,
                                    const ContextData& context_data) const {
    return IsAvailableToContext(extension, context, url, GetCurrentPlatform(),
                                context_id, context_data);
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

  Availability IsAvailableToContextIgnoringDevMode(
      const Extension* extension,
      mojom::ContextType context,
      const GURL& url,
      Platform platform,
      int context_id,
      const ContextData& context_data) const {
    return IsAvailableToContextImpl(
        extension, context, url, platform, context_id,
        /*check_developer_mode=*/false, context_data);
  }
  // Returns true if the feature is available to the current environment,
  // without needing to know information about an Extension or any other
  // contextual information. Typically used when the Feature is purely
  // configured by command line flags and/or Chrome channel.
  //
  // Generally try not to use this function. Even if you don't think a Feature
  // relies on an Extension now - maybe it will, one day, so if there's an
  // Extension available (or a runtime context, etc) then use the more targeted
  // method instead.
  virtual Availability IsAvailableToEnvironment(int context_id) const = 0;

  virtual bool IsIdInBlocklist(const HashedExtensionId& hashed_id) const = 0;
  virtual bool IsIdInAllowlist(const HashedExtensionId& hashed_id) const = 0;

  bool HasDelegatedAvailabilityCheckHandlerForTesting() const;

 protected:
  friend class SimpleFeature;
  friend class ComplexFeature;

  explicit Feature(const FeatureData* feature_data);

  // These parameters should be kept in sync with
  // DelegatedAvailabilityCheckHandler.
  virtual Availability IsAvailableToContextImpl(
      const Extension* extension,
      mojom::ContextType context,
      const GURL& url,
      Platform platform,
      int context_id,
      bool check_developer_mode,
      const ContextData& context_data) const = 0;

  // Gets whether a feature availability override handler has been set.
  virtual bool HasDelegatedAvailabilityCheckHandler() const = 0;

  // Immutable configuration, owned by whoever constructed this feature. For
  // generated features this is static storage; tests own their own copy.
  RAW_PTR_EXCLUSION const FeatureData* feature_data_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_FEATURE_H_
