// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/read_multiple_emulator.h"

#include <algorithm>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "net/base/io_buffer.h"
#include "net/socket/socket.h"

namespace net {

ReadMultipleEmulator::ReadMultipleEmulator(Socket* socket) : socket_(socket) {
  CHECK(socket_);
}

ReadMultipleEmulator::~ReadMultipleEmulator() = default;

base::expected<DatagramsMetadata, Error> ReadMultipleEmulator::ReadMultiple(
    IOBuffer* buf,
    size_t buf_len,
    size_t maximum_packet_size,
    base::OnceCallback<void(base::expected<DatagramsMetadata, Error>)>
        callback) {
  auto adapted_callback =
      base::BindOnce(&ReadMultipleEmulator::OnReadComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback));

  int rv = socket_->Read(
      buf, base::checked_cast<int>(std::min(buf_len, maximum_packet_size)),
      std::move(adapted_callback));

  if (rv < 0) {
    return base::unexpected(static_cast<Error>(rv));
  }

  return DatagramsMetadata{
      {/*offset=*/0, /*length=*/static_cast<size_t>(rv), /*tos=*/0}};
}

void ReadMultipleEmulator::OnReadComplete(
    base::OnceCallback<void(base::expected<DatagramsMetadata, Error>)> callback,
    int rv) {
  if (rv < 0) {
    std::move(callback).Run(base::unexpected(static_cast<Error>(rv)));
  } else {
    std::move(callback).Run(DatagramsMetadata{
        {/*offset=*/0, /*length=*/static_cast<size_t>(rv), /*tos=*/0}});
  }
}

}  // namespace net
