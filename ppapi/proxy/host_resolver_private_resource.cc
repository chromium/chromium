// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/host_resolver_private_resource.h"

#include "ppapi/proxy/net_address_resource.h"
#include "ppapi/shared_impl/tracked_callback.h"

namespace ppapi {
namespace proxy {

HostResolverPrivateResource::HostResolverPrivateResource(Connection connection,
                                                         PP_Instance instance)
    : HostResolverResourceBase(connection, instance, true) {
}

HostResolverPrivateResource::~HostResolverPrivateResource() {
}

thunk::PPB_HostResolver_Private_API*
HostResolverPrivateResource::AsPPB_HostResolver_Private_API() {
  return this;
}

int32_t HostResolverPrivateResource::Resolve(
    const char* host,
    uint16_t port,
    const PP_HostResolver_Private_Hint* hint,
    scoped_refptr<TrackedCallback> callback) {
  return ResolveImpl(host, port, hint, callback);
}

PP_Var HostResolverPrivateResource::GetCanonicalName() {
  return GetCanonicalNameImpl();
}

uint32_t HostResolverPrivateResource::GetSize() {
  return GetSizeImpl();
}

bool HostResolverPrivateResource::GetNetAddress(
    uint32_t index,
    PP_NetAddress_Private* address) {
  if (!address)
    return false;

  scoped_refptr<NetAddressResource> addr_resource = GetNetAddressImpl(index);
  if (!addr_resource.get())
    return false;

  *address = addr_resource->GetNetAddressPrivate();
  return true;
}

}  // namespace proxy
}  // namespace ppapi
