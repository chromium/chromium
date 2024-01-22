// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/captured_surface_controller.h"

#include <list>
#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "content/browser/media/captured_surface_control_permission_manager.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/test/browser_task_environment.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace content {
namespace {

using CapturedWheelAction = ::blink::mojom::CapturedWheelAction;
using CapturedWheelActionPtr = ::blink::mojom::CapturedWheelActionPtr;
using CSCResult = ::blink::mojom::CapturedSurfaceControlResult;
using CSCPermissionResult =
    ::content::CapturedSurfaceControlPermissionManager::PermissionResult;

// Make an arbitrary valid CapturedWheelAction.
CapturedWheelActionPtr MakeCapturedWheelActionPtr() {
  return CapturedWheelAction::New(
      /*x=*/0,
      /*y=*/0,
      /*wheel_delta_x=*/0,
      /*wheel_delta_y=*/0);
}

class InputObserver : public RenderWidgetHost::InputEventObserver {
 public:
  struct ExpectedWheelEvent {
    double x = 0;
    double y = 0;
    double delta_x = 0;
    double delta_y = 0;
  };

  ~InputObserver() override { EXPECT_TRUE(expected_events_.empty()); }

  void OnInputEvent(const blink::WebInputEvent& event) override {
    CHECK_EQ(event.GetType(), blink::WebInputEvent::Type::kMouseWheel);

    const blink::WebMouseWheelEvent& wheel_event =
        static_cast<const blink::WebMouseWheelEvent&>(event);

    CHECK(!expected_events_.empty());
    ExpectedWheelEvent expected_event = expected_events_.front();
    expected_events_.pop_front();

    EXPECT_EQ(expected_event.x, wheel_event.PositionInWidget().x());
    EXPECT_EQ(expected_event.y, wheel_event.PositionInWidget().y());
    EXPECT_EQ(expected_event.delta_x, wheel_event.delta_x);
    EXPECT_EQ(expected_event.delta_y, wheel_event.delta_y);
  }

  void AddExpectation(ExpectedWheelEvent expected_event) {
    expected_events_.push_back(expected_event);

    // The wheel event chains are closed with a scroll of zero
    // magnitude in the same location.
    expected_events_.push_back(ExpectedWheelEvent{.x = expected_event.x,
                                                  .y = expected_event.y,
                                                  .delta_x = 0,
                                                  .delta_y = 0});
  }

 private:
  std::list<ExpectedWheelEvent> expected_events_;
};

class TestView : public TestRenderWidgetHostView {
 public:
  explicit TestView(RenderWidgetHostImpl* rwhi)
      : TestRenderWidgetHostView(rwhi) {}
  ~TestView() override = default;

  void SetSize(const gfx::Size& size) override { size_ = size; }

  gfx::Size GetVisibleViewportSize() override { return size_; }

 private:
  gfx::Size size_;
};

class MockCapturedSurfaceControlPermissionManager
    : public CapturedSurfaceControlPermissionManager {
 public:
  MockCapturedSurfaceControlPermissionManager(
      GlobalRenderFrameHostId capturer_rfh_id)
      : CapturedSurfaceControlPermissionManager(capturer_rfh_id) {}
  ~MockCapturedSurfaceControlPermissionManager() override = default;

  void SetPermissionResult(CSCPermissionResult result) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    result_ = result;
  }

  void CheckPermission(
      base::OnceCallback<void(CSCPermissionResult)> callback) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    CHECK(result_.has_value());
    std::move(callback).Run(result_.value());
  }

 private:
  std::optional<CSCPermissionResult> result_;
};

using MockPermissionManager = MockCapturedSurfaceControlPermissionManager;

// Make a callback that expects `result` and then unblock `run_loop`.
base::OnceCallback<void(CSCResult)> MakeCallbackExpectingResult(
    base::RunLoop* run_loop,
    CSCResult expected_result) {
  return base::BindOnce(
      [](base::RunLoop* run_loop, CSCResult expected_result, CSCResult result) {
        EXPECT_EQ(result, expected_result);
        run_loop->Quit();
      },
      run_loop, expected_result);
}

