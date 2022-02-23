// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/key_system_properties.h"

namespace media {

bool KeySystemProperties::IsSupportedKeySystem(
    const std::string& key_system) const {
  // By default, only support the base key system.
  return key_system == GetBaseKeySystemName();
}

bool KeySystemProperties::ShouldUseBaseKeySystemName() const {
  // By default, use the sub key system names for creating CDMs.
  return false;
}

SupportedCodecs KeySystemProperties::GetSupportedHwSecureCodecs() const {
  return EME_CODEC_NONE;
}

bool KeySystemProperties::UseAesDecryptor() const {
  return false;
}

}  // namespace media
