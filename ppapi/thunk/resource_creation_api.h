// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_RESOURCE_CREATION_API_H_
#define PPAPI_THUNK_RESOURCE_CREATION_API_H_

#include <stdint.h>

#include "base/memory/unsafe_shared_memory_region.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/c/dev/ppb_file_chooser_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_file_system.h"
#include "ppapi/c/ppb_graphics_3d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/c/ppb_network_monitor.h"
#include "ppapi/c/ppb_websocket.h"
#include "ppapi/c/private/pp_private_font_charset.h"
#include "ppapi/shared_impl/api_id.h"

// Windows defines 'PostMessage', so we have to undef it.
#ifdef PostMessage
#undef PostMessage
#endif

struct PP_BrowserFont_Trusted_Description;
struct PP_NetAddress_IPv4;
struct PP_NetAddress_IPv6;
struct PP_NetAddress_Private;
struct PP_Size;

namespace gpu {
struct Capabilities;
struct GLCapabilities;
}

namespace ppapi {

struct FileRefCreateInfo;
struct Graphics3DContextAttribs;

namespace thunk {

// A functional API for creating resource types. Separating out the creation
// functions here allows us to implement most resources as a pure "resource
// API", meaning all calls are routed on a per-resource-object basis. The
// creation functions are not per-object (since there's no object during
// creation) so need functional routing based on the instance ID.
class ResourceCreationAPI {
 public:
  virtual ~ResourceCreationAPI() {}

  virtual PP_Resource CreateFileIO(PP_Instance instance) = 0;
  virtual PP_Resource CreateFileRef(
      PP_Instance instance,
      const FileRefCreateInfo& serialized) = 0;
  virtual PP_Resource CreateFileSystem(PP_Instance instance,
                                       PP_FileSystemType type) = 0;
  virtual PP_Resource CreateIMEInputEvent(PP_Instance instance,
                                          PP_InputEvent_Type type,
                                          PP_TimeTicks time_stamp,
                                          struct PP_Var text,
                                          uint32_t segment_number,
                                          const uint32_t* segment_offsets,
                                          int32_t target_segment,
                                          uint32_t selection_start,
                                          uint32_t selection_end) = 0;
  virtual PP_Resource CreateKeyboardInputEvent_1_0(
      PP_Instance instance,
      PP_InputEvent_Type type,
      PP_TimeTicks time_stamp,
      uint32_t modifiers,
      uint32_t key_code,
      struct PP_Var character_text) = 0;
  virtual PP_Resource CreateKeyboardInputEvent_1_2(
      PP_Instance instance,
      PP_InputEvent_Type type,
      PP_TimeTicks time_stamp,
      uint32_t modifiers,
      uint32_t key_code,
      struct PP_Var character_text,
      struct PP_Var code) = 0;
  virtual PP_Resource CreateMouseInputEvent(
      PP_Instance instance,
      PP_InputEvent_Type type,
      PP_TimeTicks time_stamp,
      uint32_t modifiers,
      PP_InputEvent_MouseButton mouse_button,
      const PP_Point* mouse_position,
      int32_t click_count,
      const PP_Point* mouse_movement) = 0;
  virtual PP_Resource CreateTouchInputEvent(
      PP_Instance instance,
      PP_InputEvent_Type type,
      PP_TimeTicks time_stamp,
      uint32_t modifiers) = 0;
  virtual PP_Resource CreateURLLoader(PP_Instance instance) = 0;
  virtual PP_Resource CreateURLRequestInfo(
      PP_Instance instance) = 0;

  virtual PP_Resource CreateWheelInputEvent(
      PP_Instance instance,
      PP_TimeTicks time_stamp,
      uint32_t modifiers,
      const PP_FloatPoint* wheel_delta,
      const PP_FloatPoint* wheel_ticks,
      PP_Bool scroll_by_page) = 0;

