// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/net_address.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_NetAddress_1_0>() {
  return PPB_NETADDRESS_INTERFACE_1_0;
}

}  // namespace

NetAddress::NetAddress() {
}

NetAddress::NetAddress(PassRef, PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

NetAddress::NetAddress(const InstanceHandle& instance,
                       const PP_NetAddress_IPv4& ipv4_addr) {
  if (has_interface<PPB_NetAddress_1_0>()) {
    PassRefFromConstructor(
        get_interface<PPB_NetAddress_1_0>()->CreateFromIPv4Address(
            instance.pp_instance(), &ipv4_addr));
  }
}

NetAddress::NetAddress(const InstanceHandle& instance,
                       const PP_NetAddress_IPv6& ipv6_addr) {
  if (has_interface<PPB_NetAddress_1_0>()) {
    PassRefFromConstructor(
        get_interface<PPB_NetAddress_1_0>()->CreateFromIPv6Address(
            instance.pp_instance(), &ipv6_addr));
  }
}

NetAddress::NetAddress(const NetAddress& other) : Resource(other) {
}

NetAddress::~NetAddress() {
}

NetAddress& NetAddress::operator=(const NetAddress& other) {
  Resource::operator=(other);
  return *this;
}

// static
bool NetAddress::IsAvailable() {
  return has_interface<PPB_NetAddress_1_0>();
}

PP_NetAddress_Family NetAddress::GetFamily() const {
  if (has_interface<PPB_NetAddress_1_0>())
    return get_interface<PPB_NetAddress_1_0>()->GetFamily(pp_resource());

  return PP_NETADDRESS_FAMILY_UNSPECIFIED;
}

Var NetAddress::DescribeAsString(bool include_port) const {
  if (has_interface<PPB_NetAddress_1_0>()) {
    return Var(PASS_REF,
               get_interface<PPB_NetAddress_1_0>()->DescribeAsString(
                   pp_resource(), PP_FromBool(include_port)));
  }

  return Var();
}

bool NetAddress::DescribeAsIPv4Address(PP_NetAddress_IPv4* ipv4_addr) const {
  if (has_interface<PPB_NetAddress_1_0>()) {
    return PP_ToBool(
        get_interface<PPB_NetAddress_1_0>()->DescribeAsIPv4Address(
            pp_resource(), ipv4_addr));
  }

  return false;
}

bool NetAddress::DescribeAsIPv6Address(PP_NetAddress_IPv6* ipv6_addr) const {
  if (has_interface<PPB_NetAddress_1_0>()) {
    return PP_ToBool(
        get_interface<PPB_NetAddress_1_0>()->DescribeAsIPv6Address(
            pp_resource(), ipv6_addr));
  }

  return false;
}

}  // namespace pp
