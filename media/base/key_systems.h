// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_KEY_SYSTEMS_H_
#define MEDIA_BASE_KEY_SYSTEMS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "media/base/decrypt_config.h"
#include "media/base/eme_constants.h"
#include "media/base/media_export.h"
#include "media/media_buildflags.h"

namespace media {

// Provides an interface for querying registered key systems.
//
// Many of the original static methods are still available, they should be
// migrated into this interface over time (or removed).
//
// TODO(sandersd): Provide GetKeySystem() so that it is not necessary to pass
// |key_system| to every method. http://crbug.com/457438
class MEDIA_EXPORT KeySystems {
 public:
  static KeySystems* GetInstance();

  // Returns whether |key_system| is a supported key system.
  virtual bool IsSupportedKeySystem(const std::string& key_system) const = 0;

  // Returns whether AesDecryptor can be used for the given |key_system|.
  virtual bool CanUseAesDecryptor(const std::string& key_system) const = 0;

  // Returns whether |init_data_type| is supported by |key_system|.
  virtual bool IsSupportedInitDataType(
      const std::string& key_system,
      EmeInitDataType init_data_type) const = 0;

  // Returns the configuration rule for supporting |encryption_scheme|.
  virtual EmeConfigRule GetEncryptionSchemeConfigRule(
      const std::string& key_system,
      EncryptionScheme encryption_scheme) const = 0;

  // Returns the configuration rule for supporting a container and a list of
  // codecs.
  virtual EmeConfigRule GetContentTypeConfigRule(
      const std::string& key_system,
      EmeMediaType media_type,
      const std::string& container_mime_type,
      const std::vector<std::string>& codecs) const = 0;

  // Returns the configuration rule for supporting a robustness requirement.
  virtual EmeConfigRule GetRobustnessConfigRule(
      const std::string& key_system,
      EmeMediaType media_type,
      const std::string& requested_robustness) const = 0;

  // Returns the support |key_system| provides for persistent-license sessions.
  virtual EmeSessionTypeSupport GetPersistentLicenseSessionSupport(
      const std::string& key_system) const = 0;

  // Returns the support |key_system| provides for persistent-usage-record
  // sessions.
  virtual EmeSessionTypeSupport GetPersistentUsageRecordSessionSupport(
      const std::string& key_system) const = 0;

  // Returns the support |key_system| provides for persistent state.
  virtual EmeFeatureSupport GetPersistentStateSupport(
      const std::string& key_system) const = 0;

  // Returns the support |key_system| provides for distinctive identifiers.
  virtual EmeFeatureSupport GetDistinctiveIdentifierSupport(
      const std::string& key_system) const = 0;

 protected:
  virtual ~KeySystems() {}
};

// TODO(ddorwin): WebContentDecryptionModuleSessionImpl::initializeNewSession()
// is violating this rule! https://crbug.com/249976.
// Use for prefixed EME only!
MEDIA_EXPORT bool IsSupportedKeySystemWithInitDataType(
    const std::string& key_system,
    EmeInitDataType init_data_type);

// Returns a name for |key_system| suitable to UMA logging.
MEDIA_EXPORT std::string GetKeySystemNameForUMA(const std::string& key_system);

// Returns an int mapping to |key_system| suitable for UKM reporting.
MEDIA_EXPORT int GetKeySystemIntForUKM(const std::string& key_system);

// Returns whether AesDecryptor can be used for the given |key_system|.
MEDIA_EXPORT bool CanUseAesDecryptor(const std::string& key_system);

#if defined(UNIT_TEST)
// Helper functions to add container/codec types for testing purposes.
// Call AddCodecMaskForTesting() first to ensure the mask values passed to
// AddMimeTypeCodecMaskForTesting() already exist.
MEDIA_EXPORT void AddCodecMaskForTesting(EmeMediaType media_type,
                                         const std::string& codec,
                                         uint32_t mask);
MEDIA_EXPORT void AddMimeTypeCodecMaskForTesting(const std::string& mime_type,
                                                 uint32_t mask);
#endif  // defined(UNIT_TEST)

}  // namespace media

#endif  // MEDIA_BASE_KEY_SYSTEMS_H_
