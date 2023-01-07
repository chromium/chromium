// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/feature_engagement/feature_engagement_app_interface.h"

#import <memory>

#import "base/bind.h"
#import "base/memory/singleton.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/feature_engagement/test/test_tracker.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/translate/core/browser/translate_prefs.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

using base::test::ScopedFeatureList;
using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForActionTimeout;

namespace {
std::unique_ptr<KeyedService> CreateTestFeatureEngagementTracker(
    web::BrowserState*) {
  return feature_engagement::CreateTestTracker();
}

BOOL LoadFeatureEngagementTracker() {
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();

  feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
      browser_state, base::BindRepeating(&CreateTestFeatureEngagementTracker));

  // Wait until the feature engagement tracker is initialized.
  return WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    return feature_engagement::TrackerFactory::GetForBrowserState(browser_state)
        ->IsInitialized();
  });
}

class ScopedFeatureListHolder {
 public:
  static ScopedFeatureListHolder* GetInstance() {
    return base::Singleton<ScopedFeatureListHolder>::get();
  }

  ScopedFeatureListHolder(const ScopedFeatureListHolder&) = delete;
  ScopedFeatureListHolder& operator=(const ScopedFeatureListHolder&) = delete;

  // Creates and returns new scoped feature list. List stays alive until
  // DestroyLists() is called. Allows to push multiple features via scoped
  // feature list as required by some FeatureEngagement tests.
  ScopedFeatureList& CreateList() {
    auto scoped_feature_list = std::make_unique<ScopedFeatureList>();
    scoped_feature_lists_.push_back(std::move(scoped_feature_list));
    return *(scoped_feature_lists_.back());
  }

  // Destroys all scoped feature lists objects created with CreateList().
  void DestroyLists() { scoped_feature_lists_.clear(); }

 private:
  ScopedFeatureListHolder() = default;
  std::vector<std::unique_ptr<ScopedFeatureList>> scoped_feature_lists_;
  friend struct base::DefaultSingletonTraits<ScopedFeatureListHolder>;
};

}  // namespace

@implementation FeatureEngagementAppInterface

+ (void)reset {
  ScopedFeatureListHolder::GetInstance()->DestroyLists();
}

+ (void)simulateChromeOpenedEvent {
  feature_engagement::TrackerFactory::GetForBrowserState(
      chrome_test_util::GetOriginalBrowserState())
      ->NotifyEvent(feature_engagement::events::kChromeOpened);
}

+ (BOOL)enableBadgedReadingListTriggering {
  std::map<std::string, std::string> badged_reading_list_params;

  badged_reading_list_params["event_1"] =
      "name:chrome_opened;comparator:>=5;window:90;storage:90";
  badged_reading_list_params["event_trigger"] =
      "name:badged_reading_list_trigger;comparator:==0;window:1095;storage:"
      "1095";
  badged_reading_list_params["event_used"] =
      "name:viewed_reading_list;comparator:==0;window:90;storage:90";
  badged_reading_list_params["session_rate"] = "==0";
  badged_reading_list_params["availability"] = "any";

  ScopedFeatureListHolder::GetInstance()
      ->CreateList()
      .InitAndEnableFeatureWithParameters(
          feature_engagement::kIPHBadgedReadingListFeature,
          badged_reading_list_params);
  return LoadFeatureEngagementTracker();
}

+ (BOOL)enableBadgedTranslateManualTrigger {
  std::map<std::string, std::string> badged_translate_manual_trigger_params;
  badged_translate_manual_trigger_params["availability"] = "any";
  badged_translate_manual_trigger_params["session_rate"] = "==0";
  badged_translate_manual_trigger_params["event_used"] =
      "name:triggered_translate_infobar;comparator:==0;window:360;storage:360";
  badged_translate_manual_trigger_params["event_trigger"] =
      "name:badged_translate_manual_trigger_trigger;comparator:==0;window:360;"
      "storage:360";

  ScopedFeatureListHolder::GetInstance()
      ->CreateList()
      .InitAndEnableFeatureWithParameters(
          feature_engagement::kIPHBadgedTranslateManualTriggerFeature,
          badged_translate_manual_trigger_params);
  return LoadFeatureEngagementTracker();
}

+ (BOOL)enableNewTabTipTriggering {
  std::map<std::string, std::string> new_tab_tip_params;

  new_tab_tip_params["event_1"] =
      "name:chrome_opened;comparator:>=3;window:90;storage:90";
  new_tab_tip_params["event_trigger"] =
      "name:new_tab_tip_trigger;comparator:<2;window:1095;storage:"
      "1095";
  new_tab_tip_params["event_used"] =
      "name:new_tab_opened;comparator:==0;window:90;storage:90";
  new_tab_tip_params["session_rate"] = "==0";
  new_tab_tip_params["availability"] = "any";

  ScopedFeatureListHolder::GetInstance()
      ->CreateList()
      .InitAndEnableFeatureWithParameters(
          feature_engagement::kIPHNewTabTipFeature, new_tab_tip_params);
  return LoadFeatureEngagementTracker();
}

