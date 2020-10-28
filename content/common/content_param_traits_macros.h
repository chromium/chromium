// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Singly or Multiply-included shared traits file depending on circumstances.
// This allows the use of IPC serialization macros in more than one IPC message
// file.
#ifndef CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_
#define CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_

#include "build/build_config.h"
#include "components/viz/common/quads/selection.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/common/navigation_gesture.h"
#include "content/public/common/menu_item.h"
#include "content/public/common/page_visibility_state.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/three_d_api_types.h"
#include "ipc/ipc_message_macros.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/native_theme/native_theme.h"

#if defined(OS_MAC)
#include "third_party/blink/public/platform/mac/web_scrollbar_theme.h"
#endif

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::RequestContextType,
                          blink::mojom::RequestContextType::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::ResourceType,
                          blink::mojom::ResourceType::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(
    network::mojom::ContentSecurityPolicySource,
    network::mojom::ContentSecurityPolicySource::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::ContentSecurityPolicyType,
                          network::mojom::ContentSecurityPolicyType::kMaxValue)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(ui::mojom::CursorType,
                              ui::mojom::CursorType::kNull,
                              ui::mojom::CursorType::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(content::PageVisibilityState,
                          content::PageVisibilityState::kMaxValue)

IPC_STRUCT_TRAITS_BEGIN(viz::Selection<gfx::SelectionBound>)
  IPC_STRUCT_TRAITS_MEMBER(start)
  IPC_STRUCT_TRAITS_MEMBER(end)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(content::MenuItem::Type, content::MenuItem::TYPE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(content::NavigationGesture,
                          content::NavigationGestureLast)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(content::PageZoom,
                              content::PageZoom::PAGE_ZOOM_OUT,
                              content::PageZoom::PAGE_ZOOM_IN)
IPC_ENUM_TRAITS_MAX_VALUE(content::ThreeDAPIType,
                          content::THREE_D_API_TYPE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(ui::TextInputType, ui::TEXT_INPUT_TYPE_MAX)

#if defined(OS_MAC)
IPC_ENUM_TRAITS_MAX_VALUE(blink::ScrollerStyle, blink::kScrollerStyleOverlay)
#endif

IPC_ENUM_TRAITS_MAX_VALUE(ui::NativeTheme::SystemThemeColor,
                          ui::NativeTheme::SystemThemeColor::kMaxValue)

IPC_STRUCT_TRAITS_BEGIN(content::MenuItem)
  IPC_STRUCT_TRAITS_MEMBER(label)
  IPC_STRUCT_TRAITS_MEMBER(tool_tip)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(action)
  IPC_STRUCT_TRAITS_MEMBER(rtl)
  IPC_STRUCT_TRAITS_MEMBER(has_directional_override)
  IPC_STRUCT_TRAITS_MEMBER(enabled)
  IPC_STRUCT_TRAITS_MEMBER(checked)
  IPC_STRUCT_TRAITS_MEMBER(submenu)
IPC_STRUCT_TRAITS_END()

#endif  // CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_
