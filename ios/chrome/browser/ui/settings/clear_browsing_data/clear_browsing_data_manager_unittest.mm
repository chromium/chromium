// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_manager.h"

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/browsing_data/core/pref_names.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/search_engines/template_url_data_util.h"
#import "components/search_engines/template_url_prepopulate_data.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/test/test_sync_service.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_features.h"
#import "ios/chrome/browser/browsing_data/model/cache_counter.h"
#import "ios/chrome/browser/browsing_data/model/fake_browsing_data_remover.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/fake_browsing_data_counter_wrapper_producer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

using TemplateURLPrepopulateData::GetAllPrepopulatedEngines;
using TemplateURLPrepopulateData::PrepopulatedEngine;

namespace {

std::unique_ptr<KeyedService> CreateTestSyncService(
    web::BrowserState* context) {
  return std::make_unique<syncer::TestSyncService>();
}

class ClearBrowsingDataManagerTest : public PlatformTest {
 public:
  ClearBrowsingDataManagerTest() {
    sync_preferences::PrefServiceMockFactory factory;
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs =
        factory.CreateSyncable(registry.get());
    RegisterProfilePrefs(registry.get());

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();

    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());
    account_manager_service_ =
        ChromeAccountManagerServiceFactory::GetForProfile(profile_.get());

    // Load TemplateURLService.
    template_url_service_ =
        ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
    template_url_service_->Load();

    FakeSystemIdentityManager* fake_system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
    fake_system_identity_manager->AddIdentity(fake_identity);

    model_ = [[TableViewModel alloc] init];
    remover_ = std::make_unique<FakeBrowsingDataRemover>();
    manager_ =
        [[ClearBrowsingDataManager alloc] initWithProfile:profile_.get()
                                      browsingDataRemover:remover_.get()
                       browsingDataCounterWrapperProducer:
                           [[FakeBrowsingDataCounterWrapperProducer alloc]
                               initWithProfile:profile_.get()]];
    [manager_ prepare];

