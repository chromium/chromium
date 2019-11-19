// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_ROOT_VIEW_CONTROLLING_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_ROOT_VIEW_CONTROLLING_H_

#import <Foundation/Foundation.h>

@protocol ApplicationCommands;
@protocol BrowserCommands;

// TODO(crbug.com/894800): This protocol is added to have a common interface
// between the SettingsRootViewControllers for table views and collections.
// Remove it once it is completed.
@protocol SettingsRootViewControlling

// The dispatcher used by this ViewController.
@property(nonatomic, weak) id<ApplicationCommands, BrowserCommands> dispatcher;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_ROOT_VIEW_CONTROLLING_H_
