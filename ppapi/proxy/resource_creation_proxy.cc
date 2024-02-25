// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/resource_creation_proxy.h"

#include "build/build_config.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/proxy/audio_input_resource.h"
#include "ppapi/proxy/audio_output_resource.h"
#include "ppapi/proxy/camera_device_resource.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/file_chooser_resource.h"
#include "ppapi/proxy/file_io_resource.h"
#include "ppapi/proxy/file_ref_resource.h"
#include "ppapi/proxy/file_system_resource.h"
#include "ppapi/proxy/graphics_2d_resource.h"
#include "ppapi/proxy/host_resolver_private_resource.h"
#include "ppapi/proxy/host_resolver_resource.h"
#include "ppapi/proxy/media_stream_video_track_resource.h"
#include "ppapi/proxy/net_address_resource.h"
#include "ppapi/proxy/network_monitor_resource.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_audio_proxy.h"
#include "ppapi/proxy/ppb_buffer_proxy.h"
#include "ppapi/proxy/ppb_graphics_3d_proxy.h"
#include "ppapi/proxy/ppb_image_data_proxy.h"
#include "ppapi/proxy/ppb_video_decoder_proxy.h"
#include "ppapi/proxy/ppb_x509_certificate_private_proxy.h"
#include "ppapi/proxy/printing_resource.h"
#include "ppapi/proxy/tcp_server_socket_private_resource.h"
#include "ppapi/proxy/tcp_socket_private_resource.h"
#include "ppapi/proxy/tcp_socket_resource.h"
#include "ppapi/proxy/udp_socket_private_resource.h"
#include "ppapi/proxy/udp_socket_resource.h"
#include "ppapi/proxy/url_loader_resource.h"
#include "ppapi/proxy/url_request_info_resource.h"
#include "ppapi/proxy/url_response_info_resource.h"
#include "ppapi/proxy/video_capture_resource.h"
#include "ppapi/proxy/video_decoder_resource.h"
#include "ppapi/proxy/video_encoder_resource.h"
#include "ppapi/proxy/vpn_provider_resource.h"
#include "ppapi/proxy/websocket_resource.h"
#include "ppapi/shared_impl/api_id.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/ppb_audio_config_shared.h"
#include "ppapi/shared_impl/ppb_audio_shared.h"
#include "ppapi/shared_impl/ppb_input_event_shared.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_image_data_api.h"

using ppapi::thunk::ResourceCreationAPI;

