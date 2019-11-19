// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_SERVICE_H_
#define IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_SERVICE_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include "components/keyed_service/core/keyed_service.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

class AppUrlLoadingService;
class Browser;
class UrlLoadingNotifier;
struct UrlLoadParams;

@class OpenNewTabCommand;

// Objective-C delegate for UrlLoadingService.
@protocol URLLoadingServiceDelegate

// Implementing delegate can do an animation using information in |params| when
// opening a background tab, then call |completion|.
- (void)animateOpenBackgroundTabFromParams:(const UrlLoadParams&)params
                                completion:(void (^)())completion;

@end

// Service used to load url in current or new tab.
class UrlLoadingService : public KeyedService {
 public:
  UrlLoadingService(UrlLoadingNotifier* notifier);

  void SetAppService(AppUrlLoadingService* app_service);
  void SetDelegate(id<URLLoadingServiceDelegate> delegate);
  void SetBrowser(Browser* browser);

  // Applies load strategy then calls |Dispatch|.
  virtual void Load(const UrlLoadParams& params);

 private:
  // Dispatches to one action method below, depending on |params.disposition|.
  void Dispatch(const UrlLoadParams& params);

  // Switches to a tab that matches |params.web_params| or loads in a new tab.
  virtual void SwitchToTab(const UrlLoadParams& params);

  // Loads a url based on |params| in current tab.
  virtual void LoadUrlInCurrentTab(const UrlLoadParams& params);

  // Loads a url based on |params| in a new tab.
  virtual void LoadUrlInNewTab(const UrlLoadParams& params);

  __weak id<URLLoadingServiceDelegate> delegate_;
  AppUrlLoadingService* app_service_;
  Browser* browser_;
  UrlLoadingNotifier* notifier_;
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_SERVICE_H_
