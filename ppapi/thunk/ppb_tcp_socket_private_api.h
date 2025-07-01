// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_TCP_SOCKET_PRIVATE_API_H_
#define PPAPI_THUNK_PPB_TCP_SOCKET_PRIVATE_API_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "ppapi/c/private/ppb_tcp_socket_private.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_TCPSocket_Private_API {
 public:
  virtual ~PPB_TCPSocket_Private_API() {}

  virtual int32_t Connect(const char* host,
                          uint16_t port,
                          scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t ConnectWithNetAddress(
      const PP_NetAddress_Private* addr,
      scoped_refptr<TrackedCallback> callback) = 0;
  virtual PP_Bool GetLocalAddress(PP_NetAddress_Private* local_addr) = 0;
  virtual PP_Bool GetRemoteAddress(PP_NetAddress_Private* remote_addr) = 0;
  virtual int32_t SSLHandshake(const char* server_name,
                               uint16_t server_port,
                               scoped_refptr<TrackedCallback> callback) = 0;
  virtual PP_Resource GetServerCertificate() = 0;
  virtual PP_Bool AddChainBuildingCertificate(PP_Resource certificate,
                                              PP_Bool trusted) = 0;
  virtual int32_t Read(char* buffer,
                       int32_t bytes_to_read,
                       scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t Write(const char* buffer,
                        int32_t bytes_to_write,
                        scoped_refptr<TrackedCallback> callback) = 0;
  virtual void Disconnect() = 0;
  virtual int32_t SetOption(PP_TCPSocketOption_Private name,
                            const PP_Var& value,
                            scoped_refptr<TrackedCallback> callback) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_TCP_SOCKET_PRIVATE_API_H_
