// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fidl/fuchsia.input.virtualkeyboard/cpp/fidl.h>
#include <fidl/fuchsia.ui.input3/cpp/fidl.h>
#include <lib/fit/function.h>

#include <string_view>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/koid.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/functional/callback.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/test/browser_test.h"
#include "fuchsia_web/common/test/frame_for_test.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"
#include "fuchsia_web/webengine/browser/context_impl.h"
#include "fuchsia_web/webengine/browser/frame_impl.h"
#include "fuchsia_web/webengine/browser/mock_virtual_keyboard.h"
#include "fuchsia_web/webengine/features.h"
#include "fuchsia_web/webengine/test/scenic_test_helper.h"
#include "fuchsia_web/webengine/test/scoped_connection_checker.h"
#include "fuchsia_web/webengine/test/test_data.h"
#include "fuchsia_web/webengine/test/web_engine_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/public/ozone_platform.h"

namespace virtualkeyboard = fuchsia_input_virtualkeyboard;

namespace {

const gfx::Point kNoTarget = {999, 999};

constexpr char kInputFieldText[] = "input-text";
constexpr char kInputFieldModeTel[] = "input-mode-tel";
constexpr char kInputFieldModeNumeric[] = "input-mode-numeric";
constexpr char kInputFieldModeUrl[] = "input-mode-url";
constexpr char kInputFieldModeEmail[] = "input-mode-email";
constexpr char kInputFieldModeDecimal[] = "input-mode-decimal";
constexpr char kInputFieldModeSearch[] = "input-mode-search";
constexpr char kInputFieldTypeTel[] = "input-type-tel";
constexpr char kInputFieldTypeNumber[] = "input-type-number";
constexpr char kInputFieldTypePassword[] = "input-type-password";

class VirtualKeyboardTest : public WebEngineBrowserTest {
 public:
  VirtualKeyboardTest() {
    set_test_server_root(base::FilePath(kTestServerRoot));
  }
  ~VirtualKeyboardTest() override = default;

  void SetUp() override {
    if (ui::OzonePlatform::GetPlatformNameForTest() == "headless") {
      GTEST_SKIP() << "Keyboard inputs are ignored in headless mode.";
    }

    scoped_feature_list_.InitWithFeatures(
        {features::kVirtualKeyboard, features::kKeyboardInput}, {});
    WebEngineBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    WebEngineBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());

    fuchsia::web::CreateFrameParams params;
    frame_for_test_ = FrameForTest::Create(context(), std::move(params));

    component_context_.emplace(
        base::TestComponentContextForProcess::InitialState::kCloneAll);
    controller_creator_.emplace(&component_context_.value());

    controller_ = controller_creator_->CreateController();

    // Ensure that the fuchsia.ui.input3.Keyboard service is connected.
    component_context_->additional_services()
        ->RemovePublicService<fuchsia_ui_input3::Keyboard>(
            fidl::DiscoverableProtocolName<fuchsia_ui_input3::Keyboard>);
    keyboard_input_checker_.emplace(component_context_->additional_services());

    fuchsia::web::NavigationControllerPtr controller;
    frame_for_test_.ptr()->GetNavigationController(controller.NewRequest());
    const GURL test_url(embedded_test_server()->GetURL("/input_fields.html"));
    EXPECT_TRUE(LoadUrlAndExpectResponse(
        controller.get(), fuchsia::web::LoadUrlParams(), test_url.spec()));
    frame_for_test_.navigation_listener().RunUntilUrlEquals(test_url);

    fuchsia::web::FramePtr* frame_ptr = &(frame_for_test_.ptr());
    web_contents_ =
        context_impl()->GetFrameImplForTest(frame_ptr)->web_contents();
    scenic_test_helper_.CreateScenicView(
        context_impl()->GetFrameImplForTest(frame_ptr), frame_for_test_.ptr());
    scenic_test_helper_.SetUpViewForInteraction(web_contents_);

    controller_->AwaitWatchAndRespondWith(false);
    ASSERT_EQ(
        base::GetKoid(controller_->view_ref().reference()).value(),
        base::GetKoid(scenic_test_helper_.CloneViewRef().reference).value());
  }

