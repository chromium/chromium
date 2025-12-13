// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_desktop_resizer.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "remoting/base/constants.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/linux/capture_stream.h"
#include "remoting/host/linux/capture_stream_manager.h"
#include "remoting/host/linux/fake_capture_stream.h"
#include "remoting/host/linux/gnome_display_config.h"
#include "remoting/host/linux/test_util.h"
#include "remoting/proto/control.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

namespace {

using testing::_;
using MonitorMap = std::map<std::string, GnomeDisplayConfig::MonitorInfo>;

constexpr char kMeta0[] = "Meta-0";
constexpr char kMeta1[] = "Meta-1";

static const webrtc::ScreenId kMeta0ScreenId =
    GnomeDisplayConfig::GetScreenId(kMeta0);
static const webrtc::ScreenId kMeta1ScreenId =
    GnomeDisplayConfig::GetScreenId(kMeta1);

int GetDpiNumberForScale(double scale) {
  return static_cast<int>(kDefaultDpi * scale);
}

webrtc::DesktopVector GetDpiForScale(double scale) {
  int dpi_number = GetDpiNumberForScale(scale);
  return {dpi_number, dpi_number};
}

}  // namespace

class GnomeDesktopResizerTest : public testing::Test {
 public:
  GnomeDesktopResizerTest();
  ~GnomeDesktopResizerTest() override;

 protected:
  TestDesktopSize GetTestResolutionForStream(webrtc::ScreenId screen_id);

  // Wait for a call to GnomeDesktopResizer::DoApplyPreferredMonitorsConfig(),
  // which may or may not result in a new config being applied. `trigger` should
  // trigger a call to DoApplyPreferredMonitorsConfig().
  void WaitForPossibleNewConfig(base::OnceClosure trigger = base::DoNothing());

  // Sends `display_config_` to `resizer_`, and wait for a possible new config.
  void SimulateMonitorsChangedAndWaitForPossibleNewConfig();

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeCaptureStreamManager stream_manager_;
  GnomeDesktopResizer resizer_{
      stream_manager_.GetWeakPtr(), /*display_config_monitor=*/nullptr,
      /*registry=*/nullptr,
      base::BindRepeating(&GnomeDesktopResizerTest::ApplyMonitorsConfig,
                          base::Unretained(this))};
  GnomeDisplayConfig display_config_;

 private:
  void ApplyMonitorsConfig(const GnomeDisplayConfig& config);
};

GnomeDesktopResizerTest::GnomeDesktopResizerTest() {
  resizer_.ignore_fractional_scales_in_multimon_ = false;
  display_config_.layout_mode = GnomeDisplayConfig::LayoutMode::kLogical;
  display_config_.monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 100, 100, 2.0)},
      {kMeta1, CreateMonitorInfo(50, 0, 150, 150, 2.0)}};
  stream_manager_.AddVirtualStream(kMeta0ScreenId, {100, 100});
  stream_manager_.AddVirtualStream(kMeta1ScreenId, {150, 150});
  SimulateMonitorsChangedAndWaitForPossibleNewConfig();
}

GnomeDesktopResizerTest::~GnomeDesktopResizerTest() = default;

TestDesktopSize GnomeDesktopResizerTest::GetTestResolutionForStream(
    webrtc::ScreenId screen_id) {
  return TestDesktopSize(stream_manager_.GetStream(screen_id)->resolution());
}

void GnomeDesktopResizerTest::WaitForPossibleNewConfig(
    base::OnceClosure trigger) {
  ASSERT_FALSE(
      resizer_.on_trying_to_apply_preferred_monitors_config_for_testing_);
  base::RunLoop run_loop;
  resizer_.on_trying_to_apply_preferred_monitors_config_for_testing_ =
      run_loop.QuitClosure();
  std::move(trigger).Run();
  run_loop.Run();
}

void GnomeDesktopResizerTest::
    SimulateMonitorsChangedAndWaitForPossibleNewConfig() {
  WaitForPossibleNewConfig(
      base::BindOnce(&GnomeDesktopResizer::OnGnomeDisplayConfigReceived,
                     resizer_.GetWeakPtr(), display_config_));
}

