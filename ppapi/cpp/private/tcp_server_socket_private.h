// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_TCP_SERVER_SOCKET_PRIVATE_H_
#define PPAPI_CPP_PRIVATE_TCP_SERVER_SOCKET_PRIVATE_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/private/ppb_tcp_server_socket_private.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class CompletionCallback;
class InstanceHandle;

class TCPServerSocketPrivate : public Resource {
 public:
  explicit TCPServerSocketPrivate(const InstanceHandle& instance);

  // Returns true if the required interface is available.
  static bool IsAvailable();

  int32_t Listen(const PP_NetAddress_Private* addr,
                 int32_t backlog,
                 const CompletionCallback& callback);
  // Accepts incoming connection and stores resource of accepted
  // socket into |socket|. If Accept returns PP_OK_COMPLETIONPENDING,
  // the memory pointed by |socket| should stay valid until the
  // |callback| is called or StopListening method is called.
  int32_t Accept(PP_Resource* socket,
                 const CompletionCallback& callback);
  int32_t GetLocalAddress(PP_NetAddress_Private* addr);
  void StopListening();
};

}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_TCP_SERVER_SOCKET_PRIVATE_H_
