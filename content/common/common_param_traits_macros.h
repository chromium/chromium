// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Singly or Multiply-included shared traits file depending on circumstances.
// This allows the use of IPC serialization macros in more than one IPC message
// file.
#ifndef CONTENT_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
#define CONTENT_COMMON_COMMON_PARAM_TRAITS_MACROS_H_

#include "cc/trees/browser_controls_params.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "services/device/public/mojom/screen_orientation_lock_types.mojom-shared.h"
#include "third_party/blink/public/common/widget/device_emulation_params.h"
#include "third_party/blink/public/common/widget/visual_properties.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

// Traits for VisualProperties.
IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::EmulatedScreenType,
                          blink::mojom::EmulatedScreenType::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(device::mojom::ScreenOrientationLockType,
                          device::mojom::ScreenOrientationLockType::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(display::mojom::ScreenOrientation,
                          display::mojom::ScreenOrientation::kMaxValue)

#endif  // CONTENT_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
