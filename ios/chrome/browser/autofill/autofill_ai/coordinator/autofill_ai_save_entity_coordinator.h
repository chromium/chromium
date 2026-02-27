// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_SAVE_ENTITY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_SAVE_ENTITY_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace autofill {
struct SaveEntityParams;
}  // namespace autofill

// Coordinator for the Autofill AI entity save and update detailed UI.
@interface AutofillAISaveEntityCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    params:(autofill::SaveEntityParams)params
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_SAVE_ENTITY_COORDINATOR_H_
