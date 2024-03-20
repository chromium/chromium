// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/default_browser_promo_event_exporter.h"

#import "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class DefaultBrowserEventExporterTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    ClearDefaultBrowserPromoData();
  }
  void TearDown() override {
    ClearDefaultBrowserPromoData();
    PlatformTest::TearDown();
  }

  void RequestExportEventsAndVerifyCallback() {
    __block bool callback_called = false;
    feature_engagement::TrackerEventExporter::ExportEventsCallback callback =
        base::BindOnce(
            ^(const std::vector<
                feature_engagement::TrackerEventExporter::EventData> events) {
              export_events_ = events;
              callback_called = true;
            });

    DefaultBrowserEventExporter* exporter = new DefaultBrowserEventExporter();
    exporter->ExportEvents(std::move(callback));

    EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForActionTimeout, ^bool() {
          base::RunLoop().RunUntilIdle();
          return callback_called;
        }));
  }

  int GetExportEventsCount() { return export_events_.size(); }

 private:
  web::WebTaskEnvironment task_environment_;
  std::vector<feature_engagement::TrackerEventExporter::EventData>
      export_events_;
};

TEST_F(DefaultBrowserEventExporterTest, TestFRETimestampMigration) {
  // No events to export.
  RequestExportEventsAndVerifyCallback();
  EXPECT_EQ(GetExportEventsCount(), 0);

  // When there is a FRE event, it should be exported.
  ClearDefaultBrowserPromoData();
  LogUserInteractionWithFirstRunPromo();

  RequestExportEventsAndVerifyCallback();
  EXPECT_EQ(GetExportEventsCount(), 1);

  // Second time there shouldn't be any events to export.
  RequestExportEventsAndVerifyCallback();
  EXPECT_EQ(GetExportEventsCount(), 0);

  // When FRE happened after migration, there shouldn't be any events to export.
  ClearDefaultBrowserPromoData();
  default_browser::NotifyDefaultBrowserFREPromoShown(nullptr);

  RequestExportEventsAndVerifyCallback();
  EXPECT_EQ(GetExportEventsCount(), 0);
}
