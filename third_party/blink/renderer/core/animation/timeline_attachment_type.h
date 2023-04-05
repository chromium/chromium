// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMELINE_ATTACHMENT_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMELINE_ATTACHMENT_TYPE_H_

namespace blink {

enum class TimelineAttachmentType {
  // The timeline is not attached to another timeline, and other timelines
  // can not be attached to this timeline.
  kLocal,
  // The timeline can be attached to by descendant timelines with attachment
  // type kAncestor.
  kDefer,
  // The timeline is attached to an exclusive flat-tree ancestor with
  // attachment type kDefer.
  kAncestor
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMELINE_ATTACHMENT_TYPE_H_
