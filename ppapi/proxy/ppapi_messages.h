// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPAPI_MESSAGES_H_
#define PPAPI_PROXY_PPAPI_MESSAGES_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/sync_socket.h"
#include "build/build_config.h"

#ifdef WIN32
// base/sync_socket.h will define MemoryBarrier (a Win32 macro) that
// would clash with MemoryBarrier in base/atomicops.h if someone uses
// that together with this header.
#undef MemoryBarrier
#endif  // WIN32

#include "base/files/file_path.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/common/gpu_command_buffer_traits.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/dev/pp_video_capture_dev.h"
#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/c/dev/ppp_printing_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_codecs.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_tcp_socket.h"
#include "ppapi/c/ppb_text_input_controller.h"
#include "ppapi/c/ppb_udp_socket.h"
#include "ppapi/c/ppb_video_encoder.h"
#include "ppapi/c/private/pp_private_font_charset.h"
#include "ppapi/c/private/pp_video_capture_format.h"
#include "ppapi/c/private/ppb_host_resolver_private.h"
#include "ppapi/c/private/ppb_isolated_file_system_private.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/proxy/host_resolver_private_resource.h"
#include "ppapi/proxy/network_list_resource.h"
#include "ppapi/proxy/ppapi_param_traits.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/proxy/serialized_handle.h"
#include "ppapi/proxy/serialized_structs.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/dir_contents.h"
#include "ppapi/shared_impl/file_growth.h"
#include "ppapi/shared_impl/file_path.h"
#include "ppapi/shared_impl/file_ref_create_info.h"
#include "ppapi/shared_impl/media_stream_audio_track_shared.h"
#include "ppapi/shared_impl/media_stream_video_track_shared.h"
#include "ppapi/shared_impl/ppapi_nacl_plugin_args.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "ppapi/shared_impl/ppb_device_ref_shared.h"
#include "ppapi/shared_impl/ppb_graphics_3d_shared.h"
#include "ppapi/shared_impl/ppb_input_event_shared.h"
#include "ppapi/shared_impl/ppb_tcp_socket_shared.h"
#include "ppapi/shared_impl/ppb_view_shared.h"
#include "ppapi/shared_impl/private/ppb_x509_certificate_private_shared.h"
#include "ppapi/shared_impl/socket_option_data.h"
#include "ppapi/shared_impl/url_request_info_data.h"
#include "ppapi/shared_impl/url_response_info_data.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT PPAPI_PROXY_EXPORT

#define IPC_MESSAGE_START PpapiMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(ppapi::TCPSocketVersion,
                          ppapi::TCP_SOCKET_VERSION_1_1_OR_ABOVE)
IPC_ENUM_TRAITS_MAX_VALUE(PP_AudioSampleRate, PP_AUDIOSAMPLERATE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(PP_DeviceType_Dev, PP_DEVICETYPE_DEV_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(PP_FileSystemType, PP_FILESYSTEMTYPE_ISOLATED)
IPC_ENUM_TRAITS_MAX_VALUE(PP_FileType, PP_FILETYPE_OTHER)
IPC_ENUM_TRAITS_MAX_VALUE(PP_ImageDataFormat, PP_IMAGEDATAFORMAT_LAST)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(PP_InputEvent_MouseButton,
                              PP_INPUTEVENT_MOUSEBUTTON_FIRST,
                              PP_INPUTEVENT_MOUSEBUTTON_LAST)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(PP_InputEvent_Type,
                              PP_INPUTEVENT_TYPE_FIRST,
                              PP_INPUTEVENT_TYPE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(PP_IsolatedFileSystemType_Private,
                          PP_ISOLATEDFILESYSTEMTYPE_PRIVATE_CRX)
IPC_ENUM_TRAITS_MAX_VALUE(PP_NetAddressFamily_Private,
                          PP_NETADDRESSFAMILY_PRIVATE_IPV6)
IPC_ENUM_TRAITS_MAX_VALUE(PP_NetworkList_State, PP_NETWORKLIST_STATE_UP)
IPC_ENUM_TRAITS_MAX_VALUE(PP_NetworkList_Type, PP_NETWORKLIST_TYPE_CELLULAR)
IPC_ENUM_TRAITS_MAX_VALUE(PP_PrintOrientation_Dev,
                          PP_PRINTORIENTATION_ROTATED_LAST)
IPC_ENUM_TRAITS(PP_PrintOutputFormat_Dev)  // Bitmask.
IPC_ENUM_TRAITS_MAX_VALUE(PP_PrintScalingOption_Dev, PP_PRINTSCALINGOPTION_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(PP_PrivateFontCharset, PP_PRIVATEFONTCHARSET_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(PP_TCPSocket_Option,
                          PP_TCPSOCKET_OPTION_RECV_BUFFER_SIZE)
IPC_ENUM_TRAITS_MAX_VALUE(PP_TextInput_Type, PP_TEXTINPUT_TYPE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(PP_UDPSocket_Option,
                          PP_UDPSOCKET_OPTION_MULTICAST_TTL)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(PP_VideoDecodeError_Dev,
                              PP_VIDEODECODERERROR_FIRST,
                              PP_VIDEODECODERERROR_LAST)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(PP_VideoDecoder_Profile,
                              PP_VIDEODECODER_PROFILE_FIRST,
                              PP_VIDEODECODER_PROFILE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(PP_VideoFrame_Format, PP_VIDEOFRAME_FORMAT_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(PP_HardwareAcceleration, PP_HARDWAREACCELERATION_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(PP_VideoProfile, PP_VIDEOPROFILE_MAX)

IPC_STRUCT_TRAITS_BEGIN(PP_Point)
  IPC_STRUCT_TRAITS_MEMBER(x)
  IPC_STRUCT_TRAITS_MEMBER(y)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_FloatPoint)
  IPC_STRUCT_TRAITS_MEMBER(x)
  IPC_STRUCT_TRAITS_MEMBER(y)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_Size)
  IPC_STRUCT_TRAITS_MEMBER(height)
  IPC_STRUCT_TRAITS_MEMBER(width)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_FloatSize)
  IPC_STRUCT_TRAITS_MEMBER(height)
  IPC_STRUCT_TRAITS_MEMBER(width)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_Rect)
  IPC_STRUCT_TRAITS_MEMBER(point)
  IPC_STRUCT_TRAITS_MEMBER(size)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_FloatRect)
  IPC_STRUCT_TRAITS_MEMBER(point)
  IPC_STRUCT_TRAITS_MEMBER(size)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_ImageDataDesc)
  IPC_STRUCT_TRAITS_MEMBER(format)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(stride)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_PictureBuffer_Dev)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(texture_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_Picture_Dev)
  IPC_STRUCT_TRAITS_MEMBER(picture_buffer_id)
  IPC_STRUCT_TRAITS_MEMBER(bitstream_buffer_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_PrintPageNumberRange_Dev)
  IPC_STRUCT_TRAITS_MEMBER(first_page_number)
  IPC_STRUCT_TRAITS_MEMBER(last_page_number)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_VideoCaptureDeviceInfo_Dev)
  IPC_STRUCT_TRAITS_MEMBER(width)
  IPC_STRUCT_TRAITS_MEMBER(height)
  IPC_STRUCT_TRAITS_MEMBER(frames_per_second)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_HostResolver_Private_Hint)
  IPC_STRUCT_TRAITS_MEMBER(family)
  IPC_STRUCT_TRAITS_MEMBER(flags)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_PrintSettings_Dev)
  IPC_STRUCT_TRAITS_MEMBER(printable_area)
  IPC_STRUCT_TRAITS_MEMBER(content_area)
  IPC_STRUCT_TRAITS_MEMBER(paper_size)
  IPC_STRUCT_TRAITS_MEMBER(dpi)
  IPC_STRUCT_TRAITS_MEMBER(orientation)
  IPC_STRUCT_TRAITS_MEMBER(print_scaling_option)
  IPC_STRUCT_TRAITS_MEMBER(grayscale)
  IPC_STRUCT_TRAITS_MEMBER(format)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_URLComponent_Dev)
  IPC_STRUCT_TRAITS_MEMBER(begin)
  IPC_STRUCT_TRAITS_MEMBER(len)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_URLComponents_Dev)
  IPC_STRUCT_TRAITS_MEMBER(scheme)
  IPC_STRUCT_TRAITS_MEMBER(username)
  IPC_STRUCT_TRAITS_MEMBER(password)
  IPC_STRUCT_TRAITS_MEMBER(host)
  IPC_STRUCT_TRAITS_MEMBER(port)
  IPC_STRUCT_TRAITS_MEMBER(path)
  IPC_STRUCT_TRAITS_MEMBER(query)
  IPC_STRUCT_TRAITS_MEMBER(ref)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_VideoCaptureFormat)
  IPC_STRUCT_TRAITS_MEMBER(frame_size)
  IPC_STRUCT_TRAITS_MEMBER(frame_rate)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_FileInfo)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(system_type)
  IPC_STRUCT_TRAITS_MEMBER(creation_time)
  IPC_STRUCT_TRAITS_MEMBER(last_access_time)
  IPC_STRUCT_TRAITS_MEMBER(last_modified_time)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::FileGrowth)
  IPC_STRUCT_TRAITS_MEMBER(max_written_offset)
  IPC_STRUCT_TRAITS_MEMBER(append_mode_write_amount)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::DeviceRefData)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::DirEntry)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(is_dir)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::FileRefCreateInfo)
  IPC_STRUCT_TRAITS_MEMBER(file_system_type)
  IPC_STRUCT_TRAITS_MEMBER(internal_path)
  IPC_STRUCT_TRAITS_MEMBER(display_name)
  IPC_STRUCT_TRAITS_MEMBER(browser_pending_host_resource_id)
  IPC_STRUCT_TRAITS_MEMBER(renderer_pending_host_resource_id)
  IPC_STRUCT_TRAITS_MEMBER(file_system_plugin_resource)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::MediaStreamAudioTrackShared::Attributes)
  IPC_STRUCT_TRAITS_MEMBER(buffers)
  IPC_STRUCT_TRAITS_MEMBER(duration)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::MediaStreamVideoTrackShared::Attributes)
  IPC_STRUCT_TRAITS_MEMBER(buffers)
  IPC_STRUCT_TRAITS_MEMBER(width)
  IPC_STRUCT_TRAITS_MEMBER(height)
  IPC_STRUCT_TRAITS_MEMBER(format)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::ViewData)
  IPC_STRUCT_TRAITS_MEMBER(rect)
  IPC_STRUCT_TRAITS_MEMBER(is_fullscreen)
  IPC_STRUCT_TRAITS_MEMBER(is_page_visible)
  IPC_STRUCT_TRAITS_MEMBER(clip_rect)
  IPC_STRUCT_TRAITS_MEMBER(device_scale)
  IPC_STRUCT_TRAITS_MEMBER(css_scale)
  IPC_STRUCT_TRAITS_MEMBER(scroll_offset)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_TouchPoint)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(position)
  IPC_STRUCT_TRAITS_MEMBER(radius)
  IPC_STRUCT_TRAITS_MEMBER(rotation_angle)
  IPC_STRUCT_TRAITS_MEMBER(pressure)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::Preferences)
  IPC_STRUCT_TRAITS_MEMBER(standard_font_family_map)
  IPC_STRUCT_TRAITS_MEMBER(fixed_font_family_map)
  IPC_STRUCT_TRAITS_MEMBER(serif_font_family_map)
  IPC_STRUCT_TRAITS_MEMBER(sans_serif_font_family_map)
  IPC_STRUCT_TRAITS_MEMBER(default_font_size)
  IPC_STRUCT_TRAITS_MEMBER(default_fixed_font_size)
  IPC_STRUCT_TRAITS_MEMBER(number_of_cpu_cores)
  IPC_STRUCT_TRAITS_MEMBER(is_3d_supported)
  IPC_STRUCT_TRAITS_MEMBER(is_stage3d_supported)
  IPC_STRUCT_TRAITS_MEMBER(is_stage3d_baseline_supported)
  IPC_STRUCT_TRAITS_MEMBER(is_accelerated_video_decode_enabled)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::TouchPointWithTilt)
  IPC_STRUCT_TRAITS_MEMBER(touch)
  IPC_STRUCT_TRAITS_MEMBER(tilt)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::InputEventData)
  IPC_STRUCT_TRAITS_MEMBER(is_filtered)
  IPC_STRUCT_TRAITS_MEMBER(event_type)
  IPC_STRUCT_TRAITS_MEMBER(event_time_stamp)
  IPC_STRUCT_TRAITS_MEMBER(event_modifiers)
  IPC_STRUCT_TRAITS_MEMBER(mouse_button)
  IPC_STRUCT_TRAITS_MEMBER(mouse_position)
  IPC_STRUCT_TRAITS_MEMBER(mouse_click_count)
  IPC_STRUCT_TRAITS_MEMBER(mouse_movement)
  IPC_STRUCT_TRAITS_MEMBER(wheel_delta)
  IPC_STRUCT_TRAITS_MEMBER(wheel_ticks)
  IPC_STRUCT_TRAITS_MEMBER(wheel_scroll_by_page)
  IPC_STRUCT_TRAITS_MEMBER(key_code)
  IPC_STRUCT_TRAITS_MEMBER(code)
  IPC_STRUCT_TRAITS_MEMBER(character_text)
  IPC_STRUCT_TRAITS_MEMBER(composition_segment_offsets)
  IPC_STRUCT_TRAITS_MEMBER(composition_target_segment)
  IPC_STRUCT_TRAITS_MEMBER(composition_selection_start)
  IPC_STRUCT_TRAITS_MEMBER(composition_selection_end)
  IPC_STRUCT_TRAITS_MEMBER(touches)
  IPC_STRUCT_TRAITS_MEMBER(changed_touches)
  IPC_STRUCT_TRAITS_MEMBER(target_touches)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::HostPortPair)
  IPC_STRUCT_TRAITS_MEMBER(host)
  IPC_STRUCT_TRAITS_MEMBER(port)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::URLRequestInfoData)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(method)
  IPC_STRUCT_TRAITS_MEMBER(headers)
  IPC_STRUCT_TRAITS_MEMBER(follow_redirects)
  IPC_STRUCT_TRAITS_MEMBER(record_download_progress)
  IPC_STRUCT_TRAITS_MEMBER(record_upload_progress)
  IPC_STRUCT_TRAITS_MEMBER(has_custom_referrer_url)
  IPC_STRUCT_TRAITS_MEMBER(custom_referrer_url)
  IPC_STRUCT_TRAITS_MEMBER(allow_cross_origin_requests)
  IPC_STRUCT_TRAITS_MEMBER(allow_credentials)
  IPC_STRUCT_TRAITS_MEMBER(has_custom_content_transfer_encoding)
  IPC_STRUCT_TRAITS_MEMBER(custom_content_transfer_encoding)
  IPC_STRUCT_TRAITS_MEMBER(prefetch_buffer_upper_threshold)
  IPC_STRUCT_TRAITS_MEMBER(prefetch_buffer_lower_threshold)
  IPC_STRUCT_TRAITS_MEMBER(has_custom_user_agent)
  IPC_STRUCT_TRAITS_MEMBER(custom_user_agent)
  IPC_STRUCT_TRAITS_MEMBER(body)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::URLRequestInfoData::BodyItem)
  IPC_STRUCT_TRAITS_MEMBER(is_file)
  IPC_STRUCT_TRAITS_MEMBER(data)
  IPC_STRUCT_TRAITS_MEMBER(file_ref_pp_resource)
  IPC_STRUCT_TRAITS_MEMBER(start_offset)
  IPC_STRUCT_TRAITS_MEMBER(number_of_bytes)
  IPC_STRUCT_TRAITS_MEMBER(expected_last_modified_time)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::URLResponseInfoData)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(headers)
  IPC_STRUCT_TRAITS_MEMBER(status_code)
  IPC_STRUCT_TRAITS_MEMBER(status_text)
  IPC_STRUCT_TRAITS_MEMBER(redirect_url)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::proxy::SerializedNetworkInfo)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(state)
  IPC_STRUCT_TRAITS_MEMBER(addresses)
  IPC_STRUCT_TRAITS_MEMBER(display_name)
  IPC_STRUCT_TRAITS_MEMBER(mtu)
