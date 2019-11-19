// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_DATA_MANAGER_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_DATA_MANAGER_INTERNAL_H_

#import "ios/web_view/public/cwv_autofill_data_manager.h"

namespace autofill {
class PersonalDataManager;
}  // namespace autofill;

namespace password_manager {
class PasswordStore;
}  // password_manager

NS_ASSUME_NONNULL_BEGIN

@interface CWVAutofillDataManager ()

// |personalDataManager| The underlying personal data manager being wrapped.
// |passwordStore| The underlying password store being wrapped.
// It should outlive this instance.
- (instancetype)initWithPersonalDataManager:
                    (autofill::PersonalDataManager*)personalDataManager
                              passwordStore:(password_manager::PasswordStore*)
                                                passwordStore
    NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_DATA_MANAGER_INTERNAL_H_
