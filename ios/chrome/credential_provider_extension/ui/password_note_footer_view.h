// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_PASSWORD_NOTE_FOOTER_VIEW_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_PASSWORD_NOTE_FOOTER_VIEW_H_

#import <UIKit/UIKit.h>

@interface PasswordNoteFooterView : UITableViewHeaderFooterView

// ReuseID for this class.
@property(class, readonly) NSString* reuseID;

// Label to hold the actual text.
@property(nonatomic, strong) UILabel* textLabel;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_PASSWORD_NOTE_FOOTER_VIEW_H_
