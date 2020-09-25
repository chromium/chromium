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
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/common/frame_delete_intention.h"
#include "content/common/frame_replication_state.h"
#include "content/common/navigation_gesture.h"
#include "content/common/navigation_params.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/impression.h"
#include "content/public/common/navigation_policy.h"
#include "content/public/common/page_state.h"
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
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/common/loader/previews_state.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/navigation/triggering_event_info.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/mojom/feature_policy/document_policy_feature.mojom.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-shared.h"
#include "third_party/blink/public/mojom/feature_policy/policy_disposition.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/frame/blocked_navigation_types.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_element_type.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom.h"
#include "third_party/blink/public/mojom/frame/tree_scope_type.mojom.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "third_party/blink/public/mojom/scroll/scrollbar_mode.mojom.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"
#include "third_party/blink/public/platform/viewport_intersection_state.h"
#include "third_party/blink/public/web/web_frame_owner_properties.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"
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
IPC_ENUM_TRAITS_MAX_VALUE(blink::ContextMenuDataMediaType,
                          blink::ContextMenuDataMediaType::kLast)
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
IPC_ENUM_TRAITS_MAX_VALUE(blink::TriggeringEventInfo,
                          blink::TriggeringEventInfo::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::UserActivationUpdateType,
                          blink::mojom::UserActivationUpdateType::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::PolicyDisposition,
                          blink::mojom::PolicyDisposition::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::FrameVisibility,
                          blink::mojom::FrameVisibility::kMaxValue)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(blink::FrameOcclusionState,
                              blink::FrameOcclusionState::kUnknown,
                              blink::FrameOcclusionState::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::WebFeature,
                          blink::mojom::WebFeature::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::RequestDestination,
                          network::mojom::RequestDestination::kMaxValue)
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

IPC_STRUCT_TRAITS_BEGIN(blink::mojom::FrameOwnerProperties)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(scrollbar_mode)
  IPC_STRUCT_TRAITS_MEMBER(margin_width)
  IPC_STRUCT_TRAITS_MEMBER(margin_height)
  IPC_STRUCT_TRAITS_MEMBER(allow_fullscreen)
  IPC_STRUCT_TRAITS_MEMBER(allow_payment_request)
  IPC_STRUCT_TRAITS_MEMBER(is_display_none)
  IPC_STRUCT_TRAITS_MEMBER(required_csp)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::FrameVisualProperties)
  IPC_STRUCT_TRAITS_MEMBER(screen_info)
  IPC_STRUCT_TRAITS_MEMBER(auto_resize_enabled)
  IPC_STRUCT_TRAITS_MEMBER(visible_viewport_size)
  IPC_STRUCT_TRAITS_MEMBER(min_size_for_auto_resize)
  IPC_STRUCT_TRAITS_MEMBER(max_size_for_auto_resize)
  IPC_STRUCT_TRAITS_MEMBER(root_widget_window_segments)
  IPC_STRUCT_TRAITS_MEMBER(capture_sequence_number)
  IPC_STRUCT_TRAITS_MEMBER(zoom_level)
  IPC_STRUCT_TRAITS_MEMBER(page_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(is_pinch_gesture_active)
  IPC_STRUCT_TRAITS_MEMBER(screen_space_rect)
  IPC_STRUCT_TRAITS_MEMBER(local_frame_size)
  IPC_STRUCT_TRAITS_MEMBER(compositor_viewport)
  IPC_STRUCT_TRAITS_MEMBER(local_surface_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::FramePolicy)
  IPC_STRUCT_TRAITS_MEMBER(sandbox_flags)
  IPC_STRUCT_TRAITS_MEMBER(container_policy)
  IPC_STRUCT_TRAITS_MEMBER(required_document_policy)
  IPC_STRUCT_TRAITS_MEMBER(allowed_to_download)
  IPC_STRUCT_TRAITS_MEMBER(disallow_document_access)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::ViewportIntersectionState)
  IPC_STRUCT_TRAITS_MEMBER(viewport_intersection)
  IPC_STRUCT_TRAITS_MEMBER(main_frame_intersection)
  IPC_STRUCT_TRAITS_MEMBER(compositor_visible_rect)
  IPC_STRUCT_TRAITS_MEMBER(occlusion_state)
  IPC_STRUCT_TRAITS_MEMBER(main_frame_viewport_size)
  IPC_STRUCT_TRAITS_MEMBER(main_frame_scroll_offset)
  IPC_STRUCT_TRAITS_MEMBER(main_frame_transform)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::FrameNavigateParams)
  IPC_STRUCT_TRAITS_MEMBER(nav_entry_id)
  IPC_STRUCT_TRAITS_MEMBER(item_sequence_number)
  IPC_STRUCT_TRAITS_MEMBER(document_sequence_number)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(base_url)
  IPC_STRUCT_TRAITS_MEMBER(referrer)
  IPC_STRUCT_TRAITS_MEMBER(transition)
  IPC_STRUCT_TRAITS_MEMBER(redirects)
  IPC_STRUCT_TRAITS_MEMBER(should_update_history)
  IPC_STRUCT_TRAITS_MEMBER(contents_mime_type)
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

