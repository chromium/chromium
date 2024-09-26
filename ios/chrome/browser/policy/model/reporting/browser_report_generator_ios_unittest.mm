// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/browser_report_generator_ios.h"

#import <Foundation/Foundation.h>

#import "base/files/file_path.h"
#import "base/run_loop.h"
#import "base/test/bind.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/policy/model/reporting/reporting_delegate_factory_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/platform_test.h"

namespace em = enterprise_management;

namespace enterprise_reporting {

class BrowserReportGeneratorIOSTest : public PlatformTest {
 public:
  BrowserReportGeneratorIOSTest() : generator_(&delegate_factory_) {
    profile_ =
        profile_manager_.AddProfileWithBuilder(TestProfileIOS::Builder());
  }
  BrowserReportGeneratorIOSTest(const BrowserReportGeneratorIOSTest&) = delete;
  BrowserReportGeneratorIOSTest& operator=(
      const BrowserReportGeneratorIOSTest&) = delete;
  ~BrowserReportGeneratorIOSTest() override = default;

  void GenerateAndVerify() {
    base::RunLoop run_loop;
    const base::FilePath path = profile_->GetStatePath();
    generator_.Generate(
        ReportType::kFull,
        base::BindLambdaForTesting(
            [&run_loop, &path](std::unique_ptr<em::BrowserReport> report) {
              ASSERT_TRUE(report.get());

              EXPECT_NE(std::string(), report->browser_version());
              EXPECT_TRUE(report->has_channel());

              EXPECT_NE(std::string(), report->executable_path());

              ASSERT_EQ(1, report->chrome_user_profile_infos_size());
              em::ChromeUserProfileInfo profile =
                  report->chrome_user_profile_infos(0);
              EXPECT_EQ(path.AsUTF8Unsafe(), profile.id());
              EXPECT_EQ(path.BaseName().AsUTF8Unsafe(), profile.name());

              EXPECT_FALSE(profile.is_detail_available());

              run_loop.Quit();
            }));
    run_loop.Run();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<ProfileIOS> profile_;

  ReportingDelegateFactoryIOS delegate_factory_;
  BrowserReportGenerator generator_;
};

TEST_F(BrowserReportGeneratorIOSTest, GenerateBasicReportWithProfile) {
  GenerateAndVerify();
}

}  // namespace enterprise_reporting
