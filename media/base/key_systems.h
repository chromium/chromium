// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_KEY_SYSTEMS_H_
#define MEDIA_BASE_KEY_SYSTEMS_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "media/base/eme_constants.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_export.h"

namespace media {

// Provides an interface for querying registered key systems.
//
// Many of the original static methods are still available, they should be
// migrated into this interface over time (or removed).
class MEDIA_EXPORT KeySystems {
 public:
  virtual ~KeySystems() = default;

  // Updates the list of available key systems if it's not initialized or may be
  // out of date. Calls the `done_cb` when done.
  virtual void UpdateIfNeeded(base::OnceClosure done_cb) = 0;

  // Gets the base key system name, e.g. "org.chromium.foo".
  virtual std::string GetBaseKeySystemName(
      const std::string& key_system) const = 0;

  // Returns whether |key_system| is a supported key system.
  virtual bool IsSupportedKeySystem(const std::string& key_system) const = 0;

  // Whether the base key system name should be used for CDM creation.
  virtual bool ShouldUseBaseKeySystemName(
      const std::string& key_system) const = 0;

  // Returns whether AesDecryptor can be used for the given |key_system|.
  virtual bool CanUseAesDecryptor(const std::string& key_system) const = 0;

  // Returns whether |init_data_type| is supported by |key_system|.
  virtual bool IsSupportedInitDataType(
      const std::string& key_system,
      EmeInitDataType init_data_type) const = 0;

  // Returns the configuration rule for supporting |encryption_scheme|.
  virtual EmeConfig::Rule GetEncryptionSchemeConfigRule(
      const std::string& key_system,
      EncryptionScheme encryption_scheme) const = 0;

  // Returns the configuration rule for supporting a container and a list of
  // codecs.
  virtual EmeConfig::Rule GetContentTypeConfigRule(
      const std::string& key_system,
      EmeMediaType media_type,
      const std::string& container_mime_type,
      const std::vector<std::string>& codecs) const = 0;

  // Returns the configuration rule for supporting a robustness requirement.
  // If `hw_secure_requirement` is true, then the key system already has a HW
  // secure requirement, if false then it already has a requirement to disallow
  // HW secure; if null then there is no HW secure requirement to apply. This
  // does not imply that `requested_robustness` should be ignored, both rules
  // must be applied.
  // TODO(crbug.com/40179944): Refactor this and remove the
  // `hw_secure_requirement` argument.
  virtual EmeConfig::Rule GetRobustnessConfigRule(
      const std::string& key_system,
      EmeMediaType media_type,
      const std::string& requested_robustness,
      const bool* hw_secure_requirement) const = 0;

  // Returns the support |key_system| provides for persistent-license sessions.
  virtual EmeConfig::Rule GetPersistentLicenseSessionSupport(
      const std::string& key_system) const = 0;

  // Returns the support |key_system| provides for persistent state.
  virtual EmeFeatureSupport GetPersistentStateSupport(
      const std::string& key_system) const = 0;

  // Returns the support |key_system| provides for distinctive identifiers.
  virtual EmeFeatureSupport GetDistinctiveIdentifierSupport(
      const std::string& key_system) const = 0;
};

// Returns a name for `key_system` for UMA logging. When `use_hw_secure_codecs`
// is specified (non-nullopt), names with robustness will be returned for
// supported key systems.
MEDIA_EXPORT std::string GetKeySystemNameForUMA(
    const std::string& key_system,
    std::optional<bool> use_hw_secure_codecs = std::nullopt);

// Returns an int mapping to `key_system` suitable for UKM reporting. CdmConfig
// is not needed here because we can report CdmConfig fields in UKM directly.
MEDIA_EXPORT int GetKeySystemIntForUKM(const std::string& key_system);

}  // namespace media

#endif  // MEDIA_BASE_KEY_SYSTEMS_H_
