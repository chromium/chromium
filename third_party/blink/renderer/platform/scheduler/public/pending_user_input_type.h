// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_PENDING_USER_INPUT_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_PENDING_USER_INPUT_TYPE_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {
namespace scheduler {

// A subset of DOM input events that should be supported by
// hasPendingUserInput. Each one of these maps to one or more
// WebInputEvent::Type items.
enum class PendingUserInputType {
  kNone = 0,

  kClick = 1 << 1,
  kDblClick = 1 << 2,
  kMouseDown = 1 << 3,
  kMouseEnter = 1 << 4,
  kMouseLeave = 1 << 5,
  kMouseMove = 1 << 6,
  kMouseOut = 1 << 7,
  kMouseOver = 1 << 8,
  kMouseUp = 1 << 9,

  kWheel = 1 << 10,

  kKeyDown = 1 << 11,
  kKeyUp = 1 << 12,

  kTouchStart = 1 << 13,
  kTouchEnd = 1 << 14,
  kTouchMove = 1 << 15,
  kTouchCancel = 1 << 16,

  kAny = ~0  // Wildcard input event type
};

inline constexpr PendingUserInputType operator&(PendingUserInputType a,
                                                PendingUserInputType b) {
  return static_cast<PendingUserInputType>(static_cast<int>(a) &
                                           static_cast<int>(b));
}

inline constexpr PendingUserInputType operator|(PendingUserInputType a,
                                                PendingUserInputType b) {
  return static_cast<PendingUserInputType>(static_cast<int>(a) |
                                           static_cast<int>(b));
}

inline constexpr bool operator==(PendingUserInputType a,
                                 PendingUserInputType b) {
  return static_cast<int>(a) == static_cast<int>(b);
}

// A wrapper around a set of input types that have been flagged as pending.
class PLATFORM_EXPORT PendingUserInputInfo {
  DISALLOW_NEW();

 public:
  constexpr explicit PendingUserInputInfo(PendingUserInputType mask)
      : mask_(mask) {}
  constexpr PendingUserInputInfo() : mask_(PendingUserInputType::kNone) {}

  constexpr bool HasPendingInputType(PendingUserInputType type) const {
    return (mask_ & type) != PendingUserInputType::kNone;
  }

 private:
  const PendingUserInputType mask_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_PENDING_USER_INPUT_TYPE_H_
