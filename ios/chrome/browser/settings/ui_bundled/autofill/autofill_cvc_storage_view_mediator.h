// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_AUTOFILL_CVC_STORAGE_VIEW_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_AUTOFILL_CVC_STORAGE_VIEW_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_cvc_storage_view_controller.h"

namespace autofill {
class PersonalDataManager;
}
class PrefService;

// The Mediator for controlling enabling/disabling CVC storage and deleting
// saved CVCs.
// TODO(crbug.com/437906874): Rename to AutofillCvcStorageMediator and
// AutofillCvcStorageCoordinator.
@interface AutofillCvcStorageViewMediator
    : NSObject <AutofillCvcStorageViewControllerDelegate>

- (instancetype)initWithPersonalDataManager:
                    (autofill::PersonalDataManager*)personalDataManager
                                prefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

// The view controller for this mediator.
@property(nonatomic, weak) id<AutofillCvcStorageConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_AUTOFILL_CVC_STORAGE_VIEW_MEDIATOR_H_
