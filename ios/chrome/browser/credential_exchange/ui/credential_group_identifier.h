// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_UI_CREDENTIAL_GROUP_IDENTIFIER_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_UI_CREDENTIAL_GROUP_IDENTIFIER_H_

#import <Foundation/Foundation.h>

namespace password_manager {
class AffiliatedGroup;
}

// Stable identifier for an AffiliatedGroup to be used with
// UITableViewDiffableDataSource.
@interface CredentialGroupIdentifier : NSObject

// The affiliated group of credentials.
@property(nonatomic, readonly)
    password_manager::AffiliatedGroup affiliatedGroup;

// Designated initializer.
- (instancetype)initWithGroup:(const password_manager::AffiliatedGroup&)group
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_UI_CREDENTIAL_GROUP_IDENTIFIER_H_
