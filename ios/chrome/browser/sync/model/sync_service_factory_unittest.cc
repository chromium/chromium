// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/model/sync_service_factory.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/data_sharing/public/features.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/service/sync_service_impl.h"
#include "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#include "ios/chrome/browser/history/model/history_service_factory.h"
#include "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#include "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class SyncServiceFactoryTest : public PlatformTest {
 public:
  SyncServiceFactoryTest() {
    TestProfileIOS::Builder profile_builder;
    // BOOKMARKS requires the FaviconService, which requires the HistoryService.
    profile_builder.AddTestingFactory(
        ios::FaviconServiceFactory::GetInstance(),
        ios::FaviconServiceFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        ios::HistoryServiceFactory::GetInstance(),
        ios::HistoryServiceFactory::GetDefaultFactory());
    // Some services will only be created if there is a WebDataService.
    profile_builder.AddTestingFactory(
        ios::WebDataServiceFactory::GetInstance(),
        ios::WebDataServiceFactory::GetDefaultFactory());
    profile_ = std::move(profile_builder).Build();
  }

  void TearDown() override {
    base::ThreadPoolInstance::Get()->FlushForTesting();
  }

 protected:
  // Returns the collection of default datatypes.
  syncer::DataTypeSet DefaultDatatypes() {
    static_assert(53 == syncer::GetNumDataTypes(),
                  "When adding a new type, you probably want to add it here as "
                  "well (assuming it is already enabled).");

    syncer::DataTypeSet datatypes;

    // Common types. This excludes PASSWORDS,
    // INCOMING_PASSWORD_SHARING_INVITATION and
    // INCOMING_PASSWORD_SHARING_INVITATION, because the password store factory
    // is null for testing and hence no controller gets instantiated for those
    // types.
    datatypes.Put(syncer::AUTOFILL);
    datatypes.Put(syncer::AUTOFILL_PROFILE);
    if (base::FeatureList::IsEnabled(
            syncer::kSyncAutofillWalletCredentialData)) {
      datatypes.Put(syncer::AUTOFILL_WALLET_CREDENTIAL);
    }
    datatypes.Put(syncer::AUTOFILL_WALLET_DATA);
    datatypes.Put(syncer::AUTOFILL_WALLET_METADATA);
    datatypes.Put(syncer::AUTOFILL_WALLET_OFFER);
    datatypes.Put(syncer::BOOKMARKS);
    if (base::FeatureList::IsEnabled(commerce::kProductSpecifications)) {
      datatypes.Put(syncer::PRODUCT_COMPARISON);
    }
    datatypes.Put(syncer::CONTACT_INFO);
    datatypes.Put(syncer::DEVICE_INFO);
    datatypes.Put(syncer::HISTORY);
    datatypes.Put(syncer::HISTORY_DELETE_DIRECTIVES);
    datatypes.Put(syncer::PREFERENCES);
    datatypes.Put(syncer::PRIORITY_PREFERENCES);
    datatypes.Put(syncer::READING_LIST);
    // TODO(crbug.com/41434211) Add SECURITY_EVENTS data type once it is
    // enabled.
    datatypes.Put(syncer::SESSIONS);
    datatypes.Put(syncer::SUPERVISED_USER_SETTINGS);
    datatypes.Put(syncer::USER_EVENTS);
    datatypes.Put(syncer::USER_CONSENTS);
    datatypes.Put(syncer::SEND_TAB_TO_SELF);
    if (base::FeatureList::IsEnabled(
            data_sharing::features::kDataSharingFeature)) {
      datatypes.Put(syncer::COLLABORATION_GROUP);
      datatypes.Put(syncer::SHARED_TAB_GROUP_DATA);
    }
    // syncer::PLUS_ADDRESS is excluded because GoogleGroupsManagerFactory is
    // null for testing and hence no controller gets instantiated for the type.
    if (base::FeatureList::IsEnabled(syncer::kSyncPlusAddressSetting)) {
      datatypes.Put(syncer::PLUS_ADDRESS_SETTING);
    }
    if (base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials)) {
      datatypes.Put(syncer::WEBAUTHN_CREDENTIAL);
    }
    return datatypes;
  }

  ProfileIOS* profile() { return profile_.get(); }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Verify that the disable sync flag disables creation of the sync service.
TEST_F(SyncServiceFactoryTest, DisableSyncFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(syncer::kDisableSync);
  EXPECT_FALSE(SyncServiceFactory::GetForProfile(profile()));
}

// Verify that a normal (no command line flags) SyncServiceImpl can be created
// and properly initialized.
TEST_F(SyncServiceFactoryTest, CreateSyncServiceImplDefault) {
  syncer::SyncServiceImpl* sync_service =
      SyncServiceFactory::GetAsSyncServiceImplForBrowserStateForTesting(
          profile());
  syncer::DataTypeSet types = sync_service->GetRegisteredDataTypesForTest();
  const syncer::DataTypeSet default_types = DefaultDatatypes();
  EXPECT_EQ(default_types.size(), types.size());
  for (syncer::DataType type : default_types) {
    EXPECT_TRUE(types.Has(type)) << type << " not found in datatypes map";
  }
}
