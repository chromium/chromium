// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_IPC_FIFO_BUFFER_H_
#define REMOTING_BASE_IPC_FIFO_BUFFER_H_

#include <optional>

#include "base/containers/span.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "remoting/base/fifo_buffer.h"

namespace remoting {

// A FifoBufferWriter implementation backed by a Mojo Data Pipe Producer Handle.
// All SPSC methods must be called from a single thread (lazily bound on the
// first call), but the instance can be safely constructed and destructed on a
// different sequence (such as the owner main thread) as long as there is no
// concurrent access.
class IpcFifoBufferWriter : public FifoBufferWriter {
 public:
  explicit IpcFifoBufferWriter(
      mojo::ScopedDataPipeProducerHandle producer_handle);

  IpcFifoBufferWriter(const IpcFifoBufferWriter&) = delete;
  IpcFifoBufferWriter& operator=(const IpcFifoBufferWriter&) = delete;

  ~IpcFifoBufferWriter() override;

  // FifoBufferWriter implementation.
  Result Write(base::span<const uint8_t> data) override;

  mojo::ScopedDataPipeProducerHandle TakeProducerHandle();

 private:
  mojo::ScopedDataPipeProducerHandle producer_handle_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// A FifoBufferReader implementation backed by a Mojo Data Pipe Consumer Handle.
// All SPSC methods must be called from a single thread (lazily bound on the
// first call), but the instance can be safely constructed and destructed on a
// different sequence (such as the owner main thread) as long as there is no
// concurrent access.
class IpcFifoBufferReader : public FifoBufferReader {
 public:
  explicit IpcFifoBufferReader(
      mojo::ScopedDataPipeConsumerHandle consumer_handle);

  IpcFifoBufferReader(const IpcFifoBufferReader&) = delete;
  IpcFifoBufferReader& operator=(const IpcFifoBufferReader&) = delete;

  ~IpcFifoBufferReader() override;

  // FifoBufferReader implementation.
  std::optional<size_t> Read(base::span<uint8_t> destination) override;
  std::optional<size_t> Skip(size_t bytes) override;
  void Clear() override;
  std::optional<size_t> GetBufferedBytes() const override;

  mojo::ScopedDataPipeConsumerHandle TakeConsumerHandle();

 private:
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// Creates a Mojo-backed SPSC FIFO buffer pair with the specified `capacity`.
// If successful, populates `writer` and `reader` and returns true.
// If Mojo Data Pipe creation fails, returns false.
bool CreateIpcFifoBuffer(size_t capacity,
                         std::unique_ptr<IpcFifoBufferWriter>& writer,
                         std::unique_ptr<IpcFifoBufferReader>& reader);

}  // namespace remoting

#endif  // REMOTING_BASE_IPC_FIFO_BUFFER_H_
