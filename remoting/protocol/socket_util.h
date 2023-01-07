// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_SOCKET_UTIL_H_
#define REMOTING_PROTOCOL_SOCKET_UTIL_H_

namespace remoting {

// Enum for different actions that can be taken after sendto() returns an error.
enum SocketErrorAction {
  SOCKET_ERROR_ACTION_FAIL,
  SOCKET_ERROR_ACTION_IGNORE,
  SOCKET_ERROR_ACTION_RETRY,
};

// Returns true if |error| must be ignored when returned from sendto(). |retry|
// is set set when sentto() should be called for the same packet again.
SocketErrorAction GetSocketErrorAction(int error);

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_SOCKET_UTIL_H_
