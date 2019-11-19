// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_OPENED_FRAME_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_OPENED_FRAME_TRACKER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class WebFrame;

// Small helper class to track the set of frames that a WebFrame has opened.
// Due to layering restrictions, we need to hide the implementation, since
// public/web/ cannot depend on wtf/.
class OpenedFrameTracker {
  USING_FAST_MALLOC(OpenedFrameTracker);

 public:
  OpenedFrameTracker();
  ~OpenedFrameTracker();

  bool IsEmpty() const;
  void Add(WebFrame*);
  void Remove(WebFrame*);

  // Helper used when swapping a frame into the frame tree: this updates the
  // opener for opened frames to point to the new frame being swapped in.
  void TransferTo(WebFrame*);

  // Helper function to clear the openers when the frame is being detached.
  void Dispose() { TransferTo(nullptr); }

 private:
  WTF::HashSet<WebFrame*> opened_frames_;

  DISALLOW_COPY_AND_ASSIGN(OpenedFrameTracker);
};

}  // namespace blink

#endif  // WebFramePrivate_h
