// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/input/virtualkeyboard/cpp/fidl.h>
#include <lib/fit/function.h>
#include <lib/ui/scenic/cpp/view_ref_pair.h>
#include <lib/ui/scenic/cpp/view_token_pair.h>

#include "base/callback.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/koid.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "fuchsia/base/test/frame_test_util.h"
#include "fuchsia/base/test/test_navigation_listener.h"
#include "fuchsia/engine/browser/context_impl.h"
#include "fuchsia/engine/browser/frame_impl.h"
#include "fuchsia/engine/browser/frame_window_tree_host.h"
#include "fuchsia/engine/test/test_data.h"
#include "fuchsia/engine/test/web_engine_browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace virtualkeyboard = fuchsia::input::virtualkeyboard;

namespace {

const gfx::Rect kBounds = {1000, 1000};
const gfx::Point kNoTarget = {999, 999};

constexpr char kInputFieldText[] = "input-text";
constexpr char kInputFieldTel[] = "input-tel";
constexpr char kInputFieldNumeric[] = "input-numeric";
constexpr char kInputFieldUrl[] = "input-url";
constexpr char kInputFieldEmail[] = "input-email";
constexpr char kInputFieldDecimal[] = "input-decimal";
constexpr char kInputFieldSearch[] = "input-search";

class MockVirtualKeyboardController : public virtualkeyboard::Controller {
 public:
  MockVirtualKeyboardController() : binding_(this) {}
  ~MockVirtualKeyboardController() override = default;

  MockVirtualKeyboardController(MockVirtualKeyboardController&) = delete;
  MockVirtualKeyboardController operator=(MockVirtualKeyboardController&) =
      delete;

  void Bind(fuchsia::ui::views::ViewRef view_ref,
            virtualkeyboard::TextType text_type,
            fidl::InterfaceRequest<fuchsia::input::virtualkeyboard::Controller>
                controller_request) {
    text_type_ = text_type;
    view_ref_ = std::move(view_ref);
    binding_.Bind(std::move(controller_request));
  }

  // Spins a RunLoop until the client calls WatchVisibility().
  void AwaitWatchAndRespondWith(bool is_visible) {
    if (!watch_vis_callback_) {
      base::RunLoop run_loop;
      on_watch_visibility_ = run_loop.QuitClosure();
      run_loop.Run();
      ASSERT_TRUE(watch_vis_callback_);
    }

    (*watch_vis_callback_)(is_visible);
    watch_vis_callback_ = {};
  }

  const fuchsia::ui::views::ViewRef& view_ref() const { return view_ref_; }
  virtualkeyboard::TextType text_type() const { return text_type_; }

  // virtualkeyboard::Controller implementation.
  MOCK_METHOD0(RequestShow, void());
  MOCK_METHOD0(RequestHide, void());
  MOCK_METHOD1(SetTextType, void(virtualkeyboard::TextType text_type));

 private:
  // virtualkeyboard::Controller implementation.
  void WatchVisibility(
      virtualkeyboard::Controller::WatchVisibilityCallback callback) final {
    watch_vis_callback_ = std::move(callback);

    if (on_watch_visibility_)
      std::move(on_watch_visibility_).Run();
  }

  base::OnceClosure on_watch_visibility_;
  absl::optional<virtualkeyboard::Controller::WatchVisibilityCallback>
      watch_vis_callback_;
  fuchsia::ui::views::ViewRef view_ref_;
  virtualkeyboard::TextType text_type_;
  fidl::Binding<fuchsia::input::virtualkeyboard::Controller> binding_;
};

// Services connection requests for MockVirtualKeyboardControllers.
class MockVirtualKeyboardControllerCreator
    : public virtualkeyboard::ControllerCreator {
 public:
  explicit MockVirtualKeyboardControllerCreator(
      base::TestComponentContextForProcess* component_context)
      : binding_(component_context->additional_services(), this) {}

  ~MockVirtualKeyboardControllerCreator() override {
    CHECK(!pending_controller_);
  }

  MockVirtualKeyboardControllerCreator(MockVirtualKeyboardControllerCreator&) =
      delete;
  MockVirtualKeyboardControllerCreator operator=(
      MockVirtualKeyboardControllerCreator&) = delete;

  // Returns an unbound MockVirtualKeyboardController, which will later be
  // connected when |this| handles a call to the FIDL method Create().
  std::unique_ptr<MockVirtualKeyboardController> CreateController() {
    DCHECK(!pending_controller_);

    auto controller = std::make_unique<MockVirtualKeyboardController>();
    pending_controller_ = controller.get();
    return controller;
  }

 private:
  // fuchsia::input::virtualkeyboard implementation.
  void Create(
      fuchsia::ui::views::ViewRef view_ref,
      fuchsia::input::virtualkeyboard::TextType text_type,
      fidl::InterfaceRequest<fuchsia::input::virtualkeyboard::Controller>
          controller_request) final {
    CHECK(pending_controller_);
    pending_controller_->Bind(std::move(view_ref), text_type,
                              std::move(controller_request));
    pending_controller_ = nullptr;
  }

  MockVirtualKeyboardController* pending_controller_ = nullptr;
  base::ScopedServiceBinding<virtualkeyboard::ControllerCreator> binding_;
};

class VirtualKeyboardTest : public cr_fuchsia::WebEngineBrowserTest {
 public:
  VirtualKeyboardTest() {
    set_test_server_root(base::FilePath(cr_fuchsia::kTestServerRoot));
  }
  ~VirtualKeyboardTest() override = default;

