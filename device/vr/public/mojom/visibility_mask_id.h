// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PUBLIC_MOJOM_VISIBILITY_MASK_ID_H_
#define DEVICE_VR_PUBLIC_MOJOM_VISIBILITY_MASK_ID_H_

#include "base/types/id_type.h"

namespace device {

using XrVisibilityMaskId = base::IdTypeU64<class XrVisibilityMaskIdTag>;
constexpr XrVisibilityMaskId kInvalidVisibilityMaskId;

}  // namespace device

#endif  // DEVICE_VR_PUBLIC_MOJOM_VISIBILITY_MASK_ID_H_
