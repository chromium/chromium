// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_mediator.h"

#import "base/files/scoped_temp_dir.h"
#import "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/scoped_refptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/default_clock.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_utils.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "components/bookmarks/test/bookmark_test_helpers.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/language/ios/browser/ios_language_detection_tab_helper.h"
#import "components/language/ios/browser/language_detection_java_script_feature.h"
#import "components/language_detection/core/language_detection_model.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#import "components/policy/core/common/mock_configuration_policy_provider.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/reading_list/core/reading_list_model.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "components/translate/core/browser/translate_prefs.h"
#import "components/translate/core/language_detection/language_detection_model.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/java_script_alert_dialog_overlay.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_presentation_context.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/policy/model/enterprise_policy_test_helper.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_test_utils.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_text_item.h"
#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_tools_item.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_table_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_test_navigation_manager.h"
#import "ios/chrome/browser/web/model/font_size/font_size_java_script_feature.h"
#import "ios/chrome/browser/web/model/font_size/font_size_tab_helper.h"
#import "ios/public/provider/chrome/browser/text_zoom/text_zoom_api.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_api.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/device_form_factor.h"

@interface FakePopupMenuConsumer : NSObject <PopupMenuConsumer>
@property(nonatomic, strong)
    NSArray<NSArray<TableViewItem<PopupMenuItem>*>*>* popupMenuItems;
@end

@implementation FakePopupMenuConsumer

@synthesize itemToHighlight;

- (void)itemsHaveChanged:(NSArray<TableViewItem<PopupMenuItem>*>*)items {
  // Do nothing.
}

@end

namespace {
const int kNumberOfWebStates = 3;
}  // namespace

@interface TestPopupMenuMediator
    : PopupMenuMediator<CRWWebStateObserver, WebStateListObserving>
@end

@implementation TestPopupMenuMediator
@end

class PopupMenuMediatorTest : public PlatformTest {
 public:
  PopupMenuMediatorTest()
      : model_(std::make_unique<language_detection::LanguageDetectionModel>()) {
  }

  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(ios::BookmarkModelFactory::GetInstance(),
                              ios::BookmarkModelFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(&password_manager::BuildPasswordStoreInterface<
                            web::BrowserState,
                            password_manager::MockPasswordStoreInterface>));
    builder.AddTestingFactory(
        ReadingListModelFactory::GetInstance(),
        base::BindRepeating(&BuildReadingListModelWithFakeStorage,
                            std::vector<scoped_refptr<ReadingListEntry>>()));
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();

    web::test::OverrideJavaScriptFeatures(
        profile_.get(),
        {language::LanguageDetectionJavaScriptFeature::GetInstance()});

    reading_list_model_ =
        ReadingListModelFactory::GetForProfile(profile_.get());

    popup_menu_ = OCMClassMock([PopupMenuTableViewController class]);
    popup_menu_strict_ =
        OCMStrictClassMock([PopupMenuTableViewController class]);
    OCMExpect([popup_menu_strict_ setPopupMenuItems:[OCMArg any]]);
    OCMExpect([popup_menu_strict_ setDelegate:[OCMArg any]]);

    // Set up the TestBrowser.
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    // Set up the WebStateList.
    auto navigation_manager = std::make_unique<ToolbarTestNavigationManager>();

    navigation_item_ = web::NavigationItem::Create();
    GURL url = GURL("http://chromium.org");
    navigation_item_->SetURL(url);
    navigation_manager->SetVisibleItem(navigation_item_.get());

    std::unique_ptr<web::FakeWebState> test_web_state =
        std::make_unique<web::FakeWebState>();
    test_web_state->SetNavigationManager(std::move(navigation_manager));
    test_web_state->SetLoading(true);
    test_web_state->SetBrowserState(profile_.get());
    web_state_ = test_web_state.get();

    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    auto main_frame = web::FakeWebFrame::CreateMainWebFrame(
        /*security_origin=*/url);
    main_frame->set_browser_state(profile_.get());
    frames_manager->AddWebFrame(std::move(main_frame));
    web::ContentWorld content_world =
        language::LanguageDetectionJavaScriptFeature::GetInstance()
            ->GetSupportedContentWorld();
    web_state_->SetWebFramesManager(content_world, std::move(frames_manager));

