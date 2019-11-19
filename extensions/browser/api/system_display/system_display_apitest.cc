// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/debug/leak_annotations.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "extensions/browser/api/system_display/system_display_api.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/api/system_display.h"
#include "extensions/common/extension_builder.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/result_catcher.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/test/scoped_screen_override.h"

namespace extensions {

using api::system_display::Bounds;
using api::system_display::DisplayUnitInfo;
using display::Screen;
using display::test::ScopedScreenOverride;

class MockScreen : public Screen {
 public:
  MockScreen() {
    for (int i = 0; i < 4; i++) {
      gfx::Rect bounds(0, 0, 1280, 720);
      gfx::Rect work_area(0, 0, 960, 720);
      display::Display display(i, bounds);
      display.set_work_area(work_area);
      displays_.push_back(display);
    }
  }
  ~MockScreen() override {}

 protected:
  // Overridden from display::Screen:
  gfx::Point GetCursorScreenPoint() override { return gfx::Point(); }
  bool IsWindowUnderCursor(gfx::NativeWindow window) override { return false; }
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override {
    return gfx::NativeWindow();
  }
  int GetNumDisplays() const override {
    return static_cast<int>(displays_.size());
  }
  const std::vector<display::Display>& GetAllDisplays() const override {
    return displays_;
  }
  display::Display GetDisplayNearestWindow(
      gfx::NativeWindow window) const override {
    return display::Display(0);
  }
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override {
    return display::Display(0);
  }
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override {
    return display::Display(0);
  }
  display::Display GetPrimaryDisplay() const override { return displays_[0]; }
  void AddObserver(display::DisplayObserver* observer) override {}
  void RemoveObserver(display::DisplayObserver* observer) override {}

 private:
  std::vector<display::Display> displays_;

  DISALLOW_COPY_AND_ASSIGN(MockScreen);
};

class MockDisplayInfoProvider : public DisplayInfoProvider {
 public:
  MockDisplayInfoProvider() {}

  ~MockDisplayInfoProvider() override {}

  void SetDisplayProperties(
      const std::string& display_id,
      const api::system_display::DisplayProperties& properties,
      ErrorCallback callback) override {
    // Should get called only once per test case.
    EXPECT_FALSE(set_info_value_);
    set_info_value_ = properties.ToValue();
    set_info_display_id_ = display_id;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
  }

  void EnableUnifiedDesktop(bool enable) override {
    unified_desktop_enabled_ = enable;
  }

  bool OverscanCalibrationStart(const std::string& id) override {
    if (base::Contains(overscan_started_, id))
      return false;
    overscan_started_.insert(id);
    return true;
  }

  bool OverscanCalibrationAdjust(
      const std::string& id,
      const api::system_display::Insets& delta) override {
    if (!base::Contains(overscan_started_, id))
      return false;
    overscan_adjusted_.insert(id);
    return true;
  }

  bool OverscanCalibrationReset(const std::string& id) override {
    if (!base::Contains(overscan_started_, id))
      return false;
    overscan_adjusted_.erase(id);
    return true;
  }

  bool OverscanCalibrationComplete(const std::string& id) override {
    if (!base::Contains(overscan_started_, id))
      return false;
    overscan_started_.erase(id);
    return true;
  }

  std::unique_ptr<base::DictionaryValue> GetSetInfoValue() {
    return std::move(set_info_value_);
  }

  std::string GetSetInfoDisplayId() const { return set_info_display_id_; }

  bool unified_desktop_enabled() const { return unified_desktop_enabled_; }

  bool calibration_started(const std::string& id) const {
    return base::Contains(overscan_started_, id);
  }

  bool calibration_changed(const std::string& id) const {
    return base::Contains(overscan_adjusted_, id);
  }

  const api::system_display::MirrorMode& mirror_mode() const {
    return mirror_mode_;
  }

  void SetTouchCalibrationWillSucceed(bool success) {
    native_touch_calibration_success_ = success;
  }

