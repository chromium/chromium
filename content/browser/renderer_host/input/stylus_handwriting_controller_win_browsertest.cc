// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/stylus_handwriting_controller_win.h"

#include "base/json/json_reader.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "components/stylus_handwriting/win/features.h"
#include "content/browser/renderer_host/input/mock_tfhandwriting.h"
#include "content/browser/renderer_host/input/stylus_handwriting_callback_sink_win.h"
#include "content/browser/renderer_host/input/stylus_handwriting_win_test_helper.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/win/stylus_handwriting_properties_win.h"
#include "ui/gfx/win/singleton_hwnd.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace content {

namespace {

constexpr uint32_t kHandwritingPointerId = 1;
constexpr uint64_t kHandwritingStrokeId = 1;

enum class APIError {
  kNone,
  kShellHandwritingDisabled,
  kQueryITfHandwriting,
  kQueryITfSource,
  kAdviseSink,
  kSetHandwritingState,
};

std::vector<APIError> GetAPIErrors() {
  return {APIError::kNone,
          APIError::kShellHandwritingDisabled,
          APIError::kQueryITfHandwriting,
          APIError::kQueryITfSource,
          APIError::kAdviseSink,
          APIError::kSetHandwritingState};
}

bool JSONToPoint(const std::string& str, gfx::PointF* point) {
  std::optional<base::Value> value = base::JSONReader::Read(str);
  if (!value) {
    return false;
  }

  const base::Value::Dict* root = value->GetIfDict();
  if (!root) {
    return false;
  }

  const std::optional<double> x = root->FindDouble("x");
  const std::optional<double> y = root->FindDouble("y");
  if (!x || !y) {
    return false;
  }

  point->set_x(*x);
  point->set_y(*y);
  return true;
}

void GiveItSomeTime() {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(100));
  run_loop.Run();
}

}  // namespace

class StylusHandwritingControllerWinTestBase : public ContentBrowserTest {
 protected:
  StylusHandwritingControllerWinTestBase() = default;
  StylusHandwritingControllerWinTestBase(
      const StylusHandwritingControllerWinTestBase&) = delete;
  StylusHandwritingControllerWinTestBase& operator=(
      const StylusHandwritingControllerWinTestBase&) = delete;
  ~StylusHandwritingControllerWinTestBase() override = default;

  void SetUp() override {
    // We cannot init the features in the constructor because GetAPIError()
    // might rely on the test parameters (from GetParams()), e.g, in
    // StylusHandwritingControllerWinCreationTest.
    if (GetAPIError() == APIError::kShellHandwritingDisabled) {
      scoped_feature_list_.InitAndDisableFeature(
          stylus_handwriting::win::kStylusHandwritingWin);
    } else {
      scoped_feature_list_.InitAndEnableFeature(
          stylus_handwriting::win::kStylusHandwritingWin);
    }
    ContentBrowserTest::SetUp();
  }

  void TearDown() override {
    ContentBrowserTest::TearDown();
    scoped_feature_list_.Reset();
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    stylus_handwriting_win_test_helper_.SetUpMockTfImpl();
    MockQueryInterfaceMethod();
    MockSetHandwritingStateMethod();
    MockAdviseSinkMethod();
    stylus_handwriting_win_test_helper_.SetUpStylusHandwritingControllerWin();
  }

  virtual APIError GetAPIError() const { return APIError::kNone; }

  MockTfImpl* mock_tf_impl() {
    return stylus_handwriting_win_test_helper_.mock_tf_impl();
  }

  StylusHandwritingControllerWin* controller() {
    auto* instance = StylusHandwritingControllerWin::GetInstance();
    EXPECT_TRUE(instance);
    return instance;
  }

 private:
  void MockQueryInterfaceMethod() {
    ON_CALL(*mock_tf_impl(), QueryInterface(Eq(__uuidof(::ITfHandwriting)), _))
        .WillByDefault(SetComPointeeAndReturnResult<1>(
            stylus_handwriting_win_test_helper_.GetTfHandwriting(),
            GetAPIError() == APIError::kQueryITfHandwriting ? E_NOINTERFACE
                                                            : S_OK));
    ON_CALL(*mock_tf_impl(), QueryInterface(Eq(__uuidof(ITfSource)), _))
        .WillByDefault(SetComPointeeAndReturnResult<1>(
            stylus_handwriting_win_test_helper_.GetTfSource(),
            GetAPIError() == APIError::kQueryITfSource ? E_NOINTERFACE : S_OK));
  }

