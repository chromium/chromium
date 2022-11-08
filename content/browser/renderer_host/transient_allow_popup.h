// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_TRANSIENT_ALLOW_POPUP_H_
#define CONTENT_BROWSER_RENDERER_HOST_TRANSIENT_ALLOW_POPUP_H_

#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

// This class manages a transient affordance for a frame to open a popup window.
// Sites with Window Management permission may open a popup on another screen
// after requesting fullscreen on a specific screen of a multi-screen device.
// This enables multi-screen content experiences from a single user gesture.
class CONTENT_EXPORT TransientAllowPopup {
 public:
  TransientAllowPopup();

  // The lifespan should be just long enough to allow brief async script calls
  // and long enough for the OS to complete any transition (i.e. fullscreen)
  // animations first.
  static constexpr base::TimeDelta kActivationLifespan = base::Seconds(5);

  // Activate the transient state.
  void Activate();

  // Deactivate the transient state.
  void Deactivate();

  // Returns the transient state; |true| if this object was recently activated.
  bool IsActive() const;

 private:
  base::TimeTicks transient_state_expiry_time_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_TRANSIENT_ALLOW_POPUP_H_
