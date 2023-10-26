// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/api_browser_context_keyed_service_factories.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "extensions/browser/api/alarms/alarm_manager.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/audio/audio_api.h"
#include "extensions/browser/api/bluetooth/bluetooth_api.h"
#include "extensions/browser/api/bluetooth/bluetooth_private_api.h"
#include "extensions/browser/api/bluetooth_low_energy/bluetooth_api_advertisement.h"
#include "extensions/browser/api/bluetooth_low_energy/bluetooth_low_energy_connection.h"
#include "extensions/browser/api/bluetooth_low_energy/bluetooth_low_energy_notify_session.h"
#include "extensions/browser/api/bluetooth_socket/bluetooth_api_socket.h"
#include "extensions/browser/api/bluetooth_socket/bluetooth_socket_event_dispatcher.h"
#include "extensions/browser/api/content_settings/content_settings_service.h"
#include "extensions/browser/api/declarative/rules_registry_service.h"
#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"
#include "extensions/browser/api/feedback_private/feedback_private_api.h"
#include "extensions/browser/api/hid/hid_connection_resource.h"
#include "extensions/browser/api/hid/hid_device_manager.h"
#include "extensions/browser/api/idle/idle_manager_factory.h"
#include "extensions/browser/api/management/management_api.h"
#include "extensions/browser/api/messaging/message_service.h"
#include "extensions/browser/api/messaging/messaging_api_message_filter.h"
#include "extensions/browser/api/networking_private/networking_private_event_router_factory.h"
#include "extensions/browser/api/offscreen/offscreen_document_manager.h"
#include "extensions/browser/api/power/power_api.h"
#include "extensions/browser/api/printer_provider/printer_provider_api_factory.h"
#include "extensions/browser/api/runtime/runtime_api.h"
#include "extensions/browser/api/serial/serial_connection.h"
#include "extensions/browser/api/serial/serial_port_manager.h"
#include "extensions/browser/api/socket/socket.h"
#include "extensions/browser/api/socket/tcp_socket.h"
#include "extensions/browser/api/socket/udp_socket.h"
#include "extensions/browser/api/sockets_tcp/tcp_socket_event_dispatcher.h"
#include "extensions/browser/api/sockets_tcp_server/tcp_server_socket_event_dispatcher.h"
#include "extensions/browser/api/sockets_udp/udp_socket_event_dispatcher.h"
#include "extensions/browser/api/storage/session_storage_manager.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/api/system_info/system_info_api.h"
#include "extensions/browser/api/usb/usb_device_manager.h"
#include "extensions/browser/api/usb/usb_device_resource.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/api/web_request/web_request_proxying_url_loader_factory.h"
#include "extensions/browser/api/web_request/web_request_proxying_websocket.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "extensions/browser/api/clipboard/clipboard_api.h"
#include "extensions/browser/api/socket/app_firewall_hole_manager.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "extensions/browser/api/feedback_private/log_source_resource.h"
#include "extensions/browser/api/media_perception_private/media_perception_api_manager.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_private_api.h"
#include "extensions/browser/api/webcam_private/webcam_private_api.h"
#endif

namespace extensions {

void EnsureApiBrowserContextKeyedServiceFactoriesBuilt() {
  AlarmManager::GetFactoryInstance();
  ApiResourceManager<BluetoothApiAdvertisement>::GetFactoryInstance();
  ApiResourceManager<BluetoothApiSocket>::GetFactoryInstance();
  ApiResourceManager<BluetoothLowEnergyConnection>::GetFactoryInstance();
  ApiResourceManager<BluetoothLowEnergyNotifySession>::GetFactoryInstance();
  ApiResourceManager<HidConnectionResource>::GetFactoryInstance();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ApiResourceManager<LogSourceResource>::GetFactoryInstance();
#endif
  ApiResourceManager<ResumableTCPServerSocket>::GetFactoryInstance();
  ApiResourceManager<ResumableTCPSocket>::GetFactoryInstance();
  ApiResourceManager<ResumableUDPSocket>::GetFactoryInstance();
  ApiResourceManager<SerialConnection>::GetFactoryInstance();
  ApiResourceManager<Socket>::GetFactoryInstance();
  ApiResourceManager<UsbDeviceResource>::GetFactoryInstance();
  api::BluetoothSocketEventDispatcher::GetFactoryInstance();
  api::SerialPortManager::GetFactoryInstance();
  api::TCPServerSocketEventDispatcher::GetFactoryInstance();
  api::TCPSocketEventDispatcher::GetFactoryInstance();
  api::UDPSocketEventDispatcher::GetFactoryInstance();
#if BUILDFLAG(IS_CHROMEOS)
  AppFirewallHoleManager::EnsureFactoryBuilt();
#endif
  AudioAPI::GetFactoryInstance();
  BluetoothAPI::GetFactoryInstance();
  BluetoothPrivateAPI::GetFactoryInstance();
#if BUILDFLAG(IS_CHROMEOS)
  ClipboardAPI::GetFactoryInstance();
#endif
  ContentSettingsService::GetFactoryInstance();
  declarative_net_request::RulesMonitorService::GetFactoryInstance();
  FeedbackPrivateAPI::GetFactoryInstance();
  HidDeviceManager::GetFactoryInstance();
  IdleManagerFactory::GetInstance();
  ManagementAPI::GetFactoryInstance();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  MediaPerceptionAPIManager::GetFactoryInstance();
#endif
  MessageService::GetFactoryInstance();
#if BUILDFLAG(ENABLE_EXTENSIONS_LEGACY_IPC)
  MessagingAPIMessageFilter::EnsureAssociatedFactoryBuilt();
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_MAC)
  NetworkingPrivateEventRouterFactory::GetInstance();
#endif
  OffscreenDocumentManager::GetFactory();
  PowerAPI::GetFactoryInstance();
  PrinterProviderAPIFactory::GetInstance();
  RulesRegistryService::GetFactoryInstance();
  RuntimeAPI::GetFactoryInstance();
  SessionStorageManager::GetFactory();
  StorageFrontend::GetFactoryInstance();
  SystemInfoAPI::GetFactoryInstance();
  UsbDeviceManager::GetFactoryInstance();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  VirtualKeyboardAPI::GetFactoryInstance();
  WebcamPrivateAPI::GetFactoryInstance();
#endif
  WebRequestAPI::GetFactoryInstance();
  WebRequestProxyingURLLoaderFactory::EnsureAssociatedFactoryBuilt();
  WebRequestProxyingWebSocket::EnsureAssociatedFactoryBuilt();
}

}  // namespace extensions
