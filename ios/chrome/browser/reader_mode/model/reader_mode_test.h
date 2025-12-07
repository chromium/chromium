// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_TEST_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_TEST_H_

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Provides common Reader Mode test infrastructure.
class ReaderModeTest : public PlatformTest {
 public:
  ReaderModeTest();
  ~ReaderModeTest() override;

  // Returns the human-readable label for the ReaderModeHeuristicResult.
  static std::string TestParametersReaderModeHeuristicResultToString(
      testing::TestParamInfo<ReaderModeHeuristicResult> info);

 protected:
  void SetUp() override;
  void TearDown() override;

  // Creates a fake web state for use in Reader Mode functions.
  std::unique_ptr<web::FakeWebState> CreateWebState();

  // Controls for displaying Reading Mode UI on the fake web state.
  void EnableReaderMode(web::WebState* web_state,
                        ReaderModeAccessPoint access_point);
  void DisableReaderMode(web::WebState* web_state);

  // Loads the web page with fake HTML content and commits the URL.
  void LoadWebpage(web::FakeWebState* web_state, const GURL& url);

  // Updates the provided `web_state` with the Reader Mode eligibility mapped
  // to the provided `url`.
  void SetReaderModeState(web::FakeWebState* web_state,
                          const GURL& url,
                          ReaderModeHeuristicResult eligibility,
                          std::string distilled_content);

  // Waits after a page load for the page content to be distillable.
  void WaitForPageLoadDelayAndRunUntilIdle();

  // Waits for Reader mode content availability.
  bool WaitForAvailableReaderModeContentInWebState(web::WebState* web_state);

  web::WebTaskEnvironment* task_environment() { return &task_environment_; }

  TestProfileIOS* profile() { return profile_.get(); }

 private:
  // Adds the given heuristic result to the Readability heuristic JavasScript
  // callback for the specified frame.
  void AddReadabilityHeuristicResultToFrame(ReaderModeHeuristicResult result,
                                            web::FakeWebFrame* web_frame);

  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;

  std::unique_ptr<TestProfileIOS> profile_;

  std::vector<std::unique_ptr<base::Value>> distiller_result_values_;
  std::unique_ptr<base::Value> readability_heuristic_value_;
};

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_TEST_H_
