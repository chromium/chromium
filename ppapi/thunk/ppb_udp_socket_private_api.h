// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_UDP_SOCKET_PRIVATE_API_H_
#define PPAPI_THUNK_PPB_UDP_SOCKET_PRIVATE_API_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "ppapi/c/private/ppb_udp_socket_private.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_UDPSocket_Private_API {
 public:
  virtual ~PPB_UDPSocket_Private_API() {}

  virtual int32_t SetSocketFeature(PP_UDPSocketFeature_Private name,
                                   PP_Var value) = 0;
  virtual int32_t Bind(const PP_NetAddress_Private* addr,
                       scoped_refptr<TrackedCallback> callback) = 0;
  virtual PP_Bool GetBoundAddress(PP_NetAddress_Private* addr) = 0;
  virtual int32_t RecvFrom(char* buffer,
                           int32_t num_bytes,
                           scoped_refptr<TrackedCallback> callback) = 0;
  virtual PP_Bool GetRecvFromAddress(PP_NetAddress_Private* addr) = 0;
  virtual int32_t SendTo(const char* buffer,
                         int32_t num_bytes,
                         const PP_NetAddress_Private* addr,
                         scoped_refptr<TrackedCallback> callback) = 0;
  virtual void Close() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_UDP_SOCKET_PRIVATE_API_H_
