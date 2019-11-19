// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Singly or Multiply-included shared traits file depending on circumstances.
// This allows the use of IPC serialization macros in more than one IPC message
// file.
#ifndef CONTENT_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
#define CONTENT_COMMON_COMMON_PARAM_TRAITS_MACROS_H_

#include "content/common/frame_messages.h"
#include "content/common/visual_properties.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/blink/public/web/web_device_emulation_params.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

// Traits for VisualProperties.
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebDeviceEmulationParams::ScreenPosition,
                          blink::WebDeviceEmulationParams::kScreenPositionLast)

IPC_ENUM_TRAITS_MAX_VALUE(content::ScreenOrientationValues,
                          content::SCREEN_ORIENTATION_VALUES_LAST)

IPC_ENUM_TRAITS_MIN_MAX_VALUE(blink::WebScreenOrientationType,
                              blink::kWebScreenOrientationUndefined,
                              blink::WebScreenOrientationTypeLast)

IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::DisplayMode,
                          blink::mojom::DisplayMode::kMaxValue)

IPC_STRUCT_TRAITS_BEGIN(content::VisualProperties)
  IPC_STRUCT_TRAITS_MEMBER(screen_info)
  IPC_STRUCT_TRAITS_MEMBER(auto_resize_enabled)
  IPC_STRUCT_TRAITS_MEMBER(min_size_for_auto_resize)
  IPC_STRUCT_TRAITS_MEMBER(max_size_for_auto_resize)
  IPC_STRUCT_TRAITS_MEMBER(new_size)
  IPC_STRUCT_TRAITS_MEMBER(visible_viewport_size)
  IPC_STRUCT_TRAITS_MEMBER(compositor_viewport_pixel_rect)
  IPC_STRUCT_TRAITS_MEMBER(browser_controls_shrink_blink_size)
  IPC_STRUCT_TRAITS_MEMBER(scroll_focused_node_into_view)
  IPC_STRUCT_TRAITS_MEMBER(top_controls_height)
  IPC_STRUCT_TRAITS_MEMBER(bottom_controls_height)
  IPC_STRUCT_TRAITS_MEMBER(local_surface_id_allocation)
  IPC_STRUCT_TRAITS_MEMBER(is_fullscreen_granted)
  IPC_STRUCT_TRAITS_MEMBER(display_mode)
  IPC_STRUCT_TRAITS_MEMBER(capture_sequence_number)
  IPC_STRUCT_TRAITS_MEMBER(zoom_level)
  IPC_STRUCT_TRAITS_MEMBER(page_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(is_pinch_gesture_active)
IPC_STRUCT_TRAITS_END()

#endif  // CONTENT_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
