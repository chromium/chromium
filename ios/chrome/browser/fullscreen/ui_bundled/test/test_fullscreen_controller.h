// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FULLSCREEN_UI_BUNDLED_TEST_TEST_FULLSCREEN_CONTROLLER_H_
#define IOS_CHROME_BROWSER_FULLSCREEN_UI_BUNDLED_TEST_TEST_FULLSCREEN_CONTROLLER_H_

#import "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"

class FullscreenModel;
@class FullscreenAnimator;
enum class FullscreenExitReason;

// Test version of FullscreenController with limited functionality:
// - Enables/disables a FullscreenModel.
// - Exposes a broadcaster.
// - Supports FullscreenControllerObserver::FullscreenControllerWillShutDown().
class TestFullscreenController : public FullscreenController {
 public:
  TestFullscreenController(Browser* browser);
  ~TestFullscreenController() override;

  // Overrides FullscreenController::CreateForBrowser(...) for tests.
  static void CreateForBrowser(Browser* browser);

  // Overrides FullscreenController::FromBrowser(...) for tests.
  static TestFullscreenController* FromBrowser(Browser* browser);
  static const TestFullscreenController* FromBrowser(const Browser* browser);

  // FullscreenController:
  ChromeBroadcaster* broadcaster() override;
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
  void ExitFullscreen(FullscreenExitReason fullscreen_exit_reason) override;
  void ExitFullscreenWithoutAnimation() override;
  bool IsForceFullscreenMode() const override;
  void EnterForceFullscreenMode(bool insets_update_enabled) override;
  void ExitForceFullscreenMode() override;
  void ResizeHorizontalViewport() override;
  void SetToolbarsSize(ToolbarsSize* ToolbarsSize) override;
  ToolbarsSize* GetToolbarsSize() const override;

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

  raw_ptr<FullscreenModel> getModel();

 private:
  // The model.
  std::unique_ptr<FullscreenModel> model_ = std::make_unique<FullscreenModel>();
  // The broadcaster.
  ChromeBroadcaster* broadcaster_ = nil;
  // The observers.
  base::ObserverList<FullscreenControllerObserver, true> observers_;
  // Toolbars' size
  ToolbarsSize* toolbars_size_ = nil;
};

#endif  // IOS_CHROME_BROWSER_FULLSCREEN_UI_BUNDLED_TEST_TEST_FULLSCREEN_CONTROLLER_H_
