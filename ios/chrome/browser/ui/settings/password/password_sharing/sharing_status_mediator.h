// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_SHARING_STATUS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_SHARING_STATUS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import <optional>

#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"

@class RecipientInfoForIOSDisplay;
@protocol SharingStatusConsumer;

class AuthenticationService;
class ChromeAccountManagerService;
class FaviconLoader;
class GURL;

// This mediator passes display information about sender and recipients of the
// user to its consumer.
@interface SharingStatusMediator : NSObject <TableViewFaviconDataSource>

- (instancetype)
      initWithAuthService:(AuthenticationService*)authService
    accountManagerService:(ChromeAccountManagerService*)accountManagerService
            faviconLoader:(FaviconLoader*)faviconLoader
               recipients:(NSArray<RecipientInfoForIOSDisplay*>*)recipients
                  website:(NSString*)website
                      URL:(const GURL&)URL
        changePasswordURL:(const std::optional<GURL>&)changePasswordURL
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Consumer of this mediator.
@property(nonatomic, weak) id<SharingStatusConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_SHARING_STATUS_MEDIATOR_H_