  void SetUpOnMainThread() override {
    cr_fuchsia::WebEngineBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());

    component_context_.emplace(
        base::TestComponentContextForProcess::InitialState::kCloneAll);
    controller_creator_.emplace(&*component_context_);
    controller_ = controller_creator_->CreateController();

    frame_ = CreateFrame(&navigation_listener_);
    frame_impl_ = context_impl()->GetFrameImplForTest(&frame_);

    // Navigate to the test page.
    fuchsia::web::NavigationControllerPtr controller;
    frame_->GetNavigationController(controller.NewRequest());
    const GURL test_url(embedded_test_server()->GetURL("/input_fields.html"));
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        controller.get(), fuchsia::web::LoadUrlParams(), test_url.spec()));
    navigation_listener_.RunUntilUrlEquals(test_url);

    // Simulate the creation of a Scenic View, except bypassing the actual
    // construction of a Scenic PlatformWindow in favor of using an injected
    // StubWindow.
    scenic::ViewRefPair view_ref_pair = scenic::ViewRefPair::New();
    view_ref_ = std::move(view_ref_pair.view_ref);
    fuchsia::ui::views::ViewRef view_ref_dup;
    zx_status_t status = view_ref_.reference.duplicate(ZX_RIGHT_SAME_RIGHTS,
                                                       &view_ref_dup.reference);
    ZX_CHECK(status == ZX_OK, status) << "zx_object_duplicate";

    auto view_tokens = scenic::ViewTokenPair::New();
    frame_->CreateViewWithViewRef(std::move(view_tokens.view_token),
                                  std::move(view_ref_pair.control_ref),
                                  std::move(view_ref_dup));
    base::RunLoop().RunUntilIdle();
    frame_impl_->window_tree_host_for_test()->Show();

    // Prepare the view for headless interaction by setting its focus state and
    // size.
    web_contents_ = frame_impl_->web_contents();
    content::RenderWidgetHostView* view =
        web_contents_->GetMainFrame()->GetView();
    view->SetBounds(kBounds);
    view->Focus();

    controller_->AwaitWatchAndRespondWith(false);

    ASSERT_EQ(base::GetKoid(controller_->view_ref().reference).value(),
              base::GetKoid(view_ref_.reference).value());
  }

  void TearDownOnMainThread() override {
    frame_.Unbind();
    base::RunLoop().RunUntilIdle();
  }

  gfx::Point GetCoordinatesOfInputField(base::StringPiece id) {
    // Distance to click from the top/left extents of an input field.
    constexpr int kInputFieldClickInset = 8;

    absl::optional<base::Value> result = cr_fuchsia::ExecuteJavaScript(
        frame_.get(),
        base::StringPrintf("getPointInsideText('%s')", id.data()));
    CHECK(result);

    // Note that coordinates are floating point and must be retrieved as such
    // from the Value, but we can cast them to integers and disregard the
    // fractional value with no major consequences.
    return gfx::Point(*result->FindDoublePath("x") + kInputFieldClickInset,
                      *result->FindDoublePath("y") + kInputFieldClickInset);
  }

 protected:
  // Used to publish fake virtual keyboard services for the InputMethod to use.
  absl::optional<base::TestComponentContextForProcess> component_context_;
  absl::optional<MockVirtualKeyboardControllerCreator> controller_creator_;
  std::unique_ptr<MockVirtualKeyboardController> controller_;

  fuchsia::web::FramePtr frame_;
  FrameImpl* frame_impl_ = nullptr;
  content::WebContents* web_contents_ = nullptr;
  cr_fuchsia::TestNavigationListener navigation_listener_;
  fuchsia::ui::views::ViewRef view_ref_;
};

