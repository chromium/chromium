// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_TCP_SOCKET_API_H_
#define PPAPI_THUNK_PPB_TCP_SOCKET_API_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "ppapi/c/ppb_tcp_socket.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_TCPSocket_API {
 public:
  virtual ~PPB_TCPSocket_API() {}

  virtual int32_t Bind(PP_Resource addr,
                       scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t Connect(PP_Resource addr,
                          scoped_refptr<TrackedCallback> callback) = 0;
  virtual PP_Resource GetLocalAddress() = 0;
  virtual PP_Resource GetRemoteAddress() = 0;
  virtual int32_t Read(char* buffer,
                       int32_t bytes_to_read,
                       scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t Write(const char* buffer,
                        int32_t bytes_to_write,
                        scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t Listen(int32_t backlog,
                         scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t Accept(PP_Resource* accepted_tcp_socket,
                         scoped_refptr<TrackedCallback> callback) = 0;
  virtual void Close() = 0;
  virtual int32_t SetOption1_1(PP_TCPSocket_Option name,
                               const PP_Var& value,
                               scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t SetOption(PP_TCPSocket_Option name,
                            const PP_Var& value,
                            scoped_refptr<TrackedCallback> callback) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_TCP_SOCKET_API_H_
