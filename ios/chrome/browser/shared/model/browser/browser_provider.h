// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_PROVIDER_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_PROVIDER_H_

#import <UIKit/UIKit.h>

class Browser;

// A BrowserProvider is an abstraction that exposes an interface to the Chrome
// user interface (and related model objects) to the application layer. Each
// BrowserProvider is roughly equivalent to a window on a desktop browser --
// a collection of tabs (a tab model) associated with a user profile (the
// profile) with the UI of the currently visible tab (the view controller)
// and some other attributes as well.
@protocol BrowserProvider
// The active browser. This can never be nullptr once the UI initialization has
// completed, i.e. after the app init stage has passed AppInitStage::kNormalUI.
@property(nonatomic, readonly) Browser* browser;
// The inactive browser. This can be nullptr if in an incognito interface or if
// Inactive Tabs is disabled.
@property(nonatomic) Browser* inactiveBrowser;

/*
 Properties that should be removed.
  TODO(crbug.com/40606165): The long-term goal is to reduce the size of this
  interface; this protocol allows for easy encapsulation of that process.
 */

// Only used by the FirstRunSceneAgent.
@property(nonatomic, readonly) UIViewController* viewController;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_PROVIDER_H_
