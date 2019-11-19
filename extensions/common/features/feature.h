// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_FEATURE_H_
#define EXTENSIONS_COMMON_FEATURES_FEATURE_H_

#include <set>
#include <string>

#include "base/strings/string_piece.h"
#include "base/values.h"
#include "extensions/common/hashed_extension_id.h"
#include "extensions/common/manifest.h"

class GURL;

namespace extensions {

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
  // The JavaScript contexts the feature is supported in.
  enum Context {
    UNSPECIFIED_CONTEXT,
    BLESSED_EXTENSION_CONTEXT,
    UNBLESSED_EXTENSION_CONTEXT,
    CONTENT_SCRIPT_CONTEXT,
    WEB_PAGE_CONTEXT,
    BLESSED_WEB_PAGE_CONTEXT,
    WEBUI_CONTEXT,
    LOCK_SCREEN_EXTENSION_CONTEXT,
  };

  // The platforms the feature is supported in.
  enum Platform {
    UNSPECIFIED_PLATFORM,
    CHROMEOS_PLATFORM,
    LINUX_PLATFORM,
    MACOSX_PLATFORM,
    WIN_PLATFORM
  };

  // Whether a feature is available in a given situation or not, and if not,
  // why not.
  enum AvailabilityResult {
    IS_AVAILABLE,
    NOT_FOUND_IN_WHITELIST,
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
    FOUND_IN_BLACKLIST,
    MISSING_COMMAND_LINE_SWITCH,
  };

  // Container for AvailabiltyResult that also exposes a user-visible error
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
  // Note that this arg is passed as a StringPiece to avoid a lot of bloat from
  // inlined std::string code.
  void set_name(base::StringPiece name);
  const std::string& alias() const { return alias_; }
  void set_alias(base::StringPiece alias);
  const std::string& source() const { return source_; }
  void set_source(base::StringPiece source);
  bool no_parent() const { return no_parent_; }

  // Gets the platform the code is currently running on.
  static Platform GetCurrentPlatform();

  // Tests whether this is an internal API or not.
  virtual bool IsInternal() const = 0;

  // Returns true if the feature is available to be parsed into a new extension
  // manifest.
  Availability IsAvailableToManifest(const HashedExtensionId& hashed_id,
                                     Manifest::Type type,
                                     Manifest::Location location,
                                     int manifest_version) const {
    return IsAvailableToManifest(hashed_id, type, location, manifest_version,
                                 GetCurrentPlatform());
  }
  virtual Availability IsAvailableToManifest(const HashedExtensionId& hashed_id,
                                             Manifest::Type type,
                                             Manifest::Location location,
                                             int manifest_version,
                                             Platform platform) const = 0;

  // Returns true if the feature is available to |extension|.
  Availability IsAvailableToExtension(const Extension* extension) const;

  // Returns true if the feature is available to be used in the specified
  // extension and context.
  Availability IsAvailableToContext(const Extension* extension,
                                    Context context,
                                    const GURL& url) const {
    return IsAvailableToContext(extension, context, url, GetCurrentPlatform());
  }
  virtual Availability IsAvailableToContext(const Extension* extension,
                                            Context context,
                                            const GURL& url,
                                            Platform platform) const = 0;

  // Returns true if the feature is available to the current environment,
  // without needing to know information about an Extension or any other
  // contextual information. Typically used when the Feature is purely
  // configured by command line flags and/or Chrome channel.
  //
  // Generally try not to use this function. Even if you don't think a Feature
  // relies on an Extension now - maybe it will, one day, so if there's an
  // Extension available (or a runtime context, etc) then use the more targeted
  // method instead.
  virtual Availability IsAvailableToEnvironment() const = 0;

  virtual bool IsIdInBlocklist(const HashedExtensionId& hashed_id) const = 0;
  virtual bool IsIdInAllowlist(const HashedExtensionId& hashed_id) const = 0;

 protected:
  std::string name_;
  std::string alias_;
  std::string source_;
  bool no_parent_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_FEATURE_H_
