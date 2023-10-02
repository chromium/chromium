// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SCROLL_STATE_CALLBACK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SCROLL_STATE_CALLBACK_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class ScrollState;

enum class NativeScrollBehavior {
  kDisableNativeScroll,
  kPerformBeforeNativeScroll,
  kPerformAfterNativeScroll,
};

// TODO(crbug.com/1369739): Remove this class.
class ScrollStateCallback : public GarbageCollected<ScrollStateCallback> {
 public:
  virtual ~ScrollStateCallback() = default;

  virtual void Trace(Visitor* visitor) const {}

  virtual void Invoke(ScrollState*) = 0;

  NativeScrollBehavior GetNativeScrollBehavior() const {
    return native_scroll_behavior_;
  }

 protected:
  explicit ScrollStateCallback(
      enum NativeScrollBehavior native_scroll_behavior =
          NativeScrollBehavior::kDisableNativeScroll)
      : native_scroll_behavior_(native_scroll_behavior) {}

 private:
  const enum NativeScrollBehavior native_scroll_behavior_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SCROLL_STATE_CALLBACK_H_
