// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_PROVIDER_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_PROVIDER_H_

#import <UIKit/UIKit.h>

#import "base/types/pass_key.h"

class Browser;

// Used to limit access to the UIViewController from the BrowserProvider
// to the FirstRunProfileAgent. Will be removed when the code has been
// refactored to no longer access this method (crbug.com/40606165).
class FirstRunProfileAgentHelper;
using BrowserProviderPassKey = base::PassKey<FirstRunProfileAgentHelper>;

// A BrowserProvider is an abstraction that exposes an interface to the Chrome
// user interface (and related model objects) to the application layer. Each
// BrowserProvider is roughly equivalent to a window on a desktop browser --
// a collection of tabs (a tab model) associated with a user profile (the
// profile) with the UI of the currently visible tab (the view controller)
// and some other attributes as well.
@protocol BrowserProvider

// The active browser. This can never be nullptr once the UI initialization has
// completed, i.e. after the ProfileState has passed ProfileInitStage::kUIReady.
@property(nonatomic, readonly) Browser* browser;

// TODO(crbug.com/40606165): Used by FirstRunProfileAgent. Should be removed.
- (UIViewController*)viewController:(BrowserProviderPassKey)key;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_PROVIDER_H_
