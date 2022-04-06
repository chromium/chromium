// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_TCP_READABLE_STREAM_WRAPPER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_TCP_READABLE_STREAM_WRAPPER_H_

#include "base/callback.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/modules/direct_sockets/stream_wrapper.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

// Helper class to read from a mojo consumer handle
class MODULES_EXPORT TCPReadableStreamWrapper
    : public GarbageCollected<TCPReadableStreamWrapper>,
      public ReadableStreamWrapper {
  USING_PRE_FINALIZER(TCPReadableStreamWrapper, Dispose);

 public:
  TCPReadableStreamWrapper(ScriptState*,
                           base::OnceCallback<void(bool)> on_close,
                           mojo::ScopedDataPipeConsumerHandle);

  void CloseSocket(bool error) override;
  void CloseStream(bool error) override;

  void Pull() override;
  bool Push(base::span<const uint8_t> data,
            const absl::optional<net::IPEndPoint>&) override;

  void Trace(Visitor*) const override;

 private:
  class TCPUnderlyingSource;

  // Called when |data_pipe_| becomes readable or errored.
  void OnHandleReady(MojoResult, const mojo::HandleSignalsState&);

  // Called when |data_pipe_| is closed.
  void OnPeerClosed(MojoResult, const mojo::HandleSignalsState&);

  // Errors or closes |readable_| and resets |data_pipe_|.
  void CloseOrErrorStreamAbortAndReset(bool error);

  // Resets |data_pipe_| and clears the watchers.
  void ResetPipe();

  // Prepares the object for destruction.
  void Dispose();

  base::OnceCallback<void(bool)> on_close_;

  mojo::ScopedDataPipeConsumerHandle data_pipe_;

  // Only armed when we need to read something.
  mojo::SimpleWatcher read_watcher_;

  // Always armed to detect close.
  mojo::SimpleWatcher close_watcher_;

  // Indicates if we are currently performing a two-phase read from the pipe and
  // so can't start another read.
  bool in_two_phase_read_ = false;

  // Indicates if we need to perform another read after the current one
  // completes.
  bool read_pending_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_TCP_READABLE_STREAM_WRAPPER_H_