IPC_STRUCT_TRAITS_END()

// Only whitelisted switches passed through PpapiNaClPluginArgs.
// The list of switches can be found in:
//   components/nacl/browser/nacl_process_host.cc
IPC_STRUCT_TRAITS_BEGIN(ppapi::PpapiNaClPluginArgs)
  IPC_STRUCT_TRAITS_MEMBER(off_the_record)
  IPC_STRUCT_TRAITS_MEMBER(permissions)
  IPC_STRUCT_TRAITS_MEMBER(switch_names)
  IPC_STRUCT_TRAITS_MEMBER(switch_values)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PP_VideoProfileDescription)
IPC_STRUCT_TRAITS_MEMBER(profile)
IPC_STRUCT_TRAITS_MEMBER(max_resolution)
IPC_STRUCT_TRAITS_MEMBER(max_framerate_numerator)
IPC_STRUCT_TRAITS_MEMBER(max_framerate_denominator)
IPC_STRUCT_TRAITS_MEMBER(hardware_accelerated)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ppapi::Graphics3DContextAttribs)
IPC_STRUCT_TRAITS_MEMBER(offscreen_framebuffer_size)
IPC_STRUCT_TRAITS_MEMBER(alpha_size)
IPC_STRUCT_TRAITS_MEMBER(depth_size)
IPC_STRUCT_TRAITS_MEMBER(stencil_size)
IPC_STRUCT_TRAITS_MEMBER(samples)
IPC_STRUCT_TRAITS_MEMBER(sample_buffers)
IPC_STRUCT_TRAITS_MEMBER(buffer_preserved)
IPC_STRUCT_TRAITS_MEMBER(single_buffer)
IPC_STRUCT_TRAITS_END()

// These are from the browser to the plugin.
// Loads the given plugin.
IPC_MESSAGE_CONTROL2(PpapiMsg_LoadPlugin,
                     base::FilePath /* path */,
                     ppapi::PpapiPermissions /* permissions */)

// Creates a channel to talk to a renderer. The plugin will respond with
// PpapiHostMsg_ChannelCreated.
// If |renderer_pid| is base::kNullProcessId, this is a channel used by the
// browser itself.
IPC_MESSAGE_CONTROL3(PpapiMsg_CreateChannel,
                     base::ProcessId /* renderer_pid */,
                     int /* renderer_child_id */,
                     bool /* incognito */)

// Initializes the IPC dispatchers in the NaCl plugin.
IPC_MESSAGE_CONTROL1(PpapiMsg_InitializeNaClDispatcher,
                     ppapi::PpapiNaClPluginArgs /* args */)

// Each plugin may be referenced by multiple renderers. We need the instance
// IDs to be unique within a plugin, despite coming from different renderers,
// and unique within a renderer, despite going to different plugins. This means
// that neither the renderer nor the plugin can generate instance IDs without
// consulting the other.
//
// We resolve this by having the renderer generate a unique instance ID inside
// its process. It then asks the plugin to reserve that ID by sending this sync
// message. If the plugin has not yet seen this ID, it will remember it as used
// (to prevent a race condition if another renderer tries to then use the same
// instance), and set usable as true.
//
// If the plugin has already seen the instance ID, it will set usable as false
// and the renderer must retry a new instance ID.
IPC_SYNC_MESSAGE_CONTROL1_1(PpapiMsg_ReserveInstanceId,
                            PP_Instance /* instance */,
                            bool /* usable */)

// Passes the WebKit preferences to the plugin.
IPC_MESSAGE_CONTROL1(PpapiMsg_SetPreferences,
                     ppapi::Preferences)

// Sent in both directions to see if the other side supports the given
// interface.
IPC_SYNC_MESSAGE_CONTROL1_1(PpapiMsg_SupportsInterface,
                            std::string /* interface_name */,
                            bool /* result */)

#if !BUILDFLAG(IS_NACL)
// Network state notification from the browser for implementing
// PPP_NetworkState_Dev.
IPC_MESSAGE_CONTROL1(PpapiMsg_SetNetworkState,
                     bool /* online */)

#endif  // !BUILDFLAG(IS_NACL)

// PPB_Audio.

