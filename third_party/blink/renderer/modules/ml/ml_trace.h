// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_TRACE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_TRACE_H_

#include <memory>
#include <optional>

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

// The trace starts when an object of this class is created, and ends when
// the object goes out scope.
// You should 'std::move' this object when binding callbacks. The trace ends
// if the callback is destroyed (even if it's not run).
//
// Methods in this class is safe to be called from any threads.
class MODULES_EXPORT ScopedMLTrace {
 public:
  // Create a ScopedAsyncTrace instance.
  //
  // Important note: Use literal strings only. See trace_event_common.h.
  explicit ScopedMLTrace(const char* name);
  ScopedMLTrace(ScopedMLTrace&& other);
  ScopedMLTrace& operator=(ScopedMLTrace&& other);
  ScopedMLTrace(const ScopedMLTrace&) = delete;
  ScopedMLTrace& operator=(const ScopedMLTrace&) = delete;
  ~ScopedMLTrace();

  // Starts a nested sub-trace in the current trace. The next AddStep() call
  // will mark the end of the previous sub-trace.
  void AddStep(const char* step_name);

 private:
  ScopedMLTrace(const char* name, uint64_t id);

  const char* name_;

  // The trace ID.
  //
  // An 'std::nullopt' means the trace has been transferred to another
  // 'ScopedMLTrace' object, and stops 'this''s destruction from ending the
  // trace.
  std::optional<uint64_t> id_;
  std::unique_ptr<ScopedMLTrace> step_;
};

}  // namespace blink

namespace WTF {

template <>
struct CrossThreadCopier<blink::ScopedMLTrace>
    : public WTF::CrossThreadCopierByValuePassThrough<blink::ScopedMLTrace> {};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_TRACE_H_
