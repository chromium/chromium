// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/region_data.h"

namespace chrome_pdf {

RegionData::RegionData(base::span<uint8_t> buffer, size_t stride)
    : buffer(buffer), stride(stride) {}

RegionData::RegionData(RegionData&&) noexcept = default;

RegionData& RegionData::operator=(RegionData&&) noexcept = default;

RegionData::~RegionData() = default;

}  // namespace chrome_pdf
