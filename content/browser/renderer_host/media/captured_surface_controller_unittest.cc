// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/captured_surface_controller.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "content/browser/renderer_host/media/captured_surface_control_permission_manager.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/test/browser_task_environment.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
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
  absl::optional<CSCPermissionResult> result_;
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

class CapturedSurfaceControllerTestBase : public RenderViewHostTestHarness {
 public:
  ~CapturedSurfaceControllerTestBase() override = default;

  std::unique_ptr<TestWebContents> MakeTestWebContents() {
    scoped_refptr<SiteInstance> instance =
        SiteInstance::Create(GetBrowserContext());
    instance->GetProcess()->Init();
    return TestWebContents::Create(GetBrowserContext(), std::move(instance));
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    capturing_wc_ = MakeTestWebContents();
    captured_wc_ = MakeTestWebContents();

    const RenderFrameHost* const captured_main_rfh =
        captured_wc_->GetPrimaryMainFrame();
    const WebContentsMediaCaptureId captured_wc_id(
        captured_main_rfh->GetProcess()->GetID(),
        captured_main_rfh->GetRoutingID());

    auto permission_manager = std::make_unique<MockPermissionManager>(
        capturing_wc_->GetPrimaryMainFrame()->GetGlobalId());
    permission_manager_ = permission_manager.get();

    controller_ = std::make_unique<CapturedSurfaceController>(
        captured_wc_id, std::move(permission_manager));
  }

  void TearDown() override {
    permission_manager_ = nullptr;
    controller_.reset();
    capturing_wc_.reset();
    captured_wc_.reset();

    RenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<CapturedSurfaceController> controller_;
  raw_ptr<MockPermissionManager> permission_manager_ = nullptr;
  std::unique_ptr<TestWebContents> capturing_wc_;
  std::unique_ptr<TestWebContents> captured_wc_;
};

enum class CapturedSurfaceControlAPI {
  kSendWheel,
  kSetZoomLevel,
};

class CapturedSurfaceControllerInterfaceTest
    : public CapturedSurfaceControllerTestBase,
      public ::testing::WithParamInterface<CapturedSurfaceControlAPI> {
 public:
  CapturedSurfaceControllerInterfaceTest() : tested_interface_(GetParam()) {}

  ~CapturedSurfaceControllerInterfaceTest() override = default;

  void RunTestedAction(base::OnceCallback<void(CSCResult)> callback) {
    switch (tested_interface_) {
      case CapturedSurfaceControlAPI::kSendWheel:
        controller_->SendWheel(MakeCapturedWheelActionPtr(),
                               std::move(callback));
        return;
      case CapturedSurfaceControlAPI::kSetZoomLevel:
        controller_->SetZoomLevel(/*zoom_level=*/100, std::move(callback));
        return;
    }
    NOTREACHED_NORETURN();
  }

  const CapturedSurfaceControlAPI tested_interface_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    CapturedSurfaceControllerInterfaceTest,
    ::testing::Values(CapturedSurfaceControlAPI::kSendWheel,
                      CapturedSurfaceControlAPI::kSetZoomLevel));

TEST_P(CapturedSurfaceControllerInterfaceTest, SuccessReportedIfPermitted) {
  base::RunLoop run_loop;
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);
  RunTestedAction(MakeCallbackExpectingResult(&run_loop, CSCResult::kSuccess));
  run_loop.Run();
}

TEST_P(CapturedSurfaceControllerInterfaceTest, NoPermissionReportedIfDenied) {
  base::RunLoop run_loop;
  permission_manager_->SetPermissionResult(CSCPermissionResult::kDenied);
  RunTestedAction(
      MakeCallbackExpectingResult(&run_loop, CSCResult::kNoPermissionError));
  run_loop.Run();
}

TEST_P(CapturedSurfaceControllerInterfaceTest,
       UnknownErrorReportedIfPermissionError) {
  base::RunLoop run_loop;
  permission_manager_->SetPermissionResult(CSCPermissionResult::kError);
  RunTestedAction(
      MakeCallbackExpectingResult(&run_loop, CSCResult::kUnknownError));
  run_loop.Run();
}

// Simulate the captured tab being closed after permission is granted but before
// the controller has time to process the response from the permission manager.
TEST_P(CapturedSurfaceControllerInterfaceTest,
       SurfaceNotFoundReportedIfTabClosedBeforePromptResponseHandled) {
  base::RunLoop run_loop;
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);
  captured_wc_.reset();
  RunTestedAction(MakeCallbackExpectingResult(
      &run_loop, CSCResult::kCapturedSurfaceNotFoundError));
  run_loop.Run();
}

TEST_P(CapturedSurfaceControllerInterfaceTest,
       SurfaceNotFoundReportedIfCaptureTargetUpdatedToNonTabSurface) {
  base::RunLoop run_loop;
  permission_manager_->SetPermissionResult(CSCPermissionResult::kGranted);
  controller_->UpdateCaptureTarget(WebContentsMediaCaptureId());
  RunTestedAction(MakeCallbackExpectingResult(
      &run_loop, CSCResult::kCapturedSurfaceNotFoundError));
  run_loop.Run();
}

}  // namespace
}  // namespace content
