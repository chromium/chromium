// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Protobuf ZeroCopy[Input/Output]Stream implementations capable of using
// mojo data pipes. Built to work with Protobuf CodedStreams.

#ifndef GOOGLE_APIS_GCM_BASE_SOCKET_STREAM_H_
#define GOOGLE_APIS_GCM_BASE_SOCKET_STREAM_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google_apis/gcm/base/gcm_export.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/net_errors.h"

namespace net {
class DrainableIOBuffer;
class IOBuffer;
class IOBufferWithSize;
}  // namespace net

namespace gcm {

// A helper class for interacting with a mojo consumer pipe that is receiving
// protobuf encoded messages. If an error is encounters, the input stream will
// store the error in |last_error_|, and GetState() will be set to CLOSED.
// Typical usage:
// 1. Check the GetState() of the input stream before using it. If CLOSED, the
//    input stream must be rebuilt (and the socket likely needs to be
//    reconnected as an error was encountered).
// 2. If GetState() is EMPTY, call Refresh(..), passing the maximum byte size
//    for a message, and wait until completion. It is invalid to attempt to
//    Refresh an input stream or read data from the stream while a Refresh is
//    pending.
// 3. Check GetState() again to ensure the Refresh was successful.
// 4. Use a CodedInputStream to read from the ZeroCopyInputStream interface of
//    the SocketInputStream. Next(..) will return true until there is no data
//    remaining.
// 5. Call RebuildBuffer when done reading, to shift any unread data to the
//    start of the buffer.
// 6. Repeat as necessary.
class GCM_EXPORT SocketInputStream
    : public google::protobuf::io::ZeroCopyInputStream {
 public:
  enum State {
    // No valid data to read. This means the buffer is either empty or all data
    // in the buffer has already been consumed.
    EMPTY,
    // Valid data to read.
    READY,
    // In the process of reading new data from the socket.
    READING,
    // An permanent error occurred and the stream is now closed.
    CLOSED,
  };

  // |socket| should already be connected.
  explicit SocketInputStream(mojo::ScopedDataPipeConsumerHandle stream);

  SocketInputStream(const SocketInputStream&) = delete;
  SocketInputStream& operator=(const SocketInputStream&) = delete;

  ~SocketInputStream() override;

  // ZeroCopyInputStream implementation.
  bool Next(const void** data, int* size) override;
  void BackUp(int count) override;
  bool Skip(int count) override;  // Not implemented.
  int64_t ByteCount() const override;

  // The remaining amount of valid data available to be read.
  int UnreadByteCount() const;

  // Reads from the socket, appending a max of |byte_limit| bytes onto the read
  // buffer. net::ERR_IO_PENDING is returned if the refresh can't complete
  // synchronously, in which case the callback is invoked upon completion. If
  // the refresh can complete synchronously, even in case of an error, returns
  // net::OK without invoking callback.
  // Note: GetState() (and possibly last_error()) should be checked upon
  // completion to determine whether the Refresh encountered an error.
  net::Error Refresh(base::OnceClosure callback, int byte_limit);

  // Rebuilds the buffer state by copying over any unread data to the beginning
  // of the buffer and resetting the buffer read/write positions.
  // Note: it is not valid to call Rebuild() if GetState() == CLOSED. The stream
  // must be recreated from scratch in such a scenario.
  void RebuildBuffer();

  // Returns the last fatal error encountered. Only valid if GetState() ==
  // CLOSED. Note that all network read errors will be reported as
  // net::ERR_FAILED, because mojo data pipe doesn't allow surfacing a more
  // specific error code.
  net::Error last_error() const;

  // Returns the current state.
  State GetState() const;

 private:
  // Clears the local state.
  void ResetInternal();

  void ReadMore(MojoResult result, const mojo::HandleSignalsState& state);

  // Permanently closes the stream.
  void CloseStream(net::Error error);

  // Internal net components.
  mojo::ScopedDataPipeConsumerHandle stream_;
  mojo::SimpleWatcher stream_watcher_;
  size_t read_size_;
  const scoped_refptr<net::IOBuffer> io_buffer_;
  base::OnceClosure read_callback_;
  // IOBuffer implementation that wraps the data within |io_buffer_| that hasn't
  // been written to yet by Socket::Read calls.
  const scoped_refptr<net::DrainableIOBuffer> read_buffer_;

  // Starting position of the data within |io_buffer_| to consume on subsequent
  // Next(..) call.  0 <= next_pos_ <= read_buffer_.BytesConsumed()
  // Note: next_pos == read_buffer_.BytesConsumed() implies GetState() == EMPTY.
  int next_pos_;

  // If < net::ERR_IO_PENDING, the last net error received.
  // Note: last_error_ == net::ERR_IO_PENDING implies GetState() == READING.
  net::Error last_error_;

  base::WeakPtrFactory<SocketInputStream> weak_ptr_factory_{this};
};

// A helper class for writing to a mojo producer handle with protobuf encoded
// data. Typical usage:
// 1. Check the GetState() of the output stream before using it. If CLOSED, the
//    output stream must be rebuilt (and the socket likely needs to be
//    reconnected, as an error was encountered).
// 2. If EMPTY, the output stream can be written via a CodedOutputStream using
//    the ZeroCopyOutputStream interface.
// 3. Once done writing, GetState() should be READY, so call Flush(..) to write
//    the buffer into the mojo producer handle. Wait for the callback to be
//    invoked (it's invalid to write to an output stream while it's flushing).
// 4. Check the GetState() again to ensure the Flush was successful. GetState()
//    should be EMPTY again.
// 5. Repeat.
class GCM_EXPORT SocketOutputStream
    : public google::protobuf::io::ZeroCopyOutputStream {
 public:
  enum State {
    // No valid data yet.
    EMPTY,
    // Ready for flushing (some data is present).
    READY,
    // In the process of flushing into the socket.
    FLUSHING,
    // A permanent error occurred, and the stream is now closed.
    CLOSED,
  };

  explicit SocketOutputStream(mojo::ScopedDataPipeProducerHandle stream);

  SocketOutputStream(const SocketOutputStream&) = delete;
  SocketOutputStream& operator=(const SocketOutputStream&) = delete;

  ~SocketOutputStream() override;

  // ZeroCopyOutputStream implementation.
  bool Next(void** data, int* size) override;
  void BackUp(int count) override;
  int64_t ByteCount() const override;

  // Writes the buffer into the Socket.
  net::Error Flush(base::OnceClosure callback);

  // Returns the last fatal error encountered. Only valid if GetState() ==
  // CLOSED. Note that All network read errors will be reported as
  // net::ERR_FAILED, because mojo data pipe doesn't allow surfacing a more
  // specific error code.
  net::Error last_error() const;

  // Returns the current state.
  State GetState() const;

 private:
  void WriteMore(MojoResult result, const mojo::HandleSignalsState& state);

  // Internal net components.
  mojo::ScopedDataPipeProducerHandle stream_;
  mojo::SimpleWatcher stream_watcher_;
  base::OnceClosure write_callback_;
  const scoped_refptr<net::IOBufferWithSize> io_buffer_;
  // IOBuffer implementation that wraps the data within |io_buffer_| that hasn't
  // been written to the socket yet.
  scoped_refptr<net::DrainableIOBuffer> write_buffer_;

  // Starting position of the data within |io_buffer_| to consume on subsequent
  // Next(..) call.  0 <= write_buffer_.BytesConsumed() <= next_pos_
  // Note: next_pos == 0 implies GetState() == EMPTY.
  int next_pos_;

  // If < net::ERR_IO_PENDING, the last net error received.
  // Note: last_error_ == net::ERR_IO_PENDING implies GetState() == FLUSHING.
  net::Error last_error_;

  base::WeakPtrFactory<SocketOutputStream> weak_ptr_factory_{this};
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_BASE_SOCKET_STREAM_H_
