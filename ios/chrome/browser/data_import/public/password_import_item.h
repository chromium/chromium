// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_PASSWORD_IMPORT_ITEM_H_
#define IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_PASSWORD_IMPORT_ITEM_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

@class FaviconAttributes;
@protocol PasswordImportItemFaviconDataSource;
@class URLWithTitle;

namespace password_manager {
struct ImportResults;
}  // namespace password_manager

/// Matches password_manager::ImportEntry::Status.
/// Needs to be kept in sync with PasswordManagerImportEntryStatus in
/// tools/metrics/histograms/enums.xml
enum class PasswordImportStatus {
  /// Should not be used.
  kNone,
  /// Any other error state.
  kUnknownError,
  /// Missing password field.
  kMissingPassword,
  /// Missing URL field.
  kMissingURL,
  /// Bad URL formatting.
  kInvalidURL,
  /// URL is too long.
  kLongUrl,
  /// Password is too long.
  kLongPassword,
  /// Username is too long.
  kLongUsername,
  /// Credential is already stored in profile store or account store.
  kConflictProfile,
  kConflictAccount,
  /// Note is too long.
  kLongNote,
  /// Concatenation of imported and local notes is too long.
  kLongConcatenatedNote,
  /// Valid credential.
  kValid,
};

/// A password item to be imported.
@interface PasswordImportItem : NSObject

/// URL for the website, with `url.title` being the formatted URL string, scheme
/// and path omitted.
@property(nonatomic, readonly, strong) URLWithTitle* url;

/// The username for the password.
@property(nonatomic, readonly, strong) NSString* username;

/// The password.
@property(nonatomic, readonly, strong) NSString* password;

/// Import status.
@property(nonatomic, readonly, assign) PasswordImportStatus status;

/// Data source for favicon loading. Should be set before
/// `-loadFaviconWithUIUpdateHandler` is invoked.
@property(nonatomic, weak) id<PasswordImportItemFaviconDataSource>
    faviconDataSource;

/// Favicon attributes for the URL. If current value is `nil`, call
/// `-loadFaviconWithUIUpdateHandler` and retrieve the value in the completion
/// handler.
@property(nonatomic, strong) FaviconAttributes* faviconAttributes;

/// Converts `ImportResults` to a list of `PasswordImportItem`s.
+ (NSArray<PasswordImportItem*>*)passwordImportItemsFromImportResults:
    (const password_manager::ImportResults&)results;

/// Initialization.
- (instancetype)initWithURL:(URLWithTitle*)url
                   username:(NSString*)username
                   password:(NSString*)password
                     status:(PasswordImportStatus)status
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Loads the favicon with a block to update UI on the first call to this
/// method. Does nothing on subsequent calls.
- (void)loadFaviconWithUIUpdateHandler:(ProceduralBlock)handler;

@end

#endif  // IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_PASSWORD_IMPORT_ITEM_H_
