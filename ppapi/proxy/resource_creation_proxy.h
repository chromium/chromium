// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_RESOURCE_CREATION_PROXY_H_
#define PPAPI_PROXY_RESOURCE_CREATION_PROXY_H_

#include <stdint.h>

#include "build/build_config.h"
#include "ipc/ipc_channel.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/serialized_structs.h"
#include "ppapi/thunk/resource_creation_api.h"

struct PP_Size;

namespace ppapi {

namespace proxy {

class Connection;
class Dispatcher;

class ResourceCreationProxy : public InterfaceProxy,
                              public thunk::ResourceCreationAPI {
 public:
  explicit ResourceCreationProxy(Dispatcher* dispatcher);

  ResourceCreationProxy(const ResourceCreationProxy&) = delete;
  ResourceCreationProxy& operator=(const ResourceCreationProxy&) = delete;

  ~ResourceCreationProxy() override;

  // Factory function used for registration (normal code can just use the
  // constructor).
  static InterfaceProxy* Create(Dispatcher* dispatcher);

  // ResourceCreationAPI (called in plugin).
  PP_Resource CreateFileIO(PP_Instance instance) override;
  PP_Resource CreateFileRef(
      PP_Instance instance,
      const FileRefCreateInfo& create_info) override;
  PP_Resource CreateFileSystem(PP_Instance instance,
                               PP_FileSystemType type) override;
  PP_Resource CreateIMEInputEvent(PP_Instance instance,
                                  PP_InputEvent_Type type,
                                  PP_TimeTicks time_stamp,
                                  struct PP_Var text,
                                  uint32_t segment_number,
                                  const uint32_t* segment_offsets,
                                  int32_t target_segment,
                                  uint32_t selection_start,
                                  uint32_t selection_end) override;
  PP_Resource CreateKeyboardInputEvent_1_0(
      PP_Instance instance,
      PP_InputEvent_Type type,
      PP_TimeTicks time_stamp,
      uint32_t modifiers,
      uint32_t key_code,
      PP_Var character_text) override;
  PP_Resource CreateKeyboardInputEvent_1_2(
      PP_Instance instance,
      PP_InputEvent_Type type,
      PP_TimeTicks time_stamp,
      uint32_t modifiers,
      uint32_t key_code,
      PP_Var character_text,
      PP_Var code) override;
  PP_Resource CreateMouseInputEvent(
      PP_Instance instance,
      PP_InputEvent_Type type,
      PP_TimeTicks time_stamp,
      uint32_t modifiers,
      PP_InputEvent_MouseButton mouse_button,
      const PP_Point* mouse_position,
      int32_t click_count,
      const PP_Point* mouse_movement) override;
  PP_Resource CreateTouchInputEvent(
      PP_Instance instance,
      PP_InputEvent_Type type,
      PP_TimeTicks time_stamp,
      uint32_t modifiers) override;
  PP_Resource CreateURLLoader(PP_Instance instance) override;
  PP_Resource CreateURLRequestInfo(PP_Instance instance) override;
  PP_Resource CreateWheelInputEvent(
      PP_Instance instance,
      PP_TimeTicks time_stamp,
      uint32_t modifiers,
      const PP_FloatPoint* wheel_delta,
      const PP_FloatPoint* wheel_ticks,
      PP_Bool scroll_by_page) override;
  PP_Resource CreateAudio1_0(PP_Instance instance,
                             PP_Resource config_id,
                             PPB_Audio_Callback_1_0 audio_callback,
                             void* user_data) override;
  PP_Resource CreateAudio(PP_Instance instance,
                          PP_Resource config_id,
                          PPB_Audio_Callback audio_callback,
                          void* user_data) override;
  PP_Resource CreateAudioTrusted(PP_Instance instance) override;
  PP_Resource CreateAudioConfig(PP_Instance instance,
                                PP_AudioSampleRate sample_rate,
                                uint32_t sample_frame_count) override;
  PP_Resource CreateCameraDevicePrivate(PP_Instance instance) override;
  PP_Resource CreateFileChooser(PP_Instance instance,
                                PP_FileChooserMode_Dev mode,
                                const PP_Var& accept_types) override;
  PP_Resource CreateGraphics2D(PP_Instance pp_instance,
                               const PP_Size* size,
                               PP_Bool is_always_opaque) override;
  PP_Resource CreateGraphics3D(PP_Instance instance,
                               PP_Resource share_context,
                               const int32_t* attrib_list) override;
  PP_Resource CreateGraphics3DRaw(
      PP_Instance instance,
      PP_Resource share_context,
      const Graphics3DContextAttribs& context_attribs,
      gpu::Capabilities* capabilities,
      gpu::GLCapabilities* gl_capabilities,
      const base::UnsafeSharedMemoryRegion** shared_state,
      gpu::CommandBufferId* command_buffer_id) override;
  PP_Resource CreateHostResolver(PP_Instance instance) override;
  PP_Resource CreateHostResolverPrivate(PP_Instance instance) override;
  PP_Resource CreateImageData(PP_Instance instance,
                              PP_ImageDataFormat format,
                              const PP_Size* size,
                              PP_Bool init_to_zero) override;
  PP_Resource CreateImageDataSimple(PP_Instance instance,
                                    PP_ImageDataFormat format,
                                    const PP_Size* size,
                                    PP_Bool init_to_zero) override;
  PP_Resource CreateMediaStreamVideoTrack(PP_Instance instance) override;
  PP_Resource CreateNetAddressFromIPv4Address(
      PP_Instance instance,
      const PP_NetAddress_IPv4* ipv4_addr) override;
  PP_Resource CreateNetAddressFromIPv6Address(
      PP_Instance instance,
      const PP_NetAddress_IPv6* ipv6_addr) override;
  PP_Resource CreateNetAddressFromNetAddressPrivate(
      PP_Instance instance,
      const PP_NetAddress_Private& private_addr) override;
  PP_Resource CreateNetworkMonitor(PP_Instance instance) override;
  PP_Resource CreatePrinting(PP_Instance) override;
  PP_Resource CreateTCPServerSocketPrivate(PP_Instance instance) override;
  PP_Resource CreateTCPSocket1_0(PP_Instance instance) override;
  PP_Resource CreateTCPSocket(PP_Instance instance) override;
  PP_Resource CreateTCPSocketPrivate(PP_Instance instance) override;
  PP_Resource CreateUDPSocket(PP_Instance instance) override;
  PP_Resource CreateUDPSocketPrivate(PP_Instance instance) override;
  PP_Resource CreateVideoDecoder(PP_Instance instance) override;
  PP_Resource CreateVideoEncoder(PP_Instance instance) override;
  PP_Resource CreateVpnProvider(PP_Instance instance) override;
  PP_Resource CreateWebSocket(PP_Instance instance) override;
#if !BUILDFLAG(IS_NACL)
  PP_Resource CreateX509CertificatePrivate(PP_Instance instance) override;
  PP_Resource CreateAudioInput(PP_Instance instance) override;
  PP_Resource CreateAudioOutput(PP_Instance instance) override;
  PP_Resource CreateBrowserFont(
      PP_Instance instance,
      const PP_BrowserFont_Trusted_Description* description) override;
  PP_Resource CreateBuffer(PP_Instance instance, uint32_t size) override;
  PP_Resource CreateVideoCapture(PP_Instance instance) override;
  PP_Resource CreateVideoDecoderDev(
      PP_Instance instance,
      PP_Resource context3d_id,
      PP_VideoDecoder_Profile profile) override;
#endif  // !BUILDFLAG(IS_NACL)

  bool Send(IPC::Message* msg) override;
  bool OnMessageReceived(const IPC::Message& msg) override;

 private:
  Connection GetConnection();
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_RESOURCE_CREATION_PROXY_H_
