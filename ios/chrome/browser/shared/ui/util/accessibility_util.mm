// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/accessibility_util.h"

#import <UIKit/UIKit.h>

base::TimeDelta GetConfirmationDismissDelay() {
  return UIAccessibilityIsVoiceOverRunning()
             ? kConfirmationDismissDelayIfVoiceOverRunning
             : kConfirmationDismissDelay;
}