  virtual PP_Resource CreateAudio1_0(PP_Instance instance,
                                     PP_Resource config_id,
                                     PPB_Audio_Callback_1_0 audio_callback,
                                     void* user_data) = 0;
  virtual PP_Resource CreateAudio(PP_Instance instance,
                                  PP_Resource config_id,
                                  PPB_Audio_Callback audio_callback,
                                  void* user_data) = 0;
  virtual PP_Resource CreateAudioTrusted(PP_Instance instance) = 0;
  virtual PP_Resource CreateAudioConfig(PP_Instance instance,
                                        PP_AudioSampleRate sample_rate,
                                        uint32_t sample_frame_count) = 0;
  virtual PP_Resource CreateCameraDevicePrivate(PP_Instance instance) = 0;
  virtual PP_Resource CreateFileChooser(PP_Instance instance,
                                        PP_FileChooserMode_Dev mode,
                                        const PP_Var& accept_types) = 0;
  virtual PP_Resource CreateGraphics2D(PP_Instance instance,
                                       const PP_Size* size,
                                       PP_Bool is_always_opaque) = 0;
  virtual PP_Resource CreateGraphics3D(PP_Instance instance,
                                       PP_Resource share_context,
                                       const int32_t* attrib_list) = 0;
  virtual PP_Resource CreateGraphics3DRaw(
      PP_Instance instance,
      PP_Resource share_context,
      const Graphics3DContextAttribs& attrib_helper,
      gpu::Capabilities* capabilities,
      gpu::GLCapabilities* gl_capabilities,
      const base::UnsafeSharedMemoryRegion** shared_state,
      gpu::CommandBufferId* command_buffer_id) = 0;
  virtual PP_Resource CreateHostResolver(PP_Instance instance) = 0;
  virtual PP_Resource CreateHostResolverPrivate(PP_Instance instance) = 0;
  virtual PP_Resource CreateImageData(PP_Instance instance,
                                      PP_ImageDataFormat format,
                                      const PP_Size* size,
                                      PP_Bool init_to_zero) = 0;
  virtual PP_Resource CreateImageDataSimple(PP_Instance instance,
                                            PP_ImageDataFormat format,
                                            const PP_Size* size,
                                            PP_Bool init_to_zero) = 0;
  virtual PP_Resource CreateMediaStreamVideoTrack(PP_Instance instance) = 0;
  virtual PP_Resource CreateNetAddressFromIPv4Address(
      PP_Instance instance,
      const PP_NetAddress_IPv4* ipv4_addr) = 0;
  virtual PP_Resource CreateNetAddressFromIPv6Address(
      PP_Instance instance,
      const PP_NetAddress_IPv6* ipv6_addr) = 0;
  virtual PP_Resource CreateNetAddressFromNetAddressPrivate(
      PP_Instance instance,
      const PP_NetAddress_Private& private_addr) = 0;
  virtual PP_Resource CreateNetworkMonitor(PP_Instance instance) = 0;
  virtual PP_Resource CreatePrinting(PP_Instance instance) = 0;
  virtual PP_Resource CreateTCPServerSocketPrivate(PP_Instance instance) = 0;
  virtual PP_Resource CreateTCPSocket1_0(PP_Instance instace) = 0;
  virtual PP_Resource CreateTCPSocket(PP_Instance instance) = 0;
  virtual PP_Resource CreateTCPSocketPrivate(PP_Instance instace) = 0;
  virtual PP_Resource CreateUDPSocket(PP_Instance instace) = 0;
  virtual PP_Resource CreateUDPSocketPrivate(PP_Instance instace) = 0;
  virtual PP_Resource CreateVideoDecoder(PP_Instance instance) = 0;
  virtual PP_Resource CreateVideoEncoder(PP_Instance instance) = 0;
  virtual PP_Resource CreateVpnProvider(PP_Instance instance) = 0;
  virtual PP_Resource CreateWebSocket(PP_Instance instance) = 0;
#if !BUILDFLAG(IS_NACL)
  virtual PP_Resource CreateX509CertificatePrivate(PP_Instance instance) = 0;
  virtual PP_Resource CreateAudioInput(PP_Instance instance) = 0;
  virtual PP_Resource CreateAudioOutput(PP_Instance instance) = 0;
  virtual PP_Resource CreateBrowserFont(
      PP_Instance instance,
      const PP_BrowserFont_Trusted_Description* description) = 0;
  virtual PP_Resource CreateBuffer(PP_Instance instance, uint32_t size) = 0;
  virtual PP_Resource CreateVideoCapture(PP_Instance instance) = 0;
  virtual PP_Resource CreateVideoDecoderDev(
      PP_Instance instance,
      PP_Resource context3d_id,
      PP_VideoDecoder_Profile profile) = 0;
#endif  // !BUILDFLAG(IS_NACL)

  static const ApiID kApiID = API_ID_RESOURCE_CREATION;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_RESOURCE_CREATION_API_H_
