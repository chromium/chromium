// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ENCRYPTION_PATTERN_H_
#define MEDIA_BASE_ENCRYPTION_PATTERN_H_

#include <stdint.h>
#include <ostream>

#include "media/base/media_export.h"

namespace media {

// CENC 3rd Edition adds pattern encryption, through two new protection
// schemes: 'cens' (with AES-CTR) and 'cbcs' (with AES-CBC).
// The pattern applies independently to each 'encrypted' part of the frame (as
// defined by the relevant subsample entries), and reduces further the
// actual encryption applied through a repeating pattern of (encrypt:skip)
// 16 byte blocks. For example, in a (1:9) pattern, the first block is
// encrypted, and the next nine are skipped. This pattern is applied
// repeatedly until the end of the last 16-byte block in the subsample.
// Any remaining bytes are left clear.
// TODO(jrummell): Use std::optional<EncryptionPattern> everywhere.
class MEDIA_EXPORT EncryptionPattern {
 public:
  EncryptionPattern();
  EncryptionPattern(uint32_t crypt_byte_block, uint32_t skip_byte_block);
  EncryptionPattern(const EncryptionPattern& rhs);
  EncryptionPattern& operator=(const EncryptionPattern& rhs);
  ~EncryptionPattern();

  uint32_t crypt_byte_block() const { return crypt_byte_block_; }
  uint32_t skip_byte_block() const { return skip_byte_block_; }

  bool operator==(const EncryptionPattern& other) const;
  bool operator!=(const EncryptionPattern& other) const;

 private:
  // ISO/IEC 23001-7(2016), section 10.3, discussing 'cens' pattern encryption
  // scheme, states "Tracks other than video are protected using whole-block
  // full-sample encryption as specified in 9.7 and hence skip_byte_block
  // SHALL be 0." So patterns where |skip_byte_block| = 0 should be treated
  // as whole-block full-sample encryption.
  uint32_t crypt_byte_block_ = 0;  // Count of the encrypted blocks.
  uint32_t skip_byte_block_ = 0;   // Count of the unencrypted blocks.
};

// For logging use only.
MEDIA_EXPORT std::ostream& operator<<(
    std::ostream& os,
    const EncryptionPattern& encryption_pattern);

}  // namespace media

#endif  // MEDIA_BASE_ENCRYPTION_PATTERN_H_
