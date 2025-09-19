// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_display_config.h"

#include <string_view>
#include <vector>

#include "base/no_destructor.h"
#include "remoting/host/linux/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace remoting {

TEST(GnomeDisplayConfigTest, GetScreenId) {
  ASSERT_EQ(GnomeDisplayConfig::GetScreenId("Meta-0"), 0);
  ASSERT_EQ(GnomeDisplayConfig::GetScreenId("Meta-123"), 123);

  ASSERT_EQ(GnomeDisplayConfig::GetScreenId("Meta-123"),
            GnomeDisplayConfig::GetScreenId("Meta-123"));
  ASSERT_NE(GnomeDisplayConfig::GetScreenId("Meta-123"),
            GnomeDisplayConfig::GetScreenId("SomeMonitor"));
  ASSERT_EQ(GnomeDisplayConfig::GetScreenId("SomeMonitor"),
            GnomeDisplayConfig::GetScreenId("SomeMonitor"));
  ASSERT_NE(GnomeDisplayConfig::GetScreenId("SomeMonitor"),
            GnomeDisplayConfig::GetScreenId("SomeOtherMonitor"));
}

TEST(GnomeDisplayConfigTest, FindMonitor) {
  GnomeDisplayConfig config;
  config.monitors["Meta-0"] = GnomeDisplayConfig::MonitorInfo();
  config.monitors["Other-1"] = GnomeDisplayConfig::MonitorInfo();

  auto it = config.FindMonitor(0);
  ASSERT_NE(it, config.monitors.end());
  EXPECT_EQ(it->first, "Meta-0");

  webrtc::ScreenId other_1_id = GnomeDisplayConfig::GetScreenId("Other-1");
  it = config.FindMonitor(other_1_id);
  ASSERT_NE(it, config.monitors.end());
  EXPECT_EQ(it->first, "Other-1");

  // Try to find a monitor that doesn't exist.
  it = config.FindMonitor(12345);
  EXPECT_EQ(it, config.monitors.end());
}

TEST(GnomeDisplayConfigTest, RemoveInvalidMonitors) {
  GnomeDisplayConfig config;
  config.monitors["valid-monitor"] = CreateMonitorInfo(0, 0, 100, 100, 1.0);
  config.monitors["invalid-monitor"] = GnomeDisplayConfig::MonitorInfo();

  config.RemoveInvalidMonitors();

  EXPECT_EQ(config.monitors.size(), 1u);
  EXPECT_TRUE(config.monitors.contains("valid-monitor"));
  EXPECT_FALSE(config.monitors.contains("invalid-monitor"));
}

// Layout tests:

namespace {

struct LayoutInfoTestCase {
  const std::map<std::string, GnomeDisplayConfig::MonitorInfo>&
  GetMonitorsForLayoutMode(GnomeDisplayConfig::LayoutMode layout_mode) const {
    return layout_mode == GnomeDisplayConfig::LayoutMode::kPhysical
               ? physical_layout_monitors
               : logical_layout_monitors;
  }