  void ShowNativeTouchCalibration(const std::string& id,
                                  ErrorCallback callback) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  native_touch_calibration_success_
                                      ? base::nullopt
                                      : base::Optional<std::string>("failed")));
  }

  void SetMirrorMode(const api::system_display::MirrorModeInfo& info,
                     ErrorCallback callback) override {
    mirror_mode_ = info.mode;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
  }

 private:
  // Update the content of the |unit| obtained for |display| using
  // platform specific method.
  void UpdateDisplayUnitInfoForPlatform(
      const display::Display& display,
      extensions::api::system_display::DisplayUnitInfo* unit) override {
    int64_t id = display.id();
    unit->name = "DISPLAY NAME FOR " + base::NumberToString(id);
    if (id == 1)
      unit->mirroring_source_id = "0";
    unit->is_primary = id == 0 ? true : false;
    unit->is_internal = id == 0 ? true : false;
    unit->is_enabled = true;
    unit->rotation = (90 * id) % 360;
    unit->dpi_x = 96.0;
    unit->dpi_y = 96.0;
    if (id == 0) {
      unit->overscan.left = 20;
      unit->overscan.top = 40;
      unit->overscan.right = 60;
      unit->overscan.bottom = 80;
    }
  }

  std::unique_ptr<base::DictionaryValue> set_info_value_;
  std::string set_info_display_id_;
  bool unified_desktop_enabled_ = false;
  std::set<std::string> overscan_started_;
  std::set<std::string> overscan_adjusted_;

  bool native_touch_calibration_success_ = false;

  api::system_display::MirrorMode mirror_mode_ =
      api::system_display::MIRROR_MODE_OFF;

  DISALLOW_COPY_AND_ASSIGN(MockDisplayInfoProvider);
};

class SystemDisplayApiTest : public ShellApiTest {
 public:
  SystemDisplayApiTest()
      : provider_(new MockDisplayInfoProvider), screen_(new MockScreen) {}

  ~SystemDisplayApiTest() override {}

  void SetUpOnMainThread() override {
    ShellApiTest::SetUpOnMainThread();
    ANNOTATE_LEAKING_OBJECT_PTR(display::Screen::GetScreen());
    scoped_screen_override_ =
        std::make_unique<ScopedScreenOverride>(screen_.get());
    DisplayInfoProvider::InitializeForTesting(provider_.get());
  }

 protected:
  void SetInfo(const std::string& display_id,
               const api::system_display::DisplayProperties& properties) {
    provider_->SetDisplayProperties(
        display_id, properties,
        base::BindOnce([](base::Optional<std::string>) {}));
  }
  std::unique_ptr<MockDisplayInfoProvider> provider_;
  std::unique_ptr<display::Screen> screen_;
  std::unique_ptr<ScopedScreenOverride> scoped_screen_override_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemDisplayApiTest);
};

IN_PROC_BROWSER_TEST_F(SystemDisplayApiTest, GetDisplayInfo) {
  ASSERT_TRUE(RunAppTest("system/display/info")) << message_;
}

#if !defined(OS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(SystemDisplayApiTest, SetDisplay) {
  scoped_refptr<SystemDisplaySetDisplayPropertiesFunction> set_info_function(
      new SystemDisplaySetDisplayPropertiesFunction());

  set_info_function->set_has_callback(true);

  EXPECT_EQ(
      SystemDisplayCrOSRestrictedFunction::kCrosOnlyError,
      api_test_utils::RunFunctionAndReturnError(
          set_info_function.get(), "[\"display_id\", {}]", browser_context()));

  std::unique_ptr<base::DictionaryValue> set_info =
      provider_->GetSetInfoValue();
  EXPECT_FALSE(set_info);
}

#else  // !defined(OS_CHROMEOS)

// TODO(stevenjb): Add API tests for {GS}etDisplayLayout. That code currently
// lives in src/chrome but should be getting moved soon.