// Equivalent to MakeCallbackExpectingResult, but for GetZoomLevel().
base::OnceCallback<void(std::optional<int>, CSCResult)>
MakeGetZoomCallbackExpectingResult(base::RunLoop* run_loop,
                                   CSCResult expected_result) {
  return base::BindOnce(
      [](base::RunLoop* run_loop, CSCResult expected_result,
         std::optional<int> zoom_level, CSCResult result) {
        EXPECT_EQ(result, expected_result);
        // `zoom_level` intentionally ignored.
        run_loop->Quit();
      },
      run_loop, expected_result);
}

// Make a callback that expects `result` and then unblock `run_loop`.
base::OnceCallback<void(std::optional<int>, CSCResult)>
MakeGetZoomLevelCallbackExpectingResult(base::RunLoop* run_loop,
                                        std::optional<int> expected_zoom_level,
                                        CSCResult expected_result) {
  return base::BindOnce(
      [](base::RunLoop* run_loop, std::optional<int> expected_zoom_level,
         CSCResult expected_result, std::optional<int> zoom_level,
         CSCResult result) {
        EXPECT_EQ(zoom_level, expected_zoom_level);
        EXPECT_EQ(result, expected_result);
        run_loop->Quit();
      },
      run_loop, expected_zoom_level, expected_result);
}

class CapturedSurfaceControllerTestBase : public RenderViewHostTestHarness {
 public:
  static constexpr gfx::Size kCapturedViewportSize = gfx::Size(100, 400);

  ~CapturedSurfaceControllerTestBase() override = default;

  std::unique_ptr<TestWebContents> MakeTestWebContents() {
    scoped_refptr<SiteInstance> instance =
        SiteInstance::Create(GetBrowserContext());
    instance->GetProcess()->Init();
    return TestWebContents::Create(GetBrowserContext(), std::move(instance));
  }

  RenderWidgetHostImpl* GetRenderWidgetHostImpl(WebContents& wc) {
    return RenderWidgetHostImpl::From(
        wc.GetPrimaryMainFrame()->GetRenderWidgetHost());
  }

  RenderWidgetHostViewBase* GetView(WebContents& wc) {
    return GetRenderWidgetHostImpl(wc)->GetView();
  }

  void SetView(WebContents& wc, RenderWidgetHostViewBase* rwhv) {
    GetRenderWidgetHostImpl(wc)->SetView(rwhv);
  }

  WebContentsMediaCaptureId GetWebContentsMediaCaptureId(
      WebContents& wc) const {
    RenderFrameHost* const captured_main_rfh = wc.GetPrimaryMainFrame();
    return WebContentsMediaCaptureId(captured_main_rfh->GetProcess()->GetID(),
                                     captured_main_rfh->GetRoutingID());
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    // MainSetUp() is in a helper so that test fixtures that inherit from the
    // current one would be able to directly run it without also starting
    // capture and/or running AwaitWebContentsResolution().
    MainSetUp();
    StartCaptureOf(*captured_wc_);
    AwaitWebContentsResolution();
  }

  void MainSetUp() {
    capturing_wc_ = MakeTestWebContents();
    old_capturing_rwhv_ = GetView(*capturing_wc_);
    capturing_rwhv_ =
        std::make_unique<TestView>(GetRenderWidgetHostImpl(*capturing_wc_));
    SetView(*capturing_wc_, capturing_rwhv_.get());
    capturing_rwhv_->SetSize(kCapturedViewportSize);

    captured_wc_ = MakeTestWebContents();
    old_captured_rwhv_ = GetView(*captured_wc_);
    captured_rwhv_ =
        std::make_unique<TestView>(GetRenderWidgetHostImpl(*captured_wc_));
    SetView(*captured_wc_, captured_rwhv_.get());
    captured_rwhv_->SetSize(kCapturedViewportSize);
  }

