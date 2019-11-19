// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/interface_list.h"

#include <memory>
#include <stdint.h>

#include "base/hash/hash.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "ppapi/c/dev/ppb_audio_input_dev.h"
#include "ppapi/c/dev/ppb_audio_output_dev.h"
#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/c/dev/ppb_char_set_dev.h"
#include "ppapi/c/dev/ppb_crypto_dev.h"
#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/c/dev/ppb_device_ref_dev.h"
#include "ppapi/c/dev/ppb_gles_chromium_texture_mapping_dev.h"
#include "ppapi/c/dev/ppb_ime_input_event_dev.h"
#include "ppapi/c/dev/ppb_memory_dev.h"
#include "ppapi/c/dev/ppb_opengles2ext_dev.h"
#include "ppapi/c/dev/ppb_printing_dev.h"
#include "ppapi/c/dev/ppb_text_input_dev.h"
#include "ppapi/c/dev/ppb_trace_event_dev.h"
#include "ppapi/c/dev/ppb_truetype_font_dev.h"
#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/dev/ppb_video_capture_dev.h"
#include "ppapi/c/dev/ppb_view_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_buffer.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_audio_encoder.h"
#include "ppapi/c/ppb_console.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/ppb_file_ref.h"
#include "ppapi/c/ppb_file_system.h"
#include "ppapi/c/ppb_fullscreen.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_host_resolver.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_media_stream_audio_track.h"
#include "ppapi/c/ppb_media_stream_video_track.h"
#include "ppapi/c/ppb_message_loop.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_mouse_lock.h"
#include "ppapi/c/ppb_net_address.h"
#include "ppapi/c/ppb_network_list.h"
#include "ppapi/c/ppb_network_monitor.h"
#include "ppapi/c/ppb_network_proxy.h"
#include "ppapi/c/ppb_opengles2.h"
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
#include "ppapi/c/ppb_video_frame.h"
#include "ppapi/c/ppb_view.h"
#include "ppapi/c/ppb_vpn_provider.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/private/ppb_camera_capabilities_private.h"
#include "ppapi/c/private/ppb_camera_device_private.h"
#include "ppapi/c/private/ppb_ext_crx_file_system_private.h"
#include "ppapi/c/private/ppb_file_io_private.h"
#include "ppapi/c/private/ppb_file_ref_private.h"
#include "ppapi/c/private/ppb_find_private.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/c/private/ppb_flash_clipboard.h"
#include "ppapi/c/private/ppb_flash_drm.h"
#include "ppapi/c/private/ppb_flash_file.h"
#include "ppapi/c/private/ppb_flash_font_file.h"
#include "ppapi/c/private/ppb_flash_fullscreen.h"
#include "ppapi/c/private/ppb_flash_menu.h"
#include "ppapi/c/private/ppb_flash_message_loop.h"
#include "ppapi/c/private/ppb_flash_print.h"
#include "ppapi/c/private/ppb_host_resolver_private.h"
#include "ppapi/c/private/ppb_isolated_file_system_private.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/c/private/ppb_tcp_server_socket_private.h"
#include "ppapi/c/private/ppb_tcp_socket_private.h"
#include "ppapi/c/private/ppb_testing_private.h"
#include "ppapi/c/private/ppb_udp_socket_private.h"
#include "ppapi/c/private/ppb_uma_private.h"
#include "ppapi/c/private/ppb_x509_certificate_private.h"
#include "ppapi/c/trusted/ppb_broker_trusted.h"
#include "ppapi/c/trusted/ppb_browser_font_trusted.h"
#include "ppapi/c/trusted/ppb_char_set_trusted.h"
#include "ppapi/c/trusted/ppb_file_chooser_trusted.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_audio_proxy.h"
#include "ppapi/proxy/ppb_broker_proxy.h"
#include "ppapi/proxy/ppb_buffer_proxy.h"
#include "ppapi/proxy/ppb_core_proxy.h"
#include "ppapi/proxy/ppb_flash_message_loop_proxy.h"
#include "ppapi/proxy/ppb_graphics_3d_proxy.h"
#include "ppapi/proxy/ppb_image_data_proxy.h"
#include "ppapi/proxy/ppb_instance_proxy.h"
#include "ppapi/proxy/ppb_message_loop_proxy.h"
#include "ppapi/proxy/ppb_testing_proxy.h"
#include "ppapi/proxy/ppb_var_deprecated_proxy.h"
#include "ppapi/proxy/ppb_video_decoder_proxy.h"
#include "ppapi/proxy/ppb_x509_certificate_private_proxy.h"
#include "ppapi/proxy/ppp_class_proxy.h"
#include "ppapi/proxy/ppp_find_proxy.h"
#include "ppapi/proxy/ppp_graphics_3d_proxy.h"
#include "ppapi/proxy/ppp_input_event_proxy.h"
#include "ppapi/proxy/ppp_instance_private_proxy.h"
#include "ppapi/proxy/ppp_instance_proxy.h"
#include "ppapi/proxy/ppp_messaging_proxy.h"
#include "ppapi/proxy/ppp_mouse_lock_proxy.h"
#include "ppapi/proxy/ppp_pdf_proxy.h"
#include "ppapi/proxy/ppp_printing_proxy.h"
#include "ppapi/proxy/ppp_text_input_proxy.h"
#include "ppapi/proxy/ppp_video_decoder_proxy.h"
#include "ppapi/proxy/resource_creation_proxy.h"
#include "ppapi/shared_impl/ppb_opengles2_shared.h"
#include "ppapi/shared_impl/ppb_var_shared.h"
#include "ppapi/thunk/thunk.h"

