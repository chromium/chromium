// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PICKER_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PICKER_CONSUMER_H_

@class RecipientInfoForIOSDisplay;

// Provides potential password sharing recipients of the user.
@protocol FamilyPickerConsumer <NSObject>

// Sets array of potential recipients to be displayed in the family picker view.
- (void)setRecipients:(NSArray<RecipientInfoForIOSDisplay*>*)recipients;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PICKER_CONSUMER_H_
