// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/opened_frame_tracker.h"

#include "third_party/blink/renderer/core/frame/frame.h"

namespace blink {

OpenedFrameTracker::OpenedFrameTracker() = default;

OpenedFrameTracker::~OpenedFrameTracker() {
  DCHECK(IsEmpty());
}

void OpenedFrameTracker::Trace(Visitor* visitor) const {
  visitor->Trace(opened_frames_);
}

bool OpenedFrameTracker::IsEmpty() const {
  return opened_frames_.empty();
}

void OpenedFrameTracker::Add(Frame* frame) {
  opened_frames_.insert(frame);
}

void OpenedFrameTracker::Remove(Frame* frame) {
  opened_frames_.erase(frame);
}

void OpenedFrameTracker::TransferTo(Frame* opener) const {
  // Copy the set of opened frames, since changing the owner will mutate this
  // set.
  HeapHashSet<Member<Frame>> frames(opened_frames_);
  for (const auto& frame : frames)
    frame->SetOpenerDoNotNotify(opener);
}

void OpenedFrameTracker::Dispose() {
  TransferTo(nullptr);
  DCHECK(IsEmpty());
}

}  // namespace blink
