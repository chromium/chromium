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
      base::BindRepeating(&GnomeDesktopResizerTest::ApplyMonitorsConfig,
                          base::Unretained(this))};
  GnomeDisplayConfig display_config_;

 private:
  void ApplyMonitorsConfig(const GnomeDisplayConfig& config);
};

GnomeDesktopResizerTest::GnomeDesktopResizerTest() {
  display_config_.layout_mode = GnomeDisplayConfig::LayoutMode::kLogical;
  display_config_.monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 800, 800, 2.0)},
      {kMeta1, CreateMonitorInfo(400, 0, 1200, 1200, 2.0)}};
  stream_manager_.AddVirtualStream(kMeta0ScreenId, {800, 800});
  stream_manager_.AddVirtualStream(kMeta1ScreenId, {1200, 1200});
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
            ScreenResolution({800, 800}, GetDpiForScale(2)));
  ASSERT_EQ(resizer_.GetCurrentResolution(kMeta1ScreenId),
            ScreenResolution({1200, 1200}, GetDpiForScale(2)));
  ASSERT_TRUE(
      resizer_.GetCurrentResolution(GnomeDisplayConfig::GetScreenId("Meta-2"))
          .IsEmpty());
}

TEST_F(GnomeDesktopResizerTest, GetSupportedResolutions_NoTweakingNeeded) {
  // Scale 1.5 (3/2). N=3.
  // Preferred: 900x900 (multiples of 3).
  // Logical: 600x600, area 360000 (supported).
  ScreenResolution preferred{{900, 900}, GetDpiForScale(1.5)};
  std::list<ScreenResolution> expected = {preferred};
  EXPECT_EQ(resizer_.GetSupportedResolutions(preferred, kMeta0ScreenId),
            expected);
}

TEST_F(GnomeDesktopResizerTest, GetSupportedResolutions_TweakingNeeded) {
  // Scale 1.5 (3/2). N=3.
  // Preferred: 902x901.
  // Tweaked: 900x900.
  // Logical: 600x600, area 360000 (supported).
  ScreenResolution preferred{{902, 901}, GetDpiForScale(1.5)};
  ScreenResolution expected{{900, 900}, GetDpiForScale(1.5)};
  std::list<ScreenResolution> expected_list = {expected};
  EXPECT_EQ(resizer_.GetSupportedResolutions(preferred, kMeta0ScreenId),
            expected_list);
}

TEST_F(GnomeDesktopResizerTest,
       GetSupportedResolutions_FallbackToSmallerScale) {
  // Scale 2.0 (2/1). N=2.
  // Preferred: 1000x1000.
  // 2.0x -> 1000x1000 -> logical 500x500 -> area 250000 (too small).
  // 1.75x (7/4) -> 994x994 -> logical 568x568 -> area 322624 (too small).
  // 1.666...x (5/3) -> 1000x1000 -> logical 600x600 -> area 360000
  // (supported!). Expected DPI for 5/3 is 160.
  ScreenResolution preferred{{1000, 1000}, GetDpiForScale(2.0)};
  ScreenResolution expected{{1000, 1000}, GetDpiForScale(5.0 / 3.0)};
  std::list<ScreenResolution> expected_list = {expected};
  EXPECT_EQ(resizer_.GetSupportedResolutions(preferred, kMeta0ScreenId),
            expected_list);
}

TEST_F(GnomeDesktopResizerTest, GetSupportedResolutions_FallbackToOne) {
  // Scale 1.5.
  // Preferred: 700x700.
  // All scales > 1.0 will result in area < 360000.
  // Should fallback to 1.0x (DPI 96).
  ScreenResolution preferred{{700, 700}, GetDpiForScale(1.5)};
  ScreenResolution expected{{700, 700}, GetDpiForScale(1.0)};
  std::list<ScreenResolution> expected_list = {expected};
  EXPECT_EQ(resizer_.GetSupportedResolutions(preferred, kMeta0ScreenId),
            expected_list);
}

TEST_F(GnomeDesktopResizerTest, GetSupportedResolutions_InvalidDimensions) {
  // Zero dimensions should be returned unchanged.
  ScreenResolution preferred_zero{{0, 0}, GetDpiForScale(1.5)};
  std::list<ScreenResolution> expected_zero = {preferred_zero};
  EXPECT_EQ(resizer_.GetSupportedResolutions(preferred_zero, kMeta0ScreenId),
            expected_zero);
}