  void StartCaptureOf(WebContents& captured_wc) {
    auto permission_manager = std::make_unique<MockPermissionManager>(
        capturing_wc_->GetPrimaryMainFrame()->GetGlobalId());
    permission_manager_ = permission_manager.get();

    // `base::Unretained(this)` is safe because `this` owns `controller_` and
    // therefore has a longer lifetime.
    controller_ = CapturedSurfaceController::CreateForTesting(
        capturing_wc_->GetPrimaryMainFrame()->GetGlobalId(),
        GetWebContentsMediaCaptureId(captured_wc),
        std::move(permission_manager),
        base::BindRepeating(
            &CapturedSurfaceControllerTestBase::OnWebContentsResolved,
            base::Unretained(this)));
  }

  void UnloadCapturingWebContents() {
    if (!capturing_wc_) {
      return;
    }

    SetView(*capturing_wc_, old_capturing_rwhv_);
    old_capturing_rwhv_ = nullptr;

    capturing_wc_.reset();
    capturing_rwhv_.reset();
  }

  void UnloadCapturedWebContents() {
    if (!captured_wc_) {
      return;
    }

    SetView(*captured_wc_, old_captured_rwhv_);
    old_captured_rwhv_ = nullptr;

    captured_wc_.reset();
    captured_rwhv_.reset();
  }

  void TearDown() override {
    permission_manager_ = nullptr;
    controller_.reset();
    UnloadCapturingWebContents();
    UnloadCapturedWebContents();

    RenderViewHostTestHarness::TearDown();
  }

  void AwaitWebContentsResolution() {
    CHECK(!wc_resolution_run_loop_);
    wc_resolution_run_loop_ = std::make_unique<base::RunLoop>();
    wc_resolution_run_loop_->Run();
    wc_resolution_run_loop_.reset();
  }

  void OnWebContentsResolved() {
    if (wc_resolution_run_loop_) {
      wc_resolution_run_loop_->Quit();
    }
  }

 protected:
  std::unique_ptr<CapturedSurfaceController> controller_;
  raw_ptr<MockPermissionManager> permission_manager_ = nullptr;

  std::unique_ptr<TestWebContents> capturing_wc_;
  std::unique_ptr<TestView> capturing_rwhv_;
  raw_ptr<RenderWidgetHostViewBase> old_capturing_rwhv_ = nullptr;

  std::unique_ptr<TestWebContents> captured_wc_;
  std::unique_ptr<TestView> captured_rwhv_;
  raw_ptr<RenderWidgetHostViewBase> old_captured_rwhv_ = nullptr;

  std::unique_ptr<base::RunLoop> wc_resolution_run_loop_;
};

class CapturedSurfaceControllerSendWheelTest
    : public CapturedSurfaceControllerTestBase {
 public:
  ~CapturedSurfaceControllerSendWheelTest() override = default;

  void SetUp() override {
    CapturedSurfaceControllerTestBase::SetUp();

    input_observer_ = std::make_unique<InputObserver>();
    GetRenderWidgetHostImpl(*captured_wc_)
        ->AddInputEventObserver(input_observer_.get());
  }

  void TearDown() override {
    GetRenderWidgetHostImpl(*captured_wc_)
        ->RemoveInputEventObserver(input_observer_.get());

    CapturedSurfaceControllerTestBase::TearDown();
  }

 protected:
  std::unique_ptr<InputObserver> input_observer_;
};

