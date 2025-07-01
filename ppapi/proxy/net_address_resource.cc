// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/net_address_resource.h"

#include <string>

#include "ppapi/c/pp_bool.h"
#include "ppapi/shared_impl/private/net_address_private_impl.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {
namespace proxy {

NetAddressResource::NetAddressResource(
    Connection connection,
    PP_Instance instance,
    const PP_NetAddress_IPv4& ipv4_addr)
    : PluginResource(connection, instance) {
  NetAddressPrivateImpl::CreateNetAddressPrivateFromIPv4Address(ipv4_addr,
                                                                &address_);
}

NetAddressResource::NetAddressResource(
    Connection connection,
    PP_Instance instance,
    const PP_NetAddress_IPv6& ipv6_addr)
    : PluginResource(connection, instance) {
  NetAddressPrivateImpl::CreateNetAddressPrivateFromIPv6Address(ipv6_addr,
                                                                &address_);
}

NetAddressResource::NetAddressResource(
    Connection connection,
    PP_Instance instance,
    const PP_NetAddress_Private& private_addr)
    : PluginResource(connection, instance) {
  address_ = private_addr;
}

NetAddressResource::~NetAddressResource() {
}

thunk::PPB_NetAddress_API* NetAddressResource::AsPPB_NetAddress_API() {
  return this;
}

PP_NetAddress_Family NetAddressResource::GetFamily() {
  return NetAddressPrivateImpl::GetFamilyFromNetAddressPrivate(address_);
}

PP_Var NetAddressResource::DescribeAsString(PP_Bool include_port) {
  std::string description = NetAddressPrivateImpl::DescribeNetAddress(
      address_, PP_ToBool(include_port));

  if (description.empty())
    return PP_MakeUndefined();
  return StringVar::StringToPPVar(description);
}

PP_Bool NetAddressResource::DescribeAsIPv4Address(
    PP_NetAddress_IPv4* ipv4_addr) {
  return PP_FromBool(
      NetAddressPrivateImpl::DescribeNetAddressPrivateAsIPv4Address(
          address_, ipv4_addr));
}

PP_Bool NetAddressResource::DescribeAsIPv6Address(
    PP_NetAddress_IPv6* ipv6_addr) {
  return PP_FromBool(
      NetAddressPrivateImpl::DescribeNetAddressPrivateAsIPv6Address(
          address_, ipv6_addr));
}

const PP_NetAddress_Private& NetAddressResource::GetNetAddressPrivate() {
  return address_;
}

}  // namespace proxy
}  // namespace ppapi
