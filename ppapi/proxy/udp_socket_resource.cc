// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/udp_socket_resource.h"

#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_net_address_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace proxy {

namespace {

typedef thunk::EnterResourceNoLock<thunk::PPB_NetAddress_API>
    EnterNetAddressNoLock;

}  // namespace

UDPSocketResource::UDPSocketResource(Connection connection,
                                     PP_Instance instance)
    : UDPSocketResourceBase(connection, instance, false) {
}

UDPSocketResource::~UDPSocketResource() {
}

thunk::PPB_UDPSocket_API* UDPSocketResource::AsPPB_UDPSocket_API() {
  return this;
}

int32_t UDPSocketResource::Bind(PP_Resource addr,
                                scoped_refptr<TrackedCallback> callback) {
  EnterNetAddressNoLock enter(addr, true);
  if (enter.failed())
    return PP_ERROR_BADARGUMENT;

  return BindImpl(&enter.object()->GetNetAddressPrivate(), callback);
}

PP_Resource UDPSocketResource::GetBoundAddress() {
  PP_NetAddress_Private addr_private;
  if (!GetBoundAddressImpl(&addr_private))
    return 0;

  thunk::EnterResourceCreationNoLock enter(pp_instance());
  if (enter.failed())
    return 0;
  return enter.functions()->CreateNetAddressFromNetAddressPrivate(
      pp_instance(), addr_private);
}

int32_t UDPSocketResource::RecvFrom(char* buffer,
                                    int32_t num_bytes,
                                    PP_Resource* addr,
                                    scoped_refptr<TrackedCallback> callback) {
  return RecvFromImpl(buffer, num_bytes, addr, callback);
}

int32_t UDPSocketResource::SendTo(const char* buffer,
                                  int32_t num_bytes,
                                  PP_Resource addr,
                                  scoped_refptr<TrackedCallback> callback) {
  EnterNetAddressNoLock enter(addr, true);
  if (enter.failed())
    return PP_ERROR_BADARGUMENT;

  return SendToImpl(buffer, num_bytes, &enter.object()->GetNetAddressPrivate(),
                    callback);
}

void UDPSocketResource::Close() {
  CloseImpl();
}

int32_t UDPSocketResource::SetOption1_0(
    PP_UDPSocket_Option name,
    const PP_Var& value,
    scoped_refptr<TrackedCallback> callback) {
  if (name > PP_UDPSOCKET_OPTION_RECV_BUFFER_SIZE)
    return PP_ERROR_BADARGUMENT;

  return SetOptionImpl(name, value,
                       true,  // Check bind() state.
                       callback);
}

int32_t UDPSocketResource::SetOption1_1(
    PP_UDPSocket_Option name,
    const PP_Var& value,
    scoped_refptr<TrackedCallback> callback) {
  if (name > PP_UDPSOCKET_OPTION_RECV_BUFFER_SIZE)
    return PP_ERROR_BADARGUMENT;

  return SetOptionImpl(name, value,
                       false,  // Check bind() state.
                       callback);
}

int32_t UDPSocketResource::SetOption(
    PP_UDPSocket_Option name,
    const PP_Var& value,
    scoped_refptr<TrackedCallback> callback) {
  return SetOptionImpl(name, value,
                       false,  // Check bind() state.
                       callback);
}

int32_t UDPSocketResource::JoinGroup(
    PP_Resource group,
    scoped_refptr<TrackedCallback> callback) {
  EnterNetAddressNoLock enter(group, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;

  return JoinGroupImpl(&enter.object()->GetNetAddressPrivate(),
                       callback);
}

int32_t UDPSocketResource::LeaveGroup(
    PP_Resource group,
    scoped_refptr<TrackedCallback> callback) {
  EnterNetAddressNoLock enter(group, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;

  return LeaveGroupImpl(&enter.object()->GetNetAddressPrivate(),
                        callback);
}

}  // namespace proxy
}  // namespace ppapi
