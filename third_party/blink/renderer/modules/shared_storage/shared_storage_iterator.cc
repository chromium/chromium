// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage_iterator.h"

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/iterable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

constexpr int kSharedStorageIteratorBenchmarkStep = 10;

v8::Local<v8::Object> CreateIteratorResultNotDone(
    ScriptState* script_state,
    SharedStorageIterator::Mode mode,
    const mojom::blink::SharedStorageKeyAndOrValuePtr& entry) {
  switch (mode) {
    case SharedStorageIterator::Mode::kKey:
      return bindings::ESCreateIterResultObject(
          script_state, /*done=*/false,
          ToV8Traits<IDLString>::ToV8(script_state, entry->key)
              .ToLocalChecked());
    case SharedStorageIterator::Mode::kKeyValue:
      return bindings::ESCreateIterResultObject(
          script_state, /*done=*/false,
          ToV8Traits<IDLString>::ToV8(script_state, entry->key)
              .ToLocalChecked(),
          ToV8Traits<IDLString>::ToV8(script_state, entry->value)
              .ToLocalChecked());
  }

  NOTREACHED_NORETURN();
}

v8::Local<v8::Object> CreateIteratorResultDone(ScriptState* script_state) {
  return bindings::ESCreateIterResultObject(
      script_state, /*done=*/true, v8::Undefined(script_state->GetIsolate()));
}

}  // namespace

SharedStorageIterator::SharedStorageIterator(
    Mode mode,
    ExecutionContext* execution_context,
    mojom::blink::SharedStorageWorkletServiceClient* client)
    : ActiveScriptWrappable<SharedStorageIterator>({}),
      ExecutionContextClient(execution_context),
      mode_(mode),
      next_benchmark_for_iteration_(kSharedStorageIteratorBenchmarkStep),
      receiver_(this, execution_context),
      global_scope_(To<SharedStorageWorkletGlobalScope>(execution_context)) {
  base::UmaHistogramExactLinear(
      "Storage.SharedStorage.AsyncIterator.IteratedEntriesBenchmarks", 0, 101);

  switch (mode_) {
    case Mode::kKey:
      client->SharedStorageKeys(receiver_.BindNewPipeAndPassRemote(
          execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
      break;
    case Mode::kKeyValue:
      client->SharedStorageEntries(receiver_.BindNewPipeAndPassRemote(
          execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
      break;
  }
}

SharedStorageIterator::~SharedStorageIterator() = default;

bool SharedStorageIterator::HasPendingActivity() const {
  return waiting_for_more_entries_ || !pending_resolvers_.empty();
}

ScriptPromise SharedStorageIterator::next(ScriptState* script_state,
                                          ExceptionState& exception_state) {
  next_start_times_.emplace_back(base::TimeTicks::Now());

  ScriptPromiseResolver* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  return NextHelper(resolver);
}

void SharedStorageIterator::DidReadEntries(
    bool success,
    const String& error_message,
    Vector<mojom::blink::SharedStorageKeyAndOrValuePtr> entries,
    bool has_more_entries,
    int total_queued_to_send) {
  CHECK(waiting_for_more_entries_);
  CHECK(!has_error_);
  CHECK(!(success && entries.empty() && has_more_entries));

  if (!success) {
    has_error_ = true;
    error_message_ = error_message;
  }

  if (!total_entries_queued_) {
    total_entries_queued_ = total_queued_to_send;
    base::UmaHistogramCounts10000(
        "Storage.SharedStorage.AsyncIterator.EntriesQueuedCount",
        total_entries_queued_);
  }

  base::CheckedNumeric<int> count = entries_received_;
  count += entries.size();
  entries_received_ = count.ValueOrDie();

  while (next_benchmark_for_receipt_ <= 100 &&
         MeetsBenchmark(entries_received_, next_benchmark_for_receipt_)) {
    base::UmaHistogramExactLinear(
        "Storage.SharedStorage.AsyncIterator.ReceivedEntriesBenchmarks",
        next_benchmark_for_receipt_, 101);
    next_benchmark_for_receipt_ += kSharedStorageIteratorBenchmarkStep;
  }

  for (auto& entry : entries) {
    pending_entries_.emplace_back(std::move(entry));
  }

  waiting_for_more_entries_ = has_more_entries;

  while (
      !pending_resolvers_.empty() &&
      (!pending_entries_.empty() || has_error_ || !waiting_for_more_entries_)) {
    ScriptPromiseResolver* resolver = pending_resolvers_.TakeFirst();

    ScriptState::Scope scope(resolver->GetScriptState());
    NextHelper(resolver);
  }
}

void SharedStorageIterator::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(receiver_);
  visitor->Trace(pending_resolvers_);
  visitor->Trace(global_scope_);
}

ScriptPromise SharedStorageIterator::NextHelper(
    ScriptPromiseResolver* resolver) {
  ScriptState* script_state = resolver->GetScriptState();
  ScriptPromise promise = resolver->Promise();

  if (has_error_) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kOperationError,
        error_message_));

    // We only record timing histograms when there is no error. Discard the
    // start time for this call.
    DCHECK(!next_start_times_.empty());
    next_start_times_.pop_front();

    return promise;
  }

  if (!pending_entries_.empty()) {
    mojom::blink::SharedStorageKeyAndOrValuePtr next_entry =
        pending_entries_.TakeFirst();

    resolver->Resolve(ScriptValue::From(
        script_state,
        CreateIteratorResultNotDone(script_state, mode_, next_entry)));

    base::CheckedNumeric<int> count = entries_iterated_;
    entries_iterated_ = (++count).ValueOrDie();

    while (next_benchmark_for_iteration_ <= 100 &&
           MeetsBenchmark(entries_iterated_, next_benchmark_for_iteration_)) {
      base::UmaHistogramExactLinear(
          "Storage.SharedStorage.AsyncIterator.IteratedEntriesBenchmarks",
          next_benchmark_for_iteration_, 101);
      next_benchmark_for_iteration_ += kSharedStorageIteratorBenchmarkStep;
    }

    LogElapsedTime();
    return promise;
  }

  if (waiting_for_more_entries_) {
    pending_resolvers_.emplace_back(resolver);
    return promise;
  }

  CHECK(pending_resolvers_.empty());

  resolver->Resolve(
      ScriptValue::From(script_state, CreateIteratorResultDone(script_state)));

  LogElapsedTime();
  return promise;
}

bool SharedStorageIterator::MeetsBenchmark(int value, int benchmark) {
  CHECK_GE(benchmark, 0);
  CHECK_LE(benchmark, 100);
  CHECK_EQ(benchmark % kSharedStorageIteratorBenchmarkStep, 0);
  CHECK_GE(total_entries_queued_, 0);

  if (benchmark == 0 || (total_entries_queued_ == 0 && value == 0)) {
    return true;
  }

  CHECK_GT(total_entries_queued_, 0);
  return (100 * value) / total_entries_queued_ >= benchmark;
}

void SharedStorageIterator::LogElapsedTime() {
  CHECK(!next_start_times_.empty());
  base::TimeDelta elapsed_time =
      base::TimeTicks::Now() - next_start_times_.TakeFirst();
  switch (mode_) {
    case SharedStorageIterator::Mode::kKey:
      base::UmaHistogramMediumTimes(
          "Storage.SharedStorage.Worklet.Timing.Keys.Next", elapsed_time);
      break;
    case SharedStorageIterator::Mode::kKeyValue:
      base::UmaHistogramMediumTimes(
          "Storage.SharedStorage.Worklet.Timing.Entries.Next", elapsed_time);
      break;
  }
}

}  // namespace blink
