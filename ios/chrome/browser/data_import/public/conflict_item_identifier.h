// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_CONFLICT_ITEM_IDENTIFIER_H_
#define IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_CONFLICT_ITEM_IDENTIFIER_H_

#import <Foundation/Foundation.h>

enum class CredentialConflictType {
  kPassword,
  kPasskey,
};

// Identifier for a conflicting credential item used in
// UITableViewDiffableDataSource of a conflict resolution screen.
@interface ConflictItemIdentifier : NSObject

@property(nonatomic, readonly) CredentialConflictType type;
@property(nonatomic, readonly) NSUInteger index;

- (instancetype)initWithType:(CredentialConflictType)type
                       index:(NSInteger)index NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_CONFLICT_ITEM_IDENTIFIER_H_