    browser_->GetWebStateList()->InsertWebState(
        std::move(test_web_state), WebStateList::InsertionParams::AtIndex(0));
    for (int i = 1; i < kNumberOfWebStates; i++) {
      InsertNewWebState(i);
    }

    // Set up the OverlayPresenter.
    OverlayPresenter::FromBrowser(browser_.get(),
                                  OverlayModality::kWebContentArea)
        ->SetPresentationContext(&presentation_context_);
  }

  void TearDown() override {
    // Explicitly disconnect the mediator so there won't be any WebStateList
    // observers when browser_ gets destroyed.
    [mediator_ disconnect];
    browser_.reset();

    PlatformTest::TearDown();
  }

 protected:
  PopupMenuMediator* CreateMediator(BOOL is_incognito) {
    mediator_ =
        [[PopupMenuMediator alloc] initWithIsIncognito:is_incognito
                                      readingListModel:reading_list_model_
                                browserPolicyConnector:nil];
    return mediator_;
  }

  PopupMenuMediator* CreateMediatorWithBrowserPolicyConnector(
      BOOL is_incognito,
      BrowserPolicyConnectorIOS* browser_policy_connector) {
    mediator_ = [[PopupMenuMediator alloc]
              initWithIsIncognito:is_incognito
                 readingListModel:reading_list_model_
           browserPolicyConnector:browser_policy_connector];
    return mediator_;
  }

  void CreatePrefs() {
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterBooleanPref(
        bookmarks::prefs::kEditBookmarksEnabled,
        /*default_value=*/true);
    prefs_->registry()->RegisterBooleanPref(
        translate::prefs::kOfferTranslateEnabled, true);
  }

  void SetUpBookmarks() {
    bookmark_model_ = ios::BookmarkModelFactory::GetForProfile(profile_.get());
    DCHECK(bookmark_model_);
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_);
    mediator_.bookmarkModel = bookmark_model_;
  }

  void InsertNewWebState(int index) {
    auto web_state = std::make_unique<web::FakeWebState>();
    GURL url("http://test/" + base::NumberToString(index));
    web_state->SetCurrentURL(url);

    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    auto main_frame = web::FakeWebFrame::CreateMainWebFrame(
        /*security_origin=*/url);
    main_frame->set_browser_state(profile_.get());
    frames_manager->AddWebFrame(std::move(main_frame));
    web::ContentWorld content_world =
        language::LanguageDetectionJavaScriptFeature::GetInstance()
            ->GetSupportedContentWorld();
    web_state_->SetWebFramesManager(content_world, std::move(frames_manager));

    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state), WebStateList::InsertionParams::AtIndex(index));
  }

  void SetUpActiveWebState() {
    if (!prefs_.get()) {
      CreatePrefs();
    }
    // PopupMenuMediator expects an language::IOSLanguageDetectionTabHelper for
    // the currently active WebState.
    language::IOSLanguageDetectionTabHelper::CreateForWebState(
        browser_->GetWebStateList()->GetWebStateAt(0),
        /*url_language_histogram=*/nullptr, &model_, prefs_.get());

    browser_->GetWebStateList()->ActivateWebStateAt(0);
  }

  // Checks that the popup_menu_ is receiving a number of items corresponding to
  // `number_items`.
  void CheckMediatorSetItems(NSArray<NSNumber*>* number_items) {
    mediator_.webStateList = browser_->GetWebStateList();
    SetUpActiveWebState();
    auto same_number_items = ^BOOL(id items) {
      if (![items isKindOfClass:[NSArray class]])
        return NO;
      if ([items count] != number_items.count)
        return NO;
      for (NSUInteger index = 0; index < number_items.count; index++) {
        NSArray* section = [items objectAtIndex:index];
        if (section.count != number_items[index].unsignedIntegerValue)
          return NO;
      }
      return YES;
    };
    OCMExpect([popup_menu_
        setPopupMenuItems:[OCMArg checkWithBlock:same_number_items]]);
    mediator_.popupMenu = popup_menu_;
    EXPECT_OCMOCK_VERIFY(popup_menu_);
  }

  bool HasItem(FakePopupMenuConsumer* consumer,
               NSString* accessibility_identifier,
               BOOL enabled) {
    for (NSArray* innerArray in consumer.popupMenuItems) {
      for (PopupMenuToolsItem* item in innerArray) {
        if (item.accessibilityIdentifier == accessibility_identifier)
          return item.enabled == enabled;
      }
    }
    return NO;
  }

  bool HasEnterpriseInfoItem(FakePopupMenuConsumer* consumer) {
    for (NSArray* innerArray in consumer.popupMenuItems) {
      for (PopupMenuTextItem* item in innerArray) {
        if (item.accessibilityIdentifier == kTextMenuEnterpriseInfo)
          return YES;
      }
    }
    return NO;
  }

  web::WebTaskEnvironment task_env_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;

  FakeOverlayPresentationContext presentation_context_;
  PopupMenuMediator* mediator_;
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<ReadingListModel> reading_list_model_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  raw_ptr<web::FakeWebState> web_state_;
  std::unique_ptr<web::NavigationItem> navigation_item_;
  id popup_menu_;
  // Mock refusing all calls except -setPopupMenuItems:.
  id popup_menu_strict_;
  translate::LanguageDetectionModel model_;
};

