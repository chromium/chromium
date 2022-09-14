// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol PopupMenuViewControllerDelegate;

// ViewController displaying a popup for a menu. The view of this controller is
// a transparent scrim, dismissing the popup if tapped.
@interface PopupMenuViewController : UIViewController

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

// View containing the content of this popup.
@property(nonatomic, strong, readonly) UIView* contentContainer;
// CommandHandler.
@property(nonatomic, weak) id<PopupMenuViewControllerDelegate> delegate;

// Adds `content` as a child ViewController and its view to the popup.
- (void)addContent:(UIViewController*)content;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_VIEW_CONTROLLER_H_