IN_PROC_BROWSER_TEST_F(SystemDisplayApiTest, SetDisplayNotKioskEnabled) {
  scoped_refptr<const Extension> test_extension =
      ExtensionBuilder("Test", ExtensionBuilder::Type::PLATFORM_APP).Build();

  scoped_refptr<SystemDisplaySetDisplayPropertiesFunction> set_info_function(
      new SystemDisplaySetDisplayPropertiesFunction());

  set_info_function->set_extension(test_extension.get());
  set_info_function->set_has_callback(true);

  EXPECT_EQ(
      SystemDisplayCrOSRestrictedFunction::kKioskOnlyError,
      api_test_utils::RunFunctionAndReturnError(
          set_info_function.get(), "[\"display_id\", {}]", browser_context()));

  std::unique_ptr<base::DictionaryValue> set_info =
      provider_->GetSetInfoValue();
  EXPECT_FALSE(set_info);
}

IN_PROC_BROWSER_TEST_F(SystemDisplayApiTest, SetDisplayKioskEnabled) {
  scoped_refptr<const Extension> test_extension =
      ExtensionBuilder("Test", ExtensionBuilder::Type::PLATFORM_APP)
          .SetManifestKey("kiosk_enabled", true)
          .Build();

  scoped_refptr<SystemDisplaySetDisplayPropertiesFunction> set_info_function(
      new SystemDisplaySetDisplayPropertiesFunction());

  set_info_function->set_has_callback(true);
  set_info_function->set_extension(test_extension.get());

  ASSERT_TRUE(api_test_utils::RunFunction(
      set_info_function.get(),
      "[\"display_id\", {\n"
      "  \"isPrimary\": true,\n"
      "  \"mirroringSourceId\": \"mirroringId\",\n"
      "  \"boundsOriginX\": 100,\n"
      "  \"boundsOriginY\": 200,\n"
      "  \"rotation\": 90,\n"
      "  \"overscan\": {\"left\": 1, \"top\": 2, \"right\": 3, \"bottom\": 4}\n"
      "}]",
      browser_context()));

  std::unique_ptr<base::DictionaryValue> set_info =
      provider_->GetSetInfoValue();
  ASSERT_TRUE(set_info);
  EXPECT_TRUE(api_test_utils::GetBoolean(set_info.get(), "isPrimary"));
  EXPECT_EQ("mirroringId",
            api_test_utils::GetString(set_info.get(), "mirroringSourceId"));
  EXPECT_EQ(100, api_test_utils::GetInteger(set_info.get(), "boundsOriginX"));
  EXPECT_EQ(200, api_test_utils::GetInteger(set_info.get(), "boundsOriginY"));
  EXPECT_EQ(90, api_test_utils::GetInteger(set_info.get(), "rotation"));
  base::DictionaryValue* overscan;
  ASSERT_TRUE(set_info->GetDictionary("overscan", &overscan));
  EXPECT_EQ(1, api_test_utils::GetInteger(overscan, "left"));
  EXPECT_EQ(2, api_test_utils::GetInteger(overscan, "top"));
  EXPECT_EQ(3, api_test_utils::GetInteger(overscan, "right"));
  EXPECT_EQ(4, api_test_utils::GetInteger(overscan, "bottom"));

  EXPECT_EQ("display_id", provider_->GetSetInfoDisplayId());
}

