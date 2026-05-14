// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/generic_sensor/web_contents_sensor_provider_proxy.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/cpp/test/fake_sensor_and_provider.h"
#include "services/device/public/mojom/sensor.mojom.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/sensor/web_sensor_provider.mojom.h"

namespace content {

namespace {

// MockPermissionManager with a RequestPermission() implementation that always
// grants blink::PermissionType::SENSORS requests.
class TestPermissionManager : public MockPermissionManager {
 public:
  PermissionResult GetPermissionResultForCurrentDocument(
      const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
      RenderFrameHost* render_frame_host,
      bool should_include_device_status) override {
    return PermissionResult(blink::mojom::PermissionStatus::GRANTED,
                            PermissionStatusSource::UNSPECIFIED);
  }

  void RequestPermissionsFromCurrentDocument(
      RenderFrameHost* render_frame_host,
      const PermissionRequestDescription& request_description,
      base::OnceCallback<void(const std::vector<PermissionResult>&)> callback)
      override {
    ASSERT_EQ(request_description.permissions.size(), 1ul);
    ASSERT_EQ(blink::PermissionDescriptorToPermissionType(
                  request_description.permissions[0]),
              blink::PermissionType::SENSORS);
    std::move(callback).Run(
        {PermissionResult(blink::mojom::PermissionStatus::GRANTED,
                          PermissionStatusSource::UNSPECIFIED)});
  }

  void NotifyPermissionChange(blink::PermissionType permission,
                              blink::mojom::PermissionStatus status) {
    if (!subscriptions()) {
      return;
    }
    for (content::PermissionController::SubscriptionsMap::iterator iter(
             subscriptions());
         !iter.IsAtEnd(); iter.Advance()) {
      content::PermissionResultSubscription* subscription =
          iter.GetCurrentValue();
      if (blink::PermissionDescriptorToPermissionType(
              subscription->permission_descriptor) == permission) {
        subscription->callback.Run(
            PermissionResult(status, PermissionStatusSource::UNSPECIFIED),
            /*device_status_changed=*/false);
      }
    }
  }
};

class WebContentsSensorProviderProxyTest
    : public RenderViewHostImplTestHarness {
 public:
  WebContentsSensorProviderProxyTest() = default;
  ~WebContentsSensorProviderProxyTest() override = default;

  WebContentsSensorProviderProxyTest(
      const WebContentsSensorProviderProxyTest&) = delete;
  WebContentsSensorProviderProxyTest& operator=(
      const WebContentsSensorProviderProxyTest&) = delete;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    std::unique_ptr<TestPermissionManager> mock_permission_manager(
        new testing::NiceMock<TestPermissionManager>());
    static_cast<TestBrowserContext*>(browser_context())
        ->SetPermissionControllerDelegate(std::move(mock_permission_manager));

    fake_sensor_provider_ = std::make_unique<device::FakeSensorProvider>();
    WebContentsSensorProviderProxy::OverrideSensorProviderBinderForTesting(
        base::BindRepeating(
            &WebContentsSensorProviderProxyTest::BindSensorProviderReceiver,
            base::Unretained(this)));
  }

  void TearDown() override {
    RenderViewHostImplTestHarness::TearDown();

    WebContentsSensorProviderProxy::OverrideSensorProviderBinderForTesting(
        base::NullCallback());
    fake_sensor_provider_.reset();
  }

 protected:
  void set_fake_sensor_provider(
      std::unique_ptr<device::FakeSensorProvider> fake_sensor_provider) {
    DCHECK(!fake_sensor_provider_->is_bound());
    fake_sensor_provider_ = std::move(fake_sensor_provider);
  }

  device::FakeSensorProvider* fake_sensor_provider() const {
    return fake_sensor_provider_.get();
  }

 private:
  void BindSensorProviderReceiver(
      mojo::PendingReceiver<device::mojom::SensorProvider> receiver) {
    fake_sensor_provider_->Bind(std::move(receiver));
  }

