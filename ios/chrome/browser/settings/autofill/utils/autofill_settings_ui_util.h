// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_UTILS_AUTOFILL_SETTINGS_UI_UTIL_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_UTILS_AUTOFILL_SETTINGS_UI_UTIL_H_

#import <Foundation/Foundation.h>

#import <string>

// Returns the deletion confirmation message based on whether the selection
// has local, account, or home/work/name/email profiles.
NSString* GetDeletionConfirmationString(bool has_local_profile,
                                        bool has_account_profile,
                                        bool has_home_work_name_email_profile,
                                        const std::u16string& user_email);

// Returns the deletion confirmation message when local Autofill AI entities
// are selected for deletion.
NSString* GetDeletionConfirmationStringWithEntities();

@class TableViewItem;

// Returns YES if `items` contains any local (deletable) entities.
BOOL ContainsLocalEntity(NSArray<TableViewItem*>* items);

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_UTILS_AUTOFILL_SETTINGS_UI_UTIL_H_