  void MockSetHandwritingStateMethod() {
    ON_CALL(*mock_tf_impl(), SetHandwritingState(_))
        .WillByDefault(Return(
            GetAPIError() == APIError::kSetHandwritingState ? E_FAIL : S_OK));
  }

  void MockAdviseSinkMethod() {
    ON_CALL(*mock_tf_impl(), AdviseSink(_, _, _))
        .WillByDefault(SetValueParamAndReturnResult<2>(
            /*value=*/0,
            GetAPIError() == APIError::kAdviseSink ? E_FAIL : S_OK));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  StylusHandwritingWinTestHelper stylus_handwriting_win_test_helper_;
};

class StylusHandwritingControllerWinCreationTest
    : public StylusHandwritingControllerWinTestBase,
      public ::testing::WithParamInterface<APIError> {
 protected:
  StylusHandwritingControllerWinCreationTest() = default;
  StylusHandwritingControllerWinCreationTest(
      const StylusHandwritingControllerWinCreationTest&) = delete;
  StylusHandwritingControllerWinCreationTest& operator=(
      const StylusHandwritingControllerWinCreationTest&) = delete;
  ~StylusHandwritingControllerWinCreationTest() override = default;

  APIError GetAPIError() const final { return GetParam(); }
};

class StylusHandwritingControllerWinTest
    : public StylusHandwritingControllerWinTestBase {
 protected:
  StylusHandwritingControllerWinTest() = default;
  StylusHandwritingControllerWinTest(
      const StylusHandwritingControllerWinTest&) = delete;
  StylusHandwritingControllerWinTest& operator=(
      const StylusHandwritingControllerWinTest&) = delete;
  ~StylusHandwritingControllerWinTest() override = default;

  void SetUpOnMainThread() override {
    StylusHandwritingControllerWinTestBase::SetUpOnMainThread();
    SetUpMockTfHandwritingInstances();
    MockRequestHandwritingForPointerMethod();
  }

  void TearDownOnMainThread() override {
    ASSERT_EQ(mock_handwriting_request_.Reset(), 0U);
    ContentBrowserTest::TearDownOnMainThread();
  }

  void SetUpMockTfHandwritingInstances() {
    mock_handwriting_request_ =
        Microsoft::WRL::Make<MockTfHandwritingRequest>();
    mock_focus_handwriting_target_args_ =
        Microsoft::WRL::Make<MockTfFocusHandwritingTargetArgsImpl>();
  }

  void MockRequestHandwritingForPointerMethod() {
    ON_CALL(*mock_tf_impl(), RequestHandwritingForPointer(_, _, _, _))
        .WillByDefault(
            RequestHandwritingForPointerDefault(mock_handwriting_request()));
  }

  MockTfHandwritingRequest* mock_handwriting_request() {
    return mock_handwriting_request_.Get();
  }

  MockTfFocusHandwritingTargetArgsImpl* mock_focus_handwriting_target_args() {
    return mock_focus_handwriting_target_args_.Get();
  }

  // Starts the test server and navigates to the given url. Sets a large enough
  // size to the root window. Returns after the navigation to the url is
  // complete.
  void StartTestWithPage(const std::string& url) {
    ASSERT_TRUE(embedded_test_server()->Start());
    NavigateToURLAndWaitForMainFrame(embedded_test_server()->GetURL(url));
    aura::Window* content = shell()->web_contents()->GetContentNativeView();
    content->GetHost()->SetBoundsInPixels(gfx::Rect(800, 800));
    generator_ = std::make_unique<ui::test::EventGenerator>(
        GetRenderWidgetHostViewAura()->GetNativeView()->GetRootWindow());
  }

  ui::test::EventGenerator* GetEventGenerator() { return generator_.get(); }

  RenderWidgetHostViewAura* GetRenderWidgetHostViewAura() {
    return static_cast<RenderWidgetHostViewAura*>(
        shell()->web_contents()->GetRenderWidgetHostView());
  }

  // Navigate to |url| and wait until browser thread is synchronized with render
  // thread. It's needed so that the touch action is correctly initialized.
  void NavigateToURLAndWaitForMainFrame(const GURL& url) {
    ASSERT_TRUE(NavigateToURL(shell(), url));
    content::MainThreadFrameObserver frame_observer(
        GetRenderWidgetHostViewAura()->GetRenderWidgetHost());
    frame_observer.Wait();
  }

  void FocusEmptyTextField() const {
    ASSERT_TRUE(ExecJs(shell(), "focus_empty_text_field()"));
  }

  void FocusIneligibleTextField() const {
    ASSERT_TRUE(ExecJs(shell(), "focus_ineligible_text_field()"));
  }

  gfx::PointF GetPointInsideEmptyTextField() {
    gfx::PointF point;
    EXPECT_TRUE(JSONToPoint(
        EvalJs(shell(), "get_point_inside_empty_text_field()").ExtractString(),
        &point));
    return point;
  }

  gfx::PointF GetPointInsideIneligibleTextField() {
    gfx::PointF point;
    EXPECT_TRUE(
        JSONToPoint(EvalJs(shell(), "get_point_inside_ineligible_text_field()")
                        .ExtractString(),
                    &point));
    return point;
  }

  void ExecuteStroke(gfx::Point point) {
    GetEventGenerator()->SetProperties(
        ui::CreateEventPropertiesForTesting(ui::StylusHandwritingPropertiesWin(
            kHandwritingPointerId, kHandwritingStrokeId)));
    GetEventGenerator()->EnterPenPointerMode();
    GetEventGenerator()->delegate()->ConvertPointFromTarget(
        GetRenderWidgetHostViewAura()->GetNativeView(), &point);
    const gfx::Vector2d offset(0, 50);
    GetEventGenerator()->GestureScrollSequence(
        point, point + offset, base::Milliseconds(20), /*steps=*/10);
    GiveItSomeTime();
  }

  void ExpectStrokeCallSequence(int times_called) {
    InSequence s;
    EXPECT_CALL(*mock_tf_impl(),
                RequestHandwritingForPointer(kHandwritingPointerId,
                                             kHandwritingStrokeId, _, _))
        .Times(times_called);
    EXPECT_CALL(*mock_handwriting_request(), SetInputEvaluation(_))
        .Times(times_called);
  }

  HRESULT FocusHandwritingTarget(::ITfFocusHandwritingTargetArgs& args) {
    Microsoft::WRL::ComPtr<IUnknown> unknown_sink =
        controller()->GetCallbackSinkForTesting();
    if (!unknown_sink) {
      return E_UNEXPECTED;
    }

    Microsoft::WRL::ComPtr<::ITfHandwritingSink> callback_sink;
    HRESULT hr = unknown_sink.As(&callback_sink);
    if (FAILED(hr)) {
      return hr;
    }

    return callback_sink->FocusHandwritingTarget(&args);
  }

  void ExpectFocusHandwritingTargetCallSequence(
      const RECT& focus_rect,
      ::TfHandwritingFocusTargetResponse expected_response,
      base::RunLoop& run_loop) {
    InSequence s;
    EXPECT_CALL(*mock_focus_handwriting_target_args(),
                GetPointerTargetInfo(_, _, _))
        .WillOnce(
            DoAll(SetArgPointee<0>(gfx::SingletonHwnd::GetInstance()->hwnd()),
                  SetArgPointee<1>(focus_rect), SetArgPointee<2>(SIZE(0, 0)),
                  Return(S_OK)));
    EXPECT_CALL(*mock_focus_handwriting_target_args(),
                SetResponse(expected_response))
        .WillOnce(DoAll(base::test::RunOnceClosure(run_loop.QuitClosure()),
                        Return(S_OK)));
  }

 private:
  Microsoft::WRL::ComPtr<MockTfHandwritingRequest> mock_handwriting_request_;
  Microsoft::WRL::ComPtr<MockTfFocusHandwritingTargetArgsImpl>
      mock_focus_handwriting_target_args_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         StylusHandwritingControllerWinCreationTest,
                         ::testing::ValuesIn(GetAPIErrors()));

IN_PROC_BROWSER_TEST_P(StylusHandwritingControllerWinCreationTest,
                       APIAvailability) {
  EXPECT_EQ(StylusHandwritingControllerWin::IsHandwritingAPIAvailable(),
            GetParam() == APIError::kNone);
}

// Tests that using a pen and performing a stroke in a textfield notifies
// Shell Handwriting API of a stroke.
IN_PROC_BROWSER_TEST_F(StylusHandwritingControllerWinTest, BasicStroke) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/stylus_handwriting_test.html"));
  const gfx::Point point = gfx::ToRoundedPoint(GetPointInsideEmptyTextField());
  FocusEmptyTextField();
  ExpectStrokeCallSequence(1);
  ExecuteStroke(point);
}

