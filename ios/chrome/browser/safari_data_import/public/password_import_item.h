// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_PUBLIC_PASSWORD_IMPORT_ITEM_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_PUBLIC_PASSWORD_IMPORT_ITEM_H_

#import <UIKit/UIKit.h>

@class FaviconAttributes;

/// A password item to be imported.
@interface PasswordImportItem : NSObject

/// The website URL.
@property(nonatomic, readonly, strong) NSString* url;

/// Favicon attributes for the URL.
@property(nonatomic, readonly, strong) FaviconAttributes* faviconAttributes;

/// The username for the password.
@property(nonatomic, readonly, strong) NSString* username;

/// The password.
@property(nonatomic, readonly, strong) NSString* password;

/// Initialization
- (instancetype)initWithURL:(NSString*)url
                   username:(NSString*)username
                   password:(NSString*)password NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Load the favicon with completion handler. Does nothing if a load of the
/// favicon is already in progress.
- (void)loadFaviconWithCompletionHandler:(UIAction*)handler;

@end

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_PUBLIC_PASSWORD_IMPORT_ITEM_H_
