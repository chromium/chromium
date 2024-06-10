// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/base/network_interfaces_win.h"

#include <algorithm>
#include <memory>
#include <string_view>

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/win/scoped_handle.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace net {

namespace {

// Converts Windows defined types to NetworkInterfaceType.
NetworkChangeNotifier::ConnectionType GetNetworkInterfaceType(DWORD ifType) {
  NetworkChangeNotifier::ConnectionType type =
      NetworkChangeNotifier::CONNECTION_UNKNOWN;
  if (ifType == IF_TYPE_ETHERNET_CSMACD) {
    type = NetworkChangeNotifier::CONNECTION_ETHERNET;
  } else if (ifType == IF_TYPE_IEEE80211) {
    type = NetworkChangeNotifier::CONNECTION_WIFI;
  }
  // TODO(mallinath) - Cellular?
  return type;
}

// Returns scoped_ptr to WLAN_CONNECTION_ATTRIBUTES. The scoped_ptr may hold a
// NULL pointer if WLAN_CONNECTION_ATTRIBUTES is unavailable.
std::unique_ptr<WLAN_CONNECTION_ATTRIBUTES, internal::WlanApiDeleter>
GetConnectionAttributes() {
  const internal::WlanApi& wlanapi = internal::WlanApi::GetInstance();
  std::unique_ptr<WLAN_CONNECTION_ATTRIBUTES, internal::WlanApiDeleter>
      wlan_connection_attributes;
  if (!wlanapi.initialized)
    return wlan_connection_attributes;

  internal::WlanHandle client;
  DWORD cur_version = 0;
  const DWORD kMaxClientVersion = 2;
  DWORD result = wlanapi.OpenHandle(kMaxClientVersion, &cur_version, &client);
  if (result != ERROR_SUCCESS)
    return wlan_connection_attributes;

  WLAN_INTERFACE_INFO_LIST* interface_list_ptr = nullptr;
  result =
      wlanapi.enum_interfaces_func(client.Get(), nullptr, &interface_list_ptr);
  if (result != ERROR_SUCCESS)
    return wlan_connection_attributes;
  std::unique_ptr<WLAN_INTERFACE_INFO_LIST, internal::WlanApiDeleter>
      interface_list(interface_list_ptr);

  // Assume at most one connected wifi interface.
  WLAN_INTERFACE_INFO* info = nullptr;
  for (unsigned i = 0; i < interface_list->dwNumberOfItems; ++i) {
    if (interface_list->InterfaceInfo[i].isState ==
        wlan_interface_state_connected) {
      info = &interface_list->InterfaceInfo[i];
      break;
    }
  }

  if (info == nullptr)
    return wlan_connection_attributes;

  WLAN_CONNECTION_ATTRIBUTES* conn_info_ptr = nullptr;
  DWORD conn_info_size = 0;
  WLAN_OPCODE_VALUE_TYPE op_code;
  result = wlanapi.query_interface_func(
      client.Get(), &info->InterfaceGuid, wlan_intf_opcode_current_connection,
      nullptr, &conn_info_size, reinterpret_cast<VOID**>(&conn_info_ptr),
      &op_code);
  wlan_connection_attributes.reset(conn_info_ptr);
  if (result == ERROR_SUCCESS)
    DCHECK(conn_info_ptr);
  else
    wlan_connection_attributes.reset();
  return wlan_connection_attributes;
}

}  // namespace

namespace internal {

base::LazyInstance<WlanApi>::Leaky lazy_wlanapi =
  LAZY_INSTANCE_INITIALIZER;

WlanApi& WlanApi::GetInstance() {
  return lazy_wlanapi.Get();
}

WlanApi::WlanApi() : initialized(false) {
  // Mitigate the issues caused by loading DLLs on a background thread
  // (http://crbug/973868).
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

  HMODULE module =
      ::LoadLibraryEx(L"wlanapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!module)
    return;

  open_handle_func = reinterpret_cast<WlanOpenHandleFunc>(
      ::GetProcAddress(module, "WlanOpenHandle"));
  enum_interfaces_func = reinterpret_cast<WlanEnumInterfacesFunc>(
      ::GetProcAddress(module, "WlanEnumInterfaces"));
  query_interface_func = reinterpret_cast<WlanQueryInterfaceFunc>(
      ::GetProcAddress(module, "WlanQueryInterface"));
  set_interface_func = reinterpret_cast<WlanSetInterfaceFunc>(
      ::GetProcAddress(module, "WlanSetInterface"));
  free_memory_func = reinterpret_cast<WlanFreeMemoryFunc>(
      ::GetProcAddress(module, "WlanFreeMemory"));
  close_handle_func = reinterpret_cast<WlanCloseHandleFunc>(
      ::GetProcAddress(module, "WlanCloseHandle"));
  initialized = open_handle_func && enum_interfaces_func &&
      query_interface_func && set_interface_func &&
      free_memory_func && close_handle_func;
}

bool GetNetworkListImpl(NetworkInterfaceList* networks,
                        int policy,
                        const IP_ADAPTER_ADDRESSES* adapters) {
  for (const IP_ADAPTER_ADDRESSES* adapter = adapters; adapter != nullptr;
       adapter = adapter->Next) {
    // Ignore the loopback device.
    if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
      continue;
    }

    if (adapter->OperStatus != IfOperStatusUp) {
      continue;
    }

    // Ignore any HOST side vmware adapters with a description like:
    // VMware Virtual Ethernet Adapter for VMnet1
    // but don't ignore any GUEST side adapters with a description like:
    // VMware Accelerated AMD PCNet Adapter #2
    if ((policy & EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES) &&
        strstr(adapter->AdapterName, "VMnet") != nullptr) {
      continue;
    }

    std::optional<Eui48MacAddress> mac_address;
    mac_address.emplace();
    if (adapter->PhysicalAddressLength == mac_address->size()) {
      std::copy_n(reinterpret_cast<const uint8_t*>(adapter->PhysicalAddress),
                  mac_address->size(), mac_address->begin());
    } else {
      mac_address.reset();
    }

    for (IP_ADAPTER_UNICAST_ADDRESS* address = adapter->FirstUnicastAddress;
         address; address = address->Next) {
      int family = address->Address.lpSockaddr->sa_family;
      if (family == AF_INET || family == AF_INET6) {
        IPEndPoint endpoint;
        if (endpoint.FromSockAddr(address->Address.lpSockaddr,
                                  address->Address.iSockaddrLength)) {
          size_t prefix_length = address->OnLinkPrefixLength;

          // If the duplicate address detection (DAD) state is not changed to
          // Preferred, skip this address.
          if (address->DadState != IpDadStatePreferred) {
            continue;
          }

          uint32_t index =
              (family == AF_INET) ? adapter->IfIndex : adapter->Ipv6IfIndex;

          // From http://technet.microsoft.com/en-us/ff568768(v=vs.60).aspx, the
          // way to identify a temporary IPv6 Address is to check if
          // PrefixOrigin is equal to IpPrefixOriginRouterAdvertisement and
          // SuffixOrigin equal to IpSuffixOriginRandom.
          int ip_address_attributes = IP_ADDRESS_ATTRIBUTE_NONE;
          if (family == AF_INET6) {
            if (address->PrefixOrigin == IpPrefixOriginRouterAdvertisement &&
                address->SuffixOrigin == IpSuffixOriginRandom) {
              ip_address_attributes |= IP_ADDRESS_ATTRIBUTE_TEMPORARY;
            }
            if (address->PreferredLifetime == 0) {
              ip_address_attributes |= IP_ADDRESS_ATTRIBUTE_DEPRECATED;
            }
          }
          networks->push_back(NetworkInterface(
              adapter->AdapterName,
              base::SysWideToNativeMB(adapter->FriendlyName), index,
              GetNetworkInterfaceType(adapter->IfType), endpoint.address(),
              prefix_length, ip_address_attributes, mac_address));
        }
      }
    }
  }
  return true;
}

}  // namespace internal

bool GetNetworkList(NetworkInterfaceList* networks, int policy) {
  // Max number of times to retry GetAdaptersAddresses due to
  // ERROR_BUFFER_OVERFLOW. If GetAdaptersAddresses returns this indefinitely
  // due to an unforseen reason, we don't want to be stuck in an endless loop.
  static constexpr int MAX_GETADAPTERSADDRESSES_TRIES = 10;
  // Use an initial buffer size of 15KB, as recommended by MSDN. See:
  // https://msdn.microsoft.com/en-us/library/windows/desktop/aa365915(v=vs.85).aspx
  static constexpr int INITIAL_BUFFER_SIZE = 15000;

  ULONG len = INITIAL_BUFFER_SIZE;
  ULONG flags = 0;
  // Initial buffer allocated on stack.
  char initial_buf[INITIAL_BUFFER_SIZE];
  // Dynamic buffer in case initial buffer isn't large enough.
  std::unique_ptr<char[]> buf;

  IP_ADAPTER_ADDRESSES* adapters = nullptr;
  {
    // GetAdaptersAddresses() may require IO operations.
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(&initial_buf);
    ULONG result =
        GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, adapters, &len);

    // If we get ERROR_BUFFER_OVERFLOW, call GetAdaptersAddresses in a loop,
    // because the required size may increase between successive calls,
    // resulting in ERROR_BUFFER_OVERFLOW multiple times.
    for (int tries = 1; result == ERROR_BUFFER_OVERFLOW &&
                        tries < MAX_GETADAPTERSADDRESSES_TRIES;
         ++tries) {
      buf = std::make_unique<char[]>(len);
      adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.get());
      result = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, adapters, &len);
    }

    if (result == ERROR_NO_DATA) {
      // There are 0 networks.
      return true;
    } else if (result != NO_ERROR) {
      LOG(ERROR) << "GetAdaptersAddresses failed: " << result;
      return false;
    }
  }

  return internal::GetNetworkListImpl(networks, policy, adapters);
}

