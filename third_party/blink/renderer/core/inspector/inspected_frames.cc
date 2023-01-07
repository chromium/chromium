// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspected_frames.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

InspectedFrames::InspectedFrames(LocalFrame* root) : root_(root) {}

InspectedFrames::Iterator InspectedFrames::begin() {
  return Iterator(root_, root_);
}

InspectedFrames::Iterator InspectedFrames::end() {
  return Iterator(root_, nullptr);
}

bool InspectedFrames::Contains(LocalFrame* frame) const {
  return frame->GetProbeSink() == root_->GetProbeSink();
}

LocalFrame* InspectedFrames::FrameWithSecurityOrigin(
    const String& origin_raw_string) {
  for (LocalFrame* frame : *this) {
    if (frame->DomWindow()->GetSecurityOrigin()->ToRawString() ==
        origin_raw_string)
      return frame;
  }
  return nullptr;
}

LocalFrame* InspectedFrames::FrameWithStorageKey(const String& key_raw_string) {
  for (LocalFrame* frame : *this) {
    if (static_cast<StorageKey>(frame->DomWindow()->GetStorageKey())
            .Serialize() == key_raw_string.Utf8()) {
      return frame;
    }
  }
  return nullptr;
}

InspectedFrames::Iterator::Iterator(LocalFrame* root, LocalFrame* current)
    : root_(root), current_(current) {}

InspectedFrames::Iterator& InspectedFrames::Iterator::operator++() {
  if (!current_)
    return *this;
  Frame* frame = current_->Tree().TraverseNext(root_);
  current_ = nullptr;
  for (; frame; frame = frame->Tree().TraverseNext(root_)) {
    auto* local = DynamicTo<LocalFrame>(frame);
    if (!local)
      continue;
    if (local->GetProbeSink() == root_->GetProbeSink()) {
      current_ = local;
      break;
    }
  }
  return *this;
}

InspectedFrames::Iterator InspectedFrames::Iterator::operator++(int) {
  LocalFrame* old = current_;
  ++*this;
  return Iterator(root_, old);
}

bool InspectedFrames::Iterator::operator==(const Iterator& other) const {
  return current_ == other.current_ && root_ == other.root_;
}

bool InspectedFrames::Iterator::operator!=(const Iterator& other) const {
  return !(*this == other);
}

void InspectedFrames::Trace(Visitor* visitor) const {
  visitor->Trace(root_);
}

}  // namespace blink
