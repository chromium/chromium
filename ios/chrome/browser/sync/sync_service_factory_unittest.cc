// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/sync_service_factory.h"

#include <stddef.h>

#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/service/sync_service_impl.h"
#include "ios/chrome/browser/favicon/favicon_service_factory.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/webdata_services/web_data_service_factory.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using syncer::DataTypeController;

class SyncServiceFactoryTest : public PlatformTest {
 public:
  SyncServiceFactoryTest() {
    TestChromeBrowserState::Builder browser_state_builder;
    // BOOKMARKS requires the FaviconService, which requires the HistoryService.
    browser_state_builder.AddTestingFactory(
        ios::FaviconServiceFactory::GetInstance(),
        ios::FaviconServiceFactory::GetDefaultFactory());
    browser_state_builder.AddTestingFactory(
        ios::HistoryServiceFactory::GetInstance(),
        ios::HistoryServiceFactory::GetDefaultFactory());
    // Some services will only be created if there is a WebDataService.
    browser_state_builder.AddTestingFactory(
        ios::WebDataServiceFactory::GetInstance(),
        ios::WebDataServiceFactory::GetDefaultFactory());
    chrome_browser_state_ = browser_state_builder.Build();
  }

  void TearDown() override {
    base::ThreadPoolInstance::Get()->FlushForTesting();
  }

 protected:
  // Returns the collection of default datatypes.
  syncer::ModelTypeSet DefaultDatatypes() {
    static_assert(49 == syncer::GetNumModelTypes(),
                  "When adding a new type, you probably want to add it here as "
                  "well (assuming it is already enabled).");

    syncer::ModelTypeSet datatypes;

    // Common types. This excludes PASSWORDS because the password store factory
    // is null for testing and hence no controller gets instantiated.
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
    datatypes.Put(syncer::CONTACT_INFO);
    datatypes.Put(syncer::DEVICE_INFO);
    if (base::FeatureList::IsEnabled(syncer::kSyncEnableHistoryDataType)) {
      datatypes.Put(syncer::HISTORY);
    }
    datatypes.Put(syncer::HISTORY_DELETE_DIRECTIVES);
    datatypes.Put(syncer::PREFERENCES);
    datatypes.Put(syncer::PRIORITY_PREFERENCES);
    datatypes.Put(syncer::READING_LIST);
    if (base::FeatureList::IsEnabled(syncer::kSyncSegmentationDataType)) {
      datatypes.Put(syncer::SEGMENTATION);
    }
    // TODO(crbug.com/919489) Add SECURITY_EVENTS data type once it is enabled.
    datatypes.Put(syncer::SESSIONS);

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
    datatypes.Put(syncer::SUPERVISED_USER_SETTINGS);
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

    datatypes.Put(syncer::PROXY_TABS);
    datatypes.Put(syncer::TYPED_URLS);
    datatypes.Put(syncer::USER_EVENTS);
    datatypes.Put(syncer::USER_CONSENTS);
    datatypes.Put(syncer::SEND_TAB_TO_SELF);
    // TODO(crbug.com/1445868): Add *_PASSWORD_SHARING_INVITATION once
    // implemented.

    return datatypes;
  }

  ChromeBrowserState* chrome_browser_state() {
    return chrome_browser_state_.get();
  }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

// Verify that the disable sync flag disables creation of the sync service.
TEST_F(SyncServiceFactoryTest, DisableSyncFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(syncer::kDisableSync);
  EXPECT_FALSE(SyncServiceFactory::GetForBrowserState(chrome_browser_state()));
}

// Verify that a normal (no command line flags) SyncServiceImpl can be created
// and properly initialized.
TEST_F(SyncServiceFactoryTest, CreateSyncServiceImplDefault) {
  syncer::SyncServiceImpl* sync_service =
      SyncServiceFactory::GetAsSyncServiceImplForBrowserStateForTesting(
          chrome_browser_state());
  syncer::ModelTypeSet types = sync_service->GetRegisteredDataTypesForTest();
  const syncer::ModelTypeSet default_types = DefaultDatatypes();
  EXPECT_EQ(default_types.Size(), types.Size());
  for (syncer::ModelType type : default_types) {
    EXPECT_TRUE(types.Has(type)) << type << " not found in datatypes map";
  }
}
