// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_BROKER_HOST_H_
#define MOJO_CORE_BROKER_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/files/scoped_file.h"
#include "base/message_loop/io_watcher.h"
#include "base/task/current_thread.h"
#include "mojo/core/broker_messages.h"
#include "mojo/core/system_impl_export.h"

namespace mojo::core {

// Services shared memory allocation requests from a client process's Broker
// over a dedicated socket, using the protocol in broker_messages.h.
//
// The client is untrusted: the kernel discards any file descriptors the client
// sends, and the BrokerHost closes the connection as soon as a response cannot
// be written without blocking. The client created the socket and can change its
// blocking mode at any time through a duplicate descriptor, so every read and
// write uses MSG_DONTWAIT.
//
// A BrokerHost must be created on an IO thread. It owns itself and is
// destroyed on that thread when the connection is closed, or when the thread's
// message loop is destroyed.
class MOJO_SYSTEM_IMPL_EXPORT BrokerHost final
    : public base::IOWatcher::FdWatcher,
      public base::CurrentThread::DestructionObserver {
 public:
  explicit BrokerHost(base::ScopedFD socket);

  BrokerHost(const BrokerHost&) = delete;
  BrokerHost& operator=(const BrokerHost&) = delete;

 private:
  ~BrokerHost() override;

  // base::IOWatcher::FdWatcher:
  void OnFdReadable(int fd) override;
  void OnFdWritable(int fd) override;

  // base::CurrentThread::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override;

  // Creates a region of `num_bytes` and sends it to the client. Returns false
  // if the response could not be sent.
  bool OnBufferRequest(uint32_t num_bytes);

  base::ScopedFD socket_;

  // Declared after `socket_` so that the watch is removed before the socket is
  // closed.
  std::unique_ptr<base::IOWatcher::FdWatch> read_watch_;

  // The request being received, and how many of its bytes have arrived.
  BrokerBufferRequest request_ = {};
  size_t request_bytes_received_ = 0;
};

}  // namespace mojo::core

#endif  // MOJO_CORE_BROKER_HOST_H_
