// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_WEBNN_TRACE_H_
#define SERVICES_WEBNN_PUBLIC_CPP_WEBNN_TRACE_H_

#include <stdint.h>

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace webnn {

// The trace starts when an object of this class is created, and ends when
// the object goes out scope.
// You should 'std::move' this object when binding callbacks. The trace ends
// if the callback is destroyed (even if it's not run).
//
// Methods in this class are safe to be called from any thread.
class COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) ScopedTrace {
 public:
  // Create a ScopedTrace instance.
  //
  // Important note: Use literal strings only. See trace_event_common.h.
  explicit ScopedTrace(perfetto::StaticString name);
  ScopedTrace(ScopedTrace&& other);
  ScopedTrace& operator=(ScopedTrace&& other);
  ScopedTrace(const ScopedTrace&) = delete;
  ScopedTrace& operator=(const ScopedTrace&) = delete;
  ~ScopedTrace();

  // Starts a nested sub-trace in the current trace. The next AddStep() call
  // will mark the end of the previous sub-trace.
  void AddStep(perfetto::StaticString step_name);

  perfetto::Track track() const { return perfetto::Track(id_.value()); }

 private:
  ScopedTrace(perfetto::StaticString, uint64_t id);

  perfetto::StaticString name_;

  // The trace ID.
  //
  // An 'std::nullopt' means the trace has been transferred to another
  // 'ScopedTrace' object, and stops 'this''s destruction from ending the
  // trace.
  std::optional<uint64_t> id_;

  // The step name.
  //
  // An `std::nullopt` means that either the trace has been transferred to
  // another `ScopedTrace` object or there is no active sub-trace, and stops
  // `this`'s destruction from ending the sub-trace.
  std::optional<perfetto::StaticString> step_name_;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_WEBNN_TRACE_H_
