// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_grabber_button.h"

@implementation AssistantGrabberButton

- (void)accessibilityIncrement {
  [self.accessibilityDelegate assistantGrabberButtonDidIncrement:self];
}

- (void)accessibilityDecrement {
  [self.accessibilityDelegate assistantGrabberButtonDidDecrement:self];
}

@end
