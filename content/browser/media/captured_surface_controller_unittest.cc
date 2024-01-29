// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/captured_surface_controller.h"

#include <limits>
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

enum class Boundary {
  kMin,
  kMax,
};

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

// Simulates a tab.
//
// Wraps a `WebContents`, which is the main object of interest, along with a
// `TestView`, which is essentially a `RenderWidgetHostView` that allows us to
// set a custom size, which is needed when testing SendWheel().
//
// This object records the original `RenderWidgetHostView` and injects it back
// from the destructor. This prevents LSAN/ASAN failures. This functionality is
// the main value proposition of this class.
class TestTab {
 public:
  static constexpr gfx::Size kDefaultViewportSize = gfx::Size(100, 400);

  explicit TestTab(BrowserContext* browser_context)
      : web_contents_(MakeTestWebContents(browser_context)) {
    // Store the original RenderWidgetHost, allowing it to be injected back from
    // the destructor.
    original_rwhv_ = GetRenderWidgetHostImpl()->GetView();

    // Set a new RenderWidgetHost that allows us control over its size.
    rwhv_ = std::make_unique<TestView>(GetRenderWidgetHostImpl());
    SetView(rwhv_.get());
    SetSize(kDefaultViewportSize);
  }

  virtual ~TestTab() {
    SetView(original_rwhv_);
    original_rwhv_ = nullptr;
  }

  TestWebContents* web_contents() const { return web_contents_.get(); }

  WebContentsMediaCaptureId GetWebContentsMediaCaptureId() const {
    RenderFrameHost* const rfh = web_contents_->GetPrimaryMainFrame();
    return WebContentsMediaCaptureId(rfh->GetProcess()->GetID(),
                                     rfh->GetRoutingID());
  }

  void SetSize(const gfx::Size& size) { rwhv_->SetSize(size); }

  RenderWidgetHostImpl* GetRenderWidgetHostImpl() const {
    return RenderWidgetHostImpl::From(
        web_contents_->GetPrimaryMainFrame()->GetRenderWidgetHost());
  }

  void Focus() {
    web_contents_->GetPrimaryMainFrame()->GetRenderWidgetHost()->Focus();
    FrameTree& frame_tree = web_contents_->GetPrimaryFrameTree();
    FrameTreeNode* const root = frame_tree.root();
    frame_tree.SetFocusedFrame(
        root, root->current_frame_host()->GetSiteInstance()->group());
  }

 protected:
  static std::unique_ptr<TestWebContents> MakeTestWebContents(
      BrowserContext* browser_context) {
    scoped_refptr<SiteInstance> instance =
        SiteInstance::Create(browser_context);
    instance->GetProcess()->Init();
    return TestWebContents::Create(browser_context, std::move(instance));
  }

  void SetView(RenderWidgetHostViewBase* rwhv) {
    GetRenderWidgetHostImpl()->SetView(rwhv);
  }

 private:
  const std::unique_ptr<TestWebContents> web_contents_;
  std::unique_ptr<TestView> rwhv_;
  raw_ptr<RenderWidgetHostViewBase> original_rwhv_ = nullptr;
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
  ~CapturedSurfaceControllerTestBase() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    SetUpTestTabs();
    StartCaptureOf(*capturee_);
    AwaitWebContentsResolution();
  }

  void SetUpTestTabs(bool focus_capturer = true) {
    capturer_ = std::make_unique<TestTab>(GetBrowserContext());
    capturee_ = std::make_unique<TestTab>(GetBrowserContext());
    if (focus_capturer) {
      capturer_->Focus();
    }
  }

  void StartCaptureOf(const TestTab& tab) {
    auto permission_manager = std::make_unique<MockPermissionManager>(
        capturer_->web_contents()->GetPrimaryMainFrame()->GetGlobalId());
    permission_manager_ = permission_manager.get();

    // `base::Unretained(this)` is safe because `this` owns `controller_` and
    // therefore has a longer lifetime.
    controller_ = CapturedSurfaceController::CreateForTesting(
        capturer_->web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
        tab.GetWebContentsMediaCaptureId(), std::move(permission_manager),
        base::BindRepeating(
            &CapturedSurfaceControllerTestBase::OnWebContentsResolved,
            base::Unretained(this)));
  }

  void TearDown() override {
    permission_manager_ = nullptr;
    controller_.reset();
    capturer_.reset();
    capturee_.reset();

    RenderViewHostTestHarness::TearDown();
  }

  void AwaitWebContentsResolution() {
    CHECK(!wc_resolution_run_loop_);
    wc_resolution_run_loop_ = std::make_unique<base::RunLoop>();
    wc_resolution_run_loop_->Run();
    wc_resolution_run_loop_.reset();
  }

  void OnWebContentsResolved(base::WeakPtr<WebContents> wc) {
    if (wc_resolution_run_loop_) {
      wc_resolution_run_loop_->Quit();
    }
    last_resolved_web_contents_ = wc;
  }

 protected:
  std::unique_ptr<CapturedSurfaceController> controller_;
  raw_ptr<MockPermissionManager> permission_manager_ = nullptr;
  std::unique_ptr<TestTab> capturer_;
  std::unique_ptr<TestTab> capturee_;
  std::unique_ptr<base::RunLoop> wc_resolution_run_loop_;
  absl::optional<base::WeakPtr<WebContents>> last_resolved_web_contents_;
};

