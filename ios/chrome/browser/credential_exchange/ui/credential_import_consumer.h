// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_UI_CREDENTIAL_IMPORT_CONSUMER_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_UI_CREDENTIAL_IMPORT_CONSUMER_H_

#import <Foundation/Foundation.h>

#import <string>

@class ImportDataItem;
@class ImportDataItemTableView;

@protocol CredentialImportConsumer <NSObject>

// Sets import data item to be displayed in the table view.
- (void)setImportDataItem:(ImportDataItem*)importDataItem;

// Sets the email of the signed-in user's account.
- (void)setUserEmail:(const std::string&)userEmail;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_UI_CREDENTIAL_IMPORT_CONSUMER_H_
