// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_UI_CREDENTIAL_EXPORT_CONSUMER_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_UI_CREDENTIAL_EXPORT_CONSUMER_H_

#import <Foundation/Foundation.h>

#import <vector>

namespace password_manager {
class AffiliatedGroup;
}

// Consumer for the Credential Export screen.
@protocol CredentialExportConsumer <NSObject>

// Sets the list of credential item data to be displayed.
- (void)setAffiliatedGroups:
    (std::vector<password_manager::AffiliatedGroup>)affiliatedGroups;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_UI_CREDENTIAL_EXPORT_CONSUMER_H_
