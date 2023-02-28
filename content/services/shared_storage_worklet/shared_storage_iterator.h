// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_SHARED_STORAGE_WORKLET_SHARED_STORAGE_ITERATOR_H_
#define CONTENT_SERVICES_SHARED_STORAGE_WORKLET_SHARED_STORAGE_ITERATOR_H_

#include <deque>
#include <queue>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-persistent-handle.h"
#include "v8/include/v8-promise.h"

namespace shared_storage_worklet {

extern const int kSharedStorageIteratorBenchmarkStep;

// The async iterator type for sharedStorage.keys()/entries().
class SharedStorageIterator final
    : public gin::Wrappable<SharedStorageIterator>,
      public blink::mojom::SharedStorageEntriesListener {
 public:
  enum class Mode {
    kKey,
    kKeyValue,
  };

  SharedStorageIterator(
      Mode mode,
      blink::mojom::SharedStorageWorkletServiceClient* client);

  ~SharedStorageIterator() override;

  static gin::WrapperInfo kWrapperInfo;

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  const char* GetTypeName() override;

 private:
  v8::Local<v8::Object> GetThisObject(gin::Arguments* args);

  v8::Local<v8::Promise> Next(gin::Arguments* args);

  v8::Local<v8::Promise> NextHelper(v8::Isolate* isolate,
                                    v8::Local<v8::Promise::Resolver> resolver);

  // blink::mojom::SharedStorageEntriesListener
  void DidReadEntries(
      bool success,
      const std::string& error_message,
      std::vector<blink::mojom::SharedStorageKeyAndOrValuePtr> entries,
      bool has_more_entries,
      int total_queued_to_send) override;

  v8::Local<v8::Object> CreateIteratorResult(
      v8::Isolate* isolate,
      const blink::mojom::SharedStorageKeyAndOrValuePtr& entry);

  v8::Local<v8::Object> CreateIteratorResultDone(v8::Isolate* isolate);

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
  std::string error_message_;

  // The entries that are received from the browser process but not yet returned
  // as the promise-fulfilled-value.
  std::deque<blink::mojom::SharedStorageKeyAndOrValuePtr> pending_entries_;

  // The resolvers for promises that are not yet resolved.
  std::deque<v8::Global<v8::Promise::Resolver>> pending_resolvers_;

  // This isolate is owned by SharedStorageWorkletGlobalScope::isolate_holder_.
  raw_ptr<v8::Isolate> isolate_for_pending_resolvers_ = nullptr;

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
  int next_benchmark_for_iteration_ = kSharedStorageIteratorBenchmarkStep;

  // Start times of each call to `Next()`, in order of the call. Used to record
  // a timing histogram.
  std::queue<base::TimeTicks> next_start_times_;

  mojo::Receiver<blink::mojom::SharedStorageEntriesListener> receiver_{this};
};

}  // namespace shared_storage_worklet

#endif  // CONTENT_SERVICES_SHARED_STORAGE_WORKLET_SHARED_STORAGE_ITERATOR_H_
