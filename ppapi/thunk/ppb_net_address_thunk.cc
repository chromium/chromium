// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_net_address.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/ppb_net_address_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource CreateFromIPv4Address(
    PP_Instance instance,
    const struct PP_NetAddress_IPv4* ipv4_addr) {
  VLOG(4) << "PPB_NetAddress::CreateFromIPv4Address()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateNetAddressFromIPv4Address(instance,
                                                            ipv4_addr);
}

PP_Resource CreateFromIPv6Address(
    PP_Instance instance,
    const struct PP_NetAddress_IPv6* ipv6_addr) {
  VLOG(4) << "PPB_NetAddress::CreateFromIPv6Address()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateNetAddressFromIPv6Address(instance,
                                                            ipv6_addr);
}

PP_Bool IsNetAddress(PP_Resource resource) {
  VLOG(4) << "PPB_NetAddress::IsNetAddress()";
  EnterResource<PPB_NetAddress_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

PP_NetAddress_Family GetFamily(PP_Resource addr) {
  VLOG(4) << "PPB_NetAddress::GetFamily()";
  EnterResource<PPB_NetAddress_API> enter(addr, true);
  if (enter.failed())
    return PP_NETADDRESS_FAMILY_UNSPECIFIED;
  return enter.object()->GetFamily();
}

struct PP_Var DescribeAsString(PP_Resource addr, PP_Bool include_port) {
  VLOG(4) << "PPB_NetAddress::DescribeAsString()";
  EnterResource<PPB_NetAddress_API> enter(addr, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->DescribeAsString(include_port);
}

PP_Bool DescribeAsIPv4Address(PP_Resource addr,
                              struct PP_NetAddress_IPv4* ipv4_addr) {
  VLOG(4) << "PPB_NetAddress::DescribeAsIPv4Address()";
  EnterResource<PPB_NetAddress_API> enter(addr, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->DescribeAsIPv4Address(ipv4_addr);
}

PP_Bool DescribeAsIPv6Address(PP_Resource addr,
                              struct PP_NetAddress_IPv6* ipv6_addr) {
  VLOG(4) << "PPB_NetAddress::DescribeAsIPv6Address()";
  EnterResource<PPB_NetAddress_API> enter(addr, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->DescribeAsIPv6Address(ipv6_addr);
}

const PPB_NetAddress_1_0 g_ppb_netaddress_thunk_1_0 = {
  &CreateFromIPv4Address,
  &CreateFromIPv6Address,
  &IsNetAddress,
  &GetFamily,
  &DescribeAsString,
  &DescribeAsIPv4Address,
  &DescribeAsIPv6Address
};

}  // namespace

const PPB_NetAddress_1_0* GetPPB_NetAddress_1_0_Thunk() {
  return &g_ppb_netaddress_thunk_1_0;
}

}  // namespace thunk
}  // namespace ppapi
