// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mobile_metrics/tap_friendliness_checker.h"

#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock-matchers.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {

static constexpr char kBaseUrl[] = "http://www.test.com/";
static constexpr int kDeviceWidth = 480;
static constexpr int kDeviceHeight = 800;
static constexpr float kMinimumZoom = 0.25f;
static constexpr float kMaximumZoom = 5;

class TapFriendlinessCheckerTest : public testing::Test {
 protected:
  void TearDown() override {
    ThreadState::Current()->CollectAllGarbageForTesting();
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
    recorder_ = nullptr;
  }

  void TapAt(int x, int y) {
    gfx::PointF pos(x, y);
    WebGestureEvent tap_event(WebInputEvent::Type::kGestureTap,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::GetStaticTimeStampForTests(),
                              WebGestureDevice::kTouchscreen);
    tap_event.SetPositionInWidget(pos);
    tap_event.SetPositionInScreen(pos);
    helper_->LocalMainFrame()->GetFrame()->GetEventHandler().HandleGestureEvent(
        tap_event);
  }
  static void ConfigureAndroidSettings(WebSettings* settings) {
    settings->SetViewportEnabled(true);
    settings->SetViewportMetaEnabled(true);
  }
  ukm::TestUkmRecorder* GetUkmRecorder() { return recorder_.get(); }

