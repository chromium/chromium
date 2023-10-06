// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "content/browser/generic_sensor/web_contents_sensor_provider_proxy.h"
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
  void RequestPermissionsFromCurrentDocument(
      RenderFrameHost* render_frame_host,
      const PermissionRequestDescription& request_description,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback)
      override {
    ASSERT_EQ(request_description.permissions.size(), 1ul);
    ASSERT_EQ(request_description.permissions[0],
              blink::PermissionType::SENSORS);
    std::move(callback).Run({blink::mojom::PermissionStatus::GRANTED});
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

  void GetSensor(device::mojom::SensorType type,
                 GetSensorCallback callback) override {
    std::move(interception_callback_).Run();
    device::FakeSensorProvider::GetSensor(type, std::move(callback));
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
  provider->GetSensor(device::mojom::SensorType::ACCELEROMETER,
                      base::BindOnce([](device::mojom::SensorCreationResult,
                                        device::mojom::SensorInitParamsPtr) {
                        ADD_FAILURE()
                            << "Reached GetSensor() callback unexpectedly";
                      }));
  run_loop.Run();
}

}  //  namespace content
