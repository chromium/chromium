// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_UDP_SOCKET_PRIVATE_H_
#define PPAPI_CPP_PRIVATE_UDP_SOCKET_PRIVATE_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/private/ppb_udp_socket_private.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class CompletionCallback;
class InstanceHandle;
class Var;

class UDPSocketPrivate : public Resource {
 public:
  explicit UDPSocketPrivate(const InstanceHandle& instance);

  // Returns true if the required interface is available.
  static bool IsAvailable();

  int32_t SetSocketFeature(PP_UDPSocketFeature_Private name, const Var& value);
  int32_t Bind(const PP_NetAddress_Private* addr,
               const CompletionCallback& callback);
  bool GetBoundAddress(PP_NetAddress_Private* addr);
  int32_t RecvFrom(char* buffer,
                   int32_t num_bytes,
                   const CompletionCallback& callback);
  bool GetRecvFromAddress(PP_NetAddress_Private* addr);
  int32_t SendTo(const char* buffer,
                 int32_t num_bytes,
                 const PP_NetAddress_Private* addr,
                 const CompletionCallback& callback);
  void Close();
};

}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_UDP_SOCKET_PRIVATE_H_
