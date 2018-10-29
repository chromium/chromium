// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_IMPL_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_IMPL_H_

#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_mediator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_state_list_observer.h"

@class ChromeBroadcastOberverBridge;
@class FullscreenSystemNotificationObserver;

// Implementation of FullscreenController.
class FullscreenControllerImpl : public FullscreenController {
 public:
  explicit FullscreenControllerImpl();
  ~FullscreenControllerImpl() override;

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
  CGFloat GetProgress() const override;
  void EnterFullscreen() override;
  void ExitFullscreen() override;

 private:
  // KeyedService:
  void Shutdown() override;

  // The broadcaster that drives the model.
  __strong ChromeBroadcaster* broadcaster_ = nil;
  // The model used to calculate fullscreen state.
  FullscreenModel model_;
  // Object that manages sending signals to FullscreenControllerImplObservers.
  FullscreenMediator mediator_;
  // A WebStateListObserver that updates |model_| for WebStateList changes.
  FullscreenWebStateListObserver web_state_list_observer_;
  // The bridge used to forward brodcasted UI to |model_|.
  __strong ChromeBroadcastOberverBridge* bridge_ = nil;
  // A helper object that listens for system notifications.
  __strong FullscreenSystemNotificationObserver* notification_observer_ = nil;

  DISALLOW_COPY_AND_ASSIGN(FullscreenControllerImpl);
};

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_IMPL_H_
