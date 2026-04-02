// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_display_config_dbus_client.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "remoting/host/linux/scoped_glib.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/portal/scoped_glib.h"

namespace remoting {

namespace {

// The return type of the D-Bus method
// org.gnome.Mutter.DisplayConfig.GetCurrentState().
constexpr char kDisplayConfigType[] =
    "(u"                             // serial
    "a((ssss)a(siiddada{sv})a{sv})"  // monitors
    "a(iiduba(ssss)a{sv})"           // logical_monitors
    "a{sv})";                        // properties

// These strings are based on calling g_variant_print() on GVariants returned
// by the D-Bus API. The format is accepted by g_variant_parse(), as long as
// the correct type is provided.
constexpr char kSingleDisplayConfig[] =
    "(123, [(('DUMMY0', 'unknown', 'unknown', 'unknown'), [('1024x768@60', "
    "1024, 768, 60.0, 1.0, [1.0], {'is-current': <true>}), ('800x600@60', 800, "
    "600, 60.0, 1.0, [1.0], {})], {})], [(0, 0, 1.0, 0, true, [('DUMMY0', "
    "'unknown', 'unknown', 'unknown')], {})], {'global-scale-required': "
    "<true>, 'layout-mode': <uint32 2>})";
constexpr char kDualDisplayConfig[] =
    "(124, [(('DUMMY0', 'unknown', 'unknown', 'unknown'), [('800x600@60', 800, "
    "600, 60.0, 1.0, [1.0], {}), ('1600x1200@60', 1600, 1200, 60.0, 1.0, [1.0, "
    "2.0], {'is-current': <true>})], {}), (('DUMMY1', 'unknown', 'unknown', "
    "'unknown'), [('800x600@60', 800, 600, 60.0, 1.0, [1.0], {'is-current': "
    "<true>}), ('1600x1200@60', 1600, 1200, 60.0, 1.0, [1.0, 2.0], {})], {})], "
    "[(0, 0, 1.0, 0, true, [('DUMMY0', 'unknown', 'unknown', 'unknown')], {}), "
    "(1600, 500, 1.0, 0, false, [('DUMMY1', 'unknown', 'unknown', 'unknown')], "
    "{})], {'global-scale-required': <true>, 'layout-mode': <uint32 2>})";

ScopedGVariant CreateDisplayConfig(const char* serialized) {
  webrtc::Scoped<GError> error;
  auto variant = TakeGVariant(g_variant_parse(
      G_VARIANT_TYPE(kDisplayConfigType), serialized, /*limit=*/nullptr,
      /*endptr=*/nullptr, error.receive()));
  if (error) {
    ADD_FAILURE() << "g_variant_parse: " << error->message;
  }
  return variant;
}

}  // namespace

class GnomeDisplayConfigDBusClientTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(GnomeDisplayConfigDBusClientTest, GetMonitorsConfigReturnsCorrectInfo) {
  base::RunLoop run_loop;
  GnomeDisplayConfigDBusClient client;
  client.Init();

  auto callback = [](GnomeDisplayConfig config) {
    EXPECT_EQ(config.serial, 123U);
    EXPECT_TRUE(config.global_scale_required);
    EXPECT_EQ(config.layout_mode, GnomeDisplayConfig::LayoutMode::kPhysical);
    ASSERT_EQ(config.monitors.count("DUMMY0"), 1U);
    const auto& monitor = config.monitors["DUMMY0"];
    EXPECT_EQ(monitor.x, 0);
    EXPECT_EQ(monitor.y, 0);
    EXPECT_EQ(monitor.scale, 1.0);
    EXPECT_TRUE(monitor.is_primary);
    ASSERT_EQ(monitor.modes.size(), 2U);
    const auto& mode1 = monitor.modes[0];
    const auto& mode2 = monitor.modes[1];
    EXPECT_EQ(mode1.name, "1024x768@60");
    EXPECT_EQ(mode1.width, 1024);
    EXPECT_EQ(mode1.height, 768);
    EXPECT_TRUE(mode1.is_current);
    ASSERT_EQ(mode1.supported_scales.size(), 1U);
    EXPECT_EQ(mode1.supported_scales[0], 1.0);
    EXPECT_EQ(mode2.name, "800x600@60");
    EXPECT_EQ(mode2.width, 800);
    EXPECT_EQ(mode2.height, 600);
    EXPECT_FALSE(mode2.is_current);
    ASSERT_EQ(mode2.supported_scales.size(), 1U);
    EXPECT_EQ(mode2.supported_scales[0], 1.0);
  };
  client.GetMonitorsConfig(
      base::BindOnce(callback).Then(run_loop.QuitClosure()));