void GnomeDesktopResizerTest::ApplyMonitorsConfig(
    const GnomeDisplayConfig& config) {
  display_config_ = config;
}

TEST_F(GnomeDesktopResizerTest, GetCurrentResolution) {
  ASSERT_EQ(resizer_.GetCurrentResolution(kMeta0ScreenId),
            ScreenResolution({100, 100}, GetDpiForScale(2)));
  ASSERT_EQ(resizer_.GetCurrentResolution(kMeta1ScreenId),
            ScreenResolution({150, 150}, GetDpiForScale(2)));
  ASSERT_TRUE(
      resizer_.GetCurrentResolution(GnomeDisplayConfig::GetScreenId("Meta-2"))
          .IsEmpty());
}

TEST_F(GnomeDesktopResizerTest, GetSupportedResolutions) {
  // We currently just return back the preferred resolution, even if Gnome may
  // not actually support it.
  ScreenResolution preferred_resolution{{999, 999}, GetDpiForScale(1.5)};
  ASSERT_EQ(
      resizer_.GetSupportedResolutions(preferred_resolution, kMeta0ScreenId),
      std::list<ScreenResolution>{preferred_resolution});
}

TEST_F(
    GnomeDesktopResizerTest,
    SetResolution_EverythingMatchesExpectedValue_ApplyMonitorsConfigNotCalled) {
  // See constructor for the initial display config.
  resizer_.SetResolution({{150, 150}, GetDpiForScale(1.5)}, kMeta0ScreenId);
  ASSERT_EQ(GetTestResolutionForStream(kMeta0ScreenId),
            TestDesktopSize(150, 150));

  MonitorMap monitors = {{kMeta0, CreateMonitorInfo(0, 0, 150, 150, 1.5)},
                         {kMeta1, CreateMonitorInfo(100, 0, 150, 150, 2.0)}};
  display_config_.monitors = monitors;
  SimulateMonitorsChangedAndWaitForPossibleNewConfig();

  // Display config should remain unchanged.
  ASSERT_EQ(display_config_.monitors, monitors);
}

TEST_F(GnomeDesktopResizerTest,
       SetResolution_ScaleRevertedTo1_AppliesMonitorsConfig) {
  resizer_.SetResolution({{150, 150}, GetDpiForScale(1.5)}, kMeta0ScreenId);
  ASSERT_EQ(GetTestResolutionForStream(kMeta0ScreenId),
            TestDesktopSize(150, 150));

  display_config_.monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 150, 150, 1.0)},
      {kMeta1, CreateMonitorInfo(150, 0, 150, 150, 2.0)}};
  SimulateMonitorsChangedAndWaitForPossibleNewConfig();

  MonitorMap expected_monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 150, 150, 1.5)},
      {kMeta1, CreateMonitorInfo(100, 0, 150, 150, 2.0)}};
  ASSERT_EQ(display_config_.monitors, expected_monitors);
}

TEST_F(GnomeDesktopResizerTest, SetResolution_UseClosestSupportedScale) {
  resizer_.SetResolution({{150, 150}, GetDpiForScale(1.33)}, kMeta1ScreenId);
  ASSERT_EQ(GetTestResolutionForStream(kMeta1ScreenId),
            TestDesktopSize(150, 150));
  WaitForPossibleNewConfig();

  // The supported scale that is closest to 1.33 is 1.5.
  MonitorMap expected_monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 100, 100, 2.0)},
      {kMeta1, CreateMonitorInfo(50, 0, 150, 150, 1.5)}};
  ASSERT_EQ(display_config_.monitors, expected_monitors);
}

