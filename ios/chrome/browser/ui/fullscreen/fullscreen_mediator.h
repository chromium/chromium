// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_MEDIATOR_H_

#import <Foundation/Foundation.h>
#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model_observer.h"

class FullscreenController;
class FullscreenControllerObserver;
@class FullscreenResetAnimator;
@class FullscreenScrollEndAnimator;
@class FullscreenScrollToTopAnimator;
@class FullscreenWebViewResizer;
@class ToolbarRevealAnimator;

namespace web {
class WebState;
}

// A helper object that listens to FullscreenModel changes and forwards this
// information to FullscreenControllerObservers.
class FullscreenMediator : public FullscreenModelObserver {
 public:
  FullscreenMediator(FullscreenController* controller, FullscreenModel* model);
  ~FullscreenMediator() override;

  // Adds and removes FullscreenControllerObservers.
  void AddObserver(FullscreenControllerObserver* observer) {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(FullscreenControllerObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  // Sets the WebState which view is to be resized.
  void SetWebState(web::WebState* webState);

  // Sets whether the browser view is currently handling a trait collection
  // update.  UI changes received through the broadcaster must be handled
  // differently when received for trait collection updates as opposed to normal
  // rendering and scrolling events.
  void SetIsBrowserTraitCollectionUpdating(bool updating);

  // Enters or exits fullscreen, animating the changes.
  void EnterFullscreen();
  void ExitFullscreen();

  // Instructs the mediator to stop observing its model.
  void Disconnect();

 private:
  // FullscreenModelObserver:
  void FullscreenModelToolbarHeightsUpdated(FullscreenModel* model) override;
  void FullscreenModelProgressUpdated(FullscreenModel* model) override;
  void FullscreenModelEnabledStateChanged(FullscreenModel* model) override;
  void FullscreenModelScrollEventStarted(FullscreenModel* model) override;
  void FullscreenModelScrollEventEnded(FullscreenModel* model) override;
  void FullscreenModelWasReset(FullscreenModel* model) override;

  // Sets up |animator_| with |style|, notifies FullscreenControllerObservers,
  // and starts the animation.
  void AnimateWithStyle(FullscreenAnimatorStyle style);

  // Stops the current scroll end animation if one is in progress.  If
  // |update_model| is true, the FullscreenModel will be updated with the active
  // animator's current progress value.
  void StopAnimating(bool update_model);

  // The controller.
  FullscreenController* controller_ = nullptr;
  // The model.
  FullscreenModel* model_ = nullptr;
  // The active animator.
  __strong FullscreenAnimator* animator_ = nil;
  // Fullscreen resizer, used to resize the WebView based on the fullscreen
  // progress.
  FullscreenWebViewResizer* resizer_ = nil;
  // Whether the browser's trait collection is being updated.
  bool updating_browser_trait_collection_ = false;
  // Whether the content view was scrolled to the top when the browser trait
  // collection began updating.
  bool scrolled_to_top_during_trait_collection_updates_ = false;
  // The FullscreenControllerObservers that need to get notified of model
  // changes.
  base::ObserverList<FullscreenControllerObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenMediator);
};

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_MEDIATOR_H_
