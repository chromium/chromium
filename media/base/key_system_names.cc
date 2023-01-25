// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/key_system_names.h"
#include "media/cdm/clear_key_cdm_common.h"

namespace media {

bool IsClearKey(const std::string& key_system) {
  return key_system == kClearKeyKeySystem;
}

bool IsSubKeySystemOf(const std::string& key_system, const std::string& base) {
  std::string prefix = base + '.';
  return key_system.substr(0, prefix.size()) == prefix;
}

bool IsExternalClearKey(const std::string& key_system) {
  return key_system == kExternalClearKeyKeySystem ||
         IsSubKeySystemOf(key_system, kExternalClearKeyKeySystem);
}

}  // namespace media
