// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/modules/mediastream/scoped_media_stream_tracer.h"

#include "base/trace_event/typed_macros.h"

namespace blink {

namespace {

constexpr char kMediaStreamTraceCategory[] = "mediastream";

}

// Uses `this` as a default id as most of them can be unique.
ScopedMediaStreamTracer::ScopedMediaStreamTracer(const String& event_name)
    : event_name_(event_name) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(kMediaStreamTraceCategory,
                                    event_name_.Utf8().c_str(), this);
}

ScopedMediaStreamTracer::~ScopedMediaStreamTracer() {
  End();
}

void ScopedMediaStreamTracer::End() {
  if (finished_) {
    return;
  }

  TRACE_EVENT_NESTABLE_ASYNC_END0(kMediaStreamTraceCategory,
                                  event_name_.Utf8().c_str(), this);
  finished_ = true;
}

}  // namespace blink
