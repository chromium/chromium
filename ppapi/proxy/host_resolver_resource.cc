// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/host_resolver_resource.h"

#include "base/notreached.h"
#include "ppapi/c/private/ppb_host_resolver_private.h"
#include "ppapi/proxy/net_address_resource.h"
#include "ppapi/shared_impl/tracked_callback.h"

namespace ppapi {
namespace proxy {

namespace {

PP_HostResolver_Private_Hint ConvertToHostResolverPrivateHint(
    const PP_HostResolver_Hint& hint) {
  PP_HostResolver_Private_Hint private_hint;
  switch (hint.family) {
    case PP_NETADDRESS_FAMILY_UNSPECIFIED:
      private_hint.family = PP_NETADDRESSFAMILY_PRIVATE_UNSPECIFIED;
      break;
    case PP_NETADDRESS_FAMILY_IPV4:
      private_hint.family = PP_NETADDRESSFAMILY_PRIVATE_IPV4;
      break;
    case PP_NETADDRESS_FAMILY_IPV6:
      private_hint.family = PP_NETADDRESSFAMILY_PRIVATE_IPV6;
      break;
    default:
      NOTREACHED();
  }

  private_hint.flags = 0;
  if (hint.flags & PP_HOSTRESOLVER_FLAG_CANONNAME)
    private_hint.flags |= PP_HOST_RESOLVER_PRIVATE_FLAGS_CANONNAME;

  return private_hint;
}

}  // namespace

HostResolverResource::HostResolverResource(Connection connection,
                                           PP_Instance instance)
    : HostResolverResourceBase(connection, instance, false) {
}

HostResolverResource::~HostResolverResource() {
}

thunk::PPB_HostResolver_API* HostResolverResource::AsPPB_HostResolver_API() {
  return this;
}

int32_t HostResolverResource::Resolve(const char* host,
                                      uint16_t port,
                                      const PP_HostResolver_Hint* hint,
                                      scoped_refptr<TrackedCallback> callback) {
  if (!hint)
    return PP_ERROR_BADARGUMENT;

  PP_HostResolver_Private_Hint private_hint =
      ConvertToHostResolverPrivateHint(*hint);
  return ResolveImpl(host, port, &private_hint, callback);
}

PP_Var HostResolverResource::GetCanonicalName() {
  return GetCanonicalNameImpl();
}

uint32_t HostResolverResource::GetNetAddressCount() {
  return GetSizeImpl();
}

PP_Resource HostResolverResource::GetNetAddress(uint32_t index) {
  scoped_refptr<NetAddressResource> addr_resource = GetNetAddressImpl(index);
  if (!addr_resource.get())
    return 0;

  return addr_resource->GetReference();
}

}  // namespace proxy
}  // namespace ppapi
