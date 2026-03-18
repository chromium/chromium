// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_ENTITY_EDIT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_ENTITY_EDIT_COORDINATOR_H_

#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol AutofillAIEntityEditCoordinatorDelegate;

@interface AutofillAIEntityEditCoordinator : ChromeCoordinator

// Delegate for the coordinator.
@property(nonatomic, weak) id<AutofillAIEntityEditCoordinatorDelegate> delegate;

// Initializes the coordinator with a `navigationController`, the `browser`
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        entityID:
                                            (autofill::EntityInstance::EntityId)
                                                entityID
    NS_DESIGNATED_INITIALIZER;

// Default initializer is unavailable.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_ENTITY_EDIT_COORDINATOR_H_