class CapturedSurfaceControllerSendWheelTest
    : public CapturedSurfaceControllerTestBase {
 public:
  ~CapturedSurfaceControllerSendWheelTest() override = default;

  void SetUp() override {
    CapturedSurfaceControllerTestBase::SetUp();

    input_observer_ = std::make_unique<InputObserver>();
    capturee_->GetRenderWidgetHostImpl()->AddInputEventObserver(
        input_observer_.get());
  }

  void TearDown() override {
    capturee_->GetRenderWidgetHostImpl()->RemoveInputEventObserver(
        input_observer_.get());

    CapturedSurfaceControllerTestBase::TearDown();
  }

 protected:
  std::unique_ptr<InputObserver> input_observer_;
};

TEST_F(CapturedSurfaceControllerSendWheelTest, CorrectScaling) {
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);
  capturee_->SetSize(gfx::Size(256, 4096));
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
  capturee_->SetSize(gfx::Size(0, 4096));
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
  capturee_->SetSize(gfx::Size(256, 0));
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
  capturee_->SetSize(gfx::Size(1, 4096));
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
  capturee_->SetSize(gfx::Size(256, 1));
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
  content::HostZoomMap::SetZoomLevel(capturee_->web_contents(),
                                     blink::PageZoomFactorToZoomLevel(0.9));
  base::RunLoop run_loop;
  controller_->GetZoomLevel(MakeGetZoomLevelCallbackExpectingResult(
      &run_loop, 90, CSCResult::kSuccess));
  run_loop.Run();
}

TEST_F(CapturedSurfaceControllerGetZoomLevelTest, GetZoomLevelUnknownError) {
  base::RunLoop run_loop;
  capturee_.reset();
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
                                     capturee_->web_contents()))));
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

INSTANTIATE_TEST_SUITE_P(
    ,
    CapturedSurfaceControllerInterfaceTest,
    ::testing::Values(CapturedSurfaceControlAPI::kSendWheel,
                      CapturedSurfaceControlAPI::kSetZoomLevel,
                      CapturedSurfaceControlAPI::kGetZoomLevel));

TEST_P(CapturedSurfaceControllerInterfaceTest, SuccessReportedIfPermitted) {
  base::RunLoop run_loop;
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);
  RunTestedActionAndExpect(&run_loop, CSCResult::kSuccess);
  run_loop.Run();
}

TEST_P(CapturedSurfaceControllerInterfaceTest, NoPermissionReportedIfDenied) {
  if (tested_interface_ == CapturedSurfaceControlAPI::kGetZoomLevel) {
    GTEST_SKIP() << "No permission check required for getZoomLevel().";
  }
  base::RunLoop run_loop;
  permission_manager_->SetPermissionResult(CSCPermissionResult::kDenied);
  RunTestedActionAndExpect(&run_loop, CSCResult::kNoPermissionError);
  run_loop.Run();
}

