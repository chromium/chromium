// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/ntp_home_test_utils.h"

#import <string>

#import "base/callback.h"
#import "base/mac/foundation_util.h"
#import "base/strings/utf_string_conversions.h"
#import "components/ntp_snippets/content_suggestion.h"
#import "components/ntp_snippets/status.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_constants.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#import "ios/web/common/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Helper method to get the Article category.
ntp_snippets::Category Category() {
  return ntp_snippets::Category::FromKnownCategory(
      ntp_snippets::KnownCategories::ARTICLES);
}

// Creates a suggestion with a `title` and `url`
ntp_snippets::ContentSuggestion Suggestion(std::string title, GURL url) {
  ntp_snippets::ContentSuggestion suggestion(Category(), title, url);
  suggestion.set_title(base::UTF8ToUTF16(title));

  return suggestion;
}

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

namespace ntp_snippets {

AdditionalSuggestionsHelper::AdditionalSuggestionsHelper(const GURL& url)
    : url_(url) {}

void AdditionalSuggestionsHelper::SendAdditionalSuggestions(
    FetchDoneCallback* callback) {
  std::vector<ContentSuggestion> suggestions;
  for (int i = 0; i < 10; i++) {
    std::string title = "AdditionalSuggestion" + std::to_string(i);
    suggestions.emplace_back(Suggestion(title, url_));
  }
  std::move(*callback).Run(Status(StatusCode::SUCCESS, ""),
                           std::move(suggestions));
}

}  // namespace ntp_snippets
