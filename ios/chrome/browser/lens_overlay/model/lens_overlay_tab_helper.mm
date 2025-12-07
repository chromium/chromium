// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"

#import "base/check_op.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_snapshot_controller.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
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
  CHECK(IsLensOverlayAvailable(GetProfilePrefs()));
  web_state->AddObserver(this);
}

LensOverlayTabHelper::~LensOverlayTabHelper() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
  ReleaseSnapshotAuxiliaryWindows();
}

void LensOverlayTabHelper::SetLensOverlayUIAttachedAndAlive(
    bool is_ui_attached_and_alive) {
  is_ui_attached_and_alive_ = is_ui_attached_and_alive;
  invokation_navigation_id_ = 0;

  if (IsLensOverlaySameTabNavigationEnabled(GetProfilePrefs()) &&
      is_ui_attached_and_alive && web_state_) {
    const web::NavigationManager* navigation_manager =
        web_state_->GetNavigationManager();

    if (navigation_manager && navigation_manager->GetVisibleItem()) {
      invokation_navigation_id_ =
          navigation_manager->GetVisibleItem()->GetUniqueID();
    }
  }
}

bool LensOverlayTabHelper::IsLensOverlayInvokedOnMostRecentBackItem() {
  std::vector<web::NavigationItem*> backItems =
      web_state_->GetNavigationManager()->GetBackwardItems();
  return is_ui_attached_and_alive_ && backItems.size() > 0 &&
         invokation_navigation_id_ == backItems[0]->GetUniqueID();
}

bool LensOverlayTabHelper::IsLensOverlayInvokedOnCurrentNavigationItem() {
  if (!is_ui_attached_and_alive_) {
    return false;
  }

  bool is_lens_overlay_invoked = false;

  if (web_state_->GetNavigationManager() &&
      web_state_->GetNavigationManager()->GetVisibleItem()) {
    is_lens_overlay_invoked =
        invokation_navigation_id_ ==
        web_state_->GetNavigationManager()->GetVisibleItem()->GetUniqueID();
  }

  return is_lens_overlay_invoked;
}

#pragma mark - WebStateObserver

void LensOverlayTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  BOOL usesSameTabNavigation =
      IsLensOverlaySameTabNavigationEnabled(GetProfilePrefs());
  if (!usesSameTabNavigation) {
    // If a new navigation starts without same tab navigation enabled, proceed
    // to destoroy the now stale Lens UI. This can also be caused by a reload
    // or a back button navigation.
    if (is_ui_attached_and_alive_) {
      [commands_handler_
          destroyLensUI:NO
                 reason:lens::LensOverlayDismissalSource::kPageChanged];
    }

    return;
  }

  const web::NavigationManager* navigation_manager =
      web_state_->GetNavigationManager();
  const web::NavigationItem* pending_item =
      navigation_manager ? navigation_manager->GetPendingItem() : nullptr;

  if (is_ui_attached_and_alive_ && navigation_context &&
      !navigation_context->IsSameDocument() && pending_item) {
    if (invokation_navigation_id_ == pending_item->GetUniqueID()) {
      [commands_handler_ showLensUI:NO];
    } else {
      [commands_handler_ hideLensUI:NO completion:nil];
    }
  }
}

void LensOverlayTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  const web::NavigationManager* navigation_manager =
      web_state_->GetNavigationManager();
  const web::NavigationItem* navigation_item =
      navigation_manager ? navigation_manager->GetVisibleItem() : nullptr;

  // Fallback if invokation failed during startNavigation (e.g GetPendingItem
  // returns null)
  if (IsLensOverlaySameTabNavigationEnabled(GetProfilePrefs()) &&
      is_ui_attached_and_alive_ && navigation_item) {
    if (invokation_navigation_id_ == navigation_item->GetUniqueID()) {
      [commands_handler_ showLensUI:NO];
    } else {
      [commands_handler_ hideLensUI:NO completion:nil];
    }
  }
}

void LensOverlayTabHelper::WasShown(web::WebState* web_state) {
  CHECK_EQ(web_state, web_state_, kLensOverlayNotFatalUntil);

  BOOL showAnimated = NO;
  if (IsLensOverlaySameTabNavigationEnabled(GetProfilePrefs())) {
    if (web_state_->GetNavigationManager()) {
      web::NavigationItem* visibleItem =
          web_state_->GetNavigationManager()->GetVisibleItem();

      if (is_ui_attached_and_alive_ && visibleItem &&
          invokation_navigation_id_ == visibleItem->GetUniqueID()) {
        [commands_handler_ showLensUI:showAnimated];
      }
    }
  } else if (is_ui_attached_and_alive_) {
    [commands_handler_ showLensUI:showAnimated];
  }
}

void LensOverlayTabHelper::WasHidden(web::WebState* web_state) {
  CHECK_EQ(web_state, web_state_, kLensOverlayNotFatalUntil);

  if (snapshot_controller_) {
    snapshot_controller_->CancelOngoingCaptures();
  }

  if (is_ui_attached_and_alive_) {
    [commands_handler_ hideLensUI:NO completion:nil];
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
  // Skip updating the snapshot storage if the Lens Overlay is not invoked on
  // the current navigation item.
  if (IsLensOverlaySameTabNavigationEnabled(GetProfilePrefs()) &&
      !IsLensOverlayInvokedOnCurrentNavigationItem()) {
    return;
  }

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
    bool is_NTP = IsVisibleURLNewTabPage(web_state_);
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

PrefService* LensOverlayTabHelper::GetProfilePrefs() {
  CHECK(web_state_, kLensOverlayNotFatalUntil);
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  return profile->GetPrefs();
}
