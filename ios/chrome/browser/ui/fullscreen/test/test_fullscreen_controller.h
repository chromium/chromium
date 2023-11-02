// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_TEST_TEST_FULLSCREEN_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_TEST_TEST_FULLSCREEN_CONTROLLER_H_

#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"

#include "base/observer_list.h"

class FullscreenModel;
@class FullscreenAnimator;

// Test version of FullscreenController with limited functionality:
// - Enables/disables a FullscreenModel.
// - Exposes a broadcaster.
// - Supports FullscreenControllerObserver::FullscreenControllerWillShutDown().
class TestFullscreenController : public FullscreenController {
 public:
  explicit TestFullscreenController(FullscreenModel* model);
  ~TestFullscreenController() override;

  // FullscreenController:
  ChromeBroadcaster* broadcaster() override;
  void SetWebStateList(WebStateList* web_state_list) override;
  const WebStateList* GetWebStateList() const override;
  WebStateList* GetWebStateList() override;
  void AddObserver(FullscreenControllerObserver* observer) override;
  void RemoveObserver(FullscreenControllerObserver* observer) override;
  bool IsEnabled() const override;
  void IncrementDisabledCounter() override;
  void DecrementDisabledCounter() override;
  bool ResizesScrollView() const override;
  void BrowserTraitCollectionChangedBegin() override;
  void BrowserTraitCollectionChangedEnd() override;
  CGFloat GetProgress() const override;
  UIEdgeInsets GetMinViewportInsets() const override;
  UIEdgeInsets GetMaxViewportInsets() const override;
  UIEdgeInsets GetCurrentViewportInsets() const override;
  void EnterFullscreen() override;
  void ExitFullscreen() override;
  void ResizeHorizontalViewport() override;
  void FreezeToolbarHeight(bool freeze_toolbar_height) override;

  // Calls FullscreenViewportInsetRangeChanged() on observers.
  void OnFullscreenViewportInsetRangeChanged(UIEdgeInsets min_viewport_insets,
                                             UIEdgeInsets max_viewport_insets);
  // Calls FullscreenProgressUpdated() on observers.
  void OnFullscreenProgressUpdated(CGFloat progress);
  // Calls FullscreenEnabledStateChanged() on observers.
  void OnFullscreenEnabledStateChanged(bool enabled);
  // Calls FullscreenWillAnimate() on observers.
  void OnFullscreenWillAnimate(FullscreenAnimator* animator);

  // Returns the UserDataKey, used to set the FullscreenController for a
  // browser.
  static const void* UserDataKeyForTesting();

 private:
  // The model.
  FullscreenModel* model_ = nullptr;
  // The WebStateList.
  WebStateList* web_state_list_ = nullptr;
  // The broadcaster.
  ChromeBroadcaster* broadcaster_ = nil;
  // The observers.
  base::ObserverList<FullscreenControllerObserver>::Unchecked observers_;
};

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_TEST_TEST_FULLSCREEN_CONTROLLER_H_
