// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ENCRYPTEDMEDIA_ENCRYPTED_MEDIA_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ENCRYPTEDMEDIA_ENCRYPTED_MEDIA_UTILS_H_

#include "third_party/blink/public/platform/web_encrypted_media_key_information.h"
#include "third_party/blink/public/platform/web_encrypted_media_types.h"
#include "third_party/blink/public/platform/web_media_key_system_configuration.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace media {
enum class EmeInitDataType;
}

namespace blink {

constexpr const char* kEncryptedMediaFeaturePolicyConsoleWarning =
    "Encrypted Media access has been blocked because of a Feature Policy "
    "applied to the current document. See https://goo.gl/EuHzyv for more "
    "details.";

class EncryptedMediaUtils {
  STATIC_ONLY(EncryptedMediaUtils);

 public:
  static media::EmeInitDataType ConvertToInitDataType(
      const String& init_data_type);
  static String ConvertFromInitDataType(media::EmeInitDataType init_data_type);

  static WebEncryptedMediaSessionType ConvertToSessionType(
      const String& session_type);
  static String ConvertFromSessionType(WebEncryptedMediaSessionType);

  static String ConvertKeyStatusToString(
      const WebEncryptedMediaKeyInformation::KeyStatus);

  static WebMediaKeySystemConfiguration::Requirement
  ConvertToMediaKeysRequirement(const String&);
  static String ConvertMediaKeysRequirementToString(
      WebMediaKeySystemConfiguration::Requirement);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ENCRYPTEDMEDIA_ENCRYPTED_MEDIA_UTILS_H_
