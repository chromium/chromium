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
#include "content/common/frame_delete_intention.h"
#include "content/common/frame_replication_state.h"
#include "content/common/navigation_gesture.h"
#include "content/common/navigation_params.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/impression.h"
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
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/common/loader/previews_state.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/page_state/page_state.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/mojom/feature_policy/document_policy_feature.mojom.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-shared.h"
#include "third_party/blink/public/mojom/feature_policy/policy_disposition.mojom.h"
#include "third_party/blink/public/mojom/frame/blocked_navigation_types.mojom.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_element_type.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom.h"
#include "third_party/blink/public/mojom/frame/tree_scope_type.mojom.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/blink/public/mojom/scroll/scrollbar_mode.mojom.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom.h"
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
IPC_ENUM_TRAITS_MAX_VALUE(content::FrameDeleteIntention,
                          content::FrameDeleteIntention::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::FrameOwnerElementType,
                          blink::mojom::FrameOwnerElementType::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::AdFrameType,
                          blink::mojom::AdFrameType::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::ContextMenuDataMediaType,
                          blink::mojom::ContextMenuDataMediaType::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(blink::ContextMenuDataInputFieldType,
                          blink::ContextMenuDataInputFieldType::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::ScrollbarMode,
                          blink::mojom::ScrollbarMode::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(content::StopFindAction,
                          content::STOP_FIND_ACTION_LAST)
IPC_ENUM_TRAITS(network::mojom::WebSandboxFlags)  // Bitmask.
IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::TreeScopeType,
                          blink::mojom::TreeScopeType::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(ui::MenuSourceType, ui::MENU_SOURCE_TYPE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::CSPDirectiveName,
                          network::mojom::CSPDirectiveName::kMaxValue)
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
IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::InsecureRequestPolicy,
                          blink::mojom::InsecureRequestPolicy::kMaxValue)

IPC_STRUCT_TRAITS_BEGIN(content::NavigationDownloadPolicy)
  IPC_STRUCT_TRAITS_MEMBER(observed_types)
  IPC_STRUCT_TRAITS_MEMBER(disallowed_types)
  IPC_STRUCT_TRAITS_MEMBER(blocking_downloads_in_sandbox_enabled)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::Impression)
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

IPC_STRUCT_TRAITS_BEGIN(content::FrameReplicationState)
  IPC_STRUCT_TRAITS_MEMBER(origin)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(unique_name)
  IPC_STRUCT_TRAITS_MEMBER(feature_policy_header)
  IPC_STRUCT_TRAITS_MEMBER(active_sandbox_flags)
  IPC_STRUCT_TRAITS_MEMBER(frame_policy)
  IPC_STRUCT_TRAITS_MEMBER(accumulated_csp_headers)
  IPC_STRUCT_TRAITS_MEMBER(scope)
  IPC_STRUCT_TRAITS_MEMBER(insecure_request_policy)
  IPC_STRUCT_TRAITS_MEMBER(insecure_navigations_set)
  IPC_STRUCT_TRAITS_MEMBER(has_potentially_trustworthy_unique_origin)
  IPC_STRUCT_TRAITS_MEMBER(has_active_user_gesture)
  IPC_STRUCT_TRAITS_MEMBER(has_received_user_gesture_before_nav)
  IPC_STRUCT_TRAITS_MEMBER(frame_owner_element_type)
  IPC_STRUCT_TRAITS_MEMBER(ad_frame_type)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(network::mojom::ContentSecurityPolicyHeader)
  IPC_STRUCT_TRAITS_MEMBER(header_value)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(source)
IPC_STRUCT_TRAITS_END()

#if BUILDFLAG(ENABLE_PLUGINS)
IPC_STRUCT_TRAITS_BEGIN(content::PepperRendererInstanceData)
  IPC_STRUCT_TRAITS_MEMBER(render_process_id)
  IPC_STRUCT_TRAITS_MEMBER(render_frame_id)
  IPC_STRUCT_TRAITS_MEMBER(document_url)
  IPC_STRUCT_TRAITS_MEMBER(plugin_url)
  IPC_STRUCT_TRAITS_MEMBER(is_potentially_secure_plugin_context)