TEST_P(CapturedSurfaceControllerInterfaceTest,
       UnknownErrorReportedIfPermissionError) {
  if (tested_interface_ == CapturedSurfaceControlAPI::kGetZoomLevel) {
    GTEST_SKIP() << "No permission check required for getZoomLevel().";
  }
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
  capturee_.reset();
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

TEST_P(CapturedSurfaceControllerInterfaceTest,
       CapturerNotFoundErrorReportedIfCapturerClosed) {
  base::RunLoop run_loop;
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);
  capturer_.reset();
  // TODO(crbug.com/1466247): Use kCapturerNotFoundError after introducing it.
  RunTestedActionAndExpect(&run_loop, CSCResult::kUnknownError);
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

    // Prepare a new tab to capture instead of the original one.
    new_capturee_ = std::make_unique<TestTab>(GetBrowserContext());
  }

  void TearDown() override {
    new_capturee_.reset();

    CapturedSurfaceControllerInterfaceTestBase::TearDown();
  }

 protected:
  std::unique_ptr<TestTab> new_capturee_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    CapturedSurfaceControllerWebContentsResolutionTest,
    ::testing::Values(CapturedSurfaceControlAPI::kSendWheel,
                      CapturedSurfaceControlAPI::kSetZoomLevel,
                      CapturedSurfaceControlAPI::kGetZoomLevel));

TEST_P(CapturedSurfaceControllerWebContentsResolutionTest,
       ApiInvocationAfterWebContentsResolutionSucceeds) {
  SetUpTestTabs();  // Triggers resolution but does not await it.
  StartCaptureOf(*capturee_);
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);

  AwaitWebContentsResolution();

  base::RunLoop run_loop;
  RunTestedActionAndExpect(&run_loop, CSCResult::kSuccess);
  run_loop.Run();
}

TEST_P(CapturedSurfaceControllerWebContentsResolutionTest,
       ApiInvocationPriorToWebContentsResolutionFails) {
  SetUpTestTabs();  // Triggers resolution but does not await it.
  StartCaptureOf(*capturee_);
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
    SetUpTestTabs();  // Triggers resolution but does not await it.
    StartCaptureOf(*capturee_);
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

TEST_P(CapturedSurfaceControllerWebContentsResolutionTest,
       MultiplePendingResolutions) {
  SetUpTestTabs();  // Triggers resolution but does not await it.
  StartCaptureOf(*capturee_);
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);

  // The original resolution has not yet resolved.
  ASSERT_FALSE(last_resolved_web_contents_.has_value());

  // Updating to capture another tab schedules a new task to resolve.
  controller_->UpdateCaptureTarget(
      new_capturee_->GetWebContentsMediaCaptureId());

  // Neither resolutions has completed at this point.
  ASSERT_FALSE(last_resolved_web_contents_.has_value());

  // We await the resolution to be considered complete.
  // This should only happen after the last pending task resolves.
  // In our cases, that is for the new tab. The first response
  // should be ignored.
  AwaitWebContentsResolution();
  ASSERT_TRUE(last_resolved_web_contents_.has_value());
  EXPECT_EQ(last_resolved_web_contents_.value().get(),
            new_capturee_->web_contents());
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
    new_capturee_ = std::make_unique<TestTab>(GetBrowserContext());
  }

  void TearDown() override {
    new_capturee_.reset();

    CapturedSurfaceControllerInterfaceTestBase::TearDown();
  }

 protected:
  std::unique_ptr<TestTab> new_capturee_;
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
  controller_->UpdateCaptureTarget(
      new_capturee_->GetWebContentsMediaCaptureId());
  AwaitWebContentsResolution();

  base::RunLoop run_loop;
  RunTestedActionAndExpect(&run_loop, CSCResult::kSuccess);
  run_loop.Run();
}

TEST_P(CapturedSurfaceControllerWebContentsResolutionOfUpdatesTest,
       AfterUpdateCaptureTargetApiInvocationPriorToWebContentsResolutionFails) {
  // Call UpdateCaptureTarget() - capturing a new tab.
  controller_->UpdateCaptureTarget(
      new_capturee_->GetWebContentsMediaCaptureId());
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
    SetUpTestTabs();
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    CapturedSurfaceControllerSelfCaptureTest,
    ::testing::Values(CapturedSurfaceControlAPI::kSendWheel,
                      CapturedSurfaceControlAPI::kSetZoomLevel,
                      CapturedSurfaceControlAPI::kGetZoomLevel));

