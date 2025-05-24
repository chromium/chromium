// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROFILE_MODEL_TEST_WITH_PROFILE_H_
#define IOS_CHROME_BROWSER_PROFILE_MODEL_TEST_WITH_PROFILE_H_

#include <memory>
#include <string_view>

#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "ios/chrome/browser/profile/model/profile_manager_ios_impl.h"
#include "ios/chrome/browser/signin/model/account_profile_mapper.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

class IOSChromeIOThread;
class ScopedProfileKeepAliveIOS;

// Test class that allows creating real profiles.
class TestWithProfile : public PlatformTest {
 public:
  TestWithProfile();
  TestWithProfile(const std::vector<base::test::FeatureRef>& enabled_features,
                  const std::vector<base::test::FeatureRef>& disabled_features);

  ~TestWithProfile() override;

  // Returns the path to the directory in which profiles are stored.
  const base::FilePath& profile_data_dir() const { return profile_data_dir_; }

  // Returns the ProfileManagerIOS instance.
  ProfileManagerIOS& profile_manager() { return profile_manager_; }

  // Returns the ProfileAttributesStorageIOS instance.
  ProfileAttributesStorageIOS& attributes_storage() {
    return *profile_manager_.GetProfileAttributesStorage();
  }

  // Helper to synchronously load a profile with `profile_name`.
  ScopedProfileKeepAliveIOS LoadProfile(std::string_view name);

  // Helper to synchronously create a profile with `profile_name`.
  ScopedProfileKeepAliveIOS CreateProfile(std::string_view name);

 private:
  // Helper around a ScopedFeatureList that initialize it in its constructor.
  class InitializedFeatureList {
   public:
    InitializedFeatureList(
        const std::vector<base::test::FeatureRef>& enabled_features,
        const std::vector<base::test::FeatureRef>& disabled_features);

    InitializedFeatureList(const InitializedFeatureList&) = delete;
    InitializedFeatureList& operator=(const InitializedFeatureList&) = delete;

    ~InitializedFeatureList();

   private:
    base::test::ScopedFeatureList scoped_feature_list_;
  };

  InitializedFeatureList initialized_scoped_feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<IOSChromeIOThread> chrome_io_;
  web::WebTaskEnvironment web_task_environment_{
      web::WebTaskEnvironment::IOThreadType::REAL_THREAD_DELAYED};
  // Some KeyedService requires a VariationsIdsProvider to be installed.
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  const base::FilePath profile_data_dir_;
  ProfileManagerIOSImpl profile_manager_;
  std::unique_ptr<AccountProfileMapper> account_profile_mapper_;
};

#endif  // IOS_CHROME_BROWSER_PROFILE_MODEL_TEST_WITH_PROFILE_H_
