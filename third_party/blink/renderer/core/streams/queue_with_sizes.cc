// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/queue_with_sizes.h"

#include <math.h>

#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

namespace {

// https://streams.spec.whatwg.org/#is-finite-non-negative-number
bool IsFiniteNonNegativeNumber(double v) {
  return isfinite(v) && v >= 0;
}

}  // namespace

class QueueWithSizes::ValueSizePair final
    : public GarbageCollected<ValueSizePair> {
 public:
  ValueSizePair(v8::Local<v8::Value> value, double size, v8::Isolate* isolate)
      : value_(isolate, value), size_(size) {}

  v8::Local<v8::Value> Value(v8::Isolate* isolate) {
    return value_.NewLocal(isolate);
  }

  double Size() { return size_; }

  void Trace(Visitor* visitor) { visitor->Trace(value_); }

 private:
  TraceWrapperV8Reference<v8::Value> value_;
  double size_;
};

QueueWithSizes::QueueWithSizes() = default;
QueueWithSizes::~QueueWithSizes() = default;

v8::Local<v8::Value> QueueWithSizes::DequeueValue(v8::Isolate* isolate) {
  DCHECK(!queue_.empty());
  // https://streams.spec.whatwg.org/#dequeue-value
  // 3. Let pair be the first element of container.[[queue]].
  const auto& pair = queue_.front();

  // 5. Set container.[[queueTotalSize]] to container.[[queueTotalSize]] −
  //    pair.[[size]].
  queue_total_size_ -= pair->Size();
  const auto value = pair->Value(isolate);

  // 4. Remove pair from container.[[queue]], shifting all other elements
  //    downward (so that the second becomes the first, and so on).
  queue_.pop_front();  // invalidates |pair|.

  // 6. If container.[[queueTotalSize]] < 0, set container.[[queueTotalSize]] to
  //    0. (This can occur due to rounding errors.)
  if (queue_total_size_ < 0) {
    queue_total_size_ = 0;
  }

  // 7. Return pair.[[value]].
  return value;
}

void QueueWithSizes::EnqueueValueWithSize(v8::Isolate* isolate,
                                          v8::Local<v8::Value> value,
                                          double size,
                                          ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#enqueue-value-with-size
  // 3. If ! IsFiniteNonNegativeNumber(size) is false, throw a RangeError
  //    exception.
  if (!IsFiniteNonNegativeNumber(size)) {
    exception_state.ThrowRangeError(
        "The return value of a queuing strategy's size function must be a "
        "finite, non-NaN, non-negative number");
    return;
  }

  // 4. Append Record {[[value]]: value, [[size]]: size} as the last element of
  //    container.[[queue]].
  queue_.push_back(MakeGarbageCollected<ValueSizePair>(value, size, isolate));

  // 5. Set container.[[queueTotalSize]] to container.[[queueTotalSize]] + size.
  queue_total_size_ += size;
}

v8::Local<v8::Value> QueueWithSizes::PeekQueueValue(v8::Isolate* isolate) {
  // https://streams.spec.whatwg.org/#peek-queue-value
  // 2. Assert: container.[[queue]] is not empty.
  DCHECK(!queue_.empty());

  // 3. Let pair be the first element of container.[[queue]].
  const auto& pair = queue_.front();

  // 4. Return pair.[[value]].
  return pair->Value(isolate);
}

void QueueWithSizes::ResetQueue() {
  // https://streams.spec.whatwg.org/#reset-queue
  // 2. Set container.[[queue]] to a new empty List.
  queue_.clear();

  // 3. Set container.[[queueTotalSize]] to 0.
  queue_total_size_ = 0;
}

void QueueWithSizes::Trace(Visitor* visitor) {
  visitor->Trace(queue_);
}

}  // namespace blink