    test_sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile_.get()));

    time_range_pref_.Init(browsing_data::prefs::kDeleteTimePeriod,
                          profile_->GetPrefs());
    scoped_feature_list_.InitWithFeatures(
        {feature_engagement::kIPHiOSInlineEnhancedSafeBrowsingPromoFeature},
        {});
  }

  ~ClearBrowsingDataManagerTest() override { [manager_ disconnect]; }

  id<SystemIdentity> fake_identity() {
    return account_manager_service_->GetDefaultIdentity();
  }

  // Adds a prepopulated search engine to TemplateURLService.
  // `prepopulate_id` should be big enough (>1000) to avoid collision with real
  // prepopulated search engines. The collision happens when
  // TemplateURLService::SetUserSelectedDefaultSearchProvider is called, in the
  // callback of PrefService the DefaultSearchManager will update the searchable
  // URL of default search engine from prepopulated search engines list.
  TemplateURL* AddPriorSearchEngine(const std::string& short_name,
                                    const GURL& searchable_url,
                                    int prepopulate_id,
                                    bool set_default) {
    DCHECK_GT(prepopulate_id, 1000);
    TemplateURLData data;
    data.SetShortName(base::ASCIIToUTF16(short_name));
    data.SetKeyword(base::ASCIIToUTF16(short_name));
    data.SetURL(searchable_url.possibly_invalid_spec());
    data.favicon_url = TemplateURL::GenerateFaviconURL(searchable_url);
    data.prepopulate_id = prepopulate_id;
    TemplateURL* url =
        template_url_service_->Add(std::make_unique<TemplateURL>(data));
    if (set_default)
      template_url_service_->SetUserSelectedDefaultSearchProvider(url);
    return url;
  }

  // Adds a custom search engine to TemplateURLService.
  TemplateURL* AddCustomSearchEngine(const std::string& short_name,
                                     const GURL& searchable_url,
                                     base::Time last_visited_time,
                                     bool set_default) {
    TemplateURLData data;
    data.SetShortName(base::ASCIIToUTF16(short_name));
    data.SetKeyword(base::ASCIIToUTF16(short_name));
    data.SetURL(searchable_url.possibly_invalid_spec());
    data.favicon_url = TemplateURL::GenerateFaviconURL(searchable_url);
    data.last_visited = last_visited_time;
    TemplateURL* url =
        template_url_service_->Add(std::make_unique<TemplateURL>(data));
    if (set_default)
      template_url_service_->SetUserSelectedDefaultSearchProvider(url);
    return url;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  TableViewModel* model_;
  std::unique_ptr<BrowsingDataRemover> remover_;
  ClearBrowsingDataManager* manager_;
  raw_ptr<syncer::TestSyncService> test_sync_service_;
  IntegerPrefMember time_range_pref_;
  raw_ptr<TemplateURLService> template_url_service_;  // weak
  raw_ptr<ChromeAccountManagerService> account_manager_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests model is set up with all appropriate items and sections.
TEST_F(ClearBrowsingDataManagerTest, TestModel) {
  [manager_ loadModel:model_];

  EXPECT_EQ(2, [model_ numberOfSections]);
  EXPECT_EQ(
      1,
      [model_ numberOfItemsInSection:[model_ sectionForSectionIdentifier:
                                                 SectionIdentifierTimeRange]]);
  EXPECT_EQ(
      5,
      [model_ numberOfItemsInSection:[model_ sectionForSectionIdentifier:
                                                 SectionIdentifierDataTypes]]);
}

// Tests model is set up with correct number of items and sections if signed in
// but sync is off.
TEST_F(ClearBrowsingDataManagerTest, TestModelSignedInSyncOff) {
  // Ensure that sync is not running.
  test_sync_service_->SetSignedIn(signin::ConsentLevel::kSignin);

  AuthenticationServiceFactory::GetForProfile(profile_.get())
      ->SignIn(fake_identity(),
               signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);

  [manager_ loadModel:model_];

  EXPECT_EQ(4, [model_ numberOfSections]);
  EXPECT_EQ(
      1,
      [model_ numberOfItemsInSection:[model_ sectionForSectionIdentifier:
                                                 SectionIdentifierTimeRange]]);
  EXPECT_EQ(
      5,
      [model_ numberOfItemsInSection:[model_ sectionForSectionIdentifier:
                                                 SectionIdentifierDataTypes]]);
  EXPECT_EQ(
      0,
      [model_
          numberOfItemsInSection:[model_ sectionForSectionIdentifier:
                                             SectionIdentifierSavedSiteData]]);
  EXPECT_EQ(
      0,
      [model_
          numberOfItemsInSection:[model_ sectionForSectionIdentifier:
                                             SectionIdentifierGoogleAccount]]);
}

TEST_F(ClearBrowsingDataManagerTest, TestCacheCounterFormattingForAllTime) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetInteger(browsing_data::prefs::kDeleteTimePeriod,
                    static_cast<int>(browsing_data::TimePeriod::ALL_TIME));
  CacheCounter counter(profile_.get());

  NSByteCountFormatter* formatter = [[NSByteCountFormatter alloc] init];
  formatter.allowedUnits = NSByteCountFormatterUseAll &
                           (~NSByteCountFormatterUseBytes) &
                           (~NSByteCountFormatterUseKB);
  formatter.countStyle = NSByteCountFormatterCountStyleMemory;

  NSString* format_1_mb = [formatter stringFromByteCount:(1 << 20)];
  NSString* less_than_1_mb =
      l10n_util::GetNSString(IDS_DEL_CACHE_COUNTER_ALMOST_EMPTY);
  NSString* format_1_5_mb =
      [formatter stringFromByteCount:(1 << 20) + (1 << 19)];
  NSString* format_2_mb = [formatter stringFromByteCount:(1 << 21)];
  NSString* format_1_gb = [formatter stringFromByteCount:(1 << 30)];

  // Test multiple possible types of formatting.
  // clang-format off
    const struct TestCase {
        int cache_size;
        NSString* expected_output;
    } kTestCases[] = {
        {0, less_than_1_mb},
        {(1 << 20) - 1, less_than_1_mb},
        {(1 << 20), format_1_mb},
        {(1 << 20) + (1 << 19), format_1_5_mb},
        {(1 << 21) - 10, format_2_mb},
        {(1 << 21), format_2_mb},
        {(1 << 30), format_1_gb}
    };
  // clang-format on

  for (const TestCase& test_case : kTestCases) {
    browsing_data::BrowsingDataCounter::FinishedResult result(
        &counter, test_case.cache_size);
    NSString* output = [manager_ counterTextFromResult:result];
    EXPECT_NSEQ(test_case.expected_output, output);
  }
}

