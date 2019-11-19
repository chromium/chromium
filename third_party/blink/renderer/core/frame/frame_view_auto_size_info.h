// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_VIEW_AUTO_SIZE_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_VIEW_AUTO_SIZE_INFO_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LocalFrameView;

class FrameViewAutoSizeInfo final
    : public GarbageCollected<FrameViewAutoSizeInfo> {
 public:
  explicit FrameViewAutoSizeInfo(LocalFrameView*);

  void ConfigureAutoSizeMode(const IntSize& min_size, const IntSize& max_size);
  void AutoSizeIfNeeded();

  void Trace(blink::Visitor*);

 private:
  Member<LocalFrameView> frame_view_;

  // The lower bound on the size when autosizing.
  IntSize min_auto_size_;
  // The upper bound on the size when autosizing.
  IntSize max_auto_size_;

  bool in_auto_size_;
  // True if autosize has been run since m_shouldAutoSize was set.
  bool did_run_autosize_;

  DISALLOW_COPY_AND_ASSIGN(FrameViewAutoSizeInfo);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_VIEW_AUTO_SIZE_INFO_H_
