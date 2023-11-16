// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SYSTEM_NETWORK_SYSTEM_NETWORK_API_H_
#define EXTENSIONS_BROWSER_API_SYSTEM_NETWORK_SYSTEM_NETWORK_API_H_

#include "extensions/browser/extension_function.h"
#include "net/base/network_interfaces.h"

namespace extensions::api {

class SystemNetworkGetNetworkInterfacesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.network.getNetworkInterfaces",
                             SYSTEM_NETWORK_GETNETWORKINTERFACES)

  SystemNetworkGetNetworkInterfacesFunction() = default;

 protected:
  ~SystemNetworkGetNetworkInterfacesFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void SendResponseOnUIThread(
      const std::optional<net::NetworkInterfaceList>& interface_list);
};

}  // namespace extensions::api

#endif  // EXTENSIONS_BROWSER_API_SYSTEM_NETWORK_SYSTEM_NETWORK_API_H_
