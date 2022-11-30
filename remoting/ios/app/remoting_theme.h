// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_REMOTING_THEME_H_
#define REMOTING_IOS_APP_REMOTING_THEME_H_

#import <UIKit/UIKit.h>

// Styles to be used when rendering the iOS client's UI.
@interface RemotingTheme : NSObject

// Colors

@property(class, nonatomic, readonly) UIColor* buttonBackgroundColor;
@property(class, nonatomic, readonly) UIColor* buttonTextColor;
@property(class, nonatomic, readonly) UIColor* connectionViewBackgroundColor;
@property(class, nonatomic, readonly) UIColor* connectionViewForegroundColor;
@property(class, nonatomic, readonly) UIColor* firstLaunchViewBackgroundColor;
@property(class, nonatomic, readonly) UIColor* dialogBackgroundColor;
@property(class, nonatomic, readonly) UIColor* dialogTextColor;
@property(class, nonatomic, readonly) UIColor* dialogPrimaryButtonTextColor;
@property(class, nonatomic, readonly) UIColor* dialogSecondaryButtonTextColor;
@property(class, nonatomic, readonly) UIColor* flatButtonTextColor;
@property(class, nonatomic, readonly) UIColor* hostCellTitleColor;
@property(class, nonatomic, readonly) UIColor* hostCellStatusTextColor;
@property(class, nonatomic, readonly) UIColor* hostErrorColor;
@property(class, nonatomic, readonly) UIColor* hostListBackgroundColor;
@property(class, nonatomic, readonly) UIColor* hostListHeaderTitleColor;
@property(class, nonatomic, readonly) UIColor* hostOfflineColor;
@property(class, nonatomic, readonly) UIColor* hostOnlineColor;
@property(class, nonatomic, readonly) UIColor* hostWarningColor;
@property(class, nonatomic, readonly) UIColor* menuBlueColor;
@property(class, nonatomic, readonly) UIColor* menuTextColor;
@property(class, nonatomic, readonly) UIColor* menuSeparatorColor;
@property(class, nonatomic, readonly) UIColor* refreshIndicatorColor;
@property(class, nonatomic, readonly) UIColor* pinEntryPairingColor;
@property(class, nonatomic, readonly) UIColor* pinEntryPlaceholderColor;
@property(class, nonatomic, readonly) UIColor* pinEntryTextColor;
@property(class, nonatomic, readonly) UIColor* setupListBackgroundColor;
@property(class, nonatomic, readonly) UIColor* setupListNumberColor;
@property(class, nonatomic, readonly) UIColor* setupListTextColor;
@property(class, nonatomic, readonly) UIColor* sideMenuIconColor;

// Icons

@property(class, nonatomic, readonly) UIImage* arrowIcon;
@property(class, nonatomic, readonly) UIImage* backIcon;
@property(class, nonatomic, readonly) UIImage* checkboxCheckedIcon;
@property(class, nonatomic, readonly) UIImage* checkboxOutlineIcon;
@property(class, nonatomic, readonly) UIImage* closeIcon;  // ally: "Close"
@property(class, nonatomic, readonly) UIImage* desktopIcon;
@property(class, nonatomic, readonly) UIImage* feedbackIcon;
@property(class, nonatomic, readonly) UIImage* helpIcon;
@property(class, nonatomic, readonly) UIImage* menuIcon;  // ally: "Menu"
@property(class, nonatomic, readonly) UIImage* radioCheckedIcon;
@property(class, nonatomic, readonly) UIImage* radioOutlineIcon;
@property(class, nonatomic, readonly) UIImage* refreshIcon;
@property(class, nonatomic, readonly) UIImage* settingsIcon;

@end

#endif  // REMOTING_IOS_APP_REMOTING_THEME_H_
