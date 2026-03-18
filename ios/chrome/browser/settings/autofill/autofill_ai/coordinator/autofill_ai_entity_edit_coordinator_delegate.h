// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_ENTITY_EDIT_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_ENTITY_EDIT_COORDINATOR_DELEGATE_H_

@class AutofillAIEntityEditCoordinator;

// Delegate for the AutofillAIEntityEditCoordinator.
@protocol AutofillAIEntityEditCoordinatorDelegate

// Notifies the delegate that the coordinator has finished.
- (void)autofillAIEntityEditCoordinatorDidFinish:
    (AutofillAIEntityEditCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_ENTITY_EDIT_COORDINATOR_DELEGATE_H_
