// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"

#import "base/check_op.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_snapshot_controller.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"

namespace {

// MIME type of PDF.
const char kMimeTypePDF[] = "application/pdf";

}  // namespace

LensOverlayTabHelper::LensOverlayTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  CHECK(IsLensOverlayAvailable());
  web_state->AddObserver(this);
}

LensOverlayTabHelper::~LensOverlayTabHelper() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

#pragma mark - WebStateObserver
void LensOverlayTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (web_state_ && snapshot_controller_) {
    bool is_pdf = web_state_->GetContentsMimeType() == kMimeTypePDF;
    snapshot_controller_->SetIsPDFDocument(is_pdf);
  }
}

void LensOverlayTabHelper::WasShown(web::WebState* web_state) {
  CHECK_EQ(web_state, web_state_, kLensOverlayNotFatalUntil);

  if (is_showing_lens_overlay_) {
    [commands_handler_ showLensUI:YES];
  }
}

void LensOverlayTabHelper::WasHidden(web::WebState* web_state) {
  CHECK_EQ(web_state, web_state_, kLensOverlayNotFatalUntil);

  if (snapshot_controller_) {
    snapshot_controller_->CancelOngoingCaptures();
  }

  if (is_showing_lens_overlay_) {
    // Prior to hiding the UI update the snapshot to ensure lens overlay is
    // visible in the tab switcher.
    UpdateSnapshot();

    [commands_handler_ hideLensUI:YES];
  }
}

void LensOverlayTabHelper::WebStateDestroyed(web::WebState* web_state) {
  CHECK_EQ(web_state, web_state_, kLensOverlayNotFatalUntil);

  if (snapshot_controller_) {
    snapshot_controller_->CancelOngoingCaptures();
  }

  if (is_showing_lens_overlay_) {
    [commands_handler_
        destroyLensUI:NO
               reason:lens::LensOverlayDismissalSource::kTabClosed];
  }
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

void LensOverlayTabHelper::UpdateSnapshot() {
  SnapshotTabHelper* snapshotTabHelper =
      SnapshotTabHelper::FromWebState(web_state_);

  if (snapshotTabHelper) {
    is_updating_tab_switcher_snapshot_ = true;
    base::WeakPtr<LensOverlayTabHelper> weakThis =
        weak_ptr_factory_.GetWeakPtr();
    snapshotTabHelper->UpdateSnapshotWithCallback(^(UIImage* image) {
      if (weakThis) {
        weakThis->is_updating_tab_switcher_snapshot_ = false;
      }
    });
  }
}

void LensOverlayTabHelper::SetSnapshotController(
    std::unique_ptr<LensOverlaySnapshotController> snapshot_controller) {
  snapshot_controller_ = std::move(snapshot_controller);
  snapshot_controller_->SetDelegate(weak_ptr_factory_.GetWeakPtr());

  if (web_state_ && snapshot_controller_) {
    bool is_pdf = web_state_->GetContentsMimeType() == kMimeTypePDF;
    snapshot_controller_->SetIsPDFDocument(is_pdf);
  }
}

void LensOverlayTabHelper::OnSnapshotCaptureBegin() {
  if (snapshot_controller_) {
    is_capturing_lens_overlay_snapshot_ = true;
  }
}

void LensOverlayTabHelper::OnSnapshotCaptureEnd() {
  is_capturing_lens_overlay_snapshot_ = false;
}

void LensOverlayTabHelper::CaptureFullscreenSnapshot(
    SnapshotCallback callback) {
  DCHECK(snapshot_controller_);
  if (snapshot_controller_) {
    snapshot_controller_->CaptureFullscreenSnapshot(std::move(callback));
  } else {
    std::move(callback).Run(nil);
  }
}

UIEdgeInsets LensOverlayTabHelper::GetSnapshotInsets() {
  DCHECK(snapshot_controller_);
  return snapshot_controller_->GetSnapshotInsets();
}

WEB_STATE_USER_DATA_KEY_IMPL(LensOverlayTabHelper)
