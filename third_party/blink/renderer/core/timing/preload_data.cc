// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/preload_data.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_cross_origin_mode.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

PreloadData::PreloadData(const KURL& url,
                         const String& as,
                         CrossOriginAttributeValue crossorigin,
                         std::optional<base::TimeTicks> used_time)
    : url_(url), as_(as), crossorigin_(crossorigin), used_time_(used_time) {}

V8CrossOriginMode PreloadData::crossorigin() const {
  switch (crossorigin_) {
    case kCrossOriginAttributeNotSet:
      return V8CrossOriginMode(V8CrossOriginMode::Enum::kNone);
    case kCrossOriginAttributeAnonymous:
      return V8CrossOriginMode(V8CrossOriginMode::Enum::kAnonymous);
    case kCrossOriginAttributeUseCredentials:
      return V8CrossOriginMode(V8CrossOriginMode::Enum::kUseCredentials);
  }
}

std::optional<double> PreloadData::used(ScriptState* script_state) const {
  if (!used_time_.has_value()) {
    return std::nullopt;
  }
  auto* window = LocalDOMWindow::From(script_state);
  if (!window) {
    return std::nullopt;
  }
  return DOMWindowPerformance::performance(*window)
      ->MonotonicTimeToDOMHighResTimeStamp(used_time_.value());
}

void PreloadData::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