TEST_F(CapturedSurfaceControllerSendWheelTest, CorrectScaling) {
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);
  captured_rwhv_->SetSize(gfx::Size(256, 4096));
  base::RunLoop run_loop;
  input_observer_->AddExpectation(InputObserver::ExpectedWheelEvent{
      .x = 256 * 0.25, .y = 4096 * 0.5, .delta_x = 300, .delta_y = 400});
  controller_->SendWheel(
      CapturedWheelAction::New(
          /*x=*/0.25,
          /*y=*/0.5,
          /*wheel_delta_x=*/300,
          /*wheel_delta_y=*/400),
      MakeCallbackExpectingResult(&run_loop, CSCResult::kSuccess));
  run_loop.Run();
}

TEST_F(CapturedSurfaceControllerSendWheelTest,
       GracefullyHandleZeroWidthCapturedSurface) {
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);
  captured_rwhv_->SetSize(gfx::Size(0, 4096));
  base::RunLoop run_loop;
  // Note absence of call to input_observer_->AddExpectation().
  controller_->SendWheel(
      CapturedWheelAction::New(
          /*x=*/0.25,
          /*y=*/0.5,
          /*wheel_delta_x=*/300,
          /*wheel_delta_y=*/400),
      MakeCallbackExpectingResult(&run_loop, CSCResult::kUnknownError));
  run_loop.Run();
}

TEST_F(CapturedSurfaceControllerSendWheelTest,
       GracefullyHandleZeroHeightCapturedSurface) {
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);
  captured_rwhv_->SetSize(gfx::Size(256, 0));
  base::RunLoop run_loop;
  // Note absence of call to input_observer_->AddExpectation().
  controller_->SendWheel(
      CapturedWheelAction::New(
          /*x=*/0.25,
          /*y=*/0.5,
          /*wheel_delta_x=*/300,
          /*wheel_delta_y=*/400),
      MakeCallbackExpectingResult(&run_loop, CSCResult::kUnknownError));
  run_loop.Run();
}

TEST_F(CapturedSurfaceControllerSendWheelTest,
       GracefullyHandleExtremelyNarrowCapturedSurface) {
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);
  captured_rwhv_->SetSize(gfx::Size(1, 4096));
  base::RunLoop run_loop;
  input_observer_->AddExpectation(InputObserver::ExpectedWheelEvent{
      .x = 0, .y = 4096 * 0.5, .delta_x = 300, .delta_y = 400});
  controller_->SendWheel(
      CapturedWheelAction::New(
          /*x=*/0.25,
          /*y=*/0.5,
          /*wheel_delta_x=*/300,
          /*wheel_delta_y=*/400),
      MakeCallbackExpectingResult(&run_loop, CSCResult::kSuccess));
  run_loop.Run();
}

TEST_F(CapturedSurfaceControllerSendWheelTest,
       GracefullyHandleExtremelyShortCapturedSurface) {
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);
  captured_rwhv_->SetSize(gfx::Size(256, 1));
  base::RunLoop run_loop;
  input_observer_->AddExpectation(InputObserver::ExpectedWheelEvent{
      .x = 256 * 0.25, .y = 0, .delta_x = 300, .delta_y = 400});
  controller_->SendWheel(
      CapturedWheelAction::New(
          /*x=*/0.25,
          /*y=*/0.5,
          /*wheel_delta_x=*/300,
          /*wheel_delta_y=*/400),
      MakeCallbackExpectingResult(&run_loop, CSCResult::kSuccess));
  run_loop.Run();
}

// TODO(crbug.com/1466247): Remove this test suite after the getZoomLevel() API
// is made synchronous.
class CapturedSurfaceControllerGetZoomLevelTest
    : public CapturedSurfaceControllerTestBase {
 public:
  ~CapturedSurfaceControllerGetZoomLevelTest() override = default;
};

TEST_F(CapturedSurfaceControllerGetZoomLevelTest, GetZoomLevelSuccess) {
  content::HostZoomMap::SetZoomLevel(captured_wc_.get(),
                                     blink::PageZoomFactorToZoomLevel(0.9));
  base::RunLoop run_loop;
  controller_->GetZoomLevel(MakeGetZoomLevelCallbackExpectingResult(
      &run_loop, 90, CSCResult::kSuccess));
  run_loop.Run();
}

