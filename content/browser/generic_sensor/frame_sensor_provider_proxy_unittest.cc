// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/generic_sensor/frame_sensor_provider_proxy.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "content/browser/generic_sensor/web_contents_sensor_provider_proxy.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/permission_result.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/cpp/test/fake_sensor_and_provider.h"
#include "services/device/public/mojom/sensor.mojom.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/sensor/web_sensor_provider.mojom.h"

namespace content {

namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

class MockSensorPermissionManager : public MockPermissionManager {
 public:
  MockSensorPermissionManager() = default;
  ~MockSensorPermissionManager() override = default;

  MOCK_METHOD(
      void,
      RequestPermissionsFromCurrentDocumentInternal,
      (RenderFrameHost*,
       const PermissionRequestDescription&,
       base::OnceCallback<void(const std::vector<PermissionResult>&)>&));

  void RequestPermissionsFromCurrentDocument(
      RenderFrameHost* render_frame_host,
      const PermissionRequestDescription& request_description,
      base::OnceCallback<void(const std::vector<PermissionResult>&)> callback)
      override {
    RequestPermissionsFromCurrentDocumentInternal(
        render_frame_host, request_description, callback);
  }
};

class FrameSensorProviderProxyTest : public RenderViewHostImplTestHarness {
 public:
  FrameSensorProviderProxyTest() = default;
  ~FrameSensorProviderProxyTest() override = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    static_cast<TestBrowserContext*>(browser_context())
        ->SetPermissionControllerDelegate(
            std::make_unique<testing::NiceMock<MockSensorPermissionManager>>());

    fake_sensor_provider_ = std::make_unique<device::FakeSensorProvider>();
    WebContentsSensorProviderProxy::OverrideSensorProviderBinderForTesting(
        base::BindRepeating(
            &FrameSensorProviderProxyTest::BindSensorProviderReceiver,
            base::Unretained(this)));
  }

