// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/content_browser_pepper_host_factory.h"

#include <stddef.h>
#include <utility>

#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"
#include "content/browser/renderer_host/pepper/pepper_browser_font_singleton_host.h"
#include "content/browser/renderer_host/pepper/pepper_file_io_host.h"
#include "content/browser/renderer_host/pepper/pepper_file_ref_host.h"
#include "content/browser/renderer_host/pepper/pepper_file_system_browser_host.h"
#include "content/browser/renderer_host/pepper/pepper_flash_file_message_filter.h"
#include "content/browser/renderer_host/pepper/pepper_gamepad_host.h"
#include "content/browser/renderer_host/pepper/pepper_host_resolver_message_filter.h"
#include "content/browser/renderer_host/pepper/pepper_network_monitor_host.h"
#include "content/browser/renderer_host/pepper/pepper_network_proxy_host.h"
#include "content/browser/renderer_host/pepper/pepper_print_settings_manager.h"
#include "content/browser/renderer_host/pepper/pepper_printing_host.h"
#include "content/browser/renderer_host/pepper/pepper_tcp_server_socket_message_filter.h"
#include "content/browser/renderer_host/pepper/pepper_tcp_socket_message_filter.h"
#include "content/browser/renderer_host/pepper/pepper_truetype_font_host.h"
#include "content/browser/renderer_host/pepper/pepper_truetype_font_list_host.h"
#include "content/browser/renderer_host/pepper/pepper_udp_socket_message_filter.h"
#include "ppapi/host/message_filter_host.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

#if defined(OS_CHROMEOS)
#include "content/browser/renderer_host/pepper/pepper_vpn_provider_message_filter_chromeos.h"
#endif

