// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SCOPED_ASYNC_TRACE_H_
#define MEDIA_BASE_SCOPED_ASYNC_TRACE_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "media/base/media_export.h"

namespace media {

enum class TraceCategory : uint32_t {
  kMedia,
  kMediaStream,
  kVideoAndImageCapture,
};

namespace {

template <TraceCategory category>
struct Category {};

template <>
struct Category<TraceCategory::kMedia> {
  static constexpr const char* Name() { return "media"; }
};

template <>
struct Category<TraceCategory::kMediaStream> {
  static constexpr const char* Name() {
    return TRACE_DISABLED_BY_DEFAULT("mediastream");
  }
};

template <>
struct Category<TraceCategory::kVideoAndImageCapture> {
  static constexpr const char* Name() {
    return TRACE_DISABLED_BY_DEFAULT("video_and_image_capture");
  }
};

}  // namespace

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
      const char* name) {
    bool enabled = false;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED(Category<category>::Name(), &enabled);
    return enabled ? base::WrapUnique(new TypedScopedAsyncTrace(name))
                   : nullptr;
  }

  ~TypedScopedAsyncTrace() {
    TRACE_EVENT_NESTABLE_ASYNC_END0(Category<category>::Name(), name_,
                                    TRACE_ID_LOCAL(id_));
  }

  // Adds a nested step under the current trace.
  void AddStep(const char* step_name) {
    step_.reset();  // Ensure previous trace step closes first.
    step_ = base::WrapUnique(new TypedScopedAsyncTrace(step_name, this));
  }

 private:
  explicit TypedScopedAsyncTrace(const char* name)
      : TypedScopedAsyncTrace(name, this) {}

  TypedScopedAsyncTrace(const char* name, const void* id)
      : name_(name), id_(id) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(Category<category>::Name(), name_,
                                      TRACE_ID_LOCAL(id_));
  }

  const char* name_;
  const void* id_;
  std::unique_ptr<TypedScopedAsyncTrace<category>> step_;
};

using ScopedAsyncTrace = TypedScopedAsyncTrace<TraceCategory::kMedia>;

}  // namespace media

#endif  // MEDIA_BASE_SCOPED_ASYNC_TRACE_H_