// Helper to get the proxy name PPB_Foo_Proxy given the API name PPB_Foo.
#define PROXY_CLASS_NAME(api_name) api_name##_Proxy

// Helper to get the interface ID PPB_Foo_Proxy::kApiID given the API
// name PPB_Foo.
#define PROXY_API_ID(api_name) PROXY_CLASS_NAME(api_name)::kApiID

// Helper to get the name of the templatized factory function.
#define PROXY_FACTORY_NAME(api_name) ProxyFactory<PROXY_CLASS_NAME(api_name)>

// Helper to get the name of the thunk GetPPB_Foo_1_0_Thunk given the interface
// struct name PPB_Foo_1_0.
#define INTERFACE_THUNK_NAME(iface_struct) thunk::Get##iface_struct##_Thunk

namespace ppapi {
namespace proxy {

namespace {

template<typename ProxyClass>
InterfaceProxy* ProxyFactory(Dispatcher* dispatcher) {
  return new ProxyClass(dispatcher);
}

base::LazyInstance<PpapiPermissions>::DestructorAtExit
    g_process_global_permissions;

}  // namespace

InterfaceList::InterfaceList() {
  memset(id_to_factory_, 0, sizeof(id_to_factory_));

  // Register the API factories for each of the API types. This calls AddProxy
  // for each InterfaceProxy type we support.
  #define PROXIED_API(api_name) \
      AddProxy(PROXY_API_ID(api_name), &PROXY_FACTORY_NAME(api_name));

  // Register each proxied interface by calling AddPPB for each supported
  // interface. Set current_required_permission to the appropriate value for
  // the value you want expanded by this macro.
  #define PROXIED_IFACE(iface_str, iface_struct) \
      AddPPB(iface_str, \
             INTERFACE_THUNK_NAME(iface_struct)(), \
             current_required_permission);

  // clang-format off
  {
    Permission current_required_permission = PERMISSION_NONE;
    #include "ppapi/thunk/interfaces_ppb_private_no_permissions.h"
    #include "ppapi/thunk/interfaces_ppb_public_stable.h"
  }
  {
    Permission current_required_permission = PERMISSION_DEV;
    #include "ppapi/thunk/interfaces_ppb_public_dev.h"
  }
  {
    Permission current_required_permission = PERMISSION_PRIVATE;
    #include "ppapi/thunk/interfaces_ppb_private.h"
  }
#if !defined(OS_NACL)
  {
    Permission current_required_permission = PERMISSION_FLASH;
    #include "ppapi/thunk/interfaces_ppb_private_flash.h"
  }
  {
    Permission current_required_permission = PERMISSION_PDF;
    #include "ppapi/thunk/interfaces_ppb_private_pdf.h"
  }
#endif  // !defined(OS_NACL)
  {
    Permission current_required_permission = PERMISSION_DEV_CHANNEL;
    #include "ppapi/thunk/interfaces_ppb_public_dev_channel.h"
  }
  {
    Permission current_required_permission = PERMISSION_SOCKET;
    #include "ppapi/thunk/interfaces_ppb_public_socket.h"
  }
  // clang-format on

#undef PROXIED_API
#undef PROXIED_IFACE

  // Manually add some special proxies. Some of these don't have interfaces
  // that they support, so aren't covered by the macros above, but have proxies
  // for message routing. Others have different implementations between the
  // proxy and the impl and there's no obvious message routing.
  AddProxy(API_ID_RESOURCE_CREATION, &ResourceCreationProxy::Create);
  AddProxy(API_ID_PPP_CLASS, &PPP_Class_Proxy::Create);
  AddPPB(PPB_CORE_INTERFACE_1_0,
         PPB_Core_Proxy::GetPPB_Core_Interface(), PERMISSION_NONE);
  AddPPB(PPB_MESSAGELOOP_INTERFACE_1_0,
         PPB_MessageLoop_Proxy::GetInterface(), PERMISSION_NONE);
  AddPPB(PPB_OPENGLES2_INTERFACE_1_0,
         PPB_OpenGLES2_Shared::GetInterface(), PERMISSION_NONE);
  AddPPB(PPB_OPENGLES2_INSTANCEDARRAYS_INTERFACE_1_0,
         PPB_OpenGLES2_Shared::GetInstancedArraysInterface(), PERMISSION_NONE);
  AddPPB(PPB_OPENGLES2_FRAMEBUFFERBLIT_INTERFACE_1_0,
         PPB_OpenGLES2_Shared::GetFramebufferBlitInterface(), PERMISSION_NONE);
  AddPPB(PPB_OPENGLES2_FRAMEBUFFERMULTISAMPLE_INTERFACE_1_0,
         PPB_OpenGLES2_Shared::GetFramebufferMultisampleInterface(),
         PERMISSION_NONE);
  AddPPB(PPB_OPENGLES2_CHROMIUMENABLEFEATURE_INTERFACE_1_0,
         PPB_OpenGLES2_Shared::GetChromiumEnableFeatureInterface(),
         PERMISSION_NONE);
  AddPPB(PPB_OPENGLES2_CHROMIUMMAPSUB_INTERFACE_1_0,
         PPB_OpenGLES2_Shared::GetChromiumMapSubInterface(), PERMISSION_NONE);
  AddPPB(PPB_OPENGLES2_CHROMIUMMAPSUB_DEV_INTERFACE_1_0,
         PPB_OpenGLES2_Shared::GetChromiumMapSubInterface(), PERMISSION_NONE);
  AddPPB(PPB_OPENGLES2_QUERY_INTERFACE_1_0,
         PPB_OpenGLES2_Shared::GetQueryInterface(), PERMISSION_NONE);
  AddPPB(PPB_OPENGLES2_VERTEXARRAYOBJECT_INTERFACE_1_0,
         PPB_OpenGLES2_Shared::GetVertexArrayObjectInterface(),
         PERMISSION_NONE);
  AddPPB(PPB_OPENGLES2_DRAWBUFFERS_DEV_INTERFACE_1_0,
         PPB_OpenGLES2_Shared::GetDrawBuffersInterface(),
         PERMISSION_DEV);
  AddPPB(PPB_VAR_ARRAY_BUFFER_INTERFACE_1_0,
         PPB_Var_Shared::GetVarArrayBufferInterface1_0(),
         PERMISSION_NONE);
  AddPPB(PPB_VAR_INTERFACE_1_2,
         PPB_Var_Shared::GetVarInterface1_2(), PERMISSION_NONE);
  AddPPB(PPB_VAR_INTERFACE_1_1,
         PPB_Var_Shared::GetVarInterface1_1(), PERMISSION_NONE);
  AddPPB(PPB_VAR_INTERFACE_1_0,
         PPB_Var_Shared::GetVarInterface1_0(), PERMISSION_NONE);

#if !defined(OS_NACL)
  // PPB (browser) interfaces.
  // Do not add more stuff here, they should be added to interface_list*.h
  // TODO(brettw) remove these.
  AddProxy(API_ID_PPB_INSTANCE_PRIVATE, &ProxyFactory<PPB_Instance_Proxy>);
  AddPPB(PPB_INSTANCE_PRIVATE_INTERFACE_0_1,
         thunk::GetPPB_Instance_Private_0_1_Thunk(), PERMISSION_PRIVATE);

  AddProxy(API_ID_PPB_VAR_DEPRECATED, &ProxyFactory<PPB_Var_Deprecated_Proxy>);
  AddPPB(PPB_VAR_DEPRECATED_INTERFACE,
         PPB_Var_Deprecated_Proxy::GetProxyInterface(), PERMISSION_FLASH);
#endif
  AddProxy(API_ID_PPB_TESTING, &ProxyFactory<PPB_Testing_Proxy>);
  AddPPB(PPB_TESTING_PRIVATE_INTERFACE,
         PPB_Testing_Proxy::GetProxyInterface(), PERMISSION_TESTING);

  // PPP (plugin) interfaces.
  // TODO(brettw) move these to interface_list*.h
  AddProxy(API_ID_PPP_GRAPHICS_3D, &ProxyFactory<PPP_Graphics3D_Proxy>);
  AddPPP(PPP_GRAPHICS_3D_INTERFACE, PPP_Graphics3D_Proxy::GetProxyInterface());
  AddProxy(API_ID_PPP_INPUT_EVENT, &ProxyFactory<PPP_InputEvent_Proxy>);
  AddPPP(PPP_INPUT_EVENT_INTERFACE, PPP_InputEvent_Proxy::GetProxyInterface());
  AddProxy(API_ID_PPP_INSTANCE, &ProxyFactory<PPP_Instance_Proxy>);
#if !defined(OS_NACL)
  AddPPP(PPP_INSTANCE_INTERFACE_1_1,
         PPP_Instance_Proxy::GetInstanceInterface());
  AddProxy(API_ID_PPP_INSTANCE_PRIVATE,
           &ProxyFactory<PPP_Instance_Private_Proxy>);
  AddPPP(PPP_INSTANCE_PRIVATE_INTERFACE,
         PPP_Instance_Private_Proxy::GetProxyInterface());
#endif
  AddProxy(API_ID_PPP_MESSAGING, &ProxyFactory<PPP_Messaging_Proxy>);
  AddProxy(API_ID_PPP_MOUSE_LOCK, &ProxyFactory<PPP_MouseLock_Proxy>);
  AddPPP(PPP_MOUSELOCK_INTERFACE, PPP_MouseLock_Proxy::GetProxyInterface());
  AddProxy(API_ID_PPP_PRINTING, &ProxyFactory<PPP_Printing_Proxy>);
  AddPPP(PPP_PRINTING_DEV_INTERFACE, PPP_Printing_Proxy::GetProxyInterface());
  AddProxy(API_ID_PPP_TEXT_INPUT, &ProxyFactory<PPP_TextInput_Proxy>);
  AddPPP(PPP_TEXTINPUT_DEV_INTERFACE, PPP_TextInput_Proxy::GetProxyInterface());
#if !defined(OS_NACL)
  AddProxy(API_ID_PPP_PDF, &ProxyFactory<PPP_Pdf_Proxy>);
  AddPPP(PPP_PDF_INTERFACE, PPP_Pdf_Proxy::GetProxyInterface());
  AddProxy(API_ID_PPP_FIND_PRIVATE, &ProxyFactory<PPP_Find_Proxy>);
  AddPPP(PPP_FIND_PRIVATE_INTERFACE, PPP_Find_Proxy::GetProxyInterface());
  AddProxy(API_ID_PPP_VIDEO_DECODER_DEV, &ProxyFactory<PPP_VideoDecoder_Proxy>);
  AddPPP(PPP_VIDEODECODER_DEV_INTERFACE,
         PPP_VideoDecoder_Proxy::GetProxyInterface());
#endif
}

InterfaceList::~InterfaceList() {
}

// static
InterfaceList* InterfaceList::GetInstance() {
  // CAUTION: This function is called without the ProxyLock to avoid excessive
  // excessive locking from C++ wrappers. (See also GetBrowserInterface.)
  return base::Singleton<InterfaceList>::get();
}

// static
void InterfaceList::SetProcessGlobalPermissions(
    const PpapiPermissions& permissions) {
  g_process_global_permissions.Get() = permissions;
}

InterfaceProxy::Factory InterfaceList::GetFactoryForID(ApiID id) const {
  int index = static_cast<int>(id);
  static_assert(API_ID_NONE == 0, "none must be zero");
  if (id <= 0 || id >= API_ID_COUNT)
    return nullptr;
  return id_to_factory_[index];
}

const void* InterfaceList::GetInterfaceForPPB(const std::string& name) {
  // CAUTION: This function is called without the ProxyLock to avoid excessive
  // excessive locking from C++ wrappers. (See also GetBrowserInterface.)
  auto found = name_to_browser_info_.find(name);
  if (found == name_to_browser_info_.end())
    return nullptr;

  if (g_process_global_permissions.Get().HasPermission(
          found->second->required_permission())) {
    // Only log interface use once per plugin.
    found->second->LogWithUmaOnce(
        PluginGlobals::Get()->GetBrowserSender(), name);
    return found->second->iface();
  }
  return nullptr;
}

const void* InterfaceList::GetInterfaceForPPP(const std::string& name) const {
  auto found = name_to_plugin_info_.find(name);
  if (found == name_to_plugin_info_.end())
    return nullptr;
  return found->second->iface();
}

void InterfaceList::InterfaceInfo::LogWithUmaOnce(
    IPC::Sender* sender, const std::string& name) {
  {
    base::AutoLock acquire(sent_to_uma_lock_);
    if (sent_to_uma_)
      return;
    sent_to_uma_ = true;
  }
  int hash = InterfaceList::HashInterfaceName(name);
  PluginGlobals::Get()->GetBrowserSender()->Send(
      new PpapiHostMsg_LogInterfaceUsage(hash));
}

void InterfaceList::AddProxy(ApiID id,
                             InterfaceProxy::Factory factory) {
  // For interfaces with no corresponding _Proxy objects, the macros will
  // generate calls to this function with API_ID_NONE. This means we
  // should just skip adding a factory for these functions.
  if (id == API_ID_NONE)
    return;

  // The factory should be an exact dupe of the one we already have if it
  // has already been registered before.
  int index = static_cast<int>(id);
  DCHECK(!id_to_factory_[index] || id_to_factory_[index] == factory);

  id_to_factory_[index] = factory;
}

void InterfaceList::AddPPB(const char* name,
                           const void* iface,
                           Permission perm) {
  DCHECK(name_to_browser_info_.find(name) == name_to_browser_info_.end());
  name_to_browser_info_[name] = std::make_unique<InterfaceInfo>(iface, perm);
}

void InterfaceList::AddPPP(const char* name,
                           const void* iface) {
  DCHECK(name_to_plugin_info_.find(name) == name_to_plugin_info_.end());
  name_to_plugin_info_[name] =
      std::make_unique<InterfaceInfo>(iface, PERMISSION_NONE);
}

int InterfaceList::HashInterfaceName(const std::string& name) {
  uint32_t data = base::Hash(name);
  // Strip off the signed bit because UMA doesn't support negative values,
  // but takes a signed int as input.
  return static_cast<int>(data & 0x7fffffff);
}

}  // namespace proxy
}  // namespace ppapi
