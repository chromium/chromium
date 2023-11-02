// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_udp_socket_private.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_udp_socket_private_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

typedef EnterResource<PPB_UDPSocket_Private_API> EnterUDP;

PP_Resource Create(PP_Instance instance) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateUDPSocketPrivate(instance);
}

PP_Bool IsUDPSocket(PP_Resource resource) {
  EnterUDP enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t SetSocketFeature(PP_Resource udp_socket,
                         PP_UDPSocketFeature_Private name,
                         PP_Var value) {
  EnterUDP enter(udp_socket, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return enter.object()->SetSocketFeature(name, value);
}

int32_t Bind(PP_Resource udp_socket,
             const PP_NetAddress_Private *addr,
             PP_CompletionCallback callback) {
  EnterUDP enter(udp_socket, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Bind(addr, enter.callback()));
}

PP_Bool GetBoundAddress(PP_Resource udp_socket,
                        PP_NetAddress_Private* addr) {
  EnterUDP enter(udp_socket, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->GetBoundAddress(addr);
}

int32_t RecvFrom(PP_Resource udp_socket,
                 char* buffer,
                 int32_t num_bytes,
                 PP_CompletionCallback callback) {
#ifdef NDEBUG
  EnterUDP enter(udp_socket, callback, false);
#else
  EnterUDP enter(udp_socket, callback, true);
#endif
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->RecvFrom(buffer, num_bytes,
                                                  enter.callback()));
}

PP_Bool GetRecvFromAddress(PP_Resource udp_socket,
                           PP_NetAddress_Private* addr) {
  EnterUDP enter(udp_socket, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->GetRecvFromAddress(addr);
}

int32_t SendTo(PP_Resource udp_socket,
               const char* buffer,
               int32_t num_bytes,
               const PP_NetAddress_Private* addr,
               PP_CompletionCallback callback) {
  EnterUDP enter(udp_socket, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->SendTo(buffer, num_bytes, addr,
                                                enter.callback()));
}

void Close(PP_Resource udp_socket) {
  EnterUDP enter(udp_socket, true);
  if (enter.succeeded())
    enter.object()->Close();
}

const PPB_UDPSocket_Private_0_2 g_ppb_udp_socket_thunk_0_2 = {
  &Create,
  &IsUDPSocket,
  &Bind,
  &RecvFrom,
  &GetRecvFromAddress,
  &SendTo,
  &Close
};

const PPB_UDPSocket_Private_0_3 g_ppb_udp_socket_thunk_0_3 = {
  &Create,
  &IsUDPSocket,
  &Bind,
  &GetBoundAddress,
  &RecvFrom,
  &GetRecvFromAddress,
  &SendTo,
  &Close
};

const PPB_UDPSocket_Private_0_4 g_ppb_udp_socket_thunk_0_4 = {
  &Create,
  &IsUDPSocket,
  &SetSocketFeature,
  &Bind,
  &GetBoundAddress,
  &RecvFrom,
  &GetRecvFromAddress,
  &SendTo,
  &Close
};

}  // namespace

const PPB_UDPSocket_Private_0_2* GetPPB_UDPSocket_Private_0_2_Thunk() {
  return &g_ppb_udp_socket_thunk_0_2;
}

const PPB_UDPSocket_Private_0_3* GetPPB_UDPSocket_Private_0_3_Thunk() {
  return &g_ppb_udp_socket_thunk_0_3;
}

const PPB_UDPSocket_Private_0_4* GetPPB_UDPSocket_Private_0_4_Thunk() {
  return &g_ppb_udp_socket_thunk_0_4;
}

}  // namespace thunk
}  // namespace ppapi