// Tests that using a pen and performing a stroke in a textfield without focus,
// does notify the Shell Handwriting API of a stroke.
IN_PROC_BROWSER_TEST_F(StylusHandwritingControllerWinTest, BasicStrokeNoFocus) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/stylus_handwriting_test.html"));
  const gfx::Point point = gfx::ToRoundedPoint(GetPointInsideEmptyTextField());
  ExpectStrokeCallSequence(1);
  ExecuteStroke(point);
}

// Tests that using a pen and performing a stroke in an ineligible textfield
// does not notify the Shell Handwriting API of a stroke.
IN_PROC_BROWSER_TEST_F(StylusHandwritingControllerWinTest,
                       BasicStrokeIneligibleField) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/stylus_handwriting_test.html"));
  const gfx::Point point =
      gfx::ToRoundedPoint(GetPointInsideIneligibleTextField());
  FocusIneligibleTextField();
  ExpectStrokeCallSequence(0);
  ExecuteStroke(point);
}

// Performs a stroke and then calls
// TfHandwritingCallbackSink::FocusHandwritingTarget. The goal of this test is
// to ensure that, based on the coordinates passed in to FocusHandwritingTarget,
// the correct response is set on ITfFocusHandwritingTargetArgs. The point
// passed in to FocusHandwritingTarget is the same as that of the textfield
// where the stroke was initiated. Therefore the expected response is
// TF_HANDWRITING_TARGET_FOCUSED.
IN_PROC_BROWSER_TEST_F(StylusHandwritingControllerWinTest,
                       FocusHandwritingOnEligibleTarget) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/stylus_handwriting_test.html"));
  gfx::Point point = gfx::ToRoundedPoint(GetPointInsideEmptyTextField());
  ExpectStrokeCallSequence(1);
  ExecuteStroke(point);

  GetEventGenerator()->delegate()->ConvertPointFromTarget(
      GetRenderWidgetHostViewAura()->GetNativeView(), &point);
  RECT rect(point.x(), point.y() - 2, point.x() + 2, point.y() + 100);

  base::RunLoop run_loop;
  ExpectFocusHandwritingTargetCallSequence(
      rect, ::TF_HANDWRITING_TARGET_FOCUSED, run_loop);
  EXPECT_HRESULT_SUCCEEDED(
      FocusHandwritingTarget(*mock_focus_handwriting_target_args()));
  run_loop.Run();
}

