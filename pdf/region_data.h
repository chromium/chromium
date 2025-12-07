// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_REGION_DATA_H_
#define PDF_REGION_DATA_H_

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"
#include "base/memory/raw_span.h"

namespace chrome_pdf {

struct RegionData {
  RegionData(base::span<uint8_t> buffer, size_t stride);
  RegionData(RegionData&&) noexcept;
  RegionData& operator=(RegionData&&) noexcept;
  ~RegionData();

  base::raw_span<uint8_t> buffer;  // Never empty.
  size_t stride;
};

}  // namespace chrome_pdf

#endif  // PDF_REGION_DATA_H_
