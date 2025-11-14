/// Copyright 2025 The Chromium Authors
/// Use of this source code is governed by a BSD-style license that can be
/// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_IMPORT_UI_PASSWORD_IMPORT_ITEM_CELL_CONTENT_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_DATA_IMPORT_UI_PASSWORD_IMPORT_ITEM_CELL_CONTENT_CONFIGURATION_H_

#import <UIKit/UIKit.h>

@class FaviconAttributes;
@class PasswordImportItem;

/// Content configuration for a cell that displays a password import item.
@interface PasswordImportItemCellContentConfiguration
    : NSObject <UIContentConfiguration>

/// Information from `PasswordImportItem`.
@property(nonatomic, readonly) NSString* URL;
@property(nonatomic, readonly) NSString* username;

/// Message displayed under `username` and its highlight status.
@property(nonatomic, readonly) NSString* message;
@property(nonatomic, readonly) BOOL isMessageHighlighted;

/// Attributes to load the favicon for URL. Should be set separately.
@property(nonatomic, strong) FaviconAttributes* faviconAttributes;

/// Initializes cell provider showing masked password.
+ (instancetype)cellConfigurationForMaskPassword:(PasswordImportItem*)item;

/// Initializes cell provider showing unmasked password.
+ (instancetype)cellConfigurationForUnmaskPassword:(PasswordImportItem*)item;

/// Initializes cell provider showing the error message.
+ (instancetype)cellConfigurationForErrorMessage:(PasswordImportItem*)item;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_DATA_IMPORT_UI_PASSWORD_IMPORT_ITEM_CELL_CONTENT_CONFIGURATION_H_
