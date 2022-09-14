// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/key_system_info.h"

namespace media {

bool KeySystemInfo::IsSupportedKeySystem(const std::string& key_system) const {
  // By default, only support the base key system.
  return key_system == GetBaseKeySystemName();
}

bool KeySystemInfo::ShouldUseBaseKeySystemName() const {
  // By default, use the sub key system names for creating CDMs.
  return false;
}

SupportedCodecs KeySystemInfo::GetSupportedHwSecureCodecs() const {
  return EME_CODEC_NONE;
}

bool KeySystemInfo::UseAesDecryptor() const {
  return false;
}

}  // namespace media