TEST_P(CapturedSurfaceControllerSelfCaptureTest, SelfCaptureDisallowed) {
  StartCaptureOf(*capturer_);
  AwaitWebContentsResolution();
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);

  base::RunLoop run_loop;
  RunTestedActionAndExpect(&run_loop,
                           CSCResult::kDisallowedForSelfCaptureError);
  run_loop.Run();
}

TEST_P(CapturedSurfaceControllerSelfCaptureTest,
       UpdateCaptureTargetToOtherTabEnablesCapturedSurfaceControl) {
  StartCaptureOf(*capturer_);
  AwaitWebContentsResolution();
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);

  controller_->UpdateCaptureTarget(capturee_->GetWebContentsMediaCaptureId());
  AwaitWebContentsResolution();

  base::RunLoop run_loop;
  RunTestedActionAndExpect(&run_loop, CSCResult::kSuccess);
  run_loop.Run();
}

TEST_P(CapturedSurfaceControllerSelfCaptureTest,
       UpdateCaptureTargetToCapturingTabDisablesCapturedSurfaceControl) {
  StartCaptureOf(*capturee_);
  AwaitWebContentsResolution();
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);

  {
    base::RunLoop run_loop;
    RunTestedActionAndExpect(&run_loop, CSCResult::kSuccess);
    run_loop.Run();
  }

  controller_->UpdateCaptureTarget(capturer_->GetWebContentsMediaCaptureId());
  AwaitWebContentsResolution();

  base::RunLoop run_loop;
  RunTestedActionAndExpect(&run_loop,
                           CSCResult::kDisallowedForSelfCaptureError);
  run_loop.Run();
}

class CapturedSurfaceControllerFocusRequirementTest
    : public CapturedSurfaceControllerInterfaceTestBase,
      public ::testing::WithParamInterface<CapturedSurfaceControlAPI> {
 public:
  CapturedSurfaceControllerFocusRequirementTest()
      : CapturedSurfaceControllerInterfaceTestBase(GetParam()) {}

  void SetUp() override {
    // Skip CapturedSurfaceControllerTestBase's SetUp(),
    RenderViewHostTestHarness::SetUp();
    SetUpTestTabs(/*focus_capturer=*/false);
    StartCaptureOf(*capturee_);
    AwaitWebContentsResolution();
  }

  ~CapturedSurfaceControllerFocusRequirementTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    CapturedSurfaceControllerFocusRequirementTest,
    ::testing::Values(CapturedSurfaceControlAPI::kSendWheel,
                      CapturedSurfaceControlAPI::kSetZoomLevel,
                      CapturedSurfaceControlAPI::kGetZoomLevel));

TEST_P(CapturedSurfaceControllerFocusRequirementTest,
       CallSucceedsIfCapturerFocused) {
  base::RunLoop run_loop;
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);
  capturer_->Focus();
  RunTestedActionAndExpect(&run_loop, CSCResult::kSuccess);
  run_loop.Run();
}

TEST_P(CapturedSurfaceControllerFocusRequirementTest,
       CallsFailsIfCapturerUnfocused) {
  if (tested_interface_ == CapturedSurfaceControlAPI::kGetZoomLevel) {
    GTEST_SKIP() << "The focus requirement does not apply to getZoomLevel().";
  }
  base::RunLoop run_loop;
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);
  // Note absence of call to `capturer_->Focus()`.
  // TODO(crbug.com/1466247): Use a dedicated error.
  RunTestedActionAndExpect(&run_loop, CSCResult::kUnknownError);
  run_loop.Run();
}

// This test suite checks correct clamping of x/y wheel-deltas to min/max.
//
// The suite is parameterized on the *zoom* level because that affects
// the values that will ultimately be fed into the UI system, and checking
// at both the min/max zoom levels increases coverage somewhat.
//
// The suite is *not* parameterized on the wheel deltas themselves, as that
// would increase test complexity and reduce confidence in test correctness.
class CapturedSurfaceControllerSendWheelClampTest
    : public CapturedSurfaceControllerSendWheelTest,
      public ::testing::WithParamInterface<Boundary> {
 public:
  CapturedSurfaceControllerSendWheelClampTest()
      : zoom_level_boundary_(GetParam()) {}
  ~CapturedSurfaceControllerSendWheelClampTest() override = default;

 protected:
  int zoom_level() const {
    static const double kMin = 100 * blink::kMaximumPageZoomFactor;
    static const double kMax = 100 * blink::kMinimumPageZoomFactor;
    switch (zoom_level_boundary_) {
      case Boundary::kMin:
        return static_cast<int>(std::ceil(kMin));
      case Boundary::kMax:
        return static_cast<int>(std::floor(kMax));
    }
    NOTREACHED_NORETURN();
  }

 private:
  const Boundary zoom_level_boundary_;
};

