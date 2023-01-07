// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ppapi/fake_net_address_interface.h"

#include <netinet/in.h>

#include "fake_ppapi/fake_pepper_interface.h"
#include "fake_ppapi/fake_resource_manager.h"
#include "fake_ppapi/fake_var_manager.h"
#include "gtest/gtest.h"

namespace {

class FakeNetAddressResource : public FakeResource {
 public:
  explicit FakeNetAddressResource(PP_NetAddress_IPv4 addr)
      : is_v6(false), address(addr) {}
  explicit FakeNetAddressResource(PP_NetAddress_IPv6 addr)
      : is_v6(true), address_v6(addr) {}

  static const char* classname() { return "FakeNetAddressResource"; }

  bool is_v6;
  PP_NetAddress_IPv4 address;
  PP_NetAddress_IPv6 address_v6;
};

}  // namespace

FakeNetAddressInterface::FakeNetAddressInterface(FakePepperInterface* ppapi)
    : ppapi_(ppapi) {}

PP_Resource FakeNetAddressInterface::CreateFromIPv4Address(
    PP_Instance instance,
    PP_NetAddress_IPv4* address) {
  if (instance != ppapi_->GetInstance())
    return 0;

  FakeNetAddressResource* addr_resource = new FakeNetAddressResource(*address);

  PP_Resource rtn = CREATE_RESOURCE(ppapi_->resource_manager(),
                                    FakeNetAddressResource,
                                    addr_resource);
  return rtn;
}

PP_Resource FakeNetAddressInterface::CreateFromIPv6Address(
    PP_Instance instance,
    PP_NetAddress_IPv6* address) {
  if (instance != ppapi_->GetInstance())
    return 0;

  FakeNetAddressResource* addr_resource = new FakeNetAddressResource(*address);
  PP_Resource rtn = CREATE_RESOURCE(ppapi_->resource_manager(),
                                    FakeNetAddressResource,
                                    addr_resource);
  return rtn;
}

PP_Bool FakeNetAddressInterface::IsNetAddress(PP_Resource address) {
  FakeNetAddressResource* address_resource =
      ppapi_->resource_manager()->Get<FakeNetAddressResource>(address);
  if (address_resource == NULL)
    return PP_FALSE;
  return PP_TRUE;
}

PP_NetAddress_Family FakeNetAddressInterface::GetFamily(PP_Resource address) {
  FakeNetAddressResource* address_resource =
      ppapi_->resource_manager()->Get<FakeNetAddressResource>(address);
  if (address_resource == NULL)
    return PP_NETADDRESS_FAMILY_UNSPECIFIED;

  if (address_resource->is_v6)
    return PP_NETADDRESS_FAMILY_IPV6;

  return PP_NETADDRESS_FAMILY_IPV4;
}

PP_Bool FakeNetAddressInterface::DescribeAsIPv4Address(
    PP_Resource address, PP_NetAddress_IPv4* target) {
  FakeNetAddressResource* address_resource =
      ppapi_->resource_manager()->Get<FakeNetAddressResource>(address);
  if (address_resource == NULL)
    return PP_FALSE;

  if (address_resource->is_v6)
    return PP_FALSE;

  *target = address_resource->address;
  return PP_TRUE;
}

PP_Bool FakeNetAddressInterface::DescribeAsIPv6Address(
    PP_Resource address, PP_NetAddress_IPv6* target) {
  FakeNetAddressResource* address_resource =
      ppapi_->resource_manager()->Get<FakeNetAddressResource>(address);
  if (address_resource == NULL)
    return PP_FALSE;

  if (!address_resource->is_v6)
    return PP_FALSE;

  *target = address_resource->address_v6;
  return PP_TRUE;
}

PP_Var FakeNetAddressInterface::DescribeAsString(PP_Resource, PP_Bool) {
  // TODO(sbc): implement
  assert(false);
  return PP_Var();
}
