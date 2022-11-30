// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_TRANSIENT_ALLOW_FULLSCREEN_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_TRANSIENT_ALLOW_FULLSCREEN_H_

#include "base/time/time.h"
#include "third_party/blink/public/common/common_export.h"

namespace blink {

// This class manages a transient affordance for a frame to enter fullscreen.
// This is helpful for user-generated events that do not constitute activation,
// but could still be used to grant an element fullscreen request.
class BLINK_COMMON_EXPORT TransientAllowFullscreen {
 public:
  TransientAllowFullscreen();

  // The lifespan should be just long enough to allow brief async script calls.
  static constexpr base::TimeDelta kActivationLifespan = base::Seconds(1);

  // Activate the transient state.
  void Activate();

  // Returns the transient state; |true| if this object was recently activated.
  bool IsActive() const;

 private:
  base::TimeTicks transient_state_expiry_time_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_TRANSIENT_ALLOW_FULLSCREEN_H_
