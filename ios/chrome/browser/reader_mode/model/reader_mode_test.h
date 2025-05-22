// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_TEST_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_TEST_H_

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
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

 protected:
  void SetUp() override;

  // Creates a fake web state for use in Reader Mode functions.
  std::unique_ptr<web::FakeWebState> CreateWebState();

  // Loads the web page with fake HTML content and commits the URL.
  void LoadWebpage(web::FakeWebState* web_state, const GURL& url);

  // Updates the provided `web_state` with the Reader Mode eligibility mapped
  // to the provided `url`.
  void SetReaderModeState(web::FakeWebState* web_state,
                          const GURL& url,
                          ReaderModeHeuristicResult eligibility,
                          std::string distilled_content);

  // Waits for Reader Mode content to be loaded and ready to query.
  void WaitForReaderModeContentReady();

  web::WebTaskEnvironment* task_environment() { return &task_environment_; }

  TestProfileIOS* profile() { return profile_.get(); }

 private:
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<TestProfileIOS> profile_;

  std::vector<std::unique_ptr<base::Value>> distiller_result_values_;
};

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_TEST_H_