TEST_F(GnomeDesktopResizerTest,
       SetResolution_OnlyChangingScale_AppliesMonitorsConfigImmediately) {
  // 2.0 => 1.0
  resizer_.SetResolution({{100, 100}, GetDpiForScale(1.0)}, kMeta0ScreenId);
  ASSERT_EQ(GetTestResolutionForStream(kMeta0ScreenId),
            TestDesktopSize(100, 100));
  WaitForPossibleNewConfig();

  MonitorMap expected_monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 100, 100, 1.0)},
      {kMeta1, CreateMonitorInfo(100, 0, 150, 150, 2.0)}};
  ASSERT_EQ(display_config_.monitors, expected_monitors);
}

TEST_F(GnomeDesktopResizerTest, SetResolution_MaintainsPreferredLayout) {
  // Vertical end-aligned.
  display_config_.monitors = {
      {kMeta0, CreateMonitorInfo(25, 0, 100, 100, 2.0)},
      {kMeta1, CreateMonitorInfo(0, 50, 150, 150, 2.0)}};
  SimulateMonitorsChangedAndWaitForPossibleNewConfig();

  resizer_.SetResolution({{300, 300}, GetDpiForScale(1.5)}, kMeta0ScreenId);
  ASSERT_EQ(GetTestResolutionForStream(kMeta0ScreenId),
            TestDesktopSize(300, 300));

  // Simulate resolution changed but layout reverted to horizontal start-aligned
  // and scale reverted to 1.
  display_config_.monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 300, 300, 1.0)},
      {kMeta1, CreateMonitorInfo(300, 0, 150, 150, 2.0)}};
  SimulateMonitorsChangedAndWaitForPossibleNewConfig();

  // Verify that the resizer changes the layout back to vertical end-aligned
  // and the scale is updated correctly.
  MonitorMap expected_monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 300, 300, 1.5)},
      {kMeta1, CreateMonitorInfo(125, 200, 150, 150, 2.0)}};
  ASSERT_EQ(display_config_.monitors, expected_monitors);
}

TEST_F(GnomeDesktopResizerTest, SetVideoLayout_UpdatesExistingMonitors) {
  // Note: unlike GnomeDisplayConfig, width and height in VideoTrackLayout are
  // in logical pixels (DIPs) instead of physical screen pixels.
  protocol::VideoLayout layout;
  layout.set_pixel_type(
      protocol::VideoLayout::PixelType::VideoLayout_PixelType_LOGICAL);
  protocol::VideoTrackLayout* meta_0 = layout.add_video_track();
  meta_0->set_screen_id(kMeta0ScreenId);
  meta_0->set_position_x(0);
  meta_0->set_position_y(0);
  meta_0->set_width(300);
  meta_0->set_height(300);
  meta_0->set_x_dpi(GetDpiNumberForScale(1.5));
  meta_0->set_y_dpi(GetDpiNumberForScale(1.5));
  protocol::VideoTrackLayout* meta_1 = layout.add_video_track();
  meta_1->set_screen_id(kMeta1ScreenId);
  meta_1->set_position_x(100);
  meta_1->set_position_y(300);
  meta_1->set_width(200);
  meta_1->set_height(200);
  meta_1->set_x_dpi(GetDpiNumberForScale(2.0));
  meta_1->set_y_dpi(GetDpiNumberForScale(2.0));
  resizer_.SetVideoLayout(layout);
  ASSERT_EQ(GetTestResolutionForStream(kMeta0ScreenId),
            TestDesktopSize(450, 450));
  ASSERT_EQ(GetTestResolutionForStream(kMeta1ScreenId),
            TestDesktopSize(400, 400));

  // Simulate resolution changed while scales reverted to 1.
  display_config_.monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 450, 450, 1.0)},
      {kMeta1, CreateMonitorInfo(450, 0, 400, 400, 1.0)}};
  SimulateMonitorsChangedAndWaitForPossibleNewConfig();

  MonitorMap expected_monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 450, 450, 1.5)},
      {kMeta1, CreateMonitorInfo(100, 300, 400, 400, 2.0)}};
  ASSERT_EQ(display_config_.monitors, expected_monitors);
}

