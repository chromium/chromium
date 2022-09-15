// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_NET_ADDRESS_RESOURCE_H_
#define PPAPI_PROXY_NET_ADDRESS_RESOURCE_H_

#include "base/compiler_specific.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/thunk/ppb_net_address_api.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT NetAddressResource : public PluginResource,
                                              public thunk::PPB_NetAddress_API {
 public:
  NetAddressResource(Connection connection,
                     PP_Instance instance,
                     const PP_NetAddress_IPv4& ipv4_addr);
  NetAddressResource(Connection connection,
                     PP_Instance instance,
                     const PP_NetAddress_IPv6& ipv6_addr);
  NetAddressResource(Connection connection,
                     PP_Instance instance,
                     const PP_NetAddress_Private& private_addr);

  NetAddressResource(const NetAddressResource&) = delete;
  NetAddressResource& operator=(const NetAddressResource&) = delete;

  ~NetAddressResource() override;

  // PluginResource implementation.
  thunk::PPB_NetAddress_API* AsPPB_NetAddress_API() override;

  // PPB_NetAddress_API implementation.
  PP_NetAddress_Family GetFamily() override;
  PP_Var DescribeAsString(PP_Bool include_port) override;
  PP_Bool DescribeAsIPv4Address(PP_NetAddress_IPv4* ipv4_addr) override;
  PP_Bool DescribeAsIPv6Address(PP_NetAddress_IPv6* ipv6_addr) override;
  const PP_NetAddress_Private& GetNetAddressPrivate() override;

 private:
  // TODO(yzshen): Refactor the code so that PPB_NetAddress resource doesn't
  // use PP_NetAddress_Private as storage type.
  PP_NetAddress_Private address_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_NET_ADDRESS_RESOURCE_H_
