// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/view_timeline.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_view_timeline.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_view_timeline_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"

namespace blink {

ViewTimeline* ViewTimeline::Create(Document& document,
                                   ViewTimelineOptions* options,
                                   ExceptionState& exception_state) {
  Element* subject = options->subject();

  ScrollDirection orientation;
  if (!StringToScrollDirection(options->axis(), orientation)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Invalid axis");
    return nullptr;
  }

  return MakeGarbageCollected<ViewTimeline>(&document, subject, orientation);
}

ViewTimeline::ViewTimeline(Document* document,
                           Element* subject,
                           ScrollDirection orientation)
    : ScrollTimeline(document,
                     ReferenceType::kNearestAncestor,
                     subject,
                     orientation) {}

}  // namespace blink