namespace content {

namespace {

const size_t kMaxSocketsAllowed = 1024;

bool CanCreateSocket() {
  return PepperTCPServerSocketMessageFilter::GetNumInstances() +
             PepperTCPSocketMessageFilter::GetNumInstances() +
             PepperUDPSocketMessageFilter::GetNumInstances() <
         kMaxSocketsAllowed;
}

}  // namespace

ContentBrowserPepperHostFactory::ContentBrowserPepperHostFactory(
    BrowserPpapiHostImpl* host)
    : host_(host) {}

ContentBrowserPepperHostFactory::~ContentBrowserPepperHostFactory() {}

std::unique_ptr<ppapi::host::ResourceHost>
ContentBrowserPepperHostFactory::CreateResourceHost(
    ppapi::host::PpapiHost* host,
    PP_Resource resource,
    PP_Instance instance,
    const IPC::Message& message) {
  DCHECK(host == host_->GetPpapiHost());

  // Make sure the plugin is giving us a valid instance for this resource.
  if (!host_->IsValidInstance(instance))
    return std::unique_ptr<ppapi::host::ResourceHost>();

  // Public interfaces.
  switch (message.type()) {
    case PpapiHostMsg_FileIO_Create::ID: {
      return std::unique_ptr<ppapi::host::ResourceHost>(
          new PepperFileIOHost(host_, instance, resource));
    }
    case PpapiHostMsg_FileSystem_Create::ID: {
      PP_FileSystemType file_system_type;
      if (!ppapi::UnpackMessage<PpapiHostMsg_FileSystem_Create>(
              message, &file_system_type)) {
        NOTREACHED();
        return std::unique_ptr<ppapi::host::ResourceHost>();
      }
      return std::unique_ptr<ppapi::host::ResourceHost>(
          new PepperFileSystemBrowserHost(host_, instance, resource,
                                          file_system_type));
    }
    case PpapiHostMsg_Gamepad_Create::ID: {
      return std::unique_ptr<ppapi::host::ResourceHost>(
          new PepperGamepadHost(host_, instance, resource));
    }
    case PpapiHostMsg_NetworkProxy_Create::ID: {
      return std::unique_ptr<ppapi::host::ResourceHost>(
          new PepperNetworkProxyHost(host_, instance, resource));
    }
    case PpapiHostMsg_HostResolver_Create::ID: {
      scoped_refptr<ppapi::host::ResourceMessageFilter> host_resolver(
          new PepperHostResolverMessageFilter(host_, instance, false));
      return std::unique_ptr<ppapi::host::ResourceHost>(
          new ppapi::host::MessageFilterHost(host_->GetPpapiHost(), instance,
                                             resource, host_resolver));
    }
    case PpapiHostMsg_FileRef_CreateForFileAPI::ID: {
      PP_Resource file_system;
      std::string internal_path;
      if (!ppapi::UnpackMessage<PpapiHostMsg_FileRef_CreateForFileAPI>(
              message, &file_system, &internal_path)) {
        NOTREACHED();
        return std::unique_ptr<ppapi::host::ResourceHost>();
      }
      return std::unique_ptr<ppapi::host::ResourceHost>(new PepperFileRefHost(
          host_, instance, resource, file_system, internal_path));
    }
  }

  // Socket interfaces.
  if (GetPermissions().HasPermission(ppapi::PERMISSION_SOCKET)) {
    switch (message.type()) {
      case PpapiHostMsg_TCPSocket_Create::ID: {
        ppapi::TCPSocketVersion version;
        if (!ppapi::UnpackMessage<PpapiHostMsg_TCPSocket_Create>(message,
                                                                 &version) ||
            version == ppapi::TCP_SOCKET_VERSION_PRIVATE) {
          return nullptr;
        }
        if (!CanCreateSocket())
          return nullptr;
        return CreateNewTCPSocket(instance, resource, version);
      }
      case PpapiHostMsg_UDPSocket_Create::ID: {
        if (!CanCreateSocket())
          return nullptr;
        scoped_refptr<ppapi::host::ResourceMessageFilter> udp_socket(
            new PepperUDPSocketMessageFilter(host_, instance, false));
        return std::make_unique<ppapi::host::MessageFilterHost>(
            host_->GetPpapiHost(), instance, resource, udp_socket);
      }

      // The following interfaces are "private" because permission will be
      // checked against a whitelist of apps at the time of the corresponding
      // instance's method calls (because permission check can be performed
      // only on the UI thread).
      case PpapiHostMsg_TCPServerSocket_CreatePrivate::ID: {
        if (!CanCreateSocket())
          return nullptr;
        scoped_refptr<ppapi::host::ResourceMessageFilter> tcp_server_socket(
            new PepperTCPServerSocketMessageFilter(this, host_, instance,
                                                   true));
        return std::make_unique<ppapi::host::MessageFilterHost>(
            host_->GetPpapiHost(), instance, resource, tcp_server_socket);
      }
      case PpapiHostMsg_TCPSocket_CreatePrivate::ID: {
        if (!CanCreateSocket())
          return nullptr;
        return CreateNewTCPSocket(instance, resource,
                                  ppapi::TCP_SOCKET_VERSION_PRIVATE);
      }
      case PpapiHostMsg_UDPSocket_CreatePrivate::ID: {
        if (!CanCreateSocket())
          return nullptr;
        scoped_refptr<ppapi::host::ResourceMessageFilter> udp_socket(
            new PepperUDPSocketMessageFilter(host_, instance, true));
        return std::make_unique<ppapi::host::MessageFilterHost>(
            host_->GetPpapiHost(), instance, resource, udp_socket);
      }
    }
  }

  // Dev interfaces.
  if (GetPermissions().HasPermission(ppapi::PERMISSION_DEV)) {
    switch (message.type()) {
      case PpapiHostMsg_Printing_Create::ID: {
        std::unique_ptr<PepperPrintSettingsManager> manager(
            new PepperPrintSettingsManagerImpl());
        return std::unique_ptr<ppapi::host::ResourceHost>(
            new PepperPrintingHost(host_->GetPpapiHost(), instance, resource,
                                   std::move(manager)));
      }
      case PpapiHostMsg_TrueTypeFont_Create::ID: {
        ppapi::proxy::SerializedTrueTypeFontDesc desc;
        if (!ppapi::UnpackMessage<PpapiHostMsg_TrueTypeFont_Create>(message,
                                                                    &desc)) {
          NOTREACHED();
          return std::unique_ptr<ppapi::host::ResourceHost>();
        }
        // Check that the family name is valid UTF-8 before passing it to the
        // host OS.
        if (!base::IsStringUTF8(desc.family))
          return std::unique_ptr<ppapi::host::ResourceHost>();

        return std::unique_ptr<ppapi::host::ResourceHost>(
            new PepperTrueTypeFontHost(host_, instance, resource, desc));
      }
      case PpapiHostMsg_TrueTypeFontSingleton_Create::ID: {
        return std::unique_ptr<ppapi::host::ResourceHost>(
            new PepperTrueTypeFontListHost(host_, instance, resource));
      }
#if defined(OS_CHROMEOS)
      case PpapiHostMsg_VpnProvider_Create::ID: {
        scoped_refptr<PepperVpnProviderMessageFilter> vpn_provider(
            new PepperVpnProviderMessageFilter(host_, instance));
        return std::make_unique<ppapi::host::MessageFilterHost>(
            host_->GetPpapiHost(), instance, resource, std::move(vpn_provider));
      }
#endif
    }
  }

  // Private interfaces.
  if (GetPermissions().HasPermission(ppapi::PERMISSION_PRIVATE)) {
    switch (message.type()) {
      case PpapiHostMsg_BrowserFontSingleton_Create::ID:
        return std::unique_ptr<ppapi::host::ResourceHost>(
            new PepperBrowserFontSingletonHost(host_, instance, resource));
    }
  }

  // Permissions for the following interfaces will be checked at the
  // time of the corresponding instance's methods calls (because
  // permission check can be performed only on the UI
  // thread). Currently these interfaces are available only for
  // whitelisted apps which may not have access to the other private
  // interfaces.
  if (message.type() == PpapiHostMsg_HostResolver_CreatePrivate::ID) {
    scoped_refptr<ppapi::host::ResourceMessageFilter> host_resolver(
        new PepperHostResolverMessageFilter(host_, instance, true));
    return std::unique_ptr<ppapi::host::ResourceHost>(
        new ppapi::host::MessageFilterHost(host_->GetPpapiHost(), instance,
                                           resource, host_resolver));
  }
  if (message.type() == PpapiHostMsg_NetworkMonitor_Create::ID) {
    return std::unique_ptr<ppapi::host::ResourceHost>(
        new PepperNetworkMonitorHost(host_, instance, resource));
  }

  // Flash interfaces.
  if (GetPermissions().HasPermission(ppapi::PERMISSION_FLASH)) {
    switch (message.type()) {
      case PpapiHostMsg_FlashFile_Create::ID: {
        scoped_refptr<ppapi::host::ResourceMessageFilter> file_filter(
            new PepperFlashFileMessageFilter(instance, host_));
        return std::unique_ptr<ppapi::host::ResourceHost>(
            new ppapi::host::MessageFilterHost(host_->GetPpapiHost(), instance,
                                               resource, file_filter));
      }
    }
  }

  return std::unique_ptr<ppapi::host::ResourceHost>();
}

std::unique_ptr<ppapi::host::ResourceHost>
ContentBrowserPepperHostFactory::CreateAcceptedTCPSocket(
    PP_Instance instance,
    ppapi::TCPSocketVersion version,
    mojo::PendingRemote<network::mojom::TCPConnectedSocket> connected_socket,
    mojo::PendingReceiver<network::mojom::SocketObserver>
        socket_observer_receiver,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  if (!CanCreateSocket())
    return std::unique_ptr<ppapi::host::ResourceHost>();
  scoped_refptr<PepperTCPSocketMessageFilter> tcp_socket(
      base::MakeRefCounted<PepperTCPSocketMessageFilter>(
          nullptr /* factory */, host_, instance, version));
  tcp_socket->SetConnectedSocket(
      std::move(connected_socket), std::move(socket_observer_receiver),
      std::move(receive_stream), std::move(send_stream));
  return std::unique_ptr<ppapi::host::ResourceHost>(
      new ppapi::host::MessageFilterHost(host_->GetPpapiHost(), instance, 0,
                                         tcp_socket));
}

std::unique_ptr<ppapi::host::ResourceHost>
ContentBrowserPepperHostFactory::CreateNewTCPSocket(
    PP_Instance instance,
    PP_Resource resource,
    ppapi::TCPSocketVersion version) {
  scoped_refptr<ppapi::host::ResourceMessageFilter> tcp_socket(
      new PepperTCPSocketMessageFilter(this, host_, instance, version));
  if (!tcp_socket.get())
    return std::unique_ptr<ppapi::host::ResourceHost>();

  return std::unique_ptr<ppapi::host::ResourceHost>(
      new ppapi::host::MessageFilterHost(host_->GetPpapiHost(), instance,
                                         resource, tcp_socket));
}

const ppapi::PpapiPermissions& ContentBrowserPepperHostFactory::GetPermissions()
    const {
  return host_->GetPpapiHost()->permissions();
}

}  // namespace content
