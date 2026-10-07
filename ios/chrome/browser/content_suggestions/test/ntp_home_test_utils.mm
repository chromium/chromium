// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/test/ntp_home_test_utils.h"

#import <string>

#import "base/functional/callback.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/public/ntp_home_constants.h"
#import "ios/chrome/browser/content_suggestions/set_up_list/public/set_up_list_constants.h"
#import "ios/chrome/browser/content_suggestions/set_up_list/ui/set_up_list_item_view.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_image_background_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#import "ios/web/common/uikit_ui_util.h"

namespace ntp_home {

UIView* NTPView() {
  return chrome_test_util::FindViewById(GetAnyKeyWindow(), kNTPViewIdentifier);
}

UICollectionView* CollectionView() {
  return chrome_test_util::FindViewById<UICollectionView>(
      GetAnyKeyWindow(), kNTPCollectionViewIdentifier);
}

UICollectionView* ContentSuggestionsCollectionView() {
  return chrome_test_util::FindViewById<UICollectionView>(
      GetAnyKeyWindow(), kContentSuggestionsCollectionIdentifier);
}

UIView* FakeOmnibox() {
  return chrome_test_util::FindViewById(GetAnyKeyWindow(),
                                        FakeOmniboxAccessibilityID());
}

UILabel* DiscoverHeaderLabel() {
  return chrome_test_util::FindViewById<UILabel>(
      GetAnyKeyWindow(), DiscoverHeaderTitleAccessibilityID());
}

SetUpListItemView* SetUpListItemViewInMagicStackWithAccessibilityId(
    NSString* accessibility_id) {
  return chrome_test_util::FindViewById<SetUpListItemView>(GetAnyKeyWindow(),
                                                           accessibility_id);
}

NewTabPageColorPalette* CurrentBackgroundColor() {
  return [CollectionView().traitCollection objectForNewTabPageTrait];
}

BOOL HasBackgroundImage() {
  return
      [CollectionView().traitCollection boolForNewTabPageImageBackgroundTrait];
}

}  // namespace ntp_home
