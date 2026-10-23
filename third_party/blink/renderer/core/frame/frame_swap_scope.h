// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_SWAP_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_SWAP_SCOPE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class FrameOwner;
class HTMLFrameOwnerElement;

// The purpose of this class is to suppress the propagation of frame owner
// properties while a frame is being replaced. In particular, it prevents the
// erroneous propagation of is_display_none=true, which would otherwise happen
// when the old frame is detached prior to attaching the new frame. This class
// will postpone the propagation until the properties are in their new stable
// state.
//
// It is only intended to handle cases where one frame is detached and a new
// frame immediately attached. For normal frame unload/teardown, we don't need
// to suppress the propagation.
//
// Only HTMLFrameOwnerElement propagates frame owner properties. If frame_owner
// is a RemoteFrameOwner, then the FrameSwapScope does nothing.
class CORE_EXPORT FrameSwapScope {
  STACK_ALLOCATED();

 public:
  explicit FrameSwapScope(FrameOwner* frame_owner);
  ~FrameSwapScope();

 private:
  HTMLFrameOwnerElement* owner_element_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_SWAP_SCOPE_H_
