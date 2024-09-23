// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_MODEL_SCENE_URL_LOADING_SERVICE_H_
#define IOS_CHROME_BROWSER_URL_LOADING_MODEL_SCENE_URL_LOADING_SERVICE_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#include "ios/chrome/app/application_mode.h"
#include "ui/base/page_transition_types.h"

class Browser;
struct UrlLoadParams;

// Objective-C delegate for SceneUrlLoadingService.
@protocol SceneURLLoadingServiceDelegate

// Sets the current interface to `ApplicationMode::INCOGNITO` or
// `ApplicationMode::NORMAL`.
- (void)setCurrentInterfaceForMode:(ApplicationMode)mode;

// Dismisses all modal dialogs, excluding the omnibox if `dismissOmnibox` is
// NO, then call `completion`.
- (void)dismissModalDialogsWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox;

// Opens a tab in the target BVC, and switches to it in a way that's appropriate
// to the current UI.
// If the current tab in `targetMode` is a NTP, it can be reused to open URL.
// `completion` is executed after the tab is opened. After Tab is open the
// virtual URL is set to the pending navigation item.
- (void)openSelectedTabInMode:(ApplicationModeForTabOpening)targetMode
            withUrlLoadParams:(const UrlLoadParams&)urlLoadParams
                   completion:(ProceduralBlock)completion;

// Open the list of URLs contained in `URLs` in Incognito mode if
// `inIncognitoMode` is YES. `completion` is executed after all the tabs are
// opened.
- (void)openMultipleTabsWithURLs:(const std::vector<GURL>&)URLs
                 inIncognitoMode:(BOOL)incognitoMode
                      completion:(ProceduralBlock)completion;

// Opens a new tab as if originating from `originPoint` and `focusOmnibox`.
- (void)openNewTabFromOriginPoint:(CGPoint)originPoint
                     focusOmnibox:(BOOL)focusOmnibox
                    inheritOpener:(BOOL)inheritOpener;

// Informs the BVC that a new foreground tab is about to be opened in given
// `targetMode`.
- (void)expectNewForegroundTabForMode:(ApplicationMode)targetMode;

// TODO(crbug.com/41427539): refactor to remove this and most methods above.
@property(nonatomic, readonly) Browser* currentBrowserForURLLoading;

@end

// Service used to manage url loading at scene level.
class SceneUrlLoadingService {
 public:
  SceneUrlLoadingService();
  virtual ~SceneUrlLoadingService() = default;

  void SetDelegate(id<SceneURLLoadingServiceDelegate> delegate);

  // Opens a url based on `params` in a new tab.
  virtual void LoadUrlInNewTab(const UrlLoadParams& params);

  // Returns the current active browser in the scene owning this object.
  virtual Browser* GetCurrentBrowser();

 private:
  __weak id<SceneURLLoadingServiceDelegate> delegate_;
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_MODEL_SCENE_URL_LOADING_SERVICE_H_
