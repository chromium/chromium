// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_PASSKEY_IMPORT_ITEM_H_
#define IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_PASSKEY_IMPORT_ITEM_H_

#import <UIKit/UIKit.h>

#import <vector>

#import "ios/chrome/browser/data_import/public/credential_import_item.h"

@class URLWithTitle;

namespace webauthn {
struct ImportedPasskeyInfo;
}  // namespace webauthn

/// A passkey item to be imported.
@interface PasskeyImportItem : CredentialImportItem

/// Creation date of the passkey in the exporter password manager, if available.
@property(nonatomic, readonly, strong) NSDate* creationDate;

/// Converts list of `ImportedPasskeyInfo` to a list of `PasskeyImportItem`.
+ (NSArray<PasskeyImportItem*>*)passkeyImportItemsFromImportedPasskeyInfos:
    (const std::vector<webauthn::ImportedPasskeyInfo>&)results;

- (instancetype)initWithRpId:(NSString*)rpId
                    username:(NSString*)username
                creationDate:(NSDate*)creationDate NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithUrl:(URLWithTitle*)url
                   username:(NSString*)username NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_PASSKEY_IMPORT_ITEM_H_
