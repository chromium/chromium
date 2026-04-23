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
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/permission_result.h"
#include "content/public/test/mock_permission_manager.h"
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
      .WillOnce(Return(PermissionResult(blink::mojom::PermissionStatus::GRANTED,
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
      .WillOnce(Return(PermissionResult(blink::mojom::PermissionStatus::ASK,
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
      .WillOnce(Return(PermissionResult(blink::mojom::PermissionStatus::ASK,
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

}  // namespace
}  // namespace content
