// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/reporting/browser_report_generator_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import <Foundation/Foundation.h>

#import "base/files/file_path.h"
#import "base/run_loop.h"
#import "base/test/bind.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#import "ios/chrome/browser/policy/reporting/reporting_delegate_factory_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#import "ios/chrome/test/testing_application_context.h"
#import "testing/platform_test.h"

namespace em = enterprise_management;

namespace enterprise_reporting {

namespace {

const base::FilePath kProfilePath = base::FilePath("fake/profile/default");

}  // namespace

class BrowserReportGeneratorIOSTest : public PlatformTest {
 public:
  BrowserReportGeneratorIOSTest() : generator_(&delegate_factory_) {
    TestChromeBrowserState::Builder builder;
    builder.SetPath(kProfilePath);
    scoped_browser_state_manager_ =
        std::make_unique<IOSChromeScopedTestingChromeBrowserStateManager>(
            std::make_unique<TestChromeBrowserStateManager>(builder.Build()));
  }
  BrowserReportGeneratorIOSTest(const BrowserReportGeneratorIOSTest&) = delete;
  BrowserReportGeneratorIOSTest& operator=(
      const BrowserReportGeneratorIOSTest&) = delete;
  ~BrowserReportGeneratorIOSTest() override = default;

  void GenerateAndVerify() {
    base::RunLoop run_loop;
    generator_.Generate(
        ReportType::kFull,
        base::BindLambdaForTesting(
            [&run_loop](std::unique_ptr<em::BrowserReport> report) {
              ASSERT_TRUE(report.get());

              EXPECT_NE(std::string(), report->browser_version());
              EXPECT_TRUE(report->has_channel());

              EXPECT_NE(std::string(), report->executable_path());

              ASSERT_EQ(1, report->chrome_user_profile_infos_size());
              em::ChromeUserProfileInfo profile =
                  report->chrome_user_profile_infos(0);
              EXPECT_EQ(kProfilePath.AsUTF8Unsafe(), profile.id());
              EXPECT_EQ(kProfilePath.BaseName().AsUTF8Unsafe(), profile.name());

              EXPECT_FALSE(profile.is_detail_available());

              EXPECT_EQ(0, report->plugins_size());

              run_loop.Quit();
            }));
    run_loop.Run();
  }

 private:
  base::test::TaskEnvironment task_environment_;

  ReportingDelegateFactoryIOS delegate_factory_;
  BrowserReportGenerator generator_;
  std::unique_ptr<IOSChromeScopedTestingChromeBrowserStateManager>
      scoped_browser_state_manager_;
};

TEST_F(BrowserReportGeneratorIOSTest, GenerateBasicReportWithProfile) {
  GenerateAndVerify();
}

}  // namespace enterprise_reporting
