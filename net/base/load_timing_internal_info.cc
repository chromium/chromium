// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/load_timing_internal_info.h"

namespace net {

LoadTimingInternalInfo::LoadTimingInternalInfo() = default;
LoadTimingInternalInfo::LoadTimingInternalInfo(
    const LoadTimingInternalInfo& other) = default;
LoadTimingInternalInfo::~LoadTimingInternalInfo() = default;
bool LoadTimingInternalInfo::operator==(
    const LoadTimingInternalInfo& other) const = default;

}  // namespace net
