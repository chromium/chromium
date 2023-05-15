// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/subsample_entry.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "media/base/ranges.h"

namespace media {

bool VerifySubsamplesMatchSize(const std::vector<SubsampleEntry>& subsamples,
                               size_t input_size) {
  base::CheckedNumeric<size_t> total_size = 0;
  for (const auto& subsample : subsamples) {
    // Add each entry separately to avoid the compiler doing the wrong thing.
    total_size += subsample.clear_bytes;
    total_size += subsample.cypher_bytes;
  }

  if (!total_size.IsValid() || total_size.ValueOrDie() != input_size) {
    DVLOG(1) << "Subsample sizes do not equal input size";
    return false;
  }

  return true;
}

std::vector<SubsampleEntry> EncryptedRangesToSubsampleEntry(
    const uint8_t* start,
    const uint8_t* end,
    const Ranges<const uint8_t*>& encrypted_ranges) {
  std::vector<SubsampleEntry> subsamples(encrypted_ranges.size());
  const uint8_t* cur = start;
  for (size_t i = 0; i < encrypted_ranges.size(); ++i) {
    const uint8_t* encrypted_start = encrypted_ranges.start(i);
    DCHECK_GE(encrypted_start, cur)
        << "Encrypted range started before the current buffer pointer.";
    subsamples[i].clear_bytes = encrypted_start - cur;
    const uint8_t* encrypted_end = encrypted_ranges.end(i);
    subsamples[i].cypher_bytes = encrypted_end - encrypted_start;
    cur = encrypted_end;
    DCHECK_LE(cur, end) << "Encrypted range is outside the buffer range.";
  }
  // If there is more data in the buffer but not covered by encrypted_ranges,
  // then it must be in the clear.
  if (cur < end) {
    subsamples.emplace_back(end - cur, 0);
  }
  return subsamples;
}

}  // namespace media
