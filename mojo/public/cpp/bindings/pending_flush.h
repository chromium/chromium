// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_PENDING_FLUSH_H_
#define MOJO_PUBLIC_CPP_BINDINGS_PENDING_FLUSH_H_

#include "base/component_export.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {

class AsyncFlusher;

// A PendingFlush represents an asynchronous flush operation on an arbitrary
// (and potentially remote) interface pipe. This is generally used to allow
// another pipe in the system to pause its own message processing until the
// original pipe has been flushed. As such, it's a useful primitive for
// arbitrarily complex synchronization operations across the system.
//
// The most common way to create a PendingFlush is to call |FlushAsync()| with
// arguments on a Remote or Receiver. For example, consider a storage API with a
// central control interface as well as multiple independent writers:
//
//     Remote<mojom::Storage> storage = ...;
//     Remote<mojom::Writer> writer1, writer2;
//     storage->GetWriter(writer1.BindNewPipeAndPassReceiver());
//     storage->GetWriter(writer2.BindNewPipeAndPassReceiver());
//
// Suppose we want to issue some commands on each Writer, followed by a query on
// the remote Storage object; but we want to ensure that the Storage query is
// not dispatched until all previous Writer operations are dispatched. We could
// write something like:
//
//     writer1->Put(...);
//     storage.PauseReceiverUntilFlushCompletes(writer1.FlushAsync());
//     writer2->Put(...);
//     storage.PauseReceiverUntilFlushCompletes(writer2.FlushAsync());
//     storage->Query(...);
//
// This effectively guarantees that the |Query()| call will never dispatch on
// the Storage receiver before both |Put()| calls have dispatched on their
// respective Writer receivers. This holds even if the receiving endpoints are
// all in different processes.
//
// Note that |FlushAsync()| returns a PendingFlush object. For some use cases,
// it may be desirable to create a PendingFlush before issuing a corresponding
// |FlushAsync()| call. In that case, use the single-argument constructor
// defined below.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) PendingFlush {
 public:
  // Constructs a new PendingFlush associated with |*flusher|. |*flusher| should
  // be a default-constructed AsyncFlusher, and once it is initialized by this
  // constructor it should be used to flush some Remote or Receiver using their
  // |FlushAsyncWithFlusher()| method.
  explicit PendingFlush(AsyncFlusher* flusher);
  PendingFlush(PendingFlush&&);
  PendingFlush(const PendingFlush&) = delete;
  PendingFlush& operator=(PendingFlush&&);
  PendingFlush& operator=(const PendingFlush&) = delete;
  ~PendingFlush();

 private:
  friend class PipeControlMessageProxy;

  ScopedMessagePipeHandle PassPipe();

  ScopedMessagePipeHandle pipe_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_PENDING_FLUSH_H_