TEST_F(GnomeDesktopResizerTest, SetVideoLayout_SupportsPhysicalLayout) {
  // Vertical end-aligned.
  protocol::VideoLayout layout;
  layout.set_pixel_type(
      protocol::VideoLayout::PixelType::VideoLayout_PixelType_PHYSICAL);
  protocol::VideoTrackLayout* meta_0 = layout.add_video_track();
  meta_0->set_screen_id(kMeta0ScreenId);
  meta_0->set_position_x(0);
  meta_0->set_position_y(0);
  meta_0->set_width(450);
  meta_0->set_height(450);
  meta_0->set_x_dpi(GetDpiNumberForScale(1.5));
  meta_0->set_y_dpi(GetDpiNumberForScale(1.5));
  protocol::VideoTrackLayout* meta_1 = layout.add_video_track();
  meta_1->set_screen_id(kMeta1ScreenId);
  meta_1->set_position_x(50);
  meta_1->set_position_y(450);
  meta_1->set_width(400);
  meta_1->set_height(400);
  meta_1->set_x_dpi(GetDpiNumberForScale(2.0));
  meta_1->set_y_dpi(GetDpiNumberForScale(2.0));
  resizer_.SetVideoLayout(layout);
  ASSERT_EQ(GetTestResolutionForStream(kMeta0ScreenId),
            TestDesktopSize(450, 450));
  ASSERT_EQ(GetTestResolutionForStream(kMeta1ScreenId),
            TestDesktopSize(400, 400));

  // Simulate resolution changed while scales reverted to 1.
  display_config_.monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 450, 450, 1.0)},
      {kMeta1, CreateMonitorInfo(450, 0, 400, 400, 1.0)}};
  SimulateMonitorsChangedAndWaitForPossibleNewConfig();

  // The resizer will relayout in logical layout mode.
  MonitorMap expected_monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 450, 450, 1.5)},
      {kMeta1, CreateMonitorInfo(100, 300, 400, 400, 2.0)}};
  ASSERT_EQ(display_config_.monitors, expected_monitors);
  ASSERT_EQ(display_config_.layout_mode,
            GnomeDisplayConfig::LayoutMode::kLogical);
}

TEST_F(GnomeDesktopResizerTest,
       SetVideoLayout_AddsNewMonitorAndRestoresLayout) {
  constexpr char kMeta2[] = "Meta-2";
  static const webrtc::ScreenId kMeta2ScreenId =
      GnomeDisplayConfig::GetScreenId("Meta-2");
  stream_manager_.next_screen_id = kMeta2ScreenId;

  // Vertical end-aligned.
  display_config_.monitors = {
      {kMeta0, CreateMonitorInfo(25, 0, 100, 100, 2.0)},
      {kMeta1, CreateMonitorInfo(0, 50, 150, 150, 2.0)}};
  SimulateMonitorsChangedAndWaitForPossibleNewConfig();

  // Note: unlike GnomeDisplayConfig, width and height in VideoTrackLayout are
  // in logical pixels (DIPs) instead of physical screen pixels.
  protocol::VideoLayout layout;
  layout.set_pixel_type(
      protocol::VideoLayout::PixelType::VideoLayout_PixelType_LOGICAL);
  // Meta-0 and Meta-1 are unchanged.
  protocol::VideoTrackLayout* meta_0 = layout.add_video_track();
  meta_0->set_screen_id(kMeta0ScreenId);
  meta_0->set_position_x(25);
  meta_0->set_position_y(0);
  meta_0->set_width(50);
  meta_0->set_height(50);
  meta_0->set_x_dpi(GetDpiNumberForScale(2.0));
  meta_0->set_y_dpi(GetDpiNumberForScale(2.0));
  protocol::VideoTrackLayout* meta_1 = layout.add_video_track();
  meta_1->set_screen_id(kMeta1ScreenId);
  meta_1->set_position_x(0);
  meta_1->set_position_y(50);
  meta_1->set_width(75);
  meta_1->set_height(75);
  meta_1->set_x_dpi(GetDpiNumberForScale(2.0));
  meta_1->set_y_dpi(GetDpiNumberForScale(2.0));
  resizer_.SetVideoLayout(layout);
  // New monitor.
  protocol::VideoTrackLayout* meta_2 = layout.add_video_track();
  meta_2->set_position_x(25);
  meta_2->set_position_y(125);
  meta_2->set_width(50);
  meta_2->set_height(50);
  meta_2->set_x_dpi(GetDpiNumberForScale(2.0));
  meta_2->set_y_dpi(GetDpiNumberForScale(2.0));
  resizer_.SetVideoLayout(layout);
  ASSERT_EQ(GetTestResolutionForStream(kMeta0ScreenId),
            TestDesktopSize(100, 100));
  ASSERT_EQ(GetTestResolutionForStream(kMeta1ScreenId),
            TestDesktopSize(150, 150));
  ASSERT_EQ(GetTestResolutionForStream(kMeta2ScreenId),
            TestDesktopSize(100, 100));

  // Simulate that the new monitor is created with 1x scale, and layout reverted
  // to horizontal start-aligned.
  display_config_.monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 100, 100, 2.0)},
      {kMeta1, CreateMonitorInfo(50, 0, 150, 150, 2.0)},
      {kMeta2, CreateMonitorInfo(125, 0, 100, 100, 1.0)}};
  SimulateMonitorsChangedAndWaitForPossibleNewConfig();

  // Verify that the correct position and layout are applied.
  MonitorMap expected_monitors = {
      {kMeta0, CreateMonitorInfo(25, 0, 100, 100, 2.0)},
      {kMeta1, CreateMonitorInfo(0, 50, 150, 150, 2.0)},
      {kMeta2, CreateMonitorInfo(25, 125, 100, 100, 2.0)},
  };
  ASSERT_EQ(display_config_.monitors, expected_monitors);
}