// Performs a stroke and then calls ::FocusHandwritingTarget(). The test
// simulates the case where the stroke is long and the center point of the
// target rect is located outside the eligible editable element. In this case,
// blink should fall back to the original touch down position to determine the
// target and set the focus on the element.
IN_PROC_BROWSER_TEST_F(StylusHandwritingControllerWinTest,
                       FocusHandwritingOutsideEligibleTarget) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/stylus_handwriting_test.html"));
  gfx::Point point = gfx::ToRoundedPoint(GetPointInsideEmptyTextField());
  ExpectStrokeCallSequence(1);
  ExecuteStroke(point);

  GetEventGenerator()->delegate()->ConvertPointFromTarget(
      GetRenderWidgetHostViewAura()->GetNativeView(), &point);
  RECT rect(point.x(), point.y() - 2, point.x() + 2, point.y() + 800);

  base::RunLoop run_loop;
  // TODO(crbug.com/355578906): Fix the test to expect
  // TF_HANDWRITING_TARGET_FOCUSED when the functionality is implemented.
  ExpectFocusHandwritingTargetCallSequence(rect, ::TF_NO_HANDWRITING_TARGET,
                                           run_loop);
  EXPECT_HRESULT_SUCCEEDED(
      FocusHandwritingTarget(*mock_focus_handwriting_target_args()));
  run_loop.Run();
}

// Performs a stroke and then calls ::FocusHandwritingTarget(). The test
// simulates the case where the target element eligibility has changed after the
// stroke was initiated which should prevent setting the focus on the element.
IN_PROC_BROWSER_TEST_F(StylusHandwritingControllerWinTest,
                       FocusHandwritingOnUnsupportedTarget) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/stylus_handwriting_test.html"));
  gfx::Point point = gfx::ToRoundedPoint(GetPointInsideEmptyTextField());
  ExpectStrokeCallSequence(1);
  ExecuteStroke(point);

  EXPECT_TRUE(ExecJs(shell(), "disable_handwriting_for_empty_textfield()"));

  GetEventGenerator()->delegate()->ConvertPointFromTarget(
      GetRenderWidgetHostViewAura()->GetNativeView(), &point);
  RECT rect(point.x(), point.y() - 2, point.x() + 2, point.y() + 100);

  base::RunLoop run_loop;
  ExpectFocusHandwritingTargetCallSequence(rect, ::TF_NO_HANDWRITING_TARGET,
                                           run_loop);
  EXPECT_HRESULT_SUCCEEDED(
      FocusHandwritingTarget(*mock_focus_handwriting_target_args()));
  run_loop.Run();
}

}  // namespace content
