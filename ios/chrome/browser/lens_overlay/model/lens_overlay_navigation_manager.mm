// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_navigation_manager.h"

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_navigation_mutator.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/public/provider/chrome/browser/lens/lens_overlay_result.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"

#pragma mark - LensResultItem

LensOverlayNavigationManager::LensResultItem::LensResultItem(
    id<ChromeLensOverlayResult> lens_result)
    : lens_result_(lens_result) {
  // Use `isTextSelection`, `selectionRect` and `queryText` to verify that two
  // lens result are similar.
  comparison_key_ = base::SysNSStringToUTF8(
      [NSString stringWithFormat:@"%d%@%@", lens_result.isTextSelection,
                                 NSStringFromCGRect(lens_result.selectionRect),
                                 lens_result.queryText]);
}

#pragma mark - LensOverlayNavigationManager

LensOverlayNavigationManager::LensOverlayNavigationManager(
    id<LensOverlayNavigationMutator> mutator)
    : lens_navigation_items_(), lens_reloaded_items_(), mutator_(mutator) {}

LensOverlayNavigationManager::~LensOverlayNavigationManager() {
  SetWebState(nullptr);
}

void LensOverlayNavigationManager::SetWebState(web::WebState* web_state) {
  if (web_state_) {
    web_state_->RemoveObserver(this);
  }
  web_state_ = web_state;
  if (web_state_) {
    web_state_->AddObserver(this);
  }
}

void LensOverlayNavigationManager::LensOverlayDidGenerateResult(
    id<ChromeLensOverlayResult> result) {
  std::unique_ptr<LensResultItem> item_ptr =
      std::make_unique<LensResultItem>(result);
  const LensResultItem& item = *item_ptr;
  // If `item` is a reloaded navigation.
  if (auto search = lens_reloaded_items_.find(item.comparison_key());
      search != lens_reloaded_items_.end()) {
    // Extract the reloaded index.
    size_t reloaded_index =
        lens_reloaded_items_.extract(search->first).mapped();
    // Discard the index if invalid or if the item has been replaced with a
    // different navigation.
    if (reloaded_index >= lens_navigation_items_.size() ||
        lens_navigation_items_[reloaded_index]->comparison_key() !=
            item.comparison_key()) {
      return;
    }
    // Replace the item at index with the reloaded one.
    lens_navigation_items_[reloaded_index] = std::move(item_ptr);
    // Reload the item if it's the current navigation index.
    if (reloaded_index == lens_navigation_items_.size() - 1) {
      [mutator_ loadLensResult:result];
    }
  } else {
    lens_navigation_items_.push_back(std::move(item_ptr));
    [mutator_ loadLensResult:result];
  }
}

bool LensOverlayNavigationManager::CanGoBack() const {
  return lens_navigation_items_.size() > 1;
}

void LensOverlayNavigationManager::GoBack() {
  if (!CanGoBack()) {
    OnNavigationListUpdate();
    return;
  }

  // Lens navigation back.
  lens_navigation_items_.pop_back();
  const LensResultItem& previous_item = *lens_navigation_items_.back();
  lens_reloaded_items_[previous_item.comparison_key()] =
      lens_navigation_items_.size() - 1;
  [mutator_ reloadLensResult:previous_item.lens_result()];
  OnNavigationListUpdate();
}

#pragma mark - web::WebStateObserver

void LensOverlayNavigationManager::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context && !navigation_context->IsSameDocument()) {
    // TODO(crbug.com/372914204): Store sub navigations.
    OnNavigationListUpdate();
  }
}

void LensOverlayNavigationManager::WebStateDestroyed(web::WebState* web_state) {
  SetWebState(nullptr);
}

#pragma mark - Private

void LensOverlayNavigationManager::OnNavigationListUpdate() const {
  [mutator_ onBackNavigationAvailabilityMaybeChanged:CanGoBack()];
}
