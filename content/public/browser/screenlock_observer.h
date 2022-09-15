// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SCREENLOCK_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_SCREENLOCK_OBSERVER_H_

#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT ScreenlockObserver {
 public:
  // Notification that the screen is locked.
  virtual void OnScreenLocked() {}

  // Notification that the screen is unlocked.
  virtual void OnScreenUnlocked() {}

 protected:
  virtual ~ScreenlockObserver() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SCREENLOCK_OBSERVER_H_