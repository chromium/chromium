// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_READ_MULTIPLE_EMULATOR_H_
#define NET_SOCKET_READ_MULTIPLE_EMULATOR_H_

#include <stddef.h>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/socket/datagram_client_socket.h"

namespace net {

class IOBuffer;
class Socket;

// TODO(crbug.com/515333601): This is helper class for temporary delegation to
// Read() to avoid crashes when QuicUseReadMultiple is enabled. Delete this
// once proper ReadMultiple() is implemented in all sockets.
//
// A helper class to emulate ReadMultiple() using Read().
// This can be used by DatagramClientSocket implementations that do not have
// a native ReadMultiple() implementation.
class NET_EXPORT ReadMultipleEmulator {
 public:
  // `socket` must outlive this emulator.
  explicit ReadMultipleEmulator(Socket* socket);

  ReadMultipleEmulator(const ReadMultipleEmulator&) = delete;
  ReadMultipleEmulator& operator=(const ReadMultipleEmulator&) = delete;

  ~ReadMultipleEmulator();

  base::expected<DatagramsMetadata, Error> ReadMultiple(
      IOBuffer* buf,
      size_t buf_len,
      size_t maximum_packet_size,
      base::OnceCallback<void(base::expected<DatagramsMetadata, Error>)>
          callback);

 private:
  void OnReadComplete(
      base::OnceCallback<void(base::expected<DatagramsMetadata, Error>)>
          callback,
      int rv);

  const raw_ptr<Socket> socket_;
  base::WeakPtrFactory<ReadMultipleEmulator> weak_factory_{this};
};

}  // namespace net

#endif  // NET_SOCKET_READ_MULTIPLE_EMULATOR_H_