namespace ppapi {
namespace proxy {

ResourceCreationProxy::ResourceCreationProxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

ResourceCreationProxy::~ResourceCreationProxy() {
}

// static
InterfaceProxy* ResourceCreationProxy::Create(Dispatcher* dispatcher) {
  return new ResourceCreationProxy(dispatcher);
}

PP_Resource ResourceCreationProxy::CreateFileIO(PP_Instance instance) {
  return (new FileIOResource(GetConnection(), instance))->GetReference();
}

PP_Resource ResourceCreationProxy::CreateFileRef(
    PP_Instance instance,
    const FileRefCreateInfo& create_info) {
  return FileRefResource::CreateFileRef(GetConnection(), instance, create_info);
}

PP_Resource ResourceCreationProxy::CreateFileSystem(
    PP_Instance instance,
    PP_FileSystemType type) {
  return (new FileSystemResource(GetConnection(), instance,
                                 type))->GetReference();
}

PP_Resource ResourceCreationProxy::CreateIMEInputEvent(
    PP_Instance instance,
    PP_InputEvent_Type type,
    PP_TimeTicks time_stamp,
    struct PP_Var text,
    uint32_t segment_number,
    const uint32_t* segment_offsets,
    int32_t target_segment,
    uint32_t selection_start,
    uint32_t selection_end) {
  return PPB_InputEvent_Shared::CreateIMEInputEvent(
      OBJECT_IS_PROXY, instance, type, time_stamp, text, segment_number,
      segment_offsets, target_segment, selection_start, selection_end);
}

PP_Resource ResourceCreationProxy::CreateKeyboardInputEvent_1_0(
    PP_Instance instance,
    PP_InputEvent_Type type,
    PP_TimeTicks time_stamp,
    uint32_t modifiers,
    uint32_t key_code,
    struct PP_Var character_text) {
  PP_Var code = StringVar::StringToPPVar("");
  return PPB_InputEvent_Shared::CreateKeyboardInputEvent(
      OBJECT_IS_PROXY, instance, type, time_stamp, modifiers, key_code,
      character_text, code);
}

PP_Resource ResourceCreationProxy::CreateKeyboardInputEvent_1_2(
    PP_Instance instance,
    PP_InputEvent_Type type,
    PP_TimeTicks time_stamp,
    uint32_t modifiers,
    uint32_t key_code,
    struct PP_Var character_text,
    struct PP_Var code) {
  return PPB_InputEvent_Shared::CreateKeyboardInputEvent(
      OBJECT_IS_PROXY, instance, type, time_stamp, modifiers, key_code,
      character_text, code);
}

PP_Resource ResourceCreationProxy::CreateMouseInputEvent(
    PP_Instance instance,
    PP_InputEvent_Type type,
    PP_TimeTicks time_stamp,
    uint32_t modifiers,
    PP_InputEvent_MouseButton mouse_button,
    const PP_Point* mouse_position,
    int32_t click_count,
    const PP_Point* mouse_movement) {
  return PPB_InputEvent_Shared::CreateMouseInputEvent(
      OBJECT_IS_PROXY, instance, type, time_stamp, modifiers,
      mouse_button, mouse_position, click_count, mouse_movement);
}

PP_Resource ResourceCreationProxy::CreateTouchInputEvent(
    PP_Instance instance,
    PP_InputEvent_Type type,
    PP_TimeTicks time_stamp,
    uint32_t modifiers) {
  return PPB_InputEvent_Shared::CreateTouchInputEvent(
      OBJECT_IS_PROXY, instance, type, time_stamp, modifiers);
}

PP_Resource ResourceCreationProxy::CreateURLLoader(PP_Instance instance) {
    return (new URLLoaderResource(GetConnection(), instance))->GetReference();
}

PP_Resource ResourceCreationProxy::CreateURLRequestInfo(
    PP_Instance instance) {
  return (new URLRequestInfoResource(
      GetConnection(), instance, URLRequestInfoData()))->GetReference();
}

PP_Resource ResourceCreationProxy::CreateWheelInputEvent(
    PP_Instance instance,
    PP_TimeTicks time_stamp,
    uint32_t modifiers,
    const PP_FloatPoint* wheel_delta,
    const PP_FloatPoint* wheel_ticks,
    PP_Bool scroll_by_page) {
  return PPB_InputEvent_Shared::CreateWheelInputEvent(
      OBJECT_IS_PROXY, instance, time_stamp, modifiers,
      wheel_delta, wheel_ticks, scroll_by_page);
}

PP_Resource ResourceCreationProxy::CreateAudio1_0(
    PP_Instance instance,
    PP_Resource config_id,
    PPB_Audio_Callback_1_0 audio_callback,
    void* user_data) {
  return PPB_Audio_Proxy::CreateProxyResource(
      instance, config_id, AudioCallbackCombined(audio_callback), user_data);
}

PP_Resource ResourceCreationProxy::CreateAudio(
    PP_Instance instance,
    PP_Resource config_id,
    PPB_Audio_Callback audio_callback,
    void* user_data) {
  return PPB_Audio_Proxy::CreateProxyResource(
      instance, config_id, AudioCallbackCombined(audio_callback), user_data);
}

PP_Resource ResourceCreationProxy::CreateAudioTrusted(PP_Instance instance) {
  // Proxied plugins can't create trusted audio devices.
  return 0;
}

PP_Resource ResourceCreationProxy::CreateAudioConfig(
    PP_Instance instance,
    PP_AudioSampleRate sample_rate,
    uint32_t sample_frame_count) {
  return PPB_AudioConfig_Shared::Create(
      OBJECT_IS_PROXY, instance, sample_rate, sample_frame_count);
}

PP_Resource ResourceCreationProxy::CreateCameraDevicePrivate(
    PP_Instance instance) {
  return (new CameraDeviceResource(GetConnection(), instance))->GetReference();
}

PP_Resource ResourceCreationProxy::CreateFileChooser(
    PP_Instance instance,
    PP_FileChooserMode_Dev mode,
    const PP_Var& accept_types) {
  scoped_refptr<StringVar> string_var = StringVar::FromPPVar(accept_types);
  std::string str = string_var.get() ? string_var->value() : std::string();
  return (new FileChooserResource(GetConnection(), instance, mode, str.c_str()))
      ->GetReference();
}

PP_Resource ResourceCreationProxy::CreateGraphics2D(PP_Instance instance,
                                                    const PP_Size* size,
                                                    PP_Bool is_always_opaque) {
  return (new Graphics2DResource(GetConnection(), instance, *size,
                                 is_always_opaque))->GetReference();
}

PP_Resource ResourceCreationProxy::CreateGraphics3D(
    PP_Instance instance,
    PP_Resource share_context,
    const int32_t* attrib_list) {
  return PPB_Graphics3D_Proxy::CreateProxyResource(
      instance, share_context, attrib_list);
}

PP_Resource ResourceCreationProxy::CreateGraphics3DRaw(
    PP_Instance instance,
    PP_Resource share_context,
    const Graphics3DContextAttribs& context_attribs,
    gpu::Capabilities* capabilities,
    gpu::GLCapabilities* gl_capabilities,
    const base::UnsafeSharedMemoryRegion** shared_state,
    gpu::CommandBufferId* command_buffer_id) {
  // Not proxied. The raw creation function is used only in the implementation
  // of the proxy on the host side.
  return 0;
}

PP_Resource ResourceCreationProxy::CreateHostResolver(PP_Instance instance) {
  return (new HostResolverResource(GetConnection(), instance))->GetReference();
}

PP_Resource ResourceCreationProxy::CreateHostResolverPrivate(
    PP_Instance instance) {
  return (new HostResolverPrivateResource(
      GetConnection(), instance))->GetReference();
}

PP_Resource ResourceCreationProxy::CreateImageData(
    PP_Instance instance,
    PP_ImageDataFormat format,
    const PP_Size* size,
    PP_Bool init_to_zero) {
  // On the plugin side, we create PlatformImageData resources for trusted
  // plugins and SimpleImageData resources for untrusted ones.
  PPB_ImageData_Shared::ImageDataType type =
#if !BUILDFLAG(IS_NACL)
      PPB_ImageData_Shared::PLATFORM;
#else
      PPB_ImageData_Shared::SIMPLE;
#endif
  return PPB_ImageData_Proxy::CreateProxyResource(
      instance, type,
      format, *size, init_to_zero);
}

PP_Resource ResourceCreationProxy::CreateImageDataSimple(
    PP_Instance instance,
    PP_ImageDataFormat format,
    const PP_Size* size,
    PP_Bool init_to_zero) {
  return PPB_ImageData_Proxy::CreateProxyResource(
      instance,
      PPB_ImageData_Shared::SIMPLE,
      format, *size, init_to_zero);
}

PP_Resource ResourceCreationProxy::CreateMediaStreamVideoTrack(
    PP_Instance instance) {
  return (new MediaStreamVideoTrackResource(GetConnection(),
                                            instance))->GetReference();
}

PP_Resource ResourceCreationProxy::CreateNetAddressFromIPv4Address(
    PP_Instance instance,
    const PP_NetAddress_IPv4* ipv4_addr) {
  return (new NetAddressResource(GetConnection(), instance,
                                 *ipv4_addr))->GetReference();
}

PP_Resource ResourceCreationProxy::CreateNetAddressFromIPv6Address(
    PP_Instance instance,
    const PP_NetAddress_IPv6* ipv6_addr) {
  return (new NetAddressResource(GetConnection(), instance,
                                 *ipv6_addr))->GetReference();
}

PP_Resource ResourceCreationProxy::CreateNetAddressFromNetAddressPrivate(
    PP_Instance instance,
    const PP_NetAddress_Private& private_addr) {
  return (new NetAddressResource(GetConnection(), instance,
                                 private_addr))->GetReference();
}

PP_Resource ResourceCreationProxy::CreateNetworkMonitor(
    PP_Instance instance) {
  return (new NetworkMonitorResource(GetConnection(), instance))->
      GetReference();
}

PP_Resource ResourceCreationProxy::CreatePrinting(PP_Instance instance) {
  return (new PrintingResource(GetConnection(), instance))->GetReference();
}

PP_Resource ResourceCreationProxy::CreateTCPServerSocketPrivate(
    PP_Instance instance) {
  return (new TCPServerSocketPrivateResource(GetConnection(), instance))->
      GetReference();
}

PP_Resource ResourceCreationProxy::CreateTCPSocket1_0(
    PP_Instance instance) {
  return (new TCPSocketResource(GetConnection(), instance,
                                TCP_SOCKET_VERSION_1_0))->GetReference();
}

PP_Resource ResourceCreationProxy::CreateTCPSocket(
    PP_Instance instance) {
  return (new TCPSocketResource(
      GetConnection(), instance, TCP_SOCKET_VERSION_1_1_OR_ABOVE))->
          GetReference();
}

PP_Resource ResourceCreationProxy::CreateTCPSocketPrivate(
    PP_Instance instance) {
  return (new TCPSocketPrivateResource(GetConnection(), instance))->
      GetReference();
}

PP_Resource ResourceCreationProxy::CreateUDPSocket(PP_Instance instance) {
  return (new UDPSocketResource(GetConnection(), instance))->GetReference();
}

PP_Resource ResourceCreationProxy::CreateUDPSocketPrivate(
    PP_Instance instance) {
  return (new UDPSocketPrivateResource(
      GetConnection(), instance))->GetReference();
}

PP_Resource ResourceCreationProxy::CreateVideoDecoder(PP_Instance instance) {
  return (new VideoDecoderResource(GetConnection(), instance))->GetReference();
}

PP_Resource ResourceCreationProxy::CreateVideoEncoder(PP_Instance instance) {
  return (new VideoEncoderResource(GetConnection(), instance))->GetReference();
}

PP_Resource ResourceCreationProxy::CreateVpnProvider(PP_Instance instance) {
  return (new VpnProviderResource(GetConnection(), instance))->GetReference();
}

PP_Resource ResourceCreationProxy::CreateWebSocket(PP_Instance instance) {
  return (new WebSocketResource(GetConnection(), instance))->GetReference();
}

#if !BUILDFLAG(IS_NACL)
PP_Resource ResourceCreationProxy::CreateX509CertificatePrivate(
    PP_Instance instance) {
  return PPB_X509Certificate_Private_Proxy::CreateProxyResource(instance);
}

PP_Resource ResourceCreationProxy::CreateAudioInput(
    PP_Instance instance) {
  return (new AudioInputResource(GetConnection(), instance))->GetReference();
}

PP_Resource ResourceCreationProxy::CreateAudioOutput(PP_Instance instance) {
  return (new AudioOutputResource(GetConnection(), instance))->GetReference();
}

PP_Resource ResourceCreationProxy::CreateBrowserFont(
    PP_Instance instance,
    const PP_BrowserFont_Trusted_Description* description) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;
  return PluginGlobals::Get()->CreateBrowserFont(
      GetConnection(), instance, *description, dispatcher->preferences());
}

PP_Resource ResourceCreationProxy::CreateBuffer(PP_Instance instance,
                                                uint32_t size) {
  return PPB_Buffer_Proxy::CreateProxyResource(instance, size);
}

PP_Resource ResourceCreationProxy::CreateVideoCapture(PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;
  return (new VideoCaptureResource(GetConnection(), instance, dispatcher))
      ->GetReference();
}

PP_Resource ResourceCreationProxy::CreateVideoDecoderDev(
    PP_Instance instance,
    PP_Resource context3d_id,
    PP_VideoDecoder_Profile profile) {
  return PPB_VideoDecoder_Proxy::CreateProxyResource(
      instance, context3d_id, profile);
}

#endif  // !BUILDFLAG(IS_NACL)

bool ResourceCreationProxy::Send(IPC::Message* msg) {
  return dispatcher()->Send(msg);
}

bool ResourceCreationProxy::OnMessageReceived(const IPC::Message& msg) {
  return false;
}

Connection ResourceCreationProxy::GetConnection() {
  return Connection(PluginGlobals::Get()->GetBrowserSender(),
                    static_cast<PluginDispatcher*>(dispatcher())->sender());
}

}  // namespace proxy
}  // namespace ppapi