  std::unique_ptr<device::FakeSensorProvider> fake_sensor_provider_;
};

}  //  namespace

// Allows callers to run a custom callback before running
// FakeSensorProvider::GetSensor().
class InterceptingFakeSensorProvider : public device::FakeSensorProvider {
 public:
  explicit InterceptingFakeSensorProvider(
      base::OnceClosure interception_callback)
      : interception_callback_(std::move(interception_callback)) {}

  void GetSensor(
      device::mojom::SensorType type,
      mojo::PendingRemote<device::mojom::SensorConnectionWatcher> watcher,
      GetSensorCallback callback) override {
    std::move(interception_callback_).Run();
    device::FakeSensorProvider::GetSensor(type, std::move(watcher),
                                          std::move(callback));
  }

 private:
  base::OnceClosure interception_callback_;
};

// Test for https://crbug.com/1240814: destroying
// WebContentsSensorProviderProxyTest between calling
// device::mojom::SensorProvider::GetSensor() and it running the callback does
// not crash.
TEST_F(WebContentsSensorProviderProxyTest,
       DestructionOrderWithOngoingCallback) {
  auto intercepting_fake_sensor_provider =
      std::make_unique<InterceptingFakeSensorProvider>(
          base::BindLambdaForTesting([&]() {
            // Delete the current WebContents and consequently trigger
            // WebContentsSensorProviderProxy's destruction before
            // FakeSensorProvider::GetSensor() is invoked and handles the
            // GetSensorCallback it receives.
            DeleteContents();
          }));
  set_fake_sensor_provider(std::move(intercepting_fake_sensor_provider));

  mojo::Remote<blink::mojom::WebSensorProvider> provider;
  contents()->GetPrimaryMainFrame()->GetSensorProvider(
      provider.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  provider.set_disconnect_handler(run_loop.QuitClosure());
  provider->GetSensor(
      device::mojom::SensorType::ACCELEROMETER, /*user_gesture=*/true,
      base::BindOnce([](device::mojom::SensorCreationResult,
                        device::mojom::SensorInitParamsPtr) {
        ADD_FAILURE() << "Reached GetSensor() callback unexpectedly";
      }));
  run_loop.Run();
}

TEST_F(WebContentsSensorProviderProxyTest,
       PermissionRevocationTearsDownConnection) {
  mojo::Remote<blink::mojom::WebSensorProvider> provider;
  contents()->GetPrimaryMainFrame()->GetSensorProvider(
      provider.BindNewPipeAndPassReceiver());

  mojo::Remote<device::mojom::Sensor> sensor_remote;
  base::test::TestFuture<device::mojom::SensorCreationResult,
                         device::mojom::SensorInitParamsPtr>
      get_sensor_future;
  provider->GetSensor(device::mojom::SensorType::ACCELEROMETER,
                      /*user_gesture=*/true, get_sensor_future.GetCallback());
  auto [result, params] = get_sensor_future.Take();
  EXPECT_EQ(device::mojom::SensorCreationResult::SUCCESS, result);
  sensor_remote.Bind(std::move(params->sensor));

  device::FakeSensorProvider* fake_provider = fake_sensor_provider();
  ASSERT_TRUE(fake_provider);
  device::FakeSensor* fake_sensor = fake_provider->accelerometer();
  ASSERT_TRUE(fake_sensor);

  base::RunLoop run_loop;
  fake_sensor->SetWatcherDisconnectCallback(run_loop.QuitClosure());

  // Trigger permission revocation.
  TestPermissionManager* permission_manager =
      static_cast<TestPermissionManager*>(
          browser_context()->GetPermissionControllerDelegate());
  ASSERT_TRUE(permission_manager);
  permission_manager->NotifyPermissionChange(
      blink::PermissionType::SENSORS, blink::mojom::PermissionStatus::DENIED);

  run_loop.Run();
}

}  //  namespace content