// Notifies the result of the audio stream create call. This is called in
// both error cases and in the normal success case. These cases are
// differentiated by the result code, which is one of the standard PPAPI
// result codes.
//
// The handler of this message should always close all of the handles passed
// in, since some could be valid even in the error case.
IPC_MESSAGE_ROUTED4(PpapiMsg_PPBAudio_NotifyAudioStreamCreated,
                    ppapi::HostResource /* audio_id */,
                    int32_t /* result_code (will be != PP_OK on failure) */,
                    ppapi::proxy::SerializedHandle /* socket_handle */,
                    ppapi::proxy::SerializedHandle /* handle */)

// PPB_Graphics3D.
IPC_MESSAGE_ROUTED2(PpapiMsg_PPBGraphics3D_SwapBuffersACK,
                    ppapi::HostResource /* graphics_3d */,
                    int32_t /* pp_error */)

// PPB_ImageData.
IPC_MESSAGE_ROUTED1(PpapiMsg_PPBImageData_NotifyUnusedImageData,
                    ppapi::HostResource /* old_image_data */)

// PPB_Instance.
IPC_MESSAGE_ROUTED2(PpapiMsg_PPBInstance_MouseLockComplete,
                    PP_Instance /* instance */,
                    int32_t /* result */)

// PPP_Class.
IPC_SYNC_MESSAGE_ROUTED3_2(PpapiMsg_PPPClass_HasProperty,
                           int64_t /* ppp_class */,
                           int64_t /* object */,
                           ppapi::proxy::SerializedVar /* property */,
                           ppapi::proxy::SerializedVar /* out_exception */,
                           bool /* result */)
IPC_SYNC_MESSAGE_ROUTED3_2(PpapiMsg_PPPClass_HasMethod,
                           int64_t /* ppp_class */,
                           int64_t /* object */,
                           ppapi::proxy::SerializedVar /* method */,
                           ppapi::proxy::SerializedVar /* out_exception */,
                           bool /* result */)
IPC_SYNC_MESSAGE_ROUTED3_2(PpapiMsg_PPPClass_GetProperty,
                           int64_t /* ppp_class */,
                           int64_t /* object */,
                           ppapi::proxy::SerializedVar /* property */,
                           ppapi::proxy::SerializedVar /* out_exception */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiMsg_PPPClass_EnumerateProperties,
                           int64_t /* ppp_class */,
                           int64_t /* object */,
                           std::vector<ppapi::proxy::SerializedVar> /* props */,
                           ppapi::proxy::SerializedVar /* out_exception */)
IPC_SYNC_MESSAGE_ROUTED4_1(PpapiMsg_PPPClass_SetProperty,
                           int64_t /* ppp_class */,
                           int64_t /* object */,
                           ppapi::proxy::SerializedVar /* name */,
                           ppapi::proxy::SerializedVar /* value */,
                           ppapi::proxy::SerializedVar /* out_exception */)
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiMsg_PPPClass_RemoveProperty,
                           int64_t /* ppp_class */,
                           int64_t /* object */,
                           ppapi::proxy::SerializedVar /* property */,
                           ppapi::proxy::SerializedVar /* out_exception */)
IPC_SYNC_MESSAGE_ROUTED4_2(PpapiMsg_PPPClass_Call,
                           int64_t /* ppp_class */,
                           int64_t /* object */,
                           ppapi::proxy::SerializedVar /* method_name */,
                           std::vector<ppapi::proxy::SerializedVar> /* args */,
                           ppapi::proxy::SerializedVar /* out_exception */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED3_2(PpapiMsg_PPPClass_Construct,
                           int64_t /* ppp_class */,
                           int64_t /* object */,
                           std::vector<ppapi::proxy::SerializedVar> /* args */,
                           ppapi::proxy::SerializedVar /* out_exception */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPClass_Deallocate,
                    int64_t /* ppp_class */,
                    int64_t /* object */)

// PPP_Graphics3D_Dev.
IPC_MESSAGE_ROUTED1(PpapiMsg_PPPGraphics3D_ContextLost,
                    PP_Instance /* instance */)

// PPP_InputEvent.
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPInputEvent_HandleInputEvent,
                    PP_Instance /* instance */,
                    ppapi::InputEventData /* data */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiMsg_PPPInputEvent_HandleFilteredInputEvent,
                           PP_Instance /* instance */,
                           ppapi::InputEventData /* data */,
                           PP_Bool /* result */)

// PPP_Instance.
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiMsg_PPPInstance_DidCreate,
                           PP_Instance /* instance */,
                           std::vector<std::string> /* argn */,
                           std::vector<std::string> /* argv */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED1_0(PpapiMsg_PPPInstance_DidDestroy,
                           PP_Instance /* instance */)
IPC_MESSAGE_ROUTED3(PpapiMsg_PPPInstance_DidChangeView,
                    PP_Instance /* instance */,
                    ppapi::ViewData /* new_data */,
                    PP_Bool /* flash_fullscreen */)
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPInstance_DidChangeFocus,
                    PP_Instance /* instance */,
                    PP_Bool /* has_focus */)
IPC_MESSAGE_ROUTED3(PpapiMsg_PPPInstance_HandleDocumentLoad,
    PP_Instance /* instance */,
    int /* pending_loader_host_id */,
    ppapi::URLResponseInfoData /* response */)

// PPP_Messaging and PPP_MessageHandler.
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPMessaging_HandleMessage,
                    PP_Instance /* instance */,
                    ppapi::proxy::SerializedVar /* message */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiMsg_PPPMessageHandler_HandleBlockingMessage,
                           PP_Instance /* instance */,
                           ppapi::proxy::SerializedVar /* message */,
                           ppapi::proxy::SerializedVar /* result */,
                           bool /* was_handled */)

// PPP_MouseLock.
IPC_MESSAGE_ROUTED1(PpapiMsg_PPPMouseLock_MouseLockLost,
                    PP_Instance /* instance */)

// PPP_Printing
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiMsg_PPPPrinting_QuerySupportedFormats,
                           PP_Instance /* instance */,
                           uint32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiMsg_PPPPrinting_Begin,
                           PP_Instance /* instance */,
                           PP_PrintSettings_Dev /* settings */,
                           int32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiMsg_PPPPrinting_PrintPages,
                           PP_Instance /* instance */,
                           std::vector<PP_PrintPageNumberRange_Dev> /* pages */,
                           ppapi::HostResource /* result */)
IPC_MESSAGE_ROUTED1(PpapiMsg_PPPPrinting_End,
                    PP_Instance /* instance */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiMsg_PPPPrinting_IsScalingDisabled,
                           PP_Instance /* instance */,
                           bool /* result */)

// PPP_TextInput.
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPTextInput_RequestSurroundingText,
                   PP_Instance /* instance */,
                   uint32_t /* desired_number_of_characters */)

#if !BUILDFLAG(IS_NACL)
// PPP_Instance_Private.
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiMsg_PPPInstancePrivate_GetInstanceObject,
                           PP_Instance /* instance */,
                           ppapi::proxy::SerializedVar /* result */)

#endif  // !BUILDFLAG(IS_NACL)

// This message is sent from the renderer to the PNaCl compiler process
// (NaCl untrusted code -- a nexe).  This implements the init_callback()
// IRT interface.  This message initializes the translation process,
// providing an array of object file FDs for writing output to, along with
// other parameters.
IPC_SYNC_MESSAGE_CONTROL3_2(PpapiMsg_PnaclTranslatorCompileInit,
                            /* number of threads to use */
                            int,
                            /* object file FDs for outputs */
                            std::vector<ppapi::proxy::SerializedHandle>,
                            /* list of command line flags */
                            std::vector<std::string>,
                            /* success status result */
                            bool,
                            /* error string if the success field is false */
                            std::string)

// This message is sent from the renderer to the PNaCl compiler process
// (NaCl untrusted code -- a nexe).  This implements the data_callback()
// IRT interface.  This message sends the next chunk of input bitcode data
// to the compiler process.  If the success result is false (for failure),
// the renderer can still invoke PpapiMsg_PnaclTranslatorCompileEnd to get
// a message describing the error.
IPC_SYNC_MESSAGE_CONTROL1_1(PpapiMsg_PnaclTranslatorCompileChunk,
                            /* chunk of data for the input pexe file */
                            std::string,
                            /* success status result */
                            bool)

// This message is sent from the renderer to the PNaCl compiler process
// (NaCl untrusted code -- a nexe).  This implements the end_callback() IRT
// interface.  This blocks until translation is complete or an error has
// occurred.
IPC_SYNC_MESSAGE_CONTROL0_2(PpapiMsg_PnaclTranslatorCompileEnd,
                            /* success status result */
                            bool,
                            /* error string if the success field is false */
                            std::string)

// This message is sent from the renderer to the PNaCl linker process
// (NaCl untrusted code -- a nexe).  This message tells the PNaCl
// linker to link the given object files together to produce a nexe
// file, writing the output to the given file handle.
IPC_SYNC_MESSAGE_CONTROL2_1(PpapiMsg_PnaclTranslatorLink,
                            /* object file FDs for inputs */
                            std::vector<ppapi::proxy::SerializedHandle>,
                            /* nexe file FD for output */
                            ppapi::proxy::SerializedHandle,
                            /* success status result */
                            bool)


// These are from the plugin to the renderer.

// Reply to PpapiMsg_CreateChannel. The handle will be NULL if the channel
// could not be established. This could be because the IPC could not be created
// for some weird reason, but more likely that the plugin failed to load or
// initialize properly.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_ChannelCreated,
                     IPC::ChannelHandle /* handle */)

// Notify the renderer that the PPAPI channel gets ready in the plugin.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_StartupInitializationComplete)

// This is sent from a nexe (NaCl untrusted code) to the renderer, to open a
// file listed in a NaCl manifest file (NMF).  It is part of the
// implementation of open_resource(), which is defined in NaCl's irt.h.
//
// This call returns a read-only file handle from the renderer.  When using
// validation caching, this handle is not used: The NaCl loader process will
// reacquire the handle from the more-trusted browser process via
// NaClProcessMsg_ResolveFileToken, passing the token values returned here.
//
// Note that the open_resource() interface is not a PPAPI interface (in the
// sense that it's not defined in ppapi/c/), but this message is defined here
// in ppapi_messages.h (rather than in components/nacl/) because half of the
// implementation of open_resource() lives in ppapi/nacl_irt/, and because
// this message must be processed by ppapi/proxy/nacl_message_scanner.cc.
IPC_SYNC_MESSAGE_CONTROL1_3(PpapiHostMsg_OpenResource,
                            std::string /* key */,
                            uint64_t /* file_token_lo */,
                            uint64_t /* file_token_hi */,
                            ppapi::proxy::SerializedHandle /* fd */)