// Parameters structure for mojom::FrameHost::DidCommitProvisionalLoad.
// TODO(https://crbug.com/729021): Convert this to a Mojo struct.
IPC_STRUCT_BEGIN_WITH_PARENT(FrameHostMsg_DidCommitProvisionalLoad_Params,
                             content::FrameNavigateParams)
  IPC_STRUCT_TRAITS_PARENT(content::FrameNavigateParams)

  // This is the value from the browser (copied from the navigation request)
  // indicating whether it intended to make a new entry. TODO(avi): Remove this
  // when the pending entry situation is made sane and the browser keeps them
  // around long enough to match them via nav_entry_id.
  IPC_STRUCT_MEMBER(bool, intended_as_new_entry)

  // Whether this commit created a new entry.
  IPC_STRUCT_MEMBER(bool, did_create_new_entry)

  // Whether this commit should replace the current entry.
  IPC_STRUCT_MEMBER(bool, should_replace_current_entry)

  // The gesture that initiated this navigation.
  IPC_STRUCT_MEMBER(content::NavigationGesture, gesture)

  // The HTTP method used by the navigation.
  IPC_STRUCT_MEMBER(std::string, method)

  // The POST body identifier. -1 if it doesn't exist.
  IPC_STRUCT_MEMBER(int64_t, post_id)

  // The status code of the HTTP request.
  IPC_STRUCT_MEMBER(int, http_status_code)

  // This flag is used to warn if the renderer is displaying an error page,
  // so that we can set the appropriate page type.
  IPC_STRUCT_MEMBER(bool, url_is_unreachable)

  // Serialized history item state to store in the navigation entry.
  IPC_STRUCT_MEMBER(content::PageState, page_state)

  // Original request's URL.
  IPC_STRUCT_MEMBER(GURL, original_request_url)

  // User agent override used to navigate.
  IPC_STRUCT_MEMBER(bool, is_overriding_user_agent)

  // Notifies the browser that for this navigation, the session history was
  // successfully cleared.
  IPC_STRUCT_MEMBER(bool, history_list_was_cleared)

  // Origin of the frame.  This will be replicated to any associated
  // RenderFrameProxies.
  IPC_STRUCT_MEMBER(url::Origin, origin)

  // The insecure request policy the document for the load is enforcing.
  IPC_STRUCT_MEMBER(blink::mojom::InsecureRequestPolicy,
                    insecure_request_policy)

  // The upgrade insecure navigations set the document for the load is
  // enforcing.
  IPC_STRUCT_MEMBER(std::vector<uint32_t>, insecure_navigations_set)

  // True if the document for the load is a unique origin that should be
  // considered potentially trustworthy.
  IPC_STRUCT_MEMBER(bool, has_potentially_trustworthy_unique_origin)

  // Request ID generated by the renderer.
  IPC_STRUCT_MEMBER(int, request_id)

  // A token that has been passed by the browser process when it asked the
  // renderer process to commit the navigation.
  IPC_STRUCT_MEMBER(base::UnguessableToken, navigation_token)

  // An embedding token used to signify the relationship between a document and
  // its parent. This is populated for cross-document navigations including
  // sub-documents and the main document.
  IPC_STRUCT_MEMBER(base::Optional<base::UnguessableToken>, embedding_token)
