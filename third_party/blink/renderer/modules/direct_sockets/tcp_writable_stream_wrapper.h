// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_TCP_WRITABLE_STREAM_WRAPPER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_TCP_WRITABLE_STREAM_WRAPPER_H_

#include "base/allocator/partition_allocator/partition_root.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace v8 {
class Isolate;
}

namespace blink {

class ScriptState;
class WritableStream;
class WritableStreamDefaultController;

// Helper class to write to a mojo producer handle
class MODULES_EXPORT TCPWritableStreamWrapper final
    : public GarbageCollected<TCPWritableStreamWrapper>,
      public ActiveScriptWrappable<TCPWritableStreamWrapper>,
      public ExecutionContextClient {
  USING_PRE_FINALIZER(TCPWritableStreamWrapper, Dispose);

 public:
  enum class State {
    kOpen,
    kAborted,
    kClosed,
  };

  TCPWritableStreamWrapper(ScriptState*,
                           base::OnceClosure on_abort,
                           mojo::ScopedDataPipeProducerHandle);
  ~TCPWritableStreamWrapper() override;

  WritableStream* Writable() const {
    DVLOG(1) << "TCPWritableStreamWrapper::writable() called";

    return writable_;
  }

  ScriptState* GetScriptState() { return script_state_; }

  void Reset();

  State GetState() const { return state_; }

  bool HasPendingActivity() const;

  void Trace(Visitor*) const override;

 private:
  class UnderlyingSink;

  // Called when |data_pipe_| becomes writable or errored.
  void OnHandleReady(MojoResult, const mojo::HandleSignalsState&);

  // Called when |data_pipe_| is closed.
  void OnPeerClosed(MojoResult, const mojo::HandleSignalsState&);

  // Implements UnderlyingSink::write().
  ScriptPromise SinkWrite(ScriptState*, ScriptValue chunk, ExceptionState&);

  // Writes |data| to |data_pipe_|, possible saving unwritten data to
  // |cached_data_|.
  ScriptPromise WriteOrCacheData(ScriptState*, base::span<const uint8_t> data);

  // Attempts to write some more of |cached_data_| to |data_pipe_|.
  void WriteCachedData();

  // Writes zero or more bytes of |data| synchronously to |data_pipe_|,
  // returning the number of bytes that were written.
  size_t WriteDataSynchronously(base::span<const uint8_t> data);

  // Creates a DOMException indicating that the stream has been aborted.
  ScriptValue CreateAbortException();

  // Errors |writable_|, resolves |writing_aborted_| and resets |data_pipe_|.
  void ErrorStreamAbortAndReset();

  // Reset the |data_pipe_|.
  void AbortAndReset();

  // Resets |data_pipe_| and clears the watchers. Also discards |cached_data_|.
  void ResetPipe();

  // Prepares the object for destruction.
  void Dispose();

  class CachedDataBuffer {
   public:
    CachedDataBuffer(v8::Isolate* isolate, const uint8_t* data, size_t length);

    ~CachedDataBuffer();

    size_t length() const { return length_; }

    uint8_t* data() { return buffer_.get(); }

   private:
    // We need the isolate to call |AdjustAmountOfExternalAllocatedMemory| for
    // the memory stored in |buffer_|.
    v8::Isolate* isolate_;
    size_t length_ = 0u;

    struct OnFree {
      void operator()(void* ptr) const {
        WTF::Partitions::BufferPartition()->Free(ptr);
      }
    };
    std::unique_ptr<uint8_t[], OnFree> buffer_;
  };

  const Member<ScriptState> script_state_;

  base::OnceClosure on_abort_;

  mojo::ScopedDataPipeProducerHandle data_pipe_;

  // Only armed when we need to write something.
  mojo::SimpleWatcher write_watcher_;

  // Always armed to detect close.
  mojo::SimpleWatcher close_watcher_;

  // Data which has been passed to write() but still needs to be written
  // asynchronously.
  // Uses a custom CachedDataBuffer rather than a Vector because
  // WTF::Vector is currently limited to 2GB.
  std::unique_ptr<CachedDataBuffer> cached_data_;

  // The offset into |cached_data_| of the first byte that still needs to be
  // written.
  size_t offset_ = 0;

  Member<WritableStream> writable_;
  Member<WritableStreamDefaultController> controller_;

  // If an asynchronous write() on the underlying sink object is pending, this
  // will be non-null.
  Member<ScriptPromiseResolver> write_promise_resolver_;

  State state_ = State::kOpen;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_TCP_WRITABLE_STREAM_WRAPPER_H_
