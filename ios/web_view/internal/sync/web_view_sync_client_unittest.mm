// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/web_view_sync_client.h"

#import <memory>

#import "base/feature_list.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/scoped_refptr.h"
#import "components/autofill/core/browser/webdata/mock_autofill_webdata_service.h"
#import "components/history/core/common/pref_names.h"
#import "components/invalidation/impl/fake_invalidation_service.h"
#import "components/password_manager/core/browser/test_password_store.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/sync/base/model_type.h"
#import "components/sync/service/data_type_controller.h"
#import "components/sync/test/test_model_type_store_service.h"
#import "components/sync/test/test_sync_service.h"
#import "components/sync_device_info/fake_device_info_sync_service.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

class WebViewSyncClientTest : public PlatformTest {
 protected:
  WebViewSyncClientTest()
      : profile_web_data_service_(
            base::MakeRefCounted<autofill::MockAutofillWebDataService>()),
        account_web_data_service_(
            base::MakeRefCounted<autofill::MockAutofillWebDataService>()),
        profile_password_store_(
            base::MakeRefCounted<password_manager::TestPasswordStore>()),
        account_password_store_(
            base::FeatureList::IsEnabled(
                password_manager::features::kEnablePasswordsAccountStorage)
                ? base::MakeRefCounted<password_manager::TestPasswordStore>(
                      password_manager::IsAccountStore(true))
                : nullptr),
        client_(profile_web_data_service_.get(),
                account_web_data_service_.get(),
                profile_password_store_.get(),
                account_password_store_.get(),
                &pref_service_,
                identity_test_environment_.identity_manager(),
                &model_type_store_service_,
                &device_info_sync_service_,
                &invalidation_service_,
                /*sync_invalidations_service=*/nullptr) {
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kSavingBrowserHistoryDisabled, true);
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kWereOldGoogleLoginsRemoved, false);
    profile_password_store_->Init(&pref_service_,
                                  /*affiliated_match_helper=*/nullptr);
    if (account_password_store_) {
      account_password_store_->Init(&pref_service_,
                                    /*affiliated_match_helper=*/nullptr);
    }
  }

  ~WebViewSyncClientTest() override {
    profile_password_store_->ShutdownOnUIThread();
    if (account_password_store_) {
      account_password_store_->ShutdownOnUIThread();
    }
  }

  web::WebTaskEnvironment task_environment_;
  scoped_refptr<autofill::MockAutofillWebDataService> profile_web_data_service_;
  scoped_refptr<autofill::MockAutofillWebDataService> account_web_data_service_;
  scoped_refptr<password_manager::TestPasswordStore> profile_password_store_;
  scoped_refptr<password_manager::TestPasswordStore> account_password_store_;
  TestingPrefServiceSimple pref_service_;
  signin::IdentityTestEnvironment identity_test_environment_;
  syncer::TestModelTypeStoreService model_type_store_service_;
  syncer::FakeDeviceInfoSyncService device_info_sync_service_;
  invalidation::FakeInvalidationService invalidation_service_;
  WebViewSyncClient client_;
};

// Verify enabled data types.
TEST_F(WebViewSyncClientTest, CreateDataTypeControllers) {
  syncer::TestSyncService sync_service;
  syncer::DataTypeController::TypeVector data_type_controllers =
      client_.CreateDataTypeControllers(&sync_service);
  syncer::ModelTypeSet allowed_types = {syncer::DEVICE_INFO,
                                        syncer::AUTOFILL,
                                        syncer::AUTOFILL_PROFILE,
                                        syncer::AUTOFILL_WALLET_DATA,
                                        syncer::AUTOFILL_WALLET_METADATA,
                                        syncer::PASSWORDS};
  for (const auto& data_type_controller : data_type_controllers) {
    ASSERT_TRUE(allowed_types.Has(data_type_controller->type()));
    allowed_types.Remove(data_type_controller->type());
  }
  EXPECT_TRUE(allowed_types.Empty());
}

}  // namespace ios_web_view