IPC_STRUCT_END()

IPC_STRUCT_TRAITS_BEGIN(network::mojom::SourceLocation)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(line)
  IPC_STRUCT_TRAITS_MEMBER(column)
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
  IPC_STRUCT_TRAITS_MEMBER(opener_feature_state)
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

IPC_STRUCT_BEGIN(FrameHostMsg_CreateChildFrame_Params)
  IPC_STRUCT_MEMBER(int32_t, parent_routing_id)
  IPC_STRUCT_MEMBER(blink::mojom::TreeScopeType, scope)
  IPC_STRUCT_MEMBER(std::string, frame_name)
  IPC_STRUCT_MEMBER(std::string, frame_unique_name)
  IPC_STRUCT_MEMBER(bool, is_created_by_script)
  IPC_STRUCT_MEMBER(blink::FramePolicy, frame_policy)
  IPC_STRUCT_MEMBER(blink::mojom::FrameOwnerProperties, frame_owner_properties)
  IPC_STRUCT_MEMBER(blink::mojom::FrameOwnerElementType,
                    frame_owner_element_type)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(FrameHostMsg_CreateChildFrame_Params_Reply)
  IPC_STRUCT_MEMBER(int32_t, child_routing_id)
  IPC_STRUCT_MEMBER(mojo::MessagePipeHandle, new_interface_provider)
  IPC_STRUCT_MEMBER(mojo::MessagePipeHandle, browser_interface_broker_handle)
  IPC_STRUCT_MEMBER(base::UnguessableToken, frame_token)
  IPC_STRUCT_MEMBER(base::UnguessableToken, devtools_frame_token)
IPC_STRUCT_END()

IPC_STRUCT_TRAITS_BEGIN(network::mojom::ContentSecurityPolicyHeader)
  IPC_STRUCT_TRAITS_MEMBER(header_value)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(source)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_BEGIN(FrameMsg_MixedContentFound_Params)
  IPC_STRUCT_MEMBER(GURL, main_resource_url)
  IPC_STRUCT_MEMBER(GURL, mixed_content_url)
  IPC_STRUCT_MEMBER(blink::mojom::RequestContextType, request_context_type)
  IPC_STRUCT_MEMBER(network::mojom::RequestDestination, request_destination)
  IPC_STRUCT_MEMBER(bool, was_allowed)
  IPC_STRUCT_MEMBER(GURL, url_before_redirects)
  IPC_STRUCT_MEMBER(bool, had_redirect)
  IPC_STRUCT_MEMBER(network::mojom::SourceLocation, source_location)
IPC_STRUCT_END()

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

#if BUILDFLAG(ENABLE_PLUGINS)
// Notifies the renderer of updates to the Plugin Power Saver origin allowlist.
IPC_MESSAGE_ROUTED1(FrameMsg_UpdatePluginContentOriginAllowlist,
                    std::set<url::Origin> /* origin_allowlist */)

// This message notifies that the frame that the volume of the Pepper instance
// for |pp_instance| should be changed to |volume|.
IPC_MESSAGE_ROUTED2(FrameMsg_SetPepperVolume,
                    int32_t /* pp_instance */,
                    double /* volume */)
#endif  // BUILDFLAG(ENABLE_PLUGINS)

// Informs the renderer that mixed content was found by the browser. The
// included data is used for instance to report to the CSP policy and to log to
// the frame console.
IPC_MESSAGE_ROUTED1(FrameMsg_MixedContentFound,
                    FrameMsg_MixedContentFound_Params)

// -----------------------------------------------------------------------------
// Messages sent from the renderer to the browser.

// Sent by the renderer when a child frame is created in the renderer.
//
// Each of these messages will have a corresponding mojom::FrameHost::Detach API
// sent when the frame is detached from the DOM.
// Note that |params_reply| is an out parameter. Browser process defines it for
// the renderer process.
// |params_reply.child_routing_id| may not be assigned MSG_ROUTING_NONE.
IPC_SYNC_MESSAGE_CONTROL1_1(FrameHostMsg_CreateChildFrame,
                            FrameHostMsg_CreateChildFrame_Params,
                            // params_reply
                            FrameHostMsg_CreateChildFrame_Params_Reply)

