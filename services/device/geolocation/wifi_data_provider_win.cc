// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/device/geolocation/wifi_data_provider_win.h"

#include <windows.h>

#include <winioctl.h>
#include <wlanapi.h>

#include "base/logging.h"
#include "base/win/win_util.h"
#include "services/device/geolocation/wifi_data_provider_common.h"
#include "services/device/geolocation/wifi_data_provider_common_win.h"
#include "services/device/geolocation/wifi_data_provider_handle.h"
#include "services/device/public/mojom/geolocation_internals.mojom.h"

namespace device {

namespace {

static const int kDefaultPollingIntervalMs = 10 * 1000;           // 10s
static const int kNoChangePollingIntervalMs = 2 * 60 * 1000;      // 2 mins
static const int kTwoNoChangePollingIntervalMs = 10 * 60 * 1000;  // 10 mins
static const int kNoWifiPollingIntervalMs = 20 * 1000;            // 20s

// WlanOpenHandle
typedef DWORD(WINAPI* WlanOpenHandleFunction)(DWORD dwClientVersion,
                                              PVOID pReserved,
                                              PDWORD pdwNegotiatedVersion,
                                              PHANDLE phClientHandle);

// WlanEnumInterfaces
typedef DWORD(WINAPI* WlanEnumInterfacesFunction)(
    HANDLE hClientHandle,
    PVOID pReserved,
    PWLAN_INTERFACE_INFO_LIST* ppInterfaceList);

// WlanGetNetworkBssList
typedef DWORD(WINAPI* WlanGetNetworkBssListFunction)(
    HANDLE hClientHandle,
    const GUID* pInterfaceGuid,
    const PDOT11_SSID pDot11Ssid,
    DOT11_BSS_TYPE dot11BssType,
    BOOL bSecurityEnabled,
    PVOID pReserved,
    PWLAN_BSS_LIST* ppWlanBssList);

// WlanFreeMemory
typedef VOID(WINAPI* WlanFreeMemoryFunction)(PVOID pMemory);

// WlanCloseHandle
typedef DWORD(WINAPI* WlanCloseHandleFunction)(HANDLE hClientHandle,
                                               PVOID pReserved);

// Extracts data for an access point and converts to AccessPointData.
mojom::AccessPointData GetNetworkData(const WLAN_BSS_ENTRY& bss_entry) {
  mojom::AccessPointData access_point_data;
  // Currently we get only MAC address and signal strength.
  access_point_data.mac_address = MacAddressAsString(bss_entry.dot11Bssid);
  access_point_data.radio_signal_strength = bss_entry.lRssi;

  // TODO(steveblock): Is it possible to get the following?
  // access_point_data.signal_to_noise
  // access_point_data.age
  // access_point_data.channel
  return access_point_data;
}

// This class encapsulates loading and interacting with wlan_api.dll, which can
// not be loaded statically because it's not available in Server 2008 R2, where
// it must be installed explicitly by the user if and when they wants to use the
// Wireless interface.
// https://www.bonusbits.com/wiki/KB:Wlanapi.dll_missing_on_Windows_Server_2008_R2
class WindowsWlanApi : public WifiDataProviderCommon::WlanApiInterface {
 public:
  static std::unique_ptr<WindowsWlanApi> Create();

  // Takes ownership of the library handle.
  explicit WindowsWlanApi(HINSTANCE library);
  ~WindowsWlanApi() override;

  // WlanApiInterface implementation
  bool GetAccessPointData(WifiData::AccessPointDataSet* data) override;

 private:
  bool GetInterfaceDataWLAN(HANDLE wlan_handle,
                            const GUID& interface_id,
                            WifiData::AccessPointDataSet* data);
  // Handle to the wlanapi.dll library.
  HINSTANCE library_;

