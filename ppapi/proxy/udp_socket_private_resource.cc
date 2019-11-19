// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/udp_socket_private_resource.h"

#include "base/logging.h"
#include "ppapi/c/ppb_udp_socket.h"
#include "ppapi/shared_impl/tracked_callback.h"

namespace ppapi {
namespace proxy {

UDPSocketPrivateResource::UDPSocketPrivateResource(Connection connection,
                                                   PP_Instance instance)
    : UDPSocketResourceBase(connection, instance, true) {
}

UDPSocketPrivateResource::~UDPSocketPrivateResource() {
}

thunk::PPB_UDPSocket_Private_API*
UDPSocketPrivateResource::AsPPB_UDPSocket_Private_API() {
  return this;
}

int32_t UDPSocketPrivateResource::SetSocketFeature(
    PP_UDPSocketFeature_Private name,
    PP_Var value) {
  PP_UDPSocket_Option public_name = PP_UDPSOCKET_OPTION_ADDRESS_REUSE;
  switch (name) {
    case PP_UDPSOCKETFEATURE_PRIVATE_ADDRESS_REUSE:
      // |public_name| has been initialized above.
      break;
    case PP_UDPSOCKETFEATURE_PRIVATE_BROADCAST:
      public_name = PP_UDPSOCKET_OPTION_BROADCAST;
      break;
    case PP_UDPSOCKETFEATURE_PRIVATE_COUNT:
      return PP_ERROR_BADARGUMENT;
    default:
      NOTREACHED();
      return PP_ERROR_BADARGUMENT;
  }
  int32_t result = SetOptionImpl(public_name, value,
                                 true,  // Check bind() state.
                                 nullptr);
  return result == PP_OK_COMPLETIONPENDING ? PP_OK : result;
}

int32_t UDPSocketPrivateResource::Bind(
    const PP_NetAddress_Private* addr,
    scoped_refptr<TrackedCallback> callback) {
  return BindImpl(addr, callback);
}

PP_Bool UDPSocketPrivateResource::GetBoundAddress(PP_NetAddress_Private* addr) {
  return GetBoundAddressImpl(addr);
}

int32_t UDPSocketPrivateResource::RecvFrom(
    char* buffer,
    int32_t num_bytes,
    scoped_refptr<TrackedCallback> callback) {
  return RecvFromImpl(buffer, num_bytes, nullptr, callback);
}

PP_Bool UDPSocketPrivateResource::GetRecvFromAddress(
    PP_NetAddress_Private* addr) {
  return GetRecvFromAddressImpl(addr);
}

int32_t UDPSocketPrivateResource::SendTo(
    const char* buffer,
    int32_t num_bytes,
    const PP_NetAddress_Private* addr,
    scoped_refptr<TrackedCallback> callback) {
  return SendToImpl(buffer, num_bytes, addr, callback);
}

void UDPSocketPrivateResource::Close() {
  CloseImpl();
}

}  // namespace proxy
}  // namespace ppapi
