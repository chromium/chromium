// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CBCS_DECRYPTOR_H_
#define MEDIA_CDM_CBCS_DECRYPTOR_H_

#include "base/memory/scoped_refptr.h"
#include "media/base/media_export.h"

namespace crypto {
class SymmetricKey;
}

namespace media {
class DecoderBuffer;

// This class implements pattern decryption as specified by
// ISO/IEC 23001-7:2016, section 10.4 (https://www.iso.org),
// using AES-CBC-128 decryption.
//
// Subsample encryption divides each input buffer into one or more contiguous
// subsamples. Each subsample has an unprotected part (unencrypted) followed
// by a protected part (encrypted), only one of which may be zero bytes in
// length. For example:
//   |                DecoderBuffer.data()              |
//   |  Subsample#1   |   Subsample#2   |  Subsample#3  |
//   |uuuuu|eeeeeeeeee|uuuu|eeeeeeeeeeee|uu|eeeeeeeeeeee|
// Within the protected part of each subsample, the data is treated as a
// chain of 16 byte cipher blocks, starting with the initialization vector
// associated with the sample. The IV is applied to the first encrypted
// cipher block of each subsample.
//
// A partial block at the end of a subsample (if any) is unencrypted.
//
// This supports pattern decryption, where a pattern of encrypted and clear
// (skipped) blocks is used. The Pattern is specified with each DecoderBuffer
// (in the DecryptConfig). Typically encrypted video tracks use a pattern of
// (1,9) which indicates that one 16 byte block is encrypted followed by 9
// blocks unencrypted, and then the pattern repeats through all the blocks in
// the protected part. Tracks other than video usually use full-sample
// encryption.
//
// If a pattern is not specified, the protected part will use full-sample
// encryption.

// Decrypts the encrypted buffer |input| using |key| and values found in
// |input|->DecryptConfig. The key size must be 128 bits.
MEDIA_EXPORT scoped_refptr<DecoderBuffer> DecryptCbcsBuffer(
    const DecoderBuffer& input,
    const crypto::SymmetricKey& key);

}  // namespace media

#endif  // MEDIA_CDM_CBCS_DECRYPTOR_H_
