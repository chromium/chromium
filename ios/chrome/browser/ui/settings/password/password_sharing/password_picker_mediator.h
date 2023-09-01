// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_PICKER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_PICKER_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"

namespace password_manager {
struct CredentialUIEntry;
}

@protocol PasswordPickerConsumer;

// Passes display information about passwords that were part of a details group
// from which the sharing flow originated to its consumer.
@interface PasswordPickerMediator : NSObject

- (instancetype)initWithCredentials:
    (const std::vector<password_manager::CredentialUIEntry>&)credentials
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Consumer of this mediator.
@property(nonatomic, weak) id<PasswordPickerConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PICKER_MEDIATOR_H_
