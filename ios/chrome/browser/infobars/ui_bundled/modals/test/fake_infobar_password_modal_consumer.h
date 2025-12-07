// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_MODALS_TEST_FAKE_INFOBAR_PASSWORD_MODAL_CONSUMER_H_
#define IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_MODALS_TEST_FAKE_INFOBAR_PASSWORD_MODAL_CONSUMER_H_

#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_password_modal_consumer.h"

@interface FakeInfobarPasswordModalConsumer
    : NSObject <InfobarPasswordModalConsumer>
// Allow read access to values passed to InfobarPasswwordModalConsumer
// interface.
@property(nonatomic, copy) NSString* originalUsername;
@property(nonatomic, copy) NSString* maskedPassword;
@property(nonatomic, copy) NSString* unmaskedPassword;
@property(nonatomic, copy) NSString* detailsTextMessage;
@property(nonatomic, copy) NSString* URL;
@property(nonatomic, copy) NSString* saveButtonText;
@property(nonatomic, copy) NSString* cancelButtonText;
@property(nonatomic, assign) BOOL currentCredentialsSaved;
@end

#endif  // IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_MODALS_TEST_FAKE_INFOBAR_PASSWORD_MODAL_CONSUMER_H_
