// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_LAZY_STACK_TRACE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_LAZY_STACK_TRACE_H_

#include "base/containers/span.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8-debug.h"

namespace v8 {
class Isolate;
}

namespace blink {

// A helper class that lazily captures and caches the V8 execution stack trace.
//
// This is designed to avoid the performance cost of walking the V8 stack
// multiple times if multiple observers or checks (e.g. ad tracking, extension
// tracking) run within the same synchronous call chain.
//
// NOTE: This class holds local V8 handles inside the stack frames, so it is
// STACK_ALLOCATED() and its lifetime MUST be strictly shorter than the active
// v8::HandleScope on the current thread. It cannot be stored on the heap.
class CORE_EXPORT LazyStackTrace {
  STACK_ALLOCATED();

 public:
  // Creates a new LazyStackTrace wrapper. No V8 stack walking occurs until
  // GetStack() is called.
  explicit LazyStackTrace(v8::Isolate* isolate);
  LazyStackTrace(const LazyStackTrace&) = delete;
  LazyStackTrace& operator=(const LazyStackTrace&) = delete;

  // Move constructor is supported to allow returning by value from functions.
  LazyStackTrace(LazyStackTrace&&) noexcept;
  LazyStackTrace& operator=(LazyStackTrace&&) = delete;

  // Returns a span containing up to `limit` frames of the current V8 stack
  // trace.
  //
  // Captures the stack trace on the first call. If a subsequent call requests a
  // depth that is already cached (i.e. limit <= cached depth), it returns a
  // subspan of the cached trace without invoking another V8 stack walk. If the
  // requested limit is larger than the cached depth, it will perform a new walk
  // to capture the deeper stack.
  base::span<const v8::StackTrace::ScriptData> GetStack(size_t limit);

 private:
  v8::Isolate* isolate_;
  Vector<v8::StackTrace::ScriptData, 5> cached_stack_;
  size_t max_walked_limit_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_LAZY_STACK_TRACE_H_
