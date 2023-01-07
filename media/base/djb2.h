// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DJB2_H_
#define MEDIA_BASE_DJB2_H_

#include <stddef.h>
#include <stdint.h>

#include "media/base/media_export.h"

// DJB2 is a hash algorithm with excellent distribution and speed
// on many different sets.
// It has marginally more collisions than FNV1, but makes up for it in
// performance.
// The return value is suitable for table lookups.
// For small fixed sizes (ie a pixel), it has low overhead and inlines well.
// For large data sets, it optimizes into assembly/simd and is appropriate
// for realtime applications.
// See Also:
//   http://www.cse.yorku.ca/~oz/hash.html

static const uint32_t kDJB2HashSeed = 5381u;

// These functions perform DJB2 hash. The simplest call is DJB2Hash() to
// generate the DJB2 hash of the given data:
//   uint32_t hash = DJB2Hash(data1, length1, kDJB2HashSeed);
//
// You can also compute the DJB2 hash of data incrementally by making multiple
// calls to DJB2Hash():
//   uint32_t hash_value = kDJB2HashSeed;  // Initial seed for DJB2.
//   for (size_t i = 0; i < copy_lines; ++i) {
//     hash_value = DJB2Hash(source, bytes_per_line, hash_value);
//     source += source_stride;
//   }

// For the given buffer of data, compute the DJB2 hash of
// the data. You can call this any number of times during the computation.
MEDIA_EXPORT uint32_t DJB2Hash(const void* buf, size_t len, uint32_t seed);

#endif  // MEDIA_BASE_DJB2_H_

