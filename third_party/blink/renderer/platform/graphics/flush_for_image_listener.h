// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_FLUSH_FOR_IMAGE_LISTENER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_FLUSH_FOR_IMAGE_LISTENER_H_

#include "base/observer_list.h"
#include "cc/paint/paint_image.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {

class FlushForImageObserver : public base::CheckedObserver {
 public:
  virtual void OnFlushForImage(cc::PaintImage::ContentId content_id) = 0;
};

class PLATFORM_EXPORT FlushForImageListener {
  // With deferred rendering it's possible for a drawImage operation on a canvas
  // to trigger a copy-on-write if another canvas has a read reference to it.
  // This can cause serious regressions due to extra allocations:
  // crbug.com/1030108. FlushForImageListener keeps a list of all active 2d
  // contexts on a thread and notifies them when one is attempting copy-on
  // write. If the notified context has a read reference to the canvas
  // attempting a copy-on-write it then flushes so as to make the copy-on-write
  // unnecessary.
 public:
  static FlushForImageListener* Get();

  void AddObserver(FlushForImageObserver* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(FlushForImageObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  void NotifyFlushForImage(cc::PaintImage::ContentId content_id) {
    for (FlushForImageObserver& obs : observers_) {
      obs.OnFlushForImage(content_id);
    }
  }

 private:
  friend class ThreadSpecific<FlushForImageListener>;
  base::ReentrantObserverList<FlushForImageObserver> observers_;
};

PLATFORM_EXPORT void NotifyImageBitmapWillTransfer(
    cc::PaintImage::ContentId content_id);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_FLUSH_FOR_IMAGE_LISTENER_H_
