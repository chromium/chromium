// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROFILE_MODEL_TEST_WITH_PROFILE_H_
#define IOS_CHROME_BROWSER_PROFILE_MODEL_TEST_WITH_PROFILE_H_

#include <memory>

#include "base/files/file_path.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "ios/chrome/browser/profile/model/profile_manager_ios_impl.h"
#include "ios/chrome/browser/signin/model/account_profile_mapper.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

class IOSChromeIOThread;

// Test class that allows creating real profiles.
class TestWithProfile : public PlatformTest {
 public:
  TestWithProfile();

  ~TestWithProfile() override;

  // Returns the path to the directory in which profiles are stored.
  const base::FilePath& profile_data_dir() const { return profile_data_dir_; }

  // Returns the ProfileManagerIOS instance.
  ProfileManagerIOS& profile_manager() { return profile_manager_; }

  // Returns the ProfileAttributesStorageIOS instance.
  ProfileAttributesStorageIOS& attributes_storage() {
    return *profile_manager_.GetProfileAttributesStorage();
  }

 private:
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
