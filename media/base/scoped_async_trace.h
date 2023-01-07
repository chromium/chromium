// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SCOPED_ASYNC_TRACE_H_
#define MEDIA_BASE_SCOPED_ASYNC_TRACE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "media/base/media_export.h"

namespace media {

// When adding a new TraceCategory there are two additional steps in the .cc:
//
//   1. Add a new Category::Name() implementation at the top.
//   2. Add a new  TypedScopedAsyncTrace<$NEW_CATEGORY> entry at the bottom.
//
// For frequently used scoped traces you may also add a "using" entry along with
// the "using ScopedAsyncTrace..." entry below.
enum class TraceCategory : uint32_t {
  kMedia,
  kMediaStream,
  kVideoAndImageCapture,
};

// Utility class that starts and stops an async trace event.  The intention is
// that it it will be created somewhere to start the trace event, passed around
// such as via unique_ptr argument in a callback, and eventually freed to end
// the trace event.  This guarantees that it'll be closed, even if the callback
// is destroyed without being run.
template <TraceCategory category>
class MEDIA_EXPORT TypedScopedAsyncTrace {
 public:
  // Create a TypedScopedAsyncTrace if tracing for `cateogory` is enabled,
  // returns nullptr otherwise. `name` will be the trace name.
  //
  // IMPORTANT: All string parameters must outlive |this|, since tracing needs
  // them. Use literal strings only; see trace_event_common.h.
  static std::unique_ptr<TypedScopedAsyncTrace<category>> CreateIfEnabled(
      const char* name);

  ~TypedScopedAsyncTrace();

  // Adds a nested step under the current trace.
  void AddStep(const char* step_name);

 private:
  explicit TypedScopedAsyncTrace(const char* name);
  TypedScopedAsyncTrace(const char* name, const void* id);

  const char* name_;
  raw_ptr<const void> id_;
  std::unique_ptr<TypedScopedAsyncTrace<category>> step_;
};

using ScopedAsyncTrace = TypedScopedAsyncTrace<TraceCategory::kMedia>;

}  // namespace media

#endif  // MEDIA_BASE_SCOPED_ASYNC_TRACE_H_