TEST_F(CapturedSurfaceControllerGetZoomLevelTest, GetZoomLevelUnknownError) {
  base::RunLoop run_loop;
  UnloadCapturedWebContents();
  controller_->GetZoomLevel(MakeGetZoomLevelCallbackExpectingResult(
      &run_loop, std::nullopt, CSCResult::kCapturedSurfaceNotFoundError));
  run_loop.Run();
}

class CapturedSurfaceControllerSetZoomLevelTest
    : public CapturedSurfaceControllerTestBase,
      public ::testing::WithParamInterface<int> {
 public:
  CapturedSurfaceControllerSetZoomLevelTest() : zoom_level_(GetParam()) {}
  const int zoom_level_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    CapturedSurfaceControllerSetZoomLevelTest,
    ::testing::Values(
        static_cast<int>(std::ceil(100 * blink::kMinimumPageZoomFactor)),
        static_cast<int>(std::floor(100 * blink::kMaximumPageZoomFactor))));

TEST_P(CapturedSurfaceControllerSetZoomLevelTest, SetZoomLevelSuccess) {
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);
  base::RunLoop run_loop;
  controller_->SetZoomLevel(
      zoom_level_, MakeCallbackExpectingResult(&run_loop, CSCResult::kSuccess));
  run_loop.Run();

  EXPECT_EQ(zoom_level_,
            std::round(100 * blink::PageZoomLevelToZoomFactor(
                                 content::HostZoomMap::GetZoomLevel(
                                     captured_wc_.get()))));
}

enum class CapturedSurfaceControlAPI {
  kSendWheel,
  kSetZoomLevel,
  // TODO(crbug.com/1466247): Remove kGetZoomLevel after making that API sync.
  kGetZoomLevel,
};

class CapturedSurfaceControllerInterfaceTestBase
    : public CapturedSurfaceControllerTestBase {
 public:
  CapturedSurfaceControllerInterfaceTestBase(
      CapturedSurfaceControlAPI tested_interface)
      : tested_interface_(tested_interface) {}
  ~CapturedSurfaceControllerInterfaceTestBase() override = default;

  void RunTestedActionAndExpect(base::RunLoop* run_loop,
                                CSCResult expected_result) {
    switch (tested_interface_) {
      case CapturedSurfaceControlAPI::kSendWheel:
        controller_->SendWheel(
            MakeCapturedWheelActionPtr(),
            MakeCallbackExpectingResult(run_loop, expected_result));
        return;
      case CapturedSurfaceControlAPI::kSetZoomLevel:
        controller_->SetZoomLevel(
            /*zoom_level=*/100,
            MakeCallbackExpectingResult(run_loop, expected_result));
        return;
      case CapturedSurfaceControlAPI::kGetZoomLevel:
        controller_->GetZoomLevel(
            MakeGetZoomCallbackExpectingResult(run_loop, expected_result));
        return;
    }
    NOTREACHED_NORETURN();
  }

 protected:
  const CapturedSurfaceControlAPI tested_interface_;
};

class CapturedSurfaceControllerInterfaceTest
    : public CapturedSurfaceControllerInterfaceTestBase,
      public ::testing::WithParamInterface<CapturedSurfaceControlAPI> {
 public:
  CapturedSurfaceControllerInterfaceTest()
      : CapturedSurfaceControllerInterfaceTestBase(GetParam()) {}

  ~CapturedSurfaceControllerInterfaceTest() override = default;
};

// Note that kGetZoomLevel is not tested because it's currently allowed
// without requiring permissions, and that is by design.
// TODO(crbug.com/1466247): Remove above comment after making that API sync.
INSTANTIATE_TEST_SUITE_P(
    ,
    CapturedSurfaceControllerInterfaceTest,
    ::testing::Values(CapturedSurfaceControlAPI::kSendWheel,
                      CapturedSurfaceControlAPI::kSetZoomLevel));