  ScopedGVariant config = CreateDisplayConfig(kSingleDisplayConfig);
  ASSERT_NE(config.get(), nullptr);
  client.FakeDisplayConfigForTest(std::move(config));

  run_loop.Run();
}

TEST_F(GnomeDisplayConfigDBusClientTest, CorrectInfoForSecondMonitor) {
  base::RunLoop run_loop;
  GnomeDisplayConfigDBusClient client;
  client.Init();

  auto callback = [](GnomeDisplayConfig config) {
    EXPECT_EQ(config.serial, 124U);
    ASSERT_EQ(config.monitors.count("DUMMY1"), 1U);
    const auto& monitor = config.monitors["DUMMY1"];
    EXPECT_EQ(monitor.x, 1600);
    EXPECT_EQ(monitor.y, 500);
    EXPECT_EQ(monitor.scale, 1.0);
    EXPECT_FALSE(monitor.is_primary);
    ASSERT_EQ(monitor.modes.size(), 2U);
    const auto& mode1 = monitor.modes[0];
    const auto& mode2 = monitor.modes[1];
    EXPECT_EQ(mode1.name, "800x600@60");
    EXPECT_EQ(mode1.width, 800);
    EXPECT_EQ(mode1.height, 600);
    EXPECT_TRUE(mode1.is_current);
    ASSERT_EQ(mode1.supported_scales.size(), 1U);
    EXPECT_EQ(mode1.supported_scales[0], 1.0);
    EXPECT_EQ(mode2.name, "1600x1200@60");
    EXPECT_EQ(mode2.width, 1600);
    EXPECT_EQ(mode2.height, 1200);
    EXPECT_FALSE(mode2.is_current);
    ASSERT_EQ(mode2.supported_scales.size(), 2U);
    EXPECT_EQ(mode2.supported_scales[0], 1.0);
    EXPECT_EQ(mode2.supported_scales[1], 2.0);
  };
  client.GetMonitorsConfig(
      base::BindOnce(callback).Then(run_loop.QuitClosure()));

  ScopedGVariant config = CreateDisplayConfig(kDualDisplayConfig);
  ASSERT_NE(config.get(), nullptr);
  client.FakeDisplayConfigForTest(std::move(config));

  run_loop.Run();
}

TEST_F(GnomeDisplayConfigDBusClientTest, CorrectConfigBuiltForGnome) {
  GnomeDisplayConfig::MonitorMode mode;
  mode.name = "800x600";
  mode.width = 800;
  mode.height = 600;
  mode.is_current = true;

  GnomeDisplayConfig::MonitorInfo dummy0;
  dummy0.x = 10;
  dummy0.y = 20;
  dummy0.scale = 1.0;
  dummy0.is_primary = true;
  dummy0.modes = {mode};

  GnomeDisplayConfig config;
  config.serial = 123;
  config.layout_mode = GnomeDisplayConfig::LayoutMode::kPhysical;
  config.monitors["DUMMY0"] = dummy0;

  std::string expected_format =
      "(123, 2, [(10, 20, 1.0, 0, true, [('DUMMY0', '800x600', {})])], "
      "{'layout-mode': <uint32 2>})";
  ScopedGVariant logical_monitors = config.BuildMonitorsConfigParameters();
  webrtc::Scoped<char> actual_format(
      g_variant_print(logical_monitors.get(), FALSE));

  EXPECT_EQ(actual_format.get(), expected_format);
}

}  // namespace remoting
