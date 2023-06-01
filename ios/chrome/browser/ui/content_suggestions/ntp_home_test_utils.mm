// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/ntp_home_test_utils.h"

#import <string>

#import "base/functional/callback.h"
#import "base/mac/foundation_util.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_constants.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#import "ios/web/common/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

UICollectionView* CollectionView() {
  return base::mac::ObjCCast<UICollectionView>(
      SubviewWithAccessibilityIdentifier(kNTPCollectionViewIdentifier,
                                         GetAnyKeyWindow()));
}

UICollectionView* ContentSuggestionsCollectionView() {
  return base::mac::ObjCCast<UICollectionView>(
      SubviewWithAccessibilityIdentifier(
          kContentSuggestionsCollectionIdentifier, GetAnyKeyWindow()));
}

UIView* FakeOmnibox() {
  return SubviewWithAccessibilityIdentifier(FakeOmniboxAccessibilityID(),
                                            GetAnyKeyWindow());
}

UILabel* DiscoverHeaderLabel() {
  return base::mac::ObjCCast<UILabel>(SubviewWithAccessibilityIdentifier(
      DiscoverHeaderTitleAccessibilityID(), GetAnyKeyWindow()));
}

}  // namespace ntp_home
