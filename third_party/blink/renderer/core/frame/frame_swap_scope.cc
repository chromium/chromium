// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/frame_swap_scope.h"

#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"

namespace blink {

FrameSwapScope::FrameSwapScope(FrameOwner* frame_owner) {
  // This scope is inert if frame_owner is already in the middle of a swap.
  // This can happen during FrameLoader::CommitNavigation if an event listener
  // removes and reinserts the <iframe> that is already committing, triggering
  // RenderFrameImpl::SynchronouslyCommitAboutBlankForBug778318.
  //
  // Note: We do not use a static counter, since a nested FrameSwapScope with
  // a different frame_owner should not be inert.
  auto* owner_element = DynamicTo<HTMLFrameOwnerElement>(frame_owner);
  if (owner_element && !owner_element->IsSwappingFrames()) {
    owner_element_ = owner_element;
    owner_element->SetIsSwappingFrames(true);
  }
}

FrameSwapScope::~FrameSwapScope() {
  if (owner_element_) {
    owner_element_->SetIsSwappingFrames(false);
    owner_element_->FrameOwnerPropertiesChanged();
    owner_element_->DidChangeAttributes();
  }
}

}  // namespace blink
