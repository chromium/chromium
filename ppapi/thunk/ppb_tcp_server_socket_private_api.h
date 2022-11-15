// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_TCP_SERVER_SOCKET_PRIVATE_API_H_
#define PPAPI_THUNK_PPB_TCP_SERVER_SOCKET_PRIVATE_API_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "ppapi/c/private/ppb_tcp_server_socket_private.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_TCPServerSocket_Private_API {
public:
  virtual ~PPB_TCPServerSocket_Private_API() {}

  virtual int32_t Listen(const PP_NetAddress_Private* addr,
                         int32_t backlog,
                         scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t Accept(PP_Resource* tcp_socket,
                         scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t GetLocalAddress(PP_NetAddress_Private* addr) = 0;
  virtual void StopListening() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_TCP_SERVER_SOCKET_PRIVATE_API_H_
