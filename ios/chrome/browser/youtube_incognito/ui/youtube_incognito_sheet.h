// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_YOUTUBE_INCOGNITO_UI_YOUTUBE_INCOGNITO_SHEET_H_
#define IOS_CHROME_BROWSER_YOUTUBE_INCOGNITO_UI_YOUTUBE_INCOGNITO_SHEET_H_

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

@protocol YoutubeIncognitoSheetDelegate;
@protocol NewTabPageURLLoaderDelegate;

// A `ConfirmationAlertViewController` for the Youtube Incognito interstitial,
// to be managed by the associated `YoutubeIncognitoCoordinator`.
@interface YoutubeIncognitoSheet
    : ConfirmationAlertViewController <ConfirmationAlertActionHandler>

// The delegate for interactions in this View Controller.
@property(nonatomic, weak) id<YoutubeIncognitoSheetDelegate> delegate;

// Some URLs in the controlled view can be loaded.
@property(nonatomic, weak) id<NewTabPageURLLoaderDelegate> URLLoaderDelegate;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
@end

#endif  // IOS_CHROME_BROWSER_YOUTUBE_INCOGNITO_UI_YOUTUBE_INCOGNITO_SHEET_H_
