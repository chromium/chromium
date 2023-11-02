// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_TEST_HEADLESS_POLICY_BROWSERTEST_H_
#define HEADLESS_TEST_HEADLESS_POLICY_BROWSERTEST_H_

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "headless/lib/browser/headless_browser_impl.h"

namespace headless {

template <typename Base>
class HeadlessBrowserTestWithPolicy : public Base {
 protected:
  // Implement to set policies before headless browser is instantiated.
  virtual void SetPolicy() {}

  void SetUp() override {
    mock_provider_ = std::make_unique<
        testing::NiceMock<policy::MockConfigurationPolicyProvider>>();
    mock_provider_->SetDefaultReturns(
        /*is_initialization_complete_return=*/false,
        /*is_first_policy_load_complete_return=*/false);
    policy::BrowserPolicyConnectorBase::SetPolicyProviderForTesting(
        mock_provider_.get());
    SetPolicy();
    Base::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    Base::SetUpInProcessBrowserTestFixture();
    CreateTempUserDir();
  }

  void TearDown() override {
    Base::TearDown();
    mock_provider_->Shutdown();
    policy::BrowserPolicyConnectorBase::SetPolicyProviderForTesting(nullptr);
  }

  void CreateTempUserDir() {
    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());
    EXPECT_TRUE(base::IsDirectoryEmpty(user_data_dir()));
    Base::options()->user_data_dir = user_data_dir();
  }

  const base::FilePath& user_data_dir() const {
    return user_data_dir_.GetPath();
  }

  PrefService* GetPrefs() {
    return static_cast<HeadlessBrowserImpl*>(Base::browser())->GetPrefs();
  }

  base::ScopedTempDir user_data_dir_;
  std::unique_ptr<policy::MockConfigurationPolicyProvider> mock_provider_;
};

}  // namespace headless

#endif  // HEADLESS_TEST_HEADLESS_POLICY_BROWSERTEST_H_