  void TearDownOnMainThread() override {
    frame_for_test_ = {};
    WebEngineBrowserTest::TearDownOnMainThread();
  }

  // The tests expect to have input processed immediately, even if the
  // content has not been displayed yet. That's fine for the test, but
  // we need to explicitly allow it.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch("allow-pre-commit-input");
  }

  gfx::Point GetCoordinatesOfInputField(std::string_view id) {
    // Distance to click from the top/left extents of an input field.
    constexpr int kInputFieldClickInset = 8;

    std::optional<base::Value> result = ExecuteJavaScript(
        frame_for_test_.ptr().get(),
        base::StringPrintf("getPointInsideText('%.*s')",
                           base::saturated_cast<int>(id.length()), id.data()));
    if (!result || !result->is_dict()) {
      ADD_FAILURE() << "!result";
      return {};
    }

    // Note that coordinates are floating point and must be retrieved as such
    // from the Value, but we can cast them to integers and disregard the
    // fractional value with no major consequences.
    return gfx::Point(
        *result->GetDict().FindDouble("x") + kInputFieldClickInset,
        *result->GetDict().FindDouble("y") + kInputFieldClickInset);
  }

 protected:
  FrameForTest frame_for_test_;
  ScenicTestHelper scenic_test_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;

  std::optional<EnsureConnectedChecker<fuchsia_ui_input3::Keyboard>>
      keyboard_input_checker_;

  // Fake virtual keyboard services for the InputMethod to use.
  std::optional<base::TestComponentContextForProcess> component_context_;
  std::optional<MockVirtualKeyboardControllerCreator> controller_creator_;
  std::unique_ptr<MockVirtualKeyboardController> controller_;

  content::WebContents* web_contents_ = nullptr;
};

// Verifies that RequestShow() is not called redundantly if the virtual
// keyboard is reported as visible.
IN_PROC_BROWSER_TEST_F(VirtualKeyboardTest, ShowAndHideWithVisibility) {
  testing::InSequence s;

  // Alphanumeric field click.
  base::RunLoop on_show_run_loop;
  EXPECT_CALL(*controller_, RequestShow(testing::_))
      .WillOnce(testing::InvokeWithoutArgs(
          [&on_show_run_loop]() { on_show_run_loop.Quit(); }))
      .RetiresOnSaturation();

  // Numeric field click.
  base::RunLoop click_numeric_run_loop;
  EXPECT_CALL(*controller_, RequestHide(testing::_)).RetiresOnSaturation();
  EXPECT_CALL(
      *controller_,
      SetTextType(testing::Eq(MockVirtualKeyboardController::SetTextTypeRequest{
                      {.text_type = virtualkeyboard::TextType::kNumeric}}),
                  testing::_))
      .RetiresOnSaturation();
  EXPECT_CALL(*controller_, RequestShow(testing::_))
      .WillOnce(testing::InvokeWithoutArgs(
          [&click_numeric_run_loop]() { click_numeric_run_loop.Quit(); }))
      .RetiresOnSaturation();

  // Input blur click.
  base::RunLoop on_hide_run_loop;
  EXPECT_CALL(*controller_, RequestHide(testing::_))
      .WillOnce(testing::InvokeWithoutArgs(
          [&on_hide_run_loop]() { on_hide_run_loop.Quit(); }))
      .RetiresOnSaturation();

  // In some cases, Blink may signal an
  // InputMethodClient::OnTextInputTypeChanged event, which will cause
  // an extra call to VirtualKeyboardController:RequestHide. This is harmless
  // in practice due to RequestHide()'s idempotence, however we still need to
  // anticipate that behavior in the controller mocks.
  EXPECT_CALL(*controller_, RequestHide(testing::_)).Times(testing::AtMost(1));

  // Give focus to an alphanumeric input field, which will result in
  // RequestShow() being called.
  content::SimulateTapAt(web_contents_,
                         GetCoordinatesOfInputField(kInputFieldText));
  on_show_run_loop.Run();
  EXPECT_EQ(controller_->text_type(), virtualkeyboard::TextType::kAlphanumeric);

  // Indicate that the virtual keyboard is now visible.
  controller_->AwaitWatchAndRespondWith(true);
  base::RunLoop().RunUntilIdle();

  // Tap on another text field. RequestShow should not be called a second time
  // since the keyboard is already onscreen.
  content::SimulateTapAt(web_contents_,
                         GetCoordinatesOfInputField(kInputFieldModeNumeric));
  click_numeric_run_loop.Run();

  // Trigger input blur by clicking outside any input element.
  content::SimulateTapAt(web_contents_, kNoTarget);
  on_hide_run_loop.Run();
}