IPC_STRUCT_TRAITS_END()
#endif

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

#if BUILDFLAG(ENABLE_PLUGINS)
// Sent to the browser when the renderer detects it is blocked on a pepper
// plugin message for too long. This is also sent when it becomes unhung
// (according to the value of is_hung). The browser can give the user the
// option of killing the plugin.
IPC_MESSAGE_ROUTED3(FrameHostMsg_PepperPluginHung,
                    int /* plugin_child_id */,
                    base::FilePath /* path */,
                    bool /* is_hung */)

// Return information about a plugin for the given URL and MIME
// type. If there is no matching plugin, |found| is false.
// |actual_mime_type| is the actual mime type supported by the
// found plugin.
IPC_SYNC_MESSAGE_CONTROL4_3(FrameHostMsg_GetPluginInfo,
                            int /* render_frame_id */,
                            GURL /* url */,
                            url::Origin /* main_frame_origin */,
                            std::string /* mime_type */,
                            bool /* found */,
                            content::WebPluginInfo /* plugin info */,
                            std::string /* actual_mime_type */)

// A renderer sends this to the browser process when it wants to create a ppapi
// plugin.  The browser will create the plugin process if necessary, and will
// return a handle to the channel on success.
//
// The plugin_child_id is the ChildProcessHost ID assigned in the browser
// process. This ID is valid only in the context of the browser process and is
// used to identify the proper process when the renderer notifies it that the
// plugin is hung.
//
// |embedder_origin| provides the origin of the frame that embeds the plugin
// (i.e. the origin of the document that contains the <embed> html tag).
// |embedder_origin| needs to be included in the message payload, because the
// message is received and handled on the IO thread in the browser process
// (where it is not possible to consult
// RenderFrameHostImpl::GetLastCommittedOrigin).
//
// On error an empty string and null handles are returned.
IPC_SYNC_MESSAGE_CONTROL3_3(FrameHostMsg_OpenChannelToPepperPlugin,
                            url::Origin /* embedder_origin */,
                            base::FilePath /* path */,
                            base::Optional<url::Origin>, /* origin_lock */
                            IPC::ChannelHandle /* handle to channel */,
                            base::ProcessId /* plugin_pid */,
                            int /* plugin_child_id */)

// Message from the renderer to the browser indicating the in-process instance
// has been created.
IPC_MESSAGE_CONTROL2(FrameHostMsg_DidCreateInProcessInstance,
                     int32_t /* instance */,
                     content::PepperRendererInstanceData /* instance_data */)

// Message from the renderer to the browser indicating the in-process instance
// has been destroyed.
IPC_MESSAGE_CONTROL1(FrameHostMsg_DidDeleteInProcessInstance,
                     int32_t /* instance */)

// Notification that a plugin has created a new plugin instance. The parameters
// indicate:
//  - The plugin process ID that we're creating the instance for.
//  - The instance ID of the instance being created.
//  - A PepperRendererInstanceData struct which contains properties from the
//    renderer which are associated with the plugin instance. This includes the
//    routing ID of the associated RenderFrame and the URL of plugin.
//  - Whether the plugin we're creating an instance for is external or internal.
//
// This message must be sync even though it returns no parameters to avoid
// a race condition with the plugin process. The plugin process sends messages
// to the browser that assume the browser knows about the instance. We need to
// make sure that the browser actually knows about the instance before we tell
// the plugin to run.
IPC_SYNC_MESSAGE_CONTROL4_0(
    FrameHostMsg_DidCreateOutOfProcessPepperInstance,
    int /* plugin_child_id */,
    int32_t /* pp_instance */,
    content::PepperRendererInstanceData /* creation_data */,
    bool /* is_external */)

// Notification that a plugin has destroyed an instance. This is the opposite of
// the "DidCreate" message above.
IPC_MESSAGE_CONTROL3(FrameHostMsg_DidDeleteOutOfProcessPepperInstance,
                     int /* plugin_child_id */,
                     int32_t /* pp_instance */,
                     bool /* is_external */)

#endif  // BUILDFLAG(ENABLE_PLUGINS)

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
