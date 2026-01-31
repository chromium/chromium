// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_CREDENTIAL_ITEM_IDENTIFIER_H_
#define IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_CREDENTIAL_ITEM_IDENTIFIER_H_

#import <Foundation/Foundation.h>

enum class CredentialType {
  kPassword,
  kPasskey,
};

// Identifier for a credential item used in UITableViewDiffableDataSource.
@interface CredentialItemIdentifier : NSObject

@property(nonatomic, readonly) CredentialType type;
@property(nonatomic, readonly) NSUInteger index;

- (instancetype)initWithType:(CredentialType)type
                       index:(NSInteger)index NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_CREDENTIAL_ITEM_IDENTIFIER_H_