IN_PROC_BROWSER_TEST_F(SystemDisplayApiTest, EnableUnifiedDesktop) {
  scoped_refptr<const Extension> test_extension =
      ExtensionBuilder("Test", ExtensionBuilder::Type::PLATFORM_APP)
          .SetManifestKey("kiosk_enabled", true)
          .Build();
  {
    scoped_refptr<SystemDisplayEnableUnifiedDesktopFunction>
        enable_unified_function(
            new SystemDisplayEnableUnifiedDesktopFunction());

    enable_unified_function->set_has_callback(true);
    enable_unified_function->set_extension(test_extension.get());

    EXPECT_FALSE(provider_->unified_desktop_enabled());

    ASSERT_TRUE(api_test_utils::RunFunction(enable_unified_function.get(),
                                            "[true]", browser_context()));
    EXPECT_TRUE(provider_->unified_desktop_enabled());
  }
  {
    scoped_refptr<SystemDisplayEnableUnifiedDesktopFunction>
        enable_unified_function(
            new SystemDisplayEnableUnifiedDesktopFunction());

    enable_unified_function->set_has_callback(true);
    enable_unified_function->set_extension(test_extension.get());
    ASSERT_TRUE(api_test_utils::RunFunction(enable_unified_function.get(),
                                            "[false]", browser_context()));
    EXPECT_FALSE(provider_->unified_desktop_enabled());
  }
}

IN_PROC_BROWSER_TEST_F(SystemDisplayApiTest, OverscanCalibrationStart) {
  const std::string id = "display0";
  scoped_refptr<const Extension> test_extension =
      ExtensionBuilder("Test", ExtensionBuilder::Type::PLATFORM_APP)
          .SetManifestKey("kiosk_enabled", true)
          .Build();

  // Setup MockDisplayInfoProvider.
  api::system_display::DisplayProperties params;
  SetInfo(id, params);

  // Call OverscanCalibrationStart.
  scoped_refptr<SystemDisplayOverscanCalibrationStartFunction> start_function(
      new SystemDisplayOverscanCalibrationStartFunction());
  start_function->set_extension(test_extension.get());
  start_function->set_has_callback(true);
  ASSERT_TRUE(api_test_utils::RunFunction(
      start_function.get(), "[\"" + id + "\"]", browser_context()));

  ASSERT_TRUE(provider_->calibration_started(id));

  // Call OverscanCalibrationComplete.
  scoped_refptr<SystemDisplayOverscanCalibrationCompleteFunction>
      complete_function(new SystemDisplayOverscanCalibrationCompleteFunction());
  complete_function->set_extension(test_extension.get());
  complete_function->set_has_callback(true);
  ASSERT_TRUE(api_test_utils::RunFunction(
      complete_function.get(), "[\"" + id + "\"]", browser_context()));

  ASSERT_FALSE(provider_->calibration_started(id));
}

IN_PROC_BROWSER_TEST_F(SystemDisplayApiTest, OverscanCalibrationApp) {
  // Setup MockDisplayInfoProvider.
  const std::string id = "display0";
  api::system_display::DisplayProperties params;
  SetInfo(id, params);

  ASSERT_TRUE(RunAppTest("system/display/overscan")) << message_;

  ASSERT_FALSE(provider_->calibration_started(id));
  ASSERT_TRUE(provider_->calibration_changed(id));
}

IN_PROC_BROWSER_TEST_F(SystemDisplayApiTest, OverscanCalibrationAppNoComplete) {
  // Setup MockDisplayInfoProvider.
  const std::string id = "display0";
  api::system_display::DisplayProperties params;
  SetInfo(id, params);

  ResultCatcher catcher;
  const Extension* extension = LoadApp("system/display/overscan_no_complete");
  ASSERT_TRUE(extension);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  // Calibration was started by the app but not completed.
  ASSERT_TRUE(provider_->calibration_started(id));

  // Unloading the app should complete the calibraiton (and hide the overlay).
  UnloadApp(extension);
  ASSERT_FALSE(provider_->calibration_changed(id));
  ASSERT_FALSE(provider_->calibration_started(id));
}

