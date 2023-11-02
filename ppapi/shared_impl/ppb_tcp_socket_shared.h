// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_TCP_SOCKET_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_TCP_SOCKET_SHARED_H_

#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

class PPAPI_SHARED_EXPORT TCPSocketState {
 public:
  enum StateType {
    // The socket hasn't been bound or connected.
    INITIAL,
    // The socket has been bound.
    BOUND,
    // A connection has been established.
    CONNECTED,
    // An SSL connection has been established.
    SSL_CONNECTED,
    // The socket is listening.
    LISTENING,
    // The socket has been closed.
    CLOSED
  };

  // Transitions that will change the socket state. Please note that
  // read/write/accept are not included because they don't change the socket
  // state.
  enum TransitionType { NONE, BIND, CONNECT, SSL_CONNECT, LISTEN, CLOSE };

  explicit TCPSocketState(StateType state);
  ~TCPSocketState();

  StateType state() const { return state_; }

  void SetPendingTransition(TransitionType pending_transition);
  void CompletePendingTransition(bool success);

  void DoTransition(TransitionType transition, bool success);

  bool IsValidTransition(TransitionType transition) const;
  bool IsPending(TransitionType transition) const;

  bool IsConnected() const;
  bool IsBound() const;

 private:
  StateType state_;
  TransitionType pending_transition_;
};

// TCP socket API versions.
enum TCPSocketVersion {
  // PPB_TCPSocket_Private.
  TCP_SOCKET_VERSION_PRIVATE,
  // PPB_TCPSocket v1.0.
  TCP_SOCKET_VERSION_1_0,
  // PPB_TCPSocket v1.1 or above.
  TCP_SOCKET_VERSION_1_1_OR_ABOVE
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_TCP_SOCKET_SHARED_H_
