// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/ntp_home_test_utils.h"

#import <string>

#import "base/apple/foundation_util.h"
#import "base/functional/callback.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/constants.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#import "ios/web/common/uikit_ui_util.h"

namespace {

// Returns the subview of `parentView` corresponding to the
// ContentSuggestionsViewController. Returns nil if it is not in its subviews.
UIView* SubviewWithAccessibilityIdentifier(NSString* accessibilityID,
                                           UIView* parentView) {
  if (parentView.accessibilityIdentifier == accessibilityID) {
    return parentView;
  }
  if (parentView.subviews.count == 0)
    return nil;
  for (UIView* view in parentView.subviews) {
    UIView* resultView =
        SubviewWithAccessibilityIdentifier(accessibilityID, view);
    if (resultView)
      return resultView;
  }
  return nil;
}

}  // namespace

namespace ntp_home {

UIView* NTPView() {
  return base::apple::ObjCCast<UIView>(SubviewWithAccessibilityIdentifier(
      kNTPViewIdentifier, GetAnyKeyWindow()));
}

UICollectionView* CollectionView() {
  return base::apple::ObjCCast<UICollectionView>(
      SubviewWithAccessibilityIdentifier(kNTPCollectionViewIdentifier,
                                         GetAnyKeyWindow()));
}

UICollectionView* ContentSuggestionsCollectionView() {
  return base::apple::ObjCCast<UICollectionView>(
      SubviewWithAccessibilityIdentifier(
          kContentSuggestionsCollectionIdentifier, GetAnyKeyWindow()));
}

UIView* FakeOmnibox() {
  return SubviewWithAccessibilityIdentifier(FakeOmniboxAccessibilityID(),
                                            GetAnyKeyWindow());
}

UILabel* DiscoverHeaderLabel() {
  return base::apple::ObjCCast<UILabel>(SubviewWithAccessibilityIdentifier(
      DiscoverHeaderTitleAccessibilityID(), GetAnyKeyWindow()));
}

SetUpListItemView* SetUpListItemViewInMagicStackWithAccessibilityId(
    NSString* accessibility_id) {
  return base::apple::ObjCCast<SetUpListItemView>(
      SubviewWithAccessibilityIdentifier(accessibility_id, GetAnyKeyWindow()));
}

}  // namespace ntp_home
