// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_snapshot_controller.h"

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"

LensOverlaySnapshotController::LensOverlaySnapshotController(
    SnapshotTabHelper* snapshot_tab_helper,
    FullscreenController* fullscreen_controller,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : snapshot_tab_helper_(snapshot_tab_helper),
      fullscreen_controller_(fullscreen_controller),
      task_runner_(std::move(task_runner)) {}

LensOverlaySnapshotController::~LensOverlaySnapshotController() {
  fullscreen_controller_->RemoveObserver(this);
  FinalizeCapturing();
}

void LensOverlaySnapshotController::CaptureFullscreenSnapshot(
    SnapshotCallback callback) {
  task_tracker_.PostTask(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&LensOverlaySnapshotController::OnSnapshotCallbackRecorded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
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

// - Private -
void LensOverlaySnapshotController::OnSnapshotCallbackRecorded(
    SnapshotCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_snapshot_callbacks_.push_back(std::move(callback));

  // If there is a capture in progress wait for it to complete.
  if (is_capturing_) {
    return;
  }

  // If fullscreen is already enabled directly take the screenshot.
  bool is_already_fullscreen = fullscreen_controller_->GetProgress() == 0.0;
  if (is_already_fullscreen) {
    OnFullscreenStateSettled();
    return;
  }

  // Register as observer and request fullscreen.
  BeginCapturing();
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

UIEdgeInsets LensOverlaySnapshotController::GetSnapshotInsets() {
  return fullscreen_controller_->IsEnabled()
             ? fullscreen_controller_->GetMinViewportInsets()
             : fullscreen_controller_->GetMaxViewportInsets();
}

// Fullscreen has got to a steady state, either by already being in a fullscreen
// state, completing an animation or being unable to change state.
// Regardless, a screenshot is taken when such state is reached.
void LensOverlaySnapshotController::OnFullscreenStateSettled() {
  if (!is_capturing_) {
    return;
  }

  UIImage* snapshot = snapshot_tab_helper_->GenerateSnapshotWithoutOverlays();

  // The snapshot taken was only of the visible content on the screen. To make
  // it appear fullscreen, add a solid color fill at the top and bottom of the
  // image corresponding to the initial insets.
  UIEdgeInsets viewportInsets = GetSnapshotInsets();
  CGFloat newSnapshotHeight =
      snapshot.size.height + viewportInsets.top + viewportInsets.bottom;
  CGSize newSnapshotSize = CGSizeMake(snapshot.size.width, newSnapshotHeight);
  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:newSnapshotSize];
  UIImage* snapshotWithInfill =
      [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
        [[UIColor whiteColor] setFill];
        UIRectFill(context.format.bounds);
        [snapshot drawAtPoint:CGPointMake(0, viewportInsets.top)];
      }];

  // Consume and clear the pending callbacks storage.
  for (auto& callback : pending_snapshot_callbacks_) {
    std::move(callback).Run(snapshotWithInfill);
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
