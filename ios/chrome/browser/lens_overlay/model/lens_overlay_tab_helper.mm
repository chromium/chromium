// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"

#import "base/check_op.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_snapshot_controller.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"

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

void LensOverlayTabHelper::SetLensOverlayUIAttachedAndAlive(
    bool is_ui_attached_and_alive) {
  is_ui_attached_and_alive_ = is_ui_attached_and_alive;
  if (IsLensOverlaySameTabNavigationEnabled() && is_ui_attached_and_alive &&
      web_state_) {
    invokation_navigation_id_ =
        web_state_->GetNavigationManager()->GetVisibleItem()->GetUniqueID();
  } else {
    invokation_navigation_id_ = 0;
  }
}

#pragma mark - WebStateObserver

void LensOverlayTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (IsLensOverlaySameTabNavigationEnabled() && is_ui_attached_and_alive_ &&
      navigation_context && !navigation_context->IsSameDocument()) {
    if (invokation_navigation_id_ ==
        web_state_->GetNavigationManager()->GetPendingItem()->GetUniqueID()) {
      [commands_handler_ showLensUI:NO];
    } else {
      [commands_handler_ hideLensUI:NO];
    }
  }
  if (web_state_ && snapshot_controller_) {
    NewTabPageTabHelper* NTPHelper =
        NewTabPageTabHelper::FromWebState(web_state_);
    bool is_NTP = NTPHelper && NTPHelper->IsActive();
    bool is_pdf = web_state_->GetContentsMimeType() == kMimeTypePDF;
    snapshot_controller_->SetIsPDFDocument(is_pdf);
    snapshot_controller_->SetIsNTP(is_NTP);
  }
}

void LensOverlayTabHelper::WasShown(web::WebState* web_state) {
  CHECK_EQ(web_state, web_state_, kLensOverlayNotFatalUntil);

  if (is_ui_attached_and_alive_) {
    [commands_handler_ showLensUI:YES];
  }
}

void LensOverlayTabHelper::WasHidden(web::WebState* web_state) {
  CHECK_EQ(web_state, web_state_, kLensOverlayNotFatalUntil);

  if (snapshot_controller_) {
    snapshot_controller_->CancelOngoingCaptures();
  }

  if (is_ui_attached_and_alive_) {
    [commands_handler_ hideLensUI:YES];
  }
}

void LensOverlayTabHelper::WebStateDestroyed(web::WebState* web_state) {
  CHECK_EQ(web_state, web_state_, kLensOverlayNotFatalUntil);

  if (snapshot_controller_) {
    snapshot_controller_->CancelOngoingCaptures();
  }

  if (is_ui_attached_and_alive_) {
    [commands_handler_
        destroyLensUI:NO
               reason:lens::LensOverlayDismissalSource::kTabClosed];
  }
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

void LensOverlayTabHelper::RecordViewportSnaphot() {
  if (snapshot_controller_) {
    viewport_snapshot_ = snapshot_controller_->CaptureSnapshotOfBaseWindow();
  }
}

void LensOverlayTabHelper::UpdateSnapshot() {
  SnapshotTabHelper* snapshotTabHelper =
      SnapshotTabHelper::FromWebState(web_state_);

  if (!snapshotTabHelper) {
    return;
  }

  is_updating_tab_switcher_snapshot_ = true;
  base::WeakPtr<LensOverlayTabHelper> weakThis = weak_ptr_factory_.GetWeakPtr();
  snapshotTabHelper->UpdateSnapshotWithCallback(^(UIImage* image) {
    if (weakThis) {
      weakThis->is_updating_tab_switcher_snapshot_ = false;
    }
  });
}

void LensOverlayTabHelper::UpdateSnapshotStorage() {
  SnapshotTabHelper* snapshotTabHelper =
      SnapshotTabHelper::FromWebState(web_state_);

  if (!snapshotTabHelper || !viewport_snapshot_ || !snapshot_controller_) {
    return;
  }

  UIImage* snapshot =
      snapshot_controller_->CropSnapshotToWindowSafeArea(viewport_snapshot_);
  if (snapshot) {
    snapshotTabHelper->UpdateSnapshotStorageWithImage(snapshot);
  }
}

void LensOverlayTabHelper::SetSnapshotController(
    std::unique_ptr<LensOverlaySnapshotController> snapshot_controller) {
  snapshot_controller_ = std::move(snapshot_controller);
  snapshot_controller_->SetDelegate(weak_ptr_factory_.GetWeakPtr());

  if (web_state_ && snapshot_controller_) {
    NewTabPageTabHelper* NTPHelper =
        NewTabPageTabHelper::FromWebState(web_state_);
    bool is_NTP = NTPHelper && NTPHelper->IsActive();
    bool is_pdf = web_state_->GetContentsMimeType() == kMimeTypePDF;
    snapshot_controller_->SetIsPDFDocument(is_pdf);
    snapshot_controller_->SetIsNTP(is_NTP);
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

void LensOverlayTabHelper::ReleaseSnapshotAuxiliaryWindows() {
  if (snapshot_controller_) {
    snapshot_controller_->ReleaseAuxiliaryWindows();
  }
}

UIEdgeInsets LensOverlayTabHelper::GetSnapshotInsets() {
  DCHECK(snapshot_controller_);
  return snapshot_controller_->GetSnapshotInsets();
}

WEB_STATE_USER_DATA_KEY_IMPL(LensOverlayTabHelper)
