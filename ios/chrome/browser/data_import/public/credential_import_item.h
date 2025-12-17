// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_CREDENTIAL_IMPORT_ITEM_H_
#define IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_CREDENTIAL_IMPORT_ITEM_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

@class FaviconAttributes;
@class URLWithTitle;
@protocol CredentialImportItemFaviconDataSource;

/// A credential item to be imported.
@interface CredentialImportItem : NSObject

/// URL for the website, with `url.title` being the formatted URL string, scheme
/// and path omitted.
@property(nonatomic, readonly, strong) URLWithTitle* url;

/// The username for the credential.
@property(nonatomic, readonly, strong) NSString* username;

/// Data source for favicon loading. Should be set before
/// `-loadFaviconWithUIUpdateHandler` is invoked.
@property(nonatomic, weak) id<CredentialImportItemFaviconDataSource>
    faviconDataSource;

/// Favicon attributes for the URL. If current value is `nil`, call
/// `-loadFaviconWithUIUpdateHandler` and retrieve the value in the completion
/// handler.
@property(nonatomic, strong) FaviconAttributes* faviconAttributes;

- (instancetype)initWithUrl:(URLWithTitle*)url
                   username:(NSString*)username NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Loads the favicon with a block to update UI on the first call to this
/// method. Does nothing on subsequent calls.
- (void)loadFaviconWithUIUpdateHandler:(ProceduralBlock)handler;

@end

#endif  // IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_CREDENTIAL_IMPORT_ITEM_H_
