// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_IDENTITY_DOCS_CONSUMER_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_IDENTITY_DOCS_CONSUMER_H_

#import <Foundation/Foundation.h>

@class TableViewItem;

// Consumer protocol for the Identity Docs settings page.
@protocol IdentityDocsConsumer <NSObject>

// Sets the lists of identity documents.
- (void)
    setIdentityDocsWithDriversLicenses:(NSArray<TableViewItem*>*)driversLicenses
                       nationalIdCards:(NSArray<TableViewItem*>*)nationalIdCards
                             passports:(NSArray<TableViewItem*>*)passports;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_IDENTITY_DOCS_CONSUMER_H_
