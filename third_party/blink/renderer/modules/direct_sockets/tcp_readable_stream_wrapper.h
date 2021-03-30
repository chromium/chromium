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
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class ScriptState;
class ReadableStream;
class ReadableStreamDefaultControllerWithScriptScope;

// Helper class to read from a mojo consumer handle
class MODULES_EXPORT TCPReadableStreamWrapper final
    : public GarbageCollected<TCPReadableStreamWrapper> {
  USING_PRE_FINALIZER(TCPReadableStreamWrapper, Dispose);

 public:
  enum class State {
    kOpen,
    kAborted,
    kClosed,
  };

  TCPReadableStreamWrapper(ScriptState*,
                           base::OnceClosure on_abort,
                           mojo::ScopedDataPipeConsumerHandle);
  ~TCPReadableStreamWrapper();

  ReadableStream* Readable() const {
    DVLOG(1) << "TCPReadableStreamWrapper::readable() called";

    return readable_;
  }

  ScriptState* GetScriptState() { return script_state_; }

  void Reset();

  State GetState() const { return state_; }

  void Trace(Visitor*) const;

 private:
  class UnderlyingSource;

  // Called when |data_pipe_| becomes readable or errored.
  void OnHandleReady(MojoResult, const mojo::HandleSignalsState&);

  // Called when |data_pipe_| is closed.
  void OnPeerClosed(MojoResult, const mojo::HandleSignalsState&);

  // Reads all the data currently in the pipe and enqueues it. If no data is
  // currently available, triggers the |read_watcher_| and enqueues when data
  // becomes available.
  void ReadFromPipeAndEnqueue();

  // Copies a sequence of bytes into an ArrayBuffer and enqueues it.
  void EnqueueBytes(const void* source, uint32_t byte_length);

  // Creates a DOMException indicating that the stream has been aborted.
  ScriptValue CreateAbortException();

  // Errors |readable_| and resets |data_pipe_|.
  void ErrorStreamAbortAndReset();

  // Resets the |data_pipe_|.
  void AbortAndReset();

  // Resets |data_pipe_| and clears the watchers.
  void ResetPipe();

  // Prepares the object for destruction.
  void Dispose();

  const Member<ScriptState> script_state_;

  base::OnceClosure on_abort_;

  mojo::ScopedDataPipeConsumerHandle data_pipe_;

  // Only armed when we need to read something.
  mojo::SimpleWatcher read_watcher_;

  // Always armed to detect close.
  mojo::SimpleWatcher close_watcher_;

  Member<ReadableStream> readable_;
  Member<ReadableStreamDefaultControllerWithScriptScope> controller_;

  State state_ = State::kOpen;

  // Indicates if we are currently performing a two-phase read from the pipe and
  // so can't start another read.
  bool in_two_phase_read_ = false;

  // Indicates if we need to perform another read after the current one
  // completes.
  bool read_pending_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_TCP_READABLE_STREAM_WRAPPER_H_
