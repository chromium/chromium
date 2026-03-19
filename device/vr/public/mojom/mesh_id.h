// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PUBLIC_MOJOM_MESH_ID_H_
#define DEVICE_VR_PUBLIC_MOJOM_MESH_ID_H_

#include "base/types/id_type.h"

namespace device {

using MeshId = base::IdTypeU64<class MeshTag>;
constexpr MeshId kInvalidMeshId;

}  // namespace device

#endif  // DEVICE_VR_PUBLIC_MOJOM_MESH_ID_H_
