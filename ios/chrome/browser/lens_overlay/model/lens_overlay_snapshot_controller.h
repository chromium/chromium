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
                                FullscreenController* fullscreen_controller,
                                UIWindow* window,
                                bool is_bottom_omnibox);

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

  // Releases all auxiliary windows created as part of the snapshotting process.
  void ReleaseAuxiliaryWindows();

  // Captures a snapshot of the base `UIWindow`.
  UIImage* CaptureSnapshotOfBaseWindow();
  UIImage* CropSnapshotToWindowSafeArea(UIImage* snapshot);

  // Sets whether the current web state is of a PDF document or not.
  void SetIsPDFDocument(bool is_pdf_document) {
    is_pdf_document_ = is_pdf_document;
  }

  // Sets whether the current page is an NTP or not.
  void SetIsNTP(bool is_NTP) { is_NTP_ = is_NTP; }

 private:
  // Invoked when `controller` has finished entering fullscreen.
  void FullscreenDidAnimate(FullscreenController* controller,
                            FullscreenAnimatorStyle style) override;

  // Cover the viewport with static image of the base window.
  void ShowStaticSnapshotOfBaseWindowIfNeeded();

  // Begin executing the snapshotting process.
  void StartSnapshotFlow();

  // Process the raw snapshot by adjusting the size and ifilling the edges with
  // the required colors.
  void ProcessRawSnapshot(UIImage* snapshot);

  // Notifies callers that a snapshot was taken. This method can be called with
  // a `nil` snapshot, signifying that the capturing process either errored or
  // was canceled.
  void NotifySnapshotComplete(UIImage* snapshot);

  // Marks the start of the capturing process and informs the delegate.
  void BeginCapturing();

  // Marks the end of the capturing process and cleans up.
  void FinalizeCapturing();

  // Whether or not it is necessary to show the 'mirror' window.
  bool ShouldShowStaticSnapshot();

  // Computes the insets applied to the content of the raw snapshot to match the
  // size of the window.
  UIEdgeInsets GetContentInsetsOnSnapshotCapture();

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<SnapshotTabHelper> snapshot_tab_helper_ = nullptr;
  raw_ptr<FullscreenController> fullscreen_controller_ = nullptr;
  base::WeakPtr<LensOverlaySnapshotControllerDelegate> delegate_ = nullptr;

  base::CancelableTaskTracker task_tracker_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  __weak UIWindow* base_window_ = nil;
  UIWindow* mirror_window_ = nil;
  std::vector<SnapshotCallback> pending_snapshot_callbacks_;
  bool is_capturing_ = false;
  bool is_bottom_omnibox_ = false;
  bool is_pdf_document_ = false;
  bool is_NTP_ = false;
  CGSize expected_window_size_;

  base::WeakPtrFactory<LensOverlaySnapshotController> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_SNAPSHOT_CONTROLLER_H_