TEST_F(
    GnomeDesktopResizerTest,
    SetResolution_EverythingMatchesExpectedValue_ApplyMonitorsConfigNotCalled) {
  // See constructor for the initial display config.
  resizer_.SetResolution({{1200, 1200}, GetDpiForScale(1.5)}, kMeta0ScreenId);
  ASSERT_EQ(GetTestResolutionForStream(kMeta0ScreenId),
            TestDesktopSize(1200, 1200));

  MonitorMap monitors = {{kMeta0, CreateMonitorInfo(0, 0, 1200, 1200, 1.5)},
                         {kMeta1, CreateMonitorInfo(800, 0, 1200, 1200, 2.0)}};
  display_config_.monitors = monitors;
  SimulateMonitorsChangedAndWaitForPossibleNewConfig();

  // Display config should remain unchanged.
  ASSERT_EQ(display_config_.monitors, monitors);
}

TEST_F(GnomeDesktopResizerTest,
       SetResolution_ScaleRevertedTo1_AppliesMonitorsConfig) {
  resizer_.SetResolution({{1200, 1200}, GetDpiForScale(1.5)}, kMeta0ScreenId);
  ASSERT_EQ(GetTestResolutionForStream(kMeta0ScreenId),
            TestDesktopSize(1200, 1200));

  display_config_.monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 1200, 1200, 1.0)},
      {kMeta1, CreateMonitorInfo(1200, 0, 1200, 1200, 2.0)}};
  SimulateMonitorsChangedAndWaitForPossibleNewConfig();

  MonitorMap expected_monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 1200, 1200, 1.5)},
      {kMeta1, CreateMonitorInfo(800, 0, 1200, 1200, 2.0)}};
  ASSERT_EQ(display_config_.monitors, expected_monitors);
}

TEST_F(GnomeDesktopResizerTest, SetResolution_UseClosestSupportedScale) {
  resizer_.SetResolution({{1200, 1200}, GetDpiForScale(1.33)}, kMeta1ScreenId);
  ASSERT_EQ(GetTestResolutionForStream(kMeta1ScreenId),
            TestDesktopSize(1200, 1200));
  WaitForPossibleNewConfig();

  // The supported scale that is closest to 1.33 is 1.5.
  MonitorMap expected_monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 800, 800, 2.0)},
      {kMeta1, CreateMonitorInfo(400, 0, 1200, 1200, 1.5)}};
  ASSERT_EQ(display_config_.monitors, expected_monitors);
}

TEST_F(GnomeDesktopResizerTest,
       SetResolution_OnlyChangingScale_AppliesMonitorsConfigImmediately) {
  // 2.0 => 1.0
  resizer_.SetResolution({{800, 800}, GetDpiForScale(1.0)}, kMeta0ScreenId);
  ASSERT_EQ(GetTestResolutionForStream(kMeta0ScreenId),
            TestDesktopSize(800, 800));
  WaitForPossibleNewConfig();

  MonitorMap expected_monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 800, 800, 1.0)},
      {kMeta1, CreateMonitorInfo(800, 0, 1200, 1200, 2.0)}};
  ASSERT_EQ(display_config_.monitors, expected_monitors);
}

