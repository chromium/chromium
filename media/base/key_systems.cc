// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/key_systems.h"

#include <optional>
#include <string>

#include "media/cdm/clear_key_cdm_common.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

namespace media {

namespace {
// These names are used by UMA. Do not change them!
const char kClearKeyKeySystemNameForUMA[] = "ClearKey";
const char kUnknownKeySystemNameForUMA[] = "Unknown";
const char kHardwareSecureForUMA[] = "HardwareSecure";
const char kSoftwareSecureForUMA[] = "SoftwareSecure";

enum KeySystemForUkm {
  // These values reported to UKM. Do not change their ordinal values.
  kUnknownKeySystemForUkm = 0,
  kClearKeyKeySystemForUkm,
  kWidevineKeySystemForUkm,
};
}  // namespace

// Returns a name for `key_system` for UMA logging. When `use_hw_secure_codecs`
// is specified (non-nullopt), names with robustness will be returned for
// supported key systems.
std::string GetKeySystemNameForUMA(const std::string& key_system,
                                   std::optional<bool> use_hw_secure_codecs) {
  // Here we maintain a short list of known key systems to facilitate UMA
  // reporting. Mentioned key systems are not necessarily supported by
  // the current platform.

  if (key_system == kWidevineKeySystem) {
    std::string key_system_name = kWidevineKeySystemNameForUMA;
    if (use_hw_secure_codecs.has_value()) {
      key_system_name += ".";
      key_system_name += (use_hw_secure_codecs.value() ? kHardwareSecureForUMA
                                                       : kSoftwareSecureForUMA);
    }
    return key_system_name;
  }

  // For Clear Key and unknown key systems we don't to differentiate between
  // software and hardware security.

  if (key_system == kClearKeyKeySystem) {
    return kClearKeyKeySystemNameForUMA;
  }

  return kUnknownKeySystemNameForUMA;
}

// Returns an int mapping to `key_system` suitable for UKM reporting. CdmConfig
// is not needed here because we can report CdmConfig fields in UKM directly.
MEDIA_EXPORT int GetKeySystemIntForUKM(const std::string& key_system) {
  if (key_system == kWidevineKeySystem) {
    return KeySystemForUkm::kWidevineKeySystemForUkm;
  }

  if (key_system == kClearKeyKeySystem) {
    return KeySystemForUkm::kClearKeyKeySystemForUkm;
  }

  return KeySystemForUkm::kUnknownKeySystemForUkm;
}

}  // namespace media