// Verifies that RequestShow() is not called redundantly if the virtual
// keyboard is reported as visible.
IN_PROC_BROWSER_TEST_F(VirtualKeyboardTest, ShowAndHideWithVisibility) {
  testing::InSequence s;
  base::RunLoop on_show_run_loop;

  // Alphanumeric field click.
  EXPECT_CALL(*controller_, RequestShow())
      .WillOnce(testing::InvokeWithoutArgs(
          [&on_show_run_loop]() { on_show_run_loop.Quit(); }))
      .RetiresOnSaturation();
  EXPECT_CALL(*controller_, RequestHide()).RetiresOnSaturation();

  // Numeric field click.
  base::RunLoop click_numeric_run_loop;
  EXPECT_CALL(*controller_, SetTextType(virtualkeyboard::TextType::NUMERIC))
      .WillOnce(testing::InvokeWithoutArgs(
          [&click_numeric_run_loop]() { click_numeric_run_loop.Quit(); }))
      .RetiresOnSaturation();
  EXPECT_CALL(*controller_, RequestShow()).RetiresOnSaturation();

  // Input blur click.
  EXPECT_CALL(*controller_, RequestHide()).RetiresOnSaturation();
  base::RunLoop text_type_changed_run_loop;
  EXPECT_CALL(*controller_,
              SetTextType(virtualkeyboard::TextType::ALPHANUMERIC))
      .WillOnce(testing::InvokeWithoutArgs([&text_type_changed_run_loop]() {
        text_type_changed_run_loop.Quit();
      }))
      .RetiresOnSaturation();

  content::SimulateTapAt(web_contents_,
                         GetCoordinatesOfInputField(kInputFieldText));
  on_show_run_loop.Run();
  EXPECT_EQ(controller_->text_type(), virtualkeyboard::TextType::ALPHANUMERIC);

  // Indicate that the virtual keyboard is now visible.
  controller_->AwaitWatchAndRespondWith(true);
  base::RunLoop().RunUntilIdle();

  // Tap on another text field. RequestShow should not be called a second time
  // since the keyboard is already onscreen.
  content::SimulateTapAt(web_contents_,
                         GetCoordinatesOfInputField(kInputFieldNumeric));
  click_numeric_run_loop.Run();

  // Trigger input blur by clicking outside any input element.
  content::SimulateTapAt(web_contents_, kNoTarget);
  text_type_changed_run_loop.Run();
}

// Gives focus to a sequence of HTML <input> nodes with different InputModes,
// and verifies that the InputMode's FIDL equivalent is sent via SetTextType().
IN_PROC_BROWSER_TEST_F(VirtualKeyboardTest, InputModeMappings) {
  // Note that the service will elide type updates if there is no change,
  // so the array is ordered to produce an update on each entry.
  const std::vector<std::pair<base::StringPiece, virtualkeyboard::TextType>>
      kInputTypeMappings = {
          {kInputFieldTel, virtualkeyboard::TextType::PHONE},
          {kInputFieldSearch, virtualkeyboard::TextType::ALPHANUMERIC},
          {kInputFieldNumeric, virtualkeyboard::TextType::NUMERIC},
          {kInputFieldUrl, virtualkeyboard::TextType::ALPHANUMERIC},
          {kInputFieldDecimal, virtualkeyboard::TextType::NUMERIC},
          {kInputFieldEmail, virtualkeyboard::TextType::ALPHANUMERIC},
      };

  // GMock expectations must be set upfront, hence the redundant for-each loop.
  testing::InSequence s;
  virtualkeyboard::TextType previous_text_type =
      virtualkeyboard::TextType::ALPHANUMERIC;
  std::vector<base::RunLoop> set_type_loops(base::size(kInputTypeMappings));
  for (size_t i = 0; i < base::size(kInputTypeMappings); ++i) {
    const auto& field_type_pair = kInputTypeMappings[i];
    DCHECK_NE(field_type_pair.second, previous_text_type);

    EXPECT_CALL(*controller_, SetTextType(field_type_pair.second))
        .WillOnce(testing::InvokeWithoutArgs(
            [run_loop = &set_type_loops[i]]() mutable { run_loop->Quit(); }))
        .RetiresOnSaturation();
    previous_text_type = field_type_pair.second;
  }

  controller_->AwaitWatchAndRespondWith(false);

  for (size_t i = 0; i < base::size(kInputTypeMappings); ++i) {
    content::SimulateTapAt(
        web_contents_, GetCoordinatesOfInputField(kInputTypeMappings[i].first));

    // Spin the runloop until we've received the type update.
    set_type_loops[i].Run();
  }
}

IN_PROC_BROWSER_TEST_F(VirtualKeyboardTest, Disconnection) {
  testing::InSequence s;
  base::RunLoop on_show_run_loop;
  EXPECT_CALL(*controller_, RequestShow())
      .WillOnce(testing::InvokeWithoutArgs(
          [&on_show_run_loop]() { on_show_run_loop.Quit(); }));

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
                         GetCoordinatesOfInputField(kInputFieldNumeric));
  content::SimulateTapAt(web_contents_, kNoTarget);
}

}  // namespace
