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
#import "net/base/url_util.h"

#pragma mark - LensResultItem

LensOverlayNavigationManager::LensResultItem::LensResultItem(
    id<ChromeLensOverlayResult> lens_result)
    : lens_result_(lens_result) {
  // Use `isTextSelection`, `selectionRect` and `queryText` to verify that two
  // lens result refer to the same selection query.
  comparison_key_ = base::SysNSStringToUTF8(
      [NSString stringWithFormat:@"%d%@%@", lens_result.isTextSelection,
                                 NSStringFromCGRect(lens_result.selectionRect),
                                 lens_result.queryText]);
}

LensOverlayNavigationManager::LensResultItem::~LensResultItem() {}

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

void LensOverlayNavigationManager::LoadUnimodalOmniboxNavigation(
    const GURL& destination_url,
    const std::u16string& omnibox_text) {
  /// SRP loaded in the lens overlay require this parameter.
  GURL url =
      net::AppendOrReplaceQueryParameter(destination_url, "lns_surface", "4");

  RegisterSubNavigation(url, omnibox_text);
  [mutator_ loadURL:url omniboxText:base::SysUTF16ToNSString(omnibox_text)];
}

bool LensOverlayNavigationManager::CanGoBack() const {
  // Sub navigation back.
  if (!lens_navigation_items_.empty() &&
      lens_navigation_items_.back()->sub_navigations().size() > 1) {
    return true;
  }
  // Lens navigation back.
  return lens_navigation_items_.size() > 1;
}

void LensOverlayNavigationManager::GoBack() {
  if (!CanGoBack()) {
    OnNavigationListUpdate();
    return;
  }

  // Sub navigation back.
  if (lens_navigation_items_.back()->sub_navigations().size() > 1) {
    GoToPreviousSubNavigation();
  } else {  // Lens navigation back.
    GoToPreviousLensNavigation();
  }
}

#pragma mark - web::WebStateObserver

void LensOverlayNavigationManager::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context && !navigation_context->IsSameDocument()) {
    RegisterSubNavigation(navigation_context->GetUrl(), PreviousOmniboxText());
  }
}

void LensOverlayNavigationManager::WebStateDestroyed(web::WebState* web_state) {
  SetWebState(nullptr);
}

#pragma mark - Private

void LensOverlayNavigationManager::RegisterSubNavigation(
    GURL url,
    const std::u16string& omnibox_text) {
  if (lens_navigation_items_.empty()) {
    NOTREACHED(kLensOverlayNotFatalUntil)
        << "Web navigation without lens result is not supported.";
  }

  // To prevent dark mode toggles from creating new navigation history entries,
  // remove the dark mode parameter from the URL. The parameter that determines
  // the interface style is kept in sync with the system preference and appended
  // before the URL is loaded.
  if (url.has_query()) {
    url = net::AppendOrReplaceQueryParameter(url, "cs", std::nullopt);
  }

  // Add sub navigation if it's not a reload.
  std::vector<LensSubNavigationItem>& sub_navigations =
      lens_navigation_items_.back()->sub_navigations();
  if (sub_navigations.empty() ||
      url != sub_navigations.back().destination_url) {
    sub_navigations.emplace_back(url, omnibox_text);
    OnNavigationListUpdate();
  }
}

void LensOverlayNavigationManager::OnNavigationListUpdate() const {
  [mutator_ onBackNavigationAvailabilityMaybeChanged:CanGoBack()];
}

void LensOverlayNavigationManager::GoToPreviousSubNavigation() {
  std::vector<LensSubNavigationItem>& sub_navigation =
      lens_navigation_items_.back()->sub_navigations();
  // Removes current sub navigation.
  sub_navigation.pop_back();
  // Reloads previous sub navigation.
  const LensSubNavigationItem& previous_navigation = sub_navigation.back();
  [mutator_ loadURL:previous_navigation.destination_url
        omniboxText:base::SysUTF16ToNSString(previous_navigation.omnibox_text)];
  OnNavigationListUpdate();
}

void LensOverlayNavigationManager::GoToPreviousLensNavigation() {
  // Remove the current lens navigation.
  lens_navigation_items_.pop_back();
  LensResultItem& previous_item = *lens_navigation_items_.back();
  // Clear previous sub navigations as they become invalid after reload.
  std::vector<LensSubNavigationItem>& previous_sub_navigation =
      previous_item.sub_navigations();
  previous_sub_navigation.erase(previous_sub_navigation.begin() + 1,
                                previous_sub_navigation.end());
  // Load the previous lens navigation.
  lens_reloaded_items_[previous_item.comparison_key()] =
      lens_navigation_items_.size() - 1;
  [mutator_ reloadLensResult:previous_item.lens_result()];
  OnNavigationListUpdate();
}

std::u16string LensOverlayNavigationManager::PreviousOmniboxText() const {
  if (lens_navigation_items_.empty()) {
    NOTREACHED(kLensOverlayNotFatalUntil)
        << "Should only be called with an existing LensResultItem.";
  }
  const LensResultItem& current_lens_result = *lens_navigation_items_.back();
  // If there is no previous sub navigation return the omnibox text from the
  // lens result.
  if (current_lens_result.sub_navigations().empty()) {
    return base::SysNSStringToUTF16(
        current_lens_result.lens_result().queryText);
  } else {
    return current_lens_result.sub_navigations().back().omnibox_text;
  }
}
