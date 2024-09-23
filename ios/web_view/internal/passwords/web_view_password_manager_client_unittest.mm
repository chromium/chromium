// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/web_view_password_manager_client.h"

#import <memory>

#import "base/memory/scoped_refptr.h"
#import "base/test/task_environment.h"
#import "components/autofill/core/browser/logging/stub_log_manager.h"
#import "components/password_manager/core/browser/features/password_manager_features_util.h"
#import "components/password_manager/core/browser/mock_password_form_manager_for_ui.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_form_manager_for_ui.h"
#import "components/password_manager/core/browser/password_manager.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web_view/internal/web_view_browser_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace ios_web_view {

using testing::_;
using testing::Invoke;
using testing::Return;

class WebViewPasswordManagerClientTest : public PlatformTest {
 protected:
  WebViewPasswordManagerClientTest()
      : profile_store_(
            base::MakeRefCounted<password_manager::TestPasswordStore>()),
        account_store_(
            base::MakeRefCounted<password_manager::TestPasswordStore>(
                password_manager::IsAccountStore(true))) {
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kCredentialsEnableService, true);

    profile_store_->Init(&pref_service_, /*affiliated_match_helper=*/nullptr);
    account_store_->Init(&pref_service_, /*affiliated_match_helper=*/nullptr);

    password_manager_client_ = std::make_unique<WebViewPasswordManagerClient>(
        &web_state_, &sync_service_, &pref_service_,
        /*identity_manager=*/nullptr,
        std::make_unique<autofill::StubLogManager>(), profile_store_.get(),
        account_store_.get(), /*reuse_manager=*/nullptr,
        /*requirements_service=*/nullptr);
  }

  ~WebViewPasswordManagerClientTest() override {
    profile_store_->ShutdownOnUIThread();
    account_store_->ShutdownOnUIThread();
  }

  web::FakeWebState web_state_;
  syncer::TestSyncService sync_service_;
  TestingPrefServiceSimple pref_service_;
  autofill::StubLogManager log_manager_;
  scoped_refptr<password_manager::TestPasswordStore> profile_store_;
  scoped_refptr<password_manager::TestPasswordStore> account_store_;
  std::unique_ptr<WebViewPasswordManagerClient> password_manager_client_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(WebViewPasswordManagerClientTest, NoPromptIfBlocklisted) {
  auto password_manager_for_ui =
      std::make_unique<password_manager::MockPasswordFormManagerForUI>();

  EXPECT_CALL(*password_manager_for_ui, IsBlocklisted()).WillOnce(Return(true));

  EXPECT_FALSE(password_manager_client_->PromptUserToSaveOrUpdatePassword(
      std::move(password_manager_for_ui), /*update_password=*/false));
}

TEST_F(WebViewPasswordManagerClientTest, NoPromptIfNotSignedIn) {
  auto password_manager_for_ui =
      std::make_unique<password_manager::MockPasswordFormManagerForUI>();

  EXPECT_CALL(*password_manager_for_ui, IsBlocklisted())
      .WillOnce(Return(false));

  // There's no signed-in user.
  sync_service_.SetSignedOut();

  EXPECT_FALSE(password_manager_client_->PromptUserToSaveOrUpdatePassword(
      std::move(password_manager_for_ui), /*update_password=*/false));
}

TEST_F(WebViewPasswordManagerClientTest, NoPromptIfNotOptedInToAccountStorage) {
  auto password_manager_for_ui =
      std::make_unique<password_manager::MockPasswordFormManagerForUI>();

  EXPECT_CALL(*password_manager_for_ui, IsBlocklisted())
      .WillOnce(Return(false));

  // There is a signed-in user, but they have chosen not to enable passwords.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, syncer::UserSelectableTypeSet());

  EXPECT_FALSE(password_manager_client_->PromptUserToSaveOrUpdatePassword(
      std::move(password_manager_for_ui), /*update_password=*/false));
}

TEST_F(WebViewPasswordManagerClientTest, PromptIfAllConditionsPass) {
  auto password_manager_for_ui =
      std::make_unique<password_manager::MockPasswordFormManagerForUI>();

  EXPECT_CALL(*password_manager_for_ui, IsBlocklisted())
      .WillOnce(Return(false));

  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
  // The user chose to enable passwords (along with all other types).
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/true, syncer::UserSelectableTypeSet());

  EXPECT_TRUE(password_manager_client_->PromptUserToSaveOrUpdatePassword(
      std::move(password_manager_for_ui), /*update_password=*/false));
}

}  // namespace ios_web_view