// Logs the given message to the console of all instances.
IPC_MESSAGE_CONTROL4(PpapiHostMsg_LogWithSource,
                     PP_Instance /* instance */,
                     int /* log_level */,
                     std::string /* source */,
                     std::string /* value */)

// PPB_Audio.
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBAudio_Create,
                           PP_Instance /* instance_id */,
                           int32_t /* sample_rate */,
                           uint32_t /* sample_frame_count */,
                           ppapi::HostResource /* result */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBAudio_StartOrStop,
                    ppapi::HostResource /* audio_id */,
                    bool /* play */)

// PPB_Core.
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBCore_AddRefResource,
                    ppapi::HostResource)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBCore_ReleaseResource,
                    ppapi::HostResource)

// PPB_Graphics3D.
IPC_SYNC_MESSAGE_ROUTED3_5(
    PpapiHostMsg_PPBGraphics3D_Create,
    PP_Instance /* instance */,
    ppapi::HostResource /* share_context */,
    ppapi::Graphics3DContextAttribs /* context_attribs */,
    ppapi::HostResource /* result */,
    gpu::Capabilities /* capabilities */,
    gpu::GLCapabilities /* gl_capabilities */,
    ppapi::proxy::SerializedHandle /* shared_state */,
    gpu::CommandBufferId /* command_buffer_id */)
IPC_SYNC_MESSAGE_ROUTED2_0(PpapiHostMsg_PPBGraphics3D_SetGetBuffer,
                           ppapi::HostResource /* context */,
                           int32_t /* transfer_buffer_id */)
IPC_SYNC_MESSAGE_ROUTED3_2(PpapiHostMsg_PPBGraphics3D_WaitForTokenInRange,
                           ppapi::HostResource /* context */,
                           int32_t /* start */,
                           int32_t /* end */,
                           gpu::CommandBuffer::State /* state */,
                           bool /* success */)
IPC_SYNC_MESSAGE_ROUTED4_2(PpapiHostMsg_PPBGraphics3D_WaitForGetOffsetInRange,
                           ppapi::HostResource /* context */,
                           uint32_t /* set_get_buffer_count */,
                           int32_t /* start */,
                           int32_t /* end */,
                           gpu::CommandBuffer::State /* state */,
                           bool /* success */)
IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBGraphics3D_AsyncFlush,
                    ppapi::HostResource /* context */,
                    int32_t /* put_offset */,
                    uint64_t /* release_count */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBGraphics3D_CreateTransferBuffer,
                           ppapi::HostResource /* context */,
                           uint32_t /* size */,
                           int32_t /* id */,
                           ppapi::proxy::SerializedHandle /* transfer_buffer */)
IPC_SYNC_MESSAGE_ROUTED2_0(PpapiHostMsg_PPBGraphics3D_DestroyTransferBuffer,
                           ppapi::HostResource /* context */,
                           int32_t /* id */)
// The receiver of this message takes ownership of the front buffer of the GL
// context. Each call to PpapiHostMsg_PPBGraphics3D_SwapBuffers must be preceded
// by exactly one call to
// PpapiHostMsg_PPBGraphics3D_ResolveAndDetachFramebuffer. The SyncToken passed
// to PpapiHostMsg_PPBGraphics3D_SwapBuffers must be generated after this
// message is sent.
IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBGraphics3D_SwapBuffers,
                    ppapi::HostResource /* graphics_3d */,
                    gpu::SyncToken /* sync_token */,
                    gfx::Size /* size */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBGraphics3D_EnsureWorkVisible,
                    ppapi::HostResource /* context */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBGraphics3D_ResolveAndDetachFramebuffer,
                    ppapi::HostResource /* graphics_3d */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBGraphics3D_Resize,
                    ppapi::HostResource /* graphics_3d */,
                    gfx::Size /* size */)

// PPB_ImageData.
IPC_SYNC_MESSAGE_ROUTED4_3(PpapiHostMsg_PPBImageData_CreatePlatform,
                           PP_Instance /* instance */,
                           int32_t /* format */,
                           PP_Size /* size */,
                           PP_Bool /* init_to_zero */,
                           ppapi::HostResource /* result_resource */,
                           PP_ImageDataDesc /* image_data_desc */,
                           ppapi::proxy::SerializedHandle /* result */)
IPC_SYNC_MESSAGE_ROUTED4_3(PpapiHostMsg_PPBImageData_CreateSimple,
                           PP_Instance /* instance */,
                           int32_t /* format */,
                           PP_Size /* size */,
                           PP_Bool /* init_to_zero */,
                           ppapi::HostResource /* result_resource */,
                           PP_ImageDataDesc /* image_data_desc */,
                           ppapi::proxy::SerializedHandle /* result */)

// PPB_Instance.
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBInstance_GetWindowObject,
                           PP_Instance /* instance */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBInstance_GetOwnerElementObject,
                           PP_Instance /* instance */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBInstance_BindGraphics,
                    PP_Instance /* instance */,
                    PP_Resource /* device */)
