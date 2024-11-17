// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/new_tab_page_app_interface.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item_type.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_test_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/constants.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view.h"
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

+ (void)disableSetUpList {
  PrefService* prefService =
      IsHomeCustomizationEnabled()
          ? chrome_test_util::GetOriginalProfile()->GetPrefs()
          : GetApplicationContext()->GetLocalState();
  set_up_list_prefs::DisableSetUpList(prefService);
}

+ (void)resetSetUpListPrefs {
  PrefService* localState = GetApplicationContext()->GetLocalState();
  if (IsHomeCustomizationEnabled()) {
    PrefService* prefService =
        chrome_test_util::GetOriginalProfile()->GetPrefs();
    prefService->SetBoolean(prefs::kHomeCustomizationMagicStackSetUpListEnabled,
                            true);
  } else {
    localState->ClearPref(set_up_list_prefs::kDisabled);
  }
  SetUpListItemState unknown = SetUpListItemState::kUnknown;
  set_up_list_prefs::SetItemState(localState, SetUpListItemType::kSignInSync,
                                  unknown);
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

+ (NSString*)setUpListTitle {
  return content_suggestions::SetUpListTitleString();
}

@end