TEST_F(ClearBrowsingDataManagerTest,
       TestCacheCounterFormattingForLessThanAllTime) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetInteger(browsing_data::prefs::kDeleteTimePeriod,
                    static_cast<int>(browsing_data::TimePeriod::LAST_HOUR));
  CacheCounter counter(profile_.get());

  NSByteCountFormatter* formatter = [[NSByteCountFormatter alloc] init];
  formatter.allowedUnits = NSByteCountFormatterUseAll &
                           (~NSByteCountFormatterUseBytes) &
                           (~NSByteCountFormatterUseKB);
  formatter.countStyle = NSByteCountFormatterCountStyleMemory;

  NSString* almost_empty =
      l10n_util::GetNSString(IDS_DEL_CACHE_COUNTER_ALMOST_EMPTY);
  NSString* format_1_mb = [formatter stringFromByteCount:(1 << 20)];
  NSString* less_than_1_mb =
      l10n_util::GetNSStringF(IDS_DEL_CACHE_COUNTER_UPPER_ESTIMATE,
                              base::SysNSStringToUTF16(format_1_mb));
  NSString* format_1_5_mb =
      [formatter stringFromByteCount:(1 << 20) + (1 << 19)];
  NSString* less_than_1_5_mb =
      l10n_util::GetNSStringF(IDS_DEL_CACHE_COUNTER_UPPER_ESTIMATE,
                              base::SysNSStringToUTF16(format_1_5_mb));
  NSString* format_2_mb = [formatter stringFromByteCount:(1 << 21)];
  NSString* less_than_2_mb =
      l10n_util::GetNSStringF(IDS_DEL_CACHE_COUNTER_UPPER_ESTIMATE,
                              base::SysNSStringToUTF16(format_2_mb));
  NSString* format_1_gb = [formatter stringFromByteCount:(1 << 30)];
  NSString* less_than_1_gb =
      l10n_util::GetNSStringF(IDS_DEL_CACHE_COUNTER_UPPER_ESTIMATE,
                              base::SysNSStringToUTF16(format_1_gb));

  // Test multiple possible types of formatting.
  // clang-format off
    const struct TestCase {
        int cache_size;
        NSString* expected_output;
    } kTestCases[] = {
        {0, almost_empty},
        {(1 << 20) - 1, almost_empty},
        {(1 << 20), less_than_1_mb},
        {(1 << 20) + (1 << 19), less_than_1_5_mb},
        {(1 << 21) - 10, less_than_2_mb},
        {(1 << 21), less_than_2_mb},
        {(1 << 30), less_than_1_gb}
    };
  // clang-format on

  for (const TestCase& test_case : kTestCases) {
    browsing_data::BrowsingDataCounter::FinishedResult result(
        &counter, test_case.cache_size);
    NSString* output = [manager_ counterTextFromResult:result];
    EXPECT_NSEQ(test_case.expected_output, output);
  }
}

TEST_F(ClearBrowsingDataManagerTest, TestOnPreferenceChanged) {
  [manager_ loadModel:model_];
  NSArray* timeRangeItems =
      [model_ itemsInSectionWithIdentifier:SectionIdentifierTimeRange];
  ASSERT_EQ(1UL, timeRangeItems.count);
  TableViewDetailIconItem* timeRangeItem = timeRangeItems.firstObject;
  ASSERT_TRUE([timeRangeItem isKindOfClass:[TableViewDetailIconItem class]]);

  // Changes of Time Range should trigger updates on Time Range item's
  // detailText.
  time_range_pref_.SetValue(2);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_WEEK),
              timeRangeItem.detailText);
  time_range_pref_.SetValue(3);
  EXPECT_NSEQ(
      l10n_util::GetNSString(
          IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_LAST_FOUR_WEEKS),
      timeRangeItem.detailText);
}

