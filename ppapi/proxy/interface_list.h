// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_INTERFACE_LIST_H_
#define PPAPI_PROXY_INTERFACE_LIST_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include "base/synchronization/lock.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT InterfaceList {
 public:
  InterfaceList();

  InterfaceList(const InterfaceList&) = delete;
  InterfaceList& operator=(const InterfaceList&) = delete;

  ~InterfaceList();

  static InterfaceList* GetInstance();

  // Sets the permissions that the interface list will use to compute
  // whether an interface is available to the current process. By default,
  // this will be "no permissions", which will give only access to public
  // stable interfaces via GetInterface.
  //
  // IMPORTANT: This is not a security boundary. Malicious plugins can bypass
  // this check since they run in the same address space as this code in the
  // plugin process. A real security check is required for all IPC messages.
  // This check just allows us to return NULL for interfaces you "shouldn't" be
  // using to keep honest plugins honest.
  static void SetProcessGlobalPermissions(const PpapiPermissions& permissions);

  // Looks up the factory function for the given ID. Returns NULL if not
  // supported.
  InterfaceProxy::Factory GetFactoryForID(ApiID id) const;

  // Returns the interface pointer for the given browser or plugin interface,
  // or NULL if it's not supported.
  const void* GetInterfaceForPPB(const std::string& name);
  const void* GetInterfaceForPPP(const std::string& name);

 private:
  friend class InterfaceListTest;

  class InterfaceInfo {
   public:
    InterfaceInfo(const void* in_interface, Permission in_perm)
        : iface_(in_interface),
          required_permission_(in_perm),
          sent_to_uma_(false) {
    }

    InterfaceInfo(const InterfaceInfo&) = delete;
    InterfaceInfo& operator=(const InterfaceInfo&) = delete;

    const void* iface() { return iface_; }

    // Permission required to return non-null for this interface. This will
    // be checked with the value set via SetProcessGlobalPermissionBits when
    // an interface is requested.
    Permission required_permission() { return required_permission_; }

    // Call this any time the interface is requested. It will log a UMA count
    // only the first time. This is safe to call from any thread, regardless of
    // whether the proxy lock is held.
    void LogWithUmaOnce(const std::string& name);

   private:
    const void* const iface_;
    const Permission required_permission_;

    bool sent_to_uma_;
    base::Lock sent_to_uma_lock_;
  };
  // Give friendship for HashInterfaceName.
  friend class InterfaceInfo;

  using NameToInterfaceInfoMap =
      std::unordered_map<std::string, std::unique_ptr<InterfaceInfo>>;

  void AddProxy(ApiID id, InterfaceProxy::Factory factory);

  // Permissions is the type of permission required to access the corresponding
  // interface. Currently this must be just one unique permission (rather than
  // a bitfield).
  void AddPPB(const char* name, const void* iface, Permission permission);
  void AddPPP(const char* name, const void* iface);

  // Hash the interface name for UMA logging.
  static int HashInterfaceName(const std::string& name);

  PpapiPermissions permissions_;

  NameToInterfaceInfoMap name_to_browser_info_;
  NameToInterfaceInfoMap name_to_plugin_info_;

  InterfaceProxy::Factory id_to_factory_[API_ID_COUNT];
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_INTERFACE_LIST_H_