IPC_SYNC_MESSAGE_ROUTED1_1(
    PpapiHostMsg_PPBInstance_GetAudioHardwareOutputSampleRate,
                           PP_Instance /* instance */,
                           uint32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(
    PpapiHostMsg_PPBInstance_GetAudioHardwareOutputBufferSize,
                           PP_Instance /* instance */,
                           uint32_t /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBInstance_IsFullFrame,
                           PP_Instance /* instance */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBInstance_ExecuteScript,
                           PP_Instance /* instance */,
                           ppapi::proxy::SerializedVar /* script */,
                           ppapi::proxy::SerializedVar /* out_exception */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBInstance_GetDefaultCharSet,
                           PP_Instance /* instance */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBInstance_SetFullscreen,
                           PP_Instance /* instance */,
                           PP_Bool /* fullscreen */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED1_2(PpapiHostMsg_PPBInstance_GetScreenSize,
                           PP_Instance /* instance */,
                           PP_Bool /* result */,
                           PP_Size /* size */)
IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBInstance_RequestInputEvents,
                    PP_Instance /* instance */,
                    bool /* is_filtering */,
                    uint32_t /* event_classes */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBInstance_ClearInputEvents,
                    PP_Instance /* instance */,
                    uint32_t /* event_classes */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBInstance_PostMessage,
                    PP_Instance /* instance */,
                    ppapi::proxy::SerializedVar /* message */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBInstance_LockMouse,
                    PP_Instance /* instance */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBInstance_UnlockMouse,
                    PP_Instance /* instance */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBInstance_ResolveRelativeToDocument,
                           PP_Instance /* instance */,
                           ppapi::proxy::SerializedVar /* relative */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBInstance_DocumentCanRequest,
                           PP_Instance /* instance */,
                           ppapi::proxy::SerializedVar /* relative */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED2_1(PpapiHostMsg_PPBInstance_DocumentCanAccessDocument,
                           PP_Instance /* active */,
                           PP_Instance /* target */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED1_2(PpapiHostMsg_PPBInstance_GetDocumentURL,
                           PP_Instance /* active */,
                           PP_URLComponents_Dev /* components */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBInstance_GetPluginInstanceURL,
                           PP_Instance /* active */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBInstance_GetPluginReferrerURL,
                           PP_Instance /* instance */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_MESSAGE_ROUTED4(PpapiHostMsg_PPBInstance_SetCursor,
                    PP_Instance /* instance */,
                    int32_t /* type */,
                    ppapi::HostResource /* custom_image */,
                    PP_Point /* hot_spot */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBInstance_SetTextInputType,
                    PP_Instance /* instance */,
                    PP_TextInput_Type /* type */)
IPC_MESSAGE_ROUTED3(PpapiHostMsg_PPBInstance_UpdateCaretPosition,
                    PP_Instance /* instance */,
                    PP_Rect /* caret */,
                    PP_Rect /* bounding_box */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBInstance_CancelCompositionText,
                    PP_Instance /* instance */)
IPC_MESSAGE_ROUTED4(PpapiHostMsg_PPBInstance_UpdateSurroundingText,
                    PP_Instance /* instance */,
                    std::string /* text */,
                    uint32_t /* caret */,
                    uint32_t /* anchor */)

// PPB_Var.
IPC_SYNC_MESSAGE_ROUTED1_0(PpapiHostMsg_PPBVar_AddRefObject,
                           int64_t /* object_id */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBVar_ReleaseObject, int64_t /* object_id */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBVar_HasProperty,
                           ppapi::proxy::SerializedVar /* object */,
                           ppapi::proxy::SerializedVar /* property */,
                           ppapi::proxy::SerializedVar /* out_exception */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBVar_HasMethodDeprecated,
                           ppapi::proxy::SerializedVar /* object */,
                           ppapi::proxy::SerializedVar /* method */,
                           ppapi::proxy::SerializedVar /* out_exception */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBVar_GetProperty,
                           ppapi::proxy::SerializedVar /* object */,
                           ppapi::proxy::SerializedVar /* property */,
                           ppapi::proxy::SerializedVar /* out_exception */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBVar_DeleteProperty,
                           ppapi::proxy::SerializedVar /* object */,
                           ppapi::proxy::SerializedVar /* property */,
                           ppapi::proxy::SerializedVar /* out_exception */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED1_2(PpapiHostMsg_PPBVar_EnumerateProperties,
                           ppapi::proxy::SerializedVar /* object */,
                           std::vector<ppapi::proxy::SerializedVar> /* props */,
                           ppapi::proxy::SerializedVar /* out_exception */)
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBVar_SetPropertyDeprecated,
                           ppapi::proxy::SerializedVar /* object */,
                           ppapi::proxy::SerializedVar /* name */,
                           ppapi::proxy::SerializedVar /* value */,
                           ppapi::proxy::SerializedVar /* out_exception */)
IPC_SYNC_MESSAGE_ROUTED3_2(PpapiHostMsg_PPBVar_CallDeprecated,
                           ppapi::proxy::SerializedVar /* object */,
                           ppapi::proxy::SerializedVar /* method_name */,
                           std::vector<ppapi::proxy::SerializedVar> /* args */,
                           ppapi::proxy::SerializedVar /* out_exception */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBVar_Construct,
                           ppapi::proxy::SerializedVar /* object */,
                           std::vector<ppapi::proxy::SerializedVar> /* args */,
                           ppapi::proxy::SerializedVar /* out_exception */,
                           ppapi::proxy::SerializedVar /* result */)
IPC_SYNC_MESSAGE_ROUTED2_2(PpapiHostMsg_PPBVar_IsInstanceOfDeprecated,
                           ppapi::proxy::SerializedVar /* var */,
                           int64_t /* object_class */,
                           int64_t /* object-data */,
                           PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBVar_CreateObjectDeprecated,
                           PP_Instance /* instance */,
                           int64_t /* object_class */,
                           int64_t /* object_data */,
                           ppapi::proxy::SerializedVar /* result */)

#if !BUILDFLAG(IS_NACL)
// PPB_Buffer.
IPC_SYNC_MESSAGE_ROUTED2_2(
    PpapiHostMsg_PPBBuffer_Create,
    PP_Instance /* instance */,
    uint32_t /* size */,
    ppapi::HostResource /* result_resource */,
    ppapi::proxy::SerializedHandle /* result_shm_handle */)

#endif  // !BUILDFLAG(IS_NACL)

// PPB_Testing.
IPC_SYNC_MESSAGE_ROUTED3_1(
    PpapiHostMsg_PPBTesting_ReadImageData,
    ppapi::HostResource /* device_context_2d */,
    ppapi::HostResource /* image */,
    PP_Point /* top_left */,
    PP_Bool /* result */)
IPC_SYNC_MESSAGE_ROUTED1_1(PpapiHostMsg_PPBTesting_GetLiveObjectsForInstance,
                           PP_Instance /* instance */,
                           uint32_t /* result */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBTesting_SimulateInputEvent,
                    PP_Instance /* instance */,
                    ppapi::InputEventData /* input_event */)
IPC_SYNC_MESSAGE_ROUTED1_0(
    PpapiHostMsg_PPBTesting_SetMinimumArrayBufferSizeForShmem,
    uint32_t /* threshold */)

#if !BUILDFLAG(IS_NACL)

// PPB_VideoDecoder_Dev.
// (Messages from plugin to renderer.)
IPC_SYNC_MESSAGE_ROUTED3_1(PpapiHostMsg_PPBVideoDecoder_Create,
                           PP_Instance /* instance */,
                           ppapi::HostResource /* context */,
                           PP_VideoDecoder_Profile /* profile */,
                           ppapi::HostResource /* result */)
IPC_MESSAGE_ROUTED4(PpapiHostMsg_PPBVideoDecoder_Decode,
                    ppapi::HostResource /* video_decoder */,
                    ppapi::HostResource /* bitstream buffer */,
                    int32_t /* bitstream buffer id */,
                    uint32_t /* size of buffer */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBVideoDecoder_AssignPictureBuffers,
                    ppapi::HostResource /* video_decoder */,
                    std::vector<PP_PictureBuffer_Dev> /* picture buffers */)
IPC_MESSAGE_ROUTED2(PpapiHostMsg_PPBVideoDecoder_ReusePictureBuffer,
                    ppapi::HostResource /* video_decoder */,
                    int32_t /* picture buffer id */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBVideoDecoder_Flush,
                    ppapi::HostResource /* video_decoder */)
IPC_MESSAGE_ROUTED1(PpapiHostMsg_PPBVideoDecoder_Reset,
                    ppapi::HostResource /* video_decoder */)
IPC_SYNC_MESSAGE_ROUTED1_0(PpapiHostMsg_PPBVideoDecoder_Destroy,
                           ppapi::HostResource /* video_decoder */)

// PPB_VideoDecoder_Dev.
// (Messages from renderer to plugin to notify it to run callbacks.)
IPC_MESSAGE_ROUTED3(PpapiMsg_PPBVideoDecoder_EndOfBitstreamACK,
                    ppapi::HostResource /* video_decoder */,
                    int32_t /* bitstream buffer id */,
                    int32_t /* PP_CompletionCallback result */)
IPC_MESSAGE_ROUTED2(PpapiMsg_PPBVideoDecoder_FlushACK,
                    ppapi::HostResource /* video_decoder */,
                    int32_t /* PP_CompletionCallback result  */)
IPC_MESSAGE_ROUTED2(PpapiMsg_PPBVideoDecoder_ResetACK,
                    ppapi::HostResource /* video_decoder */,
                    int32_t /* PP_CompletionCallback result */)

// PPP_VideoDecoder_Dev.
IPC_MESSAGE_ROUTED4(PpapiMsg_PPPVideoDecoder_ProvidePictureBuffers,
                    ppapi::HostResource /* video_decoder */,
                    uint32_t /* requested number of buffers */,
                    PP_Size /* dimensions of buffers */,
                    uint32_t /* texture_target */)
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPVideoDecoder_DismissPictureBuffer,
                    ppapi::HostResource /* video_decoder */,
                    int32_t /* picture buffer id */)
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPVideoDecoder_PictureReady,
                    ppapi::HostResource /* video_decoder */,
                    PP_Picture_Dev /* output picture */)
IPC_MESSAGE_ROUTED2(PpapiMsg_PPPVideoDecoder_NotifyError,
                    ppapi::HostResource /* video_decoder */,
                    PP_VideoDecodeError_Dev /* error */)
#endif  // !BUILDFLAG(IS_NACL)

//-----------------------------------------------------------------------------
// Resource call/reply messages.
//
// These are the new-style resource implementations where the resource is only
// implemented in the proxy and "resource messages" are sent between this and a
// host object. Resource messages are a wrapper around some general routing
// information and a separate message of a type defined by the specific resource
// sending/receiving it. The extra paremeters allow the nested message to be
// routed automatically to the correct resource.

// Notification that a resource has been created in the plugin. The nested
// message will be resource-type-specific.
IPC_MESSAGE_CONTROL3(PpapiHostMsg_ResourceCreated,
                     ppapi::proxy::ResourceMessageCallParams /* call_params */,
                     PP_Instance  /* instance */,
                     IPC::Message /* nested_msg */)

// Notification that a resource has been destroyed in the plugin.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_ResourceDestroyed,
                     PP_Resource /* resource */)

// Most resources are created by the plugin, which then sends a ResourceCreated
// message to create a corresponding ResourceHost in the renderer or browser
// host process. However, some resources are first created in the host and
// "pushed" or returned to the plugin.
//
// In this case, the host will create a "pending" ResourceHost object which
// is identified by an ID. The ID is sent to the plugin process and the
// PluginResource object is created. This message is sent from the plugin to
// the host process to connect the PluginResource and the pending ResourceHost
// (at which point, it's no longer pending).
IPC_MESSAGE_CONTROL2(PpapiHostMsg_AttachToPendingHost,
                     PP_Resource /* resource */,
                     int /* pending_host_id */)

// A resource call is a request from the plugin to the host. It may or may not
// require a reply, depending on the params. The nested message will be
// resource-type-specific.
IPC_MESSAGE_CONTROL2(PpapiHostMsg_ResourceCall,
                     ppapi::proxy::ResourceMessageCallParams /* call_params */,
                     IPC::Message /* nested_msg */)
IPC_MESSAGE_CONTROL3(PpapiHostMsg_InProcessResourceCall,
                     int /* routing_id */,
                     ppapi::proxy::ResourceMessageCallParams /* call_params */,
                     IPC::Message /* nested_msg */)

// A resource reply is a response to a ResourceCall from a host to the
// plugin. The resource ID + sequence number in the params will correspond to
// that of the previous ResourceCall.
IPC_MESSAGE_CONTROL2(
    PpapiPluginMsg_ResourceReply,
    ppapi::proxy::ResourceMessageReplyParams /* reply_params */,
    IPC::Message /* nested_msg */)
IPC_MESSAGE_ROUTED2(
    PpapiHostMsg_InProcessResourceReply,
    ppapi::proxy::ResourceMessageReplyParams /* reply_params */,
    IPC::Message /* nested_msg */)

IPC_SYNC_MESSAGE_CONTROL2_2(PpapiHostMsg_ResourceSyncCall,
    ppapi::proxy::ResourceMessageCallParams /* call_params */,
    IPC::Message /* nested_msg */,
    ppapi::proxy::ResourceMessageReplyParams /* reply_params */,
    IPC::Message /* reply_msg */)

// This message is sent from the renderer to the browser when it wants to create
// ResourceHosts in the browser. It contains the process ID of the plugin and
// the instance of the plugin for which to create the resource for. params
// contains the sequence number for the message to track the response.
// The nested messages are ResourceHost creation messages.
IPC_MESSAGE_CONTROL5(
    PpapiHostMsg_CreateResourceHostsFromHost,
    int /* routing_id */,
    int /* child_process_id */,
    ppapi::proxy::ResourceMessageCallParams /* params */,
    PP_Instance /* instance */,
    std::vector<IPC::Message> /* nested_msgs */)

// This message is sent from the browser to the renderer when it has created
// ResourceHosts for the renderer. It contains the sequence number that was sent
// in the request and the IDs of the pending ResourceHosts which were created in
// the browser. These IDs are only useful for the plugin which can attach to the
// ResourceHosts in the browser.
IPC_MESSAGE_ROUTED2(
    PpapiHostMsg_CreateResourceHostsFromHostReply,
    int32_t /* sequence */,
    std::vector<int> /* pending_host_ids */)

//-----------------------------------------------------------------------------
// Messages for resources using call/reply above.

// UMA
IPC_MESSAGE_CONTROL0(PpapiHostMsg_UMA_Create)
IPC_MESSAGE_CONTROL5(PpapiHostMsg_UMA_HistogramCustomTimes,
                     std::string /* name */,
                     int64_t /* sample */,
                     int64_t /* min */,
                     int64_t /* max */,
                     uint32_t /* bucket_count */)
IPC_MESSAGE_CONTROL5(PpapiHostMsg_UMA_HistogramCustomCounts,
                     std::string /* name */,
                     int32_t /* sample */,
                     int32_t /* min */,
                     int32_t /* max */,
                     uint32_t /* bucket_count */)
IPC_MESSAGE_CONTROL3(PpapiHostMsg_UMA_HistogramEnumeration,
                     std::string /* name */,
                     int32_t /* sample */,
                     int32_t /* boundary_value */)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_UMA_IsCrashReportingEnabled)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_UMA_IsCrashReportingEnabledReply)

