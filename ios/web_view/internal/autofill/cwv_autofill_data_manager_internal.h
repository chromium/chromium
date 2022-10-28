// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_DATA_MANAGER_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_DATA_MANAGER_INTERNAL_H_

#import "ios/web_view/public/cwv_autofill_data_manager.h"

namespace autofill {
class PersonalDataManager;
}  // namespace autofill;

namespace password_manager {
class PasswordStoreInterface;
}  // password_manager

NS_ASSUME_NONNULL_BEGIN

@interface CWVAutofillDataManager ()

// |personalDataManager| The underlying personal data manager being wrapped.
// |passwordStore| The underlying password store being wrapped.
// It should outlive this instance.
- (instancetype)initWithPersonalDataManager:
                    (autofill::PersonalDataManager*)personalDataManager
                              passwordStore:
                                  (password_manager::PasswordStoreInterface*)
                                      passwordStore NS_DESIGNATED_INITIALIZER;

// This is called by the associated CWVWebViewConfiguration in order to shut
// down cleanly. See `-[CWVWebViewConfiguration shutDown]` method for more info.
- (void)shutDown;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_DATA_MANAGER_INTERNAL_H_
