// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FRAME_MESSAGES_H_
#define CONTENT_COMMON_FRAME_MESSAGES_H_

// IPC messages for interacting with frames.

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/optional.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "cc/input/touch_action.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "content/common/buildflags.h"
#include "content/common/common_param_traits_macros.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/common/navigation_gesture.h"
#include "content/common/navigation_params.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/navigation_policy.h"
#include "content/public/common/referrer.h"
#include "content/public/common/stop_find_action.h"
#include "content/public/common/three_d_api_types.h"
#include "content/public/common/untrustworthy_context_menu_params.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/common/loader/previews_state.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/common/page_state/page_state.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/mojom/feature_policy/document_policy_feature.mojom.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-shared.h"
#include "third_party/blink/public/mojom/feature_policy/policy_disposition.mojom.h"
#include "third_party/blink/public/mojom/frame/blocked_navigation_types.mojom.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/blink/public/mojom/scroll/scrollbar_mode.mojom.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"
#include "third_party/blink/public/web/web_frame_owner_properties.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/common/pepper_renderer_instance_data.h"
#endif

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START FrameMsgStart
IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::ContextMenuDataMediaType,
                          blink::mojom::ContextMenuDataMediaType::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(blink::ContextMenuDataInputFieldType,
                          blink::ContextMenuDataInputFieldType::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::ScrollbarMode,
                          blink::mojom::ScrollbarMode::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(content::StopFindAction,
                          content::STOP_FIND_ACTION_LAST)
IPC_ENUM_TRAITS(network::mojom::WebSandboxFlags)  // Bitmask.
IPC_ENUM_TRAITS_MAX_VALUE(ui::MenuSourceType, ui::MENU_SOURCE_TYPE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::FeaturePolicyFeature,
                          blink::mojom::FeaturePolicyFeature::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::DocumentPolicyFeature,
                          blink::mojom::DocumentPolicyFeature::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::UserActivationUpdateType,
                          blink::mojom::UserActivationUpdateType::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::PolicyDisposition,
                          blink::mojom::PolicyDisposition::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::FrameVisibility,
                          blink::mojom::FrameVisibility::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::WebFeature,
                          blink::mojom::WebFeature::kMaxValue)

IPC_STRUCT_TRAITS_BEGIN(content::NavigationDownloadPolicy)
  IPC_STRUCT_TRAITS_MEMBER(observed_types)
  IPC_STRUCT_TRAITS_MEMBER(disallowed_types)
  IPC_STRUCT_TRAITS_MEMBER(blocking_downloads_in_sandbox_enabled)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::Impression)
  IPC_STRUCT_TRAITS_MEMBER(conversion_destination)
  IPC_STRUCT_TRAITS_MEMBER(reporting_origin)
  IPC_STRUCT_TRAITS_MEMBER(impression_data)
  IPC_STRUCT_TRAITS_MEMBER(expiry)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::UntrustworthyContextMenuParams)
  IPC_STRUCT_TRAITS_MEMBER(media_type)
  IPC_STRUCT_TRAITS_MEMBER(x)
  IPC_STRUCT_TRAITS_MEMBER(y)
  IPC_STRUCT_TRAITS_MEMBER(link_url)
  IPC_STRUCT_TRAITS_MEMBER(link_text)
  IPC_STRUCT_TRAITS_MEMBER(impression)
  IPC_STRUCT_TRAITS_MEMBER(unfiltered_link_url)
  IPC_STRUCT_TRAITS_MEMBER(src_url)
  IPC_STRUCT_TRAITS_MEMBER(has_image_contents)
  IPC_STRUCT_TRAITS_MEMBER(media_flags)
  IPC_STRUCT_TRAITS_MEMBER(selection_text)
  IPC_STRUCT_TRAITS_MEMBER(title_text)
  IPC_STRUCT_TRAITS_MEMBER(alt_text)
  IPC_STRUCT_TRAITS_MEMBER(suggested_filename)
  IPC_STRUCT_TRAITS_MEMBER(misspelled_word)
  IPC_STRUCT_TRAITS_MEMBER(dictionary_suggestions)
  IPC_STRUCT_TRAITS_MEMBER(spellcheck_enabled)
  IPC_STRUCT_TRAITS_MEMBER(is_editable)
  IPC_STRUCT_TRAITS_MEMBER(writing_direction_default)
  IPC_STRUCT_TRAITS_MEMBER(writing_direction_left_to_right)
  IPC_STRUCT_TRAITS_MEMBER(writing_direction_right_to_left)
  IPC_STRUCT_TRAITS_MEMBER(edit_flags)
  IPC_STRUCT_TRAITS_MEMBER(frame_charset)
  IPC_STRUCT_TRAITS_MEMBER(referrer_policy)
  IPC_STRUCT_TRAITS_MEMBER(custom_context)
  IPC_STRUCT_TRAITS_MEMBER(custom_items)
  IPC_STRUCT_TRAITS_MEMBER(source_type)
  IPC_STRUCT_TRAITS_MEMBER(input_field_type)
  IPC_STRUCT_TRAITS_MEMBER(selection_rect)
  IPC_STRUCT_TRAITS_MEMBER(selection_start_offset)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::CustomContextMenuContext)
  IPC_STRUCT_TRAITS_MEMBER(is_pepper_menu)
  IPC_STRUCT_TRAITS_MEMBER(request_id)
  IPC_STRUCT_TRAITS_MEMBER(render_widget_id)
  IPC_STRUCT_TRAITS_MEMBER(link_followed)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::FramePolicy)
  IPC_STRUCT_TRAITS_MEMBER(sandbox_flags)
  IPC_STRUCT_TRAITS_MEMBER(container_policy)
  IPC_STRUCT_TRAITS_MEMBER(required_document_policy)
  IPC_STRUCT_TRAITS_MEMBER(disallow_document_access)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::ScreenInfo)
  IPC_STRUCT_TRAITS_MEMBER(device_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(display_color_spaces)
  IPC_STRUCT_TRAITS_MEMBER(depth)
  IPC_STRUCT_TRAITS_MEMBER(depth_per_component)
  IPC_STRUCT_TRAITS_MEMBER(is_monochrome)
  IPC_STRUCT_TRAITS_MEMBER(display_frequency)
  IPC_STRUCT_TRAITS_MEMBER(rect)
  IPC_STRUCT_TRAITS_MEMBER(available_rect)
  IPC_STRUCT_TRAITS_MEMBER(orientation_type)
  IPC_STRUCT_TRAITS_MEMBER(orientation_angle)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::ParsedFeaturePolicyDeclaration)
  IPC_STRUCT_TRAITS_MEMBER(feature)
  IPC_STRUCT_TRAITS_MEMBER(allowed_origins)
  IPC_STRUCT_TRAITS_MEMBER(matches_all_origins)
  IPC_STRUCT_TRAITS_MEMBER(matches_opaque_src)
