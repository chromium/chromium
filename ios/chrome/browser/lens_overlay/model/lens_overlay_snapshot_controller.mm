// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_snapshot_controller.h"

#import "base/task/bind_post_task.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

LensOverlaySnapshotController::LensOverlaySnapshotController(
    SnapshotTabHelper* snapshot_tab_helper,
    FullscreenController* fullscreen_controller)
    : snapshot_tab_helper_(snapshot_tab_helper),
      fullscreen_controller_(fullscreen_controller),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

LensOverlaySnapshotController::~LensOverlaySnapshotController() {
  fullscreen_controller_->RemoveObserver(this);
  FinalizeCapturing();
}

void LensOverlaySnapshotController::CaptureFullscreenSnapshot(
    SnapshotCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_snapshot_callbacks_.push_back(std::move(callback));

  // If there is a capture in progress wait for it to complete.
  if (is_capturing_) {
    return;
  }

  // All the steps should be synchronized on the same sequence as the one that
  // initiated the capture.
  task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();

  BeginCapturing();

  // If fullscreen is already enabled directly take the screenshot.
  bool is_already_fullscreen = fullscreen_controller_->GetProgress() == 0.0;
  if (is_already_fullscreen) {
    OnFullscreenStateSettled();
    return;
  }

  // Register as observer and request fullscreen.
  fullscreen_controller_->AddObserver(this);
  if (fullscreen_controller_->IsEnabled()) {
    // Enter fullscreen and rely on the update from the fullscreen controller.
    fullscreen_controller_->EnterFullscreen();
  } else {
    // Fullscreen could not be requested, likely because the content is too
    // small to enlarge the view. Go straight to fetching a screenshot.
    OnFullscreenStateSettled();
  }
}

void LensOverlaySnapshotController::FullscreenDidAnimate(
    FullscreenController* controller,
    FullscreenAnimatorStyle style) {
  DCHECK(controller == this->fullscreen_controller_);

  // Progress of 0.0 means that the toolbar is completely hidden.
  bool is_fullscreen = fullscreen_controller_->GetProgress() == 0.0;
  if (!is_fullscreen) {
    return;
  }

  task_tracker_.PostTask(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&LensOverlaySnapshotController::OnFullscreenStateSettled,
                     weak_ptr_factory_.GetWeakPtr()));
}

// The inset amount of the content relative to the device screen when the
// snapshot is taken.
UIEdgeInsets
LensOverlaySnapshotController::GetContentInsetsOnSnapshotCapture() {
  return fullscreen_controller_->IsEnabled()
             ? fullscreen_controller_->GetMinViewportInsets()
             : fullscreen_controller_->GetMaxViewportInsets();
}

UIEdgeInsets LensOverlaySnapshotController::GetSnapshotInsets() {
  // If the fullscreen mode is achieved by adjusting the size of the scroll
  // view, the WebState view is already positioned correctly within the viewable
  // area and doesn't require any further adjustments.
  //
  // Note: In practice, this condition is true only when fullscreen smooth
  // scrolling of the default view port is disabled.
  if (fullscreen_controller_->ResizesScrollView()) {
    return UIEdgeInsetsZero;
  }

  // If the fullscreen mode is implemented using content insets, the WebState
  // view needs to be adjusted inwards by the viewport insets.
  return GetContentInsetsOnSnapshotCapture();
}

// Fullscreen has got to a steady state, either by already being in a fullscreen
// state, completing an animation or being unable to change state.
// Regardless, a screenshot is taken when such state is reached.
void LensOverlaySnapshotController::OnFullscreenStateSettled() {
  if (!is_capturing_) {
    return;
  }

  base::OnceCallback<void(UIImage*)> snapshotCapturedCallback =
      base::BindOnce(&LensOverlaySnapshotController::OnSnapshotCaptured,
                     weak_ptr_factory_.GetWeakPtr());

  auto callbackOnWorkingSequence = base::BindPostTask(
      task_runner_.get(), std::move(snapshotCapturedCallback));

  // TODO(crbug.com/365732763): Replace call to `UpdateSnapshotWithCallback`
  // once the new API method is exposed.
  snapshot_tab_helper_->UpdateSnapshotWithCallback(
      base::CallbackToBlock(std::move(callbackOnWorkingSequence)));
}

void LensOverlaySnapshotController::OnSnapshotCaptured(UIImage* snapshot) {
  if (!is_capturing_) {
    return;
  }

  // The snapshot taken was only of the visible content on the screen. To make
  // it appear fullscreen, add a solid color fill at the top and bottom of the
  // image corresponding to the initial insets.
  UIEdgeInsets viewportInsets = GetContentInsetsOnSnapshotCapture();
  CGFloat newSnapshotHeight =
      snapshot.size.height + viewportInsets.top + viewportInsets.bottom;
  CGSize newSnapshotSize = CGSizeMake(snapshot.size.width, newSnapshotHeight);
  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:newSnapshotSize];
  UIImage* snapshotWithInfill =
      [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
        [[UIColor colorNamed:kBackgroundColor] setFill];
        UIRectFill(context.format.bounds);
        [snapshot drawAtPoint:CGPointMake(0, viewportInsets.top)];
      }];

  // Lens requires the image to be 1.0 scale.
  UIImage* rescaledSnapshot =
      [[UIImage alloc] initWithCGImage:snapshotWithInfill.CGImage
                                 scale:1
                           orientation:UIImageOrientationUp];

  // Consume and clear the pending callbacks storage.
  for (auto& callback : pending_snapshot_callbacks_) {
    std::move(callback).Run(rescaledSnapshot);
  }

  fullscreen_controller_->RemoveObserver(this);
  FinalizeCapturing();
}

void LensOverlaySnapshotController::CancelOngoingCaptures() {
  task_tracker_.TryCancelAll();

  for (auto& callback : pending_snapshot_callbacks_) {
    std::move(callback).Run(nil);
  }

  fullscreen_controller_->RemoveObserver(this);
  FinalizeCapturing();
}

void LensOverlaySnapshotController::BeginCapturing() {
  is_capturing_ = true;
  if (delegate_) {
    delegate_->OnSnapshotCaptureBegin();
  }
}

void LensOverlaySnapshotController::FinalizeCapturing() {
  pending_snapshot_callbacks_.clear();
  is_capturing_ = false;
  if (delegate_) {
    delegate_->OnSnapshotCaptureEnd();
  }
}