#if BUILDFLAG(ENABLE_PLUGINS)
// Notification sent from a renderer to the browser that a Pepper plugin
// instance is created in the DOM.
IPC_MESSAGE_ROUTED1(FrameHostMsg_PepperInstanceCreated,
                    int32_t /* pp_instance */)

// Notification sent from a renderer to the browser that a Pepper plugin
// instance is deleted from the DOM.
IPC_MESSAGE_ROUTED1(FrameHostMsg_PepperInstanceDeleted,
                    int32_t /* pp_instance */)

// Sent to the browser when the renderer detects it is blocked on a pepper
// plugin message for too long. This is also sent when it becomes unhung
// (according to the value of is_hung). The browser can give the user the
// option of killing the plugin.
IPC_MESSAGE_ROUTED3(FrameHostMsg_PepperPluginHung,
                    int /* plugin_child_id */,
                    base::FilePath /* path */,
                    bool /* is_hung */)

// Sent by the renderer process to indicate that a plugin instance has crashed.
// Note: |plugin_pid| should not be trusted. The corresponding process has
// probably died. Moreover, the ID may have been reused by a new process. Any
// usage other than displaying it in a prompt to the user is very likely to be
// wrong.
IPC_MESSAGE_ROUTED2(FrameHostMsg_PluginCrashed,
                    base::FilePath /* plugin_path */,
                    base::ProcessId /* plugin_pid */)

// Notification sent from a renderer to the browser that a Pepper plugin
// instance has started playback.
IPC_MESSAGE_ROUTED1(FrameHostMsg_PepperStartsPlayback,
                    int32_t /* pp_instance */)

// Notification sent from a renderer to the browser that a Pepper plugin
// instance has stopped playback.
IPC_MESSAGE_ROUTED1(FrameHostMsg_PepperStopsPlayback,
                    int32_t /* pp_instance */)

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

// A renderer sends this to the browser process when it wants to temporarily
// whitelist an origin's plugin content as essential. This temporary whitelist
// is specific to a top level frame, and is cleared when the whitelisting
// RenderFrame is destroyed.
IPC_MESSAGE_ROUTED1(FrameHostMsg_PluginContentOriginAllowed,
                    url::Origin /* content_origin */)

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

// A renderer sends this to the browser process when it wants to
// create a ppapi broker.  The browser will create the broker process
// if necessary, and will return a handle to the channel on success.
// On error an empty string is returned.
// The browser will respond with ViewMsg_PpapiBrokerChannelCreated.
IPC_MESSAGE_CONTROL2(FrameHostMsg_OpenChannelToPpapiBroker,
                     int /* routing_id */,
                     base::FilePath /* path */)

// A renderer sends this to the browser process when it throttles or unthrottles
// a plugin instance for the Plugin Power Saver feature.
IPC_MESSAGE_CONTROL3(FrameHostMsg_PluginInstanceThrottleStateChange,
                     int /* plugin_child_id */,
                     int32_t /* pp_instance */,
                     bool /* is_throttled */)
#endif  // BUILDFLAG(ENABLE_PLUGINS)

// Indicates that the current frame has finished running its unload handler (if
// one was registered) and has been detached, as a response to
// UnfreezableFrameMsg_Unload message from the browser process.
IPC_MESSAGE_ROUTED0(FrameHostMsg_Unload_ACK)

// Tells the browser that a child's visual properties have changed.
IPC_MESSAGE_ROUTED1(FrameHostMsg_SynchronizeVisualProperties,
                    blink::FrameVisualProperties)

// Sent by a parent frame to notify its child about the state of the child's
// intersection with the parent's viewport, primarily for use by the
// IntersectionObserver API.
IPC_MESSAGE_ROUTED1(FrameHostMsg_UpdateViewportIntersection,
                    blink::ViewportIntersectionState /* intersection_state */)

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