// Tests that the feature engagement tracker get notified when the mediator is
// disconnected and the tracker wants the notification badge displayed.
TEST_F(PopupMenuMediatorTest, TestFeatureEngagementDisconnect) {
  CreateMediator(/*is_incognito=*/NO);
  feature_engagement::test::MockTracker tracker;
  EXPECT_CALL(tracker, ShouldTriggerHelpUI(testing::_))
      .WillRepeatedly(testing::Return(true));
  mediator_.popupMenu = popup_menu_;
  mediator_.engagementTracker = &tracker;

  // There may be one or more Tools Menu items that use engagement trackers.
  EXPECT_CALL(tracker, Dismissed(testing::_)).Times(testing::AtLeast(1));
  [mediator_ disconnect];
}

// Tests that the mediator is returning the right number of items and sections
// for the Tools Menu type.
TEST_F(PopupMenuMediatorTest, TestToolsMenuItemsCount) {
  CreateMediator(/*is_incognito=*/NO);
  NSUInteger number_of_action_items = 7;
  if (ios::provider::IsUserFeedbackSupported()) {
    number_of_action_items++;
  }

  if (ios::provider::IsTextZoomEnabled()) {
    number_of_action_items++;
  }

  // Stop/Reload, New Tab, New Incognito Tab.
  NSUInteger number_of_tab_actions = 3;
  if (base::ios::IsMultipleScenesSupported()) {
    // New Window option is added in this case.
    number_of_tab_actions++;
  }

  // Checks that Tools Menu has the right number of items in each section.
  CheckMediatorSetItems(@[
    @(number_of_tab_actions),
    // 4 collections, Downloads, Settings.
    @(6),
    // Other actions, depending on configuration.
    @(number_of_action_items)
  ]);
}

// Tests that the items returned by the mediator are correctly enabled on a
// WebPage.
TEST_F(PopupMenuMediatorTest, TestItemsStatusOnWebPage) {
  CreateMediator(/*is_incognito=*/NO);
  mediator_.webStateList = browser_->GetWebStateList();
  FakePopupMenuConsumer* consumer = [[FakePopupMenuConsumer alloc] init];
  mediator_.popupMenu = consumer;
  SetUpActiveWebState();

  web::FakeNavigationContext context;
  web_state_->OnNavigationFinished(&context);

  EXPECT_TRUE(HasItem(consumer, kToolsMenuNewTabId, /*enabled=*/YES));
  EXPECT_TRUE(HasItem(consumer, kToolsMenuSiteInformation, /*enabled=*/YES));
}