// Note: There is no need to explicitly set the options back
// as the OS will automatically set them back when the WlanHandle
// is closed.
class WifiOptionSetter : public ScopedWifiOptions {
 public:
  WifiOptionSetter(int options) {
    const internal::WlanApi& wlanapi = internal::WlanApi::GetInstance();
    if (!wlanapi.initialized)
      return;

    DWORD cur_version = 0;
    const DWORD kMaxClientVersion = 2;
    DWORD result = wlanapi.OpenHandle(
        kMaxClientVersion, &cur_version, &client_);
    if (result != ERROR_SUCCESS)
      return;

    WLAN_INTERFACE_INFO_LIST* interface_list_ptr = nullptr;
    result = wlanapi.enum_interfaces_func(client_.Get(), nullptr,
                                          &interface_list_ptr);
    if (result != ERROR_SUCCESS)
      return;
    std::unique_ptr<WLAN_INTERFACE_INFO_LIST, internal::WlanApiDeleter>
        interface_list(interface_list_ptr);

    for (unsigned i = 0; i < interface_list->dwNumberOfItems; ++i) {
      WLAN_INTERFACE_INFO* info = &interface_list->InterfaceInfo[i];
      if (options & WIFI_OPTIONS_DISABLE_SCAN) {
        BOOL data = false;
        wlanapi.set_interface_func(client_.Get(), &info->InterfaceGuid,
                                   wlan_intf_opcode_background_scan_enabled,
                                   sizeof(data), &data, nullptr);
      }
      if (options & WIFI_OPTIONS_MEDIA_STREAMING_MODE) {
        BOOL data = true;
        wlanapi.set_interface_func(client_.Get(), &info->InterfaceGuid,
                                   wlan_intf_opcode_media_streaming_mode,
                                   sizeof(data), &data, nullptr);
      }
    }
  }

 private:
  internal::WlanHandle client_;
};

std::unique_ptr<ScopedWifiOptions> SetWifiOptions(int options) {
  return std::make_unique<WifiOptionSetter>(options);
}

std::string GetWifiSSID() {
  auto conn_info = GetConnectionAttributes();

  if (!conn_info.get())
    return "";

  const DOT11_SSID dot11_ssid = conn_info->wlanAssociationAttributes.dot11Ssid;
  return std::string(reinterpret_cast<const char*>(dot11_ssid.ucSSID),
                     dot11_ssid.uSSIDLength);
}

}  // namespace net
