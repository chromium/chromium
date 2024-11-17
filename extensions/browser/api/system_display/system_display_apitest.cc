// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/test/gtest_tags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "extensions/browser/api/system_display/system_display_api.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/mock_display_info_provider.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/api/system_display.h"
#include "extensions/common/extension_builder.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/result_catcher.h"

namespace extensions {

class SystemDisplayApiTest : public ShellApiTest {
 public:
  SystemDisplayApiTest() : provider_(new MockDisplayInfoProvider) {}

  SystemDisplayApiTest(const SystemDisplayApiTest&) = delete;
  SystemDisplayApiTest& operator=(const SystemDisplayApiTest&) = delete;

  ~SystemDisplayApiTest() override = default;

  void SetUpOnMainThread() override {
    ShellApiTest::SetUpOnMainThread();
    DisplayInfoProvider::InitializeForTesting(provider_.get());
  }

 protected:
  void SetInfo(const std::string& display_id,
               const api::system_display::DisplayProperties& properties) {
    provider_->SetDisplayProperties(
        display_id, properties,
        base::BindOnce([](std::optional<std::string>) {}));
  }
  std::unique_ptr<MockDisplayInfoProvider> provider_;
};

#if BUILDFLAG(IS_CHROMEOS)

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

  std::optional<base::Value::Dict> set_info = provider_->GetSetInfoValue();
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

  std::optional<base::Value::Dict> set_info = provider_->GetSetInfoValue();
  ASSERT_TRUE(set_info);

  EXPECT_TRUE(api_test_utils::GetBoolean(*set_info, "isPrimary"));
  EXPECT_EQ("mirroringId",
            api_test_utils::GetString(*set_info, "mirroringSourceId"));
  EXPECT_EQ(100, api_test_utils::GetInteger(*set_info, "boundsOriginX"));
  EXPECT_EQ(200, api_test_utils::GetInteger(*set_info, "boundsOriginY"));
  EXPECT_EQ(90, api_test_utils::GetInteger(*set_info, "rotation"));
  base::Value::Dict overscan = api_test_utils::GetDict(*set_info, "overscan");
  EXPECT_EQ(1, api_test_utils::GetInteger(overscan, "left"));
  EXPECT_EQ(2, api_test_utils::GetInteger(overscan, "top"));
  EXPECT_EQ(3, api_test_utils::GetInteger(overscan, "right"));
  EXPECT_EQ(4, api_test_utils::GetInteger(overscan, "bottom"));

  EXPECT_EQ("display_id", provider_->GetSetInfoDisplayId());
}

IN_PROC_BROWSER_TEST_F(SystemDisplayApiTest, EnableUnifiedDesktop) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-49098611-4862-4326-a90f-3f04b49eb336");

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
  scoped_refptr<const Extension> extension =
      LoadApp("system/display/overscan_no_complete");
  ASSERT_TRUE(extension);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  // Calibration was started by the app but not completed.
  ASSERT_TRUE(provider_->calibration_started(id));

  // Unloading the app should complete the calibration (and hide the overlay).
  UnloadApp(extension.get());
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

  std::optional<base::Value> result(
      api_test_utils::RunFunctionAndReturnSingleResult(
          show_native_calibration.get(), "[\"" + id + "\"]",
          browser_context()));

  ASSERT_TRUE(result->is_bool());
  EXPECT_TRUE(result->GetBool());
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
    EXPECT_EQ(api::system_display::MirrorMode::kNormal,
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
    EXPECT_EQ(api::system_display::MirrorMode::kMixed,
              provider_->mirror_mode());
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
    EXPECT_EQ(api::system_display::MirrorMode::kOff, provider_->mirror_mode());
  }
}

// Tests that Overscan is reset only when the primary main frame is removed.
IN_PROC_BROWSER_TEST_F(SystemDisplayApiTest, ResetDisplayIds) {
  const std::string id = "display0";
  api::system_display::DisplayProperties params;
  SetInfo(id, params);

  ResultCatcher catcher;
  const Extension* extension = LoadApp("system/display/overscan_no_complete");
  ASSERT_TRUE(extension);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  ExtensionHost* host = ProcessManager::Get(browser_context())
                            ->GetBackgroundHostForExtension(extension->id());
  ASSERT_TRUE(host);
  ASSERT_TRUE(host->host_contents());

  // Add a sub frame.
  ASSERT_TRUE(content::ExecJs(host->host_contents(), R"(
          var bar_frame = document.createElement('iframe');
          document.body.appendChild(bar_frame);
      )"));
  content::RenderFrameHostWrapper sub_frame_render_frame_host_wrapper(
      ChildFrameAt(host->host_contents()->GetPrimaryMainFrame(), 0));

  // By loading the app, calibration is started.
  ASSERT_TRUE(provider_->calibration_started(id));

  EXPECT_TRUE(ExecJs(host->host_contents()->GetPrimaryMainFrame(),
                     "const iframe = document.querySelector('iframe');\
                     iframe.remove();"));
  ASSERT_TRUE(
      sub_frame_render_frame_host_wrapper.WaitUntilRenderFrameDeleted());

  // Removing the sub frame doesn't affect calibration.
  ASSERT_TRUE(provider_->calibration_started(id));

  content::RenderFrameHostWrapper main_frame_render_frame_host_wrapper(
      host->host_contents()->GetPrimaryMainFrame());
  host->host_contents()->ClosePage();
  ASSERT_TRUE(
      main_frame_render_frame_host_wrapper.WaitUntilRenderFrameDeleted());

  ASSERT_FALSE(provider_->calibration_started(id));
}

#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
using SystemDisplayGetInfoTest = ShellApiTest;

IN_PROC_BROWSER_TEST_F(SystemDisplayGetInfoTest, GetInfo) {
  DisplayInfoProvider::ResetForTesting();
  scoped_refptr<const Extension> test_extension =
      ExtensionBuilder("Test", ExtensionBuilder::Type::PLATFORM_APP).Build();

  scoped_refptr<SystemDisplayGetInfoFunction> get_info_function(
      new SystemDisplayGetInfoFunction());

  get_info_function->set_has_callback(true);
  get_info_function->set_extension(test_extension.get());

  // Verify that the GetInfo function runs successfully. This uses the real
  // DisplayInfoProvider to verify that DisplayInfoProvider::GetAllDisplaysInfo
  // calls its callback.
  ASSERT_TRUE(api_test_utils::RunFunction(get_info_function.get(), "[]",
                                          browser_context()));
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace extensions