INSTANTIATE_TEST_SUITE_P(,
                         CapturedSurfaceControllerSendWheelClampTest,
                         ::testing::Values(Boundary::kMin, Boundary::kMax));

TEST_P(CapturedSurfaceControllerSendWheelClampTest, ClampMinWheelDeltaX) {
  using WheelDeltaType = decltype(CapturedWheelAction::wheel_delta_x);
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);
  base::RunLoop run_loop;
  input_observer_->AddExpectation(InputObserver::ExpectedWheelEvent{
      .x = 0,
      .y = 0,
      .delta_x = -CapturedSurfaceController::kMaxWheelDeltaMagnitude,
      .delta_y = 0});
  controller_->SendWheel(
      CapturedWheelAction::New(
          /*x=*/0,
          /*y=*/0,
          /*wheel_delta_x=*/std::numeric_limits<WheelDeltaType>::min(),
          /*wheel_delta_y=*/0),
      MakeCallbackExpectingResult(&run_loop, CSCResult::kSuccess));
  run_loop.Run();
}

TEST_P(CapturedSurfaceControllerSendWheelClampTest, ClampMaxWheelDeltaX) {
  using WheelDeltaType = decltype(CapturedWheelAction::wheel_delta_x);
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);
  base::RunLoop run_loop;
  input_observer_->AddExpectation(InputObserver::ExpectedWheelEvent{
      .x = 0,
      .y = 0,
      .delta_x = CapturedSurfaceController::kMaxWheelDeltaMagnitude,
      .delta_y = 0});
  controller_->SendWheel(
      CapturedWheelAction::New(
          /*x=*/0,
          /*y=*/0,
          /*wheel_delta_x=*/std::numeric_limits<WheelDeltaType>::max(),
          /*wheel_delta_y=*/0),
      MakeCallbackExpectingResult(&run_loop, CSCResult::kSuccess));
  run_loop.Run();
}

TEST_P(CapturedSurfaceControllerSendWheelClampTest, ClampMinWheelDeltaY) {
  using WheelDeltaType = decltype(CapturedWheelAction::wheel_delta_y);
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);
  base::RunLoop run_loop;
  input_observer_->AddExpectation(InputObserver::ExpectedWheelEvent{
      .x = 0,
      .y = 0,
      .delta_x = 0,
      .delta_y = -CapturedSurfaceController::kMaxWheelDeltaMagnitude});
  controller_->SendWheel(
      CapturedWheelAction::New(
          /*x=*/0,
          /*y=*/0,
          /*wheel_delta_x=*/0,
          /*wheel_delta_y=*/std::numeric_limits<WheelDeltaType>::min()),
      MakeCallbackExpectingResult(&run_loop, CSCResult::kSuccess));
  run_loop.Run();
}

TEST_P(CapturedSurfaceControllerSendWheelClampTest, ClampMaxWheelDeltaY) {
  using WheelDeltaType = decltype(CapturedWheelAction::wheel_delta_y);
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);
  base::RunLoop run_loop;
  input_observer_->AddExpectation(InputObserver::ExpectedWheelEvent{
      .x = 0,
      .y = 0,
      .delta_x = 0,
      .delta_y = CapturedSurfaceController::kMaxWheelDeltaMagnitude});
  controller_->SendWheel(
      CapturedWheelAction::New(
          /*x=*/0,
          /*y=*/0,
          /*wheel_delta_x=*/0,
          /*wheel_delta_y=*/std::numeric_limits<WheelDeltaType>::max()),
      MakeCallbackExpectingResult(&run_loop, CSCResult::kSuccess));
  run_loop.Run();
}

}  // namespace
}  // namespace content