  std::string_view name;
  std::map<std::string, GnomeDisplayConfig::MonitorInfo>
      physical_layout_monitors;
  std::map<std::string, GnomeDisplayConfig::MonitorInfo>
      logical_layout_monitors;
  GnomeDisplayConfig::LayoutDirection direction;
  GnomeDisplayConfig::LayoutAlignment alignment;
};

const std::vector<LayoutInfoTestCase>& GetLayoutInfoTestCases() {
  static const base::NoDestructor<std::vector<LayoutInfoTestCase>> test_cases({
      {.name = "horizontal_start_aligned",
       .physical_layout_monitors =
           {{"Meta-0", CreateMonitorInfo(0, 0, 100, 100, 2.0)},
            {"Meta-1", CreateMonitorInfo(100, 0, 100, 100, 2.0)}},
       .logical_layout_monitors =
           {{"Meta-0", CreateMonitorInfo(0, 0, 100, 100, 2.0)},
            {"Meta-1", CreateMonitorInfo(50, 0, 100, 100, 2.0)}},
       .direction = GnomeDisplayConfig::LayoutDirection::kHorizontal,
       .alignment = GnomeDisplayConfig::LayoutAlignment::kStart},

      {.name = "horizontal_center_aligned",
       .physical_layout_monitors =
           {{"Meta-0", CreateMonitorInfo(0, 0, 100, 200, 2.0)},
            {"Meta-1", CreateMonitorInfo(100, 50, 100, 100, 2.0)}},
       .logical_layout_monitors =
           {{"Meta-0", CreateMonitorInfo(0, 0, 100, 200, 2.0)},
            {"Meta-1", CreateMonitorInfo(50, 25, 100, 100, 2.0)}},
       .direction = GnomeDisplayConfig::LayoutDirection::kHorizontal,
       .alignment = GnomeDisplayConfig::LayoutAlignment::kCenter},

      {.name = "horizontal_end_aligned",
       .physical_layout_monitors =
           {{"Meta-0", CreateMonitorInfo(0, 0, 100, 100, 2.0)},
            {"Meta-1", CreateMonitorInfo(100, 50, 100, 50, 2.0)}},
       .logical_layout_monitors =
           {{"Meta-0", CreateMonitorInfo(0, 0, 100, 100, 2.0)},
            {"Meta-1", CreateMonitorInfo(50, 25, 100, 50, 2.0)}},
       .direction = GnomeDisplayConfig::LayoutDirection::kHorizontal,
       .alignment = GnomeDisplayConfig::LayoutAlignment::kEnd},

      {.name = "vertical_start_aligned",
       .physical_layout_monitors =
           {{"Meta-0", CreateMonitorInfo(0, 0, 100, 100, 2.0)},
            {"Meta-1", CreateMonitorInfo(0, 100, 100, 100, 2.0)}},
       .logical_layout_monitors =
           {{"Meta-0", CreateMonitorInfo(0, 0, 100, 100, 2.0)},
            {"Meta-1", CreateMonitorInfo(0, 50, 100, 100, 2.0)}},
       .direction = GnomeDisplayConfig::LayoutDirection::kVertical,
       .alignment = GnomeDisplayConfig::LayoutAlignment::kStart},

      {.name = "vertical_center_aligned",
       .physical_layout_monitors =
           {{"Meta-0", CreateMonitorInfo(0, 0, 200, 100, 2.0)},
            {"Meta-1", CreateMonitorInfo(50, 100, 100, 100, 2.0)}},
       .logical_layout_monitors =
           {{"Meta-0", CreateMonitorInfo(0, 0, 200, 100, 2.0)},
            {"Meta-1", CreateMonitorInfo(25, 50, 100, 100, 2.0)}},
       .direction = GnomeDisplayConfig::LayoutDirection::kVertical,
       .alignment = GnomeDisplayConfig::LayoutAlignment::kCenter},

      {.name = "vertical_end_aligned",
       .physical_layout_monitors =
           {{"Meta-0", CreateMonitorInfo(0, 0, 100, 100, 2.0)},
            {"Meta-1", CreateMonitorInfo(50, 100, 50, 100, 2.0)}},
       .logical_layout_monitors =
           {{"Meta-0", CreateMonitorInfo(0, 0, 100, 100, 2.0)},
            {"Meta-1", CreateMonitorInfo(25, 50, 50, 100, 2.0)}},
       .direction = GnomeDisplayConfig::LayoutDirection::kVertical,
       .alignment = GnomeDisplayConfig::LayoutAlignment::kEnd},
  });
  return *test_cases;
}

}  // namespace

class GnomeDisplayConfigTest
    : public ::testing::TestWithParam<
          std::tuple<GnomeDisplayConfig::LayoutMode, LayoutInfoTestCase>> {};

