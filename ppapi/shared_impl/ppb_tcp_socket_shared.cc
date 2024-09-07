// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_tcp_socket_shared.h"

#include "base/check.h"
#include "base/notreached.h"

namespace ppapi {

TCPSocketState::TCPSocketState(StateType state)
    : state_(state), pending_transition_(NONE) {
  DCHECK(state_ == INITIAL || state_ == CONNECTED);
}

TCPSocketState::~TCPSocketState() {}

void TCPSocketState::SetPendingTransition(TransitionType pending_transition) {
  DCHECK(IsValidTransition(pending_transition));
  pending_transition_ = pending_transition;
}

void TCPSocketState::CompletePendingTransition(bool success) {
  switch (pending_transition_) {
    case NONE:
      NOTREACHED();
    case BIND:
      if (success)
        state_ = BOUND;
      break;
    case CONNECT:
      state_ = success ? CONNECTED : CLOSED;
      break;
    case SSL_CONNECT:
      state_ = success ? SSL_CONNECTED : CLOSED;
      break;
    case LISTEN:
      state_ = success ? LISTENING : CLOSED;
      break;
    case CLOSE:
      state_ = CLOSED;
      break;
  }
  pending_transition_ = NONE;
}

void TCPSocketState::DoTransition(TransitionType transition, bool success) {
  SetPendingTransition(transition);
  CompletePendingTransition(success);
}

bool TCPSocketState::IsValidTransition(TransitionType transition) const {
  if (pending_transition_ != NONE && transition != CLOSE)
    return false;

  switch (transition) {
    case NONE:
      return false;
    case BIND:
      return state_ == INITIAL;
    case CONNECT:
      return state_ == INITIAL || state_ == BOUND;
    case SSL_CONNECT:
      return state_ == CONNECTED;
    case LISTEN:
      return state_ == BOUND;
    case CLOSE:
      return true;
  }
  NOTREACHED();
}

bool TCPSocketState::IsPending(TransitionType transition) const {
  return pending_transition_ == transition;
}

bool TCPSocketState::IsConnected() const {
  return state_ == CONNECTED || state_ == SSL_CONNECTED;
}

bool TCPSocketState::IsBound() const {
  return state_ != INITIAL && state_ != CLOSED;
}

}  // namespace ppapi
