/* Copyright 2020 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* NOTE: this is auto-generated from IDL */
#include "ppapi/native_client/src/untrusted/pnacl_irt_shim/pnacl_shim.h"

#include "ppapi/c/ppb.h"
#include "ppapi/c/dev/ppb_audio_input_dev.h"
#include "ppapi/c/dev/ppb_audio_output_dev.h"
#include "ppapi/c/dev/ppb_device_ref_dev.h"
#include "ppapi/c/dev/ppb_file_chooser_dev.h"
#include "ppapi/c/dev/ppb_ime_input_event_dev.h"
#include "ppapi/c/dev/ppb_printing_dev.h"
#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/c/dev/ppb_video_capture_dev.h"
#include "ppapi/c/dev/ppb_video_decoder_dev.h"
#include "ppapi/c/ppb_console.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/ppb_file_ref.h"
#include "ppapi/c/ppb_file_system.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_graphics_3d.h"
#include "ppapi/c/ppb_host_resolver.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/c/ppb_media_stream_audio_track.h"
#include "ppapi/c/ppb_media_stream_video_track.h"
#include "ppapi/c/ppb_message_loop.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_mouse_lock.h"
#include "ppapi/c/ppb_net_address.h"
#include "ppapi/c/ppb_network_list.h"
#include "ppapi/c/ppb_network_monitor.h"
#include "ppapi/c/ppb_network_proxy.h"
#include "ppapi/c/ppb_tcp_socket.h"
#include "ppapi/c/ppb_text_input_controller.h"
#include "ppapi/c/ppb_udp_socket.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/c/ppb_url_response_info.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppb_var_array.h"
#include "ppapi/c/ppb_var_array_buffer.h"
#include "ppapi/c/ppb_var_dictionary.h"
#include "ppapi/c/ppb_video_decoder.h"
#include "ppapi/c/ppb_video_encoder.h"
#include "ppapi/c/ppb_vpn_provider.h"
#include "ppapi/c/ppb_websocket.h"
#include "ppapi/c/ppp_messaging.h"
#include "ppapi/c/private/ppb_camera_device_private.h"
#include "ppapi/c/private/ppb_display_color_profile_private.h"
#include "ppapi/c/private/ppb_ext_crx_file_system_private.h"
#include "ppapi/c/private/ppb_file_io_private.h"
#include "ppapi/c/private/ppb_file_ref_private.h"
#include "ppapi/c/private/ppb_host_resolver_private.h"
#include "ppapi/c/private/ppb_instance_private.h"
#include "ppapi/c/private/ppb_isolated_file_system_private.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/c/private/ppb_tcp_server_socket_private.h"
#include "ppapi/c/private/ppb_tcp_socket_private.h"
#include "ppapi/c/private/ppb_testing_private.h"
#include "ppapi/c/private/ppb_udp_socket_private.h"
#include "ppapi/c/private/ppb_uma_private.h"
#include "ppapi/c/private/ppb_x509_certificate_private.h"
#include "ppapi/c/private/ppp_instance_private.h"

/* Use local strcmp to avoid dependency on libc. */
static int mystrcmp(const char* s1, const char *s2) {
  while (1) {
    if (*s1 == 0) break;
    if (*s2 == 0) break;
    if (*s1 != *s2) break;
    ++s1;
    ++s2;
  }
  return (int)(*s1) - (int)(*s2);
}

/* BEGIN Declarations for all Wrapper Infos */

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Console_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Core_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileIO_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileIO_1_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileRef_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileRef_1_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileRef_1_2;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileSystem_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Graphics2D_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Graphics2D_1_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Graphics2D_1_2;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Graphics3D_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_HostResolver_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_MouseInputEvent_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_MouseInputEvent_1_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_WheelInputEvent_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_KeyboardInputEvent_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_KeyboardInputEvent_1_2;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TouchInputEvent_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TouchInputEvent_1_4;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_IMEInputEvent_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_MediaStreamAudioTrack_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_MediaStreamVideoTrack_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_MediaStreamVideoTrack_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_MessageLoop_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Messaging_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Messaging_1_2;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_MouseLock_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetAddress_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetworkList_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetworkMonitor_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetworkProxy_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPSocket_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPSocket_1_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPSocket_1_2;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TextInputController_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_UDPSocket_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_UDPSocket_1_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_UDPSocket_1_2;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_URLLoader_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_URLRequestInfo_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_URLResponseInfo_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Var_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Var_1_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Var_1_2;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VarArray_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VarArrayBuffer_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VarDictionary_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VideoDecoder_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VideoDecoder_0_2;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VideoDecoder_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VideoDecoder_1_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VideoEncoder_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VideoEncoder_0_2;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VpnProvider_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_WebSocket_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPP_Messaging_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_3;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_4;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_AudioOutput_Dev_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_DeviceRef_Dev_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileChooser_Dev_0_5;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileChooser_Dev_0_6;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_2;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Printing_Dev_0_7;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_6;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_7;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_3;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VideoDecoder_Dev_0_16;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_CameraDevice_Private_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_DisplayColorProfile_Private_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Ext_CrxFileSystem_Private_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileIO_Private_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileRefPrivate_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_HostResolver_Private_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Instance_Private_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_IsolatedFileSystem_Private_0_2;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetAddress_Private_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetAddress_Private_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_2;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_3;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Testing_Private_1_0;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_2;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_3;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_4;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_UMA_Private_0_3;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_X509Certificate_Private_0_1;
static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPP_Instance_Private_0_1;
/* END Declarations for all Wrapper Infos. */

/* Not generating wrapper methods for PPB_Audio_1_0 */

/* Not generating wrapper methods for PPB_Audio_1_1 */

/* Not generating wrapper methods for PPB_AudioBuffer_0_1 */

/* Not generating wrapper methods for PPB_AudioConfig_1_0 */

/* Not generating wrapper methods for PPB_AudioConfig_1_1 */

/* Begin wrapper methods for PPB_Console_1_0 */

static void Pnacl_M25_PPB_Console_Log(PP_Instance instance, PP_LogLevel level, struct PP_Var* value) {
  const struct PPB_Console_1_0 *iface = Pnacl_WrapperInfo_PPB_Console_1_0.real_iface;
  iface->Log(instance, level, *value);
}

static void Pnacl_M25_PPB_Console_LogWithSource(PP_Instance instance, PP_LogLevel level, struct PP_Var* source, struct PP_Var* value) {
  const struct PPB_Console_1_0 *iface = Pnacl_WrapperInfo_PPB_Console_1_0.real_iface;
  iface->LogWithSource(instance, level, *source, *value);
}

/* End wrapper methods for PPB_Console_1_0 */

/* Begin wrapper methods for PPB_Core_1_0 */

static void Pnacl_M14_PPB_Core_AddRefResource(PP_Resource resource) {
  const struct PPB_Core_1_0 *iface = Pnacl_WrapperInfo_PPB_Core_1_0.real_iface;
  iface->AddRefResource(resource);
}

static void Pnacl_M14_PPB_Core_ReleaseResource(PP_Resource resource) {
  const struct PPB_Core_1_0 *iface = Pnacl_WrapperInfo_PPB_Core_1_0.real_iface;
  iface->ReleaseResource(resource);
}

static PP_Time Pnacl_M14_PPB_Core_GetTime(void) {
  const struct PPB_Core_1_0 *iface = Pnacl_WrapperInfo_PPB_Core_1_0.real_iface;
  return iface->GetTime();
}

static PP_TimeTicks Pnacl_M14_PPB_Core_GetTimeTicks(void) {
  const struct PPB_Core_1_0 *iface = Pnacl_WrapperInfo_PPB_Core_1_0.real_iface;
  return iface->GetTimeTicks();
}

static void Pnacl_M14_PPB_Core_CallOnMainThread(int32_t delay_in_milliseconds, struct PP_CompletionCallback* callback, int32_t result) {
  const struct PPB_Core_1_0 *iface = Pnacl_WrapperInfo_PPB_Core_1_0.real_iface;
  iface->CallOnMainThread(delay_in_milliseconds, *callback, result);
}

static PP_Bool Pnacl_M14_PPB_Core_IsMainThread(void) {
  const struct PPB_Core_1_0 *iface = Pnacl_WrapperInfo_PPB_Core_1_0.real_iface;
  return iface->IsMainThread();
}

/* End wrapper methods for PPB_Core_1_0 */

/* Begin wrapper methods for PPB_FileIO_1_0 */

static PP_Resource Pnacl_M14_PPB_FileIO_Create(PP_Instance instance) {
  const struct PPB_FileIO_1_0 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_0.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M14_PPB_FileIO_IsFileIO(PP_Resource resource) {
  const struct PPB_FileIO_1_0 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_0.real_iface;
  return iface->IsFileIO(resource);
}

static int32_t Pnacl_M14_PPB_FileIO_Open(PP_Resource file_io, PP_Resource file_ref, int32_t open_flags, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_0 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_0.real_iface;
  return iface->Open(file_io, file_ref, open_flags, *callback);
}

static int32_t Pnacl_M14_PPB_FileIO_Query(PP_Resource file_io, struct PP_FileInfo* info, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_0 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_0.real_iface;
  return iface->Query(file_io, info, *callback);
}

static int32_t Pnacl_M14_PPB_FileIO_Touch(PP_Resource file_io, PP_Time last_access_time, PP_Time last_modified_time, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_0 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_0.real_iface;
  return iface->Touch(file_io, last_access_time, last_modified_time, *callback);
}

static int32_t Pnacl_M14_PPB_FileIO_Read(PP_Resource file_io, int64_t offset, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_0 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_0.real_iface;
  return iface->Read(file_io, offset, buffer, bytes_to_read, *callback);
}

static int32_t Pnacl_M14_PPB_FileIO_Write(PP_Resource file_io, int64_t offset, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_0 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_0.real_iface;
  return iface->Write(file_io, offset, buffer, bytes_to_write, *callback);
}

static int32_t Pnacl_M14_PPB_FileIO_SetLength(PP_Resource file_io, int64_t length, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_0 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_0.real_iface;
  return iface->SetLength(file_io, length, *callback);
}

static int32_t Pnacl_M14_PPB_FileIO_Flush(PP_Resource file_io, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_0 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_0.real_iface;
  return iface->Flush(file_io, *callback);
}

static void Pnacl_M14_PPB_FileIO_Close(PP_Resource file_io) {
  const struct PPB_FileIO_1_0 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_0.real_iface;
  iface->Close(file_io);
}

/* End wrapper methods for PPB_FileIO_1_0 */

/* Begin wrapper methods for PPB_FileIO_1_1 */

static PP_Resource Pnacl_M25_PPB_FileIO_Create(PP_Instance instance) {
  const struct PPB_FileIO_1_1 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_1.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M25_PPB_FileIO_IsFileIO(PP_Resource resource) {
  const struct PPB_FileIO_1_1 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_1.real_iface;
  return iface->IsFileIO(resource);
}

static int32_t Pnacl_M25_PPB_FileIO_Open(PP_Resource file_io, PP_Resource file_ref, int32_t open_flags, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_1 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_1.real_iface;
  return iface->Open(file_io, file_ref, open_flags, *callback);
}

static int32_t Pnacl_M25_PPB_FileIO_Query(PP_Resource file_io, struct PP_FileInfo* info, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_1 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_1.real_iface;
  return iface->Query(file_io, info, *callback);
}

static int32_t Pnacl_M25_PPB_FileIO_Touch(PP_Resource file_io, PP_Time last_access_time, PP_Time last_modified_time, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_1 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_1.real_iface;
  return iface->Touch(file_io, last_access_time, last_modified_time, *callback);
}

static int32_t Pnacl_M25_PPB_FileIO_Read(PP_Resource file_io, int64_t offset, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_1 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_1.real_iface;
  return iface->Read(file_io, offset, buffer, bytes_to_read, *callback);
}

static int32_t Pnacl_M25_PPB_FileIO_Write(PP_Resource file_io, int64_t offset, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_1 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_1.real_iface;
  return iface->Write(file_io, offset, buffer, bytes_to_write, *callback);
}

static int32_t Pnacl_M25_PPB_FileIO_SetLength(PP_Resource file_io, int64_t length, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_1 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_1.real_iface;
  return iface->SetLength(file_io, length, *callback);
}

static int32_t Pnacl_M25_PPB_FileIO_Flush(PP_Resource file_io, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_1 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_1.real_iface;
  return iface->Flush(file_io, *callback);
}

static void Pnacl_M25_PPB_FileIO_Close(PP_Resource file_io) {
  const struct PPB_FileIO_1_1 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_1.real_iface;
  iface->Close(file_io);
}

static int32_t Pnacl_M25_PPB_FileIO_ReadToArray(PP_Resource file_io, int64_t offset, int32_t max_read_length, struct PP_ArrayOutput* output, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_1_1 *iface = Pnacl_WrapperInfo_PPB_FileIO_1_1.real_iface;
  return iface->ReadToArray(file_io, offset, max_read_length, output, *callback);
}

/* End wrapper methods for PPB_FileIO_1_1 */

/* Begin wrapper methods for PPB_FileRef_1_0 */

static PP_Resource Pnacl_M14_PPB_FileRef_Create(PP_Resource file_system, const char* path) {
  const struct PPB_FileRef_1_0 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_0.real_iface;
  return iface->Create(file_system, path);
}

static PP_Bool Pnacl_M14_PPB_FileRef_IsFileRef(PP_Resource resource) {
  const struct PPB_FileRef_1_0 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_0.real_iface;
  return iface->IsFileRef(resource);
}

static PP_FileSystemType Pnacl_M14_PPB_FileRef_GetFileSystemType(PP_Resource file_ref) {
  const struct PPB_FileRef_1_0 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_0.real_iface;
  return iface->GetFileSystemType(file_ref);
}

static void Pnacl_M14_PPB_FileRef_GetName(struct PP_Var* _struct_result, PP_Resource file_ref) {
  const struct PPB_FileRef_1_0 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_0.real_iface;
  *_struct_result = iface->GetName(file_ref);
}

static void Pnacl_M14_PPB_FileRef_GetPath(struct PP_Var* _struct_result, PP_Resource file_ref) {
  const struct PPB_FileRef_1_0 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_0.real_iface;
  *_struct_result = iface->GetPath(file_ref);
}

static PP_Resource Pnacl_M14_PPB_FileRef_GetParent(PP_Resource file_ref) {
  const struct PPB_FileRef_1_0 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_0.real_iface;
  return iface->GetParent(file_ref);
}

static int32_t Pnacl_M14_PPB_FileRef_MakeDirectory(PP_Resource directory_ref, PP_Bool make_ancestors, struct PP_CompletionCallback* callback) {
  const struct PPB_FileRef_1_0 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_0.real_iface;
  return iface->MakeDirectory(directory_ref, make_ancestors, *callback);
}

static int32_t Pnacl_M14_PPB_FileRef_Touch(PP_Resource file_ref, PP_Time last_access_time, PP_Time last_modified_time, struct PP_CompletionCallback* callback) {
  const struct PPB_FileRef_1_0 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_0.real_iface;
  return iface->Touch(file_ref, last_access_time, last_modified_time, *callback);
}

static int32_t Pnacl_M14_PPB_FileRef_Delete(PP_Resource file_ref, struct PP_CompletionCallback* callback) {
  const struct PPB_FileRef_1_0 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_0.real_iface;
  return iface->Delete(file_ref, *callback);
}

static int32_t Pnacl_M14_PPB_FileRef_Rename(PP_Resource file_ref, PP_Resource new_file_ref, struct PP_CompletionCallback* callback) {
  const struct PPB_FileRef_1_0 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_0.real_iface;
  return iface->Rename(file_ref, new_file_ref, *callback);
}

/* End wrapper methods for PPB_FileRef_1_0 */

/* Begin wrapper methods for PPB_FileRef_1_1 */

static PP_Resource Pnacl_M28_PPB_FileRef_Create(PP_Resource file_system, const char* path) {
  const struct PPB_FileRef_1_1 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_1.real_iface;
  return iface->Create(file_system, path);
}

static PP_Bool Pnacl_M28_PPB_FileRef_IsFileRef(PP_Resource resource) {
  const struct PPB_FileRef_1_1 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_1.real_iface;
  return iface->IsFileRef(resource);
}

static PP_FileSystemType Pnacl_M28_PPB_FileRef_GetFileSystemType(PP_Resource file_ref) {
  const struct PPB_FileRef_1_1 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_1.real_iface;
  return iface->GetFileSystemType(file_ref);
}

static void Pnacl_M28_PPB_FileRef_GetName(struct PP_Var* _struct_result, PP_Resource file_ref) {
  const struct PPB_FileRef_1_1 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_1.real_iface;
  *_struct_result = iface->GetName(file_ref);
}

static void Pnacl_M28_PPB_FileRef_GetPath(struct PP_Var* _struct_result, PP_Resource file_ref) {
  const struct PPB_FileRef_1_1 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_1.real_iface;
  *_struct_result = iface->GetPath(file_ref);
}

static PP_Resource Pnacl_M28_PPB_FileRef_GetParent(PP_Resource file_ref) {
  const struct PPB_FileRef_1_1 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_1.real_iface;
  return iface->GetParent(file_ref);
}

static int32_t Pnacl_M28_PPB_FileRef_MakeDirectory(PP_Resource directory_ref, PP_Bool make_ancestors, struct PP_CompletionCallback* callback) {
  const struct PPB_FileRef_1_1 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_1.real_iface;
  return iface->MakeDirectory(directory_ref, make_ancestors, *callback);
}

static int32_t Pnacl_M28_PPB_FileRef_Touch(PP_Resource file_ref, PP_Time last_access_time, PP_Time last_modified_time, struct PP_CompletionCallback* callback) {
  const struct PPB_FileRef_1_1 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_1.real_iface;
  return iface->Touch(file_ref, last_access_time, last_modified_time, *callback);
}

static int32_t Pnacl_M28_PPB_FileRef_Delete(PP_Resource file_ref, struct PP_CompletionCallback* callback) {
  const struct PPB_FileRef_1_1 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_1.real_iface;
  return iface->Delete(file_ref, *callback);
}

static int32_t Pnacl_M28_PPB_FileRef_Rename(PP_Resource file_ref, PP_Resource new_file_ref, struct PP_CompletionCallback* callback) {
  const struct PPB_FileRef_1_1 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_1.real_iface;
  return iface->Rename(file_ref, new_file_ref, *callback);
}

static int32_t Pnacl_M28_PPB_FileRef_Query(PP_Resource file_ref, struct PP_FileInfo* info, struct PP_CompletionCallback* callback) {
  const struct PPB_FileRef_1_1 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_1.real_iface;
  return iface->Query(file_ref, info, *callback);
}

static int32_t Pnacl_M28_PPB_FileRef_ReadDirectoryEntries(PP_Resource file_ref, struct PP_ArrayOutput* output, struct PP_CompletionCallback* callback) {
  const struct PPB_FileRef_1_1 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_1.real_iface;
  return iface->ReadDirectoryEntries(file_ref, *output, *callback);
}

/* End wrapper methods for PPB_FileRef_1_1 */

/* Begin wrapper methods for PPB_FileRef_1_2 */

static PP_Resource Pnacl_M34_PPB_FileRef_Create(PP_Resource file_system, const char* path) {
  const struct PPB_FileRef_1_2 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_2.real_iface;
  return iface->Create(file_system, path);
}

static PP_Bool Pnacl_M34_PPB_FileRef_IsFileRef(PP_Resource resource) {
  const struct PPB_FileRef_1_2 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_2.real_iface;
  return iface->IsFileRef(resource);
}

static PP_FileSystemType Pnacl_M34_PPB_FileRef_GetFileSystemType(PP_Resource file_ref) {
  const struct PPB_FileRef_1_2 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_2.real_iface;
  return iface->GetFileSystemType(file_ref);
}

static void Pnacl_M34_PPB_FileRef_GetName(struct PP_Var* _struct_result, PP_Resource file_ref) {
  const struct PPB_FileRef_1_2 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_2.real_iface;
  *_struct_result = iface->GetName(file_ref);
}

static void Pnacl_M34_PPB_FileRef_GetPath(struct PP_Var* _struct_result, PP_Resource file_ref) {
  const struct PPB_FileRef_1_2 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_2.real_iface;
  *_struct_result = iface->GetPath(file_ref);
}

static PP_Resource Pnacl_M34_PPB_FileRef_GetParent(PP_Resource file_ref) {
  const struct PPB_FileRef_1_2 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_2.real_iface;
  return iface->GetParent(file_ref);
}

static int32_t Pnacl_M34_PPB_FileRef_MakeDirectory(PP_Resource directory_ref, int32_t make_directory_flags, struct PP_CompletionCallback* callback) {
  const struct PPB_FileRef_1_2 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_2.real_iface;
  return iface->MakeDirectory(directory_ref, make_directory_flags, *callback);
}

static int32_t Pnacl_M34_PPB_FileRef_Touch(PP_Resource file_ref, PP_Time last_access_time, PP_Time last_modified_time, struct PP_CompletionCallback* callback) {
  const struct PPB_FileRef_1_2 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_2.real_iface;
  return iface->Touch(file_ref, last_access_time, last_modified_time, *callback);
}

static int32_t Pnacl_M34_PPB_FileRef_Delete(PP_Resource file_ref, struct PP_CompletionCallback* callback) {
  const struct PPB_FileRef_1_2 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_2.real_iface;
  return iface->Delete(file_ref, *callback);
}

static int32_t Pnacl_M34_PPB_FileRef_Rename(PP_Resource file_ref, PP_Resource new_file_ref, struct PP_CompletionCallback* callback) {
  const struct PPB_FileRef_1_2 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_2.real_iface;
  return iface->Rename(file_ref, new_file_ref, *callback);
}

static int32_t Pnacl_M34_PPB_FileRef_Query(PP_Resource file_ref, struct PP_FileInfo* info, struct PP_CompletionCallback* callback) {
  const struct PPB_FileRef_1_2 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_2.real_iface;
  return iface->Query(file_ref, info, *callback);
}

static int32_t Pnacl_M34_PPB_FileRef_ReadDirectoryEntries(PP_Resource file_ref, struct PP_ArrayOutput* output, struct PP_CompletionCallback* callback) {
  const struct PPB_FileRef_1_2 *iface = Pnacl_WrapperInfo_PPB_FileRef_1_2.real_iface;
  return iface->ReadDirectoryEntries(file_ref, *output, *callback);
}

/* End wrapper methods for PPB_FileRef_1_2 */

/* Begin wrapper methods for PPB_FileSystem_1_0 */

static PP_Resource Pnacl_M14_PPB_FileSystem_Create(PP_Instance instance, PP_FileSystemType type) {
  const struct PPB_FileSystem_1_0 *iface = Pnacl_WrapperInfo_PPB_FileSystem_1_0.real_iface;
  return iface->Create(instance, type);
}

static PP_Bool Pnacl_M14_PPB_FileSystem_IsFileSystem(PP_Resource resource) {
  const struct PPB_FileSystem_1_0 *iface = Pnacl_WrapperInfo_PPB_FileSystem_1_0.real_iface;
  return iface->IsFileSystem(resource);
}

static int32_t Pnacl_M14_PPB_FileSystem_Open(PP_Resource file_system, int64_t expected_size, struct PP_CompletionCallback* callback) {
  const struct PPB_FileSystem_1_0 *iface = Pnacl_WrapperInfo_PPB_FileSystem_1_0.real_iface;
  return iface->Open(file_system, expected_size, *callback);
}

static PP_FileSystemType Pnacl_M14_PPB_FileSystem_GetType(PP_Resource file_system) {
  const struct PPB_FileSystem_1_0 *iface = Pnacl_WrapperInfo_PPB_FileSystem_1_0.real_iface;
  return iface->GetType(file_system);
}

/* End wrapper methods for PPB_FileSystem_1_0 */

/* Not generating wrapper methods for PPB_Fullscreen_1_0 */

/* Not generating wrapper methods for PPB_Gamepad_1_0 */

/* Begin wrapper methods for PPB_Graphics2D_1_0 */

static PP_Resource Pnacl_M14_PPB_Graphics2D_Create(PP_Instance instance, const struct PP_Size* size, PP_Bool is_always_opaque) {
  const struct PPB_Graphics2D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_0.real_iface;
  return iface->Create(instance, size, is_always_opaque);
}

static PP_Bool Pnacl_M14_PPB_Graphics2D_IsGraphics2D(PP_Resource resource) {
  const struct PPB_Graphics2D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_0.real_iface;
  return iface->IsGraphics2D(resource);
}

static PP_Bool Pnacl_M14_PPB_Graphics2D_Describe(PP_Resource graphics_2d, struct PP_Size* size, PP_Bool* is_always_opaque) {
  const struct PPB_Graphics2D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_0.real_iface;
  return iface->Describe(graphics_2d, size, is_always_opaque);
}

static void Pnacl_M14_PPB_Graphics2D_PaintImageData(PP_Resource graphics_2d, PP_Resource image_data, const struct PP_Point* top_left, const struct PP_Rect* src_rect) {
  const struct PPB_Graphics2D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_0.real_iface;
  iface->PaintImageData(graphics_2d, image_data, top_left, src_rect);
}

static void Pnacl_M14_PPB_Graphics2D_Scroll(PP_Resource graphics_2d, const struct PP_Rect* clip_rect, const struct PP_Point* amount) {
  const struct PPB_Graphics2D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_0.real_iface;
  iface->Scroll(graphics_2d, clip_rect, amount);
}

static void Pnacl_M14_PPB_Graphics2D_ReplaceContents(PP_Resource graphics_2d, PP_Resource image_data) {
  const struct PPB_Graphics2D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_0.real_iface;
  iface->ReplaceContents(graphics_2d, image_data);
}

static int32_t Pnacl_M14_PPB_Graphics2D_Flush(PP_Resource graphics_2d, struct PP_CompletionCallback* callback) {
  const struct PPB_Graphics2D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_0.real_iface;
  return iface->Flush(graphics_2d, *callback);
}

/* End wrapper methods for PPB_Graphics2D_1_0 */

/* Begin wrapper methods for PPB_Graphics2D_1_1 */

static PP_Resource Pnacl_M27_PPB_Graphics2D_Create(PP_Instance instance, const struct PP_Size* size, PP_Bool is_always_opaque) {
  const struct PPB_Graphics2D_1_1 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_1.real_iface;
  return iface->Create(instance, size, is_always_opaque);
}

static PP_Bool Pnacl_M27_PPB_Graphics2D_IsGraphics2D(PP_Resource resource) {
  const struct PPB_Graphics2D_1_1 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_1.real_iface;
  return iface->IsGraphics2D(resource);
}

static PP_Bool Pnacl_M27_PPB_Graphics2D_Describe(PP_Resource graphics_2d, struct PP_Size* size, PP_Bool* is_always_opaque) {
  const struct PPB_Graphics2D_1_1 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_1.real_iface;
  return iface->Describe(graphics_2d, size, is_always_opaque);
}

static void Pnacl_M27_PPB_Graphics2D_PaintImageData(PP_Resource graphics_2d, PP_Resource image_data, const struct PP_Point* top_left, const struct PP_Rect* src_rect) {
  const struct PPB_Graphics2D_1_1 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_1.real_iface;
  iface->PaintImageData(graphics_2d, image_data, top_left, src_rect);
}

static void Pnacl_M27_PPB_Graphics2D_Scroll(PP_Resource graphics_2d, const struct PP_Rect* clip_rect, const struct PP_Point* amount) {
  const struct PPB_Graphics2D_1_1 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_1.real_iface;
  iface->Scroll(graphics_2d, clip_rect, amount);
}

static void Pnacl_M27_PPB_Graphics2D_ReplaceContents(PP_Resource graphics_2d, PP_Resource image_data) {
  const struct PPB_Graphics2D_1_1 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_1.real_iface;
  iface->ReplaceContents(graphics_2d, image_data);
}

static int32_t Pnacl_M27_PPB_Graphics2D_Flush(PP_Resource graphics_2d, struct PP_CompletionCallback* callback) {
  const struct PPB_Graphics2D_1_1 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_1.real_iface;
  return iface->Flush(graphics_2d, *callback);
}

static PP_Bool Pnacl_M27_PPB_Graphics2D_SetScale(PP_Resource resource, float scale) {
  const struct PPB_Graphics2D_1_1 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_1.real_iface;
  return iface->SetScale(resource, scale);
}

static float Pnacl_M27_PPB_Graphics2D_GetScale(PP_Resource resource) {
  const struct PPB_Graphics2D_1_1 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_1.real_iface;
  return iface->GetScale(resource);
}

/* End wrapper methods for PPB_Graphics2D_1_1 */

/* Begin wrapper methods for PPB_Graphics2D_1_2 */

static PP_Resource Pnacl_M52_PPB_Graphics2D_Create(PP_Instance instance, const struct PP_Size* size, PP_Bool is_always_opaque) {
  const struct PPB_Graphics2D_1_2 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_2.real_iface;
  return iface->Create(instance, size, is_always_opaque);
}

static PP_Bool Pnacl_M52_PPB_Graphics2D_IsGraphics2D(PP_Resource resource) {
  const struct PPB_Graphics2D_1_2 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_2.real_iface;
  return iface->IsGraphics2D(resource);
}

static PP_Bool Pnacl_M52_PPB_Graphics2D_Describe(PP_Resource graphics_2d, struct PP_Size* size, PP_Bool* is_always_opaque) {
  const struct PPB_Graphics2D_1_2 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_2.real_iface;
  return iface->Describe(graphics_2d, size, is_always_opaque);
}

static void Pnacl_M52_PPB_Graphics2D_PaintImageData(PP_Resource graphics_2d, PP_Resource image_data, const struct PP_Point* top_left, const struct PP_Rect* src_rect) {
  const struct PPB_Graphics2D_1_2 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_2.real_iface;
  iface->PaintImageData(graphics_2d, image_data, top_left, src_rect);
}

static void Pnacl_M52_PPB_Graphics2D_Scroll(PP_Resource graphics_2d, const struct PP_Rect* clip_rect, const struct PP_Point* amount) {
  const struct PPB_Graphics2D_1_2 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_2.real_iface;
  iface->Scroll(graphics_2d, clip_rect, amount);
}

static void Pnacl_M52_PPB_Graphics2D_ReplaceContents(PP_Resource graphics_2d, PP_Resource image_data) {
  const struct PPB_Graphics2D_1_2 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_2.real_iface;
  iface->ReplaceContents(graphics_2d, image_data);
}

static int32_t Pnacl_M52_PPB_Graphics2D_Flush(PP_Resource graphics_2d, struct PP_CompletionCallback* callback) {
  const struct PPB_Graphics2D_1_2 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_2.real_iface;
  return iface->Flush(graphics_2d, *callback);
}

static PP_Bool Pnacl_M52_PPB_Graphics2D_SetScale(PP_Resource resource, float scale) {
  const struct PPB_Graphics2D_1_2 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_2.real_iface;
  return iface->SetScale(resource, scale);
}

static float Pnacl_M52_PPB_Graphics2D_GetScale(PP_Resource resource) {
  const struct PPB_Graphics2D_1_2 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_2.real_iface;
  return iface->GetScale(resource);
}

static PP_Bool Pnacl_M52_PPB_Graphics2D_SetLayerTransform(PP_Resource resource, float scale, const struct PP_Point* origin, const struct PP_Point* translate) {
  const struct PPB_Graphics2D_1_2 *iface = Pnacl_WrapperInfo_PPB_Graphics2D_1_2.real_iface;
  return iface->SetLayerTransform(resource, scale, origin, translate);
}

/* End wrapper methods for PPB_Graphics2D_1_2 */

/* Begin wrapper methods for PPB_Graphics3D_1_0 */

static int32_t Pnacl_M15_PPB_Graphics3D_GetAttribMaxValue(PP_Resource instance, int32_t attribute, int32_t* value) {
  const struct PPB_Graphics3D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics3D_1_0.real_iface;
  return iface->GetAttribMaxValue(instance, attribute, value);
}

static PP_Resource Pnacl_M15_PPB_Graphics3D_Create(PP_Instance instance, PP_Resource share_context, const int32_t attrib_list[]) {
  const struct PPB_Graphics3D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics3D_1_0.real_iface;
  return iface->Create(instance, share_context, attrib_list);
}

static PP_Bool Pnacl_M15_PPB_Graphics3D_IsGraphics3D(PP_Resource resource) {
  const struct PPB_Graphics3D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics3D_1_0.real_iface;
  return iface->IsGraphics3D(resource);
}

static int32_t Pnacl_M15_PPB_Graphics3D_GetAttribs(PP_Resource context, int32_t attrib_list[]) {
  const struct PPB_Graphics3D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics3D_1_0.real_iface;
  return iface->GetAttribs(context, attrib_list);
}

static int32_t Pnacl_M15_PPB_Graphics3D_SetAttribs(PP_Resource context, const int32_t attrib_list[]) {
  const struct PPB_Graphics3D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics3D_1_0.real_iface;
  return iface->SetAttribs(context, attrib_list);
}

static int32_t Pnacl_M15_PPB_Graphics3D_GetError(PP_Resource context) {
  const struct PPB_Graphics3D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics3D_1_0.real_iface;
  return iface->GetError(context);
}

static int32_t Pnacl_M15_PPB_Graphics3D_ResizeBuffers(PP_Resource context, int32_t width, int32_t height) {
  const struct PPB_Graphics3D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics3D_1_0.real_iface;
  return iface->ResizeBuffers(context, width, height);
}

static int32_t Pnacl_M15_PPB_Graphics3D_SwapBuffers(PP_Resource context, struct PP_CompletionCallback* callback) {
  const struct PPB_Graphics3D_1_0 *iface = Pnacl_WrapperInfo_PPB_Graphics3D_1_0.real_iface;
  return iface->SwapBuffers(context, *callback);
}

/* End wrapper methods for PPB_Graphics3D_1_0 */

/* Begin wrapper methods for PPB_HostResolver_1_0 */

static PP_Resource Pnacl_M29_PPB_HostResolver_Create(PP_Instance instance) {
  const struct PPB_HostResolver_1_0 *iface = Pnacl_WrapperInfo_PPB_HostResolver_1_0.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M29_PPB_HostResolver_IsHostResolver(PP_Resource resource) {
  const struct PPB_HostResolver_1_0 *iface = Pnacl_WrapperInfo_PPB_HostResolver_1_0.real_iface;
  return iface->IsHostResolver(resource);
}

static int32_t Pnacl_M29_PPB_HostResolver_Resolve(PP_Resource host_resolver, const char* host, uint16_t port, const struct PP_HostResolver_Hint* hint, struct PP_CompletionCallback* callback) {
  const struct PPB_HostResolver_1_0 *iface = Pnacl_WrapperInfo_PPB_HostResolver_1_0.real_iface;
  return iface->Resolve(host_resolver, host, port, hint, *callback);
}

static void Pnacl_M29_PPB_HostResolver_GetCanonicalName(struct PP_Var* _struct_result, PP_Resource host_resolver) {
  const struct PPB_HostResolver_1_0 *iface = Pnacl_WrapperInfo_PPB_HostResolver_1_0.real_iface;
  *_struct_result = iface->GetCanonicalName(host_resolver);
}

static uint32_t Pnacl_M29_PPB_HostResolver_GetNetAddressCount(PP_Resource host_resolver) {
  const struct PPB_HostResolver_1_0 *iface = Pnacl_WrapperInfo_PPB_HostResolver_1_0.real_iface;
  return iface->GetNetAddressCount(host_resolver);
}

static PP_Resource Pnacl_M29_PPB_HostResolver_GetNetAddress(PP_Resource host_resolver, uint32_t index) {
  const struct PPB_HostResolver_1_0 *iface = Pnacl_WrapperInfo_PPB_HostResolver_1_0.real_iface;
  return iface->GetNetAddress(host_resolver, index);
}

/* End wrapper methods for PPB_HostResolver_1_0 */

/* Not generating wrapper methods for PPB_ImageData_1_0 */

/* Not generating wrapper methods for PPB_InputEvent_1_0 */

/* Begin wrapper methods for PPB_MouseInputEvent_1_0 */

static PP_Resource Pnacl_M13_PPB_MouseInputEvent_Create(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, uint32_t modifiers, PP_InputEvent_MouseButton mouse_button, const struct PP_Point* mouse_position, int32_t click_count) {
  const struct PPB_MouseInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_MouseInputEvent_1_0.real_iface;
  return iface->Create(instance, type, time_stamp, modifiers, mouse_button, mouse_position, click_count);
}

static PP_Bool Pnacl_M13_PPB_MouseInputEvent_IsMouseInputEvent(PP_Resource resource) {
  const struct PPB_MouseInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_MouseInputEvent_1_0.real_iface;
  return iface->IsMouseInputEvent(resource);
}

static PP_InputEvent_MouseButton Pnacl_M13_PPB_MouseInputEvent_GetButton(PP_Resource mouse_event) {
  const struct PPB_MouseInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_MouseInputEvent_1_0.real_iface;
  return iface->GetButton(mouse_event);
}

static void Pnacl_M13_PPB_MouseInputEvent_GetPosition(struct PP_Point* _struct_result, PP_Resource mouse_event) {
  const struct PPB_MouseInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_MouseInputEvent_1_0.real_iface;
  *_struct_result = iface->GetPosition(mouse_event);
}

static int32_t Pnacl_M13_PPB_MouseInputEvent_GetClickCount(PP_Resource mouse_event) {
  const struct PPB_MouseInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_MouseInputEvent_1_0.real_iface;
  return iface->GetClickCount(mouse_event);
}

/* End wrapper methods for PPB_MouseInputEvent_1_0 */

/* Begin wrapper methods for PPB_MouseInputEvent_1_1 */

static PP_Resource Pnacl_M14_PPB_MouseInputEvent_Create(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, uint32_t modifiers, PP_InputEvent_MouseButton mouse_button, const struct PP_Point* mouse_position, int32_t click_count, const struct PP_Point* mouse_movement) {
  const struct PPB_MouseInputEvent_1_1 *iface = Pnacl_WrapperInfo_PPB_MouseInputEvent_1_1.real_iface;
  return iface->Create(instance, type, time_stamp, modifiers, mouse_button, mouse_position, click_count, mouse_movement);
}

static PP_Bool Pnacl_M14_PPB_MouseInputEvent_IsMouseInputEvent(PP_Resource resource) {
  const struct PPB_MouseInputEvent_1_1 *iface = Pnacl_WrapperInfo_PPB_MouseInputEvent_1_1.real_iface;
  return iface->IsMouseInputEvent(resource);
}

static PP_InputEvent_MouseButton Pnacl_M14_PPB_MouseInputEvent_GetButton(PP_Resource mouse_event) {
  const struct PPB_MouseInputEvent_1_1 *iface = Pnacl_WrapperInfo_PPB_MouseInputEvent_1_1.real_iface;
  return iface->GetButton(mouse_event);
}

static void Pnacl_M14_PPB_MouseInputEvent_GetPosition(struct PP_Point* _struct_result, PP_Resource mouse_event) {
  const struct PPB_MouseInputEvent_1_1 *iface = Pnacl_WrapperInfo_PPB_MouseInputEvent_1_1.real_iface;
  *_struct_result = iface->GetPosition(mouse_event);
}

static int32_t Pnacl_M14_PPB_MouseInputEvent_GetClickCount(PP_Resource mouse_event) {
  const struct PPB_MouseInputEvent_1_1 *iface = Pnacl_WrapperInfo_PPB_MouseInputEvent_1_1.real_iface;
  return iface->GetClickCount(mouse_event);
}

static void Pnacl_M14_PPB_MouseInputEvent_GetMovement(struct PP_Point* _struct_result, PP_Resource mouse_event) {
  const struct PPB_MouseInputEvent_1_1 *iface = Pnacl_WrapperInfo_PPB_MouseInputEvent_1_1.real_iface;
  *_struct_result = iface->GetMovement(mouse_event);
}

/* End wrapper methods for PPB_MouseInputEvent_1_1 */

/* Begin wrapper methods for PPB_WheelInputEvent_1_0 */

static PP_Resource Pnacl_M13_PPB_WheelInputEvent_Create(PP_Instance instance, PP_TimeTicks time_stamp, uint32_t modifiers, const struct PP_FloatPoint* wheel_delta, const struct PP_FloatPoint* wheel_ticks, PP_Bool scroll_by_page) {
  const struct PPB_WheelInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_WheelInputEvent_1_0.real_iface;
  return iface->Create(instance, time_stamp, modifiers, wheel_delta, wheel_ticks, scroll_by_page);
}

static PP_Bool Pnacl_M13_PPB_WheelInputEvent_IsWheelInputEvent(PP_Resource resource) {
  const struct PPB_WheelInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_WheelInputEvent_1_0.real_iface;
  return iface->IsWheelInputEvent(resource);
}

static void Pnacl_M13_PPB_WheelInputEvent_GetDelta(struct PP_FloatPoint* _struct_result, PP_Resource wheel_event) {
  const struct PPB_WheelInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_WheelInputEvent_1_0.real_iface;
  *_struct_result = iface->GetDelta(wheel_event);
}

static void Pnacl_M13_PPB_WheelInputEvent_GetTicks(struct PP_FloatPoint* _struct_result, PP_Resource wheel_event) {
  const struct PPB_WheelInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_WheelInputEvent_1_0.real_iface;
  *_struct_result = iface->GetTicks(wheel_event);
}

static PP_Bool Pnacl_M13_PPB_WheelInputEvent_GetScrollByPage(PP_Resource wheel_event) {
  const struct PPB_WheelInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_WheelInputEvent_1_0.real_iface;
  return iface->GetScrollByPage(wheel_event);
}

/* End wrapper methods for PPB_WheelInputEvent_1_0 */

/* Begin wrapper methods for PPB_KeyboardInputEvent_1_0 */

static PP_Resource Pnacl_M13_PPB_KeyboardInputEvent_Create(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, uint32_t modifiers, uint32_t key_code, struct PP_Var* character_text) {
  const struct PPB_KeyboardInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_KeyboardInputEvent_1_0.real_iface;
  return iface->Create(instance, type, time_stamp, modifiers, key_code, *character_text);
}

static PP_Bool Pnacl_M13_PPB_KeyboardInputEvent_IsKeyboardInputEvent(PP_Resource resource) {
  const struct PPB_KeyboardInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_KeyboardInputEvent_1_0.real_iface;
  return iface->IsKeyboardInputEvent(resource);
}

static uint32_t Pnacl_M13_PPB_KeyboardInputEvent_GetKeyCode(PP_Resource key_event) {
  const struct PPB_KeyboardInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_KeyboardInputEvent_1_0.real_iface;
  return iface->GetKeyCode(key_event);
}

static void Pnacl_M13_PPB_KeyboardInputEvent_GetCharacterText(struct PP_Var* _struct_result, PP_Resource character_event) {
  const struct PPB_KeyboardInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_KeyboardInputEvent_1_0.real_iface;
  *_struct_result = iface->GetCharacterText(character_event);
}

/* End wrapper methods for PPB_KeyboardInputEvent_1_0 */

/* Begin wrapper methods for PPB_KeyboardInputEvent_1_2 */

static PP_Resource Pnacl_M34_PPB_KeyboardInputEvent_Create(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, uint32_t modifiers, uint32_t key_code, struct PP_Var* character_text, struct PP_Var* code) {
  const struct PPB_KeyboardInputEvent_1_2 *iface = Pnacl_WrapperInfo_PPB_KeyboardInputEvent_1_2.real_iface;
  return iface->Create(instance, type, time_stamp, modifiers, key_code, *character_text, *code);
}

static PP_Bool Pnacl_M34_PPB_KeyboardInputEvent_IsKeyboardInputEvent(PP_Resource resource) {
  const struct PPB_KeyboardInputEvent_1_2 *iface = Pnacl_WrapperInfo_PPB_KeyboardInputEvent_1_2.real_iface;
  return iface->IsKeyboardInputEvent(resource);
}

static uint32_t Pnacl_M34_PPB_KeyboardInputEvent_GetKeyCode(PP_Resource key_event) {
  const struct PPB_KeyboardInputEvent_1_2 *iface = Pnacl_WrapperInfo_PPB_KeyboardInputEvent_1_2.real_iface;
  return iface->GetKeyCode(key_event);
}

static void Pnacl_M34_PPB_KeyboardInputEvent_GetCharacterText(struct PP_Var* _struct_result, PP_Resource character_event) {
  const struct PPB_KeyboardInputEvent_1_2 *iface = Pnacl_WrapperInfo_PPB_KeyboardInputEvent_1_2.real_iface;
  *_struct_result = iface->GetCharacterText(character_event);
}

static void Pnacl_M34_PPB_KeyboardInputEvent_GetCode(struct PP_Var* _struct_result, PP_Resource key_event) {
  const struct PPB_KeyboardInputEvent_1_2 *iface = Pnacl_WrapperInfo_PPB_KeyboardInputEvent_1_2.real_iface;
  *_struct_result = iface->GetCode(key_event);
}

/* End wrapper methods for PPB_KeyboardInputEvent_1_2 */

/* Begin wrapper methods for PPB_TouchInputEvent_1_0 */

static PP_Resource Pnacl_M13_PPB_TouchInputEvent_Create(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, uint32_t modifiers) {
  const struct PPB_TouchInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_TouchInputEvent_1_0.real_iface;
  return iface->Create(instance, type, time_stamp, modifiers);
}

static void Pnacl_M13_PPB_TouchInputEvent_AddTouchPoint(PP_Resource touch_event, PP_TouchListType list, const struct PP_TouchPoint* point) {
  const struct PPB_TouchInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_TouchInputEvent_1_0.real_iface;
  iface->AddTouchPoint(touch_event, list, point);
}

static PP_Bool Pnacl_M13_PPB_TouchInputEvent_IsTouchInputEvent(PP_Resource resource) {
  const struct PPB_TouchInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_TouchInputEvent_1_0.real_iface;
  return iface->IsTouchInputEvent(resource);
}

static uint32_t Pnacl_M13_PPB_TouchInputEvent_GetTouchCount(PP_Resource resource, PP_TouchListType list) {
  const struct PPB_TouchInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_TouchInputEvent_1_0.real_iface;
  return iface->GetTouchCount(resource, list);
}

static void Pnacl_M13_PPB_TouchInputEvent_GetTouchByIndex(struct PP_TouchPoint* _struct_result, PP_Resource resource, PP_TouchListType list, uint32_t index) {
  const struct PPB_TouchInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_TouchInputEvent_1_0.real_iface;
  *_struct_result = iface->GetTouchByIndex(resource, list, index);
}

static void Pnacl_M13_PPB_TouchInputEvent_GetTouchById(struct PP_TouchPoint* _struct_result, PP_Resource resource, PP_TouchListType list, uint32_t touch_id) {
  const struct PPB_TouchInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_TouchInputEvent_1_0.real_iface;
  *_struct_result = iface->GetTouchById(resource, list, touch_id);
}

/* End wrapper methods for PPB_TouchInputEvent_1_0 */

/* Begin wrapper methods for PPB_TouchInputEvent_1_4 */

static PP_Resource Pnacl_M60_PPB_TouchInputEvent_Create(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, uint32_t modifiers) {
  const struct PPB_TouchInputEvent_1_4 *iface = Pnacl_WrapperInfo_PPB_TouchInputEvent_1_4.real_iface;
  return iface->Create(instance, type, time_stamp, modifiers);
}

static void Pnacl_M60_PPB_TouchInputEvent_AddTouchPoint(PP_Resource touch_event, PP_TouchListType list, const struct PP_TouchPoint* point) {
  const struct PPB_TouchInputEvent_1_4 *iface = Pnacl_WrapperInfo_PPB_TouchInputEvent_1_4.real_iface;
  iface->AddTouchPoint(touch_event, list, point);
}

static PP_Bool Pnacl_M60_PPB_TouchInputEvent_IsTouchInputEvent(PP_Resource resource) {
  const struct PPB_TouchInputEvent_1_4 *iface = Pnacl_WrapperInfo_PPB_TouchInputEvent_1_4.real_iface;
  return iface->IsTouchInputEvent(resource);
}

static uint32_t Pnacl_M60_PPB_TouchInputEvent_GetTouchCount(PP_Resource resource, PP_TouchListType list) {
  const struct PPB_TouchInputEvent_1_4 *iface = Pnacl_WrapperInfo_PPB_TouchInputEvent_1_4.real_iface;
  return iface->GetTouchCount(resource, list);
}

static void Pnacl_M60_PPB_TouchInputEvent_GetTouchByIndex(struct PP_TouchPoint* _struct_result, PP_Resource resource, PP_TouchListType list, uint32_t index) {
  const struct PPB_TouchInputEvent_1_4 *iface = Pnacl_WrapperInfo_PPB_TouchInputEvent_1_4.real_iface;
  *_struct_result = iface->GetTouchByIndex(resource, list, index);
}

static void Pnacl_M60_PPB_TouchInputEvent_GetTouchById(struct PP_TouchPoint* _struct_result, PP_Resource resource, PP_TouchListType list, uint32_t touch_id) {
  const struct PPB_TouchInputEvent_1_4 *iface = Pnacl_WrapperInfo_PPB_TouchInputEvent_1_4.real_iface;
  *_struct_result = iface->GetTouchById(resource, list, touch_id);
}

static void Pnacl_M60_PPB_TouchInputEvent_GetTouchTiltByIndex(struct PP_FloatPoint* _struct_result, PP_Resource resource, PP_TouchListType list, uint32_t index) {
  const struct PPB_TouchInputEvent_1_4 *iface = Pnacl_WrapperInfo_PPB_TouchInputEvent_1_4.real_iface;
  *_struct_result = iface->GetTouchTiltByIndex(resource, list, index);
}

static void Pnacl_M60_PPB_TouchInputEvent_GetTouchTiltById(struct PP_FloatPoint* _struct_result, PP_Resource resource, PP_TouchListType list, uint32_t touch_id) {
  const struct PPB_TouchInputEvent_1_4 *iface = Pnacl_WrapperInfo_PPB_TouchInputEvent_1_4.real_iface;
  *_struct_result = iface->GetTouchTiltById(resource, list, touch_id);
}

/* End wrapper methods for PPB_TouchInputEvent_1_4 */

/* Begin wrapper methods for PPB_IMEInputEvent_1_0 */

static PP_Resource Pnacl_M13_PPB_IMEInputEvent_Create(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, struct PP_Var* text, uint32_t segment_number, const uint32_t segment_offsets[], int32_t target_segment, uint32_t selection_start, uint32_t selection_end) {
  const struct PPB_IMEInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_1_0.real_iface;
  return iface->Create(instance, type, time_stamp, *text, segment_number, segment_offsets, target_segment, selection_start, selection_end);
}

static PP_Bool Pnacl_M13_PPB_IMEInputEvent_IsIMEInputEvent(PP_Resource resource) {
  const struct PPB_IMEInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_1_0.real_iface;
  return iface->IsIMEInputEvent(resource);
}

static void Pnacl_M13_PPB_IMEInputEvent_GetText(struct PP_Var* _struct_result, PP_Resource ime_event) {
  const struct PPB_IMEInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_1_0.real_iface;
  *_struct_result = iface->GetText(ime_event);
}

static uint32_t Pnacl_M13_PPB_IMEInputEvent_GetSegmentNumber(PP_Resource ime_event) {
  const struct PPB_IMEInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_1_0.real_iface;
  return iface->GetSegmentNumber(ime_event);
}

static uint32_t Pnacl_M13_PPB_IMEInputEvent_GetSegmentOffset(PP_Resource ime_event, uint32_t index) {
  const struct PPB_IMEInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_1_0.real_iface;
  return iface->GetSegmentOffset(ime_event, index);
}

static int32_t Pnacl_M13_PPB_IMEInputEvent_GetTargetSegment(PP_Resource ime_event) {
  const struct PPB_IMEInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_1_0.real_iface;
  return iface->GetTargetSegment(ime_event);
}

static void Pnacl_M13_PPB_IMEInputEvent_GetSelection(PP_Resource ime_event, uint32_t* start, uint32_t* end) {
  const struct PPB_IMEInputEvent_1_0 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_1_0.real_iface;
  iface->GetSelection(ime_event, start, end);
}

/* End wrapper methods for PPB_IMEInputEvent_1_0 */

/* Not generating wrapper methods for PPB_Instance_1_0 */

/* Begin wrapper methods for PPB_MediaStreamAudioTrack_0_1 */

static PP_Bool Pnacl_M35_PPB_MediaStreamAudioTrack_IsMediaStreamAudioTrack(PP_Resource resource) {
  const struct PPB_MediaStreamAudioTrack_0_1 *iface = Pnacl_WrapperInfo_PPB_MediaStreamAudioTrack_0_1.real_iface;
  return iface->IsMediaStreamAudioTrack(resource);
}

static int32_t Pnacl_M35_PPB_MediaStreamAudioTrack_Configure(PP_Resource audio_track, const int32_t attrib_list[], struct PP_CompletionCallback* callback) {
  const struct PPB_MediaStreamAudioTrack_0_1 *iface = Pnacl_WrapperInfo_PPB_MediaStreamAudioTrack_0_1.real_iface;
  return iface->Configure(audio_track, attrib_list, *callback);
}

static int32_t Pnacl_M35_PPB_MediaStreamAudioTrack_GetAttrib(PP_Resource audio_track, PP_MediaStreamAudioTrack_Attrib attrib, int32_t* value) {
  const struct PPB_MediaStreamAudioTrack_0_1 *iface = Pnacl_WrapperInfo_PPB_MediaStreamAudioTrack_0_1.real_iface;
  return iface->GetAttrib(audio_track, attrib, value);
}

static void Pnacl_M35_PPB_MediaStreamAudioTrack_GetId(struct PP_Var* _struct_result, PP_Resource audio_track) {
  const struct PPB_MediaStreamAudioTrack_0_1 *iface = Pnacl_WrapperInfo_PPB_MediaStreamAudioTrack_0_1.real_iface;
  *_struct_result = iface->GetId(audio_track);
}

static PP_Bool Pnacl_M35_PPB_MediaStreamAudioTrack_HasEnded(PP_Resource audio_track) {
  const struct PPB_MediaStreamAudioTrack_0_1 *iface = Pnacl_WrapperInfo_PPB_MediaStreamAudioTrack_0_1.real_iface;
  return iface->HasEnded(audio_track);
}

static int32_t Pnacl_M35_PPB_MediaStreamAudioTrack_GetBuffer(PP_Resource audio_track, PP_Resource* buffer, struct PP_CompletionCallback* callback) {
  const struct PPB_MediaStreamAudioTrack_0_1 *iface = Pnacl_WrapperInfo_PPB_MediaStreamAudioTrack_0_1.real_iface;
  return iface->GetBuffer(audio_track, buffer, *callback);
}

static int32_t Pnacl_M35_PPB_MediaStreamAudioTrack_RecycleBuffer(PP_Resource audio_track, PP_Resource buffer) {
  const struct PPB_MediaStreamAudioTrack_0_1 *iface = Pnacl_WrapperInfo_PPB_MediaStreamAudioTrack_0_1.real_iface;
  return iface->RecycleBuffer(audio_track, buffer);
}

static void Pnacl_M35_PPB_MediaStreamAudioTrack_Close(PP_Resource audio_track) {
  const struct PPB_MediaStreamAudioTrack_0_1 *iface = Pnacl_WrapperInfo_PPB_MediaStreamAudioTrack_0_1.real_iface;
  iface->Close(audio_track);
}

/* End wrapper methods for PPB_MediaStreamAudioTrack_0_1 */

/* Begin wrapper methods for PPB_MediaStreamVideoTrack_0_1 */

static PP_Bool Pnacl_M35_PPB_MediaStreamVideoTrack_IsMediaStreamVideoTrack(PP_Resource resource) {
  const struct PPB_MediaStreamVideoTrack_0_1 *iface = Pnacl_WrapperInfo_PPB_MediaStreamVideoTrack_0_1.real_iface;
  return iface->IsMediaStreamVideoTrack(resource);
}

static int32_t Pnacl_M35_PPB_MediaStreamVideoTrack_Configure(PP_Resource video_track, const int32_t attrib_list[], struct PP_CompletionCallback* callback) {
  const struct PPB_MediaStreamVideoTrack_0_1 *iface = Pnacl_WrapperInfo_PPB_MediaStreamVideoTrack_0_1.real_iface;
  return iface->Configure(video_track, attrib_list, *callback);
}

static int32_t Pnacl_M35_PPB_MediaStreamVideoTrack_GetAttrib(PP_Resource video_track, PP_MediaStreamVideoTrack_Attrib attrib, int32_t* value) {
  const struct PPB_MediaStreamVideoTrack_0_1 *iface = Pnacl_WrapperInfo_PPB_MediaStreamVideoTrack_0_1.real_iface;
  return iface->GetAttrib(video_track, attrib, value);
}

static void Pnacl_M35_PPB_MediaStreamVideoTrack_GetId(struct PP_Var* _struct_result, PP_Resource video_track) {
  const struct PPB_MediaStreamVideoTrack_0_1 *iface = Pnacl_WrapperInfo_PPB_MediaStreamVideoTrack_0_1.real_iface;
  *_struct_result = iface->GetId(video_track);
}

static PP_Bool Pnacl_M35_PPB_MediaStreamVideoTrack_HasEnded(PP_Resource video_track) {
  const struct PPB_MediaStreamVideoTrack_0_1 *iface = Pnacl_WrapperInfo_PPB_MediaStreamVideoTrack_0_1.real_iface;
  return iface->HasEnded(video_track);
}

static int32_t Pnacl_M35_PPB_MediaStreamVideoTrack_GetFrame(PP_Resource video_track, PP_Resource* frame, struct PP_CompletionCallback* callback) {
  const struct PPB_MediaStreamVideoTrack_0_1 *iface = Pnacl_WrapperInfo_PPB_MediaStreamVideoTrack_0_1.real_iface;
  return iface->GetFrame(video_track, frame, *callback);
}

static int32_t Pnacl_M35_PPB_MediaStreamVideoTrack_RecycleFrame(PP_Resource video_track, PP_Resource frame) {
  const struct PPB_MediaStreamVideoTrack_0_1 *iface = Pnacl_WrapperInfo_PPB_MediaStreamVideoTrack_0_1.real_iface;
  return iface->RecycleFrame(video_track, frame);
}

static void Pnacl_M35_PPB_MediaStreamVideoTrack_Close(PP_Resource video_track) {
  const struct PPB_MediaStreamVideoTrack_0_1 *iface = Pnacl_WrapperInfo_PPB_MediaStreamVideoTrack_0_1.real_iface;
  iface->Close(video_track);
}

/* End wrapper methods for PPB_MediaStreamVideoTrack_0_1 */

/* Begin wrapper methods for PPB_MediaStreamVideoTrack_1_0 */

static PP_Resource Pnacl_M36_PPB_MediaStreamVideoTrack_Create(PP_Instance instance) {
  const struct PPB_MediaStreamVideoTrack_1_0 *iface = Pnacl_WrapperInfo_PPB_MediaStreamVideoTrack_1_0.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M36_PPB_MediaStreamVideoTrack_IsMediaStreamVideoTrack(PP_Resource resource) {
  const struct PPB_MediaStreamVideoTrack_1_0 *iface = Pnacl_WrapperInfo_PPB_MediaStreamVideoTrack_1_0.real_iface;
  return iface->IsMediaStreamVideoTrack(resource);
}

static int32_t Pnacl_M36_PPB_MediaStreamVideoTrack_Configure(PP_Resource video_track, const int32_t attrib_list[], struct PP_CompletionCallback* callback) {
  const struct PPB_MediaStreamVideoTrack_1_0 *iface = Pnacl_WrapperInfo_PPB_MediaStreamVideoTrack_1_0.real_iface;
  return iface->Configure(video_track, attrib_list, *callback);
}

static int32_t Pnacl_M36_PPB_MediaStreamVideoTrack_GetAttrib(PP_Resource video_track, PP_MediaStreamVideoTrack_Attrib attrib, int32_t* value) {
  const struct PPB_MediaStreamVideoTrack_1_0 *iface = Pnacl_WrapperInfo_PPB_MediaStreamVideoTrack_1_0.real_iface;
  return iface->GetAttrib(video_track, attrib, value);
}

static void Pnacl_M36_PPB_MediaStreamVideoTrack_GetId(struct PP_Var* _struct_result, PP_Resource video_track) {
  const struct PPB_MediaStreamVideoTrack_1_0 *iface = Pnacl_WrapperInfo_PPB_MediaStreamVideoTrack_1_0.real_iface;
  *_struct_result = iface->GetId(video_track);
}

static PP_Bool Pnacl_M36_PPB_MediaStreamVideoTrack_HasEnded(PP_Resource video_track) {
  const struct PPB_MediaStreamVideoTrack_1_0 *iface = Pnacl_WrapperInfo_PPB_MediaStreamVideoTrack_1_0.real_iface;
  return iface->HasEnded(video_track);
}

static int32_t Pnacl_M36_PPB_MediaStreamVideoTrack_GetFrame(PP_Resource video_track, PP_Resource* frame, struct PP_CompletionCallback* callback) {
  const struct PPB_MediaStreamVideoTrack_1_0 *iface = Pnacl_WrapperInfo_PPB_MediaStreamVideoTrack_1_0.real_iface;
  return iface->GetFrame(video_track, frame, *callback);
}

static int32_t Pnacl_M36_PPB_MediaStreamVideoTrack_RecycleFrame(PP_Resource video_track, PP_Resource frame) {
  const struct PPB_MediaStreamVideoTrack_1_0 *iface = Pnacl_WrapperInfo_PPB_MediaStreamVideoTrack_1_0.real_iface;
  return iface->RecycleFrame(video_track, frame);
}

static void Pnacl_M36_PPB_MediaStreamVideoTrack_Close(PP_Resource video_track) {
  const struct PPB_MediaStreamVideoTrack_1_0 *iface = Pnacl_WrapperInfo_PPB_MediaStreamVideoTrack_1_0.real_iface;
  iface->Close(video_track);
}

static int32_t Pnacl_M36_PPB_MediaStreamVideoTrack_GetEmptyFrame(PP_Resource video_track, PP_Resource* frame, struct PP_CompletionCallback* callback) {
  const struct PPB_MediaStreamVideoTrack_1_0 *iface = Pnacl_WrapperInfo_PPB_MediaStreamVideoTrack_1_0.real_iface;
  return iface->GetEmptyFrame(video_track, frame, *callback);
}

static int32_t Pnacl_M36_PPB_MediaStreamVideoTrack_PutFrame(PP_Resource video_track, PP_Resource frame) {
  const struct PPB_MediaStreamVideoTrack_1_0 *iface = Pnacl_WrapperInfo_PPB_MediaStreamVideoTrack_1_0.real_iface;
  return iface->PutFrame(video_track, frame);
}

/* End wrapper methods for PPB_MediaStreamVideoTrack_1_0 */

/* Begin wrapper methods for PPB_MessageLoop_1_0 */

static PP_Resource Pnacl_M25_PPB_MessageLoop_Create(PP_Instance instance) {
  const struct PPB_MessageLoop_1_0 *iface = Pnacl_WrapperInfo_PPB_MessageLoop_1_0.real_iface;
  return iface->Create(instance);
}

static PP_Resource Pnacl_M25_PPB_MessageLoop_GetForMainThread(void) {
  const struct PPB_MessageLoop_1_0 *iface = Pnacl_WrapperInfo_PPB_MessageLoop_1_0.real_iface;
  return iface->GetForMainThread();
}

static PP_Resource Pnacl_M25_PPB_MessageLoop_GetCurrent(void) {
  const struct PPB_MessageLoop_1_0 *iface = Pnacl_WrapperInfo_PPB_MessageLoop_1_0.real_iface;
  return iface->GetCurrent();
}

static int32_t Pnacl_M25_PPB_MessageLoop_AttachToCurrentThread(PP_Resource message_loop) {
  const struct PPB_MessageLoop_1_0 *iface = Pnacl_WrapperInfo_PPB_MessageLoop_1_0.real_iface;
  return iface->AttachToCurrentThread(message_loop);
}

static int32_t Pnacl_M25_PPB_MessageLoop_Run(PP_Resource message_loop) {
  const struct PPB_MessageLoop_1_0 *iface = Pnacl_WrapperInfo_PPB_MessageLoop_1_0.real_iface;
  return iface->Run(message_loop);
}

static int32_t Pnacl_M25_PPB_MessageLoop_PostWork(PP_Resource message_loop, struct PP_CompletionCallback* callback, int64_t delay_ms) {
  const struct PPB_MessageLoop_1_0 *iface = Pnacl_WrapperInfo_PPB_MessageLoop_1_0.real_iface;
  return iface->PostWork(message_loop, *callback, delay_ms);
}

static int32_t Pnacl_M25_PPB_MessageLoop_PostQuit(PP_Resource message_loop, PP_Bool should_destroy) {
  const struct PPB_MessageLoop_1_0 *iface = Pnacl_WrapperInfo_PPB_MessageLoop_1_0.real_iface;
  return iface->PostQuit(message_loop, should_destroy);
}

/* End wrapper methods for PPB_MessageLoop_1_0 */

/* Begin wrapper methods for PPB_Messaging_1_0 */

static void Pnacl_M14_PPB_Messaging_PostMessage(PP_Instance instance, struct PP_Var* message) {
  const struct PPB_Messaging_1_0 *iface = Pnacl_WrapperInfo_PPB_Messaging_1_0.real_iface;
  iface->PostMessage(instance, *message);
}

/* End wrapper methods for PPB_Messaging_1_0 */

/* Begin wrapper methods for PPB_Messaging_1_2 */

static void Pnacl_M39_PPB_Messaging_PostMessage(PP_Instance instance, struct PP_Var* message) {
  const struct PPB_Messaging_1_2 *iface = Pnacl_WrapperInfo_PPB_Messaging_1_2.real_iface;
  iface->PostMessage(instance, *message);
}

static int32_t Pnacl_M39_PPB_Messaging_RegisterMessageHandler(PP_Instance instance, void* user_data, const struct PPP_MessageHandler_0_2* handler, PP_Resource message_loop) {
  const struct PPB_Messaging_1_2 *iface = Pnacl_WrapperInfo_PPB_Messaging_1_2.real_iface;
  return iface->RegisterMessageHandler(instance, user_data, handler, message_loop);
}

static void Pnacl_M39_PPB_Messaging_UnregisterMessageHandler(PP_Instance instance) {
  const struct PPB_Messaging_1_2 *iface = Pnacl_WrapperInfo_PPB_Messaging_1_2.real_iface;
  iface->UnregisterMessageHandler(instance);
}

/* End wrapper methods for PPB_Messaging_1_2 */

/* Not generating wrapper methods for PPB_MouseCursor_1_0 */

/* Begin wrapper methods for PPB_MouseLock_1_0 */

static int32_t Pnacl_M16_PPB_MouseLock_LockMouse(PP_Instance instance, struct PP_CompletionCallback* callback) {
  const struct PPB_MouseLock_1_0 *iface = Pnacl_WrapperInfo_PPB_MouseLock_1_0.real_iface;
  return iface->LockMouse(instance, *callback);
}

static void Pnacl_M16_PPB_MouseLock_UnlockMouse(PP_Instance instance) {
  const struct PPB_MouseLock_1_0 *iface = Pnacl_WrapperInfo_PPB_MouseLock_1_0.real_iface;
  iface->UnlockMouse(instance);
}

/* End wrapper methods for PPB_MouseLock_1_0 */

/* Begin wrapper methods for PPB_NetAddress_1_0 */

static PP_Resource Pnacl_M29_PPB_NetAddress_CreateFromIPv4Address(PP_Instance instance, const struct PP_NetAddress_IPv4* ipv4_addr) {
  const struct PPB_NetAddress_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_1_0.real_iface;
  return iface->CreateFromIPv4Address(instance, ipv4_addr);
}

static PP_Resource Pnacl_M29_PPB_NetAddress_CreateFromIPv6Address(PP_Instance instance, const struct PP_NetAddress_IPv6* ipv6_addr) {
  const struct PPB_NetAddress_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_1_0.real_iface;
  return iface->CreateFromIPv6Address(instance, ipv6_addr);
}

static PP_Bool Pnacl_M29_PPB_NetAddress_IsNetAddress(PP_Resource resource) {
  const struct PPB_NetAddress_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_1_0.real_iface;
  return iface->IsNetAddress(resource);
}

static PP_NetAddress_Family Pnacl_M29_PPB_NetAddress_GetFamily(PP_Resource addr) {
  const struct PPB_NetAddress_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_1_0.real_iface;
  return iface->GetFamily(addr);
}

static void Pnacl_M29_PPB_NetAddress_DescribeAsString(struct PP_Var* _struct_result, PP_Resource addr, PP_Bool include_port) {
  const struct PPB_NetAddress_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_1_0.real_iface;
  *_struct_result = iface->DescribeAsString(addr, include_port);
}

static PP_Bool Pnacl_M29_PPB_NetAddress_DescribeAsIPv4Address(PP_Resource addr, struct PP_NetAddress_IPv4* ipv4_addr) {
  const struct PPB_NetAddress_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_1_0.real_iface;
  return iface->DescribeAsIPv4Address(addr, ipv4_addr);
}

static PP_Bool Pnacl_M29_PPB_NetAddress_DescribeAsIPv6Address(PP_Resource addr, struct PP_NetAddress_IPv6* ipv6_addr) {
  const struct PPB_NetAddress_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_1_0.real_iface;
  return iface->DescribeAsIPv6Address(addr, ipv6_addr);
}

/* End wrapper methods for PPB_NetAddress_1_0 */

/* Begin wrapper methods for PPB_NetworkList_1_0 */

static PP_Bool Pnacl_M31_PPB_NetworkList_IsNetworkList(PP_Resource resource) {
  const struct PPB_NetworkList_1_0 *iface = Pnacl_WrapperInfo_PPB_NetworkList_1_0.real_iface;
  return iface->IsNetworkList(resource);
}

static uint32_t Pnacl_M31_PPB_NetworkList_GetCount(PP_Resource resource) {
  const struct PPB_NetworkList_1_0 *iface = Pnacl_WrapperInfo_PPB_NetworkList_1_0.real_iface;
  return iface->GetCount(resource);
}

static void Pnacl_M31_PPB_NetworkList_GetName(struct PP_Var* _struct_result, PP_Resource resource, uint32_t index) {
  const struct PPB_NetworkList_1_0 *iface = Pnacl_WrapperInfo_PPB_NetworkList_1_0.real_iface;
  *_struct_result = iface->GetName(resource, index);
}

static PP_NetworkList_Type Pnacl_M31_PPB_NetworkList_GetType(PP_Resource resource, uint32_t index) {
  const struct PPB_NetworkList_1_0 *iface = Pnacl_WrapperInfo_PPB_NetworkList_1_0.real_iface;
  return iface->GetType(resource, index);
}

static PP_NetworkList_State Pnacl_M31_PPB_NetworkList_GetState(PP_Resource resource, uint32_t index) {
  const struct PPB_NetworkList_1_0 *iface = Pnacl_WrapperInfo_PPB_NetworkList_1_0.real_iface;
  return iface->GetState(resource, index);
}

static int32_t Pnacl_M31_PPB_NetworkList_GetIpAddresses(PP_Resource resource, uint32_t index, struct PP_ArrayOutput* output) {
  const struct PPB_NetworkList_1_0 *iface = Pnacl_WrapperInfo_PPB_NetworkList_1_0.real_iface;
  return iface->GetIpAddresses(resource, index, *output);
}

static void Pnacl_M31_PPB_NetworkList_GetDisplayName(struct PP_Var* _struct_result, PP_Resource resource, uint32_t index) {
  const struct PPB_NetworkList_1_0 *iface = Pnacl_WrapperInfo_PPB_NetworkList_1_0.real_iface;
  *_struct_result = iface->GetDisplayName(resource, index);
}

static uint32_t Pnacl_M31_PPB_NetworkList_GetMTU(PP_Resource resource, uint32_t index) {
  const struct PPB_NetworkList_1_0 *iface = Pnacl_WrapperInfo_PPB_NetworkList_1_0.real_iface;
  return iface->GetMTU(resource, index);
}

/* End wrapper methods for PPB_NetworkList_1_0 */

/* Begin wrapper methods for PPB_NetworkMonitor_1_0 */

static PP_Resource Pnacl_M31_PPB_NetworkMonitor_Create(PP_Instance instance) {
  const struct PPB_NetworkMonitor_1_0 *iface = Pnacl_WrapperInfo_PPB_NetworkMonitor_1_0.real_iface;
  return iface->Create(instance);
}

static int32_t Pnacl_M31_PPB_NetworkMonitor_UpdateNetworkList(PP_Resource network_monitor, PP_Resource* network_list, struct PP_CompletionCallback* callback) {
  const struct PPB_NetworkMonitor_1_0 *iface = Pnacl_WrapperInfo_PPB_NetworkMonitor_1_0.real_iface;
  return iface->UpdateNetworkList(network_monitor, network_list, *callback);
}

static PP_Bool Pnacl_M31_PPB_NetworkMonitor_IsNetworkMonitor(PP_Resource resource) {
  const struct PPB_NetworkMonitor_1_0 *iface = Pnacl_WrapperInfo_PPB_NetworkMonitor_1_0.real_iface;
  return iface->IsNetworkMonitor(resource);
}

/* End wrapper methods for PPB_NetworkMonitor_1_0 */

/* Begin wrapper methods for PPB_NetworkProxy_1_0 */

static int32_t Pnacl_M29_PPB_NetworkProxy_GetProxyForURL(PP_Instance instance, struct PP_Var* url, struct PP_Var* proxy_string, struct PP_CompletionCallback* callback) {
  const struct PPB_NetworkProxy_1_0 *iface = Pnacl_WrapperInfo_PPB_NetworkProxy_1_0.real_iface;
  return iface->GetProxyForURL(instance, *url, proxy_string, *callback);
}

/* End wrapper methods for PPB_NetworkProxy_1_0 */

/* Not generating wrapper methods for PPB_OpenGLES2_1_0 */

/* Not generating wrapper methods for PPB_OpenGLES2InstancedArrays_1_0 */

/* Not generating wrapper methods for PPB_OpenGLES2FramebufferBlit_1_0 */

/* Not generating wrapper methods for PPB_OpenGLES2FramebufferMultisample_1_0 */

/* Not generating wrapper methods for PPB_OpenGLES2ChromiumEnableFeature_1_0 */

/* Not generating wrapper methods for PPB_OpenGLES2ChromiumMapSub_1_0 */

/* Not generating wrapper methods for PPB_OpenGLES2Query_1_0 */

/* Not generating wrapper methods for PPB_OpenGLES2VertexArrayObject_1_0 */

/* Begin wrapper methods for PPB_TCPSocket_1_0 */

static PP_Resource Pnacl_M29_PPB_TCPSocket_Create(PP_Instance instance) {
  const struct PPB_TCPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_0.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M29_PPB_TCPSocket_IsTCPSocket(PP_Resource resource) {
  const struct PPB_TCPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_0.real_iface;
  return iface->IsTCPSocket(resource);
}

static int32_t Pnacl_M29_PPB_TCPSocket_Connect(PP_Resource tcp_socket, PP_Resource addr, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_0.real_iface;
  return iface->Connect(tcp_socket, addr, *callback);
}

static PP_Resource Pnacl_M29_PPB_TCPSocket_GetLocalAddress(PP_Resource tcp_socket) {
  const struct PPB_TCPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_0.real_iface;
  return iface->GetLocalAddress(tcp_socket);
}

static PP_Resource Pnacl_M29_PPB_TCPSocket_GetRemoteAddress(PP_Resource tcp_socket) {
  const struct PPB_TCPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_0.real_iface;
  return iface->GetRemoteAddress(tcp_socket);
}

static int32_t Pnacl_M29_PPB_TCPSocket_Read(PP_Resource tcp_socket, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_0.real_iface;
  return iface->Read(tcp_socket, buffer, bytes_to_read, *callback);
}

static int32_t Pnacl_M29_PPB_TCPSocket_Write(PP_Resource tcp_socket, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_0.real_iface;
  return iface->Write(tcp_socket, buffer, bytes_to_write, *callback);
}

static void Pnacl_M29_PPB_TCPSocket_Close(PP_Resource tcp_socket) {
  const struct PPB_TCPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_0.real_iface;
  iface->Close(tcp_socket);
}

static int32_t Pnacl_M29_PPB_TCPSocket_SetOption(PP_Resource tcp_socket, PP_TCPSocket_Option name, struct PP_Var* value, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_0.real_iface;
  return iface->SetOption(tcp_socket, name, *value, *callback);
}

/* End wrapper methods for PPB_TCPSocket_1_0 */

/* Begin wrapper methods for PPB_TCPSocket_1_1 */

static PP_Resource Pnacl_M31_PPB_TCPSocket_Create(PP_Instance instance) {
  const struct PPB_TCPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_1.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M31_PPB_TCPSocket_IsTCPSocket(PP_Resource resource) {
  const struct PPB_TCPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_1.real_iface;
  return iface->IsTCPSocket(resource);
}

static int32_t Pnacl_M31_PPB_TCPSocket_Bind(PP_Resource tcp_socket, PP_Resource addr, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_1.real_iface;
  return iface->Bind(tcp_socket, addr, *callback);
}

static int32_t Pnacl_M31_PPB_TCPSocket_Connect(PP_Resource tcp_socket, PP_Resource addr, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_1.real_iface;
  return iface->Connect(tcp_socket, addr, *callback);
}

static PP_Resource Pnacl_M31_PPB_TCPSocket_GetLocalAddress(PP_Resource tcp_socket) {
  const struct PPB_TCPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_1.real_iface;
  return iface->GetLocalAddress(tcp_socket);
}

static PP_Resource Pnacl_M31_PPB_TCPSocket_GetRemoteAddress(PP_Resource tcp_socket) {
  const struct PPB_TCPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_1.real_iface;
  return iface->GetRemoteAddress(tcp_socket);
}

static int32_t Pnacl_M31_PPB_TCPSocket_Read(PP_Resource tcp_socket, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_1.real_iface;
  return iface->Read(tcp_socket, buffer, bytes_to_read, *callback);
}

static int32_t Pnacl_M31_PPB_TCPSocket_Write(PP_Resource tcp_socket, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_1.real_iface;
  return iface->Write(tcp_socket, buffer, bytes_to_write, *callback);
}

static int32_t Pnacl_M31_PPB_TCPSocket_Listen(PP_Resource tcp_socket, int32_t backlog, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_1.real_iface;
  return iface->Listen(tcp_socket, backlog, *callback);
}

static int32_t Pnacl_M31_PPB_TCPSocket_Accept(PP_Resource tcp_socket, PP_Resource* accepted_tcp_socket, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_1.real_iface;
  return iface->Accept(tcp_socket, accepted_tcp_socket, *callback);
}

static void Pnacl_M31_PPB_TCPSocket_Close(PP_Resource tcp_socket) {
  const struct PPB_TCPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_1.real_iface;
  iface->Close(tcp_socket);
}

static int32_t Pnacl_M31_PPB_TCPSocket_SetOption(PP_Resource tcp_socket, PP_TCPSocket_Option name, struct PP_Var* value, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_1.real_iface;
  return iface->SetOption(tcp_socket, name, *value, *callback);
}

/* End wrapper methods for PPB_TCPSocket_1_1 */

/* Begin wrapper methods for PPB_TCPSocket_1_2 */

static PP_Resource Pnacl_M41_PPB_TCPSocket_Create(PP_Instance instance) {
  const struct PPB_TCPSocket_1_2 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_2.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M41_PPB_TCPSocket_IsTCPSocket(PP_Resource resource) {
  const struct PPB_TCPSocket_1_2 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_2.real_iface;
  return iface->IsTCPSocket(resource);
}

static int32_t Pnacl_M41_PPB_TCPSocket_Bind(PP_Resource tcp_socket, PP_Resource addr, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_2 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_2.real_iface;
  return iface->Bind(tcp_socket, addr, *callback);
}

static int32_t Pnacl_M41_PPB_TCPSocket_Connect(PP_Resource tcp_socket, PP_Resource addr, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_2 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_2.real_iface;
  return iface->Connect(tcp_socket, addr, *callback);
}

static PP_Resource Pnacl_M41_PPB_TCPSocket_GetLocalAddress(PP_Resource tcp_socket) {
  const struct PPB_TCPSocket_1_2 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_2.real_iface;
  return iface->GetLocalAddress(tcp_socket);
}

static PP_Resource Pnacl_M41_PPB_TCPSocket_GetRemoteAddress(PP_Resource tcp_socket) {
  const struct PPB_TCPSocket_1_2 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_2.real_iface;
  return iface->GetRemoteAddress(tcp_socket);
}

static int32_t Pnacl_M41_PPB_TCPSocket_Read(PP_Resource tcp_socket, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_2 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_2.real_iface;
  return iface->Read(tcp_socket, buffer, bytes_to_read, *callback);
}

static int32_t Pnacl_M41_PPB_TCPSocket_Write(PP_Resource tcp_socket, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_2 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_2.real_iface;
  return iface->Write(tcp_socket, buffer, bytes_to_write, *callback);
}

static int32_t Pnacl_M41_PPB_TCPSocket_Listen(PP_Resource tcp_socket, int32_t backlog, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_2 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_2.real_iface;
  return iface->Listen(tcp_socket, backlog, *callback);
}

static int32_t Pnacl_M41_PPB_TCPSocket_Accept(PP_Resource tcp_socket, PP_Resource* accepted_tcp_socket, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_2 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_2.real_iface;
  return iface->Accept(tcp_socket, accepted_tcp_socket, *callback);
}

static void Pnacl_M41_PPB_TCPSocket_Close(PP_Resource tcp_socket) {
  const struct PPB_TCPSocket_1_2 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_2.real_iface;
  iface->Close(tcp_socket);
}

static int32_t Pnacl_M41_PPB_TCPSocket_SetOption(PP_Resource tcp_socket, PP_TCPSocket_Option name, struct PP_Var* value, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_1_2 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_1_2.real_iface;
  return iface->SetOption(tcp_socket, name, *value, *callback);
}

/* End wrapper methods for PPB_TCPSocket_1_2 */

/* Begin wrapper methods for PPB_TextInputController_1_0 */

static void Pnacl_M30_PPB_TextInputController_SetTextInputType(PP_Instance instance, PP_TextInput_Type type) {
  const struct PPB_TextInputController_1_0 *iface = Pnacl_WrapperInfo_PPB_TextInputController_1_0.real_iface;
  iface->SetTextInputType(instance, type);
}

static void Pnacl_M30_PPB_TextInputController_UpdateCaretPosition(PP_Instance instance, const struct PP_Rect* caret) {
  const struct PPB_TextInputController_1_0 *iface = Pnacl_WrapperInfo_PPB_TextInputController_1_0.real_iface;
  iface->UpdateCaretPosition(instance, caret);
}

static void Pnacl_M30_PPB_TextInputController_CancelCompositionText(PP_Instance instance) {
  const struct PPB_TextInputController_1_0 *iface = Pnacl_WrapperInfo_PPB_TextInputController_1_0.real_iface;
  iface->CancelCompositionText(instance);
}

static void Pnacl_M30_PPB_TextInputController_UpdateSurroundingText(PP_Instance instance, struct PP_Var* text, uint32_t caret, uint32_t anchor) {
  const struct PPB_TextInputController_1_0 *iface = Pnacl_WrapperInfo_PPB_TextInputController_1_0.real_iface;
  iface->UpdateSurroundingText(instance, *text, caret, anchor);
}

/* End wrapper methods for PPB_TextInputController_1_0 */

/* Begin wrapper methods for PPB_UDPSocket_1_0 */

static PP_Resource Pnacl_M29_PPB_UDPSocket_Create(PP_Instance instance) {
  const struct PPB_UDPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_0.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M29_PPB_UDPSocket_IsUDPSocket(PP_Resource resource) {
  const struct PPB_UDPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_0.real_iface;
  return iface->IsUDPSocket(resource);
}

static int32_t Pnacl_M29_PPB_UDPSocket_Bind(PP_Resource udp_socket, PP_Resource addr, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_0.real_iface;
  return iface->Bind(udp_socket, addr, *callback);
}

static PP_Resource Pnacl_M29_PPB_UDPSocket_GetBoundAddress(PP_Resource udp_socket) {
  const struct PPB_UDPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_0.real_iface;
  return iface->GetBoundAddress(udp_socket);
}

static int32_t Pnacl_M29_PPB_UDPSocket_RecvFrom(PP_Resource udp_socket, char* buffer, int32_t num_bytes, PP_Resource* addr, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_0.real_iface;
  return iface->RecvFrom(udp_socket, buffer, num_bytes, addr, *callback);
}

static int32_t Pnacl_M29_PPB_UDPSocket_SendTo(PP_Resource udp_socket, const char* buffer, int32_t num_bytes, PP_Resource addr, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_0.real_iface;
  return iface->SendTo(udp_socket, buffer, num_bytes, addr, *callback);
}

static void Pnacl_M29_PPB_UDPSocket_Close(PP_Resource udp_socket) {
  const struct PPB_UDPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_0.real_iface;
  iface->Close(udp_socket);
}

static int32_t Pnacl_M29_PPB_UDPSocket_SetOption(PP_Resource udp_socket, PP_UDPSocket_Option name, struct PP_Var* value, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_0.real_iface;
  return iface->SetOption(udp_socket, name, *value, *callback);
}

/* End wrapper methods for PPB_UDPSocket_1_0 */

/* Begin wrapper methods for PPB_UDPSocket_1_1 */

static PP_Resource Pnacl_M41_PPB_UDPSocket_Create(PP_Instance instance) {
  const struct PPB_UDPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_1.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M41_PPB_UDPSocket_IsUDPSocket(PP_Resource resource) {
  const struct PPB_UDPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_1.real_iface;
  return iface->IsUDPSocket(resource);
}

static int32_t Pnacl_M41_PPB_UDPSocket_Bind(PP_Resource udp_socket, PP_Resource addr, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_1.real_iface;
  return iface->Bind(udp_socket, addr, *callback);
}

static PP_Resource Pnacl_M41_PPB_UDPSocket_GetBoundAddress(PP_Resource udp_socket) {
  const struct PPB_UDPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_1.real_iface;
  return iface->GetBoundAddress(udp_socket);
}

static int32_t Pnacl_M41_PPB_UDPSocket_RecvFrom(PP_Resource udp_socket, char* buffer, int32_t num_bytes, PP_Resource* addr, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_1.real_iface;
  return iface->RecvFrom(udp_socket, buffer, num_bytes, addr, *callback);
}

static int32_t Pnacl_M41_PPB_UDPSocket_SendTo(PP_Resource udp_socket, const char* buffer, int32_t num_bytes, PP_Resource addr, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_1.real_iface;
  return iface->SendTo(udp_socket, buffer, num_bytes, addr, *callback);
}

static void Pnacl_M41_PPB_UDPSocket_Close(PP_Resource udp_socket) {
  const struct PPB_UDPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_1.real_iface;
  iface->Close(udp_socket);
}

static int32_t Pnacl_M41_PPB_UDPSocket_SetOption(PP_Resource udp_socket, PP_UDPSocket_Option name, struct PP_Var* value, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_1_1 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_1.real_iface;
  return iface->SetOption(udp_socket, name, *value, *callback);
}

/* End wrapper methods for PPB_UDPSocket_1_1 */

/* Begin wrapper methods for PPB_UDPSocket_1_2 */

static PP_Resource Pnacl_M43_PPB_UDPSocket_Create(PP_Instance instance) {
  const struct PPB_UDPSocket_1_2 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_2.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M43_PPB_UDPSocket_IsUDPSocket(PP_Resource resource) {
  const struct PPB_UDPSocket_1_2 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_2.real_iface;
  return iface->IsUDPSocket(resource);
}

static int32_t Pnacl_M43_PPB_UDPSocket_Bind(PP_Resource udp_socket, PP_Resource addr, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_1_2 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_2.real_iface;
  return iface->Bind(udp_socket, addr, *callback);
}

static PP_Resource Pnacl_M43_PPB_UDPSocket_GetBoundAddress(PP_Resource udp_socket) {
  const struct PPB_UDPSocket_1_2 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_2.real_iface;
  return iface->GetBoundAddress(udp_socket);
}

static int32_t Pnacl_M43_PPB_UDPSocket_RecvFrom(PP_Resource udp_socket, char* buffer, int32_t num_bytes, PP_Resource* addr, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_1_2 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_2.real_iface;
  return iface->RecvFrom(udp_socket, buffer, num_bytes, addr, *callback);
}

static int32_t Pnacl_M43_PPB_UDPSocket_SendTo(PP_Resource udp_socket, const char* buffer, int32_t num_bytes, PP_Resource addr, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_1_2 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_2.real_iface;
  return iface->SendTo(udp_socket, buffer, num_bytes, addr, *callback);
}

static void Pnacl_M43_PPB_UDPSocket_Close(PP_Resource udp_socket) {
  const struct PPB_UDPSocket_1_2 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_2.real_iface;
  iface->Close(udp_socket);
}

static int32_t Pnacl_M43_PPB_UDPSocket_SetOption(PP_Resource udp_socket, PP_UDPSocket_Option name, struct PP_Var* value, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_1_2 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_2.real_iface;
  return iface->SetOption(udp_socket, name, *value, *callback);
}

static int32_t Pnacl_M43_PPB_UDPSocket_JoinGroup(PP_Resource udp_socket, PP_Resource group, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_1_2 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_2.real_iface;
  return iface->JoinGroup(udp_socket, group, *callback);
}

static int32_t Pnacl_M43_PPB_UDPSocket_LeaveGroup(PP_Resource udp_socket, PP_Resource group, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_1_2 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_1_2.real_iface;
  return iface->LeaveGroup(udp_socket, group, *callback);
}

/* End wrapper methods for PPB_UDPSocket_1_2 */

/* Begin wrapper methods for PPB_URLLoader_1_0 */

static PP_Resource Pnacl_M14_PPB_URLLoader_Create(PP_Instance instance) {
  const struct PPB_URLLoader_1_0 *iface = Pnacl_WrapperInfo_PPB_URLLoader_1_0.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M14_PPB_URLLoader_IsURLLoader(PP_Resource resource) {
  const struct PPB_URLLoader_1_0 *iface = Pnacl_WrapperInfo_PPB_URLLoader_1_0.real_iface;
  return iface->IsURLLoader(resource);
}

static int32_t Pnacl_M14_PPB_URLLoader_Open(PP_Resource loader, PP_Resource request_info, struct PP_CompletionCallback* callback) {
  const struct PPB_URLLoader_1_0 *iface = Pnacl_WrapperInfo_PPB_URLLoader_1_0.real_iface;
  return iface->Open(loader, request_info, *callback);
}

static int32_t Pnacl_M14_PPB_URLLoader_FollowRedirect(PP_Resource loader, struct PP_CompletionCallback* callback) {
  const struct PPB_URLLoader_1_0 *iface = Pnacl_WrapperInfo_PPB_URLLoader_1_0.real_iface;
  return iface->FollowRedirect(loader, *callback);
}

static PP_Bool Pnacl_M14_PPB_URLLoader_GetUploadProgress(PP_Resource loader, int64_t* bytes_sent, int64_t* total_bytes_to_be_sent) {
  const struct PPB_URLLoader_1_0 *iface = Pnacl_WrapperInfo_PPB_URLLoader_1_0.real_iface;
  return iface->GetUploadProgress(loader, bytes_sent, total_bytes_to_be_sent);
}

static PP_Bool Pnacl_M14_PPB_URLLoader_GetDownloadProgress(PP_Resource loader, int64_t* bytes_received, int64_t* total_bytes_to_be_received) {
  const struct PPB_URLLoader_1_0 *iface = Pnacl_WrapperInfo_PPB_URLLoader_1_0.real_iface;
  return iface->GetDownloadProgress(loader, bytes_received, total_bytes_to_be_received);
}

static PP_Resource Pnacl_M14_PPB_URLLoader_GetResponseInfo(PP_Resource loader) {
  const struct PPB_URLLoader_1_0 *iface = Pnacl_WrapperInfo_PPB_URLLoader_1_0.real_iface;
  return iface->GetResponseInfo(loader);
}

static int32_t Pnacl_M14_PPB_URLLoader_ReadResponseBody(PP_Resource loader, void* buffer, int32_t bytes_to_read, struct PP_CompletionCallback* callback) {
  const struct PPB_URLLoader_1_0 *iface = Pnacl_WrapperInfo_PPB_URLLoader_1_0.real_iface;
  return iface->ReadResponseBody(loader, buffer, bytes_to_read, *callback);
}

static int32_t Pnacl_M14_PPB_URLLoader_FinishStreamingToFile(PP_Resource loader, struct PP_CompletionCallback* callback) {
  const struct PPB_URLLoader_1_0 *iface = Pnacl_WrapperInfo_PPB_URLLoader_1_0.real_iface;
  return iface->FinishStreamingToFile(loader, *callback);
}

static void Pnacl_M14_PPB_URLLoader_Close(PP_Resource loader) {
  const struct PPB_URLLoader_1_0 *iface = Pnacl_WrapperInfo_PPB_URLLoader_1_0.real_iface;
  iface->Close(loader);
}

/* End wrapper methods for PPB_URLLoader_1_0 */

/* Begin wrapper methods for PPB_URLRequestInfo_1_0 */

static PP_Resource Pnacl_M14_PPB_URLRequestInfo_Create(PP_Instance instance) {
  const struct PPB_URLRequestInfo_1_0 *iface = Pnacl_WrapperInfo_PPB_URLRequestInfo_1_0.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M14_PPB_URLRequestInfo_IsURLRequestInfo(PP_Resource resource) {
  const struct PPB_URLRequestInfo_1_0 *iface = Pnacl_WrapperInfo_PPB_URLRequestInfo_1_0.real_iface;
  return iface->IsURLRequestInfo(resource);
}

static PP_Bool Pnacl_M14_PPB_URLRequestInfo_SetProperty(PP_Resource request, PP_URLRequestProperty property, struct PP_Var* value) {
  const struct PPB_URLRequestInfo_1_0 *iface = Pnacl_WrapperInfo_PPB_URLRequestInfo_1_0.real_iface;
  return iface->SetProperty(request, property, *value);
}

static PP_Bool Pnacl_M14_PPB_URLRequestInfo_AppendDataToBody(PP_Resource request, const void* data, uint32_t len) {
  const struct PPB_URLRequestInfo_1_0 *iface = Pnacl_WrapperInfo_PPB_URLRequestInfo_1_0.real_iface;
  return iface->AppendDataToBody(request, data, len);
}

static PP_Bool Pnacl_M14_PPB_URLRequestInfo_AppendFileToBody(PP_Resource request, PP_Resource file_ref, int64_t start_offset, int64_t number_of_bytes, PP_Time expected_last_modified_time) {
  const struct PPB_URLRequestInfo_1_0 *iface = Pnacl_WrapperInfo_PPB_URLRequestInfo_1_0.real_iface;
  return iface->AppendFileToBody(request, file_ref, start_offset, number_of_bytes, expected_last_modified_time);
}

/* End wrapper methods for PPB_URLRequestInfo_1_0 */

/* Begin wrapper methods for PPB_URLResponseInfo_1_0 */

static PP_Bool Pnacl_M14_PPB_URLResponseInfo_IsURLResponseInfo(PP_Resource resource) {
  const struct PPB_URLResponseInfo_1_0 *iface = Pnacl_WrapperInfo_PPB_URLResponseInfo_1_0.real_iface;
  return iface->IsURLResponseInfo(resource);
}

static void Pnacl_M14_PPB_URLResponseInfo_GetProperty(struct PP_Var* _struct_result, PP_Resource response, PP_URLResponseProperty property) {
  const struct PPB_URLResponseInfo_1_0 *iface = Pnacl_WrapperInfo_PPB_URLResponseInfo_1_0.real_iface;
  *_struct_result = iface->GetProperty(response, property);
}

static PP_Resource Pnacl_M14_PPB_URLResponseInfo_GetBodyAsFileRef(PP_Resource response) {
  const struct PPB_URLResponseInfo_1_0 *iface = Pnacl_WrapperInfo_PPB_URLResponseInfo_1_0.real_iface;
  return iface->GetBodyAsFileRef(response);
}

/* End wrapper methods for PPB_URLResponseInfo_1_0 */

/* Begin wrapper methods for PPB_Var_1_0 */

static void Pnacl_M14_PPB_Var_AddRef(struct PP_Var* var) {
  const struct PPB_Var_1_0 *iface = Pnacl_WrapperInfo_PPB_Var_1_0.real_iface;
  iface->AddRef(*var);
}

static void Pnacl_M14_PPB_Var_Release(struct PP_Var* var) {
  const struct PPB_Var_1_0 *iface = Pnacl_WrapperInfo_PPB_Var_1_0.real_iface;
  iface->Release(*var);
}

static void Pnacl_M14_PPB_Var_VarFromUtf8(struct PP_Var* _struct_result, PP_Module module, const char* data, uint32_t len) {
  const struct PPB_Var_1_0 *iface = Pnacl_WrapperInfo_PPB_Var_1_0.real_iface;
  *_struct_result = iface->VarFromUtf8(module, data, len);
}

static const char* Pnacl_M14_PPB_Var_VarToUtf8(struct PP_Var* var, uint32_t* len) {
  const struct PPB_Var_1_0 *iface = Pnacl_WrapperInfo_PPB_Var_1_0.real_iface;
  return iface->VarToUtf8(*var, len);
}

/* End wrapper methods for PPB_Var_1_0 */

/* Begin wrapper methods for PPB_Var_1_1 */

static void Pnacl_M18_PPB_Var_AddRef(struct PP_Var* var) {
  const struct PPB_Var_1_1 *iface = Pnacl_WrapperInfo_PPB_Var_1_1.real_iface;
  iface->AddRef(*var);
}

static void Pnacl_M18_PPB_Var_Release(struct PP_Var* var) {
  const struct PPB_Var_1_1 *iface = Pnacl_WrapperInfo_PPB_Var_1_1.real_iface;
  iface->Release(*var);
}

static void Pnacl_M18_PPB_Var_VarFromUtf8(struct PP_Var* _struct_result, const char* data, uint32_t len) {
  const struct PPB_Var_1_1 *iface = Pnacl_WrapperInfo_PPB_Var_1_1.real_iface;
  *_struct_result = iface->VarFromUtf8(data, len);
}

static const char* Pnacl_M18_PPB_Var_VarToUtf8(struct PP_Var* var, uint32_t* len) {
  const struct PPB_Var_1_1 *iface = Pnacl_WrapperInfo_PPB_Var_1_1.real_iface;
  return iface->VarToUtf8(*var, len);
}

/* End wrapper methods for PPB_Var_1_1 */

/* Begin wrapper methods for PPB_Var_1_2 */

static void Pnacl_M34_PPB_Var_AddRef(struct PP_Var* var) {
  const struct PPB_Var_1_2 *iface = Pnacl_WrapperInfo_PPB_Var_1_2.real_iface;
  iface->AddRef(*var);
}

static void Pnacl_M34_PPB_Var_Release(struct PP_Var* var) {
  const struct PPB_Var_1_2 *iface = Pnacl_WrapperInfo_PPB_Var_1_2.real_iface;
  iface->Release(*var);
}

static void Pnacl_M34_PPB_Var_VarFromUtf8(struct PP_Var* _struct_result, const char* data, uint32_t len) {
  const struct PPB_Var_1_2 *iface = Pnacl_WrapperInfo_PPB_Var_1_2.real_iface;
  *_struct_result = iface->VarFromUtf8(data, len);
}

static const char* Pnacl_M34_PPB_Var_VarToUtf8(struct PP_Var* var, uint32_t* len) {
  const struct PPB_Var_1_2 *iface = Pnacl_WrapperInfo_PPB_Var_1_2.real_iface;
  return iface->VarToUtf8(*var, len);
}

static PP_Resource Pnacl_M34_PPB_Var_VarToResource(struct PP_Var* var) {
  const struct PPB_Var_1_2 *iface = Pnacl_WrapperInfo_PPB_Var_1_2.real_iface;
  return iface->VarToResource(*var);
}

static void Pnacl_M34_PPB_Var_VarFromResource(struct PP_Var* _struct_result, PP_Resource resource) {
  const struct PPB_Var_1_2 *iface = Pnacl_WrapperInfo_PPB_Var_1_2.real_iface;
  *_struct_result = iface->VarFromResource(resource);
}

/* End wrapper methods for PPB_Var_1_2 */

/* Begin wrapper methods for PPB_VarArray_1_0 */

static void Pnacl_M29_PPB_VarArray_Create(struct PP_Var* _struct_result) {
  const struct PPB_VarArray_1_0 *iface = Pnacl_WrapperInfo_PPB_VarArray_1_0.real_iface;
  *_struct_result = iface->Create();
}

static void Pnacl_M29_PPB_VarArray_Get(struct PP_Var* _struct_result, struct PP_Var* array, uint32_t index) {
  const struct PPB_VarArray_1_0 *iface = Pnacl_WrapperInfo_PPB_VarArray_1_0.real_iface;
  *_struct_result = iface->Get(*array, index);
}

static PP_Bool Pnacl_M29_PPB_VarArray_Set(struct PP_Var* array, uint32_t index, struct PP_Var* value) {
  const struct PPB_VarArray_1_0 *iface = Pnacl_WrapperInfo_PPB_VarArray_1_0.real_iface;
  return iface->Set(*array, index, *value);
}

static uint32_t Pnacl_M29_PPB_VarArray_GetLength(struct PP_Var* array) {
  const struct PPB_VarArray_1_0 *iface = Pnacl_WrapperInfo_PPB_VarArray_1_0.real_iface;
  return iface->GetLength(*array);
}

static PP_Bool Pnacl_M29_PPB_VarArray_SetLength(struct PP_Var* array, uint32_t length) {
  const struct PPB_VarArray_1_0 *iface = Pnacl_WrapperInfo_PPB_VarArray_1_0.real_iface;
  return iface->SetLength(*array, length);
}

/* End wrapper methods for PPB_VarArray_1_0 */

/* Begin wrapper methods for PPB_VarArrayBuffer_1_0 */

static void Pnacl_M18_PPB_VarArrayBuffer_Create(struct PP_Var* _struct_result, uint32_t size_in_bytes) {
  const struct PPB_VarArrayBuffer_1_0 *iface = Pnacl_WrapperInfo_PPB_VarArrayBuffer_1_0.real_iface;
  *_struct_result = iface->Create(size_in_bytes);
}

static PP_Bool Pnacl_M18_PPB_VarArrayBuffer_ByteLength(struct PP_Var* array, uint32_t* byte_length) {
  const struct PPB_VarArrayBuffer_1_0 *iface = Pnacl_WrapperInfo_PPB_VarArrayBuffer_1_0.real_iface;
  return iface->ByteLength(*array, byte_length);
}

static void* Pnacl_M18_PPB_VarArrayBuffer_Map(struct PP_Var* array) {
  const struct PPB_VarArrayBuffer_1_0 *iface = Pnacl_WrapperInfo_PPB_VarArrayBuffer_1_0.real_iface;
  return iface->Map(*array);
}

static void Pnacl_M18_PPB_VarArrayBuffer_Unmap(struct PP_Var* array) {
  const struct PPB_VarArrayBuffer_1_0 *iface = Pnacl_WrapperInfo_PPB_VarArrayBuffer_1_0.real_iface;
  iface->Unmap(*array);
}

/* End wrapper methods for PPB_VarArrayBuffer_1_0 */

/* Begin wrapper methods for PPB_VarDictionary_1_0 */

static void Pnacl_M29_PPB_VarDictionary_Create(struct PP_Var* _struct_result) {
  const struct PPB_VarDictionary_1_0 *iface = Pnacl_WrapperInfo_PPB_VarDictionary_1_0.real_iface;
  *_struct_result = iface->Create();
}

static void Pnacl_M29_PPB_VarDictionary_Get(struct PP_Var* _struct_result, struct PP_Var* dict, struct PP_Var* key) {
  const struct PPB_VarDictionary_1_0 *iface = Pnacl_WrapperInfo_PPB_VarDictionary_1_0.real_iface;
  *_struct_result = iface->Get(*dict, *key);
}

static PP_Bool Pnacl_M29_PPB_VarDictionary_Set(struct PP_Var* dict, struct PP_Var* key, struct PP_Var* value) {
  const struct PPB_VarDictionary_1_0 *iface = Pnacl_WrapperInfo_PPB_VarDictionary_1_0.real_iface;
  return iface->Set(*dict, *key, *value);
}

static void Pnacl_M29_PPB_VarDictionary_Delete(struct PP_Var* dict, struct PP_Var* key) {
  const struct PPB_VarDictionary_1_0 *iface = Pnacl_WrapperInfo_PPB_VarDictionary_1_0.real_iface;
  iface->Delete(*dict, *key);
}

static PP_Bool Pnacl_M29_PPB_VarDictionary_HasKey(struct PP_Var* dict, struct PP_Var* key) {
  const struct PPB_VarDictionary_1_0 *iface = Pnacl_WrapperInfo_PPB_VarDictionary_1_0.real_iface;
  return iface->HasKey(*dict, *key);
}

static void Pnacl_M29_PPB_VarDictionary_GetKeys(struct PP_Var* _struct_result, struct PP_Var* dict) {
  const struct PPB_VarDictionary_1_0 *iface = Pnacl_WrapperInfo_PPB_VarDictionary_1_0.real_iface;
  *_struct_result = iface->GetKeys(*dict);
}

/* End wrapper methods for PPB_VarDictionary_1_0 */

/* Begin wrapper methods for PPB_VideoDecoder_0_1 */

static PP_Resource Pnacl_M36_PPB_VideoDecoder_Create(PP_Instance instance) {
  const struct PPB_VideoDecoder_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_0_1.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M36_PPB_VideoDecoder_IsVideoDecoder(PP_Resource resource) {
  const struct PPB_VideoDecoder_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_0_1.real_iface;
  return iface->IsVideoDecoder(resource);
}

static int32_t Pnacl_M36_PPB_VideoDecoder_Initialize(PP_Resource video_decoder, PP_Resource graphics3d_context, PP_VideoProfile profile, PP_Bool allow_software_fallback, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDecoder_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_0_1.real_iface;
  return iface->Initialize(video_decoder, graphics3d_context, profile, allow_software_fallback, *callback);
}

static int32_t Pnacl_M36_PPB_VideoDecoder_Decode(PP_Resource video_decoder, uint32_t decode_id, uint32_t size, const void* buffer, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDecoder_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_0_1.real_iface;
  return iface->Decode(video_decoder, decode_id, size, buffer, *callback);
}

static int32_t Pnacl_M36_PPB_VideoDecoder_GetPicture(PP_Resource video_decoder, struct PP_VideoPicture_0_1* picture, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDecoder_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_0_1.real_iface;
  return iface->GetPicture(video_decoder, picture, *callback);
}

static void Pnacl_M36_PPB_VideoDecoder_RecyclePicture(PP_Resource video_decoder, const struct PP_VideoPicture* picture) {
  const struct PPB_VideoDecoder_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_0_1.real_iface;
  iface->RecyclePicture(video_decoder, picture);
}

static int32_t Pnacl_M36_PPB_VideoDecoder_Flush(PP_Resource video_decoder, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDecoder_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_0_1.real_iface;
  return iface->Flush(video_decoder, *callback);
}

static int32_t Pnacl_M36_PPB_VideoDecoder_Reset(PP_Resource video_decoder, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDecoder_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_0_1.real_iface;
  return iface->Reset(video_decoder, *callback);
}

/* End wrapper methods for PPB_VideoDecoder_0_1 */

/* Begin wrapper methods for PPB_VideoDecoder_0_2 */

static PP_Resource Pnacl_M39_PPB_VideoDecoder_Create(PP_Instance instance) {
  const struct PPB_VideoDecoder_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_0_2.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M39_PPB_VideoDecoder_IsVideoDecoder(PP_Resource resource) {
  const struct PPB_VideoDecoder_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_0_2.real_iface;
  return iface->IsVideoDecoder(resource);
}

static int32_t Pnacl_M39_PPB_VideoDecoder_Initialize(PP_Resource video_decoder, PP_Resource graphics3d_context, PP_VideoProfile profile, PP_HardwareAcceleration acceleration, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDecoder_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_0_2.real_iface;
  return iface->Initialize(video_decoder, graphics3d_context, profile, acceleration, *callback);
}

static int32_t Pnacl_M39_PPB_VideoDecoder_Decode(PP_Resource video_decoder, uint32_t decode_id, uint32_t size, const void* buffer, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDecoder_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_0_2.real_iface;
  return iface->Decode(video_decoder, decode_id, size, buffer, *callback);
}

static int32_t Pnacl_M39_PPB_VideoDecoder_GetPicture(PP_Resource video_decoder, struct PP_VideoPicture_0_1* picture, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDecoder_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_0_2.real_iface;
  return iface->GetPicture(video_decoder, picture, *callback);
}

static void Pnacl_M39_PPB_VideoDecoder_RecyclePicture(PP_Resource video_decoder, const struct PP_VideoPicture* picture) {
  const struct PPB_VideoDecoder_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_0_2.real_iface;
  iface->RecyclePicture(video_decoder, picture);
}

static int32_t Pnacl_M39_PPB_VideoDecoder_Flush(PP_Resource video_decoder, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDecoder_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_0_2.real_iface;
  return iface->Flush(video_decoder, *callback);
}

static int32_t Pnacl_M39_PPB_VideoDecoder_Reset(PP_Resource video_decoder, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDecoder_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_0_2.real_iface;
  return iface->Reset(video_decoder, *callback);
}

/* End wrapper methods for PPB_VideoDecoder_0_2 */

/* Begin wrapper methods for PPB_VideoDecoder_1_0 */

static PP_Resource Pnacl_M40_PPB_VideoDecoder_Create(PP_Instance instance) {
  const struct PPB_VideoDecoder_1_0 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_1_0.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M40_PPB_VideoDecoder_IsVideoDecoder(PP_Resource resource) {
  const struct PPB_VideoDecoder_1_0 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_1_0.real_iface;
  return iface->IsVideoDecoder(resource);
}

static int32_t Pnacl_M40_PPB_VideoDecoder_Initialize(PP_Resource video_decoder, PP_Resource graphics3d_context, PP_VideoProfile profile, PP_HardwareAcceleration acceleration, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDecoder_1_0 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_1_0.real_iface;
  return iface->Initialize(video_decoder, graphics3d_context, profile, acceleration, *callback);
}

static int32_t Pnacl_M40_PPB_VideoDecoder_Decode(PP_Resource video_decoder, uint32_t decode_id, uint32_t size, const void* buffer, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDecoder_1_0 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_1_0.real_iface;
  return iface->Decode(video_decoder, decode_id, size, buffer, *callback);
}

static int32_t Pnacl_M40_PPB_VideoDecoder_GetPicture(PP_Resource video_decoder, struct PP_VideoPicture* picture, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDecoder_1_0 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_1_0.real_iface;
  return iface->GetPicture(video_decoder, picture, *callback);
}

static void Pnacl_M40_PPB_VideoDecoder_RecyclePicture(PP_Resource video_decoder, const struct PP_VideoPicture* picture) {
  const struct PPB_VideoDecoder_1_0 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_1_0.real_iface;
  iface->RecyclePicture(video_decoder, picture);
}

static int32_t Pnacl_M40_PPB_VideoDecoder_Flush(PP_Resource video_decoder, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDecoder_1_0 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_1_0.real_iface;
  return iface->Flush(video_decoder, *callback);
}

static int32_t Pnacl_M40_PPB_VideoDecoder_Reset(PP_Resource video_decoder, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDecoder_1_0 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_1_0.real_iface;
  return iface->Reset(video_decoder, *callback);
}

/* End wrapper methods for PPB_VideoDecoder_1_0 */

/* Begin wrapper methods for PPB_VideoDecoder_1_1 */

static PP_Resource Pnacl_M46_PPB_VideoDecoder_Create(PP_Instance instance) {
  const struct PPB_VideoDecoder_1_1 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_1_1.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M46_PPB_VideoDecoder_IsVideoDecoder(PP_Resource resource) {
  const struct PPB_VideoDecoder_1_1 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_1_1.real_iface;
  return iface->IsVideoDecoder(resource);
}

static int32_t Pnacl_M46_PPB_VideoDecoder_Initialize(PP_Resource video_decoder, PP_Resource graphics3d_context, PP_VideoProfile profile, PP_HardwareAcceleration acceleration, uint32_t min_picture_count, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDecoder_1_1 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_1_1.real_iface;
  return iface->Initialize(video_decoder, graphics3d_context, profile, acceleration, min_picture_count, *callback);
}

static int32_t Pnacl_M46_PPB_VideoDecoder_Decode(PP_Resource video_decoder, uint32_t decode_id, uint32_t size, const void* buffer, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDecoder_1_1 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_1_1.real_iface;
  return iface->Decode(video_decoder, decode_id, size, buffer, *callback);
}

static int32_t Pnacl_M46_PPB_VideoDecoder_GetPicture(PP_Resource video_decoder, struct PP_VideoPicture* picture, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDecoder_1_1 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_1_1.real_iface;
  return iface->GetPicture(video_decoder, picture, *callback);
}

static void Pnacl_M46_PPB_VideoDecoder_RecyclePicture(PP_Resource video_decoder, const struct PP_VideoPicture* picture) {
  const struct PPB_VideoDecoder_1_1 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_1_1.real_iface;
  iface->RecyclePicture(video_decoder, picture);
}

static int32_t Pnacl_M46_PPB_VideoDecoder_Flush(PP_Resource video_decoder, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDecoder_1_1 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_1_1.real_iface;
  return iface->Flush(video_decoder, *callback);
}

static int32_t Pnacl_M46_PPB_VideoDecoder_Reset(PP_Resource video_decoder, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDecoder_1_1 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_1_1.real_iface;
  return iface->Reset(video_decoder, *callback);
}

/* End wrapper methods for PPB_VideoDecoder_1_1 */

/* Begin wrapper methods for PPB_VideoEncoder_0_1 */

static PP_Resource Pnacl_M42_PPB_VideoEncoder_Create(PP_Instance instance) {
  const struct PPB_VideoEncoder_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoEncoder_0_1.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M42_PPB_VideoEncoder_IsVideoEncoder(PP_Resource resource) {
  const struct PPB_VideoEncoder_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoEncoder_0_1.real_iface;
  return iface->IsVideoEncoder(resource);
}

static int32_t Pnacl_M42_PPB_VideoEncoder_GetSupportedProfiles(PP_Resource video_encoder, struct PP_ArrayOutput* output, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoEncoder_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoEncoder_0_1.real_iface;
  return iface->GetSupportedProfiles(video_encoder, *output, *callback);
}

static int32_t Pnacl_M42_PPB_VideoEncoder_Initialize(PP_Resource video_encoder, PP_VideoFrame_Format input_format, const struct PP_Size* input_visible_size, PP_VideoProfile output_profile, uint32_t initial_bitrate, PP_HardwareAcceleration acceleration, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoEncoder_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoEncoder_0_1.real_iface;
  return iface->Initialize(video_encoder, input_format, input_visible_size, output_profile, initial_bitrate, acceleration, *callback);
}

static int32_t Pnacl_M42_PPB_VideoEncoder_GetFramesRequired(PP_Resource video_encoder) {
  const struct PPB_VideoEncoder_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoEncoder_0_1.real_iface;
  return iface->GetFramesRequired(video_encoder);
}

static int32_t Pnacl_M42_PPB_VideoEncoder_GetFrameCodedSize(PP_Resource video_encoder, struct PP_Size* coded_size) {
  const struct PPB_VideoEncoder_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoEncoder_0_1.real_iface;
  return iface->GetFrameCodedSize(video_encoder, coded_size);
}

static int32_t Pnacl_M42_PPB_VideoEncoder_GetVideoFrame(PP_Resource video_encoder, PP_Resource* video_frame, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoEncoder_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoEncoder_0_1.real_iface;
  return iface->GetVideoFrame(video_encoder, video_frame, *callback);
}

static int32_t Pnacl_M42_PPB_VideoEncoder_Encode(PP_Resource video_encoder, PP_Resource video_frame, PP_Bool force_keyframe, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoEncoder_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoEncoder_0_1.real_iface;
  return iface->Encode(video_encoder, video_frame, force_keyframe, *callback);
}

static int32_t Pnacl_M42_PPB_VideoEncoder_GetBitstreamBuffer(PP_Resource video_encoder, struct PP_BitstreamBuffer* bitstream_buffer, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoEncoder_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoEncoder_0_1.real_iface;
  return iface->GetBitstreamBuffer(video_encoder, bitstream_buffer, *callback);
}

static void Pnacl_M42_PPB_VideoEncoder_RecycleBitstreamBuffer(PP_Resource video_encoder, const struct PP_BitstreamBuffer* bitstream_buffer) {
  const struct PPB_VideoEncoder_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoEncoder_0_1.real_iface;
  iface->RecycleBitstreamBuffer(video_encoder, bitstream_buffer);
}

static void Pnacl_M42_PPB_VideoEncoder_RequestEncodingParametersChange(PP_Resource video_encoder, uint32_t bitrate, uint32_t framerate) {
  const struct PPB_VideoEncoder_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoEncoder_0_1.real_iface;
  iface->RequestEncodingParametersChange(video_encoder, bitrate, framerate);
}

static void Pnacl_M42_PPB_VideoEncoder_Close(PP_Resource video_encoder) {
  const struct PPB_VideoEncoder_0_1 *iface = Pnacl_WrapperInfo_PPB_VideoEncoder_0_1.real_iface;
  iface->Close(video_encoder);
}

/* End wrapper methods for PPB_VideoEncoder_0_1 */

/* Begin wrapper methods for PPB_VideoEncoder_0_2 */

static PP_Resource Pnacl_M44_PPB_VideoEncoder_Create(PP_Instance instance) {
  const struct PPB_VideoEncoder_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoEncoder_0_2.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M44_PPB_VideoEncoder_IsVideoEncoder(PP_Resource resource) {
  const struct PPB_VideoEncoder_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoEncoder_0_2.real_iface;
  return iface->IsVideoEncoder(resource);
}

static int32_t Pnacl_M44_PPB_VideoEncoder_GetSupportedProfiles(PP_Resource video_encoder, struct PP_ArrayOutput* output, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoEncoder_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoEncoder_0_2.real_iface;
  return iface->GetSupportedProfiles(video_encoder, *output, *callback);
}

static int32_t Pnacl_M44_PPB_VideoEncoder_Initialize(PP_Resource video_encoder, PP_VideoFrame_Format input_format, const struct PP_Size* input_visible_size, PP_VideoProfile output_profile, uint32_t initial_bitrate, PP_HardwareAcceleration acceleration, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoEncoder_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoEncoder_0_2.real_iface;
  return iface->Initialize(video_encoder, input_format, input_visible_size, output_profile, initial_bitrate, acceleration, *callback);
}

static int32_t Pnacl_M44_PPB_VideoEncoder_GetFramesRequired(PP_Resource video_encoder) {
  const struct PPB_VideoEncoder_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoEncoder_0_2.real_iface;
  return iface->GetFramesRequired(video_encoder);
}

static int32_t Pnacl_M44_PPB_VideoEncoder_GetFrameCodedSize(PP_Resource video_encoder, struct PP_Size* coded_size) {
  const struct PPB_VideoEncoder_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoEncoder_0_2.real_iface;
  return iface->GetFrameCodedSize(video_encoder, coded_size);
}

static int32_t Pnacl_M44_PPB_VideoEncoder_GetVideoFrame(PP_Resource video_encoder, PP_Resource* video_frame, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoEncoder_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoEncoder_0_2.real_iface;
  return iface->GetVideoFrame(video_encoder, video_frame, *callback);
}

static int32_t Pnacl_M44_PPB_VideoEncoder_Encode(PP_Resource video_encoder, PP_Resource video_frame, PP_Bool force_keyframe, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoEncoder_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoEncoder_0_2.real_iface;
  return iface->Encode(video_encoder, video_frame, force_keyframe, *callback);
}

static int32_t Pnacl_M44_PPB_VideoEncoder_GetBitstreamBuffer(PP_Resource video_encoder, struct PP_BitstreamBuffer* bitstream_buffer, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoEncoder_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoEncoder_0_2.real_iface;
  return iface->GetBitstreamBuffer(video_encoder, bitstream_buffer, *callback);
}

static void Pnacl_M44_PPB_VideoEncoder_RecycleBitstreamBuffer(PP_Resource video_encoder, const struct PP_BitstreamBuffer* bitstream_buffer) {
  const struct PPB_VideoEncoder_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoEncoder_0_2.real_iface;
  iface->RecycleBitstreamBuffer(video_encoder, bitstream_buffer);
}

static void Pnacl_M44_PPB_VideoEncoder_RequestEncodingParametersChange(PP_Resource video_encoder, uint32_t bitrate, uint32_t framerate) {
  const struct PPB_VideoEncoder_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoEncoder_0_2.real_iface;
  iface->RequestEncodingParametersChange(video_encoder, bitrate, framerate);
}

static void Pnacl_M44_PPB_VideoEncoder_Close(PP_Resource video_encoder) {
  const struct PPB_VideoEncoder_0_2 *iface = Pnacl_WrapperInfo_PPB_VideoEncoder_0_2.real_iface;
  iface->Close(video_encoder);
}

/* End wrapper methods for PPB_VideoEncoder_0_2 */

/* Not generating wrapper methods for PPB_VideoFrame_0_1 */

/* Not generating wrapper methods for PPB_View_1_0 */

/* Not generating wrapper methods for PPB_View_1_1 */

/* Not generating wrapper methods for PPB_View_1_2 */

/* Begin wrapper methods for PPB_VpnProvider_0_1 */

static PP_Resource Pnacl_M52_PPB_VpnProvider_Create(PP_Instance instance) {
  const struct PPB_VpnProvider_0_1 *iface = Pnacl_WrapperInfo_PPB_VpnProvider_0_1.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M52_PPB_VpnProvider_IsVpnProvider(PP_Resource resource) {
  const struct PPB_VpnProvider_0_1 *iface = Pnacl_WrapperInfo_PPB_VpnProvider_0_1.real_iface;
  return iface->IsVpnProvider(resource);
}

static int32_t Pnacl_M52_PPB_VpnProvider_Bind(PP_Resource vpn_provider, struct PP_Var* configuration_id, struct PP_Var* configuration_name, struct PP_CompletionCallback* callback) {
  const struct PPB_VpnProvider_0_1 *iface = Pnacl_WrapperInfo_PPB_VpnProvider_0_1.real_iface;
  return iface->Bind(vpn_provider, *configuration_id, *configuration_name, *callback);
}

static int32_t Pnacl_M52_PPB_VpnProvider_SendPacket(PP_Resource vpn_provider, struct PP_Var* packet, struct PP_CompletionCallback* callback) {
  const struct PPB_VpnProvider_0_1 *iface = Pnacl_WrapperInfo_PPB_VpnProvider_0_1.real_iface;
  return iface->SendPacket(vpn_provider, *packet, *callback);
}

static int32_t Pnacl_M52_PPB_VpnProvider_ReceivePacket(PP_Resource vpn_provider, struct PP_Var* packet, struct PP_CompletionCallback* callback) {
  const struct PPB_VpnProvider_0_1 *iface = Pnacl_WrapperInfo_PPB_VpnProvider_0_1.real_iface;
  return iface->ReceivePacket(vpn_provider, packet, *callback);
}

/* End wrapper methods for PPB_VpnProvider_0_1 */

/* Begin wrapper methods for PPB_WebSocket_1_0 */

static PP_Resource Pnacl_M18_PPB_WebSocket_Create(PP_Instance instance) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M18_PPB_WebSocket_IsWebSocket(PP_Resource resource) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  return iface->IsWebSocket(resource);
}

static int32_t Pnacl_M18_PPB_WebSocket_Connect(PP_Resource web_socket, struct PP_Var* url, const struct PP_Var protocols[], uint32_t protocol_count, struct PP_CompletionCallback* callback) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  return iface->Connect(web_socket, *url, protocols, protocol_count, *callback);
}

static int32_t Pnacl_M18_PPB_WebSocket_Close(PP_Resource web_socket, uint16_t code, struct PP_Var* reason, struct PP_CompletionCallback* callback) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  return iface->Close(web_socket, code, *reason, *callback);
}

static int32_t Pnacl_M18_PPB_WebSocket_ReceiveMessage(PP_Resource web_socket, struct PP_Var* message, struct PP_CompletionCallback* callback) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  return iface->ReceiveMessage(web_socket, message, *callback);
}

static int32_t Pnacl_M18_PPB_WebSocket_SendMessage(PP_Resource web_socket, struct PP_Var* message) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  return iface->SendMessage(web_socket, *message);
}

static uint64_t Pnacl_M18_PPB_WebSocket_GetBufferedAmount(PP_Resource web_socket) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  return iface->GetBufferedAmount(web_socket);
}

static uint16_t Pnacl_M18_PPB_WebSocket_GetCloseCode(PP_Resource web_socket) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  return iface->GetCloseCode(web_socket);
}

static void Pnacl_M18_PPB_WebSocket_GetCloseReason(struct PP_Var* _struct_result, PP_Resource web_socket) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  *_struct_result = iface->GetCloseReason(web_socket);
}

static PP_Bool Pnacl_M18_PPB_WebSocket_GetCloseWasClean(PP_Resource web_socket) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  return iface->GetCloseWasClean(web_socket);
}

static void Pnacl_M18_PPB_WebSocket_GetExtensions(struct PP_Var* _struct_result, PP_Resource web_socket) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  *_struct_result = iface->GetExtensions(web_socket);
}

static void Pnacl_M18_PPB_WebSocket_GetProtocol(struct PP_Var* _struct_result, PP_Resource web_socket) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  *_struct_result = iface->GetProtocol(web_socket);
}

static PP_WebSocketReadyState Pnacl_M18_PPB_WebSocket_GetReadyState(PP_Resource web_socket) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  return iface->GetReadyState(web_socket);
}

static void Pnacl_M18_PPB_WebSocket_GetURL(struct PP_Var* _struct_result, PP_Resource web_socket) {
  const struct PPB_WebSocket_1_0 *iface = Pnacl_WrapperInfo_PPB_WebSocket_1_0.real_iface;
  *_struct_result = iface->GetURL(web_socket);
}

/* End wrapper methods for PPB_WebSocket_1_0 */

/* Not generating wrapper methods for PPP_Graphics3D_1_0 */

/* Not generating wrapper methods for PPP_InputEvent_0_1 */

/* Not generating wrapper methods for PPP_Instance_1_0 */

/* Not generating wrapper methods for PPP_Instance_1_1 */

/* Not generating wrapper methods for PPP_MessageHandler_0_2 */

/* Begin wrapper methods for PPP_Messaging_1_0 */

static void Pnacl_M14_PPP_Messaging_HandleMessage(PP_Instance instance, struct PP_Var message) {
  const struct PPP_Messaging_1_0 *iface = Pnacl_WrapperInfo_PPP_Messaging_1_0.real_iface;
  void (*temp_fp)(PP_Instance instance, struct PP_Var* message) =
    ((void (*)(PP_Instance instance, struct PP_Var* message))iface->HandleMessage);
  temp_fp(instance, &message);
}

/* End wrapper methods for PPP_Messaging_1_0 */

/* Not generating wrapper methods for PPP_MouseLock_1_0 */

/* Not generating wrapper methods for PPB_BrowserFont_Trusted_1_0 */

/* Not generating wrapper methods for PPB_CharSet_Trusted_1_0 */

/* Not generating wrapper methods for PPB_FileChooserTrusted_0_5 */

/* Not generating wrapper methods for PPB_FileChooserTrusted_0_6 */

/* Not generating wrapper methods for PPB_URLLoaderTrusted_0_3 */

/* Begin wrapper methods for PPB_AudioInput_Dev_0_3 */

static PP_Resource Pnacl_M25_PPB_AudioInput_Dev_Create(PP_Instance instance) {
  const struct PPB_AudioInput_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_3.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M25_PPB_AudioInput_Dev_IsAudioInput(PP_Resource resource) {
  const struct PPB_AudioInput_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_3.real_iface;
  return iface->IsAudioInput(resource);
}

static int32_t Pnacl_M25_PPB_AudioInput_Dev_EnumerateDevices(PP_Resource audio_input, struct PP_ArrayOutput* output, struct PP_CompletionCallback* callback) {
  const struct PPB_AudioInput_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_3.real_iface;
  return iface->EnumerateDevices(audio_input, *output, *callback);
}

static int32_t Pnacl_M25_PPB_AudioInput_Dev_MonitorDeviceChange(PP_Resource audio_input, PP_MonitorDeviceChangeCallback callback, void* user_data) {
  const struct PPB_AudioInput_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_3.real_iface;
  return iface->MonitorDeviceChange(audio_input, callback, user_data);
}

static int32_t Pnacl_M25_PPB_AudioInput_Dev_Open(PP_Resource audio_input, PP_Resource device_ref, PP_Resource config, PPB_AudioInput_Callback_0_3 audio_input_callback, void* user_data, struct PP_CompletionCallback* callback) {
  const struct PPB_AudioInput_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_3.real_iface;
  return iface->Open(audio_input, device_ref, config, audio_input_callback, user_data, *callback);
}

static PP_Resource Pnacl_M25_PPB_AudioInput_Dev_GetCurrentConfig(PP_Resource audio_input) {
  const struct PPB_AudioInput_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_3.real_iface;
  return iface->GetCurrentConfig(audio_input);
}

static PP_Bool Pnacl_M25_PPB_AudioInput_Dev_StartCapture(PP_Resource audio_input) {
  const struct PPB_AudioInput_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_3.real_iface;
  return iface->StartCapture(audio_input);
}

static PP_Bool Pnacl_M25_PPB_AudioInput_Dev_StopCapture(PP_Resource audio_input) {
  const struct PPB_AudioInput_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_3.real_iface;
  return iface->StopCapture(audio_input);
}

static void Pnacl_M25_PPB_AudioInput_Dev_Close(PP_Resource audio_input) {
  const struct PPB_AudioInput_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_3.real_iface;
  iface->Close(audio_input);
}

/* End wrapper methods for PPB_AudioInput_Dev_0_3 */

/* Begin wrapper methods for PPB_AudioInput_Dev_0_4 */

static PP_Resource Pnacl_M30_PPB_AudioInput_Dev_Create(PP_Instance instance) {
  const struct PPB_AudioInput_Dev_0_4 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_4.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M30_PPB_AudioInput_Dev_IsAudioInput(PP_Resource resource) {
  const struct PPB_AudioInput_Dev_0_4 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_4.real_iface;
  return iface->IsAudioInput(resource);
}

static int32_t Pnacl_M30_PPB_AudioInput_Dev_EnumerateDevices(PP_Resource audio_input, struct PP_ArrayOutput* output, struct PP_CompletionCallback* callback) {
  const struct PPB_AudioInput_Dev_0_4 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_4.real_iface;
  return iface->EnumerateDevices(audio_input, *output, *callback);
}

static int32_t Pnacl_M30_PPB_AudioInput_Dev_MonitorDeviceChange(PP_Resource audio_input, PP_MonitorDeviceChangeCallback callback, void* user_data) {
  const struct PPB_AudioInput_Dev_0_4 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_4.real_iface;
  return iface->MonitorDeviceChange(audio_input, callback, user_data);
}

static int32_t Pnacl_M30_PPB_AudioInput_Dev_Open(PP_Resource audio_input, PP_Resource device_ref, PP_Resource config, PPB_AudioInput_Callback audio_input_callback, void* user_data, struct PP_CompletionCallback* callback) {
  const struct PPB_AudioInput_Dev_0_4 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_4.real_iface;
  return iface->Open(audio_input, device_ref, config, audio_input_callback, user_data, *callback);
}

static PP_Resource Pnacl_M30_PPB_AudioInput_Dev_GetCurrentConfig(PP_Resource audio_input) {
  const struct PPB_AudioInput_Dev_0_4 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_4.real_iface;
  return iface->GetCurrentConfig(audio_input);
}

static PP_Bool Pnacl_M30_PPB_AudioInput_Dev_StartCapture(PP_Resource audio_input) {
  const struct PPB_AudioInput_Dev_0_4 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_4.real_iface;
  return iface->StartCapture(audio_input);
}

static PP_Bool Pnacl_M30_PPB_AudioInput_Dev_StopCapture(PP_Resource audio_input) {
  const struct PPB_AudioInput_Dev_0_4 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_4.real_iface;
  return iface->StopCapture(audio_input);
}

static void Pnacl_M30_PPB_AudioInput_Dev_Close(PP_Resource audio_input) {
  const struct PPB_AudioInput_Dev_0_4 *iface = Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_4.real_iface;
  iface->Close(audio_input);
}

/* End wrapper methods for PPB_AudioInput_Dev_0_4 */

/* Begin wrapper methods for PPB_AudioOutput_Dev_0_1 */

static PP_Resource Pnacl_M59_PPB_AudioOutput_Dev_Create(PP_Instance instance) {
  const struct PPB_AudioOutput_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_AudioOutput_Dev_0_1.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M59_PPB_AudioOutput_Dev_IsAudioOutput(PP_Resource resource) {
  const struct PPB_AudioOutput_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_AudioOutput_Dev_0_1.real_iface;
  return iface->IsAudioOutput(resource);
}

static int32_t Pnacl_M59_PPB_AudioOutput_Dev_EnumerateDevices(PP_Resource audio_output, struct PP_ArrayOutput* output, struct PP_CompletionCallback* callback) {
  const struct PPB_AudioOutput_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_AudioOutput_Dev_0_1.real_iface;
  return iface->EnumerateDevices(audio_output, *output, *callback);
}

static int32_t Pnacl_M59_PPB_AudioOutput_Dev_MonitorDeviceChange(PP_Resource audio_output, PP_MonitorDeviceChangeCallback callback, void* user_data) {
  const struct PPB_AudioOutput_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_AudioOutput_Dev_0_1.real_iface;
  return iface->MonitorDeviceChange(audio_output, callback, user_data);
}

static int32_t Pnacl_M59_PPB_AudioOutput_Dev_Open(PP_Resource audio_output, PP_Resource device_ref, PP_Resource config, PPB_AudioOutput_Callback audio_output_callback, void* user_data, struct PP_CompletionCallback* callback) {
  const struct PPB_AudioOutput_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_AudioOutput_Dev_0_1.real_iface;
  return iface->Open(audio_output, device_ref, config, audio_output_callback, user_data, *callback);
}

static PP_Resource Pnacl_M59_PPB_AudioOutput_Dev_GetCurrentConfig(PP_Resource audio_output) {
  const struct PPB_AudioOutput_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_AudioOutput_Dev_0_1.real_iface;
  return iface->GetCurrentConfig(audio_output);
}

static PP_Bool Pnacl_M59_PPB_AudioOutput_Dev_StartPlayback(PP_Resource audio_output) {
  const struct PPB_AudioOutput_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_AudioOutput_Dev_0_1.real_iface;
  return iface->StartPlayback(audio_output);
}

static PP_Bool Pnacl_M59_PPB_AudioOutput_Dev_StopPlayback(PP_Resource audio_output) {
  const struct PPB_AudioOutput_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_AudioOutput_Dev_0_1.real_iface;
  return iface->StopPlayback(audio_output);
}

static void Pnacl_M59_PPB_AudioOutput_Dev_Close(PP_Resource audio_output) {
  const struct PPB_AudioOutput_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_AudioOutput_Dev_0_1.real_iface;
  iface->Close(audio_output);
}

/* End wrapper methods for PPB_AudioOutput_Dev_0_1 */

/* Not generating wrapper methods for PPB_Buffer_Dev_0_4 */

/* Not generating wrapper methods for PPB_Crypto_Dev_0_1 */

/* Not generating wrapper methods for PPB_CursorControl_Dev_0_4 */

/* Begin wrapper methods for PPB_DeviceRef_Dev_0_1 */

static PP_Bool Pnacl_M18_PPB_DeviceRef_Dev_IsDeviceRef(PP_Resource resource) {
  const struct PPB_DeviceRef_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_DeviceRef_Dev_0_1.real_iface;
  return iface->IsDeviceRef(resource);
}

static PP_DeviceType_Dev Pnacl_M18_PPB_DeviceRef_Dev_GetType(PP_Resource device_ref) {
  const struct PPB_DeviceRef_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_DeviceRef_Dev_0_1.real_iface;
  return iface->GetType(device_ref);
}

static void Pnacl_M18_PPB_DeviceRef_Dev_GetName(struct PP_Var* _struct_result, PP_Resource device_ref) {
  const struct PPB_DeviceRef_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_DeviceRef_Dev_0_1.real_iface;
  *_struct_result = iface->GetName(device_ref);
}

/* End wrapper methods for PPB_DeviceRef_Dev_0_1 */

/* Begin wrapper methods for PPB_FileChooser_Dev_0_5 */

static PP_Resource Pnacl_M16_PPB_FileChooser_Dev_Create(PP_Instance instance, PP_FileChooserMode_Dev mode, struct PP_Var* accept_types) {
  const struct PPB_FileChooser_Dev_0_5 *iface = Pnacl_WrapperInfo_PPB_FileChooser_Dev_0_5.real_iface;
  return iface->Create(instance, mode, *accept_types);
}

static PP_Bool Pnacl_M16_PPB_FileChooser_Dev_IsFileChooser(PP_Resource resource) {
  const struct PPB_FileChooser_Dev_0_5 *iface = Pnacl_WrapperInfo_PPB_FileChooser_Dev_0_5.real_iface;
  return iface->IsFileChooser(resource);
}

static int32_t Pnacl_M16_PPB_FileChooser_Dev_Show(PP_Resource chooser, struct PP_CompletionCallback* callback) {
  const struct PPB_FileChooser_Dev_0_5 *iface = Pnacl_WrapperInfo_PPB_FileChooser_Dev_0_5.real_iface;
  return iface->Show(chooser, *callback);
}

static PP_Resource Pnacl_M16_PPB_FileChooser_Dev_GetNextChosenFile(PP_Resource chooser) {
  const struct PPB_FileChooser_Dev_0_5 *iface = Pnacl_WrapperInfo_PPB_FileChooser_Dev_0_5.real_iface;
  return iface->GetNextChosenFile(chooser);
}

/* End wrapper methods for PPB_FileChooser_Dev_0_5 */

/* Begin wrapper methods for PPB_FileChooser_Dev_0_6 */

static PP_Resource Pnacl_M19_PPB_FileChooser_Dev_Create(PP_Instance instance, PP_FileChooserMode_Dev mode, struct PP_Var* accept_types) {
  const struct PPB_FileChooser_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_FileChooser_Dev_0_6.real_iface;
  return iface->Create(instance, mode, *accept_types);
}

static PP_Bool Pnacl_M19_PPB_FileChooser_Dev_IsFileChooser(PP_Resource resource) {
  const struct PPB_FileChooser_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_FileChooser_Dev_0_6.real_iface;
  return iface->IsFileChooser(resource);
}

static int32_t Pnacl_M19_PPB_FileChooser_Dev_Show(PP_Resource chooser, struct PP_ArrayOutput* output, struct PP_CompletionCallback* callback) {
  const struct PPB_FileChooser_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_FileChooser_Dev_0_6.real_iface;
  return iface->Show(chooser, *output, *callback);
}

/* End wrapper methods for PPB_FileChooser_Dev_0_6 */

/* Begin wrapper methods for PPB_IMEInputEvent_Dev_0_1 */

static PP_Bool Pnacl_M16_PPB_IMEInputEvent_Dev_IsIMEInputEvent(PP_Resource resource) {
  const struct PPB_IMEInputEvent_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_1.real_iface;
  return iface->IsIMEInputEvent(resource);
}

static void Pnacl_M16_PPB_IMEInputEvent_Dev_GetText(struct PP_Var* _struct_result, PP_Resource ime_event) {
  const struct PPB_IMEInputEvent_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_1.real_iface;
  *_struct_result = iface->GetText(ime_event);
}

static uint32_t Pnacl_M16_PPB_IMEInputEvent_Dev_GetSegmentNumber(PP_Resource ime_event) {
  const struct PPB_IMEInputEvent_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_1.real_iface;
  return iface->GetSegmentNumber(ime_event);
}

static uint32_t Pnacl_M16_PPB_IMEInputEvent_Dev_GetSegmentOffset(PP_Resource ime_event, uint32_t index) {
  const struct PPB_IMEInputEvent_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_1.real_iface;
  return iface->GetSegmentOffset(ime_event, index);
}

static int32_t Pnacl_M16_PPB_IMEInputEvent_Dev_GetTargetSegment(PP_Resource ime_event) {
  const struct PPB_IMEInputEvent_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_1.real_iface;
  return iface->GetTargetSegment(ime_event);
}

static void Pnacl_M16_PPB_IMEInputEvent_Dev_GetSelection(PP_Resource ime_event, uint32_t* start, uint32_t* end) {
  const struct PPB_IMEInputEvent_Dev_0_1 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_1.real_iface;
  iface->GetSelection(ime_event, start, end);
}

/* End wrapper methods for PPB_IMEInputEvent_Dev_0_1 */

/* Begin wrapper methods for PPB_IMEInputEvent_Dev_0_2 */

static PP_Resource Pnacl_M21_PPB_IMEInputEvent_Dev_Create(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, struct PP_Var* text, uint32_t segment_number, const uint32_t segment_offsets[], int32_t target_segment, uint32_t selection_start, uint32_t selection_end) {
  const struct PPB_IMEInputEvent_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_2.real_iface;
  return iface->Create(instance, type, time_stamp, *text, segment_number, segment_offsets, target_segment, selection_start, selection_end);
}

static PP_Bool Pnacl_M21_PPB_IMEInputEvent_Dev_IsIMEInputEvent(PP_Resource resource) {
  const struct PPB_IMEInputEvent_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_2.real_iface;
  return iface->IsIMEInputEvent(resource);
}

static void Pnacl_M21_PPB_IMEInputEvent_Dev_GetText(struct PP_Var* _struct_result, PP_Resource ime_event) {
  const struct PPB_IMEInputEvent_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_2.real_iface;
  *_struct_result = iface->GetText(ime_event);
}

static uint32_t Pnacl_M21_PPB_IMEInputEvent_Dev_GetSegmentNumber(PP_Resource ime_event) {
  const struct PPB_IMEInputEvent_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_2.real_iface;
  return iface->GetSegmentNumber(ime_event);
}

static uint32_t Pnacl_M21_PPB_IMEInputEvent_Dev_GetSegmentOffset(PP_Resource ime_event, uint32_t index) {
  const struct PPB_IMEInputEvent_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_2.real_iface;
  return iface->GetSegmentOffset(ime_event, index);
}

static int32_t Pnacl_M21_PPB_IMEInputEvent_Dev_GetTargetSegment(PP_Resource ime_event) {
  const struct PPB_IMEInputEvent_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_2.real_iface;
  return iface->GetTargetSegment(ime_event);
}

static void Pnacl_M21_PPB_IMEInputEvent_Dev_GetSelection(PP_Resource ime_event, uint32_t* start, uint32_t* end) {
  const struct PPB_IMEInputEvent_Dev_0_2 *iface = Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_2.real_iface;
  iface->GetSelection(ime_event, start, end);
}

/* End wrapper methods for PPB_IMEInputEvent_Dev_0_2 */

/* Not generating wrapper methods for PPB_Memory_Dev_0_1 */

/* Not generating wrapper methods for PPB_OpenGLES2DrawBuffers_Dev_1_0 */

/* Begin wrapper methods for PPB_Printing_Dev_0_7 */

static PP_Resource Pnacl_M23_PPB_Printing_Dev_Create(PP_Instance instance) {
  const struct PPB_Printing_Dev_0_7 *iface = Pnacl_WrapperInfo_PPB_Printing_Dev_0_7.real_iface;
  return iface->Create(instance);
}

static int32_t Pnacl_M23_PPB_Printing_Dev_GetDefaultPrintSettings(PP_Resource resource, struct PP_PrintSettings_Dev* print_settings, struct PP_CompletionCallback* callback) {
  const struct PPB_Printing_Dev_0_7 *iface = Pnacl_WrapperInfo_PPB_Printing_Dev_0_7.real_iface;
  return iface->GetDefaultPrintSettings(resource, print_settings, *callback);
}

/* End wrapper methods for PPB_Printing_Dev_0_7 */

/* Not generating wrapper methods for PPB_TextInput_Dev_0_1 */

/* Not generating wrapper methods for PPB_TextInput_Dev_0_2 */

/* Not generating wrapper methods for PPB_Trace_Event_Dev_0_1 */

/* Not generating wrapper methods for PPB_Trace_Event_Dev_0_2 */

/* Begin wrapper methods for PPB_URLUtil_Dev_0_6 */

static void Pnacl_M17_PPB_URLUtil_Dev_Canonicalize(struct PP_Var* _struct_result, struct PP_Var* url, struct PP_URLComponents_Dev* components) {
  const struct PPB_URLUtil_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_6.real_iface;
  *_struct_result = iface->Canonicalize(*url, components);
}

static void Pnacl_M17_PPB_URLUtil_Dev_ResolveRelativeToURL(struct PP_Var* _struct_result, struct PP_Var* base_url, struct PP_Var* relative_string, struct PP_URLComponents_Dev* components) {
  const struct PPB_URLUtil_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_6.real_iface;
  *_struct_result = iface->ResolveRelativeToURL(*base_url, *relative_string, components);
}

static void Pnacl_M17_PPB_URLUtil_Dev_ResolveRelativeToDocument(struct PP_Var* _struct_result, PP_Instance instance, struct PP_Var* relative_string, struct PP_URLComponents_Dev* components) {
  const struct PPB_URLUtil_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_6.real_iface;
  *_struct_result = iface->ResolveRelativeToDocument(instance, *relative_string, components);
}

static PP_Bool Pnacl_M17_PPB_URLUtil_Dev_IsSameSecurityOrigin(struct PP_Var* url_a, struct PP_Var* url_b) {
  const struct PPB_URLUtil_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_6.real_iface;
  return iface->IsSameSecurityOrigin(*url_a, *url_b);
}

static PP_Bool Pnacl_M17_PPB_URLUtil_Dev_DocumentCanRequest(PP_Instance instance, struct PP_Var* url) {
  const struct PPB_URLUtil_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_6.real_iface;
  return iface->DocumentCanRequest(instance, *url);
}

static PP_Bool Pnacl_M17_PPB_URLUtil_Dev_DocumentCanAccessDocument(PP_Instance active, PP_Instance target) {
  const struct PPB_URLUtil_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_6.real_iface;
  return iface->DocumentCanAccessDocument(active, target);
}

static void Pnacl_M17_PPB_URLUtil_Dev_GetDocumentURL(struct PP_Var* _struct_result, PP_Instance instance, struct PP_URLComponents_Dev* components) {
  const struct PPB_URLUtil_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_6.real_iface;
  *_struct_result = iface->GetDocumentURL(instance, components);
}

static void Pnacl_M17_PPB_URLUtil_Dev_GetPluginInstanceURL(struct PP_Var* _struct_result, PP_Instance instance, struct PP_URLComponents_Dev* components) {
  const struct PPB_URLUtil_Dev_0_6 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_6.real_iface;
  *_struct_result = iface->GetPluginInstanceURL(instance, components);
}

/* End wrapper methods for PPB_URLUtil_Dev_0_6 */

/* Begin wrapper methods for PPB_URLUtil_Dev_0_7 */

static void Pnacl_M31_PPB_URLUtil_Dev_Canonicalize(struct PP_Var* _struct_result, struct PP_Var* url, struct PP_URLComponents_Dev* components) {
  const struct PPB_URLUtil_Dev_0_7 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_7.real_iface;
  *_struct_result = iface->Canonicalize(*url, components);
}

static void Pnacl_M31_PPB_URLUtil_Dev_ResolveRelativeToURL(struct PP_Var* _struct_result, struct PP_Var* base_url, struct PP_Var* relative_string, struct PP_URLComponents_Dev* components) {
  const struct PPB_URLUtil_Dev_0_7 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_7.real_iface;
  *_struct_result = iface->ResolveRelativeToURL(*base_url, *relative_string, components);
}

static void Pnacl_M31_PPB_URLUtil_Dev_ResolveRelativeToDocument(struct PP_Var* _struct_result, PP_Instance instance, struct PP_Var* relative_string, struct PP_URLComponents_Dev* components) {
  const struct PPB_URLUtil_Dev_0_7 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_7.real_iface;
  *_struct_result = iface->ResolveRelativeToDocument(instance, *relative_string, components);
}

static PP_Bool Pnacl_M31_PPB_URLUtil_Dev_IsSameSecurityOrigin(struct PP_Var* url_a, struct PP_Var* url_b) {
  const struct PPB_URLUtil_Dev_0_7 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_7.real_iface;
  return iface->IsSameSecurityOrigin(*url_a, *url_b);
}

static PP_Bool Pnacl_M31_PPB_URLUtil_Dev_DocumentCanRequest(PP_Instance instance, struct PP_Var* url) {
  const struct PPB_URLUtil_Dev_0_7 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_7.real_iface;
  return iface->DocumentCanRequest(instance, *url);
}

static PP_Bool Pnacl_M31_PPB_URLUtil_Dev_DocumentCanAccessDocument(PP_Instance active, PP_Instance target) {
  const struct PPB_URLUtil_Dev_0_7 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_7.real_iface;
  return iface->DocumentCanAccessDocument(active, target);
}

static void Pnacl_M31_PPB_URLUtil_Dev_GetDocumentURL(struct PP_Var* _struct_result, PP_Instance instance, struct PP_URLComponents_Dev* components) {
  const struct PPB_URLUtil_Dev_0_7 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_7.real_iface;
  *_struct_result = iface->GetDocumentURL(instance, components);
}

static void Pnacl_M31_PPB_URLUtil_Dev_GetPluginInstanceURL(struct PP_Var* _struct_result, PP_Instance instance, struct PP_URLComponents_Dev* components) {
  const struct PPB_URLUtil_Dev_0_7 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_7.real_iface;
  *_struct_result = iface->GetPluginInstanceURL(instance, components);
}

static void Pnacl_M31_PPB_URLUtil_Dev_GetPluginReferrerURL(struct PP_Var* _struct_result, PP_Instance instance, struct PP_URLComponents_Dev* components) {
  const struct PPB_URLUtil_Dev_0_7 *iface = Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_7.real_iface;
  *_struct_result = iface->GetPluginReferrerURL(instance, components);
}

/* End wrapper methods for PPB_URLUtil_Dev_0_7 */

/* Begin wrapper methods for PPB_VideoCapture_Dev_0_3 */

static PP_Resource Pnacl_M25_PPB_VideoCapture_Dev_Create(PP_Instance instance) {
  const struct PPB_VideoCapture_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_3.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M25_PPB_VideoCapture_Dev_IsVideoCapture(PP_Resource video_capture) {
  const struct PPB_VideoCapture_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_3.real_iface;
  return iface->IsVideoCapture(video_capture);
}

static int32_t Pnacl_M25_PPB_VideoCapture_Dev_EnumerateDevices(PP_Resource video_capture, struct PP_ArrayOutput* output, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoCapture_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_3.real_iface;
  return iface->EnumerateDevices(video_capture, *output, *callback);
}

static int32_t Pnacl_M25_PPB_VideoCapture_Dev_MonitorDeviceChange(PP_Resource video_capture, PP_MonitorDeviceChangeCallback callback, void* user_data) {
  const struct PPB_VideoCapture_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_3.real_iface;
  return iface->MonitorDeviceChange(video_capture, callback, user_data);
}

static int32_t Pnacl_M25_PPB_VideoCapture_Dev_Open(PP_Resource video_capture, PP_Resource device_ref, const struct PP_VideoCaptureDeviceInfo_Dev* requested_info, uint32_t buffer_count, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoCapture_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_3.real_iface;
  return iface->Open(video_capture, device_ref, requested_info, buffer_count, *callback);
}

static int32_t Pnacl_M25_PPB_VideoCapture_Dev_StartCapture(PP_Resource video_capture) {
  const struct PPB_VideoCapture_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_3.real_iface;
  return iface->StartCapture(video_capture);
}

static int32_t Pnacl_M25_PPB_VideoCapture_Dev_ReuseBuffer(PP_Resource video_capture, uint32_t buffer) {
  const struct PPB_VideoCapture_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_3.real_iface;
  return iface->ReuseBuffer(video_capture, buffer);
}

static int32_t Pnacl_M25_PPB_VideoCapture_Dev_StopCapture(PP_Resource video_capture) {
  const struct PPB_VideoCapture_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_3.real_iface;
  return iface->StopCapture(video_capture);
}

static void Pnacl_M25_PPB_VideoCapture_Dev_Close(PP_Resource video_capture) {
  const struct PPB_VideoCapture_Dev_0_3 *iface = Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_3.real_iface;
  iface->Close(video_capture);
}

/* End wrapper methods for PPB_VideoCapture_Dev_0_3 */

/* Begin wrapper methods for PPB_VideoDecoder_Dev_0_16 */

static PP_Resource Pnacl_M14_PPB_VideoDecoder_Dev_Create(PP_Instance instance, PP_Resource context, PP_VideoDecoder_Profile profile) {
  const struct PPB_VideoDecoder_Dev_0_16 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_Dev_0_16.real_iface;
  return iface->Create(instance, context, profile);
}

static PP_Bool Pnacl_M14_PPB_VideoDecoder_Dev_IsVideoDecoder(PP_Resource resource) {
  const struct PPB_VideoDecoder_Dev_0_16 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_Dev_0_16.real_iface;
  return iface->IsVideoDecoder(resource);
}

static int32_t Pnacl_M14_PPB_VideoDecoder_Dev_Decode(PP_Resource video_decoder, const struct PP_VideoBitstreamBuffer_Dev* bitstream_buffer, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDecoder_Dev_0_16 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_Dev_0_16.real_iface;
  return iface->Decode(video_decoder, bitstream_buffer, *callback);
}

static void Pnacl_M14_PPB_VideoDecoder_Dev_AssignPictureBuffers(PP_Resource video_decoder, uint32_t no_of_buffers, const struct PP_PictureBuffer_Dev buffers[]) {
  const struct PPB_VideoDecoder_Dev_0_16 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_Dev_0_16.real_iface;
  iface->AssignPictureBuffers(video_decoder, no_of_buffers, buffers);
}

static void Pnacl_M14_PPB_VideoDecoder_Dev_ReusePictureBuffer(PP_Resource video_decoder, int32_t picture_buffer_id) {
  const struct PPB_VideoDecoder_Dev_0_16 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_Dev_0_16.real_iface;
  iface->ReusePictureBuffer(video_decoder, picture_buffer_id);
}

static int32_t Pnacl_M14_PPB_VideoDecoder_Dev_Flush(PP_Resource video_decoder, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDecoder_Dev_0_16 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_Dev_0_16.real_iface;
  return iface->Flush(video_decoder, *callback);
}

static int32_t Pnacl_M14_PPB_VideoDecoder_Dev_Reset(PP_Resource video_decoder, struct PP_CompletionCallback* callback) {
  const struct PPB_VideoDecoder_Dev_0_16 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_Dev_0_16.real_iface;
  return iface->Reset(video_decoder, *callback);
}

static void Pnacl_M14_PPB_VideoDecoder_Dev_Destroy(PP_Resource video_decoder) {
  const struct PPB_VideoDecoder_Dev_0_16 *iface = Pnacl_WrapperInfo_PPB_VideoDecoder_Dev_0_16.real_iface;
  iface->Destroy(video_decoder);
}

/* End wrapper methods for PPB_VideoDecoder_Dev_0_16 */

/* Not generating wrapper methods for PPB_View_Dev_0_1 */

/* Not generating wrapper methods for PPP_NetworkState_Dev_0_1 */

/* Not generating wrapper methods for PPP_Printing_Dev_0_6 */

/* Not generating wrapper methods for PPP_TextInput_Dev_0_1 */

/* Not generating wrapper methods for PPP_VideoCapture_Dev_0_1 */

/* Not generating wrapper methods for PPP_VideoDecoder_Dev_0_11 */

/* Not generating wrapper methods for PPB_CameraCapabilities_Private_0_1 */

/* Begin wrapper methods for PPB_CameraDevice_Private_0_1 */

static PP_Resource Pnacl_M42_PPB_CameraDevice_Private_Create(PP_Instance instance) {
  const struct PPB_CameraDevice_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_CameraDevice_Private_0_1.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M42_PPB_CameraDevice_Private_IsCameraDevice(PP_Resource resource) {
  const struct PPB_CameraDevice_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_CameraDevice_Private_0_1.real_iface;
  return iface->IsCameraDevice(resource);
}

static int32_t Pnacl_M42_PPB_CameraDevice_Private_Open(PP_Resource camera_device, struct PP_Var* device_id, struct PP_CompletionCallback* callback) {
  const struct PPB_CameraDevice_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_CameraDevice_Private_0_1.real_iface;
  return iface->Open(camera_device, *device_id, *callback);
}

static void Pnacl_M42_PPB_CameraDevice_Private_Close(PP_Resource camera_device) {
  const struct PPB_CameraDevice_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_CameraDevice_Private_0_1.real_iface;
  iface->Close(camera_device);
}

static int32_t Pnacl_M42_PPB_CameraDevice_Private_GetCameraCapabilities(PP_Resource camera_device, PP_Resource* capabilities, struct PP_CompletionCallback* callback) {
  const struct PPB_CameraDevice_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_CameraDevice_Private_0_1.real_iface;
  return iface->GetCameraCapabilities(camera_device, capabilities, *callback);
}

/* End wrapper methods for PPB_CameraDevice_Private_0_1 */

/* Begin wrapper methods for PPB_DisplayColorProfile_Private_0_1 */

static PP_Resource Pnacl_M33_PPB_DisplayColorProfile_Private_Create(PP_Instance instance) {
  const struct PPB_DisplayColorProfile_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_DisplayColorProfile_Private_0_1.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M33_PPB_DisplayColorProfile_Private_IsDisplayColorProfile(PP_Resource resource) {
  const struct PPB_DisplayColorProfile_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_DisplayColorProfile_Private_0_1.real_iface;
  return iface->IsDisplayColorProfile(resource);
}

static int32_t Pnacl_M33_PPB_DisplayColorProfile_Private_GetColorProfile(PP_Resource display_color_profile_res, struct PP_ArrayOutput* color_profile, struct PP_CompletionCallback* callback) {
  const struct PPB_DisplayColorProfile_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_DisplayColorProfile_Private_0_1.real_iface;
  return iface->GetColorProfile(display_color_profile_res, *color_profile, *callback);
}

static int32_t Pnacl_M33_PPB_DisplayColorProfile_Private_RegisterColorProfileChangeCallback(PP_Resource display_color_profile_res, struct PP_CompletionCallback* callback) {
  const struct PPB_DisplayColorProfile_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_DisplayColorProfile_Private_0_1.real_iface;
  return iface->RegisterColorProfileChangeCallback(display_color_profile_res, *callback);
}

/* End wrapper methods for PPB_DisplayColorProfile_Private_0_1 */

/* Begin wrapper methods for PPB_Ext_CrxFileSystem_Private_0_1 */

static int32_t Pnacl_M28_PPB_Ext_CrxFileSystem_Private_Open(PP_Instance instance, PP_Resource* file_system, struct PP_CompletionCallback* callback) {
  const struct PPB_Ext_CrxFileSystem_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_Ext_CrxFileSystem_Private_0_1.real_iface;
  return iface->Open(instance, file_system, *callback);
}

/* End wrapper methods for PPB_Ext_CrxFileSystem_Private_0_1 */

/* Begin wrapper methods for PPB_FileIO_Private_0_1 */

static int32_t Pnacl_M28_PPB_FileIO_Private_RequestOSFileHandle(PP_Resource file_io, PP_FileHandle* handle, struct PP_CompletionCallback* callback) {
  const struct PPB_FileIO_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_FileIO_Private_0_1.real_iface;
  return iface->RequestOSFileHandle(file_io, handle, *callback);
}

/* End wrapper methods for PPB_FileIO_Private_0_1 */

/* Begin wrapper methods for PPB_FileRefPrivate_0_1 */

static void Pnacl_M15_PPB_FileRefPrivate_GetAbsolutePath(struct PP_Var* _struct_result, PP_Resource file_ref) {
  const struct PPB_FileRefPrivate_0_1 *iface = Pnacl_WrapperInfo_PPB_FileRefPrivate_0_1.real_iface;
  *_struct_result = iface->GetAbsolutePath(file_ref);
}

/* End wrapper methods for PPB_FileRefPrivate_0_1 */

/* Not generating wrapper methods for PPB_Find_Private_0_3 */

/* Not generating wrapper methods for PPB_Flash_FontFile_0_1 */

/* Not generating wrapper methods for PPB_Flash_FontFile_0_2 */

/* Begin wrapper methods for PPB_HostResolver_Private_0_1 */

static PP_Resource Pnacl_M19_PPB_HostResolver_Private_Create(PP_Instance instance) {
  const struct PPB_HostResolver_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_HostResolver_Private_0_1.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M19_PPB_HostResolver_Private_IsHostResolver(PP_Resource resource) {
  const struct PPB_HostResolver_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_HostResolver_Private_0_1.real_iface;
  return iface->IsHostResolver(resource);
}

static int32_t Pnacl_M19_PPB_HostResolver_Private_Resolve(PP_Resource host_resolver, const char* host, uint16_t port, const struct PP_HostResolver_Private_Hint* hint, struct PP_CompletionCallback* callback) {
  const struct PPB_HostResolver_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_HostResolver_Private_0_1.real_iface;
  return iface->Resolve(host_resolver, host, port, hint, *callback);
}

static void Pnacl_M19_PPB_HostResolver_Private_GetCanonicalName(struct PP_Var* _struct_result, PP_Resource host_resolver) {
  const struct PPB_HostResolver_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_HostResolver_Private_0_1.real_iface;
  *_struct_result = iface->GetCanonicalName(host_resolver);
}

static uint32_t Pnacl_M19_PPB_HostResolver_Private_GetSize(PP_Resource host_resolver) {
  const struct PPB_HostResolver_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_HostResolver_Private_0_1.real_iface;
  return iface->GetSize(host_resolver);
}

static PP_Bool Pnacl_M19_PPB_HostResolver_Private_GetNetAddress(PP_Resource host_resolver, uint32_t index, struct PP_NetAddress_Private* addr) {
  const struct PPB_HostResolver_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_HostResolver_Private_0_1.real_iface;
  return iface->GetNetAddress(host_resolver, index, addr);
}

/* End wrapper methods for PPB_HostResolver_Private_0_1 */

/* Begin wrapper methods for PPB_Instance_Private_0_1 */

static void Pnacl_M13_PPB_Instance_Private_GetWindowObject(struct PP_Var* _struct_result, PP_Instance instance) {
  const struct PPB_Instance_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_Instance_Private_0_1.real_iface;
  *_struct_result = iface->GetWindowObject(instance);
}

static void Pnacl_M13_PPB_Instance_Private_GetOwnerElementObject(struct PP_Var* _struct_result, PP_Instance instance) {
  const struct PPB_Instance_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_Instance_Private_0_1.real_iface;
  *_struct_result = iface->GetOwnerElementObject(instance);
}

static void Pnacl_M13_PPB_Instance_Private_ExecuteScript(struct PP_Var* _struct_result, PP_Instance instance, struct PP_Var* script, struct PP_Var* exception) {
  const struct PPB_Instance_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_Instance_Private_0_1.real_iface;
  *_struct_result = iface->ExecuteScript(instance, *script, exception);
}

/* End wrapper methods for PPB_Instance_Private_0_1 */

/* Begin wrapper methods for PPB_IsolatedFileSystem_Private_0_2 */

static int32_t Pnacl_M33_PPB_IsolatedFileSystem_Private_Open(PP_Instance instance, PP_IsolatedFileSystemType_Private type, PP_Resource* file_system, struct PP_CompletionCallback* callback) {
  const struct PPB_IsolatedFileSystem_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_IsolatedFileSystem_Private_0_2.real_iface;
  return iface->Open(instance, type, file_system, *callback);
}

/* End wrapper methods for PPB_IsolatedFileSystem_Private_0_2 */

/* Begin wrapper methods for PPB_NetAddress_Private_0_1 */

static PP_Bool Pnacl_M17_PPB_NetAddress_Private_AreEqual(const struct PP_NetAddress_Private* addr1, const struct PP_NetAddress_Private* addr2) {
  const struct PPB_NetAddress_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_0_1.real_iface;
  return iface->AreEqual(addr1, addr2);
}

static PP_Bool Pnacl_M17_PPB_NetAddress_Private_AreHostsEqual(const struct PP_NetAddress_Private* addr1, const struct PP_NetAddress_Private* addr2) {
  const struct PPB_NetAddress_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_0_1.real_iface;
  return iface->AreHostsEqual(addr1, addr2);
}

static void Pnacl_M17_PPB_NetAddress_Private_Describe(struct PP_Var* _struct_result, PP_Module module, const struct PP_NetAddress_Private* addr, PP_Bool include_port) {
  const struct PPB_NetAddress_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_0_1.real_iface;
  *_struct_result = iface->Describe(module, addr, include_port);
}

static PP_Bool Pnacl_M17_PPB_NetAddress_Private_ReplacePort(const struct PP_NetAddress_Private* src_addr, uint16_t port, struct PP_NetAddress_Private* addr_out) {
  const struct PPB_NetAddress_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_0_1.real_iface;
  return iface->ReplacePort(src_addr, port, addr_out);
}

static void Pnacl_M17_PPB_NetAddress_Private_GetAnyAddress(PP_Bool is_ipv6, struct PP_NetAddress_Private* addr) {
  const struct PPB_NetAddress_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_0_1.real_iface;
  iface->GetAnyAddress(is_ipv6, addr);
}

/* End wrapper methods for PPB_NetAddress_Private_0_1 */

/* Begin wrapper methods for PPB_NetAddress_Private_1_0 */

static PP_Bool Pnacl_M19_0_PPB_NetAddress_Private_AreEqual(const struct PP_NetAddress_Private* addr1, const struct PP_NetAddress_Private* addr2) {
  const struct PPB_NetAddress_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_0.real_iface;
  return iface->AreEqual(addr1, addr2);
}

static PP_Bool Pnacl_M19_0_PPB_NetAddress_Private_AreHostsEqual(const struct PP_NetAddress_Private* addr1, const struct PP_NetAddress_Private* addr2) {
  const struct PPB_NetAddress_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_0.real_iface;
  return iface->AreHostsEqual(addr1, addr2);
}

static void Pnacl_M19_0_PPB_NetAddress_Private_Describe(struct PP_Var* _struct_result, PP_Module module, const struct PP_NetAddress_Private* addr, PP_Bool include_port) {
  const struct PPB_NetAddress_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_0.real_iface;
  *_struct_result = iface->Describe(module, addr, include_port);
}

static PP_Bool Pnacl_M19_0_PPB_NetAddress_Private_ReplacePort(const struct PP_NetAddress_Private* src_addr, uint16_t port, struct PP_NetAddress_Private* addr_out) {
  const struct PPB_NetAddress_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_0.real_iface;
  return iface->ReplacePort(src_addr, port, addr_out);
}

static void Pnacl_M19_0_PPB_NetAddress_Private_GetAnyAddress(PP_Bool is_ipv6, struct PP_NetAddress_Private* addr) {
  const struct PPB_NetAddress_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_0.real_iface;
  iface->GetAnyAddress(is_ipv6, addr);
}

static PP_NetAddressFamily_Private Pnacl_M19_0_PPB_NetAddress_Private_GetFamily(const struct PP_NetAddress_Private* addr) {
  const struct PPB_NetAddress_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_0.real_iface;
  return iface->GetFamily(addr);
}

static uint16_t Pnacl_M19_0_PPB_NetAddress_Private_GetPort(const struct PP_NetAddress_Private* addr) {
  const struct PPB_NetAddress_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_0.real_iface;
  return iface->GetPort(addr);
}

static PP_Bool Pnacl_M19_0_PPB_NetAddress_Private_GetAddress(const struct PP_NetAddress_Private* addr, void* address, uint16_t address_size) {
  const struct PPB_NetAddress_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_0.real_iface;
  return iface->GetAddress(addr, address, address_size);
}

/* End wrapper methods for PPB_NetAddress_Private_1_0 */

/* Begin wrapper methods for PPB_NetAddress_Private_1_1 */

static PP_Bool Pnacl_M19_1_PPB_NetAddress_Private_AreEqual(const struct PP_NetAddress_Private* addr1, const struct PP_NetAddress_Private* addr2) {
  const struct PPB_NetAddress_Private_1_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1.real_iface;
  return iface->AreEqual(addr1, addr2);
}

static PP_Bool Pnacl_M19_1_PPB_NetAddress_Private_AreHostsEqual(const struct PP_NetAddress_Private* addr1, const struct PP_NetAddress_Private* addr2) {
  const struct PPB_NetAddress_Private_1_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1.real_iface;
  return iface->AreHostsEqual(addr1, addr2);
}

static void Pnacl_M19_1_PPB_NetAddress_Private_Describe(struct PP_Var* _struct_result, PP_Module module, const struct PP_NetAddress_Private* addr, PP_Bool include_port) {
  const struct PPB_NetAddress_Private_1_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1.real_iface;
  *_struct_result = iface->Describe(module, addr, include_port);
}

static PP_Bool Pnacl_M19_1_PPB_NetAddress_Private_ReplacePort(const struct PP_NetAddress_Private* src_addr, uint16_t port, struct PP_NetAddress_Private* addr_out) {
  const struct PPB_NetAddress_Private_1_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1.real_iface;
  return iface->ReplacePort(src_addr, port, addr_out);
}

static void Pnacl_M19_1_PPB_NetAddress_Private_GetAnyAddress(PP_Bool is_ipv6, struct PP_NetAddress_Private* addr) {
  const struct PPB_NetAddress_Private_1_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1.real_iface;
  iface->GetAnyAddress(is_ipv6, addr);
}

static PP_NetAddressFamily_Private Pnacl_M19_1_PPB_NetAddress_Private_GetFamily(const struct PP_NetAddress_Private* addr) {
  const struct PPB_NetAddress_Private_1_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1.real_iface;
  return iface->GetFamily(addr);
}

static uint16_t Pnacl_M19_1_PPB_NetAddress_Private_GetPort(const struct PP_NetAddress_Private* addr) {
  const struct PPB_NetAddress_Private_1_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1.real_iface;
  return iface->GetPort(addr);
}

static PP_Bool Pnacl_M19_1_PPB_NetAddress_Private_GetAddress(const struct PP_NetAddress_Private* addr, void* address, uint16_t address_size) {
  const struct PPB_NetAddress_Private_1_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1.real_iface;
  return iface->GetAddress(addr, address, address_size);
}

static uint32_t Pnacl_M19_1_PPB_NetAddress_Private_GetScopeID(const struct PP_NetAddress_Private* addr) {
  const struct PPB_NetAddress_Private_1_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1.real_iface;
  return iface->GetScopeID(addr);
}

static void Pnacl_M19_1_PPB_NetAddress_Private_CreateFromIPv4Address(const uint8_t ip[4], uint16_t port, struct PP_NetAddress_Private* addr_out) {
  const struct PPB_NetAddress_Private_1_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1.real_iface;
  iface->CreateFromIPv4Address(ip, port, addr_out);
}

static void Pnacl_M19_1_PPB_NetAddress_Private_CreateFromIPv6Address(const uint8_t ip[16], uint32_t scope_id, uint16_t port, struct PP_NetAddress_Private* addr_out) {
  const struct PPB_NetAddress_Private_1_1 *iface = Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1.real_iface;
  iface->CreateFromIPv6Address(ip, scope_id, port, addr_out);
}

/* End wrapper methods for PPB_NetAddress_Private_1_1 */

/* Begin wrapper methods for PPB_TCPServerSocket_Private_0_1 */

static PP_Resource Pnacl_M18_PPB_TCPServerSocket_Private_Create(PP_Instance instance) {
  const struct PPB_TCPServerSocket_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_1.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M18_PPB_TCPServerSocket_Private_IsTCPServerSocket(PP_Resource resource) {
  const struct PPB_TCPServerSocket_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_1.real_iface;
  return iface->IsTCPServerSocket(resource);
}

static int32_t Pnacl_M18_PPB_TCPServerSocket_Private_Listen(PP_Resource tcp_server_socket, const struct PP_NetAddress_Private* addr, int32_t backlog, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPServerSocket_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_1.real_iface;
  return iface->Listen(tcp_server_socket, addr, backlog, *callback);
}

static int32_t Pnacl_M18_PPB_TCPServerSocket_Private_Accept(PP_Resource tcp_server_socket, PP_Resource* tcp_socket, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPServerSocket_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_1.real_iface;
  return iface->Accept(tcp_server_socket, tcp_socket, *callback);
}

static void Pnacl_M18_PPB_TCPServerSocket_Private_StopListening(PP_Resource tcp_server_socket) {
  const struct PPB_TCPServerSocket_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_1.real_iface;
  iface->StopListening(tcp_server_socket);
}

/* End wrapper methods for PPB_TCPServerSocket_Private_0_1 */

/* Begin wrapper methods for PPB_TCPServerSocket_Private_0_2 */

static PP_Resource Pnacl_M28_PPB_TCPServerSocket_Private_Create(PP_Instance instance) {
  const struct PPB_TCPServerSocket_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_2.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M28_PPB_TCPServerSocket_Private_IsTCPServerSocket(PP_Resource resource) {
  const struct PPB_TCPServerSocket_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_2.real_iface;
  return iface->IsTCPServerSocket(resource);
}

static int32_t Pnacl_M28_PPB_TCPServerSocket_Private_Listen(PP_Resource tcp_server_socket, const struct PP_NetAddress_Private* addr, int32_t backlog, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPServerSocket_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_2.real_iface;
  return iface->Listen(tcp_server_socket, addr, backlog, *callback);
}

static int32_t Pnacl_M28_PPB_TCPServerSocket_Private_Accept(PP_Resource tcp_server_socket, PP_Resource* tcp_socket, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPServerSocket_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_2.real_iface;
  return iface->Accept(tcp_server_socket, tcp_socket, *callback);
}

static int32_t Pnacl_M28_PPB_TCPServerSocket_Private_GetLocalAddress(PP_Resource tcp_server_socket, struct PP_NetAddress_Private* addr) {
  const struct PPB_TCPServerSocket_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_2.real_iface;
  return iface->GetLocalAddress(tcp_server_socket, addr);
}

static void Pnacl_M28_PPB_TCPServerSocket_Private_StopListening(PP_Resource tcp_server_socket) {
  const struct PPB_TCPServerSocket_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_2.real_iface;
  iface->StopListening(tcp_server_socket);
}

/* End wrapper methods for PPB_TCPServerSocket_Private_0_2 */

/* Begin wrapper methods for PPB_TCPSocket_Private_0_3 */

static PP_Resource Pnacl_M17_PPB_TCPSocket_Private_Create(PP_Instance instance) {
  const struct PPB_TCPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_3.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M17_PPB_TCPSocket_Private_IsTCPSocket(PP_Resource resource) {
  const struct PPB_TCPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_3.real_iface;
  return iface->IsTCPSocket(resource);
}

static int32_t Pnacl_M17_PPB_TCPSocket_Private_Connect(PP_Resource tcp_socket, const char* host, uint16_t port, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_3.real_iface;
  return iface->Connect(tcp_socket, host, port, *callback);
}

static int32_t Pnacl_M17_PPB_TCPSocket_Private_ConnectWithNetAddress(PP_Resource tcp_socket, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_3.real_iface;
  return iface->ConnectWithNetAddress(tcp_socket, addr, *callback);
}

static PP_Bool Pnacl_M17_PPB_TCPSocket_Private_GetLocalAddress(PP_Resource tcp_socket, struct PP_NetAddress_Private* local_addr) {
  const struct PPB_TCPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_3.real_iface;
  return iface->GetLocalAddress(tcp_socket, local_addr);
}

static PP_Bool Pnacl_M17_PPB_TCPSocket_Private_GetRemoteAddress(PP_Resource tcp_socket, struct PP_NetAddress_Private* remote_addr) {
  const struct PPB_TCPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_3.real_iface;
  return iface->GetRemoteAddress(tcp_socket, remote_addr);
}

static int32_t Pnacl_M17_PPB_TCPSocket_Private_SSLHandshake(PP_Resource tcp_socket, const char* server_name, uint16_t server_port, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_3.real_iface;
  return iface->SSLHandshake(tcp_socket, server_name, server_port, *callback);
}

static int32_t Pnacl_M17_PPB_TCPSocket_Private_Read(PP_Resource tcp_socket, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_3.real_iface;
  return iface->Read(tcp_socket, buffer, bytes_to_read, *callback);
}

static int32_t Pnacl_M17_PPB_TCPSocket_Private_Write(PP_Resource tcp_socket, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_3.real_iface;
  return iface->Write(tcp_socket, buffer, bytes_to_write, *callback);
}

static void Pnacl_M17_PPB_TCPSocket_Private_Disconnect(PP_Resource tcp_socket) {
  const struct PPB_TCPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_3.real_iface;
  iface->Disconnect(tcp_socket);
}

/* End wrapper methods for PPB_TCPSocket_Private_0_3 */

/* Begin wrapper methods for PPB_TCPSocket_Private_0_4 */

static PP_Resource Pnacl_M20_PPB_TCPSocket_Private_Create(PP_Instance instance) {
  const struct PPB_TCPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M20_PPB_TCPSocket_Private_IsTCPSocket(PP_Resource resource) {
  const struct PPB_TCPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4.real_iface;
  return iface->IsTCPSocket(resource);
}

static int32_t Pnacl_M20_PPB_TCPSocket_Private_Connect(PP_Resource tcp_socket, const char* host, uint16_t port, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4.real_iface;
  return iface->Connect(tcp_socket, host, port, *callback);
}

static int32_t Pnacl_M20_PPB_TCPSocket_Private_ConnectWithNetAddress(PP_Resource tcp_socket, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4.real_iface;
  return iface->ConnectWithNetAddress(tcp_socket, addr, *callback);
}

static PP_Bool Pnacl_M20_PPB_TCPSocket_Private_GetLocalAddress(PP_Resource tcp_socket, struct PP_NetAddress_Private* local_addr) {
  const struct PPB_TCPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4.real_iface;
  return iface->GetLocalAddress(tcp_socket, local_addr);
}

static PP_Bool Pnacl_M20_PPB_TCPSocket_Private_GetRemoteAddress(PP_Resource tcp_socket, struct PP_NetAddress_Private* remote_addr) {
  const struct PPB_TCPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4.real_iface;
  return iface->GetRemoteAddress(tcp_socket, remote_addr);
}

static int32_t Pnacl_M20_PPB_TCPSocket_Private_SSLHandshake(PP_Resource tcp_socket, const char* server_name, uint16_t server_port, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4.real_iface;
  return iface->SSLHandshake(tcp_socket, server_name, server_port, *callback);
}

static PP_Resource Pnacl_M20_PPB_TCPSocket_Private_GetServerCertificate(PP_Resource tcp_socket) {
  const struct PPB_TCPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4.real_iface;
  return iface->GetServerCertificate(tcp_socket);
}

static PP_Bool Pnacl_M20_PPB_TCPSocket_Private_AddChainBuildingCertificate(PP_Resource tcp_socket, PP_Resource certificate, PP_Bool is_trusted) {
  const struct PPB_TCPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4.real_iface;
  return iface->AddChainBuildingCertificate(tcp_socket, certificate, is_trusted);
}

static int32_t Pnacl_M20_PPB_TCPSocket_Private_Read(PP_Resource tcp_socket, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4.real_iface;
  return iface->Read(tcp_socket, buffer, bytes_to_read, *callback);
}

static int32_t Pnacl_M20_PPB_TCPSocket_Private_Write(PP_Resource tcp_socket, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4.real_iface;
  return iface->Write(tcp_socket, buffer, bytes_to_write, *callback);
}

static void Pnacl_M20_PPB_TCPSocket_Private_Disconnect(PP_Resource tcp_socket) {
  const struct PPB_TCPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4.real_iface;
  iface->Disconnect(tcp_socket);
}

/* End wrapper methods for PPB_TCPSocket_Private_0_4 */

/* Begin wrapper methods for PPB_TCPSocket_Private_0_5 */

static PP_Resource Pnacl_M27_PPB_TCPSocket_Private_Create(PP_Instance instance) {
  const struct PPB_TCPSocket_Private_0_5 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M27_PPB_TCPSocket_Private_IsTCPSocket(PP_Resource resource) {
  const struct PPB_TCPSocket_Private_0_5 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5.real_iface;
  return iface->IsTCPSocket(resource);
}

static int32_t Pnacl_M27_PPB_TCPSocket_Private_Connect(PP_Resource tcp_socket, const char* host, uint16_t port, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_5 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5.real_iface;
  return iface->Connect(tcp_socket, host, port, *callback);
}

static int32_t Pnacl_M27_PPB_TCPSocket_Private_ConnectWithNetAddress(PP_Resource tcp_socket, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_5 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5.real_iface;
  return iface->ConnectWithNetAddress(tcp_socket, addr, *callback);
}

static PP_Bool Pnacl_M27_PPB_TCPSocket_Private_GetLocalAddress(PP_Resource tcp_socket, struct PP_NetAddress_Private* local_addr) {
  const struct PPB_TCPSocket_Private_0_5 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5.real_iface;
  return iface->GetLocalAddress(tcp_socket, local_addr);
}

static PP_Bool Pnacl_M27_PPB_TCPSocket_Private_GetRemoteAddress(PP_Resource tcp_socket, struct PP_NetAddress_Private* remote_addr) {
  const struct PPB_TCPSocket_Private_0_5 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5.real_iface;
  return iface->GetRemoteAddress(tcp_socket, remote_addr);
}

static int32_t Pnacl_M27_PPB_TCPSocket_Private_SSLHandshake(PP_Resource tcp_socket, const char* server_name, uint16_t server_port, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_5 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5.real_iface;
  return iface->SSLHandshake(tcp_socket, server_name, server_port, *callback);
}

static PP_Resource Pnacl_M27_PPB_TCPSocket_Private_GetServerCertificate(PP_Resource tcp_socket) {
  const struct PPB_TCPSocket_Private_0_5 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5.real_iface;
  return iface->GetServerCertificate(tcp_socket);
}

static PP_Bool Pnacl_M27_PPB_TCPSocket_Private_AddChainBuildingCertificate(PP_Resource tcp_socket, PP_Resource certificate, PP_Bool is_trusted) {
  const struct PPB_TCPSocket_Private_0_5 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5.real_iface;
  return iface->AddChainBuildingCertificate(tcp_socket, certificate, is_trusted);
}

static int32_t Pnacl_M27_PPB_TCPSocket_Private_Read(PP_Resource tcp_socket, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_5 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5.real_iface;
  return iface->Read(tcp_socket, buffer, bytes_to_read, *callback);
}

static int32_t Pnacl_M27_PPB_TCPSocket_Private_Write(PP_Resource tcp_socket, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_5 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5.real_iface;
  return iface->Write(tcp_socket, buffer, bytes_to_write, *callback);
}

static void Pnacl_M27_PPB_TCPSocket_Private_Disconnect(PP_Resource tcp_socket) {
  const struct PPB_TCPSocket_Private_0_5 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5.real_iface;
  iface->Disconnect(tcp_socket);
}

static int32_t Pnacl_M27_PPB_TCPSocket_Private_SetOption(PP_Resource tcp_socket, PP_TCPSocketOption_Private name, struct PP_Var* value, struct PP_CompletionCallback* callback) {
  const struct PPB_TCPSocket_Private_0_5 *iface = Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5.real_iface;
  return iface->SetOption(tcp_socket, name, *value, *callback);
}

/* End wrapper methods for PPB_TCPSocket_Private_0_5 */

/* Begin wrapper methods for PPB_Testing_Private_1_0 */

static PP_Bool Pnacl_M33_PPB_Testing_Private_ReadImageData(PP_Resource device_context_2d, PP_Resource image, const struct PP_Point* top_left) {
  const struct PPB_Testing_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_Testing_Private_1_0.real_iface;
  return iface->ReadImageData(device_context_2d, image, top_left);
}

static void Pnacl_M33_PPB_Testing_Private_RunMessageLoop(PP_Instance instance) {
  const struct PPB_Testing_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_Testing_Private_1_0.real_iface;
  iface->RunMessageLoop(instance);
}

static void Pnacl_M33_PPB_Testing_Private_QuitMessageLoop(PP_Instance instance) {
  const struct PPB_Testing_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_Testing_Private_1_0.real_iface;
  iface->QuitMessageLoop(instance);
}

static uint32_t Pnacl_M33_PPB_Testing_Private_GetLiveObjectsForInstance(PP_Instance instance) {
  const struct PPB_Testing_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_Testing_Private_1_0.real_iface;
  return iface->GetLiveObjectsForInstance(instance);
}

static PP_Bool Pnacl_M33_PPB_Testing_Private_IsOutOfProcess(void) {
  const struct PPB_Testing_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_Testing_Private_1_0.real_iface;
  return iface->IsOutOfProcess();
}

static void Pnacl_M33_PPB_Testing_Private_SimulateInputEvent(PP_Instance instance, PP_Resource input_event) {
  const struct PPB_Testing_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_Testing_Private_1_0.real_iface;
  iface->SimulateInputEvent(instance, input_event);
}

static void Pnacl_M33_PPB_Testing_Private_GetDocumentURL(struct PP_Var* _struct_result, PP_Instance instance, struct PP_URLComponents_Dev* components) {
  const struct PPB_Testing_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_Testing_Private_1_0.real_iface;
  *_struct_result = iface->GetDocumentURL(instance, components);
}

static uint32_t Pnacl_M33_PPB_Testing_Private_GetLiveVars(struct PP_Var live_vars[], uint32_t array_size) {
  const struct PPB_Testing_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_Testing_Private_1_0.real_iface;
  return iface->GetLiveVars(live_vars, array_size);
}

static void Pnacl_M33_PPB_Testing_Private_SetMinimumArrayBufferSizeForShmem(PP_Instance instance, uint32_t threshold) {
  const struct PPB_Testing_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_Testing_Private_1_0.real_iface;
  iface->SetMinimumArrayBufferSizeForShmem(instance, threshold);
}

static void Pnacl_M33_PPB_Testing_Private_RunV8GC(PP_Instance instance) {
  const struct PPB_Testing_Private_1_0 *iface = Pnacl_WrapperInfo_PPB_Testing_Private_1_0.real_iface;
  iface->RunV8GC(instance);
}

/* End wrapper methods for PPB_Testing_Private_1_0 */

/* Begin wrapper methods for PPB_UDPSocket_Private_0_2 */

static PP_Resource Pnacl_M17_PPB_UDPSocket_Private_Create(PP_Instance instance_id) {
  const struct PPB_UDPSocket_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_2.real_iface;
  return iface->Create(instance_id);
}

static PP_Bool Pnacl_M17_PPB_UDPSocket_Private_IsUDPSocket(PP_Resource resource_id) {
  const struct PPB_UDPSocket_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_2.real_iface;
  return iface->IsUDPSocket(resource_id);
}

static int32_t Pnacl_M17_PPB_UDPSocket_Private_Bind(PP_Resource udp_socket, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_2.real_iface;
  return iface->Bind(udp_socket, addr, *callback);
}

static int32_t Pnacl_M17_PPB_UDPSocket_Private_RecvFrom(PP_Resource udp_socket, char* buffer, int32_t num_bytes, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_2.real_iface;
  return iface->RecvFrom(udp_socket, buffer, num_bytes, *callback);
}

static PP_Bool Pnacl_M17_PPB_UDPSocket_Private_GetRecvFromAddress(PP_Resource udp_socket, struct PP_NetAddress_Private* addr) {
  const struct PPB_UDPSocket_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_2.real_iface;
  return iface->GetRecvFromAddress(udp_socket, addr);
}

static int32_t Pnacl_M17_PPB_UDPSocket_Private_SendTo(PP_Resource udp_socket, const char* buffer, int32_t num_bytes, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_2.real_iface;
  return iface->SendTo(udp_socket, buffer, num_bytes, addr, *callback);
}

static void Pnacl_M17_PPB_UDPSocket_Private_Close(PP_Resource udp_socket) {
  const struct PPB_UDPSocket_Private_0_2 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_2.real_iface;
  iface->Close(udp_socket);
}

/* End wrapper methods for PPB_UDPSocket_Private_0_2 */

/* Begin wrapper methods for PPB_UDPSocket_Private_0_3 */

static PP_Resource Pnacl_M19_PPB_UDPSocket_Private_Create(PP_Instance instance_id) {
  const struct PPB_UDPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_3.real_iface;
  return iface->Create(instance_id);
}

static PP_Bool Pnacl_M19_PPB_UDPSocket_Private_IsUDPSocket(PP_Resource resource_id) {
  const struct PPB_UDPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_3.real_iface;
  return iface->IsUDPSocket(resource_id);
}

static int32_t Pnacl_M19_PPB_UDPSocket_Private_Bind(PP_Resource udp_socket, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_3.real_iface;
  return iface->Bind(udp_socket, addr, *callback);
}

static PP_Bool Pnacl_M19_PPB_UDPSocket_Private_GetBoundAddress(PP_Resource udp_socket, struct PP_NetAddress_Private* addr) {
  const struct PPB_UDPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_3.real_iface;
  return iface->GetBoundAddress(udp_socket, addr);
}

static int32_t Pnacl_M19_PPB_UDPSocket_Private_RecvFrom(PP_Resource udp_socket, char* buffer, int32_t num_bytes, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_3.real_iface;
  return iface->RecvFrom(udp_socket, buffer, num_bytes, *callback);
}

static PP_Bool Pnacl_M19_PPB_UDPSocket_Private_GetRecvFromAddress(PP_Resource udp_socket, struct PP_NetAddress_Private* addr) {
  const struct PPB_UDPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_3.real_iface;
  return iface->GetRecvFromAddress(udp_socket, addr);
}

static int32_t Pnacl_M19_PPB_UDPSocket_Private_SendTo(PP_Resource udp_socket, const char* buffer, int32_t num_bytes, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_3.real_iface;
  return iface->SendTo(udp_socket, buffer, num_bytes, addr, *callback);
}

static void Pnacl_M19_PPB_UDPSocket_Private_Close(PP_Resource udp_socket) {
  const struct PPB_UDPSocket_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_3.real_iface;
  iface->Close(udp_socket);
}

/* End wrapper methods for PPB_UDPSocket_Private_0_3 */

/* Begin wrapper methods for PPB_UDPSocket_Private_0_4 */

static PP_Resource Pnacl_M23_PPB_UDPSocket_Private_Create(PP_Instance instance_id) {
  const struct PPB_UDPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_4.real_iface;
  return iface->Create(instance_id);
}

static PP_Bool Pnacl_M23_PPB_UDPSocket_Private_IsUDPSocket(PP_Resource resource_id) {
  const struct PPB_UDPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_4.real_iface;
  return iface->IsUDPSocket(resource_id);
}

static int32_t Pnacl_M23_PPB_UDPSocket_Private_SetSocketFeature(PP_Resource udp_socket, PP_UDPSocketFeature_Private name, struct PP_Var* value) {
  const struct PPB_UDPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_4.real_iface;
  return iface->SetSocketFeature(udp_socket, name, *value);
}

static int32_t Pnacl_M23_PPB_UDPSocket_Private_Bind(PP_Resource udp_socket, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_4.real_iface;
  return iface->Bind(udp_socket, addr, *callback);
}

static PP_Bool Pnacl_M23_PPB_UDPSocket_Private_GetBoundAddress(PP_Resource udp_socket, struct PP_NetAddress_Private* addr) {
  const struct PPB_UDPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_4.real_iface;
  return iface->GetBoundAddress(udp_socket, addr);
}

static int32_t Pnacl_M23_PPB_UDPSocket_Private_RecvFrom(PP_Resource udp_socket, char* buffer, int32_t num_bytes, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_4.real_iface;
  return iface->RecvFrom(udp_socket, buffer, num_bytes, *callback);
}

static PP_Bool Pnacl_M23_PPB_UDPSocket_Private_GetRecvFromAddress(PP_Resource udp_socket, struct PP_NetAddress_Private* addr) {
  const struct PPB_UDPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_4.real_iface;
  return iface->GetRecvFromAddress(udp_socket, addr);
}

static int32_t Pnacl_M23_PPB_UDPSocket_Private_SendTo(PP_Resource udp_socket, const char* buffer, int32_t num_bytes, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback* callback) {
  const struct PPB_UDPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_4.real_iface;
  return iface->SendTo(udp_socket, buffer, num_bytes, addr, *callback);
}

static void Pnacl_M23_PPB_UDPSocket_Private_Close(PP_Resource udp_socket) {
  const struct PPB_UDPSocket_Private_0_4 *iface = Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_4.real_iface;
  iface->Close(udp_socket);
}

/* End wrapper methods for PPB_UDPSocket_Private_0_4 */

/* Begin wrapper methods for PPB_UMA_Private_0_3 */

static void Pnacl_M35_PPB_UMA_Private_HistogramCustomTimes(PP_Instance instance, struct PP_Var* name, int64_t sample, int64_t min, int64_t max, uint32_t bucket_count) {
  const struct PPB_UMA_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_UMA_Private_0_3.real_iface;
  iface->HistogramCustomTimes(instance, *name, sample, min, max, bucket_count);
}

static void Pnacl_M35_PPB_UMA_Private_HistogramCustomCounts(PP_Instance instance, struct PP_Var* name, int32_t sample, int32_t min, int32_t max, uint32_t bucket_count) {
  const struct PPB_UMA_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_UMA_Private_0_3.real_iface;
  iface->HistogramCustomCounts(instance, *name, sample, min, max, bucket_count);
}

static void Pnacl_M35_PPB_UMA_Private_HistogramEnumeration(PP_Instance instance, struct PP_Var* name, int32_t sample, int32_t boundary_value) {
  const struct PPB_UMA_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_UMA_Private_0_3.real_iface;
  iface->HistogramEnumeration(instance, *name, sample, boundary_value);
}

static int32_t Pnacl_M35_PPB_UMA_Private_IsCrashReportingEnabled(PP_Instance instance, struct PP_CompletionCallback* callback) {
  const struct PPB_UMA_Private_0_3 *iface = Pnacl_WrapperInfo_PPB_UMA_Private_0_3.real_iface;
  return iface->IsCrashReportingEnabled(instance, *callback);
}

/* End wrapper methods for PPB_UMA_Private_0_3 */

/* Begin wrapper methods for PPB_X509Certificate_Private_0_1 */

static PP_Resource Pnacl_M19_PPB_X509Certificate_Private_Create(PP_Instance instance) {
  const struct PPB_X509Certificate_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_X509Certificate_Private_0_1.real_iface;
  return iface->Create(instance);
}

static PP_Bool Pnacl_M19_PPB_X509Certificate_Private_IsX509CertificatePrivate(PP_Resource resource) {
  const struct PPB_X509Certificate_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_X509Certificate_Private_0_1.real_iface;
  return iface->IsX509CertificatePrivate(resource);
}

static PP_Bool Pnacl_M19_PPB_X509Certificate_Private_Initialize(PP_Resource resource, const char* bytes, uint32_t length) {
  const struct PPB_X509Certificate_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_X509Certificate_Private_0_1.real_iface;
  return iface->Initialize(resource, bytes, length);
}

static void Pnacl_M19_PPB_X509Certificate_Private_GetField(struct PP_Var* _struct_result, PP_Resource resource, PP_X509Certificate_Private_Field field) {
  const struct PPB_X509Certificate_Private_0_1 *iface = Pnacl_WrapperInfo_PPB_X509Certificate_Private_0_1.real_iface;
  *_struct_result = iface->GetField(resource, field);
}

/* End wrapper methods for PPB_X509Certificate_Private_0_1 */

/* Not generating wrapper methods for PPP_Find_Private_0_3 */

/* Begin wrapper methods for PPP_Instance_Private_0_1 */

static struct PP_Var Pnacl_M18_PPP_Instance_Private_GetInstanceObject(PP_Instance instance) {
  const struct PPP_Instance_Private_0_1 *iface = Pnacl_WrapperInfo_PPP_Instance_Private_0_1.real_iface;
  void (*temp_fp)(struct PP_Var* _struct_result, PP_Instance instance) =
    ((void (*)(struct PP_Var* _struct_result, PP_Instance instance))iface->GetInstanceObject);
  struct PP_Var _struct_result;
  temp_fp(&_struct_result, instance);
  return _struct_result;
}

/* End wrapper methods for PPP_Instance_Private_0_1 */

/* Not generating wrapper methods for PPP_PexeStreamHandler_1_0 */

/* Not generating wrapper interface for PPB_Audio_1_0 */

/* Not generating wrapper interface for PPB_Audio_1_1 */

/* Not generating wrapper interface for PPB_AudioBuffer_0_1 */

/* Not generating wrapper interface for PPB_AudioConfig_1_0 */

/* Not generating wrapper interface for PPB_AudioConfig_1_1 */

static const struct PPB_Console_1_0 Pnacl_Wrappers_PPB_Console_1_0 = {
    .Log = (void (*)(PP_Instance instance, PP_LogLevel level, struct PP_Var value))&Pnacl_M25_PPB_Console_Log,
    .LogWithSource = (void (*)(PP_Instance instance, PP_LogLevel level, struct PP_Var source, struct PP_Var value))&Pnacl_M25_PPB_Console_LogWithSource
};

static const struct PPB_Core_1_0 Pnacl_Wrappers_PPB_Core_1_0 = {
    .AddRefResource = (void (*)(PP_Resource resource))&Pnacl_M14_PPB_Core_AddRefResource,
    .ReleaseResource = (void (*)(PP_Resource resource))&Pnacl_M14_PPB_Core_ReleaseResource,
    .GetTime = (PP_Time (*)(void))&Pnacl_M14_PPB_Core_GetTime,
    .GetTimeTicks = (PP_TimeTicks (*)(void))&Pnacl_M14_PPB_Core_GetTimeTicks,
    .CallOnMainThread = (void (*)(int32_t delay_in_milliseconds, struct PP_CompletionCallback callback, int32_t result))&Pnacl_M14_PPB_Core_CallOnMainThread,
    .IsMainThread = (PP_Bool (*)(void))&Pnacl_M14_PPB_Core_IsMainThread
};

static const struct PPB_FileIO_1_0 Pnacl_Wrappers_PPB_FileIO_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M14_PPB_FileIO_Create,
    .IsFileIO = (PP_Bool (*)(PP_Resource resource))&Pnacl_M14_PPB_FileIO_IsFileIO,
    .Open = (int32_t (*)(PP_Resource file_io, PP_Resource file_ref, int32_t open_flags, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_FileIO_Open,
    .Query = (int32_t (*)(PP_Resource file_io, struct PP_FileInfo* info, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_FileIO_Query,
    .Touch = (int32_t (*)(PP_Resource file_io, PP_Time last_access_time, PP_Time last_modified_time, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_FileIO_Touch,
    .Read = (int32_t (*)(PP_Resource file_io, int64_t offset, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_FileIO_Read,
    .Write = (int32_t (*)(PP_Resource file_io, int64_t offset, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_FileIO_Write,
    .SetLength = (int32_t (*)(PP_Resource file_io, int64_t length, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_FileIO_SetLength,
    .Flush = (int32_t (*)(PP_Resource file_io, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_FileIO_Flush,
    .Close = (void (*)(PP_Resource file_io))&Pnacl_M14_PPB_FileIO_Close
};

static const struct PPB_FileIO_1_1 Pnacl_Wrappers_PPB_FileIO_1_1 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M25_PPB_FileIO_Create,
    .IsFileIO = (PP_Bool (*)(PP_Resource resource))&Pnacl_M25_PPB_FileIO_IsFileIO,
    .Open = (int32_t (*)(PP_Resource file_io, PP_Resource file_ref, int32_t open_flags, struct PP_CompletionCallback callback))&Pnacl_M25_PPB_FileIO_Open,
    .Query = (int32_t (*)(PP_Resource file_io, struct PP_FileInfo* info, struct PP_CompletionCallback callback))&Pnacl_M25_PPB_FileIO_Query,
    .Touch = (int32_t (*)(PP_Resource file_io, PP_Time last_access_time, PP_Time last_modified_time, struct PP_CompletionCallback callback))&Pnacl_M25_PPB_FileIO_Touch,
    .Read = (int32_t (*)(PP_Resource file_io, int64_t offset, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback callback))&Pnacl_M25_PPB_FileIO_Read,
    .Write = (int32_t (*)(PP_Resource file_io, int64_t offset, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback callback))&Pnacl_M25_PPB_FileIO_Write,
    .SetLength = (int32_t (*)(PP_Resource file_io, int64_t length, struct PP_CompletionCallback callback))&Pnacl_M25_PPB_FileIO_SetLength,
    .Flush = (int32_t (*)(PP_Resource file_io, struct PP_CompletionCallback callback))&Pnacl_M25_PPB_FileIO_Flush,
    .Close = (void (*)(PP_Resource file_io))&Pnacl_M25_PPB_FileIO_Close,
    .ReadToArray = (int32_t (*)(PP_Resource file_io, int64_t offset, int32_t max_read_length, struct PP_ArrayOutput* output, struct PP_CompletionCallback callback))&Pnacl_M25_PPB_FileIO_ReadToArray
};

static const struct PPB_FileRef_1_0 Pnacl_Wrappers_PPB_FileRef_1_0 = {
    .Create = (PP_Resource (*)(PP_Resource file_system, const char* path))&Pnacl_M14_PPB_FileRef_Create,
    .IsFileRef = (PP_Bool (*)(PP_Resource resource))&Pnacl_M14_PPB_FileRef_IsFileRef,
    .GetFileSystemType = (PP_FileSystemType (*)(PP_Resource file_ref))&Pnacl_M14_PPB_FileRef_GetFileSystemType,
    .GetName = (struct PP_Var (*)(PP_Resource file_ref))&Pnacl_M14_PPB_FileRef_GetName,
    .GetPath = (struct PP_Var (*)(PP_Resource file_ref))&Pnacl_M14_PPB_FileRef_GetPath,
    .GetParent = (PP_Resource (*)(PP_Resource file_ref))&Pnacl_M14_PPB_FileRef_GetParent,
    .MakeDirectory = (int32_t (*)(PP_Resource directory_ref, PP_Bool make_ancestors, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_FileRef_MakeDirectory,
    .Touch = (int32_t (*)(PP_Resource file_ref, PP_Time last_access_time, PP_Time last_modified_time, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_FileRef_Touch,
    .Delete = (int32_t (*)(PP_Resource file_ref, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_FileRef_Delete,
    .Rename = (int32_t (*)(PP_Resource file_ref, PP_Resource new_file_ref, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_FileRef_Rename
};

static const struct PPB_FileRef_1_1 Pnacl_Wrappers_PPB_FileRef_1_1 = {
    .Create = (PP_Resource (*)(PP_Resource file_system, const char* path))&Pnacl_M28_PPB_FileRef_Create,
    .IsFileRef = (PP_Bool (*)(PP_Resource resource))&Pnacl_M28_PPB_FileRef_IsFileRef,
    .GetFileSystemType = (PP_FileSystemType (*)(PP_Resource file_ref))&Pnacl_M28_PPB_FileRef_GetFileSystemType,
    .GetName = (struct PP_Var (*)(PP_Resource file_ref))&Pnacl_M28_PPB_FileRef_GetName,
    .GetPath = (struct PP_Var (*)(PP_Resource file_ref))&Pnacl_M28_PPB_FileRef_GetPath,
    .GetParent = (PP_Resource (*)(PP_Resource file_ref))&Pnacl_M28_PPB_FileRef_GetParent,
    .MakeDirectory = (int32_t (*)(PP_Resource directory_ref, PP_Bool make_ancestors, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_FileRef_MakeDirectory,
    .Touch = (int32_t (*)(PP_Resource file_ref, PP_Time last_access_time, PP_Time last_modified_time, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_FileRef_Touch,
    .Delete = (int32_t (*)(PP_Resource file_ref, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_FileRef_Delete,
    .Rename = (int32_t (*)(PP_Resource file_ref, PP_Resource new_file_ref, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_FileRef_Rename,
    .Query = (int32_t (*)(PP_Resource file_ref, struct PP_FileInfo* info, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_FileRef_Query,
    .ReadDirectoryEntries = (int32_t (*)(PP_Resource file_ref, struct PP_ArrayOutput output, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_FileRef_ReadDirectoryEntries
};

static const struct PPB_FileRef_1_2 Pnacl_Wrappers_PPB_FileRef_1_2 = {
    .Create = (PP_Resource (*)(PP_Resource file_system, const char* path))&Pnacl_M34_PPB_FileRef_Create,
    .IsFileRef = (PP_Bool (*)(PP_Resource resource))&Pnacl_M34_PPB_FileRef_IsFileRef,
    .GetFileSystemType = (PP_FileSystemType (*)(PP_Resource file_ref))&Pnacl_M34_PPB_FileRef_GetFileSystemType,
    .GetName = (struct PP_Var (*)(PP_Resource file_ref))&Pnacl_M34_PPB_FileRef_GetName,
    .GetPath = (struct PP_Var (*)(PP_Resource file_ref))&Pnacl_M34_PPB_FileRef_GetPath,
    .GetParent = (PP_Resource (*)(PP_Resource file_ref))&Pnacl_M34_PPB_FileRef_GetParent,
    .MakeDirectory = (int32_t (*)(PP_Resource directory_ref, int32_t make_directory_flags, struct PP_CompletionCallback callback))&Pnacl_M34_PPB_FileRef_MakeDirectory,
    .Touch = (int32_t (*)(PP_Resource file_ref, PP_Time last_access_time, PP_Time last_modified_time, struct PP_CompletionCallback callback))&Pnacl_M34_PPB_FileRef_Touch,
    .Delete = (int32_t (*)(PP_Resource file_ref, struct PP_CompletionCallback callback))&Pnacl_M34_PPB_FileRef_Delete,
    .Rename = (int32_t (*)(PP_Resource file_ref, PP_Resource new_file_ref, struct PP_CompletionCallback callback))&Pnacl_M34_PPB_FileRef_Rename,
    .Query = (int32_t (*)(PP_Resource file_ref, struct PP_FileInfo* info, struct PP_CompletionCallback callback))&Pnacl_M34_PPB_FileRef_Query,
    .ReadDirectoryEntries = (int32_t (*)(PP_Resource file_ref, struct PP_ArrayOutput output, struct PP_CompletionCallback callback))&Pnacl_M34_PPB_FileRef_ReadDirectoryEntries
};

static const struct PPB_FileSystem_1_0 Pnacl_Wrappers_PPB_FileSystem_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance, PP_FileSystemType type))&Pnacl_M14_PPB_FileSystem_Create,
    .IsFileSystem = (PP_Bool (*)(PP_Resource resource))&Pnacl_M14_PPB_FileSystem_IsFileSystem,
    .Open = (int32_t (*)(PP_Resource file_system, int64_t expected_size, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_FileSystem_Open,
    .GetType = (PP_FileSystemType (*)(PP_Resource file_system))&Pnacl_M14_PPB_FileSystem_GetType
};

/* Not generating wrapper interface for PPB_Fullscreen_1_0 */

/* Not generating wrapper interface for PPB_Gamepad_1_0 */

static const struct PPB_Graphics2D_1_0 Pnacl_Wrappers_PPB_Graphics2D_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance, const struct PP_Size* size, PP_Bool is_always_opaque))&Pnacl_M14_PPB_Graphics2D_Create,
    .IsGraphics2D = (PP_Bool (*)(PP_Resource resource))&Pnacl_M14_PPB_Graphics2D_IsGraphics2D,
    .Describe = (PP_Bool (*)(PP_Resource graphics_2d, struct PP_Size* size, PP_Bool* is_always_opaque))&Pnacl_M14_PPB_Graphics2D_Describe,
    .PaintImageData = (void (*)(PP_Resource graphics_2d, PP_Resource image_data, const struct PP_Point* top_left, const struct PP_Rect* src_rect))&Pnacl_M14_PPB_Graphics2D_PaintImageData,
    .Scroll = (void (*)(PP_Resource graphics_2d, const struct PP_Rect* clip_rect, const struct PP_Point* amount))&Pnacl_M14_PPB_Graphics2D_Scroll,
    .ReplaceContents = (void (*)(PP_Resource graphics_2d, PP_Resource image_data))&Pnacl_M14_PPB_Graphics2D_ReplaceContents,
    .Flush = (int32_t (*)(PP_Resource graphics_2d, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_Graphics2D_Flush
};

static const struct PPB_Graphics2D_1_1 Pnacl_Wrappers_PPB_Graphics2D_1_1 = {
    .Create = (PP_Resource (*)(PP_Instance instance, const struct PP_Size* size, PP_Bool is_always_opaque))&Pnacl_M27_PPB_Graphics2D_Create,
    .IsGraphics2D = (PP_Bool (*)(PP_Resource resource))&Pnacl_M27_PPB_Graphics2D_IsGraphics2D,
    .Describe = (PP_Bool (*)(PP_Resource graphics_2d, struct PP_Size* size, PP_Bool* is_always_opaque))&Pnacl_M27_PPB_Graphics2D_Describe,
    .PaintImageData = (void (*)(PP_Resource graphics_2d, PP_Resource image_data, const struct PP_Point* top_left, const struct PP_Rect* src_rect))&Pnacl_M27_PPB_Graphics2D_PaintImageData,
    .Scroll = (void (*)(PP_Resource graphics_2d, const struct PP_Rect* clip_rect, const struct PP_Point* amount))&Pnacl_M27_PPB_Graphics2D_Scroll,
    .ReplaceContents = (void (*)(PP_Resource graphics_2d, PP_Resource image_data))&Pnacl_M27_PPB_Graphics2D_ReplaceContents,
    .Flush = (int32_t (*)(PP_Resource graphics_2d, struct PP_CompletionCallback callback))&Pnacl_M27_PPB_Graphics2D_Flush,
    .SetScale = (PP_Bool (*)(PP_Resource resource, float scale))&Pnacl_M27_PPB_Graphics2D_SetScale,
    .GetScale = (float (*)(PP_Resource resource))&Pnacl_M27_PPB_Graphics2D_GetScale
};

static const struct PPB_Graphics2D_1_2 Pnacl_Wrappers_PPB_Graphics2D_1_2 = {
    .Create = (PP_Resource (*)(PP_Instance instance, const struct PP_Size* size, PP_Bool is_always_opaque))&Pnacl_M52_PPB_Graphics2D_Create,
    .IsGraphics2D = (PP_Bool (*)(PP_Resource resource))&Pnacl_M52_PPB_Graphics2D_IsGraphics2D,
    .Describe = (PP_Bool (*)(PP_Resource graphics_2d, struct PP_Size* size, PP_Bool* is_always_opaque))&Pnacl_M52_PPB_Graphics2D_Describe,
    .PaintImageData = (void (*)(PP_Resource graphics_2d, PP_Resource image_data, const struct PP_Point* top_left, const struct PP_Rect* src_rect))&Pnacl_M52_PPB_Graphics2D_PaintImageData,
    .Scroll = (void (*)(PP_Resource graphics_2d, const struct PP_Rect* clip_rect, const struct PP_Point* amount))&Pnacl_M52_PPB_Graphics2D_Scroll,
    .ReplaceContents = (void (*)(PP_Resource graphics_2d, PP_Resource image_data))&Pnacl_M52_PPB_Graphics2D_ReplaceContents,
    .Flush = (int32_t (*)(PP_Resource graphics_2d, struct PP_CompletionCallback callback))&Pnacl_M52_PPB_Graphics2D_Flush,
    .SetScale = (PP_Bool (*)(PP_Resource resource, float scale))&Pnacl_M52_PPB_Graphics2D_SetScale,
    .GetScale = (float (*)(PP_Resource resource))&Pnacl_M52_PPB_Graphics2D_GetScale,
    .SetLayerTransform = (PP_Bool (*)(PP_Resource resource, float scale, const struct PP_Point* origin, const struct PP_Point* translate))&Pnacl_M52_PPB_Graphics2D_SetLayerTransform
};

static const struct PPB_Graphics3D_1_0 Pnacl_Wrappers_PPB_Graphics3D_1_0 = {
    .GetAttribMaxValue = (int32_t (*)(PP_Resource instance, int32_t attribute, int32_t* value))&Pnacl_M15_PPB_Graphics3D_GetAttribMaxValue,
    .Create = (PP_Resource (*)(PP_Instance instance, PP_Resource share_context, const int32_t attrib_list[]))&Pnacl_M15_PPB_Graphics3D_Create,
    .IsGraphics3D = (PP_Bool (*)(PP_Resource resource))&Pnacl_M15_PPB_Graphics3D_IsGraphics3D,
    .GetAttribs = (int32_t (*)(PP_Resource context, int32_t attrib_list[]))&Pnacl_M15_PPB_Graphics3D_GetAttribs,
    .SetAttribs = (int32_t (*)(PP_Resource context, const int32_t attrib_list[]))&Pnacl_M15_PPB_Graphics3D_SetAttribs,
    .GetError = (int32_t (*)(PP_Resource context))&Pnacl_M15_PPB_Graphics3D_GetError,
    .ResizeBuffers = (int32_t (*)(PP_Resource context, int32_t width, int32_t height))&Pnacl_M15_PPB_Graphics3D_ResizeBuffers,
    .SwapBuffers = (int32_t (*)(PP_Resource context, struct PP_CompletionCallback callback))&Pnacl_M15_PPB_Graphics3D_SwapBuffers
};

static const struct PPB_HostResolver_1_0 Pnacl_Wrappers_PPB_HostResolver_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M29_PPB_HostResolver_Create,
    .IsHostResolver = (PP_Bool (*)(PP_Resource resource))&Pnacl_M29_PPB_HostResolver_IsHostResolver,
    .Resolve = (int32_t (*)(PP_Resource host_resolver, const char* host, uint16_t port, const struct PP_HostResolver_Hint* hint, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_HostResolver_Resolve,
    .GetCanonicalName = (struct PP_Var (*)(PP_Resource host_resolver))&Pnacl_M29_PPB_HostResolver_GetCanonicalName,
    .GetNetAddressCount = (uint32_t (*)(PP_Resource host_resolver))&Pnacl_M29_PPB_HostResolver_GetNetAddressCount,
    .GetNetAddress = (PP_Resource (*)(PP_Resource host_resolver, uint32_t index))&Pnacl_M29_PPB_HostResolver_GetNetAddress
};

/* Not generating wrapper interface for PPB_ImageData_1_0 */

/* Not generating wrapper interface for PPB_InputEvent_1_0 */

static const struct PPB_MouseInputEvent_1_0 Pnacl_Wrappers_PPB_MouseInputEvent_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, uint32_t modifiers, PP_InputEvent_MouseButton mouse_button, const struct PP_Point* mouse_position, int32_t click_count))&Pnacl_M13_PPB_MouseInputEvent_Create,
    .IsMouseInputEvent = (PP_Bool (*)(PP_Resource resource))&Pnacl_M13_PPB_MouseInputEvent_IsMouseInputEvent,
    .GetButton = (PP_InputEvent_MouseButton (*)(PP_Resource mouse_event))&Pnacl_M13_PPB_MouseInputEvent_GetButton,
    .GetPosition = (struct PP_Point (*)(PP_Resource mouse_event))&Pnacl_M13_PPB_MouseInputEvent_GetPosition,
    .GetClickCount = (int32_t (*)(PP_Resource mouse_event))&Pnacl_M13_PPB_MouseInputEvent_GetClickCount
};

static const struct PPB_MouseInputEvent_1_1 Pnacl_Wrappers_PPB_MouseInputEvent_1_1 = {
    .Create = (PP_Resource (*)(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, uint32_t modifiers, PP_InputEvent_MouseButton mouse_button, const struct PP_Point* mouse_position, int32_t click_count, const struct PP_Point* mouse_movement))&Pnacl_M14_PPB_MouseInputEvent_Create,
    .IsMouseInputEvent = (PP_Bool (*)(PP_Resource resource))&Pnacl_M14_PPB_MouseInputEvent_IsMouseInputEvent,
    .GetButton = (PP_InputEvent_MouseButton (*)(PP_Resource mouse_event))&Pnacl_M14_PPB_MouseInputEvent_GetButton,
    .GetPosition = (struct PP_Point (*)(PP_Resource mouse_event))&Pnacl_M14_PPB_MouseInputEvent_GetPosition,
    .GetClickCount = (int32_t (*)(PP_Resource mouse_event))&Pnacl_M14_PPB_MouseInputEvent_GetClickCount,
    .GetMovement = (struct PP_Point (*)(PP_Resource mouse_event))&Pnacl_M14_PPB_MouseInputEvent_GetMovement
};

static const struct PPB_WheelInputEvent_1_0 Pnacl_Wrappers_PPB_WheelInputEvent_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance, PP_TimeTicks time_stamp, uint32_t modifiers, const struct PP_FloatPoint* wheel_delta, const struct PP_FloatPoint* wheel_ticks, PP_Bool scroll_by_page))&Pnacl_M13_PPB_WheelInputEvent_Create,
    .IsWheelInputEvent = (PP_Bool (*)(PP_Resource resource))&Pnacl_M13_PPB_WheelInputEvent_IsWheelInputEvent,
    .GetDelta = (struct PP_FloatPoint (*)(PP_Resource wheel_event))&Pnacl_M13_PPB_WheelInputEvent_GetDelta,
    .GetTicks = (struct PP_FloatPoint (*)(PP_Resource wheel_event))&Pnacl_M13_PPB_WheelInputEvent_GetTicks,
    .GetScrollByPage = (PP_Bool (*)(PP_Resource wheel_event))&Pnacl_M13_PPB_WheelInputEvent_GetScrollByPage
};

static const struct PPB_KeyboardInputEvent_1_0 Pnacl_Wrappers_PPB_KeyboardInputEvent_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, uint32_t modifiers, uint32_t key_code, struct PP_Var character_text))&Pnacl_M13_PPB_KeyboardInputEvent_Create,
    .IsKeyboardInputEvent = (PP_Bool (*)(PP_Resource resource))&Pnacl_M13_PPB_KeyboardInputEvent_IsKeyboardInputEvent,
    .GetKeyCode = (uint32_t (*)(PP_Resource key_event))&Pnacl_M13_PPB_KeyboardInputEvent_GetKeyCode,
    .GetCharacterText = (struct PP_Var (*)(PP_Resource character_event))&Pnacl_M13_PPB_KeyboardInputEvent_GetCharacterText
};

static const struct PPB_KeyboardInputEvent_1_2 Pnacl_Wrappers_PPB_KeyboardInputEvent_1_2 = {
    .Create = (PP_Resource (*)(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, uint32_t modifiers, uint32_t key_code, struct PP_Var character_text, struct PP_Var code))&Pnacl_M34_PPB_KeyboardInputEvent_Create,
    .IsKeyboardInputEvent = (PP_Bool (*)(PP_Resource resource))&Pnacl_M34_PPB_KeyboardInputEvent_IsKeyboardInputEvent,
    .GetKeyCode = (uint32_t (*)(PP_Resource key_event))&Pnacl_M34_PPB_KeyboardInputEvent_GetKeyCode,
    .GetCharacterText = (struct PP_Var (*)(PP_Resource character_event))&Pnacl_M34_PPB_KeyboardInputEvent_GetCharacterText,
    .GetCode = (struct PP_Var (*)(PP_Resource key_event))&Pnacl_M34_PPB_KeyboardInputEvent_GetCode
};

static const struct PPB_TouchInputEvent_1_0 Pnacl_Wrappers_PPB_TouchInputEvent_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, uint32_t modifiers))&Pnacl_M13_PPB_TouchInputEvent_Create,
    .AddTouchPoint = (void (*)(PP_Resource touch_event, PP_TouchListType list, const struct PP_TouchPoint* point))&Pnacl_M13_PPB_TouchInputEvent_AddTouchPoint,
    .IsTouchInputEvent = (PP_Bool (*)(PP_Resource resource))&Pnacl_M13_PPB_TouchInputEvent_IsTouchInputEvent,
    .GetTouchCount = (uint32_t (*)(PP_Resource resource, PP_TouchListType list))&Pnacl_M13_PPB_TouchInputEvent_GetTouchCount,
    .GetTouchByIndex = (struct PP_TouchPoint (*)(PP_Resource resource, PP_TouchListType list, uint32_t index))&Pnacl_M13_PPB_TouchInputEvent_GetTouchByIndex,
    .GetTouchById = (struct PP_TouchPoint (*)(PP_Resource resource, PP_TouchListType list, uint32_t touch_id))&Pnacl_M13_PPB_TouchInputEvent_GetTouchById
};

static const struct PPB_TouchInputEvent_1_4 Pnacl_Wrappers_PPB_TouchInputEvent_1_4 = {
    .Create = (PP_Resource (*)(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, uint32_t modifiers))&Pnacl_M60_PPB_TouchInputEvent_Create,
    .AddTouchPoint = (void (*)(PP_Resource touch_event, PP_TouchListType list, const struct PP_TouchPoint* point))&Pnacl_M60_PPB_TouchInputEvent_AddTouchPoint,
    .IsTouchInputEvent = (PP_Bool (*)(PP_Resource resource))&Pnacl_M60_PPB_TouchInputEvent_IsTouchInputEvent,
    .GetTouchCount = (uint32_t (*)(PP_Resource resource, PP_TouchListType list))&Pnacl_M60_PPB_TouchInputEvent_GetTouchCount,
    .GetTouchByIndex = (struct PP_TouchPoint (*)(PP_Resource resource, PP_TouchListType list, uint32_t index))&Pnacl_M60_PPB_TouchInputEvent_GetTouchByIndex,
    .GetTouchById = (struct PP_TouchPoint (*)(PP_Resource resource, PP_TouchListType list, uint32_t touch_id))&Pnacl_M60_PPB_TouchInputEvent_GetTouchById,
    .GetTouchTiltByIndex = (struct PP_FloatPoint (*)(PP_Resource resource, PP_TouchListType list, uint32_t index))&Pnacl_M60_PPB_TouchInputEvent_GetTouchTiltByIndex,
    .GetTouchTiltById = (struct PP_FloatPoint (*)(PP_Resource resource, PP_TouchListType list, uint32_t touch_id))&Pnacl_M60_PPB_TouchInputEvent_GetTouchTiltById
};

static const struct PPB_IMEInputEvent_1_0 Pnacl_Wrappers_PPB_IMEInputEvent_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, struct PP_Var text, uint32_t segment_number, const uint32_t segment_offsets[], int32_t target_segment, uint32_t selection_start, uint32_t selection_end))&Pnacl_M13_PPB_IMEInputEvent_Create,
    .IsIMEInputEvent = (PP_Bool (*)(PP_Resource resource))&Pnacl_M13_PPB_IMEInputEvent_IsIMEInputEvent,
    .GetText = (struct PP_Var (*)(PP_Resource ime_event))&Pnacl_M13_PPB_IMEInputEvent_GetText,
    .GetSegmentNumber = (uint32_t (*)(PP_Resource ime_event))&Pnacl_M13_PPB_IMEInputEvent_GetSegmentNumber,
    .GetSegmentOffset = (uint32_t (*)(PP_Resource ime_event, uint32_t index))&Pnacl_M13_PPB_IMEInputEvent_GetSegmentOffset,
    .GetTargetSegment = (int32_t (*)(PP_Resource ime_event))&Pnacl_M13_PPB_IMEInputEvent_GetTargetSegment,
    .GetSelection = (void (*)(PP_Resource ime_event, uint32_t* start, uint32_t* end))&Pnacl_M13_PPB_IMEInputEvent_GetSelection
};

/* Not generating wrapper interface for PPB_Instance_1_0 */

static const struct PPB_MediaStreamAudioTrack_0_1 Pnacl_Wrappers_PPB_MediaStreamAudioTrack_0_1 = {
    .IsMediaStreamAudioTrack = (PP_Bool (*)(PP_Resource resource))&Pnacl_M35_PPB_MediaStreamAudioTrack_IsMediaStreamAudioTrack,
    .Configure = (int32_t (*)(PP_Resource audio_track, const int32_t attrib_list[], struct PP_CompletionCallback callback))&Pnacl_M35_PPB_MediaStreamAudioTrack_Configure,
    .GetAttrib = (int32_t (*)(PP_Resource audio_track, PP_MediaStreamAudioTrack_Attrib attrib, int32_t* value))&Pnacl_M35_PPB_MediaStreamAudioTrack_GetAttrib,
    .GetId = (struct PP_Var (*)(PP_Resource audio_track))&Pnacl_M35_PPB_MediaStreamAudioTrack_GetId,
    .HasEnded = (PP_Bool (*)(PP_Resource audio_track))&Pnacl_M35_PPB_MediaStreamAudioTrack_HasEnded,
    .GetBuffer = (int32_t (*)(PP_Resource audio_track, PP_Resource* buffer, struct PP_CompletionCallback callback))&Pnacl_M35_PPB_MediaStreamAudioTrack_GetBuffer,
    .RecycleBuffer = (int32_t (*)(PP_Resource audio_track, PP_Resource buffer))&Pnacl_M35_PPB_MediaStreamAudioTrack_RecycleBuffer,
    .Close = (void (*)(PP_Resource audio_track))&Pnacl_M35_PPB_MediaStreamAudioTrack_Close
};

static const struct PPB_MediaStreamVideoTrack_0_1 Pnacl_Wrappers_PPB_MediaStreamVideoTrack_0_1 = {
    .IsMediaStreamVideoTrack = (PP_Bool (*)(PP_Resource resource))&Pnacl_M35_PPB_MediaStreamVideoTrack_IsMediaStreamVideoTrack,
    .Configure = (int32_t (*)(PP_Resource video_track, const int32_t attrib_list[], struct PP_CompletionCallback callback))&Pnacl_M35_PPB_MediaStreamVideoTrack_Configure,
    .GetAttrib = (int32_t (*)(PP_Resource video_track, PP_MediaStreamVideoTrack_Attrib attrib, int32_t* value))&Pnacl_M35_PPB_MediaStreamVideoTrack_GetAttrib,
    .GetId = (struct PP_Var (*)(PP_Resource video_track))&Pnacl_M35_PPB_MediaStreamVideoTrack_GetId,
    .HasEnded = (PP_Bool (*)(PP_Resource video_track))&Pnacl_M35_PPB_MediaStreamVideoTrack_HasEnded,
    .GetFrame = (int32_t (*)(PP_Resource video_track, PP_Resource* frame, struct PP_CompletionCallback callback))&Pnacl_M35_PPB_MediaStreamVideoTrack_GetFrame,
    .RecycleFrame = (int32_t (*)(PP_Resource video_track, PP_Resource frame))&Pnacl_M35_PPB_MediaStreamVideoTrack_RecycleFrame,
    .Close = (void (*)(PP_Resource video_track))&Pnacl_M35_PPB_MediaStreamVideoTrack_Close
};

static const struct PPB_MediaStreamVideoTrack_1_0 Pnacl_Wrappers_PPB_MediaStreamVideoTrack_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M36_PPB_MediaStreamVideoTrack_Create,
    .IsMediaStreamVideoTrack = (PP_Bool (*)(PP_Resource resource))&Pnacl_M36_PPB_MediaStreamVideoTrack_IsMediaStreamVideoTrack,
    .Configure = (int32_t (*)(PP_Resource video_track, const int32_t attrib_list[], struct PP_CompletionCallback callback))&Pnacl_M36_PPB_MediaStreamVideoTrack_Configure,
    .GetAttrib = (int32_t (*)(PP_Resource video_track, PP_MediaStreamVideoTrack_Attrib attrib, int32_t* value))&Pnacl_M36_PPB_MediaStreamVideoTrack_GetAttrib,
    .GetId = (struct PP_Var (*)(PP_Resource video_track))&Pnacl_M36_PPB_MediaStreamVideoTrack_GetId,
    .HasEnded = (PP_Bool (*)(PP_Resource video_track))&Pnacl_M36_PPB_MediaStreamVideoTrack_HasEnded,
    .GetFrame = (int32_t (*)(PP_Resource video_track, PP_Resource* frame, struct PP_CompletionCallback callback))&Pnacl_M36_PPB_MediaStreamVideoTrack_GetFrame,
    .RecycleFrame = (int32_t (*)(PP_Resource video_track, PP_Resource frame))&Pnacl_M36_PPB_MediaStreamVideoTrack_RecycleFrame,
    .Close = (void (*)(PP_Resource video_track))&Pnacl_M36_PPB_MediaStreamVideoTrack_Close,
    .GetEmptyFrame = (int32_t (*)(PP_Resource video_track, PP_Resource* frame, struct PP_CompletionCallback callback))&Pnacl_M36_PPB_MediaStreamVideoTrack_GetEmptyFrame,
    .PutFrame = (int32_t (*)(PP_Resource video_track, PP_Resource frame))&Pnacl_M36_PPB_MediaStreamVideoTrack_PutFrame
};

static const struct PPB_MessageLoop_1_0 Pnacl_Wrappers_PPB_MessageLoop_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M25_PPB_MessageLoop_Create,
    .GetForMainThread = (PP_Resource (*)(void))&Pnacl_M25_PPB_MessageLoop_GetForMainThread,
    .GetCurrent = (PP_Resource (*)(void))&Pnacl_M25_PPB_MessageLoop_GetCurrent,
    .AttachToCurrentThread = (int32_t (*)(PP_Resource message_loop))&Pnacl_M25_PPB_MessageLoop_AttachToCurrentThread,
    .Run = (int32_t (*)(PP_Resource message_loop))&Pnacl_M25_PPB_MessageLoop_Run,
    .PostWork = (int32_t (*)(PP_Resource message_loop, struct PP_CompletionCallback callback, int64_t delay_ms))&Pnacl_M25_PPB_MessageLoop_PostWork,
    .PostQuit = (int32_t (*)(PP_Resource message_loop, PP_Bool should_destroy))&Pnacl_M25_PPB_MessageLoop_PostQuit
};

static const struct PPB_Messaging_1_0 Pnacl_Wrappers_PPB_Messaging_1_0 = {
    .PostMessage = (void (*)(PP_Instance instance, struct PP_Var message))&Pnacl_M14_PPB_Messaging_PostMessage
};

static const struct PPB_Messaging_1_2 Pnacl_Wrappers_PPB_Messaging_1_2 = {
    .PostMessage = (void (*)(PP_Instance instance, struct PP_Var message))&Pnacl_M39_PPB_Messaging_PostMessage,
    .RegisterMessageHandler = (int32_t (*)(PP_Instance instance, void* user_data, const struct PPP_MessageHandler_0_2* handler, PP_Resource message_loop))&Pnacl_M39_PPB_Messaging_RegisterMessageHandler,
    .UnregisterMessageHandler = (void (*)(PP_Instance instance))&Pnacl_M39_PPB_Messaging_UnregisterMessageHandler
};

/* Not generating wrapper interface for PPB_MouseCursor_1_0 */

static const struct PPB_MouseLock_1_0 Pnacl_Wrappers_PPB_MouseLock_1_0 = {
    .LockMouse = (int32_t (*)(PP_Instance instance, struct PP_CompletionCallback callback))&Pnacl_M16_PPB_MouseLock_LockMouse,
    .UnlockMouse = (void (*)(PP_Instance instance))&Pnacl_M16_PPB_MouseLock_UnlockMouse
};

static const struct PPB_NetAddress_1_0 Pnacl_Wrappers_PPB_NetAddress_1_0 = {
    .CreateFromIPv4Address = (PP_Resource (*)(PP_Instance instance, const struct PP_NetAddress_IPv4* ipv4_addr))&Pnacl_M29_PPB_NetAddress_CreateFromIPv4Address,
    .CreateFromIPv6Address = (PP_Resource (*)(PP_Instance instance, const struct PP_NetAddress_IPv6* ipv6_addr))&Pnacl_M29_PPB_NetAddress_CreateFromIPv6Address,
    .IsNetAddress = (PP_Bool (*)(PP_Resource resource))&Pnacl_M29_PPB_NetAddress_IsNetAddress,
    .GetFamily = (PP_NetAddress_Family (*)(PP_Resource addr))&Pnacl_M29_PPB_NetAddress_GetFamily,
    .DescribeAsString = (struct PP_Var (*)(PP_Resource addr, PP_Bool include_port))&Pnacl_M29_PPB_NetAddress_DescribeAsString,
    .DescribeAsIPv4Address = (PP_Bool (*)(PP_Resource addr, struct PP_NetAddress_IPv4* ipv4_addr))&Pnacl_M29_PPB_NetAddress_DescribeAsIPv4Address,
    .DescribeAsIPv6Address = (PP_Bool (*)(PP_Resource addr, struct PP_NetAddress_IPv6* ipv6_addr))&Pnacl_M29_PPB_NetAddress_DescribeAsIPv6Address
};

static const struct PPB_NetworkList_1_0 Pnacl_Wrappers_PPB_NetworkList_1_0 = {
    .IsNetworkList = (PP_Bool (*)(PP_Resource resource))&Pnacl_M31_PPB_NetworkList_IsNetworkList,
    .GetCount = (uint32_t (*)(PP_Resource resource))&Pnacl_M31_PPB_NetworkList_GetCount,
    .GetName = (struct PP_Var (*)(PP_Resource resource, uint32_t index))&Pnacl_M31_PPB_NetworkList_GetName,
    .GetType = (PP_NetworkList_Type (*)(PP_Resource resource, uint32_t index))&Pnacl_M31_PPB_NetworkList_GetType,
    .GetState = (PP_NetworkList_State (*)(PP_Resource resource, uint32_t index))&Pnacl_M31_PPB_NetworkList_GetState,
    .GetIpAddresses = (int32_t (*)(PP_Resource resource, uint32_t index, struct PP_ArrayOutput output))&Pnacl_M31_PPB_NetworkList_GetIpAddresses,
    .GetDisplayName = (struct PP_Var (*)(PP_Resource resource, uint32_t index))&Pnacl_M31_PPB_NetworkList_GetDisplayName,
    .GetMTU = (uint32_t (*)(PP_Resource resource, uint32_t index))&Pnacl_M31_PPB_NetworkList_GetMTU
};

static const struct PPB_NetworkMonitor_1_0 Pnacl_Wrappers_PPB_NetworkMonitor_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M31_PPB_NetworkMonitor_Create,
    .UpdateNetworkList = (int32_t (*)(PP_Resource network_monitor, PP_Resource* network_list, struct PP_CompletionCallback callback))&Pnacl_M31_PPB_NetworkMonitor_UpdateNetworkList,
    .IsNetworkMonitor = (PP_Bool (*)(PP_Resource resource))&Pnacl_M31_PPB_NetworkMonitor_IsNetworkMonitor
};

static const struct PPB_NetworkProxy_1_0 Pnacl_Wrappers_PPB_NetworkProxy_1_0 = {
    .GetProxyForURL = (int32_t (*)(PP_Instance instance, struct PP_Var url, struct PP_Var* proxy_string, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_NetworkProxy_GetProxyForURL
};

/* Not generating wrapper interface for PPB_OpenGLES2_1_0 */

/* Not generating wrapper interface for PPB_OpenGLES2InstancedArrays_1_0 */

/* Not generating wrapper interface for PPB_OpenGLES2FramebufferBlit_1_0 */

/* Not generating wrapper interface for PPB_OpenGLES2FramebufferMultisample_1_0 */

/* Not generating wrapper interface for PPB_OpenGLES2ChromiumEnableFeature_1_0 */

/* Not generating wrapper interface for PPB_OpenGLES2ChromiumMapSub_1_0 */

/* Not generating wrapper interface for PPB_OpenGLES2Query_1_0 */

/* Not generating wrapper interface for PPB_OpenGLES2VertexArrayObject_1_0 */

static const struct PPB_TCPSocket_1_0 Pnacl_Wrappers_PPB_TCPSocket_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M29_PPB_TCPSocket_Create,
    .IsTCPSocket = (PP_Bool (*)(PP_Resource resource))&Pnacl_M29_PPB_TCPSocket_IsTCPSocket,
    .Connect = (int32_t (*)(PP_Resource tcp_socket, PP_Resource addr, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_TCPSocket_Connect,
    .GetLocalAddress = (PP_Resource (*)(PP_Resource tcp_socket))&Pnacl_M29_PPB_TCPSocket_GetLocalAddress,
    .GetRemoteAddress = (PP_Resource (*)(PP_Resource tcp_socket))&Pnacl_M29_PPB_TCPSocket_GetRemoteAddress,
    .Read = (int32_t (*)(PP_Resource tcp_socket, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_TCPSocket_Read,
    .Write = (int32_t (*)(PP_Resource tcp_socket, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_TCPSocket_Write,
    .Close = (void (*)(PP_Resource tcp_socket))&Pnacl_M29_PPB_TCPSocket_Close,
    .SetOption = (int32_t (*)(PP_Resource tcp_socket, PP_TCPSocket_Option name, struct PP_Var value, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_TCPSocket_SetOption
};

static const struct PPB_TCPSocket_1_1 Pnacl_Wrappers_PPB_TCPSocket_1_1 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M31_PPB_TCPSocket_Create,
    .IsTCPSocket = (PP_Bool (*)(PP_Resource resource))&Pnacl_M31_PPB_TCPSocket_IsTCPSocket,
    .Bind = (int32_t (*)(PP_Resource tcp_socket, PP_Resource addr, struct PP_CompletionCallback callback))&Pnacl_M31_PPB_TCPSocket_Bind,
    .Connect = (int32_t (*)(PP_Resource tcp_socket, PP_Resource addr, struct PP_CompletionCallback callback))&Pnacl_M31_PPB_TCPSocket_Connect,
    .GetLocalAddress = (PP_Resource (*)(PP_Resource tcp_socket))&Pnacl_M31_PPB_TCPSocket_GetLocalAddress,
    .GetRemoteAddress = (PP_Resource (*)(PP_Resource tcp_socket))&Pnacl_M31_PPB_TCPSocket_GetRemoteAddress,
    .Read = (int32_t (*)(PP_Resource tcp_socket, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback callback))&Pnacl_M31_PPB_TCPSocket_Read,
    .Write = (int32_t (*)(PP_Resource tcp_socket, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback callback))&Pnacl_M31_PPB_TCPSocket_Write,
    .Listen = (int32_t (*)(PP_Resource tcp_socket, int32_t backlog, struct PP_CompletionCallback callback))&Pnacl_M31_PPB_TCPSocket_Listen,
    .Accept = (int32_t (*)(PP_Resource tcp_socket, PP_Resource* accepted_tcp_socket, struct PP_CompletionCallback callback))&Pnacl_M31_PPB_TCPSocket_Accept,
    .Close = (void (*)(PP_Resource tcp_socket))&Pnacl_M31_PPB_TCPSocket_Close,
    .SetOption = (int32_t (*)(PP_Resource tcp_socket, PP_TCPSocket_Option name, struct PP_Var value, struct PP_CompletionCallback callback))&Pnacl_M31_PPB_TCPSocket_SetOption
};

static const struct PPB_TCPSocket_1_2 Pnacl_Wrappers_PPB_TCPSocket_1_2 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M41_PPB_TCPSocket_Create,
    .IsTCPSocket = (PP_Bool (*)(PP_Resource resource))&Pnacl_M41_PPB_TCPSocket_IsTCPSocket,
    .Bind = (int32_t (*)(PP_Resource tcp_socket, PP_Resource addr, struct PP_CompletionCallback callback))&Pnacl_M41_PPB_TCPSocket_Bind,
    .Connect = (int32_t (*)(PP_Resource tcp_socket, PP_Resource addr, struct PP_CompletionCallback callback))&Pnacl_M41_PPB_TCPSocket_Connect,
    .GetLocalAddress = (PP_Resource (*)(PP_Resource tcp_socket))&Pnacl_M41_PPB_TCPSocket_GetLocalAddress,
    .GetRemoteAddress = (PP_Resource (*)(PP_Resource tcp_socket))&Pnacl_M41_PPB_TCPSocket_GetRemoteAddress,
    .Read = (int32_t (*)(PP_Resource tcp_socket, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback callback))&Pnacl_M41_PPB_TCPSocket_Read,
    .Write = (int32_t (*)(PP_Resource tcp_socket, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback callback))&Pnacl_M41_PPB_TCPSocket_Write,
    .Listen = (int32_t (*)(PP_Resource tcp_socket, int32_t backlog, struct PP_CompletionCallback callback))&Pnacl_M41_PPB_TCPSocket_Listen,
    .Accept = (int32_t (*)(PP_Resource tcp_socket, PP_Resource* accepted_tcp_socket, struct PP_CompletionCallback callback))&Pnacl_M41_PPB_TCPSocket_Accept,
    .Close = (void (*)(PP_Resource tcp_socket))&Pnacl_M41_PPB_TCPSocket_Close,
    .SetOption = (int32_t (*)(PP_Resource tcp_socket, PP_TCPSocket_Option name, struct PP_Var value, struct PP_CompletionCallback callback))&Pnacl_M41_PPB_TCPSocket_SetOption
};

static const struct PPB_TextInputController_1_0 Pnacl_Wrappers_PPB_TextInputController_1_0 = {
    .SetTextInputType = (void (*)(PP_Instance instance, PP_TextInput_Type type))&Pnacl_M30_PPB_TextInputController_SetTextInputType,
    .UpdateCaretPosition = (void (*)(PP_Instance instance, const struct PP_Rect* caret))&Pnacl_M30_PPB_TextInputController_UpdateCaretPosition,
    .CancelCompositionText = (void (*)(PP_Instance instance))&Pnacl_M30_PPB_TextInputController_CancelCompositionText,
    .UpdateSurroundingText = (void (*)(PP_Instance instance, struct PP_Var text, uint32_t caret, uint32_t anchor))&Pnacl_M30_PPB_TextInputController_UpdateSurroundingText
};

static const struct PPB_UDPSocket_1_0 Pnacl_Wrappers_PPB_UDPSocket_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M29_PPB_UDPSocket_Create,
    .IsUDPSocket = (PP_Bool (*)(PP_Resource resource))&Pnacl_M29_PPB_UDPSocket_IsUDPSocket,
    .Bind = (int32_t (*)(PP_Resource udp_socket, PP_Resource addr, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_UDPSocket_Bind,
    .GetBoundAddress = (PP_Resource (*)(PP_Resource udp_socket))&Pnacl_M29_PPB_UDPSocket_GetBoundAddress,
    .RecvFrom = (int32_t (*)(PP_Resource udp_socket, char* buffer, int32_t num_bytes, PP_Resource* addr, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_UDPSocket_RecvFrom,
    .SendTo = (int32_t (*)(PP_Resource udp_socket, const char* buffer, int32_t num_bytes, PP_Resource addr, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_UDPSocket_SendTo,
    .Close = (void (*)(PP_Resource udp_socket))&Pnacl_M29_PPB_UDPSocket_Close,
    .SetOption = (int32_t (*)(PP_Resource udp_socket, PP_UDPSocket_Option name, struct PP_Var value, struct PP_CompletionCallback callback))&Pnacl_M29_PPB_UDPSocket_SetOption
};

static const struct PPB_UDPSocket_1_1 Pnacl_Wrappers_PPB_UDPSocket_1_1 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M41_PPB_UDPSocket_Create,
    .IsUDPSocket = (PP_Bool (*)(PP_Resource resource))&Pnacl_M41_PPB_UDPSocket_IsUDPSocket,
    .Bind = (int32_t (*)(PP_Resource udp_socket, PP_Resource addr, struct PP_CompletionCallback callback))&Pnacl_M41_PPB_UDPSocket_Bind,
    .GetBoundAddress = (PP_Resource (*)(PP_Resource udp_socket))&Pnacl_M41_PPB_UDPSocket_GetBoundAddress,
    .RecvFrom = (int32_t (*)(PP_Resource udp_socket, char* buffer, int32_t num_bytes, PP_Resource* addr, struct PP_CompletionCallback callback))&Pnacl_M41_PPB_UDPSocket_RecvFrom,
    .SendTo = (int32_t (*)(PP_Resource udp_socket, const char* buffer, int32_t num_bytes, PP_Resource addr, struct PP_CompletionCallback callback))&Pnacl_M41_PPB_UDPSocket_SendTo,
    .Close = (void (*)(PP_Resource udp_socket))&Pnacl_M41_PPB_UDPSocket_Close,
    .SetOption = (int32_t (*)(PP_Resource udp_socket, PP_UDPSocket_Option name, struct PP_Var value, struct PP_CompletionCallback callback))&Pnacl_M41_PPB_UDPSocket_SetOption
};

static const struct PPB_UDPSocket_1_2 Pnacl_Wrappers_PPB_UDPSocket_1_2 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M43_PPB_UDPSocket_Create,
    .IsUDPSocket = (PP_Bool (*)(PP_Resource resource))&Pnacl_M43_PPB_UDPSocket_IsUDPSocket,
    .Bind = (int32_t (*)(PP_Resource udp_socket, PP_Resource addr, struct PP_CompletionCallback callback))&Pnacl_M43_PPB_UDPSocket_Bind,
    .GetBoundAddress = (PP_Resource (*)(PP_Resource udp_socket))&Pnacl_M43_PPB_UDPSocket_GetBoundAddress,
    .RecvFrom = (int32_t (*)(PP_Resource udp_socket, char* buffer, int32_t num_bytes, PP_Resource* addr, struct PP_CompletionCallback callback))&Pnacl_M43_PPB_UDPSocket_RecvFrom,
    .SendTo = (int32_t (*)(PP_Resource udp_socket, const char* buffer, int32_t num_bytes, PP_Resource addr, struct PP_CompletionCallback callback))&Pnacl_M43_PPB_UDPSocket_SendTo,
    .Close = (void (*)(PP_Resource udp_socket))&Pnacl_M43_PPB_UDPSocket_Close,
    .SetOption = (int32_t (*)(PP_Resource udp_socket, PP_UDPSocket_Option name, struct PP_Var value, struct PP_CompletionCallback callback))&Pnacl_M43_PPB_UDPSocket_SetOption,
    .JoinGroup = (int32_t (*)(PP_Resource udp_socket, PP_Resource group, struct PP_CompletionCallback callback))&Pnacl_M43_PPB_UDPSocket_JoinGroup,
    .LeaveGroup = (int32_t (*)(PP_Resource udp_socket, PP_Resource group, struct PP_CompletionCallback callback))&Pnacl_M43_PPB_UDPSocket_LeaveGroup
};

static const struct PPB_URLLoader_1_0 Pnacl_Wrappers_PPB_URLLoader_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M14_PPB_URLLoader_Create,
    .IsURLLoader = (PP_Bool (*)(PP_Resource resource))&Pnacl_M14_PPB_URLLoader_IsURLLoader,
    .Open = (int32_t (*)(PP_Resource loader, PP_Resource request_info, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_URLLoader_Open,
    .FollowRedirect = (int32_t (*)(PP_Resource loader, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_URLLoader_FollowRedirect,
    .GetUploadProgress = (PP_Bool (*)(PP_Resource loader, int64_t* bytes_sent, int64_t* total_bytes_to_be_sent))&Pnacl_M14_PPB_URLLoader_GetUploadProgress,
    .GetDownloadProgress = (PP_Bool (*)(PP_Resource loader, int64_t* bytes_received, int64_t* total_bytes_to_be_received))&Pnacl_M14_PPB_URLLoader_GetDownloadProgress,
    .GetResponseInfo = (PP_Resource (*)(PP_Resource loader))&Pnacl_M14_PPB_URLLoader_GetResponseInfo,
    .ReadResponseBody = (int32_t (*)(PP_Resource loader, void* buffer, int32_t bytes_to_read, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_URLLoader_ReadResponseBody,
    .FinishStreamingToFile = (int32_t (*)(PP_Resource loader, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_URLLoader_FinishStreamingToFile,
    .Close = (void (*)(PP_Resource loader))&Pnacl_M14_PPB_URLLoader_Close
};

static const struct PPB_URLRequestInfo_1_0 Pnacl_Wrappers_PPB_URLRequestInfo_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M14_PPB_URLRequestInfo_Create,
    .IsURLRequestInfo = (PP_Bool (*)(PP_Resource resource))&Pnacl_M14_PPB_URLRequestInfo_IsURLRequestInfo,
    .SetProperty = (PP_Bool (*)(PP_Resource request, PP_URLRequestProperty property, struct PP_Var value))&Pnacl_M14_PPB_URLRequestInfo_SetProperty,
    .AppendDataToBody = (PP_Bool (*)(PP_Resource request, const void* data, uint32_t len))&Pnacl_M14_PPB_URLRequestInfo_AppendDataToBody,
    .AppendFileToBody = (PP_Bool (*)(PP_Resource request, PP_Resource file_ref, int64_t start_offset, int64_t number_of_bytes, PP_Time expected_last_modified_time))&Pnacl_M14_PPB_URLRequestInfo_AppendFileToBody
};

static const struct PPB_URLResponseInfo_1_0 Pnacl_Wrappers_PPB_URLResponseInfo_1_0 = {
    .IsURLResponseInfo = (PP_Bool (*)(PP_Resource resource))&Pnacl_M14_PPB_URLResponseInfo_IsURLResponseInfo,
    .GetProperty = (struct PP_Var (*)(PP_Resource response, PP_URLResponseProperty property))&Pnacl_M14_PPB_URLResponseInfo_GetProperty,
    .GetBodyAsFileRef = (PP_Resource (*)(PP_Resource response))&Pnacl_M14_PPB_URLResponseInfo_GetBodyAsFileRef
};

static const struct PPB_Var_1_0 Pnacl_Wrappers_PPB_Var_1_0 = {
    .AddRef = (void (*)(struct PP_Var var))&Pnacl_M14_PPB_Var_AddRef,
    .Release = (void (*)(struct PP_Var var))&Pnacl_M14_PPB_Var_Release,
    .VarFromUtf8 = (struct PP_Var (*)(PP_Module module, const char* data, uint32_t len))&Pnacl_M14_PPB_Var_VarFromUtf8,
    .VarToUtf8 = (const char* (*)(struct PP_Var var, uint32_t* len))&Pnacl_M14_PPB_Var_VarToUtf8
};

static const struct PPB_Var_1_1 Pnacl_Wrappers_PPB_Var_1_1 = {
    .AddRef = (void (*)(struct PP_Var var))&Pnacl_M18_PPB_Var_AddRef,
    .Release = (void (*)(struct PP_Var var))&Pnacl_M18_PPB_Var_Release,
    .VarFromUtf8 = (struct PP_Var (*)(const char* data, uint32_t len))&Pnacl_M18_PPB_Var_VarFromUtf8,
    .VarToUtf8 = (const char* (*)(struct PP_Var var, uint32_t* len))&Pnacl_M18_PPB_Var_VarToUtf8
};

static const struct PPB_Var_1_2 Pnacl_Wrappers_PPB_Var_1_2 = {
    .AddRef = (void (*)(struct PP_Var var))&Pnacl_M34_PPB_Var_AddRef,
    .Release = (void (*)(struct PP_Var var))&Pnacl_M34_PPB_Var_Release,
    .VarFromUtf8 = (struct PP_Var (*)(const char* data, uint32_t len))&Pnacl_M34_PPB_Var_VarFromUtf8,
    .VarToUtf8 = (const char* (*)(struct PP_Var var, uint32_t* len))&Pnacl_M34_PPB_Var_VarToUtf8,
    .VarToResource = (PP_Resource (*)(struct PP_Var var))&Pnacl_M34_PPB_Var_VarToResource,
    .VarFromResource = (struct PP_Var (*)(PP_Resource resource))&Pnacl_M34_PPB_Var_VarFromResource
};

static const struct PPB_VarArray_1_0 Pnacl_Wrappers_PPB_VarArray_1_0 = {
    .Create = (struct PP_Var (*)(void))&Pnacl_M29_PPB_VarArray_Create,
    .Get = (struct PP_Var (*)(struct PP_Var array, uint32_t index))&Pnacl_M29_PPB_VarArray_Get,
    .Set = (PP_Bool (*)(struct PP_Var array, uint32_t index, struct PP_Var value))&Pnacl_M29_PPB_VarArray_Set,
    .GetLength = (uint32_t (*)(struct PP_Var array))&Pnacl_M29_PPB_VarArray_GetLength,
    .SetLength = (PP_Bool (*)(struct PP_Var array, uint32_t length))&Pnacl_M29_PPB_VarArray_SetLength
};

static const struct PPB_VarArrayBuffer_1_0 Pnacl_Wrappers_PPB_VarArrayBuffer_1_0 = {
    .Create = (struct PP_Var (*)(uint32_t size_in_bytes))&Pnacl_M18_PPB_VarArrayBuffer_Create,
    .ByteLength = (PP_Bool (*)(struct PP_Var array, uint32_t* byte_length))&Pnacl_M18_PPB_VarArrayBuffer_ByteLength,
    .Map = (void* (*)(struct PP_Var array))&Pnacl_M18_PPB_VarArrayBuffer_Map,
    .Unmap = (void (*)(struct PP_Var array))&Pnacl_M18_PPB_VarArrayBuffer_Unmap
};

static const struct PPB_VarDictionary_1_0 Pnacl_Wrappers_PPB_VarDictionary_1_0 = {
    .Create = (struct PP_Var (*)(void))&Pnacl_M29_PPB_VarDictionary_Create,
    .Get = (struct PP_Var (*)(struct PP_Var dict, struct PP_Var key))&Pnacl_M29_PPB_VarDictionary_Get,
    .Set = (PP_Bool (*)(struct PP_Var dict, struct PP_Var key, struct PP_Var value))&Pnacl_M29_PPB_VarDictionary_Set,
    .Delete = (void (*)(struct PP_Var dict, struct PP_Var key))&Pnacl_M29_PPB_VarDictionary_Delete,
    .HasKey = (PP_Bool (*)(struct PP_Var dict, struct PP_Var key))&Pnacl_M29_PPB_VarDictionary_HasKey,
    .GetKeys = (struct PP_Var (*)(struct PP_Var dict))&Pnacl_M29_PPB_VarDictionary_GetKeys
};

static const struct PPB_VideoDecoder_0_1 Pnacl_Wrappers_PPB_VideoDecoder_0_1 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M36_PPB_VideoDecoder_Create,
    .IsVideoDecoder = (PP_Bool (*)(PP_Resource resource))&Pnacl_M36_PPB_VideoDecoder_IsVideoDecoder,
    .Initialize = (int32_t (*)(PP_Resource video_decoder, PP_Resource graphics3d_context, PP_VideoProfile profile, PP_Bool allow_software_fallback, struct PP_CompletionCallback callback))&Pnacl_M36_PPB_VideoDecoder_Initialize,
    .Decode = (int32_t (*)(PP_Resource video_decoder, uint32_t decode_id, uint32_t size, const void* buffer, struct PP_CompletionCallback callback))&Pnacl_M36_PPB_VideoDecoder_Decode,
    .GetPicture = (int32_t (*)(PP_Resource video_decoder, struct PP_VideoPicture_0_1* picture, struct PP_CompletionCallback callback))&Pnacl_M36_PPB_VideoDecoder_GetPicture,
    .RecyclePicture = (void (*)(PP_Resource video_decoder, const struct PP_VideoPicture* picture))&Pnacl_M36_PPB_VideoDecoder_RecyclePicture,
    .Flush = (int32_t (*)(PP_Resource video_decoder, struct PP_CompletionCallback callback))&Pnacl_M36_PPB_VideoDecoder_Flush,
    .Reset = (int32_t (*)(PP_Resource video_decoder, struct PP_CompletionCallback callback))&Pnacl_M36_PPB_VideoDecoder_Reset
};

static const struct PPB_VideoDecoder_0_2 Pnacl_Wrappers_PPB_VideoDecoder_0_2 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M39_PPB_VideoDecoder_Create,
    .IsVideoDecoder = (PP_Bool (*)(PP_Resource resource))&Pnacl_M39_PPB_VideoDecoder_IsVideoDecoder,
    .Initialize = (int32_t (*)(PP_Resource video_decoder, PP_Resource graphics3d_context, PP_VideoProfile profile, PP_HardwareAcceleration acceleration, struct PP_CompletionCallback callback))&Pnacl_M39_PPB_VideoDecoder_Initialize,
    .Decode = (int32_t (*)(PP_Resource video_decoder, uint32_t decode_id, uint32_t size, const void* buffer, struct PP_CompletionCallback callback))&Pnacl_M39_PPB_VideoDecoder_Decode,
    .GetPicture = (int32_t (*)(PP_Resource video_decoder, struct PP_VideoPicture_0_1* picture, struct PP_CompletionCallback callback))&Pnacl_M39_PPB_VideoDecoder_GetPicture,
    .RecyclePicture = (void (*)(PP_Resource video_decoder, const struct PP_VideoPicture* picture))&Pnacl_M39_PPB_VideoDecoder_RecyclePicture,
    .Flush = (int32_t (*)(PP_Resource video_decoder, struct PP_CompletionCallback callback))&Pnacl_M39_PPB_VideoDecoder_Flush,
    .Reset = (int32_t (*)(PP_Resource video_decoder, struct PP_CompletionCallback callback))&Pnacl_M39_PPB_VideoDecoder_Reset
};

static const struct PPB_VideoDecoder_1_0 Pnacl_Wrappers_PPB_VideoDecoder_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M40_PPB_VideoDecoder_Create,
    .IsVideoDecoder = (PP_Bool (*)(PP_Resource resource))&Pnacl_M40_PPB_VideoDecoder_IsVideoDecoder,
    .Initialize = (int32_t (*)(PP_Resource video_decoder, PP_Resource graphics3d_context, PP_VideoProfile profile, PP_HardwareAcceleration acceleration, struct PP_CompletionCallback callback))&Pnacl_M40_PPB_VideoDecoder_Initialize,
    .Decode = (int32_t (*)(PP_Resource video_decoder, uint32_t decode_id, uint32_t size, const void* buffer, struct PP_CompletionCallback callback))&Pnacl_M40_PPB_VideoDecoder_Decode,
    .GetPicture = (int32_t (*)(PP_Resource video_decoder, struct PP_VideoPicture* picture, struct PP_CompletionCallback callback))&Pnacl_M40_PPB_VideoDecoder_GetPicture,
    .RecyclePicture = (void (*)(PP_Resource video_decoder, const struct PP_VideoPicture* picture))&Pnacl_M40_PPB_VideoDecoder_RecyclePicture,
    .Flush = (int32_t (*)(PP_Resource video_decoder, struct PP_CompletionCallback callback))&Pnacl_M40_PPB_VideoDecoder_Flush,
    .Reset = (int32_t (*)(PP_Resource video_decoder, struct PP_CompletionCallback callback))&Pnacl_M40_PPB_VideoDecoder_Reset
};

static const struct PPB_VideoDecoder_1_1 Pnacl_Wrappers_PPB_VideoDecoder_1_1 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M46_PPB_VideoDecoder_Create,
    .IsVideoDecoder = (PP_Bool (*)(PP_Resource resource))&Pnacl_M46_PPB_VideoDecoder_IsVideoDecoder,
    .Initialize = (int32_t (*)(PP_Resource video_decoder, PP_Resource graphics3d_context, PP_VideoProfile profile, PP_HardwareAcceleration acceleration, uint32_t min_picture_count, struct PP_CompletionCallback callback))&Pnacl_M46_PPB_VideoDecoder_Initialize,
    .Decode = (int32_t (*)(PP_Resource video_decoder, uint32_t decode_id, uint32_t size, const void* buffer, struct PP_CompletionCallback callback))&Pnacl_M46_PPB_VideoDecoder_Decode,
    .GetPicture = (int32_t (*)(PP_Resource video_decoder, struct PP_VideoPicture* picture, struct PP_CompletionCallback callback))&Pnacl_M46_PPB_VideoDecoder_GetPicture,
    .RecyclePicture = (void (*)(PP_Resource video_decoder, const struct PP_VideoPicture* picture))&Pnacl_M46_PPB_VideoDecoder_RecyclePicture,
    .Flush = (int32_t (*)(PP_Resource video_decoder, struct PP_CompletionCallback callback))&Pnacl_M46_PPB_VideoDecoder_Flush,
    .Reset = (int32_t (*)(PP_Resource video_decoder, struct PP_CompletionCallback callback))&Pnacl_M46_PPB_VideoDecoder_Reset
};

static const struct PPB_VideoEncoder_0_1 Pnacl_Wrappers_PPB_VideoEncoder_0_1 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M42_PPB_VideoEncoder_Create,
    .IsVideoEncoder = (PP_Bool (*)(PP_Resource resource))&Pnacl_M42_PPB_VideoEncoder_IsVideoEncoder,
    .GetSupportedProfiles = (int32_t (*)(PP_Resource video_encoder, struct PP_ArrayOutput output, struct PP_CompletionCallback callback))&Pnacl_M42_PPB_VideoEncoder_GetSupportedProfiles,
    .Initialize = (int32_t (*)(PP_Resource video_encoder, PP_VideoFrame_Format input_format, const struct PP_Size* input_visible_size, PP_VideoProfile output_profile, uint32_t initial_bitrate, PP_HardwareAcceleration acceleration, struct PP_CompletionCallback callback))&Pnacl_M42_PPB_VideoEncoder_Initialize,
    .GetFramesRequired = (int32_t (*)(PP_Resource video_encoder))&Pnacl_M42_PPB_VideoEncoder_GetFramesRequired,
    .GetFrameCodedSize = (int32_t (*)(PP_Resource video_encoder, struct PP_Size* coded_size))&Pnacl_M42_PPB_VideoEncoder_GetFrameCodedSize,
    .GetVideoFrame = (int32_t (*)(PP_Resource video_encoder, PP_Resource* video_frame, struct PP_CompletionCallback callback))&Pnacl_M42_PPB_VideoEncoder_GetVideoFrame,
    .Encode = (int32_t (*)(PP_Resource video_encoder, PP_Resource video_frame, PP_Bool force_keyframe, struct PP_CompletionCallback callback))&Pnacl_M42_PPB_VideoEncoder_Encode,
    .GetBitstreamBuffer = (int32_t (*)(PP_Resource video_encoder, struct PP_BitstreamBuffer* bitstream_buffer, struct PP_CompletionCallback callback))&Pnacl_M42_PPB_VideoEncoder_GetBitstreamBuffer,
    .RecycleBitstreamBuffer = (void (*)(PP_Resource video_encoder, const struct PP_BitstreamBuffer* bitstream_buffer))&Pnacl_M42_PPB_VideoEncoder_RecycleBitstreamBuffer,
    .RequestEncodingParametersChange = (void (*)(PP_Resource video_encoder, uint32_t bitrate, uint32_t framerate))&Pnacl_M42_PPB_VideoEncoder_RequestEncodingParametersChange,
    .Close = (void (*)(PP_Resource video_encoder))&Pnacl_M42_PPB_VideoEncoder_Close
};

static const struct PPB_VideoEncoder_0_2 Pnacl_Wrappers_PPB_VideoEncoder_0_2 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M44_PPB_VideoEncoder_Create,
    .IsVideoEncoder = (PP_Bool (*)(PP_Resource resource))&Pnacl_M44_PPB_VideoEncoder_IsVideoEncoder,
    .GetSupportedProfiles = (int32_t (*)(PP_Resource video_encoder, struct PP_ArrayOutput output, struct PP_CompletionCallback callback))&Pnacl_M44_PPB_VideoEncoder_GetSupportedProfiles,
    .Initialize = (int32_t (*)(PP_Resource video_encoder, PP_VideoFrame_Format input_format, const struct PP_Size* input_visible_size, PP_VideoProfile output_profile, uint32_t initial_bitrate, PP_HardwareAcceleration acceleration, struct PP_CompletionCallback callback))&Pnacl_M44_PPB_VideoEncoder_Initialize,
    .GetFramesRequired = (int32_t (*)(PP_Resource video_encoder))&Pnacl_M44_PPB_VideoEncoder_GetFramesRequired,
    .GetFrameCodedSize = (int32_t (*)(PP_Resource video_encoder, struct PP_Size* coded_size))&Pnacl_M44_PPB_VideoEncoder_GetFrameCodedSize,
    .GetVideoFrame = (int32_t (*)(PP_Resource video_encoder, PP_Resource* video_frame, struct PP_CompletionCallback callback))&Pnacl_M44_PPB_VideoEncoder_GetVideoFrame,
    .Encode = (int32_t (*)(PP_Resource video_encoder, PP_Resource video_frame, PP_Bool force_keyframe, struct PP_CompletionCallback callback))&Pnacl_M44_PPB_VideoEncoder_Encode,
    .GetBitstreamBuffer = (int32_t (*)(PP_Resource video_encoder, struct PP_BitstreamBuffer* bitstream_buffer, struct PP_CompletionCallback callback))&Pnacl_M44_PPB_VideoEncoder_GetBitstreamBuffer,
    .RecycleBitstreamBuffer = (void (*)(PP_Resource video_encoder, const struct PP_BitstreamBuffer* bitstream_buffer))&Pnacl_M44_PPB_VideoEncoder_RecycleBitstreamBuffer,
    .RequestEncodingParametersChange = (void (*)(PP_Resource video_encoder, uint32_t bitrate, uint32_t framerate))&Pnacl_M44_PPB_VideoEncoder_RequestEncodingParametersChange,
    .Close = (void (*)(PP_Resource video_encoder))&Pnacl_M44_PPB_VideoEncoder_Close
};

/* Not generating wrapper interface for PPB_VideoFrame_0_1 */

/* Not generating wrapper interface for PPB_View_1_0 */

/* Not generating wrapper interface for PPB_View_1_1 */

/* Not generating wrapper interface for PPB_View_1_2 */

static const struct PPB_VpnProvider_0_1 Pnacl_Wrappers_PPB_VpnProvider_0_1 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M52_PPB_VpnProvider_Create,
    .IsVpnProvider = (PP_Bool (*)(PP_Resource resource))&Pnacl_M52_PPB_VpnProvider_IsVpnProvider,
    .Bind = (int32_t (*)(PP_Resource vpn_provider, struct PP_Var configuration_id, struct PP_Var configuration_name, struct PP_CompletionCallback callback))&Pnacl_M52_PPB_VpnProvider_Bind,
    .SendPacket = (int32_t (*)(PP_Resource vpn_provider, struct PP_Var packet, struct PP_CompletionCallback callback))&Pnacl_M52_PPB_VpnProvider_SendPacket,
    .ReceivePacket = (int32_t (*)(PP_Resource vpn_provider, struct PP_Var* packet, struct PP_CompletionCallback callback))&Pnacl_M52_PPB_VpnProvider_ReceivePacket
};

static const struct PPB_WebSocket_1_0 Pnacl_Wrappers_PPB_WebSocket_1_0 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M18_PPB_WebSocket_Create,
    .IsWebSocket = (PP_Bool (*)(PP_Resource resource))&Pnacl_M18_PPB_WebSocket_IsWebSocket,
    .Connect = (int32_t (*)(PP_Resource web_socket, struct PP_Var url, const struct PP_Var protocols[], uint32_t protocol_count, struct PP_CompletionCallback callback))&Pnacl_M18_PPB_WebSocket_Connect,
    .Close = (int32_t (*)(PP_Resource web_socket, uint16_t code, struct PP_Var reason, struct PP_CompletionCallback callback))&Pnacl_M18_PPB_WebSocket_Close,
    .ReceiveMessage = (int32_t (*)(PP_Resource web_socket, struct PP_Var* message, struct PP_CompletionCallback callback))&Pnacl_M18_PPB_WebSocket_ReceiveMessage,
    .SendMessage = (int32_t (*)(PP_Resource web_socket, struct PP_Var message))&Pnacl_M18_PPB_WebSocket_SendMessage,
    .GetBufferedAmount = (uint64_t (*)(PP_Resource web_socket))&Pnacl_M18_PPB_WebSocket_GetBufferedAmount,
    .GetCloseCode = (uint16_t (*)(PP_Resource web_socket))&Pnacl_M18_PPB_WebSocket_GetCloseCode,
    .GetCloseReason = (struct PP_Var (*)(PP_Resource web_socket))&Pnacl_M18_PPB_WebSocket_GetCloseReason,
    .GetCloseWasClean = (PP_Bool (*)(PP_Resource web_socket))&Pnacl_M18_PPB_WebSocket_GetCloseWasClean,
    .GetExtensions = (struct PP_Var (*)(PP_Resource web_socket))&Pnacl_M18_PPB_WebSocket_GetExtensions,
    .GetProtocol = (struct PP_Var (*)(PP_Resource web_socket))&Pnacl_M18_PPB_WebSocket_GetProtocol,
    .GetReadyState = (PP_WebSocketReadyState (*)(PP_Resource web_socket))&Pnacl_M18_PPB_WebSocket_GetReadyState,
    .GetURL = (struct PP_Var (*)(PP_Resource web_socket))&Pnacl_M18_PPB_WebSocket_GetURL
};

/* Not generating wrapper interface for PPP_Graphics3D_1_0 */

/* Not generating wrapper interface for PPP_InputEvent_0_1 */

/* Not generating wrapper interface for PPP_Instance_1_0 */

/* Not generating wrapper interface for PPP_Instance_1_1 */

/* Not generating wrapper interface for PPP_MessageHandler_0_2 */

static const struct PPP_Messaging_1_0 Pnacl_Wrappers_PPP_Messaging_1_0 = {
    .HandleMessage = &Pnacl_M14_PPP_Messaging_HandleMessage
};

/* Not generating wrapper interface for PPP_MouseLock_1_0 */

/* Not generating wrapper interface for PPB_BrowserFont_Trusted_1_0 */

/* Not generating wrapper interface for PPB_CharSet_Trusted_1_0 */

/* Not generating wrapper interface for PPB_FileChooserTrusted_0_5 */

/* Not generating wrapper interface for PPB_FileChooserTrusted_0_6 */

/* Not generating wrapper interface for PPB_URLLoaderTrusted_0_3 */

static const struct PPB_AudioInput_Dev_0_3 Pnacl_Wrappers_PPB_AudioInput_Dev_0_3 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M25_PPB_AudioInput_Dev_Create,
    .IsAudioInput = (PP_Bool (*)(PP_Resource resource))&Pnacl_M25_PPB_AudioInput_Dev_IsAudioInput,
    .EnumerateDevices = (int32_t (*)(PP_Resource audio_input, struct PP_ArrayOutput output, struct PP_CompletionCallback callback))&Pnacl_M25_PPB_AudioInput_Dev_EnumerateDevices,
    .MonitorDeviceChange = (int32_t (*)(PP_Resource audio_input, PP_MonitorDeviceChangeCallback callback, void* user_data))&Pnacl_M25_PPB_AudioInput_Dev_MonitorDeviceChange,
    .Open = (int32_t (*)(PP_Resource audio_input, PP_Resource device_ref, PP_Resource config, PPB_AudioInput_Callback_0_3 audio_input_callback, void* user_data, struct PP_CompletionCallback callback))&Pnacl_M25_PPB_AudioInput_Dev_Open,
    .GetCurrentConfig = (PP_Resource (*)(PP_Resource audio_input))&Pnacl_M25_PPB_AudioInput_Dev_GetCurrentConfig,
    .StartCapture = (PP_Bool (*)(PP_Resource audio_input))&Pnacl_M25_PPB_AudioInput_Dev_StartCapture,
    .StopCapture = (PP_Bool (*)(PP_Resource audio_input))&Pnacl_M25_PPB_AudioInput_Dev_StopCapture,
    .Close = (void (*)(PP_Resource audio_input))&Pnacl_M25_PPB_AudioInput_Dev_Close
};

static const struct PPB_AudioInput_Dev_0_4 Pnacl_Wrappers_PPB_AudioInput_Dev_0_4 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M30_PPB_AudioInput_Dev_Create,
    .IsAudioInput = (PP_Bool (*)(PP_Resource resource))&Pnacl_M30_PPB_AudioInput_Dev_IsAudioInput,
    .EnumerateDevices = (int32_t (*)(PP_Resource audio_input, struct PP_ArrayOutput output, struct PP_CompletionCallback callback))&Pnacl_M30_PPB_AudioInput_Dev_EnumerateDevices,
    .MonitorDeviceChange = (int32_t (*)(PP_Resource audio_input, PP_MonitorDeviceChangeCallback callback, void* user_data))&Pnacl_M30_PPB_AudioInput_Dev_MonitorDeviceChange,
    .Open = (int32_t (*)(PP_Resource audio_input, PP_Resource device_ref, PP_Resource config, PPB_AudioInput_Callback audio_input_callback, void* user_data, struct PP_CompletionCallback callback))&Pnacl_M30_PPB_AudioInput_Dev_Open,
    .GetCurrentConfig = (PP_Resource (*)(PP_Resource audio_input))&Pnacl_M30_PPB_AudioInput_Dev_GetCurrentConfig,
    .StartCapture = (PP_Bool (*)(PP_Resource audio_input))&Pnacl_M30_PPB_AudioInput_Dev_StartCapture,
    .StopCapture = (PP_Bool (*)(PP_Resource audio_input))&Pnacl_M30_PPB_AudioInput_Dev_StopCapture,
    .Close = (void (*)(PP_Resource audio_input))&Pnacl_M30_PPB_AudioInput_Dev_Close
};

static const struct PPB_AudioOutput_Dev_0_1 Pnacl_Wrappers_PPB_AudioOutput_Dev_0_1 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M59_PPB_AudioOutput_Dev_Create,
    .IsAudioOutput = (PP_Bool (*)(PP_Resource resource))&Pnacl_M59_PPB_AudioOutput_Dev_IsAudioOutput,
    .EnumerateDevices = (int32_t (*)(PP_Resource audio_output, struct PP_ArrayOutput output, struct PP_CompletionCallback callback))&Pnacl_M59_PPB_AudioOutput_Dev_EnumerateDevices,
    .MonitorDeviceChange = (int32_t (*)(PP_Resource audio_output, PP_MonitorDeviceChangeCallback callback, void* user_data))&Pnacl_M59_PPB_AudioOutput_Dev_MonitorDeviceChange,
    .Open = (int32_t (*)(PP_Resource audio_output, PP_Resource device_ref, PP_Resource config, PPB_AudioOutput_Callback audio_output_callback, void* user_data, struct PP_CompletionCallback callback))&Pnacl_M59_PPB_AudioOutput_Dev_Open,
    .GetCurrentConfig = (PP_Resource (*)(PP_Resource audio_output))&Pnacl_M59_PPB_AudioOutput_Dev_GetCurrentConfig,
    .StartPlayback = (PP_Bool (*)(PP_Resource audio_output))&Pnacl_M59_PPB_AudioOutput_Dev_StartPlayback,
    .StopPlayback = (PP_Bool (*)(PP_Resource audio_output))&Pnacl_M59_PPB_AudioOutput_Dev_StopPlayback,
    .Close = (void (*)(PP_Resource audio_output))&Pnacl_M59_PPB_AudioOutput_Dev_Close
};

/* Not generating wrapper interface for PPB_Buffer_Dev_0_4 */

/* Not generating wrapper interface for PPB_Crypto_Dev_0_1 */

/* Not generating wrapper interface for PPB_CursorControl_Dev_0_4 */

static const struct PPB_DeviceRef_Dev_0_1 Pnacl_Wrappers_PPB_DeviceRef_Dev_0_1 = {
    .IsDeviceRef = (PP_Bool (*)(PP_Resource resource))&Pnacl_M18_PPB_DeviceRef_Dev_IsDeviceRef,
    .GetType = (PP_DeviceType_Dev (*)(PP_Resource device_ref))&Pnacl_M18_PPB_DeviceRef_Dev_GetType,
    .GetName = (struct PP_Var (*)(PP_Resource device_ref))&Pnacl_M18_PPB_DeviceRef_Dev_GetName
};

static const struct PPB_FileChooser_Dev_0_5 Pnacl_Wrappers_PPB_FileChooser_Dev_0_5 = {
    .Create = (PP_Resource (*)(PP_Instance instance, PP_FileChooserMode_Dev mode, struct PP_Var accept_types))&Pnacl_M16_PPB_FileChooser_Dev_Create,
    .IsFileChooser = (PP_Bool (*)(PP_Resource resource))&Pnacl_M16_PPB_FileChooser_Dev_IsFileChooser,
    .Show = (int32_t (*)(PP_Resource chooser, struct PP_CompletionCallback callback))&Pnacl_M16_PPB_FileChooser_Dev_Show,
    .GetNextChosenFile = (PP_Resource (*)(PP_Resource chooser))&Pnacl_M16_PPB_FileChooser_Dev_GetNextChosenFile
};

static const struct PPB_FileChooser_Dev_0_6 Pnacl_Wrappers_PPB_FileChooser_Dev_0_6 = {
    .Create = (PP_Resource (*)(PP_Instance instance, PP_FileChooserMode_Dev mode, struct PP_Var accept_types))&Pnacl_M19_PPB_FileChooser_Dev_Create,
    .IsFileChooser = (PP_Bool (*)(PP_Resource resource))&Pnacl_M19_PPB_FileChooser_Dev_IsFileChooser,
    .Show = (int32_t (*)(PP_Resource chooser, struct PP_ArrayOutput output, struct PP_CompletionCallback callback))&Pnacl_M19_PPB_FileChooser_Dev_Show
};

static const struct PPB_IMEInputEvent_Dev_0_1 Pnacl_Wrappers_PPB_IMEInputEvent_Dev_0_1 = {
    .IsIMEInputEvent = (PP_Bool (*)(PP_Resource resource))&Pnacl_M16_PPB_IMEInputEvent_Dev_IsIMEInputEvent,
    .GetText = (struct PP_Var (*)(PP_Resource ime_event))&Pnacl_M16_PPB_IMEInputEvent_Dev_GetText,
    .GetSegmentNumber = (uint32_t (*)(PP_Resource ime_event))&Pnacl_M16_PPB_IMEInputEvent_Dev_GetSegmentNumber,
    .GetSegmentOffset = (uint32_t (*)(PP_Resource ime_event, uint32_t index))&Pnacl_M16_PPB_IMEInputEvent_Dev_GetSegmentOffset,
    .GetTargetSegment = (int32_t (*)(PP_Resource ime_event))&Pnacl_M16_PPB_IMEInputEvent_Dev_GetTargetSegment,
    .GetSelection = (void (*)(PP_Resource ime_event, uint32_t* start, uint32_t* end))&Pnacl_M16_PPB_IMEInputEvent_Dev_GetSelection
};

static const struct PPB_IMEInputEvent_Dev_0_2 Pnacl_Wrappers_PPB_IMEInputEvent_Dev_0_2 = {
    .Create = (PP_Resource (*)(PP_Instance instance, PP_InputEvent_Type type, PP_TimeTicks time_stamp, struct PP_Var text, uint32_t segment_number, const uint32_t segment_offsets[], int32_t target_segment, uint32_t selection_start, uint32_t selection_end))&Pnacl_M21_PPB_IMEInputEvent_Dev_Create,
    .IsIMEInputEvent = (PP_Bool (*)(PP_Resource resource))&Pnacl_M21_PPB_IMEInputEvent_Dev_IsIMEInputEvent,
    .GetText = (struct PP_Var (*)(PP_Resource ime_event))&Pnacl_M21_PPB_IMEInputEvent_Dev_GetText,
    .GetSegmentNumber = (uint32_t (*)(PP_Resource ime_event))&Pnacl_M21_PPB_IMEInputEvent_Dev_GetSegmentNumber,
    .GetSegmentOffset = (uint32_t (*)(PP_Resource ime_event, uint32_t index))&Pnacl_M21_PPB_IMEInputEvent_Dev_GetSegmentOffset,
    .GetTargetSegment = (int32_t (*)(PP_Resource ime_event))&Pnacl_M21_PPB_IMEInputEvent_Dev_GetTargetSegment,
    .GetSelection = (void (*)(PP_Resource ime_event, uint32_t* start, uint32_t* end))&Pnacl_M21_PPB_IMEInputEvent_Dev_GetSelection
};

/* Not generating wrapper interface for PPB_Memory_Dev_0_1 */

/* Not generating wrapper interface for PPB_OpenGLES2DrawBuffers_Dev_1_0 */

static const struct PPB_Printing_Dev_0_7 Pnacl_Wrappers_PPB_Printing_Dev_0_7 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M23_PPB_Printing_Dev_Create,
    .GetDefaultPrintSettings = (int32_t (*)(PP_Resource resource, struct PP_PrintSettings_Dev* print_settings, struct PP_CompletionCallback callback))&Pnacl_M23_PPB_Printing_Dev_GetDefaultPrintSettings
};

/* Not generating wrapper interface for PPB_TextInput_Dev_0_1 */

/* Not generating wrapper interface for PPB_TextInput_Dev_0_2 */

/* Not generating wrapper interface for PPB_Trace_Event_Dev_0_1 */

/* Not generating wrapper interface for PPB_Trace_Event_Dev_0_2 */

static const struct PPB_URLUtil_Dev_0_6 Pnacl_Wrappers_PPB_URLUtil_Dev_0_6 = {
    .Canonicalize = (struct PP_Var (*)(struct PP_Var url, struct PP_URLComponents_Dev* components))&Pnacl_M17_PPB_URLUtil_Dev_Canonicalize,
    .ResolveRelativeToURL = (struct PP_Var (*)(struct PP_Var base_url, struct PP_Var relative_string, struct PP_URLComponents_Dev* components))&Pnacl_M17_PPB_URLUtil_Dev_ResolveRelativeToURL,
    .ResolveRelativeToDocument = (struct PP_Var (*)(PP_Instance instance, struct PP_Var relative_string, struct PP_URLComponents_Dev* components))&Pnacl_M17_PPB_URLUtil_Dev_ResolveRelativeToDocument,
    .IsSameSecurityOrigin = (PP_Bool (*)(struct PP_Var url_a, struct PP_Var url_b))&Pnacl_M17_PPB_URLUtil_Dev_IsSameSecurityOrigin,
    .DocumentCanRequest = (PP_Bool (*)(PP_Instance instance, struct PP_Var url))&Pnacl_M17_PPB_URLUtil_Dev_DocumentCanRequest,
    .DocumentCanAccessDocument = (PP_Bool (*)(PP_Instance active, PP_Instance target))&Pnacl_M17_PPB_URLUtil_Dev_DocumentCanAccessDocument,
    .GetDocumentURL = (struct PP_Var (*)(PP_Instance instance, struct PP_URLComponents_Dev* components))&Pnacl_M17_PPB_URLUtil_Dev_GetDocumentURL,
    .GetPluginInstanceURL = (struct PP_Var (*)(PP_Instance instance, struct PP_URLComponents_Dev* components))&Pnacl_M17_PPB_URLUtil_Dev_GetPluginInstanceURL
};

static const struct PPB_URLUtil_Dev_0_7 Pnacl_Wrappers_PPB_URLUtil_Dev_0_7 = {
    .Canonicalize = (struct PP_Var (*)(struct PP_Var url, struct PP_URLComponents_Dev* components))&Pnacl_M31_PPB_URLUtil_Dev_Canonicalize,
    .ResolveRelativeToURL = (struct PP_Var (*)(struct PP_Var base_url, struct PP_Var relative_string, struct PP_URLComponents_Dev* components))&Pnacl_M31_PPB_URLUtil_Dev_ResolveRelativeToURL,
    .ResolveRelativeToDocument = (struct PP_Var (*)(PP_Instance instance, struct PP_Var relative_string, struct PP_URLComponents_Dev* components))&Pnacl_M31_PPB_URLUtil_Dev_ResolveRelativeToDocument,
    .IsSameSecurityOrigin = (PP_Bool (*)(struct PP_Var url_a, struct PP_Var url_b))&Pnacl_M31_PPB_URLUtil_Dev_IsSameSecurityOrigin,
    .DocumentCanRequest = (PP_Bool (*)(PP_Instance instance, struct PP_Var url))&Pnacl_M31_PPB_URLUtil_Dev_DocumentCanRequest,
    .DocumentCanAccessDocument = (PP_Bool (*)(PP_Instance active, PP_Instance target))&Pnacl_M31_PPB_URLUtil_Dev_DocumentCanAccessDocument,
    .GetDocumentURL = (struct PP_Var (*)(PP_Instance instance, struct PP_URLComponents_Dev* components))&Pnacl_M31_PPB_URLUtil_Dev_GetDocumentURL,
    .GetPluginInstanceURL = (struct PP_Var (*)(PP_Instance instance, struct PP_URLComponents_Dev* components))&Pnacl_M31_PPB_URLUtil_Dev_GetPluginInstanceURL,
    .GetPluginReferrerURL = (struct PP_Var (*)(PP_Instance instance, struct PP_URLComponents_Dev* components))&Pnacl_M31_PPB_URLUtil_Dev_GetPluginReferrerURL
};

static const struct PPB_VideoCapture_Dev_0_3 Pnacl_Wrappers_PPB_VideoCapture_Dev_0_3 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M25_PPB_VideoCapture_Dev_Create,
    .IsVideoCapture = (PP_Bool (*)(PP_Resource video_capture))&Pnacl_M25_PPB_VideoCapture_Dev_IsVideoCapture,
    .EnumerateDevices = (int32_t (*)(PP_Resource video_capture, struct PP_ArrayOutput output, struct PP_CompletionCallback callback))&Pnacl_M25_PPB_VideoCapture_Dev_EnumerateDevices,
    .MonitorDeviceChange = (int32_t (*)(PP_Resource video_capture, PP_MonitorDeviceChangeCallback callback, void* user_data))&Pnacl_M25_PPB_VideoCapture_Dev_MonitorDeviceChange,
    .Open = (int32_t (*)(PP_Resource video_capture, PP_Resource device_ref, const struct PP_VideoCaptureDeviceInfo_Dev* requested_info, uint32_t buffer_count, struct PP_CompletionCallback callback))&Pnacl_M25_PPB_VideoCapture_Dev_Open,
    .StartCapture = (int32_t (*)(PP_Resource video_capture))&Pnacl_M25_PPB_VideoCapture_Dev_StartCapture,
    .ReuseBuffer = (int32_t (*)(PP_Resource video_capture, uint32_t buffer))&Pnacl_M25_PPB_VideoCapture_Dev_ReuseBuffer,
    .StopCapture = (int32_t (*)(PP_Resource video_capture))&Pnacl_M25_PPB_VideoCapture_Dev_StopCapture,
    .Close = (void (*)(PP_Resource video_capture))&Pnacl_M25_PPB_VideoCapture_Dev_Close
};

static const struct PPB_VideoDecoder_Dev_0_16 Pnacl_Wrappers_PPB_VideoDecoder_Dev_0_16 = {
    .Create = (PP_Resource (*)(PP_Instance instance, PP_Resource context, PP_VideoDecoder_Profile profile))&Pnacl_M14_PPB_VideoDecoder_Dev_Create,
    .IsVideoDecoder = (PP_Bool (*)(PP_Resource resource))&Pnacl_M14_PPB_VideoDecoder_Dev_IsVideoDecoder,
    .Decode = (int32_t (*)(PP_Resource video_decoder, const struct PP_VideoBitstreamBuffer_Dev* bitstream_buffer, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_VideoDecoder_Dev_Decode,
    .AssignPictureBuffers = (void (*)(PP_Resource video_decoder, uint32_t no_of_buffers, const struct PP_PictureBuffer_Dev buffers[]))&Pnacl_M14_PPB_VideoDecoder_Dev_AssignPictureBuffers,
    .ReusePictureBuffer = (void (*)(PP_Resource video_decoder, int32_t picture_buffer_id))&Pnacl_M14_PPB_VideoDecoder_Dev_ReusePictureBuffer,
    .Flush = (int32_t (*)(PP_Resource video_decoder, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_VideoDecoder_Dev_Flush,
    .Reset = (int32_t (*)(PP_Resource video_decoder, struct PP_CompletionCallback callback))&Pnacl_M14_PPB_VideoDecoder_Dev_Reset,
    .Destroy = (void (*)(PP_Resource video_decoder))&Pnacl_M14_PPB_VideoDecoder_Dev_Destroy
};

/* Not generating wrapper interface for PPB_View_Dev_0_1 */

/* Not generating wrapper interface for PPP_NetworkState_Dev_0_1 */

/* Not generating wrapper interface for PPP_Printing_Dev_0_6 */

/* Not generating wrapper interface for PPP_TextInput_Dev_0_1 */

/* Not generating wrapper interface for PPP_VideoCapture_Dev_0_1 */

/* Not generating wrapper interface for PPP_VideoDecoder_Dev_0_11 */

/* Not generating wrapper interface for PPB_CameraCapabilities_Private_0_1 */

static const struct PPB_CameraDevice_Private_0_1 Pnacl_Wrappers_PPB_CameraDevice_Private_0_1 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M42_PPB_CameraDevice_Private_Create,
    .IsCameraDevice = (PP_Bool (*)(PP_Resource resource))&Pnacl_M42_PPB_CameraDevice_Private_IsCameraDevice,
    .Open = (int32_t (*)(PP_Resource camera_device, struct PP_Var device_id, struct PP_CompletionCallback callback))&Pnacl_M42_PPB_CameraDevice_Private_Open,
    .Close = (void (*)(PP_Resource camera_device))&Pnacl_M42_PPB_CameraDevice_Private_Close,
    .GetCameraCapabilities = (int32_t (*)(PP_Resource camera_device, PP_Resource* capabilities, struct PP_CompletionCallback callback))&Pnacl_M42_PPB_CameraDevice_Private_GetCameraCapabilities
};

static const struct PPB_DisplayColorProfile_Private_0_1 Pnacl_Wrappers_PPB_DisplayColorProfile_Private_0_1 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M33_PPB_DisplayColorProfile_Private_Create,
    .IsDisplayColorProfile = (PP_Bool (*)(PP_Resource resource))&Pnacl_M33_PPB_DisplayColorProfile_Private_IsDisplayColorProfile,
    .GetColorProfile = (int32_t (*)(PP_Resource display_color_profile_res, struct PP_ArrayOutput color_profile, struct PP_CompletionCallback callback))&Pnacl_M33_PPB_DisplayColorProfile_Private_GetColorProfile,
    .RegisterColorProfileChangeCallback = (int32_t (*)(PP_Resource display_color_profile_res, struct PP_CompletionCallback callback))&Pnacl_M33_PPB_DisplayColorProfile_Private_RegisterColorProfileChangeCallback
};

static const struct PPB_Ext_CrxFileSystem_Private_0_1 Pnacl_Wrappers_PPB_Ext_CrxFileSystem_Private_0_1 = {
    .Open = (int32_t (*)(PP_Instance instance, PP_Resource* file_system, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_Ext_CrxFileSystem_Private_Open
};

static const struct PPB_FileIO_Private_0_1 Pnacl_Wrappers_PPB_FileIO_Private_0_1 = {
    .RequestOSFileHandle = (int32_t (*)(PP_Resource file_io, PP_FileHandle* handle, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_FileIO_Private_RequestOSFileHandle
};

static const struct PPB_FileRefPrivate_0_1 Pnacl_Wrappers_PPB_FileRefPrivate_0_1 = {
    .GetAbsolutePath = (struct PP_Var (*)(PP_Resource file_ref))&Pnacl_M15_PPB_FileRefPrivate_GetAbsolutePath
};

/* Not generating wrapper interface for PPB_Find_Private_0_3 */

/* Not generating wrapper interface for PPB_Flash_FontFile_0_1 */

/* Not generating wrapper interface for PPB_Flash_FontFile_0_2 */

static const struct PPB_HostResolver_Private_0_1 Pnacl_Wrappers_PPB_HostResolver_Private_0_1 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M19_PPB_HostResolver_Private_Create,
    .IsHostResolver = (PP_Bool (*)(PP_Resource resource))&Pnacl_M19_PPB_HostResolver_Private_IsHostResolver,
    .Resolve = (int32_t (*)(PP_Resource host_resolver, const char* host, uint16_t port, const struct PP_HostResolver_Private_Hint* hint, struct PP_CompletionCallback callback))&Pnacl_M19_PPB_HostResolver_Private_Resolve,
    .GetCanonicalName = (struct PP_Var (*)(PP_Resource host_resolver))&Pnacl_M19_PPB_HostResolver_Private_GetCanonicalName,
    .GetSize = (uint32_t (*)(PP_Resource host_resolver))&Pnacl_M19_PPB_HostResolver_Private_GetSize,
    .GetNetAddress = (PP_Bool (*)(PP_Resource host_resolver, uint32_t index, struct PP_NetAddress_Private* addr))&Pnacl_M19_PPB_HostResolver_Private_GetNetAddress
};

static const struct PPB_Instance_Private_0_1 Pnacl_Wrappers_PPB_Instance_Private_0_1 = {
    .GetWindowObject = (struct PP_Var (*)(PP_Instance instance))&Pnacl_M13_PPB_Instance_Private_GetWindowObject,
    .GetOwnerElementObject = (struct PP_Var (*)(PP_Instance instance))&Pnacl_M13_PPB_Instance_Private_GetOwnerElementObject,
    .ExecuteScript = (struct PP_Var (*)(PP_Instance instance, struct PP_Var script, struct PP_Var* exception))&Pnacl_M13_PPB_Instance_Private_ExecuteScript
};

static const struct PPB_IsolatedFileSystem_Private_0_2 Pnacl_Wrappers_PPB_IsolatedFileSystem_Private_0_2 = {
    .Open = (int32_t (*)(PP_Instance instance, PP_IsolatedFileSystemType_Private type, PP_Resource* file_system, struct PP_CompletionCallback callback))&Pnacl_M33_PPB_IsolatedFileSystem_Private_Open
};

static const struct PPB_NetAddress_Private_0_1 Pnacl_Wrappers_PPB_NetAddress_Private_0_1 = {
    .AreEqual = (PP_Bool (*)(const struct PP_NetAddress_Private* addr1, const struct PP_NetAddress_Private* addr2))&Pnacl_M17_PPB_NetAddress_Private_AreEqual,
    .AreHostsEqual = (PP_Bool (*)(const struct PP_NetAddress_Private* addr1, const struct PP_NetAddress_Private* addr2))&Pnacl_M17_PPB_NetAddress_Private_AreHostsEqual,
    .Describe = (struct PP_Var (*)(PP_Module module, const struct PP_NetAddress_Private* addr, PP_Bool include_port))&Pnacl_M17_PPB_NetAddress_Private_Describe,
    .ReplacePort = (PP_Bool (*)(const struct PP_NetAddress_Private* src_addr, uint16_t port, struct PP_NetAddress_Private* addr_out))&Pnacl_M17_PPB_NetAddress_Private_ReplacePort,
    .GetAnyAddress = (void (*)(PP_Bool is_ipv6, struct PP_NetAddress_Private* addr))&Pnacl_M17_PPB_NetAddress_Private_GetAnyAddress
};

static const struct PPB_NetAddress_Private_1_0 Pnacl_Wrappers_PPB_NetAddress_Private_1_0 = {
    .AreEqual = (PP_Bool (*)(const struct PP_NetAddress_Private* addr1, const struct PP_NetAddress_Private* addr2))&Pnacl_M19_0_PPB_NetAddress_Private_AreEqual,
    .AreHostsEqual = (PP_Bool (*)(const struct PP_NetAddress_Private* addr1, const struct PP_NetAddress_Private* addr2))&Pnacl_M19_0_PPB_NetAddress_Private_AreHostsEqual,
    .Describe = (struct PP_Var (*)(PP_Module module, const struct PP_NetAddress_Private* addr, PP_Bool include_port))&Pnacl_M19_0_PPB_NetAddress_Private_Describe,
    .ReplacePort = (PP_Bool (*)(const struct PP_NetAddress_Private* src_addr, uint16_t port, struct PP_NetAddress_Private* addr_out))&Pnacl_M19_0_PPB_NetAddress_Private_ReplacePort,
    .GetAnyAddress = (void (*)(PP_Bool is_ipv6, struct PP_NetAddress_Private* addr))&Pnacl_M19_0_PPB_NetAddress_Private_GetAnyAddress,
    .GetFamily = (PP_NetAddressFamily_Private (*)(const struct PP_NetAddress_Private* addr))&Pnacl_M19_0_PPB_NetAddress_Private_GetFamily,
    .GetPort = (uint16_t (*)(const struct PP_NetAddress_Private* addr))&Pnacl_M19_0_PPB_NetAddress_Private_GetPort,
    .GetAddress = (PP_Bool (*)(const struct PP_NetAddress_Private* addr, void* address, uint16_t address_size))&Pnacl_M19_0_PPB_NetAddress_Private_GetAddress
};

static const struct PPB_NetAddress_Private_1_1 Pnacl_Wrappers_PPB_NetAddress_Private_1_1 = {
    .AreEqual = (PP_Bool (*)(const struct PP_NetAddress_Private* addr1, const struct PP_NetAddress_Private* addr2))&Pnacl_M19_1_PPB_NetAddress_Private_AreEqual,
    .AreHostsEqual = (PP_Bool (*)(const struct PP_NetAddress_Private* addr1, const struct PP_NetAddress_Private* addr2))&Pnacl_M19_1_PPB_NetAddress_Private_AreHostsEqual,
    .Describe = (struct PP_Var (*)(PP_Module module, const struct PP_NetAddress_Private* addr, PP_Bool include_port))&Pnacl_M19_1_PPB_NetAddress_Private_Describe,
    .ReplacePort = (PP_Bool (*)(const struct PP_NetAddress_Private* src_addr, uint16_t port, struct PP_NetAddress_Private* addr_out))&Pnacl_M19_1_PPB_NetAddress_Private_ReplacePort,
    .GetAnyAddress = (void (*)(PP_Bool is_ipv6, struct PP_NetAddress_Private* addr))&Pnacl_M19_1_PPB_NetAddress_Private_GetAnyAddress,
    .GetFamily = (PP_NetAddressFamily_Private (*)(const struct PP_NetAddress_Private* addr))&Pnacl_M19_1_PPB_NetAddress_Private_GetFamily,
    .GetPort = (uint16_t (*)(const struct PP_NetAddress_Private* addr))&Pnacl_M19_1_PPB_NetAddress_Private_GetPort,
    .GetAddress = (PP_Bool (*)(const struct PP_NetAddress_Private* addr, void* address, uint16_t address_size))&Pnacl_M19_1_PPB_NetAddress_Private_GetAddress,
    .GetScopeID = (uint32_t (*)(const struct PP_NetAddress_Private* addr))&Pnacl_M19_1_PPB_NetAddress_Private_GetScopeID,
    .CreateFromIPv4Address = (void (*)(const uint8_t ip[4], uint16_t port, struct PP_NetAddress_Private* addr_out))&Pnacl_M19_1_PPB_NetAddress_Private_CreateFromIPv4Address,
    .CreateFromIPv6Address = (void (*)(const uint8_t ip[16], uint32_t scope_id, uint16_t port, struct PP_NetAddress_Private* addr_out))&Pnacl_M19_1_PPB_NetAddress_Private_CreateFromIPv6Address
};

static const struct PPB_TCPServerSocket_Private_0_1 Pnacl_Wrappers_PPB_TCPServerSocket_Private_0_1 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M18_PPB_TCPServerSocket_Private_Create,
    .IsTCPServerSocket = (PP_Bool (*)(PP_Resource resource))&Pnacl_M18_PPB_TCPServerSocket_Private_IsTCPServerSocket,
    .Listen = (int32_t (*)(PP_Resource tcp_server_socket, const struct PP_NetAddress_Private* addr, int32_t backlog, struct PP_CompletionCallback callback))&Pnacl_M18_PPB_TCPServerSocket_Private_Listen,
    .Accept = (int32_t (*)(PP_Resource tcp_server_socket, PP_Resource* tcp_socket, struct PP_CompletionCallback callback))&Pnacl_M18_PPB_TCPServerSocket_Private_Accept,
    .StopListening = (void (*)(PP_Resource tcp_server_socket))&Pnacl_M18_PPB_TCPServerSocket_Private_StopListening
};

static const struct PPB_TCPServerSocket_Private_0_2 Pnacl_Wrappers_PPB_TCPServerSocket_Private_0_2 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M28_PPB_TCPServerSocket_Private_Create,
    .IsTCPServerSocket = (PP_Bool (*)(PP_Resource resource))&Pnacl_M28_PPB_TCPServerSocket_Private_IsTCPServerSocket,
    .Listen = (int32_t (*)(PP_Resource tcp_server_socket, const struct PP_NetAddress_Private* addr, int32_t backlog, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_TCPServerSocket_Private_Listen,
    .Accept = (int32_t (*)(PP_Resource tcp_server_socket, PP_Resource* tcp_socket, struct PP_CompletionCallback callback))&Pnacl_M28_PPB_TCPServerSocket_Private_Accept,
    .GetLocalAddress = (int32_t (*)(PP_Resource tcp_server_socket, struct PP_NetAddress_Private* addr))&Pnacl_M28_PPB_TCPServerSocket_Private_GetLocalAddress,
    .StopListening = (void (*)(PP_Resource tcp_server_socket))&Pnacl_M28_PPB_TCPServerSocket_Private_StopListening
};

static const struct PPB_TCPSocket_Private_0_3 Pnacl_Wrappers_PPB_TCPSocket_Private_0_3 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M17_PPB_TCPSocket_Private_Create,
    .IsTCPSocket = (PP_Bool (*)(PP_Resource resource))&Pnacl_M17_PPB_TCPSocket_Private_IsTCPSocket,
    .Connect = (int32_t (*)(PP_Resource tcp_socket, const char* host, uint16_t port, struct PP_CompletionCallback callback))&Pnacl_M17_PPB_TCPSocket_Private_Connect,
    .ConnectWithNetAddress = (int32_t (*)(PP_Resource tcp_socket, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback callback))&Pnacl_M17_PPB_TCPSocket_Private_ConnectWithNetAddress,
    .GetLocalAddress = (PP_Bool (*)(PP_Resource tcp_socket, struct PP_NetAddress_Private* local_addr))&Pnacl_M17_PPB_TCPSocket_Private_GetLocalAddress,
    .GetRemoteAddress = (PP_Bool (*)(PP_Resource tcp_socket, struct PP_NetAddress_Private* remote_addr))&Pnacl_M17_PPB_TCPSocket_Private_GetRemoteAddress,
    .SSLHandshake = (int32_t (*)(PP_Resource tcp_socket, const char* server_name, uint16_t server_port, struct PP_CompletionCallback callback))&Pnacl_M17_PPB_TCPSocket_Private_SSLHandshake,
    .Read = (int32_t (*)(PP_Resource tcp_socket, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback callback))&Pnacl_M17_PPB_TCPSocket_Private_Read,
    .Write = (int32_t (*)(PP_Resource tcp_socket, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback callback))&Pnacl_M17_PPB_TCPSocket_Private_Write,
    .Disconnect = (void (*)(PP_Resource tcp_socket))&Pnacl_M17_PPB_TCPSocket_Private_Disconnect
};

static const struct PPB_TCPSocket_Private_0_4 Pnacl_Wrappers_PPB_TCPSocket_Private_0_4 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M20_PPB_TCPSocket_Private_Create,
    .IsTCPSocket = (PP_Bool (*)(PP_Resource resource))&Pnacl_M20_PPB_TCPSocket_Private_IsTCPSocket,
    .Connect = (int32_t (*)(PP_Resource tcp_socket, const char* host, uint16_t port, struct PP_CompletionCallback callback))&Pnacl_M20_PPB_TCPSocket_Private_Connect,
    .ConnectWithNetAddress = (int32_t (*)(PP_Resource tcp_socket, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback callback))&Pnacl_M20_PPB_TCPSocket_Private_ConnectWithNetAddress,
    .GetLocalAddress = (PP_Bool (*)(PP_Resource tcp_socket, struct PP_NetAddress_Private* local_addr))&Pnacl_M20_PPB_TCPSocket_Private_GetLocalAddress,
    .GetRemoteAddress = (PP_Bool (*)(PP_Resource tcp_socket, struct PP_NetAddress_Private* remote_addr))&Pnacl_M20_PPB_TCPSocket_Private_GetRemoteAddress,
    .SSLHandshake = (int32_t (*)(PP_Resource tcp_socket, const char* server_name, uint16_t server_port, struct PP_CompletionCallback callback))&Pnacl_M20_PPB_TCPSocket_Private_SSLHandshake,
    .GetServerCertificate = (PP_Resource (*)(PP_Resource tcp_socket))&Pnacl_M20_PPB_TCPSocket_Private_GetServerCertificate,
    .AddChainBuildingCertificate = (PP_Bool (*)(PP_Resource tcp_socket, PP_Resource certificate, PP_Bool is_trusted))&Pnacl_M20_PPB_TCPSocket_Private_AddChainBuildingCertificate,
    .Read = (int32_t (*)(PP_Resource tcp_socket, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback callback))&Pnacl_M20_PPB_TCPSocket_Private_Read,
    .Write = (int32_t (*)(PP_Resource tcp_socket, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback callback))&Pnacl_M20_PPB_TCPSocket_Private_Write,
    .Disconnect = (void (*)(PP_Resource tcp_socket))&Pnacl_M20_PPB_TCPSocket_Private_Disconnect
};

static const struct PPB_TCPSocket_Private_0_5 Pnacl_Wrappers_PPB_TCPSocket_Private_0_5 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M27_PPB_TCPSocket_Private_Create,
    .IsTCPSocket = (PP_Bool (*)(PP_Resource resource))&Pnacl_M27_PPB_TCPSocket_Private_IsTCPSocket,
    .Connect = (int32_t (*)(PP_Resource tcp_socket, const char* host, uint16_t port, struct PP_CompletionCallback callback))&Pnacl_M27_PPB_TCPSocket_Private_Connect,
    .ConnectWithNetAddress = (int32_t (*)(PP_Resource tcp_socket, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback callback))&Pnacl_M27_PPB_TCPSocket_Private_ConnectWithNetAddress,
    .GetLocalAddress = (PP_Bool (*)(PP_Resource tcp_socket, struct PP_NetAddress_Private* local_addr))&Pnacl_M27_PPB_TCPSocket_Private_GetLocalAddress,
    .GetRemoteAddress = (PP_Bool (*)(PP_Resource tcp_socket, struct PP_NetAddress_Private* remote_addr))&Pnacl_M27_PPB_TCPSocket_Private_GetRemoteAddress,
    .SSLHandshake = (int32_t (*)(PP_Resource tcp_socket, const char* server_name, uint16_t server_port, struct PP_CompletionCallback callback))&Pnacl_M27_PPB_TCPSocket_Private_SSLHandshake,
    .GetServerCertificate = (PP_Resource (*)(PP_Resource tcp_socket))&Pnacl_M27_PPB_TCPSocket_Private_GetServerCertificate,
    .AddChainBuildingCertificate = (PP_Bool (*)(PP_Resource tcp_socket, PP_Resource certificate, PP_Bool is_trusted))&Pnacl_M27_PPB_TCPSocket_Private_AddChainBuildingCertificate,
    .Read = (int32_t (*)(PP_Resource tcp_socket, char* buffer, int32_t bytes_to_read, struct PP_CompletionCallback callback))&Pnacl_M27_PPB_TCPSocket_Private_Read,
    .Write = (int32_t (*)(PP_Resource tcp_socket, const char* buffer, int32_t bytes_to_write, struct PP_CompletionCallback callback))&Pnacl_M27_PPB_TCPSocket_Private_Write,
    .Disconnect = (void (*)(PP_Resource tcp_socket))&Pnacl_M27_PPB_TCPSocket_Private_Disconnect,
    .SetOption = (int32_t (*)(PP_Resource tcp_socket, PP_TCPSocketOption_Private name, struct PP_Var value, struct PP_CompletionCallback callback))&Pnacl_M27_PPB_TCPSocket_Private_SetOption
};

static const struct PPB_Testing_Private_1_0 Pnacl_Wrappers_PPB_Testing_Private_1_0 = {
    .ReadImageData = (PP_Bool (*)(PP_Resource device_context_2d, PP_Resource image, const struct PP_Point* top_left))&Pnacl_M33_PPB_Testing_Private_ReadImageData,
    .RunMessageLoop = (void (*)(PP_Instance instance))&Pnacl_M33_PPB_Testing_Private_RunMessageLoop,
    .QuitMessageLoop = (void (*)(PP_Instance instance))&Pnacl_M33_PPB_Testing_Private_QuitMessageLoop,
    .GetLiveObjectsForInstance = (uint32_t (*)(PP_Instance instance))&Pnacl_M33_PPB_Testing_Private_GetLiveObjectsForInstance,
    .IsOutOfProcess = (PP_Bool (*)(void))&Pnacl_M33_PPB_Testing_Private_IsOutOfProcess,
    .SimulateInputEvent = (void (*)(PP_Instance instance, PP_Resource input_event))&Pnacl_M33_PPB_Testing_Private_SimulateInputEvent,
    .GetDocumentURL = (struct PP_Var (*)(PP_Instance instance, struct PP_URLComponents_Dev* components))&Pnacl_M33_PPB_Testing_Private_GetDocumentURL,
    .GetLiveVars = (uint32_t (*)(struct PP_Var live_vars[], uint32_t array_size))&Pnacl_M33_PPB_Testing_Private_GetLiveVars,
    .SetMinimumArrayBufferSizeForShmem = (void (*)(PP_Instance instance, uint32_t threshold))&Pnacl_M33_PPB_Testing_Private_SetMinimumArrayBufferSizeForShmem,
    .RunV8GC = (void (*)(PP_Instance instance))&Pnacl_M33_PPB_Testing_Private_RunV8GC
};

static const struct PPB_UDPSocket_Private_0_2 Pnacl_Wrappers_PPB_UDPSocket_Private_0_2 = {
    .Create = (PP_Resource (*)(PP_Instance instance_id))&Pnacl_M17_PPB_UDPSocket_Private_Create,
    .IsUDPSocket = (PP_Bool (*)(PP_Resource resource_id))&Pnacl_M17_PPB_UDPSocket_Private_IsUDPSocket,
    .Bind = (int32_t (*)(PP_Resource udp_socket, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback callback))&Pnacl_M17_PPB_UDPSocket_Private_Bind,
    .RecvFrom = (int32_t (*)(PP_Resource udp_socket, char* buffer, int32_t num_bytes, struct PP_CompletionCallback callback))&Pnacl_M17_PPB_UDPSocket_Private_RecvFrom,
    .GetRecvFromAddress = (PP_Bool (*)(PP_Resource udp_socket, struct PP_NetAddress_Private* addr))&Pnacl_M17_PPB_UDPSocket_Private_GetRecvFromAddress,
    .SendTo = (int32_t (*)(PP_Resource udp_socket, const char* buffer, int32_t num_bytes, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback callback))&Pnacl_M17_PPB_UDPSocket_Private_SendTo,
    .Close = (void (*)(PP_Resource udp_socket))&Pnacl_M17_PPB_UDPSocket_Private_Close
};

static const struct PPB_UDPSocket_Private_0_3 Pnacl_Wrappers_PPB_UDPSocket_Private_0_3 = {
    .Create = (PP_Resource (*)(PP_Instance instance_id))&Pnacl_M19_PPB_UDPSocket_Private_Create,
    .IsUDPSocket = (PP_Bool (*)(PP_Resource resource_id))&Pnacl_M19_PPB_UDPSocket_Private_IsUDPSocket,
    .Bind = (int32_t (*)(PP_Resource udp_socket, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback callback))&Pnacl_M19_PPB_UDPSocket_Private_Bind,
    .GetBoundAddress = (PP_Bool (*)(PP_Resource udp_socket, struct PP_NetAddress_Private* addr))&Pnacl_M19_PPB_UDPSocket_Private_GetBoundAddress,
    .RecvFrom = (int32_t (*)(PP_Resource udp_socket, char* buffer, int32_t num_bytes, struct PP_CompletionCallback callback))&Pnacl_M19_PPB_UDPSocket_Private_RecvFrom,
    .GetRecvFromAddress = (PP_Bool (*)(PP_Resource udp_socket, struct PP_NetAddress_Private* addr))&Pnacl_M19_PPB_UDPSocket_Private_GetRecvFromAddress,
    .SendTo = (int32_t (*)(PP_Resource udp_socket, const char* buffer, int32_t num_bytes, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback callback))&Pnacl_M19_PPB_UDPSocket_Private_SendTo,
    .Close = (void (*)(PP_Resource udp_socket))&Pnacl_M19_PPB_UDPSocket_Private_Close
};

static const struct PPB_UDPSocket_Private_0_4 Pnacl_Wrappers_PPB_UDPSocket_Private_0_4 = {
    .Create = (PP_Resource (*)(PP_Instance instance_id))&Pnacl_M23_PPB_UDPSocket_Private_Create,
    .IsUDPSocket = (PP_Bool (*)(PP_Resource resource_id))&Pnacl_M23_PPB_UDPSocket_Private_IsUDPSocket,
    .SetSocketFeature = (int32_t (*)(PP_Resource udp_socket, PP_UDPSocketFeature_Private name, struct PP_Var value))&Pnacl_M23_PPB_UDPSocket_Private_SetSocketFeature,
    .Bind = (int32_t (*)(PP_Resource udp_socket, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback callback))&Pnacl_M23_PPB_UDPSocket_Private_Bind,
    .GetBoundAddress = (PP_Bool (*)(PP_Resource udp_socket, struct PP_NetAddress_Private* addr))&Pnacl_M23_PPB_UDPSocket_Private_GetBoundAddress,
    .RecvFrom = (int32_t (*)(PP_Resource udp_socket, char* buffer, int32_t num_bytes, struct PP_CompletionCallback callback))&Pnacl_M23_PPB_UDPSocket_Private_RecvFrom,
    .GetRecvFromAddress = (PP_Bool (*)(PP_Resource udp_socket, struct PP_NetAddress_Private* addr))&Pnacl_M23_PPB_UDPSocket_Private_GetRecvFromAddress,
    .SendTo = (int32_t (*)(PP_Resource udp_socket, const char* buffer, int32_t num_bytes, const struct PP_NetAddress_Private* addr, struct PP_CompletionCallback callback))&Pnacl_M23_PPB_UDPSocket_Private_SendTo,
    .Close = (void (*)(PP_Resource udp_socket))&Pnacl_M23_PPB_UDPSocket_Private_Close
};

static const struct PPB_UMA_Private_0_3 Pnacl_Wrappers_PPB_UMA_Private_0_3 = {
    .HistogramCustomTimes = (void (*)(PP_Instance instance, struct PP_Var name, int64_t sample, int64_t min, int64_t max, uint32_t bucket_count))&Pnacl_M35_PPB_UMA_Private_HistogramCustomTimes,
    .HistogramCustomCounts = (void (*)(PP_Instance instance, struct PP_Var name, int32_t sample, int32_t min, int32_t max, uint32_t bucket_count))&Pnacl_M35_PPB_UMA_Private_HistogramCustomCounts,
    .HistogramEnumeration = (void (*)(PP_Instance instance, struct PP_Var name, int32_t sample, int32_t boundary_value))&Pnacl_M35_PPB_UMA_Private_HistogramEnumeration,
    .IsCrashReportingEnabled = (int32_t (*)(PP_Instance instance, struct PP_CompletionCallback callback))&Pnacl_M35_PPB_UMA_Private_IsCrashReportingEnabled
};

static const struct PPB_X509Certificate_Private_0_1 Pnacl_Wrappers_PPB_X509Certificate_Private_0_1 = {
    .Create = (PP_Resource (*)(PP_Instance instance))&Pnacl_M19_PPB_X509Certificate_Private_Create,
    .IsX509CertificatePrivate = (PP_Bool (*)(PP_Resource resource))&Pnacl_M19_PPB_X509Certificate_Private_IsX509CertificatePrivate,
    .Initialize = (PP_Bool (*)(PP_Resource resource, const char* bytes, uint32_t length))&Pnacl_M19_PPB_X509Certificate_Private_Initialize,
    .GetField = (struct PP_Var (*)(PP_Resource resource, PP_X509Certificate_Private_Field field))&Pnacl_M19_PPB_X509Certificate_Private_GetField
};

/* Not generating wrapper interface for PPP_Find_Private_0_3 */

static const struct PPP_Instance_Private_0_1 Pnacl_Wrappers_PPP_Instance_Private_0_1 = {
    .GetInstanceObject = &Pnacl_M18_PPP_Instance_Private_GetInstanceObject
};

/* Not generating wrapper interface for PPP_PexeStreamHandler_1_0 */

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Console_1_0 = {
  .iface_macro = PPB_CONSOLE_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_Console_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Core_1_0 = {
  .iface_macro = PPB_CORE_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_Core_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileIO_1_0 = {
  .iface_macro = PPB_FILEIO_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_FileIO_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileIO_1_1 = {
  .iface_macro = PPB_FILEIO_INTERFACE_1_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_FileIO_1_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileRef_1_0 = {
  .iface_macro = PPB_FILEREF_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_FileRef_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileRef_1_1 = {
  .iface_macro = PPB_FILEREF_INTERFACE_1_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_FileRef_1_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileRef_1_2 = {
  .iface_macro = PPB_FILEREF_INTERFACE_1_2,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_FileRef_1_2,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileSystem_1_0 = {
  .iface_macro = PPB_FILESYSTEM_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_FileSystem_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Graphics2D_1_0 = {
  .iface_macro = PPB_GRAPHICS_2D_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_Graphics2D_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Graphics2D_1_1 = {
  .iface_macro = PPB_GRAPHICS_2D_INTERFACE_1_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_Graphics2D_1_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Graphics2D_1_2 = {
  .iface_macro = PPB_GRAPHICS_2D_INTERFACE_1_2,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_Graphics2D_1_2,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Graphics3D_1_0 = {
  .iface_macro = PPB_GRAPHICS_3D_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_Graphics3D_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_HostResolver_1_0 = {
  .iface_macro = PPB_HOSTRESOLVER_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_HostResolver_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_MouseInputEvent_1_0 = {
  .iface_macro = PPB_MOUSE_INPUT_EVENT_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_MouseInputEvent_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_MouseInputEvent_1_1 = {
  .iface_macro = PPB_MOUSE_INPUT_EVENT_INTERFACE_1_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_MouseInputEvent_1_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_WheelInputEvent_1_0 = {
  .iface_macro = PPB_WHEEL_INPUT_EVENT_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_WheelInputEvent_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_KeyboardInputEvent_1_0 = {
  .iface_macro = PPB_KEYBOARD_INPUT_EVENT_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_KeyboardInputEvent_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_KeyboardInputEvent_1_2 = {
  .iface_macro = PPB_KEYBOARD_INPUT_EVENT_INTERFACE_1_2,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_KeyboardInputEvent_1_2,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TouchInputEvent_1_0 = {
  .iface_macro = PPB_TOUCH_INPUT_EVENT_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_TouchInputEvent_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TouchInputEvent_1_4 = {
  .iface_macro = PPB_TOUCH_INPUT_EVENT_INTERFACE_1_4,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_TouchInputEvent_1_4,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_IMEInputEvent_1_0 = {
  .iface_macro = PPB_IME_INPUT_EVENT_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_IMEInputEvent_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_MediaStreamAudioTrack_0_1 = {
  .iface_macro = PPB_MEDIASTREAMAUDIOTRACK_INTERFACE_0_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_MediaStreamAudioTrack_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_MediaStreamVideoTrack_0_1 = {
  .iface_macro = PPB_MEDIASTREAMVIDEOTRACK_INTERFACE_0_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_MediaStreamVideoTrack_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_MediaStreamVideoTrack_1_0 = {
  .iface_macro = PPB_MEDIASTREAMVIDEOTRACK_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_MediaStreamVideoTrack_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_MessageLoop_1_0 = {
  .iface_macro = PPB_MESSAGELOOP_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_MessageLoop_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Messaging_1_0 = {
  .iface_macro = PPB_MESSAGING_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_Messaging_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Messaging_1_2 = {
  .iface_macro = PPB_MESSAGING_INTERFACE_1_2,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_Messaging_1_2,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_MouseLock_1_0 = {
  .iface_macro = PPB_MOUSELOCK_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_MouseLock_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetAddress_1_0 = {
  .iface_macro = PPB_NETADDRESS_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_NetAddress_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetworkList_1_0 = {
  .iface_macro = PPB_NETWORKLIST_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_NetworkList_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetworkMonitor_1_0 = {
  .iface_macro = PPB_NETWORKMONITOR_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_NetworkMonitor_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetworkProxy_1_0 = {
  .iface_macro = PPB_NETWORKPROXY_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_NetworkProxy_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPSocket_1_0 = {
  .iface_macro = PPB_TCPSOCKET_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_TCPSocket_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPSocket_1_1 = {
  .iface_macro = PPB_TCPSOCKET_INTERFACE_1_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_TCPSocket_1_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPSocket_1_2 = {
  .iface_macro = PPB_TCPSOCKET_INTERFACE_1_2,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_TCPSocket_1_2,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TextInputController_1_0 = {
  .iface_macro = PPB_TEXTINPUTCONTROLLER_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_TextInputController_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_UDPSocket_1_0 = {
  .iface_macro = PPB_UDPSOCKET_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_UDPSocket_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_UDPSocket_1_1 = {
  .iface_macro = PPB_UDPSOCKET_INTERFACE_1_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_UDPSocket_1_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_UDPSocket_1_2 = {
  .iface_macro = PPB_UDPSOCKET_INTERFACE_1_2,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_UDPSocket_1_2,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_URLLoader_1_0 = {
  .iface_macro = PPB_URLLOADER_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_URLLoader_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_URLRequestInfo_1_0 = {
  .iface_macro = PPB_URLREQUESTINFO_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_URLRequestInfo_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_URLResponseInfo_1_0 = {
  .iface_macro = PPB_URLRESPONSEINFO_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_URLResponseInfo_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Var_1_0 = {
  .iface_macro = PPB_VAR_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_Var_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Var_1_1 = {
  .iface_macro = PPB_VAR_INTERFACE_1_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_Var_1_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Var_1_2 = {
  .iface_macro = PPB_VAR_INTERFACE_1_2,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_Var_1_2,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VarArray_1_0 = {
  .iface_macro = PPB_VAR_ARRAY_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_VarArray_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VarArrayBuffer_1_0 = {
  .iface_macro = PPB_VAR_ARRAY_BUFFER_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_VarArrayBuffer_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VarDictionary_1_0 = {
  .iface_macro = PPB_VAR_DICTIONARY_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_VarDictionary_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VideoDecoder_0_1 = {
  .iface_macro = PPB_VIDEODECODER_INTERFACE_0_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_VideoDecoder_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VideoDecoder_0_2 = {
  .iface_macro = PPB_VIDEODECODER_INTERFACE_0_2,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_VideoDecoder_0_2,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VideoDecoder_1_0 = {
  .iface_macro = PPB_VIDEODECODER_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_VideoDecoder_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VideoDecoder_1_1 = {
  .iface_macro = PPB_VIDEODECODER_INTERFACE_1_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_VideoDecoder_1_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VideoEncoder_0_1 = {
  .iface_macro = PPB_VIDEOENCODER_INTERFACE_0_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_VideoEncoder_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VideoEncoder_0_2 = {
  .iface_macro = PPB_VIDEOENCODER_INTERFACE_0_2,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_VideoEncoder_0_2,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VpnProvider_0_1 = {
  .iface_macro = PPB_VPNPROVIDER_INTERFACE_0_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_VpnProvider_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_WebSocket_1_0 = {
  .iface_macro = PPB_WEBSOCKET_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_WebSocket_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPP_Messaging_1_0 = {
  .iface_macro = PPP_MESSAGING_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPP_Messaging_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_3 = {
  .iface_macro = PPB_AUDIO_INPUT_DEV_INTERFACE_0_3,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_AudioInput_Dev_0_3,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_4 = {
  .iface_macro = PPB_AUDIO_INPUT_DEV_INTERFACE_0_4,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_AudioInput_Dev_0_4,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_AudioOutput_Dev_0_1 = {
  .iface_macro = PPB_AUDIO_OUTPUT_DEV_INTERFACE_0_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_AudioOutput_Dev_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_DeviceRef_Dev_0_1 = {
  .iface_macro = PPB_DEVICEREF_DEV_INTERFACE_0_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_DeviceRef_Dev_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileChooser_Dev_0_5 = {
  .iface_macro = PPB_FILECHOOSER_DEV_INTERFACE_0_5,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_FileChooser_Dev_0_5,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileChooser_Dev_0_6 = {
  .iface_macro = PPB_FILECHOOSER_DEV_INTERFACE_0_6,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_FileChooser_Dev_0_6,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_1 = {
  .iface_macro = PPB_IME_INPUT_EVENT_DEV_INTERFACE_0_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_IMEInputEvent_Dev_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_2 = {
  .iface_macro = PPB_IME_INPUT_EVENT_DEV_INTERFACE_0_2,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_IMEInputEvent_Dev_0_2,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Printing_Dev_0_7 = {
  .iface_macro = PPB_PRINTING_DEV_INTERFACE_0_7,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_Printing_Dev_0_7,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_6 = {
  .iface_macro = PPB_URLUTIL_DEV_INTERFACE_0_6,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_URLUtil_Dev_0_6,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_7 = {
  .iface_macro = PPB_URLUTIL_DEV_INTERFACE_0_7,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_URLUtil_Dev_0_7,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_3 = {
  .iface_macro = PPB_VIDEOCAPTURE_DEV_INTERFACE_0_3,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_VideoCapture_Dev_0_3,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_VideoDecoder_Dev_0_16 = {
  .iface_macro = PPB_VIDEODECODER_DEV_INTERFACE_0_16,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_VideoDecoder_Dev_0_16,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_CameraDevice_Private_0_1 = {
  .iface_macro = PPB_CAMERADEVICE_PRIVATE_INTERFACE_0_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_CameraDevice_Private_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_DisplayColorProfile_Private_0_1 = {
  .iface_macro = PPB_DISPLAYCOLORPROFILE_PRIVATE_INTERFACE_0_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_DisplayColorProfile_Private_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Ext_CrxFileSystem_Private_0_1 = {
  .iface_macro = PPB_EXT_CRXFILESYSTEM_PRIVATE_INTERFACE_0_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_Ext_CrxFileSystem_Private_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileIO_Private_0_1 = {
  .iface_macro = PPB_FILEIO_PRIVATE_INTERFACE_0_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_FileIO_Private_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_FileRefPrivate_0_1 = {
  .iface_macro = PPB_FILEREFPRIVATE_INTERFACE_0_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_FileRefPrivate_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_HostResolver_Private_0_1 = {
  .iface_macro = PPB_HOSTRESOLVER_PRIVATE_INTERFACE_0_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_HostResolver_Private_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Instance_Private_0_1 = {
  .iface_macro = PPB_INSTANCE_PRIVATE_INTERFACE_0_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_Instance_Private_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_IsolatedFileSystem_Private_0_2 = {
  .iface_macro = PPB_ISOLATEDFILESYSTEM_PRIVATE_INTERFACE_0_2,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_IsolatedFileSystem_Private_0_2,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetAddress_Private_0_1 = {
  .iface_macro = PPB_NETADDRESS_PRIVATE_INTERFACE_0_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_NetAddress_Private_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetAddress_Private_1_0 = {
  .iface_macro = PPB_NETADDRESS_PRIVATE_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_NetAddress_Private_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1 = {
  .iface_macro = PPB_NETADDRESS_PRIVATE_INTERFACE_1_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_NetAddress_Private_1_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_1 = {
  .iface_macro = PPB_TCPSERVERSOCKET_PRIVATE_INTERFACE_0_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_TCPServerSocket_Private_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_2 = {
  .iface_macro = PPB_TCPSERVERSOCKET_PRIVATE_INTERFACE_0_2,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_TCPServerSocket_Private_0_2,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_3 = {
  .iface_macro = PPB_TCPSOCKET_PRIVATE_INTERFACE_0_3,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_TCPSocket_Private_0_3,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4 = {
  .iface_macro = PPB_TCPSOCKET_PRIVATE_INTERFACE_0_4,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_TCPSocket_Private_0_4,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5 = {
  .iface_macro = PPB_TCPSOCKET_PRIVATE_INTERFACE_0_5,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_TCPSocket_Private_0_5,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_Testing_Private_1_0 = {
  .iface_macro = PPB_TESTING_PRIVATE_INTERFACE_1_0,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_Testing_Private_1_0,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_2 = {
  .iface_macro = PPB_UDPSOCKET_PRIVATE_INTERFACE_0_2,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_UDPSocket_Private_0_2,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_3 = {
  .iface_macro = PPB_UDPSOCKET_PRIVATE_INTERFACE_0_3,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_UDPSocket_Private_0_3,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_4 = {
  .iface_macro = PPB_UDPSOCKET_PRIVATE_INTERFACE_0_4,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_UDPSocket_Private_0_4,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_UMA_Private_0_3 = {
  .iface_macro = PPB_UMA_PRIVATE_INTERFACE_0_3,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_UMA_Private_0_3,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPB_X509Certificate_Private_0_1 = {
  .iface_macro = PPB_X509CERTIFICATE_PRIVATE_INTERFACE_0_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPB_X509Certificate_Private_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo Pnacl_WrapperInfo_PPP_Instance_Private_0_1 = {
  .iface_macro = PPP_INSTANCE_PRIVATE_INTERFACE_0_1,
  .wrapped_iface = (const void *) &Pnacl_Wrappers_PPP_Instance_Private_0_1,
  .real_iface = NULL
};

static struct __PnaclWrapperInfo *s_ppb_wrappers[] = {
  &Pnacl_WrapperInfo_PPB_Console_1_0,
  &Pnacl_WrapperInfo_PPB_Core_1_0,
  &Pnacl_WrapperInfo_PPB_FileIO_1_0,
  &Pnacl_WrapperInfo_PPB_FileIO_1_1,
  &Pnacl_WrapperInfo_PPB_FileRef_1_0,
  &Pnacl_WrapperInfo_PPB_FileRef_1_1,
  &Pnacl_WrapperInfo_PPB_FileRef_1_2,
  &Pnacl_WrapperInfo_PPB_FileSystem_1_0,
  &Pnacl_WrapperInfo_PPB_Graphics2D_1_0,
  &Pnacl_WrapperInfo_PPB_Graphics2D_1_1,
  &Pnacl_WrapperInfo_PPB_Graphics2D_1_2,
  &Pnacl_WrapperInfo_PPB_Graphics3D_1_0,
  &Pnacl_WrapperInfo_PPB_HostResolver_1_0,
  &Pnacl_WrapperInfo_PPB_MouseInputEvent_1_0,
  &Pnacl_WrapperInfo_PPB_MouseInputEvent_1_1,
  &Pnacl_WrapperInfo_PPB_WheelInputEvent_1_0,
  &Pnacl_WrapperInfo_PPB_KeyboardInputEvent_1_0,
  &Pnacl_WrapperInfo_PPB_KeyboardInputEvent_1_2,
  &Pnacl_WrapperInfo_PPB_TouchInputEvent_1_0,
  &Pnacl_WrapperInfo_PPB_TouchInputEvent_1_4,
  &Pnacl_WrapperInfo_PPB_IMEInputEvent_1_0,
  &Pnacl_WrapperInfo_PPB_MediaStreamAudioTrack_0_1,
  &Pnacl_WrapperInfo_PPB_MediaStreamVideoTrack_0_1,
  &Pnacl_WrapperInfo_PPB_MediaStreamVideoTrack_1_0,
  &Pnacl_WrapperInfo_PPB_MessageLoop_1_0,
  &Pnacl_WrapperInfo_PPB_Messaging_1_0,
  &Pnacl_WrapperInfo_PPB_Messaging_1_2,
  &Pnacl_WrapperInfo_PPB_MouseLock_1_0,
  &Pnacl_WrapperInfo_PPB_NetAddress_1_0,
  &Pnacl_WrapperInfo_PPB_NetworkList_1_0,
  &Pnacl_WrapperInfo_PPB_NetworkMonitor_1_0,
  &Pnacl_WrapperInfo_PPB_NetworkProxy_1_0,
  &Pnacl_WrapperInfo_PPB_TCPSocket_1_0,
  &Pnacl_WrapperInfo_PPB_TCPSocket_1_1,
  &Pnacl_WrapperInfo_PPB_TCPSocket_1_2,
  &Pnacl_WrapperInfo_PPB_TextInputController_1_0,
  &Pnacl_WrapperInfo_PPB_UDPSocket_1_0,
  &Pnacl_WrapperInfo_PPB_UDPSocket_1_1,
  &Pnacl_WrapperInfo_PPB_UDPSocket_1_2,
  &Pnacl_WrapperInfo_PPB_URLLoader_1_0,
  &Pnacl_WrapperInfo_PPB_URLRequestInfo_1_0,
  &Pnacl_WrapperInfo_PPB_URLResponseInfo_1_0,
  &Pnacl_WrapperInfo_PPB_Var_1_0,
  &Pnacl_WrapperInfo_PPB_Var_1_1,
  &Pnacl_WrapperInfo_PPB_Var_1_2,
  &Pnacl_WrapperInfo_PPB_VarArray_1_0,
  &Pnacl_WrapperInfo_PPB_VarArrayBuffer_1_0,
  &Pnacl_WrapperInfo_PPB_VarDictionary_1_0,
  &Pnacl_WrapperInfo_PPB_VideoDecoder_0_1,
  &Pnacl_WrapperInfo_PPB_VideoDecoder_0_2,
  &Pnacl_WrapperInfo_PPB_VideoDecoder_1_0,
  &Pnacl_WrapperInfo_PPB_VideoDecoder_1_1,
  &Pnacl_WrapperInfo_PPB_VideoEncoder_0_1,
  &Pnacl_WrapperInfo_PPB_VideoEncoder_0_2,
  &Pnacl_WrapperInfo_PPB_VpnProvider_0_1,
  &Pnacl_WrapperInfo_PPB_WebSocket_1_0,
  &Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_3,
  &Pnacl_WrapperInfo_PPB_AudioInput_Dev_0_4,
  &Pnacl_WrapperInfo_PPB_AudioOutput_Dev_0_1,
  &Pnacl_WrapperInfo_PPB_DeviceRef_Dev_0_1,
  &Pnacl_WrapperInfo_PPB_FileChooser_Dev_0_5,
  &Pnacl_WrapperInfo_PPB_FileChooser_Dev_0_6,
  &Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_1,
  &Pnacl_WrapperInfo_PPB_IMEInputEvent_Dev_0_2,
  &Pnacl_WrapperInfo_PPB_Printing_Dev_0_7,
  &Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_6,
  &Pnacl_WrapperInfo_PPB_URLUtil_Dev_0_7,
  &Pnacl_WrapperInfo_PPB_VideoCapture_Dev_0_3,
  &Pnacl_WrapperInfo_PPB_VideoDecoder_Dev_0_16,
  &Pnacl_WrapperInfo_PPB_CameraDevice_Private_0_1,
  &Pnacl_WrapperInfo_PPB_DisplayColorProfile_Private_0_1,
  &Pnacl_WrapperInfo_PPB_Ext_CrxFileSystem_Private_0_1,
  &Pnacl_WrapperInfo_PPB_FileIO_Private_0_1,
  &Pnacl_WrapperInfo_PPB_FileRefPrivate_0_1,
  &Pnacl_WrapperInfo_PPB_HostResolver_Private_0_1,
  &Pnacl_WrapperInfo_PPB_Instance_Private_0_1,
  &Pnacl_WrapperInfo_PPB_IsolatedFileSystem_Private_0_2,
  &Pnacl_WrapperInfo_PPB_NetAddress_Private_0_1,
  &Pnacl_WrapperInfo_PPB_NetAddress_Private_1_0,
  &Pnacl_WrapperInfo_PPB_NetAddress_Private_1_1,
  &Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_1,
  &Pnacl_WrapperInfo_PPB_TCPServerSocket_Private_0_2,
  &Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_3,
  &Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_4,
  &Pnacl_WrapperInfo_PPB_TCPSocket_Private_0_5,
  &Pnacl_WrapperInfo_PPB_Testing_Private_1_0,
  &Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_2,
  &Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_3,
  &Pnacl_WrapperInfo_PPB_UDPSocket_Private_0_4,
  &Pnacl_WrapperInfo_PPB_UMA_Private_0_3,
  &Pnacl_WrapperInfo_PPB_X509Certificate_Private_0_1,
  NULL
};

static struct __PnaclWrapperInfo *s_ppp_wrappers[] = {
  &Pnacl_WrapperInfo_PPP_Messaging_1_0,
  &Pnacl_WrapperInfo_PPP_Instance_Private_0_1,
  NULL
};



static PPB_GetInterface __real_PPBGetInterface;
static PPP_GetInterface_Type __real_PPPGetInterface;

void __set_real_Pnacl_PPBGetInterface(PPB_GetInterface real) {
  __real_PPBGetInterface = real;
}

void __set_real_Pnacl_PPPGetInterface(PPP_GetInterface_Type real) {
  __real_PPPGetInterface = real;
}

/* Map interface string -> wrapper metadata */
static struct __PnaclWrapperInfo *PnaclPPBShimIface(
    const char *name) {
  struct __PnaclWrapperInfo **next = s_ppb_wrappers;
  while (*next != NULL) {
    if (mystrcmp(name, (*next)->iface_macro) == 0) return *next;
    ++next;
  }
  return NULL;
}

/* Map interface string -> wrapper metadata */
static struct __PnaclWrapperInfo *PnaclPPPShimIface(
    const char *name) {
  struct __PnaclWrapperInfo **next = s_ppp_wrappers;
  while (*next != NULL) {
    if (mystrcmp(name, (*next)->iface_macro) == 0) return *next;
    ++next;
  }
  return NULL;
}

const void *__Pnacl_PPBGetInterface(const char *name) {
  struct __PnaclWrapperInfo *wrapper = PnaclPPBShimIface(name);
  if (wrapper == NULL) {
    /* We did not generate a wrapper for this, so return the real interface. */
    return (*__real_PPBGetInterface)(name);
  }

  /* Initialize the real_iface if it hasn't been. The wrapper depends on it. */
  if (wrapper->real_iface == NULL) {
    const void *iface = (*__real_PPBGetInterface)(name);
    if (NULL == iface) return NULL;
    wrapper->real_iface = iface;
  }

  return wrapper->wrapped_iface;
}

const void *__Pnacl_PPPGetInterface(const char *name) {
  struct __PnaclWrapperInfo *wrapper = PnaclPPPShimIface(name);
  if (wrapper == NULL) {
    /* We did not generate a wrapper for this, so return the real interface. */
    return (*__real_PPPGetInterface)(name);
  }

  /* Initialize the real_iface if it hasn't been. The wrapper depends on it. */
  if (wrapper->real_iface == NULL) {
    const void *iface = (*__real_PPPGetInterface)(name);
    if (NULL == iface) return NULL;
    wrapper->real_iface = iface;
  }

  return wrapper->wrapped_iface;
}
