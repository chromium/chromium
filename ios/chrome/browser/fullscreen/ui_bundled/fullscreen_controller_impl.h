// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FULLSCREEN_UI_BUNDLED_FULLSCREEN_CONTROLLER_IMPL_H_
#define IOS_CHROME_BROWSER_FULLSCREEN_UI_BUNDLED_FULLSCREEN_CONTROLLER_IMPL_H_

#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_browser_observer.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_mediator.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_model.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_web_state_list_observer.h"

class Browser;
@class ChromeBroadcastOberverBridge;
@class FullscreenSystemNotificationObserver;
enum class FullscreenExitReason;

// Implementation of FullscreenController.
class FullscreenControllerImpl : public FullscreenController {
 public:
  explicit FullscreenControllerImpl(Browser* browser);

  // Not copyable or movable
  FullscreenControllerImpl(const FullscreenControllerImpl&) = delete;
  FullscreenControllerImpl& operator=(const FullscreenControllerImpl&) = delete;
  FullscreenControllerImpl(FullscreenControllerImpl&&) = delete;
  FullscreenControllerImpl& operator=(FullscreenControllerImpl&&) = delete;

  ~FullscreenControllerImpl() override;

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
  // Needs to be cleanup.
  void ExitFullscreen() override;
  void ExitFullscreen(FullscreenExitReason fullscreen_exit_reason) override;
  void ExitFullscreenWithoutAnimation() override;
  bool IsForceFullscreenMode() const override;
  void EnterForceFullscreenMode(bool insets_update_enabled) override;
  void ExitForceFullscreenMode() override;
  void ResizeHorizontalViewport() override;
  void SetToolbarsSize(ToolbarsSize* toolbars_size) override;
  ToolbarsSize* GetToolbarsSize() const override;

 private:
  // The broadcaster that drives the model.
  __strong ChromeBroadcaster* broadcaster_ = nil;
  // The model used to calculate fullscreen state.
  std::unique_ptr<FullscreenModel> model_ = nullptr;
  // Object that manages sending signals to FullscreenControllerImplObservers.
  FullscreenMediator mediator_;
  // A WebStateListObserver that updates `model_` for WebStateList changes.
  FullscreenWebStateListObserver web_state_list_observer_;
  // A FullscreenBrowserObserver that resets `web_state_list_` when the Browser
  // is destroyed.
  FullscreenBrowserObserver fullscreen_browser_observer_;
  // The bridge used to forward brodcasted UI to `model_`.
  __strong ChromeBroadcastOberverBridge* bridge_ = nil;
  // A helper object that listens for system notifications.
  __strong FullscreenSystemNotificationObserver* notification_observer_ = nil;
  // The `toolbars_size_` that is used to determine the toolbar's visibility.
  // TODO(crbug.com/397683088): Move this to the browser so that it is not a
  // dependency of fullscreen.
  __strong ToolbarsSize* toolbars_size_ = nil;
};

#endif  // IOS_CHROME_BROWSER_FULLSCREEN_UI_BUNDLED_FULLSCREEN_CONTROLLER_IMPL_H_