TEST_F(ClearBrowsingDataManagerTest, TestGoogleDSETextSignedIn) {
  AuthenticationServiceFactory::GetForProfile(profile_.get())
      ->SignIn(fake_identity(),
               signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);

  [manager_ loadModel:model_];

  ASSERT_TRUE(
      [model_ hasSectionForSectionIdentifier:SectionIdentifierGoogleAccount]);
  ASSERT_FALSE(
      ![model_ footerForSectionWithIdentifier:SectionIdentifierGoogleAccount]);
  ListItem* googleAccount =
      [model_ footerForSectionWithIdentifier:SectionIdentifierGoogleAccount];
  TableViewLinkHeaderFooterItem* accountFooterTextItem =
      base::apple::ObjCCastStrict<TableViewLinkHeaderFooterItem>(googleAccount);
  ASSERT_TRUE(([accountFooterTextItem.text rangeOfString:@"Google"].location !=
               NSNotFound));
  ASSERT_EQ(2u, [accountFooterTextItem.urls count]);
}

TEST_F(ClearBrowsingDataManagerTest, TestGoogleDSETextSignedOut) {
  AuthenticationServiceFactory::GetForProfile(profile_.get())
      ->SignOut(signin_metrics::ProfileSignout::kAbortSignin,
                /*force_clear_browsing_data=*/false, nil);

  [manager_ loadModel:model_];

  ASSERT_FALSE(
      [model_ hasSectionForSectionIdentifier:SectionIdentifierGoogleAccount]);
}

TEST_F(ClearBrowsingDataManagerTest, TestPrepopulatedTextSignedIn) {
  AuthenticationServiceFactory::GetForProfile(profile_.get())
      ->SignIn(fake_identity(),
               signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);

  // Set DSE to one from "prepoulated list".
  const std::string kEngineP1Name = "prepopulated-1";
  const GURL kEngineP1Url = GURL("https://p1.com?q={searchTerms}");

  AddPriorSearchEngine(/* short_name */ kEngineP1Name,
                       /* searchable_url */ kEngineP1Url,
                       /* prepopulated_id */ 1001, /* set_default */ true);

  [manager_ loadModel:model_];

  ASSERT_TRUE(
      [model_ hasSectionForSectionIdentifier:SectionIdentifierGoogleAccount]);
  ASSERT_FALSE(
      ![model_ footerForSectionWithIdentifier:SectionIdentifierGoogleAccount]);
  ListItem* googleAccount =
      [model_ footerForSectionWithIdentifier:SectionIdentifierGoogleAccount];
  TableViewLinkHeaderFooterItem* accountFooterTextItem =
      base::apple::ObjCCastStrict<TableViewLinkHeaderFooterItem>(googleAccount);
  ASSERT_TRUE(
      ([accountFooterTextItem.text
           rangeOfString:[NSString
                             stringWithCString:kEngineP1Name.c_str()
                                      encoding:[NSString
                                                   defaultCStringEncoding]]]
           .location != NSNotFound));
  ASSERT_EQ(1u, [accountFooterTextItem.urls count]);
}

TEST_F(ClearBrowsingDataManagerTest, TestPrepopulatedTextSignedOut) {
  AuthenticationServiceFactory::GetForProfile(profile_.get())
      ->SignOut(signin_metrics::ProfileSignout::kAbortSignin,
                /*force_clear_browsing_data=*/false, nil);

  // Set DSE to one from "prepoulated list".
  const std::string kEngineP1Name = "prepopulated-1";
  // NSString kEngineP1NameNS = kEngineP1Name as NS
  const GURL kEngineP1Url = GURL("https://p1.com?q={searchTerms}");

  AddPriorSearchEngine(/* short_name */ kEngineP1Name,
                       /* searchable_url */ kEngineP1Url,
                       /* prepopulated_id */ 1001, /* set_default */ true);

  [manager_ loadModel:model_];

  ASSERT_TRUE(
      [model_ hasSectionForSectionIdentifier:SectionIdentifierGoogleAccount]);
  ASSERT_FALSE(
      ![model_ footerForSectionWithIdentifier:SectionIdentifierGoogleAccount]);
  ListItem* googleAccount =
      [model_ footerForSectionWithIdentifier:SectionIdentifierGoogleAccount];
  TableViewLinkHeaderFooterItem* accountFooterTextItem =
      base::apple::ObjCCastStrict<TableViewLinkHeaderFooterItem>(googleAccount);
  ASSERT_TRUE(
      ([accountFooterTextItem.text
           rangeOfString:[NSString
                             stringWithCString:kEngineP1Name.c_str()
                                      encoding:[NSString
                                                   defaultCStringEncoding]]]
           .location != NSNotFound));
  ASSERT_EQ(0u, [accountFooterTextItem.urls count]);
}