TEST_F(GnomeDesktopResizerTest, SetResolution_MaintainsPreferredLayout) {
  // Vertical end-aligned.
  display_config_.monitors = {
      {kMeta0, CreateMonitorInfo(200, 0, 800, 800, 2.0)},
      {kMeta1, CreateMonitorInfo(0, 400, 1200, 1200, 2.0)}};
  SimulateMonitorsChangedAndWaitForPossibleNewConfig();

  resizer_.SetResolution({{2400, 2400}, GetDpiForScale(1.5)}, kMeta0ScreenId);
  ASSERT_EQ(GetTestResolutionForStream(kMeta0ScreenId),
            TestDesktopSize(2400, 2400));

  // Simulate resolution changed but layout reverted to horizontal start-aligned
  // and scale reverted to 1.
  display_config_.monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 2400, 2400, 1.0)},
      {kMeta1, CreateMonitorInfo(2400, 0, 1200, 1200, 2.0)}};
  SimulateMonitorsChangedAndWaitForPossibleNewConfig();

  // Verify that the resizer changes the layout back to vertical end-aligned
  // and the scale is updated correctly.
  MonitorMap expected_monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 2400, 2400, 1.5)},
      {kMeta1, CreateMonitorInfo(1000, 1600, 1200, 1200, 2.0)}};
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
  meta_0->set_width(900);
  meta_0->set_height(900);
  meta_0->set_x_dpi(GetDpiNumberForScale(1.5));
  meta_0->set_y_dpi(GetDpiNumberForScale(1.5));
  protocol::VideoTrackLayout* meta_1 = layout.add_video_track();
  meta_1->set_screen_id(kMeta1ScreenId);
  meta_1->set_position_x(300);
  meta_1->set_position_y(900);
  meta_1->set_width(600);
  meta_1->set_height(600);
  meta_1->set_x_dpi(GetDpiNumberForScale(2.0));
  meta_1->set_y_dpi(GetDpiNumberForScale(2.0));
  resizer_.SetVideoLayout(layout);
  ASSERT_EQ(GetTestResolutionForStream(kMeta0ScreenId),
            TestDesktopSize(1350, 1350));
  ASSERT_EQ(GetTestResolutionForStream(kMeta1ScreenId),
            TestDesktopSize(1200, 1200));

  // Simulate resolution changed while scales reverted to 1.
  display_config_.monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 1350, 1350, 1.0)},
      {kMeta1, CreateMonitorInfo(1350, 0, 1200, 1200, 1.0)}};
  SimulateMonitorsChangedAndWaitForPossibleNewConfig();

  MonitorMap expected_monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 1350, 1350, 1.5)},
      {kMeta1, CreateMonitorInfo(300, 900, 1200, 1200, 2.0)}};
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
  meta_0->set_width(1350);
  meta_0->set_height(1350);
  meta_0->set_x_dpi(GetDpiNumberForScale(1.5));
  meta_0->set_y_dpi(GetDpiNumberForScale(1.5));
  protocol::VideoTrackLayout* meta_1 = layout.add_video_track();
  meta_1->set_screen_id(kMeta1ScreenId);
  meta_1->set_position_x(150);
  meta_1->set_position_y(1350);
  meta_1->set_width(1200);
  meta_1->set_height(1200);
  meta_1->set_x_dpi(GetDpiNumberForScale(2.0));
  meta_1->set_y_dpi(GetDpiNumberForScale(2.0));
  resizer_.SetVideoLayout(layout);
  ASSERT_EQ(GetTestResolutionForStream(kMeta0ScreenId),
            TestDesktopSize(1350, 1350));
  ASSERT_EQ(GetTestResolutionForStream(kMeta1ScreenId),
            TestDesktopSize(1200, 1200));

  // Simulate resolution changed while scales reverted to 1.
  display_config_.monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 1350, 1350, 1.0)},
      {kMeta1, CreateMonitorInfo(1350, 0, 1200, 1200, 1.0)}};
  SimulateMonitorsChangedAndWaitForPossibleNewConfig();

  // The resizer will relayout in logical layout mode.
  MonitorMap expected_monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 1350, 1350, 1.5)},
      {kMeta1, CreateMonitorInfo(300, 900, 1200, 1200, 2.0)}};
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
      {kMeta0, CreateMonitorInfo(300, 0, 1200, 1200, 2.0)},
      {kMeta1, CreateMonitorInfo(0, 600, 1800, 1800, 2.0)}};
  SimulateMonitorsChangedAndWaitForPossibleNewConfig();

  // Note: unlike GnomeDisplayConfig, width and height in VideoTrackLayout are
  // in logical pixels (DIPs) instead of physical screen pixels.
  protocol::VideoLayout layout;
  layout.set_pixel_type(
      protocol::VideoLayout::PixelType::VideoLayout_PixelType_LOGICAL);
  // Meta-0 and Meta-1 are unchanged.
  protocol::VideoTrackLayout* meta_0 = layout.add_video_track();
  meta_0->set_screen_id(kMeta0ScreenId);
  meta_0->set_position_x(300);
  meta_0->set_position_y(0);
  meta_0->set_width(600);
  meta_0->set_height(600);
  meta_0->set_x_dpi(GetDpiNumberForScale(2.0));
  meta_0->set_y_dpi(GetDpiNumberForScale(2.0));
  protocol::VideoTrackLayout* meta_1 = layout.add_video_track();
  meta_1->set_screen_id(kMeta1ScreenId);
  meta_1->set_position_x(0);
  meta_1->set_position_y(600);
  meta_1->set_width(900);
  meta_1->set_height(900);
  meta_1->set_x_dpi(GetDpiNumberForScale(2.0));
  meta_1->set_y_dpi(GetDpiNumberForScale(2.0));
  resizer_.SetVideoLayout(layout);
  // New monitor.
  protocol::VideoTrackLayout* meta_2 = layout.add_video_track();
  meta_2->set_position_x(300);
  meta_2->set_position_y(1500);
  meta_2->set_width(600);
  meta_2->set_height(600);
  meta_2->set_x_dpi(GetDpiNumberForScale(2.0));
  meta_2->set_y_dpi(GetDpiNumberForScale(2.0));
  resizer_.SetVideoLayout(layout);
  ASSERT_EQ(GetTestResolutionForStream(kMeta0ScreenId),
            TestDesktopSize(1200, 1200));
  ASSERT_EQ(GetTestResolutionForStream(kMeta1ScreenId),
            TestDesktopSize(1800, 1800));
  ASSERT_EQ(GetTestResolutionForStream(kMeta2ScreenId),
            TestDesktopSize(1200, 1200));

  // Simulate that the new monitor is created with 1x scale, and layout reverted
  // to horizontal start-aligned.
  display_config_.monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 1200, 1200, 2.0)},
      {kMeta1, CreateMonitorInfo(600, 0, 1800, 1800, 2.0)},
      {kMeta2, CreateMonitorInfo(1500, 0, 1200, 1200, 1.0)}};
  SimulateMonitorsChangedAndWaitForPossibleNewConfig();

  // Verify that the correct position and layout are applied.
  MonitorMap expected_monitors = {
      {kMeta0, CreateMonitorInfo(300, 0, 1200, 1200, 2.0)},
      {kMeta1, CreateMonitorInfo(0, 600, 1800, 1800, 2.0)},
      {kMeta2, CreateMonitorInfo(300, 1500, 1200, 1200, 2.0)},
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
  // Meta-1 is resized to 600x600(DIPs)@1.5x
  protocol::VideoTrackLayout* meta_1 = layout.add_video_track();
  meta_1->set_screen_id(kMeta1ScreenId);
  meta_1->set_position_x(0);
  meta_1->set_position_y(0);
  meta_1->set_width(600);
  meta_1->set_height(600);
  meta_1->set_x_dpi(GetDpiNumberForScale(1.5));
  meta_1->set_y_dpi(GetDpiNumberForScale(1.5));
  resizer_.SetVideoLayout(layout);

  ASSERT_TRUE(stream_manager_.GetStream(kMeta0ScreenId) == nullptr);
  // Resizes are not applied until the stream is removed. This is the initial
  // resolution set in the constructor.
  ASSERT_EQ(GetTestResolutionForStream(kMeta1ScreenId),
            TestDesktopSize(1200, 1200));

  // Simulate that Meta-0 is removed.
  display_config_.monitors = {{kMeta1, CreateMonitorInfo(0, 0, 1200, 1200, 2.0)}};
  SimulateMonitorsChangedAndWaitForPossibleNewConfig();

  // Now Meta-0 is being resized.
  ASSERT_EQ(GetTestResolutionForStream(kMeta1ScreenId),
            TestDesktopSize(900, 900));

  // Meta-0's scale being reverted to 1.
  display_config_.monitors = {{kMeta1, CreateMonitorInfo(0, 0, 900, 900, 1.0)}};
  SimulateMonitorsChangedAndWaitForPossibleNewConfig();

  MonitorMap expected_monitors = {
      {kMeta1, CreateMonitorInfo(0, 0, 900, 900, 1.5)}};
  ASSERT_EQ(display_config_.monitors, expected_monitors);
}