// Tests that the items returned by the mediator are correctly enabled on the
// NTP.
TEST_F(PopupMenuMediatorTest, TestItemsStatusOnNTP) {
  CreateMediator(/*is_incognito=*/NO);
  mediator_.webStateList = browser_->GetWebStateList();
  FakePopupMenuConsumer* consumer = [[FakePopupMenuConsumer alloc] init];
  mediator_.popupMenu = consumer;
  SetUpActiveWebState();

  navigation_item_->SetVirtualURL(GURL("chrome://newtab"));
  web::FakeNavigationContext context;
  web_state_->OnNavigationFinished(&context);

  EXPECT_TRUE(HasItem(consumer, kToolsMenuNewTabId, /*enabled=*/YES));
  EXPECT_TRUE(HasItem(consumer, kToolsMenuSiteInformation, /*enabled=*/YES));
}

// Tests that the "Add to Reading List" button is disabled while overlay UI is
// displayed in OverlayModality::kWebContentArea.
TEST_F(PopupMenuMediatorTest, TestReadLaterDisabled) {
  const GURL kUrl("https://chromium.test");
  web_state_->SetCurrentURL(kUrl);
  CreatePrefs();
  CreateMediator(/*is_incognito=*/NO);
  mediator_.webStateList = browser_->GetWebStateList();
  mediator_.webContentAreaOverlayPresenter = OverlayPresenter::FromBrowser(
      browser_.get(), OverlayModality::kWebContentArea);
  FakePopupMenuConsumer* consumer = [[FakePopupMenuConsumer alloc] init];
  mediator_.popupMenu = consumer;
  mediator_.prefService = prefs_.get();
  SetUpActiveWebState();
  ASSERT_TRUE(HasItem(consumer, kToolsMenuReadLater, /*enabled=*/YES));

  // Present a JavaScript alert over the WebState and verify that the page is no
  // longer shareable.
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state_, OverlayModality::kWebContentArea);
  queue->AddRequest(
      OverlayRequest::CreateWithConfig<JavaScriptAlertDialogRequest>(
          web_state_, kUrl,
          /*is_main_frame=*/true, @"message"));
  EXPECT_TRUE(HasItem(consumer, kToolsMenuReadLater, /*enabled=*/NO));

  // Cancel the request and verify that the "Add to Reading List" button is
  // enabled.
  queue->CancelAllRequests();
  EXPECT_TRUE(HasItem(consumer, kToolsMenuReadLater, /*enabled=*/YES));
}

// Tests that the "Text Zoom..." button is disabled on non-HTML pages.
TEST_F(PopupMenuMediatorTest, TestTextZoomDisabled) {
  CreateMediator(/*is_incognito=*/NO);
  mediator_.webStateList = browser_->GetWebStateList();

  FakePopupMenuConsumer* consumer = [[FakePopupMenuConsumer alloc] init];
  mediator_.popupMenu = consumer;

  // FontSizeTabHelper requires a web frames manager.
  web_state_->SetWebFramesManager(
      FontSizeJavaScriptFeature::GetInstance()->GetSupportedContentWorld(),
      std::make_unique<web::FakeWebFramesManager>());
  FontSizeTabHelper::CreateForWebState(
      browser_->GetWebStateList()->GetWebStateAt(0));
  SetUpActiveWebState();
  EXPECT_TRUE(HasItem(consumer, kToolsMenuTextZoom, /*enabled=*/YES));

  web_state_->SetContentIsHTML(false);
  // Fake a navigationFinished to force the popup menu items to update.
  web::FakeNavigationContext context;
  web_state_->OnNavigationFinished(&context);
  EXPECT_TRUE(HasItem(consumer, kToolsMenuTextZoom, /*enabled=*/NO));
}

// Tests that the "Managed by..." item is hidden when none of the policies is
// set.
TEST_F(PopupMenuMediatorTest, TestEnterpriseInfoHidden) {
  CreateMediator(/*is_incognito=*/NO);

  mediator_.webStateList = browser_->GetWebStateList();
  FakePopupMenuConsumer* consumer = [[FakePopupMenuConsumer alloc] init];
  mediator_.popupMenu = consumer;
  SetUpActiveWebState();

  ASSERT_FALSE(HasEnterpriseInfoItem(consumer));
}

