// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_APP_URL_LOADING_SERVICE_H_
#define IOS_CHROME_BROWSER_URL_LOADING_APP_URL_LOADING_SERVICE_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"
#include "ios/chrome/app/application_mode.h"
#include "ui/base/page_transition_types.h"

struct UrlLoadParams;

@class TabModel;

namespace ios {
class ChromeBrowserState;
}

// Objective-C delegate for AppUrlLoadingService.
@protocol AppURLLoadingServiceDelegate

// Sets the current interface to |ApplicationMode::INCOGNITO| or
// |ApplicationMode::NORMAL|.
- (void)setCurrentInterfaceForMode:(ApplicationMode)mode;

// Dismisses all modal dialogs, excluding the omnibox if |dismissOmnibox| is
// NO, then call |completion|.
- (void)dismissModalDialogsWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox;

// Opens a tab in the target BVC, and switches to it in a way that's appropriate
// to the current UI.
// If the current tab in |targetMode| is a NTP, it can be reused to open URL.
// |completion| is executed after the tab is opened. After Tab is open the
// virtual URL is set to the pending navigation item.
- (void)openSelectedTabInMode:(ApplicationMode)targetMode
            withUrlLoadParams:(const UrlLoadParams&)urlLoadParams
                   completion:(ProceduralBlock)completion;

// Opens a new tab as if originating from |originPoint| and |focusOmnibox|.
- (void)openNewTabFromOriginPoint:(CGPoint)originPoint
                     focusOmnibox:(BOOL)focusOmnibox;

// Informs the BVC that a new foreground tab is about to be opened in given
// |targetMode|. This is intended to be called before setWebUsageSuspended:NO in
// cases where a new tab is about to appear in order to allow the BVC to avoid
// doing unnecessary work related to showing the previously selected tab.
- (void)expectNewForegroundTabForMode:(ApplicationMode)targetMode;

// TODO(crbug.com/907527): refactor to remove these and most methods above.
- (ios::ChromeBrowserState*)currentBrowserState;
- (TabModel*)currentTabModel;

@end

// Service used to manage url loading at application level.
class AppUrlLoadingService {
 public:
  AppUrlLoadingService();

  void SetDelegate(id<AppURLLoadingServiceDelegate> delegate);

  // Opens a url based on |params| in a new tab.
  virtual void LoadUrlInNewTab(const UrlLoadParams& params);

  // Returns the current browser state.
  virtual ios::ChromeBrowserState* GetCurrentBrowserState();

 private:
  __weak id<AppURLLoadingServiceDelegate> delegate_;
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_APP_URL_LOADING_SERVICE_H_
