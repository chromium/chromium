// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_ENTITY_EDIT_COORDINATOR_TESTING_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_ENTITY_EDIT_COORDINATOR_TESTING_H_

#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_coordinator.h"

@class AutofillAIEntityEditMediator;

// Testing category exposing internal properties for tests.
@interface AutofillAIEntityEditCoordinator (Testing)

// The mediator used by this coordinator. Exposed for testing.
@property(nonatomic, strong) AutofillAIEntityEditMediator* mediator;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_ENTITY_EDIT_COORDINATOR_TESTING_H_
