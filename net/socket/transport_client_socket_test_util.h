// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TRANSPORT_CLIENT_SOCKET_TEST_UTIL_H_
#define NET_SOCKET_TRANSPORT_CLIENT_SOCKET_TEST_UTIL_H_

#include <stdint.h>
#include <string>

#include "net/base/test_completion_callback.h"
#include "net/socket/stream_socket.h"

namespace net {

class IOBuffer;

// Sends a request from `socket` to `connected_socket`. Makes `connected_socket`
// read the request and send a response.
void SendRequestAndResponse(StreamSocket* socket,
                            StreamSocket* connected_socket);

// Reads `expected_bytes_read` bytes from `socket`. Returns the data
// read as a string.
std::string ReadDataOfExpectedLength(StreamSocket* socket,
                                     int expected_bytes_read);

// Sends response from `socket`.
void SendServerResponse(StreamSocket* socket);

// `socket` reads `bytes_to_read` number of bytes into `buf`. Returns number of
// bytes read.
int DrainStreamSocket(StreamSocket* socket,
                      IOBuffer* buf,
                      uint32_t buf_len,
                      uint32_t bytes_to_read,
                      TestCompletionCallback* callback);

}  // namespace net

#endif  // NET_SOCKET_TRANSPORT_CLIENT_SOCKET_TEST_UTIL_H_
