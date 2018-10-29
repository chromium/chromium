// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Singly or Multiply-included shared traits file depending on circumstances.
// This allows the use of IPC serialization macros in more than one IPC message
// file.
#ifndef CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_
#define CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_

#include "components/viz/common/quads/selection.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/common/download/mhtml_save_status.h"
#include "content/common/render_widget_surface_properties.h"
#include "content/public/common/input_event_ack_state.h"
#include "content/public/common/resource_type.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom.h"
#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/platform/web_content_security_policy.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/web/web_ime_text_span.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"
#include "ui/gfx/ipc/gfx_param_traits.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

IPC_ENUM_TRAITS_MAX_VALUE(content::InputEventAckState,
                          content::INPUT_EVENT_ACK_STATE_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::RequestContextType,
                          blink::mojom::RequestContextType::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(content::ResourceType,
                          content::RESOURCE_TYPE_LAST_TYPE - 1)
IPC_ENUM_TRAITS_MAX_VALUE(content::MhtmlSaveStatus,
                          content::MhtmlSaveStatus::LAST)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebContentSecurityPolicySource,
                          blink::kWebContentSecurityPolicySourceLast)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebContentSecurityPolicyType,
                          blink::kWebContentSecurityPolicyTypeLast)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(blink::WebInputEvent::Type,
                              blink::WebInputEvent::kTypeFirst,
                              blink::WebInputEvent::kTypeLast)
IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::PageVisibilityState,
                          blink::mojom::PageVisibilityState::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebImeTextSpan::Type,
                          blink::WebImeTextSpan::Type::kMisspellingSuggestion)
IPC_ENUM_TRAITS_MAX_VALUE(ws::mojom::ImeTextSpanThickness,
                          ws::mojom::ImeTextSpanThickness::kThick)

IPC_STRUCT_TRAITS_BEGIN(viz::Selection<gfx::SelectionBound>)
  IPC_STRUCT_TRAITS_MEMBER(start)
  IPC_STRUCT_TRAITS_MEMBER(end)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::WebImeTextSpan)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(start_offset)
  IPC_STRUCT_TRAITS_MEMBER(end_offset)
  IPC_STRUCT_TRAITS_MEMBER(underline_color)
  IPC_STRUCT_TRAITS_MEMBER(thickness)
  IPC_STRUCT_TRAITS_MEMBER(background_color)
  IPC_STRUCT_TRAITS_MEMBER(suggestion_highlight_color)
  IPC_STRUCT_TRAITS_MEMBER(suggestions)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::RenderWidgetSurfaceProperties)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(device_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(top_controls_height)
  IPC_STRUCT_TRAITS_MEMBER(top_controls_shown_ratio)
#ifdef OS_ANDROID
  IPC_STRUCT_TRAITS_MEMBER(bottom_controls_height)
  IPC_STRUCT_TRAITS_MEMBER(bottom_controls_shown_ratio)
  IPC_STRUCT_TRAITS_MEMBER(selection)
  IPC_STRUCT_TRAITS_MEMBER(has_transparent_background)
#endif
IPC_STRUCT_TRAITS_END()

#endif  // CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_
