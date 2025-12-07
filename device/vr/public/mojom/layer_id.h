// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PUBLIC_MOJOM_LAYER_ID_H_
#define DEVICE_VR_PUBLIC_MOJOM_LAYER_ID_H_

#include "base/types/id_type.h"

namespace device {

using LayerId = base::IdTypeU64<class LayerTag>;
constexpr LayerId kInvalidLayerId;

}  // namespace device

#endif  // DEVICE_VR_PUBLIC_MOJOM_LAYER_ID_H_