+ (BOOL)enableBottomToolbarTipTriggering {
  std::map<std::string, std::string> bottom_toolbar_tip_params;

  bottom_toolbar_tip_params["availability"] = "any";
  bottom_toolbar_tip_params["session_rate"] = "==0";
  bottom_toolbar_tip_params["event_used"] =
      "name:bottom_toolbar_opened;comparator:any;window:90;storage:90";
  bottom_toolbar_tip_params["event_trigger"] =
      "name:bottom_toolbar_trigger;comparator:==0;window:90;storage:90";

  ScopedFeatureListHolder::GetInstance()
      ->CreateList()
      .InitAndEnableFeatureWithParameters(
          feature_engagement::kIPHBottomToolbarTipFeature,
          bottom_toolbar_tip_params);
  return LoadFeatureEngagementTracker();
}

+ (BOOL)enableLongPressTipTriggering {
  std::map<std::string, std::string> long_press_tip_params;

  long_press_tip_params["availability"] = "any";
  long_press_tip_params["session_rate"] = "<=1";
  long_press_tip_params["event_used"] =
      "name:long_press_toolbar_opened;comparator:any;window:90;storage:90";
  long_press_tip_params["event_trigger"] =
      "name:long_press_toolbar_trigger;comparator:==0;window:90;storage:90";
  long_press_tip_params["event_1"] =
      "name:bottom_toolbar_opened;comparator:>=1;window:90;storage:90";

  ScopedFeatureListHolder::GetInstance()
      ->CreateList()
      .InitAndEnableFeatureWithParameters(
          feature_engagement::kIPHLongPressToolbarTipFeature,
          long_press_tip_params);
  return LoadFeatureEngagementTracker();
}

+ (BOOL)enableDefaultSiteViewTipTriggering {
  std::map<std::string, std::string> default_site_view_tip_params;

  default_site_view_tip_params["availability"] = "any";
  default_site_view_tip_params["session_rate"] = "<3";
  default_site_view_tip_params["event_used"] =
      "name:default_site_view_used;comparator:==0;window:720;storage:720";
  default_site_view_tip_params["event_trigger"] =
      "name:default_site_view_shown;comparator:==0;window:720;storage:720";
  default_site_view_tip_params["event_1"] =
      "name:desktop_version_requested;comparator:>=3;window:60;storage:60";

  ScopedFeatureListHolder::GetInstance()
      ->CreateList()
      .InitAndEnableFeatureWithParameters(
          feature_engagement::kIPHDefaultSiteViewFeature,
          default_site_view_tip_params);
  return LoadFeatureEngagementTracker();
}

+ (BOOL)enablePasswordSuggestionsTipTriggering {
  std::map<std::string, std::string> password_suggestions_tip_params;

  password_suggestions_tip_params["availability"] = "any";
  password_suggestions_tip_params["session_rate"] = "any";
  password_suggestions_tip_params["event_used"] =
      "name:password_suggestions_shown;comparator:==0;window:90;"
      "storage:360";
  password_suggestions_tip_params["event_trigger"] =
      "name:password_suggestions_iph_triggered;comparator:==0;window:1825;"
      "storage:1825";

  ScopedFeatureListHolder::GetInstance()
      ->CreateList()
      .InitAndEnableFeatureWithParameters(
          feature_engagement::kIPHPasswordSuggestionsFeature,
          password_suggestions_tip_params);

  return LoadFeatureEngagementTracker();
}

+ (BOOL)enableOverflowMenuTipTriggering {
  std::map<std::string, std::string> overflow_menu_tip_params;

  overflow_menu_tip_params["availability"] = "any";
  overflow_menu_tip_params["session_rate"] = "any";
  overflow_menu_tip_params["event_used"] =
      "name:popup_menu_tip_used;comparator:==0;window:180;"
      "storage:360";
  overflow_menu_tip_params["event_trigger"] =
      "name:popup_menu_tip_triggered;comparator:==0;window:1825;"
      "storage:1825";
  overflow_menu_tip_params["event_lockout"] =
      "name:overflow_menu_no_horizontal_scroll_or_action;comparator:>=2;window:"
      "180;storage:360";

  ScopedFeatureListHolder::GetInstance()
      ->CreateList()
      .InitAndEnableFeatureWithParameters(
          feature_engagement::kIPHOverflowMenuTipFeature,
          overflow_menu_tip_params);

  return LoadFeatureEngagementTracker();
}

+ (void)showTranslate {
  [chrome_test_util::HandlerForActiveBrowser() showTranslate];
}

+ (void)showReadingList {
  [chrome_test_util::HandlerForActiveBrowser() showReadingList];
}

@end