TEST_P(GnomeDisplayConfigTest, GetLayoutInfo) {
  const auto& [layout_mode, test_case] = GetParam();
  const auto& monitors = test_case.GetMonitorsForLayoutMode(layout_mode);

  GnomeDisplayConfig config;
  config.layout_mode = layout_mode;
  config.monitors = monitors;

  auto actual_layout_info = config.GetLayoutInfo();

  EXPECT_EQ(actual_layout_info.layout_mode, layout_mode);
  EXPECT_EQ(actual_layout_info.direction, test_case.direction);
  EXPECT_EQ(actual_layout_info.alignment, test_case.alignment);
}

TEST_P(GnomeDisplayConfigTest, SwitchLayoutMode) {
  const auto& [to_layout_mode, test_case] = GetParam();
  GnomeDisplayConfig::LayoutMode from_layout_mode =
      to_layout_mode == GnomeDisplayConfig::LayoutMode::kPhysical
          ? GnomeDisplayConfig::LayoutMode::kLogical
          : GnomeDisplayConfig::LayoutMode::kPhysical;
  const auto& from_monitors =
      test_case.GetMonitorsForLayoutMode(from_layout_mode);
  const auto& to_monitors = test_case.GetMonitorsForLayoutMode(to_layout_mode);

  GnomeDisplayConfig config;
  config.layout_mode = from_layout_mode;
  config.monitors = from_monitors;

  config.SwitchLayoutMode(to_layout_mode);
  auto new_layout_info = config.GetLayoutInfo();

  EXPECT_EQ(config.monitors, to_monitors);
  EXPECT_EQ(new_layout_info.layout_mode, to_layout_mode);
  EXPECT_EQ(new_layout_info.direction, test_case.direction);
  EXPECT_EQ(new_layout_info.alignment, test_case.alignment);
}

TEST_P(GnomeDisplayConfigTest, Relayout_ClosesGaps) {
  const auto& [layout_mode, test_case] = GetParam();
  const auto& monitors = test_case.GetMonitorsForLayoutMode(layout_mode);

  GnomeDisplayConfig config;
  config.layout_mode = layout_mode;
  config.monitors = monitors;
  auto layout_info = config.GetLayoutInfo();

  config.monitors["Meta-1"].x += 10;
  config.monitors["Meta-1"].y += 10;
  config.Relayout(layout_info);

  EXPECT_EQ(config.monitors, monitors);
}

TEST_P(GnomeDisplayConfigTest, Relayout_RemovesOverlaps) {
  const auto& [layout_mode, test_case] = GetParam();
  const auto& monitors = test_case.GetMonitorsForLayoutMode(layout_mode);

  GnomeDisplayConfig config;
  config.layout_mode = layout_mode;
  config.monitors = monitors;
  auto layout_info = config.GetLayoutInfo();

  config.monitors["Meta-1"].x -= 10;
  config.monitors["Meta-1"].y -= 10;
  config.Relayout(layout_info);

  EXPECT_EQ(config.monitors, monitors);
}

TEST_P(GnomeDisplayConfigTest, Relayout_RestoresLayout) {
  const auto& [layout_mode, test_case] = GetParam();
  const auto& monitors = test_case.GetMonitorsForLayoutMode(layout_mode);

  GnomeDisplayConfig config;
  config.layout_mode = layout_mode;
  config.monitors = monitors;
  auto layout_info = config.GetLayoutInfo();

  config.monitors["Meta-1"].x = 0;
  config.monitors["Meta-1"].y = 0;
  config.Relayout(layout_info);

  EXPECT_EQ(config.monitors, monitors);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GnomeDisplayConfigTest,
    testing::Combine(testing::Values(GnomeDisplayConfig::LayoutMode::kLogical,
                                     GnomeDisplayConfig::LayoutMode::kPhysical),
                     testing::ValuesIn(GetLayoutInfoTestCases())),
    [](const auto& info) {
      const auto& layout_mode = std::get<0>(info.param);
      const auto& test_case = std::get<1>(info.param);
      return std::string(test_case.name) +
             (layout_mode == GnomeDisplayConfig::LayoutMode::kPhysical
                  ? "_physical"
                  : "_logical");
    });

}  // namespace remoting
