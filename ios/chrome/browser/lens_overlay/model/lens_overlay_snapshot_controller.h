// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_SNAPSHOT_CONTROLLER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_SNAPSHOT_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include "base/functional/callback_forward.h"
#import "base/memory/raw_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_snapshot_controller_delegate.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_observer.h"

class SnapshotTabHelper;
class FullscreenController;
enum class FullscreenAnimatorStyle : short;

class LensOverlaySnapshotController final
    : public FullscreenControllerObserver {
 public:
  LensOverlaySnapshotController(SnapshotTabHelper* snapshot_tab_helper,
                                FullscreenController* fullscreen_controller);

  LensOverlaySnapshotController(const SnapshotTabHelper&) = delete;
  LensOverlaySnapshotController& operator=(const SnapshotTabHelper&) = delete;

  ~LensOverlaySnapshotController() override;

  // Sets the delegate.
  void SetDelegate(
      base::WeakPtr<LensOverlaySnapshotControllerDelegate> delegate) {
    delegate_ = delegate;
  }

  using SnapshotCallback = base::OnceCallback<void(UIImage*)>;

  // Enters fullscreen and captures a new snapshot when the animation is
  // complete.
  void CaptureFullscreenSnapshot(SnapshotCallback);

  // Tears down any in flight screenshot requests.
  void CancelOngoingCaptures();

  // Returns the dimensions for the inset area of the lens overlay snapshot.
  UIEdgeInsets GetSnapshotInsets();

  // Sets whether the current web state is of a PDF document or not.
  void SetIsPDFDocument(bool is_pdf_document) {
    is_pdf_document_ = is_pdf_document;
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<SnapshotTabHelper> snapshot_tab_helper_ = nullptr;
  raw_ptr<FullscreenController> fullscreen_controller_ = nullptr;
  base::WeakPtr<LensOverlaySnapshotControllerDelegate> delegate_ = nullptr;

  base::CancelableTaskTracker task_tracker_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::vector<SnapshotCallback> pending_snapshot_callbacks_;
  bool is_capturing_ = false;
  bool is_pdf_document_ = false;
  base::WeakPtrFactory<LensOverlaySnapshotController> weak_ptr_factory_{this};

  void FullscreenDidAnimate(FullscreenController* controller,
                            FullscreenAnimatorStyle style) override;

  void OnFullscreenStateSettled();

  void OnSnapshotCaptured(UIImage*);

  void BeginCapturing();

  void FinalizeCapturing();

  UIEdgeInsets GetContentInsetsOnSnapshotCapture();
};

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_SNAPSHOT_CONTROLLER_H_
