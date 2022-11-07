// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CENC_DECRYPTOR_H_
#define MEDIA_CDM_CENC_DECRYPTOR_H_

#include "base/memory/scoped_refptr.h"
#include "media/base/media_export.h"

namespace crypto {
class SymmetricKey;
}

namespace media {
class DecoderBuffer;

// This class implements 'cenc' AES-CTR scheme as specified by
// ISO/IEC 23001-7:2016, section 10.1 (https://www.iso.org). Decryption
// uses the Advanced Encryption Standard specified by AES [FIPS-197,
// https://www.nist.gov] using 128-bit keys in Counter Mode (AES-CTR-128),
// as specified in Block Cipher Modes [NIST 800-38A, https://www.nist.gov].
//
// Each input buffer is divided into one or more contiguous subsamples. Each
// subsample has an unprotected part (unencrypted) followed by a protected part
// (encrypted), only one of which may be zero bytes in length. For example:
//   |                DecoderBuffer.data()              |
//   |  Subsample#1   |   Subsample#2   |  Subsample#3  |
//   |uuuuu|eeeeeeeeee|uuuu|eeeeeeeeeeee|uu|eeeeeeeeeeee|
// Subsample encryption encrypts all the bytes in the protected part of the
// sample. The protected byte sequences of all subsamples is treated as a
// logically continuous chain of 16 byte cipher blocks, even when they are
// separated by unprotected data.
//
// If no subsamples are specified, the whole input buffer will be treated as
// protected data.

// Decrypts the encrypted buffer |input| using |key| and values found in
// |input|->DecryptConfig. The key size must be 128 bits.
MEDIA_EXPORT scoped_refptr<DecoderBuffer> DecryptCencBuffer(
    const DecoderBuffer& input,
    const crypto::SymmetricKey& key);

}  // namespace media

#endif  // MEDIA_CDM_CENC_DECRYPTOR_H_
