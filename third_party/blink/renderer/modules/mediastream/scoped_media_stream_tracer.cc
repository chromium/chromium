// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/modules/mediastream/scoped_media_stream_tracer.h"

#include "base/trace_event/typed_macros.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace blink {

namespace {

constexpr char kMediaStreamTraceCategory[] = "mediastream";

}

// Uses `this` as a default id as most of them can be unique.
ScopedMediaStreamTracer::ScopedMediaStreamTracer(const String& event_name)
    : event_name_(event_name) {
  TRACE_EVENT_BEGIN(kMediaStreamTraceCategory,
                    perfetto::DynamicString(event_name_.Utf8().c_str()),
                    perfetto::Track::FromPointer(this));
}

ScopedMediaStreamTracer::~ScopedMediaStreamTracer() {
  End();
}

void ScopedMediaStreamTracer::End() {
  if (finished_) {
    return;
  }

  TRACE_EVENT_END(kMediaStreamTraceCategory,
                  perfetto::Track::FromPointer(this));
  finished_ = true;
}

}  // namespace blink