IPC_STRUCT_TRAITS_END()

// -----------------------------------------------------------------------------
// Messages sent from the browser to the renderer.

// Sent in response to a FrameHostMsg_ContextMenu to let the renderer know that
// the menu has been closed.
IPC_MESSAGE_ROUTED1(FrameMsg_ContextMenuClosed,
                    content::CustomContextMenuContext /* custom_context */)

// Executes custom context menu action that was provided from Blink.
IPC_MESSAGE_ROUTED2(FrameMsg_CustomContextMenuAction,
                    content::CustomContextMenuContext /* custom_context */,
                    unsigned /* action */)

// -----------------------------------------------------------------------------
// Messages sent from the renderer to the browser.

// Used to tell the parent that the user right clicked on an area of the
// content area, and a context menu should be shown for it. The params
// object contains information about the node(s) that were selected when the
// user right clicked.
IPC_MESSAGE_ROUTED1(FrameHostMsg_ContextMenu,
                    content::UntrustworthyContextMenuParams)

// Adding a new message? Stick to the sort order above: first platform
// independent FrameMsg, then ifdefs for platform specific FrameMsg, then
// platform independent FrameHostMsg, then ifdefs for platform specific
// FrameHostMsg.

#endif  // CONTENT_COMMON_FRAME_MESSAGES_H_
