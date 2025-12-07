// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FULLSCREEN_UI_BUNDLED_FULLSCREEN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_FULLSCREEN_UI_BUNDLED_FULLSCREEN_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import <optional>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/observer_list.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_animator.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_metrics.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_model_observer.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_reason.h"

class FullscreenController;
class FullscreenControllerObserver;
enum class FullscreenModelScrollDirection;
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

  FullscreenMediator(const FullscreenMediator&) = delete;
  FullscreenMediator& operator=(const FullscreenMediator&) = delete;

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
  void ExitFullscreen(FullscreenExitReason fullscreen_exit_reason);

  // Force enters fullscreen without animation. This enters fullscreen even when
  // the model is disabled.
  void ForceEnterFullscreen();
  // Exits fullscreen without animation.
  void ExitFullscreenWithoutAnimation();

  // Instructs the mediator to stop observing its model.
  void Disconnect();

  // Instructs the mediator to signal the need to resize the horizontal insets.
  // TODO(crbug.com/40143738) remove after fixing multiwindow resizing issue.
  void ResizeHorizontalInsets();

 private:
  // FullscreenModelObserver:
  void FullscreenModelToolbarHeightsUpdated(FullscreenModel* model) override;
  void FullscreenModelProgressUpdated(FullscreenModel* model) override;
  void FullscreenModelEnabledStateChanged(FullscreenModel* model) override;
  void FullscreenModelScrollEventStarted(FullscreenModel* model) override;
  void FullscreenModelScrollEventEnded(FullscreenModel* model) override;
  void FullscreenModelWasReset(FullscreenModel* model) override;

  // Sets up `animator_` with `style`, notifies FullscreenControllerObservers,
  // and starts the animation.
  void AnimateWithStyle(FullscreenAnimatorStyle style);

  // Stops the current scroll end animation if one is in progress.  If
  // `update_model` is true, the FullscreenModel will be updated with the active
  // animator's current progress value.
  void StopAnimating(bool update_model);

  // Returns the animator style given a scroll direction.
  FullscreenAnimatorStyle AnimatorStyleFromScrollDirection(
      FullscreenModelScrollDirection direction);

  // Records fullscreen exit entrypoints in a histogram.
  void RecordFullscreenExitMode();

  // Converts `FullscreenExitReason` enum to `FullscreenModeTransitionReason`
  // enum.
  FullscreenModeTransitionReason ConvertFullscreenExitReasonToTransitionReason(
      FullscreenExitReason exit_reason);

  // Progress value when scroll event started.
  float start_progress_;
  // The controller.
  raw_ptr<FullscreenController> controller_ = nullptr;
  // The model.
  raw_ptr<FullscreenModel> model_ = nullptr;
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
  base::ObserverList<FullscreenControllerObserver, true> observers_;
  // Type of entrypoint that triggers the exit of fullscreen mode.
  std::optional<FullscreenExitReason> fullscreen_exit_reason_;
  // Whether the user has scrolled to the bottom of the page for the first time
  // on the current page. This is reset to false as soon as the user scrolls up.
  bool has_reached_bottom_once_ = false;
  base::WeakPtrFactory<FullscreenMediator> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_FULLSCREEN_UI_BUNDLED_FULLSCREEN_MEDIATOR_H_
