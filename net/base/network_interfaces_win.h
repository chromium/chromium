// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_INTERFACES_WIN_H_
#define NET_BASE_NETWORK_INTERFACES_WIN_H_

// This file is only used to expose some of the internals
// of network_interfaces_win.cc to tests.

#include <winsock2.h>

#include <iphlpapi.h>
#include <wlanapi.h>

#include "base/win/scoped_handle.h"
#include "net/base/net_export.h"
#include "net/base/network_interfaces.h"

namespace net::internal {

struct NET_EXPORT WlanApi {
  typedef DWORD (WINAPI *WlanOpenHandleFunc)(
      DWORD, VOID*, DWORD*, HANDLE*);
  typedef DWORD (WINAPI *WlanEnumInterfacesFunc)(
      HANDLE, VOID*, WLAN_INTERFACE_INFO_LIST**);
  typedef DWORD (WINAPI *WlanQueryInterfaceFunc)(
      HANDLE, const GUID*, WLAN_INTF_OPCODE, VOID*, DWORD*, VOID**,
      WLAN_OPCODE_VALUE_TYPE*);
  typedef DWORD (WINAPI *WlanSetInterfaceFunc)(
      HANDLE, const GUID*, WLAN_INTF_OPCODE, DWORD, const VOID*, VOID*);
  typedef VOID (WINAPI *WlanFreeMemoryFunc)(VOID*);
  typedef DWORD (WINAPI *WlanCloseHandleFunc)(HANDLE, VOID*);

  WlanApi();
  static WlanApi& GetInstance();

  template <typename T>
  DWORD OpenHandle(DWORD client_version, DWORD* cur_version, T* handle) const {
    HANDLE temp_handle;
    DWORD result =
        open_handle_func(client_version, nullptr, cur_version, &temp_handle);
    if (result != ERROR_SUCCESS)
      return result;
    handle->Set(temp_handle);
    return ERROR_SUCCESS;
  }

  WlanOpenHandleFunc open_handle_func;
  WlanEnumInterfacesFunc enum_interfaces_func;
  WlanQueryInterfaceFunc query_interface_func;
  WlanSetInterfaceFunc set_interface_func;
  WlanFreeMemoryFunc free_memory_func;
  WlanCloseHandleFunc close_handle_func;
  bool initialized;
};

struct WlanApiHandleTraits {
  typedef HANDLE Handle;

  static bool CloseHandle(HANDLE handle) {
    return WlanApi::GetInstance().close_handle_func(handle, nullptr) ==
           ERROR_SUCCESS;
  }
  static bool IsHandleValid(HANDLE handle) {
    return base::win::HandleTraits::IsHandleValid(handle);
  }
  static HANDLE NullHandle() {
    return base::win::HandleTraits::NullHandle();
  }
};

typedef base::win::GenericScopedHandle<
  WlanApiHandleTraits,
  base::win::DummyVerifierTraits> WlanHandle;

struct WlanApiDeleter {
  inline void operator()(void* ptr) const {
    WlanApi::GetInstance().free_memory_func(ptr);
  }
};

NET_EXPORT bool GetNetworkListImpl(
    NetworkInterfaceList* networks,
    int policy,
    const IP_ADAPTER_ADDRESSES* ip_adapter_addresses);

}  // namespace net::internal

#endif   // NET_BASE_NETWORK_INTERFACES_WIN_H_