TEST_F(GnomeDesktopResizerTest, SetVideoLayout_ReusesRemovedMonitors) {
  // Note: unlike GnomeDisplayConfig, width and height in VideoTrackLayout are
  // in logical pixels (DIPs) instead of physical screen pixels.
  protocol::VideoLayout layout;
  layout.set_pixel_type(
      protocol::VideoLayout::PixelType::VideoLayout_PixelType_LOGICAL);
  protocol::VideoTrackLayout* meta_0 = layout.add_video_track();
  meta_0->set_screen_id(kMeta0ScreenId);
  meta_0->set_position_x(0);
  meta_0->set_position_y(0);
  meta_0->set_width(900);
  meta_0->set_height(900);
  meta_0->set_x_dpi(GetDpiNumberForScale(1.5));
  meta_0->set_y_dpi(GetDpiNumberForScale(1.5));
  protocol::VideoTrackLayout* meta_1 = layout.add_video_track();
  // No screen ID is set for Meta-1.
  meta_1->set_position_x(300);
  meta_1->set_position_y(900);
  meta_1->set_width(600);
  meta_1->set_height(600);
  meta_1->set_x_dpi(GetDpiNumberForScale(2.0));
  meta_1->set_y_dpi(GetDpiNumberForScale(2.0));
  resizer_.SetVideoLayout(layout);
  ASSERT_EQ(GetTestResolutionForStream(kMeta0ScreenId),
            TestDesktopSize(1350, 1350));
  ASSERT_EQ(GetTestResolutionForStream(kMeta1ScreenId),
            TestDesktopSize(1200, 1200));

  // Simulate resolution changed while scales reverted to 1.
  display_config_.monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 1350, 1350, 1.0)},
      {kMeta1, CreateMonitorInfo(1350, 0, 1200, 1200, 1.0)}};
  SimulateMonitorsChangedAndWaitForPossibleNewConfig();

  MonitorMap expected_monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 1350, 1350, 1.5)},
      {kMeta1, CreateMonitorInfo(300, 900, 1200, 1200, 2.0)}};
  ASSERT_EQ(display_config_.monitors, expected_monitors);
}

