// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_PASSWORD_IMPORT_ITEM_H_
#define IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_PASSWORD_IMPORT_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/data_import/public/credential_import_item.h"

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
@interface PasswordImportItem : CredentialImportItem

/// The password.
@property(nonatomic, readonly, strong) NSString* password;

/// Import status.
@property(nonatomic, readonly, assign) PasswordImportStatus status;

/// Converts `ImportResults` to a list of `PasswordImportItem`s.
+ (NSArray<PasswordImportItem*>*)passwordImportItemsFromImportResults:
    (const password_manager::ImportResults&)results;

/// Initialization.
- (instancetype)initWithURL:(URLWithTitle*)url
                   username:(NSString*)username
                   password:(NSString*)password
                     status:(PasswordImportStatus)status
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithUrl:(URLWithTitle*)url
                   username:(NSString*)username NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_PASSWORD_IMPORT_ITEM_H_
