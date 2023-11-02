// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides infrastructure for dispatching host resource call
// messages. Normal IPC message handlers can't take extra parameters or
// return values. We want to take a HostMessageContext as a parameter and
// also return the int32_t return value to the caller.

#ifndef PPAPI_HOST_DISPATCH_HOST_MESSAGE_H_
#define PPAPI_HOST_DISPATCH_HOST_MESSAGE_H_

#include <stdint.h>

#include <tuple>

#include "ipc/ipc_message_macros.h"
#include "ppapi/c/pp_errors.h"

namespace ppapi {
namespace host {

struct HostMessageContext;

template <class ObjT, class Method>
inline int32_t DispatchResourceCall(ObjT* obj, Method method,
                                    HostMessageContext* context,
                                    std::tuple<>& arg) {
  return (obj->*method)(context);
}

template <class ObjT, class Method, class A>
inline int32_t DispatchResourceCall(ObjT* obj, Method method,
                                    HostMessageContext* context,
                                    std::tuple<A>& arg) {
  return (obj->*method)(context, std::get<0>(arg));
}

template<class ObjT, class Method, class A, class B>
inline int32_t DispatchResourceCall(ObjT* obj, Method method,
                                    HostMessageContext* context,
                                    std::tuple<A, B>& arg) {
  return (obj->*method)(context, std::get<0>(arg), std::get<1>(arg));
}

template<class ObjT, class Method, class A, class B, class C>
inline int32_t DispatchResourceCall(ObjT* obj, Method method,
                                    HostMessageContext* context,
                                    std::tuple<A, B, C>& arg) {
  return (obj->*method)(context, std::get<0>(arg), std::get<1>(arg),
                        std::get<2>(arg));
}

template<class ObjT, class Method, class A, class B, class C, class D>
inline int32_t DispatchResourceCall(ObjT* obj, Method method,
                                    HostMessageContext* context,
                                    std::tuple<A, B, C, D>& arg) {
  return (obj->*method)(context, std::get<0>(arg), std::get<1>(arg),
                        std::get<2>(arg), std::get<3>(arg));
}

template<class ObjT, class Method, class A, class B, class C, class D, class E>
inline int32_t DispatchResourceCall(ObjT* obj, Method method,
                                    HostMessageContext* context,
                                    std::tuple<A, B, C, D, E>& arg) {
  return (obj->*method)(context, std::get<0>(arg), std::get<1>(arg),
                        std::get<2>(arg), std::get<3>(arg),
                        std::get<4>(arg));
}

// Note that this only works for message with 1 or more parameters. For
// 0-parameter messages you need to use the _0 version below (since there are
// no params in the message).
#define PPAPI_DISPATCH_HOST_RESOURCE_CALL(msg_class, member_func) \
    case msg_class::ID: { \
      msg_class::Schema::Param p; \
      if (msg_class::Read(&ipc_message__, &p)) { \
        return ppapi::host::DispatchResourceCall( \
            this, \
            &_IpcMessageHandlerClass::member_func, \
            context, p); \
      } \
      return PP_ERROR_FAILED; \
    }

#define PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(msg_class, member_func) \
  case msg_class::ID: { \
    return member_func(context); \
  }

}  // namespace host
}  // namespace ppapi

#endif  // PPAPI_HOST_DISPATCH_HOST_MESSAGE_H_
