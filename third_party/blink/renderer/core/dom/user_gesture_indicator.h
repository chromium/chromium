// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_USER_GESTURE_INDICATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_USER_GESTURE_INDICATOR_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace base {
class Clock;
}

namespace blink {

// A UserGestureToken represents the current state of a user gesture. It can be
// retrieved from a UserGestureIndicator to save for later (see, e.g., DOMTimer,
// which propagates user gestures to the timer fire in certain situations).
// Passing it to a UserGestureIndicator later on will cause it to be considered
// as currently being processed.
//
// DEPRECATED: Use |UserActivationState| accessors in |Frame|.
class CORE_EXPORT UserGestureToken : public RefCounted<UserGestureToken> {
  friend class UserGestureIndicator;

 public:
  ~UserGestureToken() = default;

  void SetClockForTesting(const base::Clock* clock) { clock_ = clock; }
  void ResetTimestampForTesting() { ResetTimestamp(); }

 private:
  FRIEND_TEST_ALL_PREFIXES(UserGestureIndicatorTest, Timeouts);
  UserGestureToken();

  void TransferGestureTo(UserGestureToken*);
  bool ConsumeGesture();
  void ResetTimestamp();
  bool HasGestures() const;
  bool HasTimedOut() const;

  size_t consumable_gestures_;
  const base::Clock* clock_;
  double timestamp_;
  DISALLOW_COPY_AND_ASSIGN(UserGestureToken);
};

// DEPRECATED: Use |UserActivationState| accessors in |Frame|.
class CORE_EXPORT UserGestureIndicator final {
  USING_FAST_MALLOC(UserGestureIndicator);

 public:
  // This method is for testing only.
  static UserGestureToken* CurrentTokenForTest();

  // This method is safe to call from any thread.
  static UserGestureToken* CurrentTokenThreadSafe();

  explicit UserGestureIndicator(scoped_refptr<UserGestureToken>);

  // Constructs a UserGestureIndicator with a new UserGestureToken of the given
  // status.
  explicit UserGestureIndicator();
  ~UserGestureIndicator();

 private:
  void UpdateRootToken();

  static UserGestureToken* root_token_;

  scoped_refptr<UserGestureToken> token_;
  DISALLOW_COPY_AND_ASSIGN(UserGestureIndicator);
};

}  // namespace blink

#endif