// Tests that the "Managed by..." item is shown.
TEST_F(PopupMenuMediatorTest, TestEnterpriseInfoShown) {
  // Set a policy.
  base::ScopedTempDir state_directory;
  ASSERT_TRUE(state_directory.CreateUniqueTempDir());

  std::unique_ptr<EnterprisePolicyTestHelper> enterprise_policy_helper =
      std::make_unique<EnterprisePolicyTestHelper>(state_directory.GetPath());
  BrowserPolicyConnectorIOS* connector =
      enterprise_policy_helper->GetBrowserPolicyConnector();

  policy::PolicyMap map;
  map.Set("test-policy", policy::POLICY_LEVEL_MANDATORY,
          policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_PLATFORM,
          base::Value("hello"), nullptr);
  enterprise_policy_helper->GetPolicyProvider()->UpdateChromePolicy(map);

  CreateMediatorWithBrowserPolicyConnector(
      /*is_incognito=*/NO, connector);

  mediator_.webStateList = browser_->GetWebStateList();
  FakePopupMenuConsumer* consumer = [[FakePopupMenuConsumer alloc] init];
  mediator_.popupMenu = consumer;
  SetUpActiveWebState();
  ASSERT_TRUE(HasEnterpriseInfoItem(consumer));
}

// Tests that 1) the tools menu has an enabled 'Add to Bookmarks' button when
// the current URL is not in bookmarks 2) the bookmark button changes to an
// enabled 'Edit bookmark' button when navigating to a bookmarked URL, 3) the
// bookmark button changes to 'Add to Bookmarks' when the bookmark is removed.
TEST_F(PopupMenuMediatorTest, TestBookmarksToolsMenuButtons) {
  const GURL url("https://bookmarked.url");
  web_state_->SetCurrentURL(url);
  CreateMediator(/*is_incognito=*/NO);
  CreatePrefs();
  SetUpBookmarks();

  bookmark_model_->AddNewURL(bookmark_model_->mobile_node(), 0,
                             base::SysNSStringToUTF16(@"Test bookmark"), url);
  mediator_.webStateList = browser_->GetWebStateList();
  FakePopupMenuConsumer* consumer = [[FakePopupMenuConsumer alloc] init];
  mediator_.popupMenu = consumer;
  mediator_.prefService = prefs_.get();

  EXPECT_TRUE(HasItem(consumer, kToolsMenuAddToBookmarks, /*enabled=*/YES));

  SetUpActiveWebState();
  EXPECT_FALSE(HasItem(consumer, kToolsMenuAddToBookmarks, /*enabled=*/YES));
  EXPECT_TRUE(HasItem(consumer, kToolsMenuEditBookmark, /*enabled=*/YES));

  ios::BookmarkModelFactory::GetForProfile(profile_.get())
      ->RemoveAllUserBookmarks(FROM_HERE);
  EXPECT_TRUE(HasItem(consumer, kToolsMenuAddToBookmarks, /*enabled=*/YES));
  EXPECT_FALSE(HasItem(consumer, kToolsMenuEditBookmark, /*enabled=*/YES));
}

// Tests that the bookmark button is disabled when EditBookmarksEnabled pref is
// changed to false.
TEST_F(PopupMenuMediatorTest, TestDisableBookmarksButton) {
  CreateMediator(/*is_incognito=*/NO);
  CreatePrefs();
  FakePopupMenuConsumer* consumer = [[FakePopupMenuConsumer alloc] init];
  mediator_.popupMenu = consumer;
  mediator_.prefService = prefs_.get();

  EXPECT_TRUE(HasItem(consumer, kToolsMenuAddToBookmarks, /*enabled=*/YES));

  prefs_->SetBoolean(bookmarks::prefs::kEditBookmarksEnabled, false);
  EXPECT_TRUE(HasItem(consumer, kToolsMenuAddToBookmarks, /*enabled=*/NO));
}
