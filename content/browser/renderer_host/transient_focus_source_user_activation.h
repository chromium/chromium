// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_TRANSIENT_FOCUS_SOURCE_USER_ACTIVATION_H_
#define CONTENT_BROWSER_RENDERER_HOST_TRANSIENT_FOCUS_SOURCE_USER_ACTIVATION_H_

#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

// This class manages a transient state tracking mechanism for tracking if the
// frame initiating the focus call has user activation. This is used to verify
// the validity of focus crossing a fenced frame boundary. When activated, the
// transient state is active for the same amount of time that a transient user
// activation stays active for.
class CONTENT_EXPORT TransientFocusSourceUserActivation {
 public:
  TransientFocusSourceUserActivation();

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

#endif  // CONTENT_BROWSER_RENDERER_HOST_TRANSIENT_FOCUS_SOURCE_USER_ACTIVATION_H_