  void TearDown() override {
    WebContentsSensorProviderProxy::OverrideSensorProviderBinderForTesting(
        base::NullCallback());
    fake_sensor_provider_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

 protected:
  MockSensorPermissionManager* permission_manager() {
    return static_cast<MockSensorPermissionManager*>(
        static_cast<TestBrowserContext*>(browser_context())
            ->GetPermissionControllerDelegate());
  }

  device::FakeSensorProvider* fake_sensor_provider() {
    return fake_sensor_provider_.get();
  }

  mojo::Remote<blink::mojom::WebSensorProvider> GetWebSensorProvider() {
    mojo::Remote<blink::mojom::WebSensorProvider> provider;
    main_test_rfh()->GetSensorProvider(provider.BindNewPipeAndPassReceiver());
    return provider;
  }

  device::mojom::SensorCreationResult GetSensorSync(
      mojo::Remote<blink::mojom::WebSensorProvider>& provider,
      device::mojom::SensorType type,
      bool user_gesture) {
    base::test::TestFuture<device::mojom::SensorCreationResult,
                           device::mojom::SensorInitParamsPtr>
        future;
    provider->GetSensor(type, user_gesture, future.GetCallback());
    return future.Get<0>();
  }

 private:
  void BindSensorProviderReceiver(
      mojo::PendingReceiver<device::mojom::SensorProvider> receiver) {
    fake_sensor_provider_->Bind(std::move(receiver));
  }

  std::unique_ptr<device::FakeSensorProvider> fake_sensor_provider_;
};

TEST_F(FrameSensorProviderProxyTest, GetSensor_PermissionGranted) {
  // 1. Initial silent check says GRANTED.
  EXPECT_CALL(*permission_manager(),
              GetPermissionResultForCurrentDocument(_, _, _))
      .Times(2)
      .WillRepeatedly(
          Return(PermissionResult(blink::mojom::PermissionStatus::GRANTED,
                                  PermissionStatusSource::UNSPECIFIED)));

  // 2. No prompt should be requested.
  EXPECT_CALL(*permission_manager(),
              RequestPermissionsFromCurrentDocumentInternal(_, _, _))
      .Times(0);

  auto provider = GetWebSensorProvider();
  auto result =
      GetSensorSync(provider, device::mojom::SensorType::ACCELEROMETER,
                    /*user_gesture=*/false);
  EXPECT_EQ(result, device::mojom::SensorCreationResult::SUCCESS);
}

TEST_F(FrameSensorProviderProxyTest, GetSensor_PermissionDenied) {
  // 1. Initial silent check says DENIED.
  EXPECT_CALL(*permission_manager(),
              GetPermissionResultForCurrentDocument(_, _, _))
      .WillOnce(Return(PermissionResult(blink::mojom::PermissionStatus::DENIED,
                                        PermissionStatusSource::UNSPECIFIED)));

  auto provider = GetWebSensorProvider();
  auto result =
      GetSensorSync(provider, device::mojom::SensorType::ACCELEROMETER,
                    /*user_gesture=*/true);
  EXPECT_EQ(result, device::mojom::SensorCreationResult::ERROR_NOT_ALLOWED);
}

TEST_F(FrameSensorProviderProxyTest,
       GetSensor_PermissionAsk_NoGesture_FailsSilently) {
  // 1. Initial silent check says ASK.
  EXPECT_CALL(*permission_manager(),
              GetPermissionResultForCurrentDocument(_, _, _))
      .WillOnce(Return(PermissionResult(blink::mojom::PermissionStatus::ASK,
                                        PermissionStatusSource::UNSPECIFIED)));

  // 2. No prompt should be requested because there is no gesture.
  EXPECT_CALL(*permission_manager(),
              RequestPermissionsFromCurrentDocumentInternal(_, _, _))
      .Times(0);

  auto provider = GetWebSensorProvider();
  auto result =
      GetSensorSync(provider, device::mojom::SensorType::ACCELEROMETER,
                    /*user_gesture=*/false);
  EXPECT_EQ(result, device::mojom::SensorCreationResult::ERROR_NOT_ALLOWED);
}

TEST_F(FrameSensorProviderProxyTest,
       GetSensor_PermissionAsk_WithGesture_PromptsAndGrants) {
  // 1. Initial silent check says ASK.
  EXPECT_CALL(*permission_manager(),
              GetPermissionResultForCurrentDocument(_, _, _))
      .Times(2)
      .WillRepeatedly(
          Return(PermissionResult(blink::mojom::PermissionStatus::ASK,
                                  PermissionStatusSource::UNSPECIFIED)));

  // 2. Prompt is requested because there is a gesture.
  // We assert that the gesture bit is correctly passed into the description.
  EXPECT_CALL(*permission_manager(),
              RequestPermissionsFromCurrentDocumentInternal(_, _, _))
      .WillOnce(
          [](RenderFrameHost* rfh,
             const PermissionRequestDescription& request_description,
             base::OnceCallback<void(const std::vector<PermissionResult>&)>&
                 cb) {
            EXPECT_TRUE(request_description.user_gesture);
            std::move(cb).Run(
                {PermissionResult(blink::mojom::PermissionStatus::GRANTED,
                                  PermissionStatusSource::UNSPECIFIED)});
          });

  auto provider = GetWebSensorProvider();
  static_cast<TestRenderFrameHost*>(main_test_rfh())->SimulateUserActivation();
  auto result =
      GetSensorSync(provider, device::mojom::SensorType::ACCELEROMETER,
                    /*user_gesture=*/true);
  EXPECT_EQ(result, device::mojom::SensorCreationResult::SUCCESS);
}

TEST_F(FrameSensorProviderProxyTest,
       GetSensor_HardwareNotAvailable_NoPromptShown) {
  // 1. Setup fake hardware to fail.
  fake_sensor_provider()->set_accelerometer_is_available(false);

  // 2. Initial silent check says ASK.
  EXPECT_CALL(*permission_manager(),
              GetPermissionResultForCurrentDocument(_, _, _))
      .WillOnce(Return(PermissionResult(blink::mojom::PermissionStatus::ASK,
                                        PermissionStatusSource::UNSPECIFIED)));

  // 3. CRITICAL: No prompt should be shown because hardware is missing.
  // The probe should fail fast.
  EXPECT_CALL(*permission_manager(),
              RequestPermissionsFromCurrentDocumentInternal(_, _, _))
      .Times(0);

  auto provider = GetWebSensorProvider();
  static_cast<TestRenderFrameHost*>(main_test_rfh())->SimulateUserActivation();
  auto result =
      GetSensorSync(provider, device::mojom::SensorType::ACCELEROMETER,
                    /*user_gesture=*/true);
  EXPECT_EQ(result, device::mojom::SensorCreationResult::ERROR_NOT_AVAILABLE);
}

TEST_F(FrameSensorProviderProxyTest,
       GetSensor_PermissionAsk_WithGesture_PromptsAndDenies) {
  // 1. Initial silent check says ASK.
  EXPECT_CALL(*permission_manager(),
              GetPermissionResultForCurrentDocument(_, _, _))
      .Times(2)
      .WillRepeatedly(
          Return(PermissionResult(blink::mojom::PermissionStatus::ASK,
                                  PermissionStatusSource::UNSPECIFIED)));

  // 2. Prompt is requested because there is a gesture.
  // We simulate the user clicking "Block".
  EXPECT_CALL(*permission_manager(),
              RequestPermissionsFromCurrentDocumentInternal(_, _, _))
      .WillOnce(
          [](RenderFrameHost* rfh,
             const PermissionRequestDescription& request_description,
             base::OnceCallback<void(const std::vector<PermissionResult>&)>&
                 cb) {
            std::move(cb).Run(
                {PermissionResult(blink::mojom::PermissionStatus::DENIED,
                                  PermissionStatusSource::UNSPECIFIED)});
          });

  auto provider = GetWebSensorProvider();
  static_cast<TestRenderFrameHost*>(main_test_rfh())->SimulateUserActivation();
  auto result =
      GetSensorSync(provider, device::mojom::SensorType::ACCELEROMETER,
                    /*user_gesture=*/true);
  EXPECT_EQ(result, device::mojom::SensorCreationResult::ERROR_NOT_ALLOWED);
}

TEST_F(FrameSensorProviderProxyTest,
       GetSensor_VisibilityChanged_SuspendsAndResumes) {
  // Ensure initially focused.
  auto* rwh_visibility_test = static_cast<RenderWidgetHostImpl*>(
      main_test_rfh()->GetRenderWidgetHost());
  rwh_visibility_test->SetPageFocus(true);
  FocusWebContentsOnFrame(main_test_rfh());

  EXPECT_CALL(*permission_manager(),
              GetPermissionResultForCurrentDocument(_, _, _))
      .Times(2)
      .WillRepeatedly(
          Return(PermissionResult(blink::mojom::PermissionStatus::GRANTED,
                                  PermissionStatusSource::UNSPECIFIED)));

  auto provider = GetWebSensorProvider();
  static_cast<TestRenderFrameHost*>(main_test_rfh())->SimulateUserActivation();

  mojo::Remote<device::mojom::Sensor> sensor_remote;
  base::test::TestFuture<device::mojom::SensorCreationResult,
                         device::mojom::SensorInitParamsPtr>
      future;
  provider->GetSensor(device::mojom::SensorType::ACCELEROMETER,
                      /*user_gesture=*/true, future.GetCallback());
  auto [result, params] = future.Take();
  EXPECT_EQ(result, device::mojom::SensorCreationResult::SUCCESS);
  sensor_remote.Bind(std::move(params->sensor));

  device::FakeSensor* fake_sensor = fake_sensor_provider()->accelerometer();
  ASSERT_TRUE(fake_sensor);

  web_contents()->WasHidden();
  EXPECT_TRUE(fake_sensor->WaitForBrowserSuspend(true));

  web_contents()->WasShown();
  EXPECT_TRUE(fake_sensor->WaitForBrowserSuspend(false));
}

TEST_F(FrameSensorProviderProxyTest,
       GetSensor_InitiallyHidden_StartsSuspended) {
  EXPECT_CALL(*permission_manager(),
              GetPermissionResultForCurrentDocument(_, _, _))
      .Times(2)
      .WillRepeatedly(
          Return(PermissionResult(blink::mojom::PermissionStatus::GRANTED,
                                  PermissionStatusSource::UNSPECIFIED)));

  web_contents()->WasHidden();
  auto provider = GetWebSensorProvider();
  static_cast<TestRenderFrameHost*>(main_test_rfh())->SimulateUserActivation();

  mojo::Remote<device::mojom::Sensor> sensor_remote;
  base::test::TestFuture<device::mojom::SensorCreationResult,
                         device::mojom::SensorInitParamsPtr>
      future;
  provider->GetSensor(device::mojom::SensorType::ACCELEROMETER,
                      /*user_gesture=*/true, future.GetCallback());
  auto [result, params] = future.Take();
  EXPECT_EQ(result, device::mojom::SensorCreationResult::SUCCESS);
  sensor_remote.Bind(std::move(params->sensor));

  device::FakeSensor* fake_sensor = fake_sensor_provider()->accelerometer();
  ASSERT_TRUE(fake_sensor);
  EXPECT_TRUE(fake_sensor->is_browser_suspended());
}

TEST_F(FrameSensorProviderProxyTest,
       GetSensor_HiddenDuringPendingRequest_SensorSuspended) {
  EXPECT_CALL(*permission_manager(),
              GetPermissionResultForCurrentDocument(_, _, _))
      .Times(2)
      .WillRepeatedly(
          Return(PermissionResult(blink::mojom::PermissionStatus::GRANTED,
                                  PermissionStatusSource::UNSPECIFIED)));

  auto provider = GetWebSensorProvider();
  static_cast<TestRenderFrameHost*>(main_test_rfh())->SimulateUserActivation();

  fake_sensor_provider()->set_sensor_requested_callback(
      base::BindLambdaForTesting([this](device::mojom::SensorType type) {
        web_contents()->WasHidden();
      }));

  mojo::Remote<device::mojom::Sensor> sensor_remote;
  base::test::TestFuture<device::mojom::SensorCreationResult,
                         device::mojom::SensorInitParamsPtr>
      future;
  provider->GetSensor(device::mojom::SensorType::ACCELEROMETER,
                      /*user_gesture=*/true, future.GetCallback());
  auto [result, params] = future.Take();
  EXPECT_EQ(result, device::mojom::SensorCreationResult::SUCCESS);
  sensor_remote.Bind(std::move(params->sensor));

  device::FakeSensor* fake_sensor = fake_sensor_provider()->accelerometer();
  ASSERT_TRUE(fake_sensor);
  EXPECT_TRUE(fake_sensor->is_browser_suspended());
}

TEST_F(FrameSensorProviderProxyTest,
       GetSensor_PermissionRevokedDuringInFlightHardwareCheck) {
  // 1. Initial check in GetSensor returns GRANTED.
  // 2. Second check in OnHardwareCheckCompleted returns DENIED.
  EXPECT_CALL(*permission_manager(),
              GetPermissionResultForCurrentDocument(_, _, _))
      .WillOnce(Return(PermissionResult(blink::mojom::PermissionStatus::GRANTED,
                                        PermissionStatusSource::UNSPECIFIED)))
      .WillOnce(Return(PermissionResult(blink::mojom::PermissionStatus::DENIED,
                                        PermissionStatusSource::UNSPECIFIED)));

  auto provider = GetWebSensorProvider();
  auto result =
      GetSensorSync(provider, device::mojom::SensorType::ACCELEROMETER,
                    /*user_gesture=*/true);
  EXPECT_EQ(result, device::mojom::SensorCreationResult::ERROR_NOT_ALLOWED);
}

TEST_F(FrameSensorProviderProxyTest, GetSensor_Occluded_Suspends) {
  // Ensure initially focused.
  auto* rwh = static_cast<RenderWidgetHostImpl*>(
      main_test_rfh()->GetRenderWidgetHost());
  rwh->SetPageFocus(true);
  FocusWebContentsOnFrame(main_test_rfh());

  EXPECT_CALL(*permission_manager(),
              GetPermissionResultForCurrentDocument(_, _, _))
      .WillRepeatedly(
          Return(PermissionResult(blink::mojom::PermissionStatus::GRANTED,
                                  PermissionStatusSource::UNSPECIFIED)));

  auto provider = GetWebSensorProvider();
  static_cast<TestRenderFrameHost*>(main_test_rfh())->SimulateUserActivation();

  mojo::Remote<device::mojom::Sensor> sensor_remote;
  base::test::TestFuture<device::mojom::SensorCreationResult,
                         device::mojom::SensorInitParamsPtr>
      future;
  provider->GetSensor(device::mojom::SensorType::ACCELEROMETER,
                      /*user_gesture=*/true, future.GetCallback());
  auto [result, params] = future.Take();
  EXPECT_EQ(result, device::mojom::SensorCreationResult::SUCCESS);
  sensor_remote.Bind(std::move(params->sensor));

  device::FakeSensor* fake_sensor = fake_sensor_provider()->accelerometer();
  ASSERT_TRUE(fake_sensor);

  // Occlude the web contents.
  web_contents()->WasOccluded();
  EXPECT_TRUE(fake_sensor->WaitForBrowserSuspend(true));

  // Show it again.
  web_contents()->WasShown();
  EXPECT_TRUE(fake_sensor->WaitForBrowserSuspend(false));
}

TEST_F(FrameSensorProviderProxyTest,
       GetSensor_InitiallyOccluded_StartsSuspended) {
  EXPECT_CALL(*permission_manager(),
              GetPermissionResultForCurrentDocument(_, _, _))
      .WillRepeatedly(
          Return(PermissionResult(blink::mojom::PermissionStatus::GRANTED,
                                  PermissionStatusSource::UNSPECIFIED)));

  web_contents()->WasOccluded();
  auto provider = GetWebSensorProvider();
  static_cast<TestRenderFrameHost*>(main_test_rfh())->SimulateUserActivation();

  mojo::Remote<device::mojom::Sensor> sensor_remote;
  base::test::TestFuture<device::mojom::SensorCreationResult,
                         device::mojom::SensorInitParamsPtr>
      future;
  provider->GetSensor(device::mojom::SensorType::ACCELEROMETER,
                      /*user_gesture=*/true, future.GetCallback());
  auto [result, params] = future.Take();
  EXPECT_EQ(result, device::mojom::SensorCreationResult::SUCCESS);
  sensor_remote.Bind(std::move(params->sensor));

  device::FakeSensor* fake_sensor = fake_sensor_provider()->accelerometer();
  ASSERT_TRUE(fake_sensor);
  EXPECT_TRUE(fake_sensor->is_browser_suspended());
}

TEST_F(FrameSensorProviderProxyTest, GetSensor_LostFocus_Suspends) {
  EXPECT_CALL(*permission_manager(),
              GetPermissionResultForCurrentDocument(_, _, _))
      .WillRepeatedly(
          Return(PermissionResult(blink::mojom::PermissionStatus::GRANTED,
                                  PermissionStatusSource::UNSPECIFIED)));

  // Ensure initially focused.
  auto* rwh = static_cast<RenderWidgetHostImpl*>(
      main_test_rfh()->GetRenderWidgetHost());
  rwh->SetPageFocus(true);
  FocusWebContentsOnFrame(main_test_rfh());
  static_cast<WebContentsImpl*>(web_contents())->NotifyWebContentsFocused(rwh);

  auto provider = GetWebSensorProvider();
  static_cast<TestRenderFrameHost*>(main_test_rfh())->SimulateUserActivation();

  mojo::Remote<device::mojom::Sensor> sensor_remote;
  base::test::TestFuture<device::mojom::SensorCreationResult,
                         device::mojom::SensorInitParamsPtr>
      future;
  provider->GetSensor(device::mojom::SensorType::ACCELEROMETER,
                      /*user_gesture=*/true, future.GetCallback());
  auto [result, params] = future.Take();
  EXPECT_EQ(result, device::mojom::SensorCreationResult::SUCCESS);
  sensor_remote.Bind(std::move(params->sensor));

  device::FakeSensor* fake_sensor = fake_sensor_provider()->accelerometer();
  ASSERT_TRUE(fake_sensor);
  EXPECT_FALSE(fake_sensor->is_browser_suspended());

  // Lose focus.
  rwh->SetPageFocus(false);
  static_cast<WebContentsImpl*>(web_contents())
      ->NotifyWebContentsLostFocus(rwh);
  EXPECT_TRUE(fake_sensor->WaitForBrowserSuspend(true));

  // Gain focus again.
  rwh->SetPageFocus(true);
  static_cast<WebContentsImpl*>(web_contents())->NotifyWebContentsFocused(rwh);
  EXPECT_TRUE(fake_sensor->WaitForBrowserSuspend(false));
}

TEST_F(FrameSensorProviderProxyTest,
       GetSensor_InitiallyUnfocused_StartsSuspended) {
  EXPECT_CALL(*permission_manager(),
              GetPermissionResultForCurrentDocument(_, _, _))
      .WillRepeatedly(
          Return(PermissionResult(blink::mojom::PermissionStatus::GRANTED,
                                  PermissionStatusSource::UNSPECIFIED)));

  // Ensure initially unfocused.
  auto* rwh = static_cast<RenderWidgetHostImpl*>(
      main_test_rfh()->GetRenderWidgetHost());
  rwh->SetPageFocus(false);
  static_cast<WebContentsImpl*>(web_contents())
      ->NotifyWebContentsLostFocus(rwh);

  auto provider = GetWebSensorProvider();
  static_cast<TestRenderFrameHost*>(main_test_rfh())->SimulateUserActivation();

  mojo::Remote<device::mojom::Sensor> sensor_remote;
  base::test::TestFuture<device::mojom::SensorCreationResult,
                         device::mojom::SensorInitParamsPtr>
      future;
  provider->GetSensor(device::mojom::SensorType::ACCELEROMETER,
                      /*user_gesture=*/true, future.GetCallback());
  auto [result, params] = future.Take();
  EXPECT_EQ(result, device::mojom::SensorCreationResult::SUCCESS);
  sensor_remote.Bind(std::move(params->sensor));

  device::FakeSensor* fake_sensor = fake_sensor_provider()->accelerometer();
  ASSERT_TRUE(fake_sensor);
  EXPECT_TRUE(fake_sensor->is_browser_suspended());
}

TEST_F(FrameSensorProviderProxyTest, GetSensor_CrossOriginFocus_Suspends) {
  NavigateAndCommit(GURL("https://google.com"));

  EXPECT_CALL(*permission_manager(),
              GetPermissionResultForCurrentDocument(_, _, _))
      .WillRepeatedly(
          Return(PermissionResult(blink::mojom::PermissionStatus::GRANTED,
                                  PermissionStatusSource::UNSPECIFIED)));

  // Ensure main frame is focused initially.
  auto* main_rwh = static_cast<RenderWidgetHostImpl*>(
      main_test_rfh()->GetRenderWidgetHost());
  main_rwh->SetPageFocus(true);
  FocusWebContentsOnFrame(main_test_rfh());

  auto provider = GetWebSensorProvider();
  static_cast<TestRenderFrameHost*>(main_test_rfh())->SimulateUserActivation();

  mojo::Remote<device::mojom::Sensor> sensor_remote;
  base::test::TestFuture<device::mojom::SensorCreationResult,
                         device::mojom::SensorInitParamsPtr>
      future;
  provider->GetSensor(device::mojom::SensorType::ACCELEROMETER,
                      /*user_gesture=*/true, future.GetCallback());
  auto [result, params] = future.Take();
  EXPECT_EQ(result, device::mojom::SensorCreationResult::SUCCESS);
  sensor_remote.Bind(std::move(params->sensor));

  device::FakeSensor* fake_sensor = fake_sensor_provider()->accelerometer();
  ASSERT_TRUE(fake_sensor);
  EXPECT_FALSE(fake_sensor->is_browser_suspended());

  // Create a child frame and navigate it to a cross-origin site.
  TestRenderFrameHost* child_rfh = main_test_rfh()->AppendChild("child");
  ASSERT_NE(nullptr, child_rfh);
  GURL cross_origin_url("https://example.com");
  auto simulator =
      NavigationSimulator::CreateRendererInitiated(cross_origin_url, child_rfh);
  simulator->Commit();
  child_rfh =
      static_cast<TestRenderFrameHost*>(simulator->GetFinalRenderFrameHost());

  // Focus the child frame.
  FocusWebContentsOnFrame(child_rfh);
  // Trigger OnFocusChangedInPage.
  static_cast<WebContentsImpl*>(web_contents())
      ->OnFocusedElementChangedInFrame(child_rfh, gfx::Rect(),
                                       blink::mojom::FocusType::kNone,
                                       blink::DOMNodeIdType(0));

  // The parent frame's sensor should now be suspended because focus is in a
  // cross-origin frame.
  EXPECT_TRUE(fake_sensor->WaitForBrowserSuspend(true));

  // Focus back to main frame.
  FocusWebContentsOnFrame(main_test_rfh());
  static_cast<WebContentsImpl*>(web_contents())
      ->OnFocusedElementChangedInFrame(main_test_rfh(), gfx::Rect(),
                                       blink::mojom::FocusType::kNone,
                                       blink::DOMNodeIdType(0));
  EXPECT_TRUE(fake_sensor->WaitForBrowserSuspend(false));
}

TEST_F(FrameSensorProviderProxyTest,
       GetSensor_SameOriginChildFocus_NotSuspended) {
  NavigateAndCommit(GURL("https://google.com"));

  EXPECT_CALL(*permission_manager(),
              GetPermissionResultForCurrentDocument(_, _, _))
      .WillRepeatedly(
          Return(PermissionResult(blink::mojom::PermissionStatus::GRANTED,
                                  PermissionStatusSource::UNSPECIFIED)));

  // Ensure main frame is focused initially.
  auto* main_rwh = static_cast<RenderWidgetHostImpl*>(
      main_test_rfh()->GetRenderWidgetHost());
  main_rwh->SetPageFocus(true);
  FocusWebContentsOnFrame(main_test_rfh());

  auto provider = GetWebSensorProvider();
  static_cast<TestRenderFrameHost*>(main_test_rfh())->SimulateUserActivation();

  mojo::Remote<device::mojom::Sensor> sensor_remote;
  base::test::TestFuture<device::mojom::SensorCreationResult,
                         device::mojom::SensorInitParamsPtr>
      future;
  provider->GetSensor(device::mojom::SensorType::ACCELEROMETER,
                      /*user_gesture=*/true, future.GetCallback());
  auto [result, params] = future.Take();
  EXPECT_EQ(result, device::mojom::SensorCreationResult::SUCCESS);
  sensor_remote.Bind(std::move(params->sensor));

  device::FakeSensor* fake_sensor = fake_sensor_provider()->accelerometer();
  ASSERT_TRUE(fake_sensor);
  EXPECT_FALSE(fake_sensor->is_browser_suspended());

  // Create a child frame and navigate it to a same-origin site.
  TestRenderFrameHost* child_rfh = main_test_rfh()->AppendChild("child");
  ASSERT_NE(nullptr, child_rfh);
  GURL same_origin_url("https://google.com/foo");
  auto simulator =
      NavigationSimulator::CreateRendererInitiated(same_origin_url, child_rfh);
  simulator->Commit();
  child_rfh =
      static_cast<TestRenderFrameHost*>(simulator->GetFinalRenderFrameHost());

  // Focus the child frame.
  FocusWebContentsOnFrame(child_rfh);
  // Trigger OnFocusChangedInPage.
  static_cast<WebContentsImpl*>(web_contents())
      ->OnFocusedElementChangedInFrame(child_rfh, gfx::Rect(),
                                       blink::mojom::FocusType::kNone,
                                       blink::DOMNodeIdType(0));

  // The parent frame's sensor should NOT be suspended because focus is in a
  // same-origin frame.
  base::test::TestFuture<void> flush_future;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, flush_future.GetCallback());
  EXPECT_TRUE(flush_future.Wait());
  EXPECT_FALSE(fake_sensor->is_browser_suspended());
}
}  // namespace
}  // namespace content