TEST_P(CapturedSurfaceControllerInterfaceTest, SuccessReportedIfPermitted) {
  base::RunLoop run_loop;
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);
  RunTestedActionAndExpect(&run_loop, CSCResult::kSuccess);
  run_loop.Run();
}

TEST_P(CapturedSurfaceControllerInterfaceTest, NoPermissionReportedIfDenied) {
  base::RunLoop run_loop;
  permission_manager_->SetPermissionResult(CSCPermissionResult::kDenied);
  RunTestedActionAndExpect(&run_loop, CSCResult::kNoPermissionError);
  run_loop.Run();
}

TEST_P(CapturedSurfaceControllerInterfaceTest,
       UnknownErrorReportedIfPermissionError) {
  base::RunLoop run_loop;
  permission_manager_->SetPermissionResult(CSCPermissionResult::kError);
  RunTestedActionAndExpect(&run_loop, CSCResult::kUnknownError);
  run_loop.Run();
}

// Simulate the captured tab being closed after permission is granted but before
// the controller has time to process the response from the permission manager.
TEST_P(CapturedSurfaceControllerInterfaceTest,
       SurfaceNotFoundReportedIfTabClosedBeforePromptResponseHandled) {
  base::RunLoop run_loop;
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);
  UnloadCapturedWebContents();
  RunTestedActionAndExpect(&run_loop, CSCResult::kCapturedSurfaceNotFoundError);
  run_loop.Run();
}

TEST_P(CapturedSurfaceControllerInterfaceTest,
       SurfaceNotFoundReportedIfCaptureTargetUpdatedToNonTabSurface) {
  base::RunLoop run_loop;
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);
  controller_->UpdateCaptureTarget(WebContentsMediaCaptureId());
  RunTestedActionAndExpect(&run_loop, CSCResult::kCapturedSurfaceNotFoundError);
  run_loop.Run();
}

// Test suite ensuring that API calls before/after the WebContents ID is
// resolved to a base::WeakPtr<WebContents> behave as expected.
class CapturedSurfaceControllerWebContentsResolutionTest
    : public CapturedSurfaceControllerInterfaceTestBase,
      public ::testing::WithParamInterface<CapturedSurfaceControlAPI> {
 public:
  CapturedSurfaceControllerWebContentsResolutionTest()
      : CapturedSurfaceControllerInterfaceTestBase(GetParam()) {}

  ~CapturedSurfaceControllerWebContentsResolutionTest() override = default;

  void SetUp() override {
    // Intentionally skip CapturedSurfaceControllerInterfaceTestBase's SetUp(),
    // and therefore also CapturedSurfaceControllerTestBase's SetUp().
    RenderViewHostTestHarness::SetUp();
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    CapturedSurfaceControllerWebContentsResolutionTest,
    ::testing::Values(CapturedSurfaceControlAPI::kSendWheel,
                      CapturedSurfaceControlAPI::kSetZoomLevel,
                      CapturedSurfaceControlAPI::kGetZoomLevel));

TEST_P(CapturedSurfaceControllerWebContentsResolutionTest,
       ApiInvocationAfterWebContentsResolutionSucceeds) {
  MainSetUp();  // Triggers resolution but does not await it.
  StartCaptureOf(*captured_wc_);
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);

  AwaitWebContentsResolution();

  base::RunLoop run_loop;
  RunTestedActionAndExpect(&run_loop, CSCResult::kSuccess);
  run_loop.Run();
}

TEST_P(CapturedSurfaceControllerWebContentsResolutionTest,
       ApiInvocationPriorToWebContentsResolutionFails) {
  MainSetUp();  // Triggers resolution but does not await it.
  StartCaptureOf(*captured_wc_);
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);

  base::RunLoop run_loop;
  RunTestedActionAndExpect(&run_loop, CSCResult::kCapturedSurfaceNotFoundError);
  run_loop.Run();

  AwaitWebContentsResolution();
}

