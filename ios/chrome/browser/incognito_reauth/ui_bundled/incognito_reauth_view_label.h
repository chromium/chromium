// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_VIEW_LABEL_H_
#define IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_VIEW_LABEL_H_

#import <UIKit/UIKit.h>

// Protocol for the owner of the label, which will receive the notifications.
@protocol IncognitoReauthViewLabelOwner

// Called when the label layout.
- (void)labelDidLayout;

@end

// Label use to get notifications when it setting its layout.
@interface IncognitoReauthViewLabel : UILabel

// Owner of the label, receiving notifications.
@property(nonatomic, weak) id<IncognitoReauthViewLabelOwner> owner;

@end

#endif  // IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_VIEW_LABEL_H_
