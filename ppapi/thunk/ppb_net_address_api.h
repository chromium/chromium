// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_NET_ADDRESS_API_H_
#define PPAPI_THUNK_PPB_NET_ADDRESS_API_H_

#include "ppapi/c/ppb_net_address.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

struct PP_NetAddress_Private;

namespace ppapi {
namespace thunk {

class PPAPI_THUNK_EXPORT PPB_NetAddress_API {
 public:
  virtual ~PPB_NetAddress_API() {}

  virtual PP_NetAddress_Family GetFamily() = 0;
  virtual PP_Var DescribeAsString(PP_Bool include_port) = 0;
  virtual PP_Bool DescribeAsIPv4Address(PP_NetAddress_IPv4* ipv4_addr) = 0;
  virtual PP_Bool DescribeAsIPv6Address(PP_NetAddress_IPv6* ipv6_addr) = 0;

  virtual const PP_NetAddress_Private& GetNetAddressPrivate() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_NET_ADDRESS_API_H_