TEST_F(GnomeDesktopResizerTest, BlockAndQueueDisplayChanges_SetResolution) {
  resizer_.BlockAndQueueDisplayChanges();
  resizer_.SetResolution({{1200, 1200}, GetDpiForScale(1.5)}, kMeta0ScreenId);

  // New resolution is not immediately applied.
  ASSERT_NE(GetTestResolutionForStream(kMeta0ScreenId),
            TestDesktopSize(1200, 1200));

  resizer_.UnblockAndFlushDisplayChanges();

  // New resolution is applied now.
  ASSERT_EQ(GetTestResolutionForStream(kMeta0ScreenId),
            TestDesktopSize(1200, 1200));

  // Monitor DPI changes are covered by tests above.
}

TEST_F(GnomeDesktopResizerTest, BlockAndQueueDisplayChanges_SetVideoLayout) {
  resizer_.BlockAndQueueDisplayChanges();
  // Note: unlike GnomeDisplayConfig, width and height in VideoTrackLayout are
  // in logical pixels (DIPs) instead of physical screen pixels.
  protocol::VideoLayout layout;
  layout.set_pixel_type(
      protocol::VideoLayout::PixelType::VideoLayout_PixelType_LOGICAL);
  protocol::VideoTrackLayout* meta_0 = layout.add_video_track();
  meta_0->set_screen_id(kMeta0ScreenId);
  meta_0->set_position_x(0);
  meta_0->set_position_y(0);
  meta_0->set_width(800);
  meta_0->set_height(800);
  meta_0->set_x_dpi(GetDpiNumberForScale(1.5));
  meta_0->set_y_dpi(GetDpiNumberForScale(1.5));
  protocol::VideoTrackLayout* meta_1 = layout.add_video_track();
  meta_1->set_screen_id(kMeta1ScreenId);
  meta_1->set_position_x(400);
  meta_1->set_position_y(800);
  meta_1->set_width(500);
  meta_1->set_height(500);
  meta_1->set_x_dpi(GetDpiNumberForScale(2.0));
  meta_1->set_y_dpi(GetDpiNumberForScale(2.0));
  resizer_.SetVideoLayout(layout);

  // New resolutions are not immediately applied.
  ASSERT_NE(GetTestResolutionForStream(kMeta0ScreenId),
            TestDesktopSize(1200, 1200));
  ASSERT_NE(GetTestResolutionForStream(kMeta1ScreenId),
            TestDesktopSize(1000, 1000));

  resizer_.UnblockAndFlushDisplayChanges();

  // New resolutions are applied now.
  ASSERT_EQ(GetTestResolutionForStream(kMeta0ScreenId),
            TestDesktopSize(1200, 1200));
  ASSERT_EQ(GetTestResolutionForStream(kMeta1ScreenId),
            TestDesktopSize(1000, 1000));

  // Monitor DPI changes are covered by tests above.
}

