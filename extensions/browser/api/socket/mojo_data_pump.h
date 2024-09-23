// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SOCKET_MOJO_DATA_PUMP_H_
#define EXTENSIONS_BROWSER_API_SOCKET_MOJO_DATA_PUMP_H_

#include "base/memory/scoped_refptr.h"
#include "extensions/browser/api/socket/socket.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/completion_once_callback.h"

namespace net {
class IOBuffer;
}

namespace extensions {

// Helper class to read from a mojo consumer handle and write to mojo producer
// handle.
class MojoDataPump {
 public:
  using ReadCallback =
      base::OnceCallback<void(int, scoped_refptr<net::IOBuffer> io_buffer)>;

  MojoDataPump(mojo::ScopedDataPipeConsumerHandle receive_stream,
               mojo::ScopedDataPipeProducerHandle send_stream);

  MojoDataPump(const MojoDataPump&) = delete;
  MojoDataPump& operator=(const MojoDataPump&) = delete;

  ~MojoDataPump();

  // Reads from |receive_stream|. It's illegal to call Read() when a previous
  // one hasn't completed yet.
  void Read(int count, ReadCallback callback);

  // Writes to |send_stream|. It's illegal to call Write() when a previous one
  // hasn't completed yet.
  void Write(net::IOBuffer* io_buffer,
             int io_buffer_size,
             net::CompletionOnceCallback callback);

  // Returns whether a read is pending.
  bool HasPendingRead() const { return !read_callback_.is_null(); }

  // Returns whether a write is pending.
  bool HasPendingWrite() const { return !write_callback_.is_null(); }

 private:
  void OnReadComplete(scoped_refptr<net::IOBuffer> io_buffer, int result);
  void StartWatching();
  void ReceiveMore(MojoResult result, const mojo::HandleSignalsState& state);
  void SendMore(MojoResult result, const mojo::HandleSignalsState& state);

  mojo::ScopedDataPipeConsumerHandle receive_stream_;
  mojo::SimpleWatcher receive_stream_watcher_;
  mojo::ScopedDataPipeProducerHandle send_stream_;
  mojo::SimpleWatcher send_stream_watcher_;

  ReadCallback read_callback_;
  net::CompletionOnceCallback write_callback_;
  scoped_refptr<net::IOBuffer> pending_write_buffer_;
  int pending_write_buffer_size_ = 0;
  size_t read_size_ = 0;
};

}  //  namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SOCKET_MOJO_DATA_PUMP_H_
