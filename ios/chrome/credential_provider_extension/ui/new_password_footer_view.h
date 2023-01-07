// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_FOOTER_VIEW_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_FOOTER_VIEW_H_

#import <UIKit/UIKit.h>

@interface NewPasswordFooterView : UITableViewHeaderFooterView

// ReuseID for ths class.
@property(class, readonly) NSString* reuseID;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_FOOTER_VIEW_H_
