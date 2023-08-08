// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/memory/scoped_refptr.h"

@protocol PasswordSharingMediatorDelegate;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

// This mediator fetches information about the family members of the user (their
// display info and eligibility for receiving shared passwords) and notifies the
// coordinator with the result.
@interface PasswordSharingMediator : NSObject

- (instancetype)initWithDelegate:(id<PasswordSharingMediatorDelegate>)delegate
          SharedURLLoaderFactory:
              (scoped_refptr<network::SharedURLLoaderFactory>)
                  sharedURLLoaderFactory
                 identityManager:(signin::IdentityManager*)identityManager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_MEDIATOR_H_
