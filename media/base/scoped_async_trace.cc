// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/scoped_async_trace.h"

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"

namespace media {

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

template <TraceCategory category>
std::unique_ptr<TypedScopedAsyncTrace<category>>
TypedScopedAsyncTrace<category>::CreateIfEnabled(const char* name) {
  bool enabled = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(Category<category>::Name(), &enabled);
  return enabled ? base::WrapUnique(new TypedScopedAsyncTrace<category>(name))
                 : nullptr;
}

template <TraceCategory category>
TypedScopedAsyncTrace<category>::~TypedScopedAsyncTrace() {
  TRACE_EVENT_NESTABLE_ASYNC_END0(Category<category>::Name(), name_,
                                  TRACE_ID_LOCAL(id_));
}

template <TraceCategory category>
void TypedScopedAsyncTrace<category>::AddStep(const char* step_name) {
  step_.reset();  // Ensure previous trace step closes first.
  step_ = base::WrapUnique(new TypedScopedAsyncTrace(step_name, this));
}

template <TraceCategory category>
TypedScopedAsyncTrace<category>::TypedScopedAsyncTrace(const char* name)
    : TypedScopedAsyncTrace<category>(name, this) {}

template <TraceCategory category>
TypedScopedAsyncTrace<category>::TypedScopedAsyncTrace(const char* name,
                                                       const void* id)
    : name_(name), id_(id) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(Category<category>::Name(), name_,
                                    TRACE_ID_LOCAL(id_));
}

template class MEDIA_EXPORT TypedScopedAsyncTrace<TraceCategory::kMedia>;
template class MEDIA_EXPORT TypedScopedAsyncTrace<TraceCategory::kMediaStream>;
template class MEDIA_EXPORT
    TypedScopedAsyncTrace<TraceCategory::kVideoAndImageCapture>;

}  // namespace media
