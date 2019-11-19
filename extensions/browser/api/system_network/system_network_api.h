// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SYSTEM_NETWORK_SYSTEM_NETWORK_API_H_
#define EXTENSIONS_BROWSER_API_SYSTEM_NETWORK_SYSTEM_NETWORK_API_H_

#include <memory>

#include "extensions/browser/extension_function.h"
#include "extensions/common/api/system_network.h"
#include "net/base/network_interfaces.h"

namespace extensions {
namespace api {

class SystemNetworkGetNetworkInterfacesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.network.getNetworkInterfaces",
                             SYSTEM_NETWORK_GETNETWORKINTERFACES)

  SystemNetworkGetNetworkInterfacesFunction();

 protected:
  ~SystemNetworkGetNetworkInterfacesFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void SendResponseOnUIThread(
      const base::Optional<net::NetworkInterfaceList>& interface_list);
};

}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SYSTEM_NETWORK_SYSTEM_NETWORK_API_H_