// File chooser.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_FileChooser_Create)
IPC_MESSAGE_CONTROL4(PpapiHostMsg_FileChooser_Show,
                     bool /* save_as */,
                     bool /* open_multiple */,
                     std::string /* suggested_file_name */,
                     std::vector<std::string> /* accept_mime_types */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_FileChooser_ShowReply,
                     std::vector<ppapi::FileRefCreateInfo> /* files */)

// FileIO
IPC_MESSAGE_CONTROL0(PpapiHostMsg_FileIO_Create)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_FileIO_Open,
                     PP_Resource /* file_ref_resource */,
                     int32_t /* open_flags */)
IPC_MESSAGE_CONTROL2(PpapiPluginMsg_FileIO_OpenReply,
                     PP_Resource /* quota_file_system */,
                     int64_t /* file_size */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_FileIO_Close,
                     ppapi::FileGrowth /* file_growth */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_FileIO_Touch,
                     PP_Time /* last_access_time */,
                     PP_Time /* last_modified_time */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_FileIO_SetLength,
                     int64_t /* length */)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_FileIO_Flush)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_FileIO_RequestOSFileHandle)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_FileIO_RequestOSFileHandleReply)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_FileIO_GeneralReply)

// FileRef
// Creates a FileRef to a path on an external file system. This message may
// only be sent from the renderer.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_FileRef_CreateForRawFS,
                     base::FilePath /* external_path */)

// Creates a FileRef to a path on a file system that uses fileapi.
// This message may be sent from the renderer or the plugin.
IPC_MESSAGE_CONTROL2(PpapiHostMsg_FileRef_CreateForFileAPI,
                     PP_Resource /* file_system */,
                     std::string /* internal_path */)

// Requests that the browser create a directory at the location indicated by
// the FileRef.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_FileRef_MakeDirectory,
                     int32_t /* make_directory_flags */)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_FileRef_MakeDirectoryReply)

// Requests that the browser update the last accessed and last modified times
// at the location indicated by the FileRef.
IPC_MESSAGE_CONTROL2(PpapiHostMsg_FileRef_Touch,
                     PP_Time /* last_accessed */,
                     PP_Time /* last_modified */)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_FileRef_TouchReply)

// Requests that the browser delete a file or directory at the location
// indicated by the FileRef.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_FileRef_Delete)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_FileRef_DeleteReply)

// Requests that the browser rename a file or directory at the location
// indicated by the FileRef.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_FileRef_Rename,
                     PP_Resource /* new_file_ref */)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_FileRef_RenameReply)

// Requests that the browser retrieve metadata information for a file or
// directory at the location indicated by the FileRef.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_FileRef_Query)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_FileRef_QueryReply,
                     PP_FileInfo /* file_info */)

// Requests that the browser retrieve then entries in a directory at the
// location indicated by the FileRef.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_FileRef_ReadDirectoryEntries)

// FileRefCreateInfo does not provide file type information, so two
// corresponding vectors are returned.
IPC_MESSAGE_CONTROL2(PpapiPluginMsg_FileRef_ReadDirectoryEntriesReply,
                     std::vector<ppapi::FileRefCreateInfo> /* files */,
                     std::vector<PP_FileType> /* file_types */)

// Requests that the browser reply with the absolute path to the indicated
// file.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_FileRef_GetAbsolutePath)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_FileRef_GetAbsolutePathReply,
                     std::string /* absolute_path */)

// FileSystem
IPC_MESSAGE_CONTROL1(PpapiHostMsg_FileSystem_Create,
                     PP_FileSystemType /* type */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_FileSystem_Open,
                     int64_t /* expected_size */)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_FileSystem_OpenReply)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_FileSystem_InitIsolatedFileSystem,
                     std::string /* fsid */,
                     PP_IsolatedFileSystemType_Private /* type */)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_FileSystem_InitIsolatedFileSystemReply)
// Passed from renderer to browser. Creates an already-open file system with a
// given |root_url| and |file_system_type|.
IPC_MESSAGE_CONTROL2(PpapiHostMsg_FileSystem_CreateFromRenderer,
                     std::string /* root_url */,
                     PP_FileSystemType /* file_system_type */)
// Nested within a ResourceVar for file systems being passed from the renderer
// to the plugin. Creates an already-open file system resource on the plugin,
// linked to the existing resource host given in the ResourceVar.
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_FileSystem_CreateFromPendingHost,
                     PP_FileSystemType /* file_system_type */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_FileSystem_ReserveQuota,
                     int64_t /* amount */,
                     ppapi::FileGrowthMap /* file_growths */)
IPC_MESSAGE_CONTROL2(PpapiPluginMsg_FileSystem_ReserveQuotaReply,
                     int64_t /* amount */,
                     ppapi::FileSizeMap /* file_sizes */)

// Gamepad.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_Gamepad_Create)

// Requests that the gamepad host send the shared memory handle to the plugin
// process.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_Gamepad_RequestMemory)

// Reply to a RequestMemory call. This supplies the shared memory handle. The
// actual handle is passed in the ReplyParams struct.
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_Gamepad_SendMemory)


// Graphics2D, plugin -> host
IPC_MESSAGE_CONTROL2(PpapiHostMsg_Graphics2D_Create,
                     PP_Size /* size */,
                     PP_Bool /* is_always_opaque */)
IPC_MESSAGE_CONTROL4(PpapiHostMsg_Graphics2D_PaintImageData,
                     ppapi::HostResource /* image_data */,
                     PP_Point /* top_left */,
                     bool /* src_rect_specified */,
                     PP_Rect /* src_rect */)
IPC_MESSAGE_CONTROL3(PpapiHostMsg_Graphics2D_Scroll,
                     bool /* clip_specified */,
                     PP_Rect /* clip */,
                     PP_Point /* amount */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_Graphics2D_ReplaceContents,
                     ppapi::HostResource /* image_data */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_Graphics2D_SetScale,
                     float /* scale */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_Graphics2D_SetLayerTransform,
                     float /* scale */,
                     PP_FloatPoint /* translate */)

// Graphics2D, plugin -> host -> plugin
IPC_MESSAGE_CONTROL0(PpapiHostMsg_Graphics2D_Flush)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_Graphics2D_FlushAck)

IPC_MESSAGE_CONTROL2(PpapiHostMsg_Graphics2D_ReadImageData,
                     PP_Resource /* image */,
                     PP_Point /* top_left */)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_Graphics2D_ReadImageDataAck)

// CameraDevice ----------------------------------------------------------------
IPC_MESSAGE_CONTROL0(PpapiHostMsg_CameraDevice_Create)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_CameraDevice_Close)

IPC_MESSAGE_CONTROL1(PpapiHostMsg_CameraDevice_Open,
                     std::string /* camera_source_id */)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_CameraDevice_OpenReply)

IPC_MESSAGE_CONTROL0(
    PpapiHostMsg_CameraDevice_GetSupportedVideoCaptureFormats)
IPC_MESSAGE_CONTROL1(
    PpapiPluginMsg_CameraDevice_GetSupportedVideoCaptureFormatsReply,
    std::vector<PP_VideoCaptureFormat> /* video_capture_formats */)

// IsolatedFileSystem ----------------------------------------------------------
IPC_MESSAGE_CONTROL0(PpapiHostMsg_IsolatedFileSystem_Create)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_IsolatedFileSystem_BrowserOpen,
                     PP_IsolatedFileSystemType_Private /* type */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_IsolatedFileSystem_BrowserOpenReply,
                     std::string /* fsid */)

// MediaStream -----------------------------------------------------------------
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_MediaStreamAudioTrack_CreateFromPendingHost,
                     std::string /* track_id */)
IPC_MESSAGE_CONTROL1(
    PpapiHostMsg_MediaStreamAudioTrack_Configure,
    ppapi::MediaStreamAudioTrackShared::Attributes /* attributes */)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_MediaStreamAudioTrack_ConfigureReply)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_MediaStreamVideoTrack_CreateFromPendingHost,
                     std::string /* track_id */)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_MediaStreamVideoTrack_Create)
IPC_MESSAGE_CONTROL1(
    PpapiHostMsg_MediaStreamVideoTrack_Configure,
    ppapi::MediaStreamVideoTrackShared::Attributes /* attributes */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_MediaStreamVideoTrack_ConfigureReply,
                     std::string /* track_id */)

// Message for init buffers. It also takes a shared memory handle which is put
// in the outer ResourceReplyMessage.
IPC_MESSAGE_CONTROL3(PpapiPluginMsg_MediaStreamTrack_InitBuffers,
                     int32_t /* number_of_buffers */,
                     int32_t /* buffer_size */,
                     bool /* readonly */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_MediaStreamTrack_EnqueueBuffer,
                     int32_t /* index */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_MediaStreamTrack_EnqueueBuffer,
                     int32_t /* index */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_MediaStreamTrack_EnqueueBuffers,
                     std::vector<int32_t> /* indices */)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_MediaStreamTrack_Close)

// NetworkMonitor.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_NetworkMonitor_Create)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_NetworkMonitor_NetworkList,
                     ppapi::proxy::SerializedNetworkList /* network_list */)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_NetworkMonitor_Forbidden)

// NetworkProxy ----------------------------------------------------------------
IPC_MESSAGE_CONTROL0(PpapiHostMsg_NetworkProxy_Create)

// Query the browser for the proxy server to use for the given URL.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_NetworkProxy_GetProxyForURL,
                     std::string /* url */)

// Reply message for GetProxyForURL containing the proxy server.
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_NetworkProxy_GetProxyForURLReply,
                     std::string /* proxy */)

// Host Resolver ---------------------------------------------------------------
// Creates a PPB_HostResolver resource.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_HostResolver_Create)

