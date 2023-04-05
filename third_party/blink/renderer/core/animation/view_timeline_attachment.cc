// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/view_timeline_attachment.h"

namespace blink {

ViewTimelineAttachment::ViewTimelineAttachment(Element* subject,
                                               ScrollAxis axis,
                                               TimelineInset inset)
    : ScrollTimelineAttachment(ReferenceType::kNearestAncestor,
                               /* reference_element */ subject,
                               axis),
      inset_(inset) {}

}  // namespace blink