  // Function pointers for WLAN
  WlanOpenHandleFunction WlanOpenHandle_function_;
  WlanEnumInterfacesFunction WlanEnumInterfaces_function_;
  WlanGetNetworkBssListFunction WlanGetNetworkBssList_function_;
  WlanFreeMemoryFunction WlanFreeMemory_function_;
  WlanCloseHandleFunction WlanCloseHandle_function_;
};

// static
std::unique_ptr<WindowsWlanApi> WindowsWlanApi::Create() {
  // Use an absolute path to load the DLL to avoid DLL preloading attacks.
  auto path =
      base::win::ExpandEnvironmentVariables(L"%WINDIR%\\system32\\wlanapi.dll");
  if (!path) {
    return nullptr;
  }

  HINSTANCE library =
      LoadLibraryEx(path->c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
  if (!library) {
    return nullptr;
  }

  return std::make_unique<WindowsWlanApi>(library);
}

WindowsWlanApi::WindowsWlanApi(HINSTANCE library) : library_(library) {
  DCHECK(library_);
  // Extract all methods from |library_|.
  WlanOpenHandle_function_ = reinterpret_cast<WlanOpenHandleFunction>(
      GetProcAddress(library_, "WlanOpenHandle"));
  WlanEnumInterfaces_function_ = reinterpret_cast<WlanEnumInterfacesFunction>(
      GetProcAddress(library_, "WlanEnumInterfaces"));
  WlanGetNetworkBssList_function_ =
      reinterpret_cast<WlanGetNetworkBssListFunction>(
          GetProcAddress(library_, "WlanGetNetworkBssList"));
  WlanFreeMemory_function_ = reinterpret_cast<WlanFreeMemoryFunction>(
      GetProcAddress(library_, "WlanFreeMemory"));
  WlanCloseHandle_function_ = reinterpret_cast<WlanCloseHandleFunction>(
      GetProcAddress(library_, "WlanCloseHandle"));

  DCHECK(WlanOpenHandle_function_ && WlanEnumInterfaces_function_ &&
         WlanGetNetworkBssList_function_ && WlanFreeMemory_function_ &&
         WlanCloseHandle_function_);
}

WindowsWlanApi::~WindowsWlanApi() {
  FreeLibrary(library_);
}

bool WindowsWlanApi::GetAccessPointData(WifiData::AccessPointDataSet* data) {
  DCHECK(data);

  DWORD negotiated_version;
  HANDLE wlan_handle = nullptr;
  // Highest WLAN API version supported by the client; pass the lowest. It seems
  // that the negotiated version is the Vista version (the highest) irrespective
  // of what we pass!
  static const int kXpWlanClientVersion = 1;
  if ((*WlanOpenHandle_function_)(kXpWlanClientVersion, NULL,
                                  &negotiated_version,
                                  &wlan_handle) != ERROR_SUCCESS) {
    return false;
  }
  DCHECK(wlan_handle);

  // Get the list of interfaces. WlanEnumInterfaces allocates |interface_list|.
  WLAN_INTERFACE_INFO_LIST* interface_list = nullptr;
  if ((*WlanEnumInterfaces_function_)(wlan_handle, NULL, &interface_list) !=
      ERROR_SUCCESS) {
    return false;
  }
  DCHECK(interface_list);

  // Go through the list of interfaces and get the data for each.
  for (size_t i = 0; i < interface_list->dwNumberOfItems; ++i) {
    const WLAN_INTERFACE_INFO interface_info = interface_list->InterfaceInfo[i];

    // Skip any interface that is midway through association; the
    // WlanGetNetworkBssList function call is known to hang indefinitely
    // when it's in this state. https://crbug.com/39300
    if (interface_info.isState == wlan_interface_state_associating) {
      DLOG(WARNING) << "Skipping wifi scan on adapter " << i << " ("
                    << interface_info.strInterfaceDescription
                    << ") in 'associating' state. Repeated occurrences "
                       "indicates a non-responding adapter.";
      continue;
    }
    GetInterfaceDataWLAN(wlan_handle, interface_info.InterfaceGuid, data);
  }

  (*WlanFreeMemory_function_)(interface_list);

  return (*WlanCloseHandle_function_)(wlan_handle, NULL) == ERROR_SUCCESS;
}

// Appends the data for a single interface to |data|. Returns false for error.
bool WindowsWlanApi::GetInterfaceDataWLAN(const HANDLE wlan_handle,
                                          const GUID& interface_id,
                                          WifiData::AccessPointDataSet* data) {
  // WlanGetNetworkBssList allocates |bss_list|.
  WLAN_BSS_LIST* bss_list = nullptr;
  if ((*WlanGetNetworkBssList_function_)(wlan_handle, &interface_id,
                                         NULL,  // Use all SSIDs.
                                         dot11_BSS_type_any,
                                         false,  // bSecurityEnabled - unused
                                         NULL,   // reserved
                                         &bss_list) != ERROR_SUCCESS) {
    return false;
  }
  // WlanGetNetworkBssList() can return success without filling |bss_list|.
  if (!bss_list)
    return false;

  for (size_t i = 0; i < bss_list->dwNumberOfItems; ++i)
    data->insert(GetNetworkData(bss_list->wlanBssEntries[i]));

  (*WlanFreeMemory_function_)(bss_list);

  return true;
}

}  // anonymous namespace

WifiDataProvider* WifiDataProviderHandle::DefaultFactoryFunction() {
  return new WifiDataProviderWin();
}

WifiDataProviderWin::WifiDataProviderWin() = default;

WifiDataProviderWin::~WifiDataProviderWin() = default;

std::unique_ptr<WifiDataProviderCommon::WlanApiInterface>
WifiDataProviderWin::CreateWlanApi() {
  return WindowsWlanApi::Create();
}

std::unique_ptr<WifiPollingPolicy> WifiDataProviderWin::CreatePollingPolicy() {
  return std::make_unique<GenericWifiPollingPolicy<
      kDefaultPollingIntervalMs, kNoChangePollingIntervalMs,
      kTwoNoChangePollingIntervalMs, kNoWifiPollingIntervalMs>>();
}

}  // namespace device
