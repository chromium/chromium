// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ppapi/fake_host_resolver_interface.h"

#include <netinet/in.h>

#include "fake_ppapi/fake_pepper_interface.h"
#include "fake_ppapi/fake_resource_manager.h"
#include "fake_ppapi/fake_var_manager.h"
#include "fake_ppapi/fake_util.h"
#include "gtest/gtest.h"

namespace {

class FakeHostResolverResource : public FakeResource {
 public:
  FakeHostResolverResource() : resolved(false) {}
  static const char* classname() { return "FakeHostResolverResource"; }

  bool resolved;
  PP_HostResolver_Hint hints;
};
}

FakeHostResolverInterface::FakeHostResolverInterface(FakePepperInterface* ppapi)
    : ppapi_(ppapi) {}

PP_Resource FakeHostResolverInterface::Create(PP_Instance instance) {
  if (instance != ppapi_->GetInstance())
    return PP_ERROR_BADRESOURCE;

  FakeHostResolverResource* resolver_resource = new FakeHostResolverResource();

  return CREATE_RESOURCE(ppapi_->resource_manager(), FakeHostResolverResource,
                         resolver_resource);
}

int32_t FakeHostResolverInterface::Resolve(PP_Resource resource,
                                           const char* hostname,
                                           uint16_t,
                                           const PP_HostResolver_Hint* hints,
                                           PP_CompletionCallback callback) {
  FakeHostResolverResource* resolver =
      ppapi_->resource_manager()->Get<FakeHostResolverResource>(resource);
  resolver->resolved = false;
  resolver->hints = *hints;
  if (!fake_hostname.empty() && fake_hostname == hostname) {
    resolver->resolved = true;
    return RunCompletionCallback(&callback, PP_OK);
  }
  return RunCompletionCallback(&callback, PP_ERROR_NAME_NOT_RESOLVED);
}

PP_Var FakeHostResolverInterface::GetCanonicalName(PP_Resource resource) {
  FakeHostResolverResource* res =
      ppapi_->resource_manager()->Get<FakeHostResolverResource>(resource);
  if (!res->resolved)
    return PP_Var();
  return ppapi_->GetVarInterface()->VarFromUtf8(fake_hostname.data(),
                                                fake_hostname.length());
}

uint32_t FakeHostResolverInterface::GetNetAddressCount(PP_Resource resolver) {
  FakeHostResolverResource* res =
      ppapi_->resource_manager()->Get<FakeHostResolverResource>(resolver);
  if (!res->resolved)
    return 0;

  uint32_t rtn = 0;
  if (res->hints.family == PP_NETADDRESS_FAMILY_IPV6 ||
      res->hints.family == PP_NETADDRESS_FAMILY_UNSPECIFIED)
    rtn += fake_addresses_v6.size();

  if (res->hints.family == PP_NETADDRESS_FAMILY_IPV4 ||
      res->hints.family == PP_NETADDRESS_FAMILY_UNSPECIFIED)
    rtn += fake_addresses_v4.size();

  return rtn;
}

PP_Resource FakeHostResolverInterface::GetNetAddress(PP_Resource resource,
                                                     uint32_t index) {
  FakeHostResolverResource* res =
      ppapi_->resource_manager()->Get<FakeHostResolverResource>(resource);
  if (!res->resolved)
    return 0;

  bool include_v4 = false;
  int max_index = 0;
  switch (res->hints.family) {
    case PP_NETADDRESS_FAMILY_IPV4:
      max_index = fake_addresses_v4.size();
      include_v4 = true;
      break;
    case PP_NETADDRESS_FAMILY_IPV6:
      max_index = fake_addresses_v6.size();
      break;
    case PP_NETADDRESS_FAMILY_UNSPECIFIED:
      include_v4 = true;
      max_index = fake_addresses_v4.size() + fake_addresses_v6.size();
      break;
    default:
      return 0;
  }

  if (index >= max_index)
    return 0;

  nacl_io::NetAddressInterface* iface = ppapi_->GetNetAddressInterface();

  // Create a new NetAddress resource and return it.
  if (include_v4 && index < fake_addresses_v4.size()) {
    PP_NetAddress_IPv4 addr;
    sockaddr_in& addr4 = fake_addresses_v4[index];
    memcpy(addr.addr, &addr4.sin_addr, sizeof(addr4.sin_addr));
    return iface->CreateFromIPv4Address(ppapi_->GetInstance(), &addr);
  } else {
    if (include_v4)
      index -= fake_addresses_v4.size();
    PP_NetAddress_IPv6 addr;
    sockaddr_in6& addr6 = fake_addresses_v6[index];
    memcpy(addr.addr, &addr6.sin6_addr, sizeof(addr6.sin6_addr));
    return iface->CreateFromIPv6Address(ppapi_->GetInstance(), &addr);
  }
}
