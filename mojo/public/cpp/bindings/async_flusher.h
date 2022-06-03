// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ASYNC_FLUSHER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ASYNC_FLUSHER_H_

#include "base/component_export.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {

class PendingFlush;
class PipeControlMessageProxy;

// An object that can be consumed by |FlushAsync()| on a Remote or Receiver in
// order to perform an asynchronous flush operation on the object. Every
// AsyncFlusher is associated with a PendingFlush object which can be monitored
// or consumed to remotely observe completion of the corresponding flush
// operation.
//
// NOTE: Most commonly for asynchronous flush operations, |FlushAsync()| can
// be called on a Remote or Receiver with no arguments. This creates an
// AsyncFlusher/PendingFlush pair and immediately flushes the callee with the
// resulting AsyncFlusher. The entangled PendingFlush is returned for subsequent
// consumption.
//
// Direct use of AsyncFlusher (and in particular of the PendingFlush constructor
// which takes an AsyncFlusher* argument to initialize) is reserved for edge
// cases where a PendingFlush is needed before its corresponding flush operation
// can be initiated (e.g. when the interface to flush lives on a different
// thread from the interface that will wait on its PendingFlush).
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) AsyncFlusher {
 public:
  AsyncFlusher();
  AsyncFlusher(AsyncFlusher&&);
  AsyncFlusher(const AsyncFlusher&) = delete;
  AsyncFlusher& operator=(AsyncFlusher&&);
  AsyncFlusher& operator=(const AsyncFlusher&) = delete;
  ~AsyncFlusher();

 private:
  friend class PendingFlush;
  friend class PipeControlMessageProxy;

  void SetPipe(ScopedMessagePipeHandle pipe);
  ScopedMessagePipeHandle PassPipe();

  ScopedMessagePipeHandle pipe_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ASYNC_FLUSHER_H_
