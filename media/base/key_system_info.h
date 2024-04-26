// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_KEY_SYSTEM_INFO_H_
#define MEDIA_BASE_KEY_SYSTEM_INFO_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "media/base/decrypt_config.h"
#include "media/base/eme_constants.h"
#include "media/base/media_export.h"

namespace media {

// Provides an interface for querying the properties of a registered key system.
class MEDIA_EXPORT KeySystemInfo {
 public:
  virtual ~KeySystemInfo() {}

  // Gets the base key system name, e.g. "org.chromium.foo".
  virtual std::string GetBaseKeySystemName() const = 0;

  // Returns whether the `key_system` is supported. Only the base key system and
  // some of its sub key systems should be supported, e.g. for base key system
  // name "org.chromium.foo", "org.chromium.foo" and "org.chromium.foo.bar"
  // could be supported, but "org.chromium.baz" should NOT be supported.
  virtual bool IsSupportedKeySystem(const std::string& key_system) const;

  // Whether the base key system should be used for all supported key systems
  // when creating CDMs.
  virtual bool ShouldUseBaseKeySystemName() const;

  // Returns whether |init_data_type| is supported by this key system.
  virtual bool IsSupportedInitDataType(
      EmeInitDataType init_data_type) const = 0;

  // Returns the configuration rule for supporting |encryption_scheme|.
  virtual EmeConfig::Rule GetEncryptionSchemeConfigRule(
      EncryptionScheme encryption_scheme) const = 0;

  // Returns the codecs supported by this key system.
  virtual SupportedCodecs GetSupportedCodecs() const = 0;

  // Returns the codecs can be used with the hardware-secure mode of this key
  // system. The codecs may be supported with any hardware-based robustness
  // level. Other parts of the code handle reporting appropriate levels of
  // robustness support for audio and video.
  virtual SupportedCodecs GetSupportedHwSecureCodecs() const;

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

  // Returns the support this key system provides for persistent-license
  // sessions. The returned `EmeConfig` (if supported) assumes persistence
  // requirement, which is enforced by `KeySystemConfigSelector`. Therefore, the
  // returned `EmeConfig` doesn't need to specify persistence requirement
  // explicitly.
  // TODO(crbug.com/40839176): Refactor `EmeConfig` to make it easier to
  // express combinations of requirements.
  virtual EmeConfig::Rule GetPersistentLicenseSessionSupport() const = 0;

  // Returns the support this key system provides for persistent state.
  virtual EmeFeatureSupport GetPersistentStateSupport() const = 0;

  // Returns the support this key system provides for distinctive identifiers.
  virtual EmeFeatureSupport GetDistinctiveIdentifierSupport() const = 0;

  // Returns whether AesDecryptor can be used for this key system.
  virtual bool UseAesDecryptor() const;
};

using KeySystemInfos = std::vector<std::unique_ptr<KeySystemInfo>>;

// TODO(b/321307544): Rename this callback to more appropriate name e.g.
// SupportedKeySystemsUpdateCB.
using GetSupportedKeySystemsCB = base::RepeatingCallback<void(KeySystemInfos)>;

}  // namespace media

#endif  // MEDIA_BASE_KEY_SYSTEM_INFO_H_
