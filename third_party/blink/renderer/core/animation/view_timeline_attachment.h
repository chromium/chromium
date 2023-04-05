// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_VIEW_TIMELINE_ATTACHMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_VIEW_TIMELINE_ATTACHMENT_H_

#include "third_party/blink/renderer/core/animation/scroll_timeline_attachment.h"
#include "third_party/blink/renderer/core/animation/timeline_inset.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class Element;

class CORE_EXPORT ViewTimelineAttachment final
    : public ScrollTimelineAttachment {
 public:
  explicit ViewTimelineAttachment(Element* subject, ScrollAxis, TimelineInset);

  Type GetType() const override { return Type::kView; }

  const TimelineInset& GetInset() const { return inset_; }

 private:
  TimelineInset inset_;
};

template <>
struct DowncastTraits<ViewTimelineAttachment> {
  static bool AllowFrom(const ScrollTimelineAttachment& attachment) {
    return attachment.GetType() == ScrollTimelineAttachment::Type::kView;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_VIEW_TIMELINE_ATTACHMENT_H_
