// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_TEST_FAKE_INFOBAR_EDIT_ADDRESS_PROFILE_MODAL_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_TEST_FAKE_INFOBAR_EDIT_ADDRESS_PROFILE_MODAL_CONSUMER_H_

#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/infobar_edit_address_profile_modal_consumer.h"

@interface FakeInfobarEditAddressProfileModalConsumer
    : NSObject <InfobarEditAddressProfileModalConsumer>
// Allow read access to values passed to InfobarEditAddressProfileModalConsumer
// interface.
@property(nonatomic, strong) NSMutableDictionary* profileData;
@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_TEST_FAKE_INFOBAR_EDIT_ADDRESS_PROFILE_MODAL_CONSUMER_H_