IN_PROC_BROWSER_TEST_F(SystemDisplayApiTest, ShowNativeTouchCalibrationFail) {
  const std::string id = "display0";
  scoped_refptr<const Extension> test_extension =
      ExtensionBuilder("Test", ExtensionBuilder::Type::PLATFORM_APP)
          .SetManifestKey("kiosk_enabled", true)
          .Build();

  scoped_refptr<SystemDisplayShowNativeTouchCalibrationFunction>
      show_native_calibration(
          new SystemDisplayShowNativeTouchCalibrationFunction());

  show_native_calibration->set_has_callback(true);
  show_native_calibration->set_extension(test_extension.get());

  provider_->SetTouchCalibrationWillSucceed(false);

  std::string result(api_test_utils::RunFunctionAndReturnError(
      show_native_calibration.get(), "[\"" + id + "\"]", browser_context()));

  EXPECT_FALSE(result.empty());
}

IN_PROC_BROWSER_TEST_F(SystemDisplayApiTest, ShowNativeTouchCalibration) {
  const std::string id = "display0";
  scoped_refptr<const Extension> test_extension =
      ExtensionBuilder("Test", ExtensionBuilder::Type::PLATFORM_APP)
          .SetManifestKey("kiosk_enabled", true)
          .Build();

  scoped_refptr<SystemDisplayShowNativeTouchCalibrationFunction>
      show_native_calibration(
          new SystemDisplayShowNativeTouchCalibrationFunction());

  show_native_calibration->set_has_callback(true);
  show_native_calibration->set_extension(test_extension.get());

  provider_->SetTouchCalibrationWillSucceed(true);

  std::unique_ptr<base::Value> result(
      api_test_utils::RunFunctionAndReturnSingleResult(
          show_native_calibration.get(), "[\"" + id + "\"]",
          browser_context()));

  bool callback_result;
  ASSERT_TRUE(result->GetAsBoolean(&callback_result));
  ASSERT_TRUE(callback_result);
}

IN_PROC_BROWSER_TEST_F(SystemDisplayApiTest, SetMirrorMode) {
  scoped_refptr<const Extension> test_extension =
      ExtensionBuilder("Test", ExtensionBuilder::Type::PLATFORM_APP)
          .SetManifestKey("kiosk_enabled", true)
          .Build();
  {
    auto set_mirror_mode_function =
        base::MakeRefCounted<SystemDisplaySetMirrorModeFunction>();

    set_mirror_mode_function->set_has_callback(true);
    set_mirror_mode_function->set_extension(test_extension.get());

    ASSERT_TRUE(api_test_utils::RunFunction(set_mirror_mode_function.get(),
                                            "[{\n"
                                            "  \"mode\": \"normal\"\n"
                                            "}]",
                                            browser_context()));
    EXPECT_EQ(api::system_display::MIRROR_MODE_NORMAL,
              provider_->mirror_mode());
  }
  {
    auto set_mirror_mode_function =
        base::MakeRefCounted<SystemDisplaySetMirrorModeFunction>();

    set_mirror_mode_function->set_has_callback(true);
    set_mirror_mode_function->set_extension(test_extension.get());
    ASSERT_TRUE(
        api_test_utils::RunFunction(set_mirror_mode_function.get(),
                                    "[{\n"
                                    "  \"mode\": \"mixed\",\n"
                                    "  \"mirroringSourceId\": \"10\",\n"
                                    "  \"mirroringDestinationIds\": [\"11\"]\n"
                                    "}]",
                                    browser_context()));
    EXPECT_EQ(api::system_display::MIRROR_MODE_MIXED, provider_->mirror_mode());
  }
  {
    auto set_mirror_mode_function =
        base::MakeRefCounted<SystemDisplaySetMirrorModeFunction>();

    set_mirror_mode_function->set_has_callback(true);
    set_mirror_mode_function->set_extension(test_extension.get());

    ASSERT_TRUE(api_test_utils::RunFunction(set_mirror_mode_function.get(),
                                            "[{\n"
                                            "  \"mode\": \"off\"\n"
                                            "}]",
                                            browser_context()));
    EXPECT_EQ(api::system_display::MIRROR_MODE_OFF, provider_->mirror_mode());
  }
}

#endif  // !defined(OS_CHROMEOS)

}  // namespace extensions