  void LoadHTML(const std::string& html, float device_scale = 1.0) {
    helper_ = std::make_unique<frame_test_helpers::WebViewHelper>();
    helper_->Initialize(nullptr, nullptr, ConfigureAndroidSettings);
    helper_->GetWebView()->MainFrameWidget()->SetDeviceScaleFactorForTesting(
        device_scale);
    helper_->Resize(gfx::Size(kDeviceWidth, kDeviceHeight));
    helper_->GetWebView()->GetPage()->SetDefaultPageScaleLimits(kMinimumZoom,
                                                                kMaximumZoom);
    helper_->GetWebView()->GetPage()->GetSettings().SetTextAutosizingEnabled(
        true);
    helper_->GetWebView()
        ->GetPage()
        ->GetSettings()
        .SetShrinksViewportContentToFit(true);
    helper_->GetWebView()->GetPage()->GetSettings().SetViewportStyle(
        mojom::blink::ViewportStyle::kMobile);
    helper_->LoadAhem();
    frame_test_helpers::LoadHTMLString(helper_->GetWebView()->MainFrameImpl(),
                                       html,
                                       url_test_helpers::ToKURL(kBaseUrl));
    recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

 protected:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<frame_test_helpers::WebViewHelper> helper_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> recorder_;
};

TEST_F(TapFriendlinessCheckerTest, NoTapTarget) {
  LoadHTML(R"HTML(
<html>
  <head>
    <meta name="viewport" content="width=400, initial-scale=1">
  </head>
  <body style="font-size: 18px; margin: 0px">
  </body>
</html>
)HTML");
  ukm::TestUkmRecorder* recorder = GetUkmRecorder();
  TapAt(5, 5);
  ASSERT_EQ(recorder->entries_count(), 0u);
  auto entries =
      recorder->GetEntriesByName("MobileFriendliness.TappedBadTargets");
  ASSERT_EQ(entries.size(), 0u);
}

TEST_F(TapFriendlinessCheckerTest, TapTargetExists) {
  LoadHTML(R"HTML(
<html>
  <head>
    <meta name="viewport" content="width=400, initial-scale=1">
  </head>
  <body style="font-size: 18px; margin: 0px">
    <button style="width: 400px; height: 400px">
      button
    </a>
  </body>
</html>
)HTML");

  ukm::TestUkmRecorder* recorder = GetUkmRecorder();
  TapAt(200, 200);
  ASSERT_EQ(recorder->entries_count(), 1u);
  auto entries =
      recorder->GetEntriesByName("MobileFriendliness.TappedBadTargets");
  ASSERT_EQ(entries.size(), 1u);
  ASSERT_EQ(entries[0]->event_hash,
            ukm::builders::MobileFriendliness_TappedBadTargets::kEntryNameHash);
  ASSERT_TRUE(entries[0]->metrics.empty());
}

TEST_F(TapFriendlinessCheckerTest, ClickThreeTimes) {
  LoadHTML(R"HTML(
<html>
  <head>
    <meta name="viewport" content="width=400, initial-scale=1">
  </head>
  <body style="font-size: 18px; margin: 0px">
    <button style="width: 400px; height: 400px">
      button
    </a>
  </body>
</html>
)HTML");

  ukm::TestUkmRecorder* recorder = GetUkmRecorder();
  TapAt(200, 200);
  TapAt(100, 300);
  TapAt(250, 150);
  ASSERT_EQ(recorder->entries_count(), 3u);
  auto entries =
      recorder->GetEntriesByName("MobileFriendliness.TappedBadTargets");
  ASSERT_EQ(entries.size(), 3u);
  ASSERT_TRUE(entries[0]->metrics.empty());
  ASSERT_EQ(entries[0]->event_hash,
            ukm::builders::MobileFriendliness_TappedBadTargets::kEntryNameHash);
  ASSERT_TRUE(entries[1]->metrics.empty());
  ASSERT_EQ(entries[1]->event_hash,
            ukm::builders::MobileFriendliness_TappedBadTargets::kEntryNameHash);
  ASSERT_TRUE(entries[2]->metrics.empty());
  ASSERT_EQ(entries[2]->event_hash,
            ukm::builders::MobileFriendliness_TappedBadTargets::kEntryNameHash);
}

TEST_F(TapFriendlinessCheckerTest, SmallTapTarget) {
  LoadHTML(R"HTML(
<html>
  <head>
    <meta name="viewport" content="width=400, initial-scale=1">
  </head>
  <body style="font-size: 18px; margin: 0px">
    <button style="margin: 190px; width: 20px; height: 20px">
      button
    </a>
  </body>
</html>
)HTML");

  ukm::TestUkmRecorder* recorder = GetUkmRecorder();
  TapAt(200, 200);
  ASSERT_EQ(recorder->entries_count(), 1u);
  auto entries =
      recorder->GetEntriesByName("MobileFriendliness.TappedBadTargets");
  ASSERT_EQ(entries.size(), 1u);
  ASSERT_EQ(entries[0]->event_hash,
            ukm::builders::MobileFriendliness_TappedBadTargets::kEntryNameHash);
  ASSERT_EQ(entries[0]->metrics.size(), 1u);
  auto it = entries[0]->metrics.find(
      ukm::builders::MobileFriendliness_TappedBadTargets::kTooSmallNameHash);
  ASSERT_EQ(it->second, 1);
}

TEST_F(TapFriendlinessCheckerTest, CloseDisplayEdgeTapTarget) {
  LoadHTML(R"HTML(
<html>
  <head>
    <meta name="viewport" content="width=400, initial-scale=1">
  </head>
  <body style="font-size: 18px; margin: 0px">
    <button style="margin-left: 190px; width: 200px; height: 20px">
      button
    </a>
  </body>
</html>
)HTML");

  ukm::TestUkmRecorder* recorder = GetUkmRecorder();
  TapAt(200, 10);
  TapAt(200, 150);  // Miss tap, should be ignored.
  ASSERT_EQ(recorder->entries_count(), 1u);
  auto entries =
      recorder->GetEntriesByName("MobileFriendliness.TappedBadTargets");
  ASSERT_EQ(entries.size(), 1u);
  ASSERT_EQ(entries[0]->event_hash,
            ukm::builders::MobileFriendliness_TappedBadTargets::kEntryNameHash);
  ASSERT_EQ(entries[0]->metrics.size(), 1u);
  auto it = entries[0]->metrics.find(
      ukm::builders::MobileFriendliness_TappedBadTargets::
          kCloseDisplayEdgeNameHash);
  ASSERT_EQ(it->second, 1);
}

TEST_F(TapFriendlinessCheckerTest, SmallAndCloseDisplayEdgeTapTarget) {
  LoadHTML(R"HTML(
<html>
  <head>
    <meta name="viewport" content="width=400, initial-scale=1">
  </head>
  <body style="font-size: 18px; margin: 0px">
    <button style="margin-left: 190px; width: 20px; height: 20px">
      button
    </a>
  </body>
</html>
)HTML");

  ukm::TestUkmRecorder* recorder = GetUkmRecorder();
  TapAt(200, 10);
  TapAt(200, 150);  // Miss tap, should be ignored.
  ASSERT_EQ(recorder->entries_count(), 1u);
  auto entries =
      recorder->GetEntriesByName("MobileFriendliness.TappedBadTargets");
  ASSERT_EQ(entries.size(), 1u);
  ASSERT_EQ(entries[0]->event_hash,
            ukm::builders::MobileFriendliness_TappedBadTargets::kEntryNameHash);

  ASSERT_EQ(entries[0]->metrics.size(), 2u);
  auto close_it = entries[0]->metrics.find(
      ukm::builders::MobileFriendliness_TappedBadTargets::
          kCloseDisplayEdgeNameHash);
  ASSERT_EQ(close_it->second, 1);
  auto small_it = entries[0]->metrics.find(
      ukm::builders::MobileFriendliness_TappedBadTargets::kTooSmallNameHash);
  ASSERT_EQ(small_it->second, 1);
}

}  // namespace blink