// Creates a PPB_HostResolver_Private resource.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_HostResolver_CreatePrivate)

// Resolves the given hostname.
IPC_MESSAGE_CONTROL2(PpapiHostMsg_HostResolver_Resolve,
                     ppapi::HostPortPair /* host_port */,
                     PP_HostResolver_Private_Hint /* hint */)

// This message is a reply to HostResolver_Resolve. On success,
// |canonical_name| contains the canonical name of the host; |net_address_list|
// is a list of network addresses. On failure, both fields are set to empty.
IPC_MESSAGE_CONTROL2(PpapiPluginMsg_HostResolver_ResolveReply,
                     std::string /* canonical_name */,
                     std::vector<PP_NetAddress_Private> /* net_address_list */)

// Printing.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_Printing_Create)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_Printing_GetDefaultPrintSettings)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_Printing_GetDefaultPrintSettingsReply,
                     PP_PrintSettings_Dev /* print_settings */)

// TCP Socket ------------------------------------------------------------------
// Creates a PPB_TCPSocket resource.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_TCPSocket_Create,
                     ppapi::TCPSocketVersion /* version */)

// Creates a PPB_TCPSocket_Private resource.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_TCPSocket_CreatePrivate)

IPC_MESSAGE_CONTROL1(PpapiHostMsg_TCPSocket_Bind,
                     PP_NetAddress_Private /* net_addr */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_TCPSocket_BindReply,
                     PP_NetAddress_Private /* local_addr */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_TCPSocket_Connect,
                     std::string /* host */,
                     uint16_t /* port */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_TCPSocket_ConnectWithNetAddress,
                     PP_NetAddress_Private /* net_addr */)
IPC_MESSAGE_CONTROL2(PpapiPluginMsg_TCPSocket_ConnectReply,
                     PP_NetAddress_Private /* local_addr */,
                     PP_NetAddress_Private /* remote_addr */)
IPC_MESSAGE_CONTROL4(PpapiHostMsg_TCPSocket_SSLHandshake,
                     std::string /* server_name */,
                     uint16_t /* server_port */,
                     std::vector<std::vector<char> > /* trusted_certs */,
                     std::vector<std::vector<char> > /* untrusted_certs */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_TCPSocket_SSLHandshakeReply,
                     ppapi::PPB_X509Certificate_Fields /* certificate_fields */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_TCPSocket_Read,
                     int32_t /* bytes_to_read */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_TCPSocket_ReadReply,
                     std::string /* data */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_TCPSocket_Write,
                     std::string /* data */)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_TCPSocket_WriteReply)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_TCPSocket_Listen,
                     int32_t /* backlog */)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_TCPSocket_ListenReply)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_TCPSocket_Accept)
IPC_MESSAGE_CONTROL3(PpapiPluginMsg_TCPSocket_AcceptReply,
                     int /* pending_host_id*/,
                     PP_NetAddress_Private /* local_addr */,
                     PP_NetAddress_Private /* remote_addr */)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_TCPSocket_Close)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_TCPSocket_SetOption,
                     PP_TCPSocket_Option /* name */,
                     ppapi::SocketOptionData /* value */)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_TCPSocket_SetOptionReply)

// TCP Server Socket -----------------------------------------------------------
// Creates a PPB_TCPServerSocket_Private resource.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_TCPServerSocket_CreatePrivate)

IPC_MESSAGE_CONTROL2(PpapiHostMsg_TCPServerSocket_Listen,
                     PP_NetAddress_Private /* addr */,
                     int32_t /* backlog */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_TCPServerSocket_ListenReply,
                     PP_NetAddress_Private /* local_addr */)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_TCPServerSocket_Accept)
IPC_MESSAGE_CONTROL3(PpapiPluginMsg_TCPServerSocket_AcceptReply,
                     int /* pending_resource_id */,
                     PP_NetAddress_Private /* local_addr */,
                     PP_NetAddress_Private /* remote_addr */)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_TCPServerSocket_StopListening)

// UDP Socket ------------------------------------------------------------------
// Creates a PPB_UDPSocket resource.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_UDPSocket_Create)

// Creates a PPB_UDPSocket_Private resource.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_UDPSocket_CreatePrivate)

IPC_MESSAGE_CONTROL2(PpapiHostMsg_UDPSocket_SetOption,
                     PP_UDPSocket_Option /* name */,
                     ppapi::SocketOptionData /* value */)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_UDPSocket_SetOptionReply)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_UDPSocket_Bind,
                     PP_NetAddress_Private /* net_addr */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_UDPSocket_BindReply,
                     PP_NetAddress_Private /* bound_addr */)
IPC_MESSAGE_CONTROL3(PpapiPluginMsg_UDPSocket_PushRecvResult,
                     int32_t /* result */,
                     std::string /* data */,
                     PP_NetAddress_Private /* remote_addr */)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_UDPSocket_RecvSlotAvailable)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_UDPSocket_SendTo,
                     std::string /* data */,
                     PP_NetAddress_Private /* net_addr */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_UDPSocket_SendToReply,
                     int32_t /* bytes_written */)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_UDPSocket_Close)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_UDPSocket_JoinGroup,
                     PP_NetAddress_Private /* net_addr */)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_UDPSocket_JoinGroupReply)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_UDPSocket_LeaveGroup,
                     PP_NetAddress_Private /* net_addr */)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_UDPSocket_LeaveGroupReply)

// URLLoader ------------------------------------------------------------------

IPC_MESSAGE_CONTROL0(PpapiHostMsg_URLLoader_Create)

// These messages correspond to PPAPI calls and all should get a
// CallbackComplete message.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_URLLoader_Open,
                     ppapi::URLRequestInfoData /* request_data */)

// The plugin can tell the host to defer a load to hold off on sending more
// data because the buffer in the plugin is full. When defers_loading is set to
// false, data streaming will resume.
//
// When auditing redirects (no auto follow) the load will be automatically
// deferred each time we get a redirect. The plugin will reset this to false
// by sending this message when it wants to continue following the redirect.
//
// When streaming data, the host may still send more data after this call (for
// example, it could already be in-flight at the time of this request).
IPC_MESSAGE_CONTROL1(PpapiHostMsg_URLLoader_SetDeferLoading,
                     bool /* defers_loading */)

// Closes the URLLoader. There is no reply.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_URLLoader_Close)

// Requests that cross-site restrictions be ignored. The plugin must have
// the private permission set. Otherwise this message will be ignored by the
// renderer. There is no reply.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_URLLoader_GrantUniversalAccess)

// Push notification that a response is available.
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_URLLoader_ReceivedResponse,
                     ppapi::URLResponseInfoData /* response */)

// Push notification with load data from the renderer. It is a custom generated
// message with the response data (array of bytes stored via WriteData)
// appended.
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_URLLoader_SendData)

// Push notification indicating that all data has been sent, either via
// SendData or by streaming it to a file. Note that since this is a push
// notification, we don't use the result field of the ResourceMessageReply.
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_URLLoader_FinishedLoading,
                     int32_t /* result */)

// Push notification from the renderer to the plugin to tell it about download
// and upload progress. This will only be sent if the plugin has requested
// progress updates, and only the fields requested by the plugin will be
// valid.
IPC_MESSAGE_CONTROL4(PpapiPluginMsg_URLLoader_UpdateProgress,
                     int64_t /* bytes_sent */,
                     int64_t /* total_bytes_to_be_sent */,
                     int64_t /* bytes_received */,
                     int64_t /* total_bytes_to_be_received */)

// Shared memory ---------------------------------------------------------------

// Creates shared memory on the host side, returning a handle to the shared
// memory on the plugin and keeping the memory mapped in on the host.
// We return a "host handle_id" that can be mapped back to the
// handle on the host side by PpapiGlobals::UntrackSharedMemoryHandle().
IPC_SYNC_MESSAGE_CONTROL2_2(PpapiHostMsg_SharedMemory_CreateSharedMemory,
                            PP_Instance /* instance */,
                            uint32_t /* size */,
                            int /* host_handle_id */,
                            ppapi::proxy::SerializedHandle /* plugin_handle */)

// VpnProvider ----------------------------------------------------------------
IPC_MESSAGE_CONTROL0(PpapiHostMsg_VpnProvider_Create)

// VpnProvider plugin -> host -> plugin
IPC_MESSAGE_CONTROL2(PpapiHostMsg_VpnProvider_Bind,
                     std::string /* configuration_id */,
                     std::string /* configuration_name */)
IPC_MESSAGE_CONTROL3(PpapiPluginMsg_VpnProvider_BindReply,
                     uint32_t /* queue_size */,
                     uint32_t /* max_packet_size */,
                     int32_t /* status */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_VpnProvider_SendPacket,
                     uint32_t /* packet_size */,
                     uint32_t /* id */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_VpnProvider_SendPacketReply,
                     uint32_t /* id */)

// VpnProvider host -> plugin
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_VpnProvider_OnUnbind)

// VpnProvider host -> plugin -> host
IPC_MESSAGE_CONTROL2(PpapiPluginMsg_VpnProvider_OnPacketReceived,
                     uint32_t /* packet_size */,
                     uint32_t /* id */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_VpnProvider_OnPacketReceivedReply,
                     uint32_t /* id */)

// WebSocket -------------------------------------------------------------------

IPC_MESSAGE_CONTROL0(PpapiHostMsg_WebSocket_Create)

// Establishes the connection to a server. This message requires
// WebSocket_ConnectReply as a reply message.
IPC_MESSAGE_CONTROL2(PpapiHostMsg_WebSocket_Connect,
                     std::string /* url */,
                     std::vector<std::string> /* protocols */)

// Closes established connection with graceful closing handshake. This message
// requires WebSocket_CloseReply as a reply message.
IPC_MESSAGE_CONTROL2(PpapiHostMsg_WebSocket_Close,
                     int32_t /* code */,
                     std::string /* reason */)

// Sends a text frame to the server. No reply is defined.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_WebSocket_SendText,
                     std::string /* message */)

// Sends a binary frame to the server. No reply is defined.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_WebSocket_SendBinary,
                     std::vector<uint8_t> /* message */)

// Fails the connection. This message invokes RFC6455 defined
// _Fail the WebSocket Connection_ operation. No reply is defined.
IPC_MESSAGE_CONTROL1(PpapiHostMsg_WebSocket_Fail,
                     std::string /* message */)

