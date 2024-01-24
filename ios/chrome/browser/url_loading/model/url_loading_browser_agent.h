// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_MODEL_URL_LOADING_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_URL_LOADING_MODEL_URL_LOADING_BROWSER_AGENT_H_

#import <Foundation/Foundation.h>

#import <optional>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"

class SceneUrlLoadingService;
class Browser;
class UrlLoadingNotifierBrowserAgent;
struct UrlLoadParams;

// A delegate for URL loading that can handle UI animations that are needed at
// specific points in the loading cycle.
@protocol URLLoadingDelegate

// Implementing delegate can do an animation using information in `params` when
// opening a background tab, then call `completion`.
- (void)animateOpenBackgroundTabFromParams:(const UrlLoadParams&)params
                                completion:(void (^)())completion;

@end

// Service used to load url in current or new tab.
class UrlLoadingBrowserAgent : public BrowserUserData<UrlLoadingBrowserAgent> {
 public:
  // Not copyable or moveable.
  UrlLoadingBrowserAgent(const UrlLoadingBrowserAgent&) = delete;
  UrlLoadingBrowserAgent& operator=(const UrlLoadingBrowserAgent&) = delete;
  ~UrlLoadingBrowserAgent() override;

  void SetSceneService(SceneUrlLoadingService* app_service);
  void SetIncognitoLoader(UrlLoadingBrowserAgent* loader);
  void SetDelegate(id<URLLoadingDelegate> delegate);

  // Applies load strategy then calls `Dispatch`.
  void Load(const UrlLoadParams& params);

 private:
  friend class BrowserUserData<UrlLoadingBrowserAgent>;
  friend class FakeUrlLoadingBrowserAgent;
  BROWSER_USER_DATA_KEY_DECL();

  explicit UrlLoadingBrowserAgent(Browser* browser);

  // Dispatches to one action method below, depending on `params.disposition`.
  void Dispatch(const UrlLoadParams& params);

  // Action methods.
  // Switches to a tab that matches `params.web_params` or loads in a new tab.
  virtual void SwitchToTab(const UrlLoadParams& params);

  // Loads a url based on `params` in current tab.
  virtual void LoadUrlInCurrentTab(const UrlLoadParams& params);

  // Loads a url based on `params` in a new tab.
  virtual void LoadUrlInNewTab(const UrlLoadParams& params);

  // Helper function implementing the creation and insertion of the new tab
  // for LoadUrlInNewTab(). It is split to a separate function as it can be
  // called asynchronously if the tab is opened in a background (and moving
  // it to a separate function makes it safer not to capture state that can
  // become invalid when creating the asynchronous task).
  void LoadUrlInNewTabImpl(const UrlLoadParams& params,
                           std::optional<void*> hint);

  __weak id<URLLoadingDelegate> delegate_;
  raw_ptr<Browser> browser_;
  raw_ptr<UrlLoadingNotifierBrowserAgent> notifier_ = nullptr;
  raw_ptr<UrlLoadingBrowserAgent> incognito_loader_ = nullptr;
  raw_ptr<SceneUrlLoadingService> scene_service_ = nullptr;

  base::WeakPtrFactory<UrlLoadingBrowserAgent> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_MODEL_URL_LOADING_BROWSER_AGENT_H_
