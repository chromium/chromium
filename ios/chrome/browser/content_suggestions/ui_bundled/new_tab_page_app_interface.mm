// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/new_tab_page_app_interface.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/ntp_tiles/pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/ntp_home_test_utils.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/public/set_up_list_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/ui/set_up_list_item_view.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item_type.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "net/base/apple/url_conversions.h"

using content_suggestions::SearchFieldWidth;
using set_up_list_prefs::SetUpListItemState;

@implementation NewTabPageAppInterface

+ (CGFloat)searchFieldWidthForCollectionWidth:(CGFloat)collectionWidth
                              traitCollection:
                                  (UITraitCollection*)traitCollection {
  return content_suggestions::SearchFieldWidth(collectionWidth,
                                               traitCollection);
}

+ (UIView*)NTPView {
  return ntp_home::NTPView();
}

+ (UICollectionView*)collectionView {
  return ntp_home::CollectionView();
}

+ (UICollectionView*)contentSuggestionsCollectionView {
  return ntp_home::ContentSuggestionsCollectionView();
}

+ (UIView*)fakeOmnibox {
  return ntp_home::FakeOmnibox();
}

+ (UILabel*)discoverHeaderLabel {
  return ntp_home::DiscoverHeaderLabel();
}

+ (void)disableTipsCards {
  chrome_test_util::GetOriginalProfile()->GetPrefs()->SetBoolean(
      ntp_tiles::prefs::kTipsHomeModuleEnabled, false);
}

+ (void)resetSetUpListPrefs {
  PrefService* localState = GetApplicationContext()->GetLocalState();
  PrefService* prefService = chrome_test_util::GetOriginalProfile()->GetPrefs();
  prefService->SetBoolean(ntp_tiles::prefs::kTipsHomeModuleEnabled, true);
  SetUpListItemState unknown = SetUpListItemState::kUnknown;
  set_up_list_prefs::SetItemState(localState,
                                  SetUpListItemType::kDefaultBrowser, unknown);
  set_up_list_prefs::SetItemState(localState, SetUpListItemType::kAutofill,
                                  unknown);
  set_up_list_prefs::SetItemState(localState, SetUpListItemType::kNotifications,
                                  unknown);
}

+ (BOOL)setUpListItemDefaultBrowserInMagicStackIsComplete {
  SetUpListItemView* view =
      ntp_home::SetUpListItemViewInMagicStackWithAccessibilityId(
          set_up_list::kDefaultBrowserItemID);
  return view.complete;
}

+ (BOOL)setUpListItemAutofillInMagicStackIsComplete {
  return ntp_home::SetUpListItemViewInMagicStackWithAccessibilityId(
             set_up_list::kAutofillItemID)
      .complete;
}

+ (NewTabPageColorPalette*)currentBackgroundColor {
  return ntp_home::CurrentBackgroundColor();
}

+ (BOOL)hasBackgroundImage {
  return ntp_home::HasBackgroundImage();
}

@end
