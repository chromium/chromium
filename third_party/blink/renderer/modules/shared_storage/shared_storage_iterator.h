// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_ITERATOR_H_

#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"

namespace blink {

class ExceptionState;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;
class SharedStorageWorkletGlobalScope;

class MODULES_EXPORT SharedStorageIterator final
    : public ScriptWrappable,
      public ActiveScriptWrappable<SharedStorageIterator>,
      public ExecutionContextClient,
      public mojom::blink::SharedStorageEntriesListener {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum class Mode {
    kKey,
    kKeyValue,
  };

  SharedStorageIterator(
      Mode mode,
      ExecutionContext* execution_context,
      mojom::blink::SharedStorageWorkletServiceClient* client);

  ~SharedStorageIterator() override;

  // ScriptWrappable
  bool HasPendingActivity() const override;

  // SharedStorageIterator IDL
  ScriptPromise next(ScriptState*, ExceptionState&);

  // mojom::blink::SharedStorageEntriesListener
  void DidReadEntries(
      bool success,
      const String& error_message,
      Vector<mojom::blink::SharedStorageKeyAndOrValuePtr> entries,
      bool has_more_entries,
      int total_queued_to_send) override;

  void Trace(Visitor*) const override;

 private:
  ScriptPromise NextHelper(ScriptPromiseResolver* resolver);

  // Checks if `value` meets `benchmark` percentage, for purposes of histogram
  // logging.
  bool MeetsBenchmark(int value, int benchmark);

  // Logs the elasped time for calls to `Next()` to a histogram.
  void LogElapsedTime();

  Mode mode_;

  // The error state can only be set once, when the first error is encountered
  // in the DidReadEntries() listener callback. In this state, no further
  // listener callbacks are expected, and the outstanding and future promises
  // will be rejected with the error message.
  bool has_error_ = false;
  String error_message_;

  // The entries that are received from the browser process but not yet used to
  // fulfill promises.
  Deque<mojom::blink::SharedStorageKeyAndOrValuePtr> pending_entries_;

  // The resolvers for promises that are not yet resolved.
  HeapDeque<Member<ScriptPromiseResolver>> pending_resolvers_;

  // True if we haven't got the browser process's signal for the last batch of
  // entries. After the state is set to false, no further DidReadEntries()
  // listener callbacks are expected.
  bool waiting_for_more_entries_ = true;

  // The total number of entries that the database has queued to send via this
  // iterator.
  int total_entries_queued_ = 0;

  // The number of entries that the iterator has received from the database so
  // far.
  int entries_received_ = 0;

  // The number of entries that the iterator has iterated through.
  int entries_iterated_ = 0;

  // The lowest benchmark for received entries that is currently unmet and so
  // has not been logged.
  int next_benchmark_for_receipt_ = 0;

  // The lowest benchmark for iterated entries that is currently unmet and so
  // has not been logged.
  int next_benchmark_for_iteration_;

  // Start times of each call to `next()`, in order of the calls. Used to record
  // a timing histogram.
  Deque<base::TimeTicks> next_start_times_;

  HeapMojoReceiver<mojom::blink::SharedStorageEntriesListener,
                   SharedStorageIterator>
      receiver_;

  Member<SharedStorageWorkletGlobalScope> global_scope_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_ITERATOR_H_