TEST_P(
    CapturedSurfaceControllerWebContentsResolutionTest,
    ApiInvocationPriorToWebContentsResolutionFailsButSubsequentCallsAreNotBlocked) {
  // Setup - repeat ApiInvocationPriorToWebContentsResolutionFails.
  {
    MainSetUp();  // Triggers resolution but does not await it.
    StartCaptureOf(*captured_wc_);
    permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);

    base::RunLoop run_loop;
    RunTestedActionAndExpect(&run_loop,
                             CSCResult::kCapturedSurfaceNotFoundError);
    run_loop.Run();

    AwaitWebContentsResolution();
  }

  // After AwaitWebContentsResolution() is called, subsequent API calls succeed.
  base::RunLoop run_loop;
  RunTestedActionAndExpect(&run_loop, CSCResult::kSuccess);
  run_loop.Run();
}

// Similar to CapturedSurfaceControllerWebContentsResolutionTest,
// but focuses on calls to UpdateCaptureTarget(), which also trigger resolution.
class CapturedSurfaceControllerWebContentsResolutionOfUpdatesTest
    : public CapturedSurfaceControllerInterfaceTestBase,
      public ::testing::WithParamInterface<CapturedSurfaceControlAPI> {
 public:
  CapturedSurfaceControllerWebContentsResolutionOfUpdatesTest()
      : CapturedSurfaceControllerInterfaceTestBase(GetParam()) {}

  ~CapturedSurfaceControllerWebContentsResolutionOfUpdatesTest() override =
      default;

  void SetUp() override {
    // Unlike CapturedSurfaceControllerWebContentsResolutionTest, the current
    // test works well with the parent's SetUp(), which awaits the resolution
    // of the *first* ID. This is due to the current test's focus on what
    // happens before/after the call to UpdateCaptureTarget().
    CapturedSurfaceControllerInterfaceTestBase::SetUp();

    permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);

    // Prepare a new tab to capture instead of the original one.
    newly_captured_wc_ = MakeTestWebContents();
    RenderFrameHost* const captured_main_rfh =
        newly_captured_wc_->GetPrimaryMainFrame();
    newly_captured_wc_id_ =
        WebContentsMediaCaptureId(captured_main_rfh->GetProcess()->GetID(),
                                  captured_main_rfh->GetRoutingID());

    // Set up a RenderWidgetHostImpl with a custom size. This is needed by
    // SendWheel(), or else it would error out, producing false-positives and
    // false-negatives in the tests.
    newly_captured_wc_original_rwhv_ = GetView(*newly_captured_wc_);
    newly_captured_rwhv_ = std::make_unique<TestView>(
        GetRenderWidgetHostImpl(*newly_captured_wc_));
    SetView(*newly_captured_wc_, newly_captured_rwhv_.get());
    newly_captured_rwhv_->SetSize(kCapturedViewportSize);
  }

  void TearDown() override {
    SetView(*newly_captured_wc_, newly_captured_wc_original_rwhv_);
    newly_captured_wc_original_rwhv_ = nullptr;

    newly_captured_rwhv_.reset();
    newly_captured_wc_.reset();

    CapturedSurfaceControllerInterfaceTestBase::TearDown();
  }

 protected:
  std::unique_ptr<TestWebContents> newly_captured_wc_;
  std::unique_ptr<TestView> newly_captured_rwhv_;
  raw_ptr<RenderWidgetHostViewBase> newly_captured_wc_original_rwhv_ = nullptr;
  WebContentsMediaCaptureId newly_captured_wc_id_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    CapturedSurfaceControllerWebContentsResolutionOfUpdatesTest,
    ::testing::Values(CapturedSurfaceControlAPI::kSendWheel,
                      CapturedSurfaceControlAPI::kSetZoomLevel,
                      CapturedSurfaceControlAPI::kGetZoomLevel));

