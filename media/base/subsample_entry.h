// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SUBSAMPLE_ENTRY_H_
#define MEDIA_BASE_SUBSAMPLE_ENTRY_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "media/base/media_export.h"

namespace media {

// The Common Encryption spec provides for subsample encryption, where portions
// of a sample are set in cleartext. A SubsampleEntry specifies the number of
// clear and encrypted bytes in each subsample. For decryption, all of the
// encrypted bytes in a sample should be considered a single logical stream,
// regardless of how they are divided into subsamples, and the clear bytes
// should not be considered as part of decryption. This is logically equivalent
// to concatenating all 'cypher_bytes' portions of subsamples, decrypting that
// result, and then copying each byte from the decrypted block over the
// position of the corresponding encrypted byte.
struct SubsampleEntry {
  SubsampleEntry() : clear_bytes(0), cypher_bytes(0) {}
  SubsampleEntry(uint32_t clear_bytes, uint32_t cypher_bytes)
      : clear_bytes(clear_bytes), cypher_bytes(cypher_bytes) {}
  uint32_t clear_bytes;
  uint32_t cypher_bytes;
};

// Verifies that |subsamples| correctly specifies a buffer of length
// |input_size|. Returns false if the total of bytes specified in |subsamples|
// does not match |input_size|.
MEDIA_EXPORT bool VerifySubsamplesMatchSize(
    const std::vector<SubsampleEntry>& subsamples,
    size_t input_size);

}  // namespace media

#endif  // MEDIA_BASE_SUBSAMPLE_ENTRY_H_