TEST_F(ClearBrowsingDataManagerTest, TestCustomTextSignedIn) {
  AuthenticationServiceFactory::GetForProfile(profile_.get())
      ->SignIn(fake_identity(),
               signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);

  // Set DSE to a be fully custom.
  const std::string kEngineC1Name = "custom-1";
  const GURL kEngineC1Url = GURL("https://c1.com?q={searchTerms}");

  AddCustomSearchEngine(
      /* short_name */ kEngineC1Name,
      /* searchable_url */ kEngineC1Url,
      /* last_visited_time */ base::Time::Now() - base::Seconds(10),
      /* set_default */ true);

  [manager_ loadModel:model_];

  ASSERT_TRUE(
      [model_ hasSectionForSectionIdentifier:SectionIdentifierGoogleAccount]);
  ASSERT_FALSE(
      ![model_ footerForSectionWithIdentifier:SectionIdentifierGoogleAccount]);
  ListItem* googleAccount =
      [model_ footerForSectionWithIdentifier:SectionIdentifierGoogleAccount];
  TableViewLinkHeaderFooterItem* accountFooterTextItem =
      base::apple::ObjCCastStrict<TableViewLinkHeaderFooterItem>(googleAccount);
  ASSERT_FALSE(
      ([accountFooterTextItem.text
           rangeOfString:[NSString
                             stringWithCString:kEngineC1Name.c_str()
                                      encoding:[NSString
                                                   defaultCStringEncoding]]]
           .location != NSNotFound));
  ASSERT_EQ(1u, [accountFooterTextItem.urls count]);
}

TEST_F(ClearBrowsingDataManagerTest, TestCustomeTextSignedOut) {
  AuthenticationServiceFactory::GetForProfile(profile_.get())
      ->SignOut(signin_metrics::ProfileSignout::kAbortSignin,
                /*force_clear_browsing_data=*/false, nil);

  // Set DSE to a be fully custom.
  const std::string kEngineC1Name = "custom-1";
  const GURL kEngineC1Url = GURL("https://c1.com?q={searchTerms}");

  AddCustomSearchEngine(
      /* short_name */ kEngineC1Name,
      /* searchable_url */ kEngineC1Url,
      /* last_visited_time */ base::Time::Now() - base::Seconds(10),
      /* set_default */ true);

  [manager_ loadModel:model_];

  ASSERT_TRUE(
      [model_ hasSectionForSectionIdentifier:SectionIdentifierGoogleAccount]);
  ASSERT_FALSE(
      ![model_ footerForSectionWithIdentifier:SectionIdentifierGoogleAccount]);
  ListItem* googleAccount =
      [model_ footerForSectionWithIdentifier:SectionIdentifierGoogleAccount];
  TableViewLinkHeaderFooterItem* accountFooterTextItem =
      base::apple::ObjCCastStrict<TableViewLinkHeaderFooterItem>(googleAccount);
  ASSERT_FALSE(
      ([accountFooterTextItem.text
           rangeOfString:[NSString
                             stringWithCString:kEngineC1Name.c_str()
                                      encoding:[NSString
                                                   defaultCStringEncoding]]]
           .location != NSNotFound));
  ASSERT_EQ(0u, [accountFooterTextItem.urls count]);
}

}  // namespace
