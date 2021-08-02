// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_MEDIATOR_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/credential_provider_extension/ui/new_password_view_controller.h"

@class ASCredentialServiceIdentifier;

// This mediator fetches requirements and saves new credentials for its
// consumer.
@interface NewPasswordMediator : NSObject <NewCredentialHandler>

- (instancetype)initWithServiceIdentifier:
    (ASCredentialServiceIdentifier*)serviceIdentifier NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_MEDIATOR_H_