TEST_P(
    CapturedSurfaceControllerWebContentsResolutionOfUpdatesTest,
    AfterUpdateCaptureTargetApiInvocationAfterToWebContentsResolutionSucceeds) {
  // Call UpdateCaptureTarget() - capturing a new tab.
  controller_->UpdateCaptureTarget(newly_captured_wc_id_);
  AwaitWebContentsResolution();

  base::RunLoop run_loop;
  RunTestedActionAndExpect(&run_loop, CSCResult::kSuccess);
  run_loop.Run();
}

TEST_P(CapturedSurfaceControllerWebContentsResolutionOfUpdatesTest,
       AfterUpdateCaptureTargetApiInvocationPriorToWebContentsResolutionFails) {
  // Call UpdateCaptureTarget() - capturing a new tab.
  controller_->UpdateCaptureTarget(newly_captured_wc_id_);
  // Note absence of call to AwaitWebContentsResolution().

  base::RunLoop run_loop;
  RunTestedActionAndExpect(&run_loop, CSCResult::kCapturedSurfaceNotFoundError);
  run_loop.Run();

  AwaitWebContentsResolution();
}

// Test suite ensuring that API calls before/after the WebContents ID is
// resolved to a base::WeakPtr<WebContents> behave as expected.
class CapturedSurfaceControllerSelfCaptureTest
    : public CapturedSurfaceControllerInterfaceTestBase,
      public ::testing::WithParamInterface<CapturedSurfaceControlAPI> {
 public:
  CapturedSurfaceControllerSelfCaptureTest()
      : CapturedSurfaceControllerInterfaceTestBase(GetParam()) {}

  ~CapturedSurfaceControllerSelfCaptureTest() override = default;

  void SetUp() override {
    // Intentionally skip CapturedSurfaceControllerInterfaceTestBase's SetUp(),
    // and therefore also CapturedSurfaceControllerTestBase's SetUp().
    RenderViewHostTestHarness::SetUp();
    MainSetUp();
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    CapturedSurfaceControllerSelfCaptureTest,
    ::testing::Values(CapturedSurfaceControlAPI::kSendWheel,
                      CapturedSurfaceControlAPI::kSetZoomLevel,
                      CapturedSurfaceControlAPI::kGetZoomLevel));

TEST_P(CapturedSurfaceControllerSelfCaptureTest, SelfCaptureDisallowed) {
  StartCaptureOf(*capturing_wc_);
  AwaitWebContentsResolution();
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);

  base::RunLoop run_loop;
  RunTestedActionAndExpect(&run_loop, CSCResult::kUnknownError);
  run_loop.Run();
}

TEST_P(CapturedSurfaceControllerSelfCaptureTest,
       UpdateCaptureTargetToOtherTabEnablesCapturedSurfaceControl) {
  StartCaptureOf(*capturing_wc_);
  AwaitWebContentsResolution();
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);

  controller_->UpdateCaptureTarget(GetWebContentsMediaCaptureId(*captured_wc_));
  AwaitWebContentsResolution();

  base::RunLoop run_loop;
  RunTestedActionAndExpect(&run_loop, CSCResult::kSuccess);
  run_loop.Run();
}

TEST_P(CapturedSurfaceControllerSelfCaptureTest,
       UpdateCaptureTargetToCapturingTabDisablesCapturedSurfaceControl) {
  StartCaptureOf(*captured_wc_);
  AwaitWebContentsResolution();
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);

  {
    base::RunLoop run_loop;
    RunTestedActionAndExpect(&run_loop, CSCResult::kSuccess);
    run_loop.Run();
  }

  controller_->UpdateCaptureTarget(
      GetWebContentsMediaCaptureId(*capturing_wc_));
  AwaitWebContentsResolution();

  base::RunLoop run_loop;
  RunTestedActionAndExpect(&run_loop, CSCResult::kUnknownError);
  run_loop.Run();
}

}  // namespace
}  // namespace content
