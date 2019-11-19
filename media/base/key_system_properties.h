// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_KEY_SYSTEM_PROPERTIES_H_
#define MEDIA_BASE_KEY_SYSTEM_PROPERTIES_H_

#include <string>

#include "build/build_config.h"
#include "media/base/decrypt_config.h"
#include "media/base/eme_constants.h"
#include "media/base/media_export.h"

namespace media {

// Provides an interface for querying the properties of a registered key system.
class MEDIA_EXPORT KeySystemProperties {
 public:
  virtual ~KeySystemProperties() {}

  // Gets the name of this key system.
  virtual std::string GetKeySystemName() const = 0;

  // Returns whether |init_data_type| is supported by this key system.
  virtual bool IsSupportedInitDataType(
      EmeInitDataType init_data_type) const = 0;

  // Returns the configuration rule for supporting |encryption_scheme|.
  virtual EmeConfigRule GetEncryptionSchemeConfigRule(
      EncryptionScheme encryption_scheme) const = 0;

  // Returns the codecs supported by this key system.
  virtual SupportedCodecs GetSupportedCodecs() const = 0;

  // Returns the codecs can be used with the hardware-secure mode of this key
  // system. The codecs may be supported with any hardware-based robustness
  // level. Other parts of the code handle reporting appropriate levels of
  // robustness support for audio and video.
  virtual SupportedCodecs GetSupportedHwSecureCodecs() const;

  // Returns the configuration rule for supporting a robustness requirement.
  virtual EmeConfigRule GetRobustnessConfigRule(
      EmeMediaType media_type,
      const std::string& requested_robustness) const = 0;

  // Returns the support this key system provides for persistent-license
  // sessions.
  virtual EmeSessionTypeSupport GetPersistentLicenseSessionSupport() const = 0;

  // Returns the support this key system provides for persistent-usage-record
  // sessions.
  virtual EmeSessionTypeSupport GetPersistentUsageRecordSessionSupport()
      const = 0;

  // Returns the support this key system provides for persistent state.
  virtual EmeFeatureSupport GetPersistentStateSupport() const = 0;

  // Returns the support this key system provides for distinctive identifiers.
  virtual EmeFeatureSupport GetDistinctiveIdentifierSupport() const = 0;

  // Returns whether AesDecryptor can be used for this key system.
  virtual bool UseAesDecryptor() const;
};

}  // namespace media

#endif  // MEDIA_BASE_KEY_SYSTEM_PROPERTIES_H_
