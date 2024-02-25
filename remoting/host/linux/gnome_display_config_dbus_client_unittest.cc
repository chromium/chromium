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
    "<true>})";
constexpr char kDualDisplayConfig[] =
    "(124, [(('DUMMY0', 'unknown', 'unknown', 'unknown'), [('800x600@60', 800, "
    "600, 60.0, 1.0, [1.0], {}), ('1600x1200@60', 1600, 1200, 60.0, 1.0, [1.0, "
    "2.0], {'is-current': <true>})], {}), (('DUMMY1', 'unknown', 'unknown', "
    "'unknown'), [('800x600@60', 800, 600, 60.0, 1.0, [1.0], {'is-current': "
    "<true>}), ('1600x1200@60', 1600, 1200, 60.0, 1.0, [1.0, 2.0], {})], {})], "
    "[(0, 0, 1.0, 0, true, [('DUMMY0', 'unknown', 'unknown', 'unknown')], {}), "
    "(1600, 500, 1.0, 0, false, [('DUMMY1', 'unknown', 'unknown', 'unknown')], "
    "{})], {'global-scale-required': <true>})";

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
    EXPECT_EQ(123U, config.serial);
    EXPECT_TRUE(config.global_scale_required);
    ASSERT_EQ(1U, config.monitors.count("DUMMY0"));
    const auto& monitor = config.monitors["DUMMY0"];
    EXPECT_EQ(0, monitor.x);
    EXPECT_EQ(0, monitor.y);
    EXPECT_EQ(1.0, monitor.scale);
    EXPECT_TRUE(monitor.is_primary);
    ASSERT_EQ(2U, monitor.modes.size());
    const auto& mode1 = monitor.modes[0];
    const auto& mode2 = monitor.modes[1];
    EXPECT_EQ("1024x768@60", mode1.name);
    EXPECT_EQ(1024, mode1.width);
    EXPECT_EQ(768, mode1.height);
    EXPECT_TRUE(mode1.is_current);
    EXPECT_EQ("800x600@60", mode2.name);
    EXPECT_EQ(800, mode2.width);
    EXPECT_EQ(600, mode2.height);
    EXPECT_FALSE(mode2.is_current);
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
    EXPECT_EQ(124U, config.serial);
    ASSERT_EQ(1U, config.monitors.count("DUMMY1"));
    const auto& monitor = config.monitors["DUMMY1"];
    EXPECT_EQ(1600, monitor.x);
    EXPECT_EQ(500, monitor.y);
    EXPECT_EQ(1.0, monitor.scale);
    EXPECT_FALSE(monitor.is_primary);
    ASSERT_EQ(2U, monitor.modes.size());
    const auto& mode1 = monitor.modes[0];
    const auto& mode2 = monitor.modes[1];
    EXPECT_EQ("800x600@60", mode1.name);
    EXPECT_EQ(800, mode1.width);
    EXPECT_EQ(600, mode1.height);
    EXPECT_TRUE(mode1.is_current);
    EXPECT_EQ("1600x1200@60", mode2.name);
    EXPECT_EQ(1600, mode2.width);
    EXPECT_EQ(1200, mode2.height);
    EXPECT_FALSE(mode2.is_current);
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
  config.monitors["DUMMY0"] = dummy0;

  std::string expected_format =
      "(123, 2, [(10, 20, 1.0, 0, true, [('DUMMY0', '800x600', {})])], {})";
  ScopedGVariant logical_monitors = config.BuildMonitorsConfigParameters();
  webrtc::Scoped<char> actual_format(
      g_variant_print(logical_monitors.get(), FALSE));

  EXPECT_EQ(expected_format, actual_format.get());
}

}  // namespace remoting