// This message is a reply to WebSocket_Connect. If the |url| and |protocols|
// are invalid, WebSocket_ConnectReply is issued immediately and it contains
// proper error code in its result. Otherwise, WebSocket_ConnectReply is sent
// with valid |url|, |protocol|, and result PP_OK. |protocol| is not a passed
// |protocols|, but a result of opening handshake negotiation. If the
// connection can not be established successfully, WebSocket_ConnectReply is
// not issued, but WebSocket_ClosedReply is sent instead.
IPC_MESSAGE_CONTROL2(PpapiPluginMsg_WebSocket_ConnectReply,
                     std::string /* url */,
                     std::string /* protocol */)

// This message is a reply to WebSocket_Close. If the operation fails,
// WebSocket_CloseReply is issued immediately and it contains PP_ERROR_FAILED.
// Otherwise, CloseReply will be issued after the closing handshake is
// finished. All arguments will be valid iff the result is PP_OK and it means
// that the client initiated closing handshake is finished gracefully.
IPC_MESSAGE_CONTROL4(PpapiPluginMsg_WebSocket_CloseReply,
                     uint64_t /* buffered_amount */,
                     bool /* was_clean */,
                     uint16_t /* code */,
                     std::string /* reason */)

// Unsolicited reply message to transmit a receiving text frame.
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_WebSocket_ReceiveTextReply,
                     std::string /* message */)

// Unsolicited reply message to transmit a receiving binary frame.
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_WebSocket_ReceiveBinaryReply,
                     std::vector<uint8_t> /* message */)

// Unsolicited reply message to notify a error on underlying network connetion.
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_WebSocket_ErrorReply)

// Unsolicited reply message to update the buffered amount value.
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_WebSocket_BufferedAmountReply,
                     uint64_t /* buffered_amount */)

// Unsolicited reply message to update |state| because of incoming external
// events, e.g., protocol error, or unexpected network closure.
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_WebSocket_StateReply,
                     int32_t /* state */)

// Unsolicited reply message to notify that the connection is closed without
// any WebSocket_Close request. Server initiated closing handshake or
// unexpected network errors will invoke this message.
IPC_MESSAGE_CONTROL4(PpapiPluginMsg_WebSocket_ClosedReply,
                     uint64_t /* buffered_amount */,
                     bool /* was_clean */,
                     uint16_t /* code */,
                     std::string /* reason */)

// VideoDecoder ------------------------------------------------------

IPC_MESSAGE_CONTROL0(PpapiHostMsg_VideoDecoder_Create)
IPC_MESSAGE_CONTROL4(PpapiHostMsg_VideoDecoder_Initialize,
                     ppapi::HostResource /* graphics_context */,
                     PP_VideoProfile /* profile */,
                     PP_HardwareAcceleration /* acceleration */,
                     uint32_t /* min_picture_count */)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_VideoDecoder_InitializeReply)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_VideoDecoder_GetShm,
                     uint32_t /* shm_id */,
                     uint32_t /* shm_size */)
// On success, a shm handle is passed in the ReplyParams struct.
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_VideoDecoder_GetShmReply,
                     uint32_t /* shm_size */)
IPC_MESSAGE_CONTROL3(PpapiHostMsg_VideoDecoder_Decode,
                     uint32_t /* shm_id */,
                     uint32_t /* size */,
                     int32_t /* decode_id */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_VideoDecoder_DecodeReply,
                     uint32_t /* shm_id */)
IPC_MESSAGE_CONTROL4(PpapiPluginMsg_VideoDecoder_SharedImageReady,
                     int32_t /* decode_id */,
                     gpu::Mailbox /* mailbox */,
                     PP_Size /* size */,
                     PP_Rect /* visible_rect */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_VideoDecoder_RecycleSharedImage,
                     gpu::Mailbox /* mailbox */)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_VideoDecoder_Flush)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_VideoDecoder_FlushReply)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_VideoDecoder_Reset)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_VideoDecoder_ResetReply)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_VideoDecoder_NotifyError,
                     int32_t /* error */)

// VideoEncoder ------------------------------------------------------

IPC_MESSAGE_CONTROL0(PpapiHostMsg_VideoEncoder_Create)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_VideoEncoder_GetSupportedProfiles)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_VideoEncoder_GetSupportedProfilesReply,
                     std::vector<PP_VideoProfileDescription> /* results */)
IPC_MESSAGE_CONTROL5(PpapiHostMsg_VideoEncoder_Initialize,
                     PP_VideoFrame_Format /* input_format */,
                     PP_Size /* input_visible_size */,
                     PP_VideoProfile /* output_profile */,
                     uint32_t /* initial_bitrate */,
                     PP_HardwareAcceleration /* acceleration */)
IPC_MESSAGE_CONTROL2(PpapiPluginMsg_VideoEncoder_InitializeReply,
                     uint32_t /* input_frame_count */,
                     PP_Size /* input_coded_size */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_VideoEncoder_BitstreamBuffers,
                     uint32_t /* buffer_length */)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_VideoEncoder_GetVideoFrames)
IPC_MESSAGE_CONTROL3(PpapiPluginMsg_VideoEncoder_GetVideoFramesReply,
                     uint32_t /* frame_count */,
                     uint32_t /* frame_length */,
                     PP_Size /* frame_size */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_VideoEncoder_Encode,
                     uint32_t /* frame_id */,
                     bool /* force_keyframe */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_VideoEncoder_EncodeReply,
                     uint32_t /* frame_id */)
IPC_MESSAGE_CONTROL3(PpapiPluginMsg_VideoEncoder_BitstreamBufferReady,
                     uint32_t /* buffer_id */,
                     uint32_t /* buffer_size */,
                     bool /* key_frame */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_VideoEncoder_RecycleBitstreamBuffer,
                     uint32_t /* buffer_id */)
IPC_MESSAGE_CONTROL2(PpapiHostMsg_VideoEncoder_RequestEncodingParametersChange,
                     uint32_t /* bitrate */,
                     uint32_t /* framerate */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_VideoEncoder_NotifyError,
                     int32_t /* error */)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_VideoEncoder_Close)

#if !BUILDFLAG(IS_NACL)

// Audio input.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_AudioInput_Create)
IPC_MESSAGE_CONTROL3(PpapiHostMsg_AudioInput_Open,
                     std::string /* device_id */,
                     PP_AudioSampleRate /* sample_rate */,
                     uint32_t /* sample_frame_count */)
// Reply to an Open call. This supplies a socket handle and a shared memory
// handle. Both handles are passed in the ReplyParams struct.
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_AudioInput_OpenReply)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_AudioInput_StartOrStop, bool /* capture */)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_AudioInput_Close)

// Audio output.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_AudioOutput_Create)
IPC_MESSAGE_CONTROL3(PpapiHostMsg_AudioOutput_Open,
                     std::string /* device_id */,
                     PP_AudioSampleRate /* sample_rate */,
                     uint32_t /* sample_frame_count */)
// Reply to an Open call. This supplies a socket handle and a shared memory
// handle. Both handles are passed in the ReplyParams struct.
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_AudioOutput_OpenReply)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_AudioOutput_StartOrStop, bool /* playback */)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_AudioOutput_Close)

// BrowserFont -----------------------------------------------------------------

IPC_MESSAGE_CONTROL0(PpapiHostMsg_BrowserFontSingleton_Create)

// Requests that the browser reply with the list of font families via
// PpapiPluginMsg_BrowserFontSingleton_GetFontFamiliesReply.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_BrowserFontSingleton_GetFontFamilies)

// Reply to PpapiHostMsg_BrowserFontSingleton_GetFontFamilies with the font
// family list. The |families| result is encoded by separating each family name
// by a null character.
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_BrowserFontSingleton_GetFontFamiliesReply,
                     std::string /* families */)

// DeviceEnumeration -----------------------------------------------------------
// Device enumeration messages used by audio input and video capture.
IPC_MESSAGE_CONTROL0(PpapiHostMsg_DeviceEnumeration_EnumerateDevices)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_DeviceEnumeration_EnumerateDevicesReply,
                     std::vector<ppapi::DeviceRefData> /* devices */)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_DeviceEnumeration_MonitorDeviceChange,
                     uint32_t /* callback_id */)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_DeviceEnumeration_StopMonitoringDeviceChange)
IPC_MESSAGE_CONTROL2(PpapiPluginMsg_DeviceEnumeration_NotifyDeviceChange,
                     uint32_t /* callback_id */,
                     std::vector<ppapi::DeviceRefData> /* devices */)

// VideoCapture ----------------------------------------------------------------

// VideoCapture_Dev, plugin -> host
IPC_MESSAGE_CONTROL0(PpapiHostMsg_VideoCapture_Create)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_VideoCapture_StartCapture)
IPC_MESSAGE_CONTROL1(PpapiHostMsg_VideoCapture_ReuseBuffer,
                     uint32_t /* buffer */)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_VideoCapture_StopCapture)
IPC_MESSAGE_CONTROL0(PpapiHostMsg_VideoCapture_Close)

// VideoCapture_Dev, plugin -> host -> plugin
IPC_MESSAGE_CONTROL3(PpapiHostMsg_VideoCapture_Open,
                     std::string /* device_id */,
                     PP_VideoCaptureDeviceInfo_Dev /* requested_info */,
                     uint32_t /* buffer_count */)
IPC_MESSAGE_CONTROL0(PpapiPluginMsg_VideoCapture_OpenReply)

// VideoCapture_Dev, host -> plugin
IPC_MESSAGE_CONTROL3(PpapiPluginMsg_VideoCapture_OnDeviceInfo,
                     PP_VideoCaptureDeviceInfo_Dev /* info */,
                     std::vector<ppapi::HostResource> /* buffers */,
                     uint32_t /* buffer_size */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_VideoCapture_OnStatus,
                     uint32_t /* status */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_VideoCapture_OnError,
                     uint32_t /* error */)
IPC_MESSAGE_CONTROL1(PpapiPluginMsg_VideoCapture_OnBufferReady,
                     uint32_t /* buffer */)

#endif  // !BUILDFLAG(IS_NACL)

#endif  // PPAPI_PROXY_PPAPI_MESSAGES_H_
