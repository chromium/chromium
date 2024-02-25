// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_FEATURE_H_
#define EXTENSIONS_COMMON_FEATURES_FEATURE_H_

#include <map>
#include <set>
#include <string>
#include <string_view>

#include "extensions/common/context_data.h"
#include "extensions/common/hashed_extension_id.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/context_type.mojom-forward.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"

class GURL;

namespace extensions {

inline constexpr int kUnspecifiedContextId = -1;

class Extension;

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
    LACROS_PLATFORM,
    LINUX_PLATFORM,
    MACOSX_PLATFORM,
    WIN_PLATFORM,
    FUCHSIA_PLATFORM,
  };

  // Whether a feature is available in a given situation or not, and if not,
  // why not.
  enum AvailabilityResult {
    IS_AVAILABLE,
    NOT_FOUND_IN_ALLOWLIST,
    INVALID_URL,
    INVALID_TYPE,
    INVALID_CONTEXT,
    INVALID_LOCATION,
    INVALID_PLATFORM,
    INVALID_MIN_MANIFEST_VERSION,
    INVALID_MAX_MANIFEST_VERSION,
    INVALID_SESSION_TYPE,
    NOT_PRESENT,
    UNSUPPORTED_CHANNEL,
    FOUND_IN_BLOCKLIST,
    MISSING_COMMAND_LINE_SWITCH,
    FEATURE_FLAG_DISABLED,
    REQUIRES_DEVELOPER_MODE,
    MISSING_DELEGATED_AVAILABILITY_CHECK,
    FAILED_DELEGATED_AVAILABILITY_CHECK,
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
      std::map<std::string, DelegatedAvailabilityCheckHandler>;

  // Container for AvailabilityResult that also exposes a user-visible error
  // message in cases where the feature is not available.
  class Availability {
   public:
    Availability(AvailabilityResult result, const std::string& message)
        : result_(result), message_(message) {}

    AvailabilityResult result() const { return result_; }
    bool is_available() const { return result_ == IS_AVAILABLE; }
    const std::string& message() const { return message_; }

   private:
    friend class SimpleFeature;
    friend class Feature;

    const AvailabilityResult result_;
    const std::string message_;
  };

  Feature();
  virtual ~Feature();

  const std::string& name() const { return name_; }
  // Note that this arg is passed as a string_view to avoid a lot of bloat from
  // inlined std::string code.
  void set_name(std::string_view name);
  const std::string& alias() const { return alias_; }
  void set_alias(std::string_view alias);
  const std::string& source() const { return source_; }
  void set_source(std::string_view source);
  bool no_parent() const { return no_parent_; }

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

  // Returns true if the feature is available to |extension|.
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

  std::string name_;
  std::string alias_;
  std::string source_;
  bool no_parent_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_FEATURE_H_
