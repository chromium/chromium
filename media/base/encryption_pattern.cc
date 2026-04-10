// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/encryption_pattern.h"

#include "base/feature_list.h"
#include "media/base/media_switches.h"

namespace media {

EncryptionPattern::EncryptionPattern() = default;

EncryptionPattern::EncryptionPattern(const EncryptionPattern& rhs) = default;

EncryptionPattern& EncryptionPattern::operator=(const EncryptionPattern& rhs) =
    default;

EncryptionPattern::~EncryptionPattern() = default;

// static
std::optional<EncryptionPattern> EncryptionPattern::Create(
    uint32_t crypt_byte_block,
    uint32_t skip_byte_block) {
  if (base::FeatureList::IsEnabled(kValidateEncryptionPatternSize) &&
      (crypt_byte_block > kMaxPatternSize ||
       skip_byte_block > kMaxPatternSize)) {
    return std::nullopt;
  }
  return EncryptionPattern(crypt_byte_block, skip_byte_block);
}

bool EncryptionPattern::operator==(const EncryptionPattern& other) const {
  return crypt_byte_block_ == other.crypt_byte_block_ &&
         skip_byte_block_ == other.skip_byte_block_;
}

bool EncryptionPattern::operator!=(const EncryptionPattern& other) const {
  return !operator==(other);
}

std::ostream& operator<<(std::ostream& os,
                         const EncryptionPattern& encryption_pattern) {
  return os << "{" << encryption_pattern.crypt_byte_block() << ", "
            << encryption_pattern.skip_byte_block() << "}";
}

EncryptionPattern::EncryptionPattern(uint32_t crypt_byte_block,
                                     uint32_t skip_byte_block)
    : crypt_byte_block_(crypt_byte_block), skip_byte_block_(skip_byte_block) {}

}  // namespace media
