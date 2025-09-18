// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PLANE_ID_H_
#define DEVICE_VR_PLANE_ID_H_

#include <optional>

#include "base/types/id_type.h"

namespace device {

using PlaneId = base::IdTypeU64<class PlaneTag>;
constexpr PlaneId kInvalidPlaneId;

inline std::optional<PlaneId> MaybeCreatePlaneId(
    std::optional<uint64_t> maybe_id) {
  if (!maybe_id || *maybe_id == kInvalidPlaneId.GetUnsafeValue()) {
    return std::nullopt;
  }

  return std::optional<PlaneId>(*maybe_id);
}

}  // namespace device

#endif  // DEVICE_VR_PLANE_ID_H_