TEST_F(GnomeDesktopResizerTest, SetVideoLayout_RemovesStreamThenResizes) {
  // Note: unlike GnomeDisplayConfig, width and height in VideoTrackLayout are
  // in logical pixels (DIPs) instead of physical screen pixels.
  protocol::VideoLayout layout;
  layout.set_pixel_type(
      protocol::VideoLayout::PixelType::VideoLayout_PixelType_LOGICAL);
  // Meta-0 is absent from the new layout.
  // Meta-1 is resized to 200x200(DIPs)@1.5x
  protocol::VideoTrackLayout* meta_1 = layout.add_video_track();
  meta_1->set_screen_id(kMeta1ScreenId);
  meta_1->set_position_x(0);
  meta_1->set_position_y(0);
  meta_1->set_width(200);
  meta_1->set_height(200);
  meta_1->set_x_dpi(GetDpiNumberForScale(1.5));
  meta_1->set_y_dpi(GetDpiNumberForScale(1.5));
  resizer_.SetVideoLayout(layout);

  ASSERT_TRUE(stream_manager_.GetStream(kMeta0ScreenId) == nullptr);
  // Resizes are not applied until the stream is removed. This is the initial
  // resolution set in the constructor.
  ASSERT_EQ(GetTestResolutionForStream(kMeta1ScreenId),
            TestDesktopSize(150, 150));

  // Simulate that Meta-0 is removed.
  display_config_.monitors = {{kMeta1, CreateMonitorInfo(0, 0, 150, 150, 2.0)}};
  SimulateMonitorsChangedAndWaitForPossibleNewConfig();

  // Now Meta-0 is being resized.
  ASSERT_EQ(GetTestResolutionForStream(kMeta1ScreenId),
            TestDesktopSize(300, 300));

  // Meta-0's scale being reverted to 1.
  display_config_.monitors = {{kMeta1, CreateMonitorInfo(0, 0, 300, 300, 1.0)}};
  SimulateMonitorsChangedAndWaitForPossibleNewConfig();

  MonitorMap expected_monitors = {
      {kMeta1, CreateMonitorInfo(0, 0, 300, 300, 1.5)}};
  ASSERT_EQ(display_config_.monitors, expected_monitors);
}

}  // namespace remoting
