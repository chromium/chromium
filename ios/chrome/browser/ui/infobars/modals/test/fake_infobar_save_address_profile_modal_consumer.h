// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_TEST_FAKE_INFOBAR_SAVE_ADDRESS_PROFILE_MODAL_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_TEST_FAKE_INFOBAR_SAVE_ADDRESS_PROFILE_MODAL_CONSUMER_H_

#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/infobar_save_address_profile_modal_consumer.h"

@interface FakeInfobarSaveAddressProfileModalConsumer
    : NSObject <InfobarSaveAddressProfileModalConsumer>
// Allow read access to values passed to InfobarSaveAddressProfileModalConsumer
// interface.
@property(nonatomic, copy) NSString* address;
@property(nonatomic, copy) NSString* phoneNumber;
@property(nonatomic, copy) NSString* emailAddress;
@property(nonatomic, assign) BOOL currentAddressProfileSaved;
@property(nonatomic, assign) BOOL isUpdateModal;
@property(nonatomic, copy) NSDictionary* profileDataDiff;
@property(nonatomic, copy) NSString* updateModalDescription;
@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_TEST_FAKE_INFOBAR_SAVE_ADDRESS_PROFILE_MODAL_CONSUMER_H_
