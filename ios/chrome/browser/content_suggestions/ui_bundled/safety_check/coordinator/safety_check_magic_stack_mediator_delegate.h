// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SAFETY_CHECK_COORDINATOR_SAFETY_CHECK_MAGIC_STACK_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SAFETY_CHECK_COORDINATOR_SAFETY_CHECK_MAGIC_STACK_MEDIATOR_DELEGATE_H_

// Handles Safety Check Module events.
@protocol SafetyCheckMagicStackMediatorDelegate

// Indicates to receiver that the Safety Check module should be removed.
- (void)removeSafetyCheckModule;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SAFETY_CHECK_COORDINATOR_SAFETY_CHECK_MAGIC_STACK_MEDIATOR_DELEGATE_H_
