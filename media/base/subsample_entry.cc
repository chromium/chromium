// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/subsample_entry.h"

#include "base/logging.h"
#include "base/numerics/safe_math.h"

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

}  // namespace media