TEST_F(GnomeDesktopResizerTest, SetVideoLayout_TweaksResolutions) {
  protocol::VideoLayout layout;
  layout.set_pixel_type(
      protocol::VideoLayout::PixelType::VideoLayout_PixelType_PHYSICAL);
  protocol::VideoTrackLayout* meta_0 = layout.add_video_track();
  meta_0->set_screen_id(kMeta0ScreenId);
  meta_0->set_position_x(0);
  meta_0->set_position_y(0);
  // Requested dimensions 902x901 at scale 1.5 (numerator N=3).
  meta_0->set_width(902);
  meta_0->set_height(901);
  meta_0->set_x_dpi(GetDpiNumberForScale(1.5));
  meta_0->set_y_dpi(GetDpiNumberForScale(1.5));
  protocol::VideoTrackLayout* meta_1 = layout.add_video_track();
  meta_1->set_screen_id(kMeta1ScreenId);
  meta_1->set_position_x(900);
  meta_1->set_position_y(0);
  meta_1->set_width(1200);
  meta_1->set_height(1200);
  meta_1->set_x_dpi(GetDpiNumberForScale(2.0));
  meta_1->set_y_dpi(GetDpiNumberForScale(2.0));
  resizer_.SetVideoLayout(layout);

  // SetVideoLayout should pass requested resolutions through
  // GetSupportedResolutions(), rounding dimensions down to multiples of 3
  // (900x900).
  ASSERT_EQ(GetTestResolutionForStream(kMeta0ScreenId),
            TestDesktopSize(900, 900));
  ASSERT_EQ(GetTestResolutionForStream(kMeta1ScreenId),
            TestDesktopSize(1200, 1200));
}

TEST_F(GnomeDesktopResizerTest, SetResolution_MinimumSizeClamped) {
  resizer_.SetResolution({{150, 150}, GetDpiForScale(1.0)}, kMeta0ScreenId);
  EXPECT_EQ(GetTestResolutionForStream(kMeta0ScreenId),
            TestDesktopSize(640, 480));
}

TEST_F(GnomeDesktopResizerTest, SetVideoLayout_MinimumSizeClampedAndLaidOut) {
  // Left: width = 600 (should be clamped to 640).
  // Right: offset_x = 600 (should be shifted to 640).
  protocol::VideoLayout layout;
  layout.set_pixel_type(
      protocol::VideoLayout::PixelType::VideoLayout_PixelType_LOGICAL);

  protocol::VideoTrackLayout* meta_0 = layout.add_video_track();
  meta_0->set_screen_id(kMeta0ScreenId);
  meta_0->set_position_x(0);
  meta_0->set_position_y(0);
  meta_0->set_width(600);
  meta_0->set_height(600);
  meta_0->set_x_dpi(GetDpiNumberForScale(1.0));
  meta_0->set_y_dpi(GetDpiNumberForScale(1.0));

  protocol::VideoTrackLayout* meta_1 = layout.add_video_track();
  meta_1->set_screen_id(kMeta1ScreenId);
  meta_1->set_position_x(600);
  meta_1->set_position_y(0);
  meta_1->set_width(800);
  meta_1->set_height(800);
  meta_1->set_x_dpi(GetDpiNumberForScale(1.0));
  meta_1->set_y_dpi(GetDpiNumberForScale(1.0));

  resizer_.SetVideoLayout(layout);

  // Monitor 0 height is 600 (already >= 480). Width is clamped to 640.
  EXPECT_EQ(GetTestResolutionForStream(kMeta0ScreenId),
            TestDesktopSize(640, 600));

  // Monitor 1 remains 800x800.
  EXPECT_EQ(GetTestResolutionForStream(kMeta1ScreenId),
            TestDesktopSize(800, 800));

  // Apply monitor structure change notifications:
  display_config_.monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 640, 600, 1.0)},
      {kMeta1, CreateMonitorInfo(600, 0, 800, 800, 1.0)}};
  SimulateMonitorsChangedAndWaitForPossibleNewConfig();

  // Offset of Monitor 1 should be shifted from 600 to 640 to prevent overlaps.
  MonitorMap expected_monitors = {
      {kMeta0, CreateMonitorInfo(0, 0, 640, 600, 1.0)},
      {kMeta1, CreateMonitorInfo(640, 0, 800, 800, 1.0)}};
  EXPECT_EQ(display_config_.monitors, expected_monitors);
}

}  // namespace remoting
