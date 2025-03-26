// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/dwa_web_state_observer.h"

#import "base/test/scoped_feature_list.h"
#import "components/metrics/dwa/dwa_entry_builder.h"
#import "components/metrics/dwa/dwa_recorder.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

// Test Fixture for DwaWebStateObserver
class DwaWebStateObserverTest : public PlatformTest {
 protected:
  DwaWebStateObserverTest() : scoped_feature_list_(metrics::dwa::kDwaFeature) {}

  void SetUp() override {
    PlatformTest::SetUp();
    DwaWebStateObserver::CreateForWebState(&web_state_);
    metrics::dwa::DwaRecorder::Get()->EnableRecording();
  }

  void TearDown() override { metrics::dwa::DwaRecorder::Get()->Purge(); }

  void ExpectHasEntriesAndNoPageLoadEvents() {
    EXPECT_TRUE(metrics::dwa::DwaRecorder::Get()->HasEntries());
    EXPECT_FALSE(metrics::dwa::DwaRecorder::Get()->HasPageLoadEvents());
  }

  void ExpectHasPageLoadEventsAndNoEntries() {
    EXPECT_FALSE(metrics::dwa::DwaRecorder::Get()->HasEntries());
    EXPECT_TRUE(metrics::dwa::DwaRecorder::Get()->HasPageLoadEvents());
  }

  web::FakeWebState web_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

void RecordTestDwaEntryMetric() {
  dwa::DwaEntryBuilder builder("Kangaroo.Jumped");
  builder.SetContent("adtech.com");
  builder.SetMetric("Length", 5);
  builder.Record(metrics::dwa::DwaRecorder::Get());
}

TEST_F(DwaWebStateObserverTest,
       DwaRecorderTriggeredWhenPageLoadCompletionSuccess) {
  RecordTestDwaEntryMetric();
  ExpectHasEntriesAndNoPageLoadEvents();
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  ExpectHasPageLoadEventsAndNoEntries();
}

TEST_F(DwaWebStateObserverTest,
       DwaRecorderTriggeredWhenPageLoadCompletionFailure) {
  RecordTestDwaEntryMetric();
  ExpectHasEntriesAndNoPageLoadEvents();
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  ExpectHasEntriesAndNoPageLoadEvents();
}