// Gives focus to a sequence of HTML <input> nodes with different InputModes,
// and verifies that the InputMode's FIDL equivalent is sent via SetTextType().
IN_PROC_BROWSER_TEST_F(VirtualKeyboardTest, InputModeMappings) {
  // Note that the service will elide type updates if there is no change,
  // so the array is ordered to produce an update on each entry.
  const std::vector<std::pair<std::string_view, virtualkeyboard::TextType>>
      kInputTypeMappings = {
          {kInputFieldModeTel, virtualkeyboard::TextType::kPhone},
          {kInputFieldModeSearch, virtualkeyboard::TextType::kAlphanumeric},
          {kInputFieldModeNumeric, virtualkeyboard::TextType::kNumeric},
          {kInputFieldModeUrl, virtualkeyboard::TextType::kAlphanumeric},
          {kInputFieldModeDecimal, virtualkeyboard::TextType::kNumeric},
          {kInputFieldModeEmail, virtualkeyboard::TextType::kAlphanumeric},
          {kInputFieldTypeTel, virtualkeyboard::TextType::kPhone},
          {kInputFieldTypeNumber, virtualkeyboard::TextType::kNumeric},
          {kInputFieldTypePassword, virtualkeyboard::TextType::kAlphanumeric},
      };

  // GMock expectations must be set upfront, hence the redundant for-each loop.
  testing::InSequence s;
  virtualkeyboard::TextType previous_text_type =
      virtualkeyboard::TextType::kAlphanumeric;
  std::vector<base::RunLoop> set_type_loops(std::size(kInputTypeMappings));
  for (size_t i = 0; i < std::size(kInputTypeMappings); ++i) {
    const auto& field_type_pair = kInputTypeMappings[i];
    EXPECT_NE(field_type_pair.second, previous_text_type);

    EXPECT_CALL(
        *controller_,
        SetTextType(
            testing::Eq(MockVirtualKeyboardController::SetTextTypeRequest{
                {.text_type = field_type_pair.second}}),
            testing::_))
        .WillOnce(testing::InvokeWithoutArgs(
            [run_loop = &set_type_loops[i]]() mutable { run_loop->Quit(); }))
        .RetiresOnSaturation();
    previous_text_type = field_type_pair.second;
  }

  controller_->AwaitWatchAndRespondWith(false);

  for (size_t i = 0; i < std::size(kInputTypeMappings); ++i) {
    content::SimulateTapAt(
        web_contents_, GetCoordinatesOfInputField(kInputTypeMappings[i].first));

    // Spin the runloop until we've received the type update.
    set_type_loops[i].Run();
  }
}

IN_PROC_BROWSER_TEST_F(VirtualKeyboardTest, Disconnection) {
  testing::InSequence s;
  base::RunLoop on_show_run_loop;
  EXPECT_CALL(*controller_, RequestShow(testing::_))
      .WillOnce([&on_show_run_loop](
                    MockVirtualKeyboardController::RequestShowCompleter::Sync&
                        completer) { on_show_run_loop.Quit(); });

  // Tapping inside the text field should show the IME and signal RequestShow.
  content::SimulateTapAt(web_contents_,
                         GetCoordinatesOfInputField(kInputFieldText));
  on_show_run_loop.Run();

  controller_->AwaitWatchAndRespondWith(true);
  base::RunLoop().RunUntilIdle();

  // Disconnect the FIDL service.
  controller_.reset();
  base::RunLoop().RunUntilIdle();

  // Focus on another text field, then defocus. Nothing should crash.
  content::SimulateTapAt(web_contents_,
                         GetCoordinatesOfInputField(kInputFieldModeNumeric));
  content::SimulateTapAt(web_contents_, kNoTarget);
}

}  // namespace
