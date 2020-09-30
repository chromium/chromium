// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/geolocation_service_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_frame_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "services/device/public/mojom/geolocation.mojom.h"
#include "services/device/public/mojom/geolocation_context.mojom.h"
#include "services/device/public/mojom/geoposition.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom.h"

using blink::mojom::GeolocationService;
using blink::mojom::PermissionStatus;
using device::mojom::Geolocation;
using device::mojom::GeopositionPtr;

namespace content {
namespace {

using PermissionCallback = base::OnceCallback<void(PermissionStatus)>;

double kMockLatitude = 1.0;
double kMockLongitude = 10.0;

class TestPermissionManager : public MockPermissionManager {
 public:
  TestPermissionManager()
      : request_id_(PermissionController::kNoPendingOperation) {}
  ~TestPermissionManager() override = default;

  int RequestPermission(PermissionType permissions,
                        RenderFrameHost* render_frame_host,
                        const GURL& requesting_origin,
                        bool user_gesture,
                        PermissionCallback callback) override {
    EXPECT_EQ(permissions, PermissionType::GEOLOCATION);
    EXPECT_TRUE(user_gesture);
    request_callback_.Run(std::move(callback));
    return request_id_;
  }

  void SetRequestId(int request_id) { request_id_ = request_id; }

  void SetRequestCallback(
      base::RepeatingCallback<void(PermissionCallback)> request_callback) {
    request_callback_ = std::move(request_callback);
  }

 private:
  int request_id_;

  base::RepeatingCallback<void(PermissionCallback)> request_callback_;
};

class GeolocationServiceTest : public RenderViewHostImplTestHarness {
 protected:
  GeolocationServiceTest() {}

  ~GeolocationServiceTest() override {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    NavigateAndCommit(GURL("https://www.google.com/maps"));
    browser_context_.reset(new content::TestBrowserContext());
    browser_context_->SetPermissionControllerDelegate(
        std::make_unique<TestPermissionManager>());

    geolocation_overrider_ =
        std::make_unique<device::ScopedGeolocationOverrider>(kMockLatitude,
                                                             kMockLongitude);
    GetDeviceService().BindGeolocationContext(
        context_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    context_.reset();
    geolocation_overrider_.reset();
    browser_context_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

  void CreateEmbeddedFrameAndGeolocationService(bool allow_via_feature_policy) {
    const GURL kEmbeddedUrl("https://embeddables.com/someframe");
    blink::ParsedFeaturePolicy frame_policy = {};
    if (allow_via_feature_policy) {
      frame_policy.push_back(
          {blink::mojom::FeaturePolicyFeature::kGeolocation,
           std::vector<url::Origin>{url::Origin::Create(kEmbeddedUrl)}, false,
           false});
    }
    RenderFrameHost* embedded_rfh =
        RenderFrameHostTester::For(main_rfh())
            ->AppendChildWithPolicy("", frame_policy);
    RenderFrameHostTester::For(embedded_rfh)->InitializeRenderFrameIfNeeded();
    auto navigation_simulator = NavigationSimulator::CreateRendererInitiated(
        kEmbeddedUrl, embedded_rfh);
    navigation_simulator->Commit();
    embedded_rfh = navigation_simulator->GetFinalRenderFrameHost();

    BrowserContext::SetPermissionControllerForTesting(
        embedded_rfh->GetProcess()->GetBrowserContext(),
        std::make_unique<PermissionControllerImpl>(browser_context_.get()));
    service_.reset(new GeolocationServiceImpl(context_.get(), embedded_rfh));
    service_->Bind(service_remote_.BindNewPipeAndPassReceiver());
  }

  mojo::Remote<blink::mojom::GeolocationService>& service_remote() {
    return service_remote_;
  }

  TestPermissionManager* permission_manager() {
    return static_cast<TestPermissionManager*>(
        browser_context_->GetPermissionControllerDelegate());
  }

 private:
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;

  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<GeolocationServiceImpl> service_;
  mojo::Remote<blink::mojom::GeolocationService> service_remote_;
  mojo::Remote<device::mojom::GeolocationContext> context_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationServiceTest);
};

}  // namespace

TEST_F(GeolocationServiceTest, PermissionGrantedPolicyViolation) {
  // The embedded frame is not allowed.
  CreateEmbeddedFrameAndGeolocationService(/*allow_via_feature_policy=*/false);

  permission_manager()->SetRequestCallback(
      base::BindRepeating([](PermissionCallback callback) {
        ADD_FAILURE() << "Permissions checked unexpectedly.";
      }));
  mojo::Remote<Geolocation> geolocation;
  service_remote()->CreateGeolocation(
      geolocation.BindNewPipeAndPassReceiver(), true,
      base::BindOnce([](blink::mojom::PermissionStatus status) {
        EXPECT_EQ(blink::mojom::PermissionStatus::DENIED, status);
      }));

  base::RunLoop loop;
  geolocation.set_disconnect_handler(loop.QuitClosure());

  geolocation->QueryNextPosition(base::BindOnce([](GeopositionPtr geoposition) {
    ADD_FAILURE() << "Position updated unexpectedly";
  }));
  loop.Run();
}

TEST_F(GeolocationServiceTest, PermissionGrantedNoPolicyViolation) {
  // Allow the embedded frame.
  CreateEmbeddedFrameAndGeolocationService(/*allow_via_feature_policy=*/true);

  permission_manager()->SetRequestCallback(
      base::BindRepeating([](PermissionCallback callback) {
        std::move(callback).Run(PermissionStatus::GRANTED);
      }));
  mojo::Remote<Geolocation> geolocation;
  service_remote()->CreateGeolocation(
      geolocation.BindNewPipeAndPassReceiver(), true,
      base::BindOnce([](blink::mojom::PermissionStatus status) {
        EXPECT_EQ(blink::mojom::PermissionStatus::GRANTED, status);
      }));

  base::RunLoop loop;
  geolocation.set_disconnect_handler(base::BindOnce(
      [] { ADD_FAILURE() << "Connection error handler called unexpectedly"; }));

  geolocation->QueryNextPosition(base::BindOnce(
      [](base::OnceClosure callback, GeopositionPtr geoposition) {
        EXPECT_DOUBLE_EQ(kMockLatitude, geoposition->latitude);
        EXPECT_DOUBLE_EQ(kMockLongitude, geoposition->longitude);
        std::move(callback).Run();
      },
      loop.QuitClosure()));
  loop.Run();
}

TEST_F(GeolocationServiceTest, PermissionGrantedSync) {
  CreateEmbeddedFrameAndGeolocationService(/*allow_via_feature_policy=*/true);
  permission_manager()->SetRequestCallback(
      base::BindRepeating([](PermissionCallback callback) {
        std::move(callback).Run(PermissionStatus::GRANTED);
      }));
  mojo::Remote<Geolocation> geolocation;
  service_remote()->CreateGeolocation(
      geolocation.BindNewPipeAndPassReceiver(), true,
      base::BindOnce([](blink::mojom::PermissionStatus status) {
        EXPECT_EQ(blink::mojom::PermissionStatus::GRANTED, status);
      }));

  base::RunLoop loop;
  geolocation.set_disconnect_handler(base::BindOnce(
      [] { ADD_FAILURE() << "Connection error handler called unexpectedly"; }));

  geolocation->QueryNextPosition(base::BindOnce(
      [](base::OnceClosure callback, GeopositionPtr geoposition) {
        EXPECT_DOUBLE_EQ(kMockLatitude, geoposition->latitude);
        EXPECT_DOUBLE_EQ(kMockLongitude, geoposition->longitude);
        std::move(callback).Run();
      },
      loop.QuitClosure()));
  loop.Run();
}

TEST_F(GeolocationServiceTest, PermissionDeniedSync) {
  CreateEmbeddedFrameAndGeolocationService(/*allow_via_feature_policy=*/true);
  permission_manager()->SetRequestCallback(
      base::BindRepeating([](PermissionCallback callback) {
        std::move(callback).Run(PermissionStatus::DENIED);
      }));
  mojo::Remote<Geolocation> geolocation;
  service_remote()->CreateGeolocation(
      geolocation.BindNewPipeAndPassReceiver(), true,
      base::BindOnce([](blink::mojom::PermissionStatus status) {
        EXPECT_EQ(blink::mojom::PermissionStatus::DENIED, status);
      }));

  base::RunLoop loop;
  geolocation.set_disconnect_handler(loop.QuitClosure());

  geolocation->QueryNextPosition(base::BindOnce([](GeopositionPtr geoposition) {
    ADD_FAILURE() << "Position updated unexpectedly";
  }));
  loop.Run();
}

TEST_F(GeolocationServiceTest, PermissionGrantedAsync) {
  CreateEmbeddedFrameAndGeolocationService(/*allow_via_feature_policy=*/true);
  permission_manager()->SetRequestId(42);
  permission_manager()->SetRequestCallback(
      base::BindRepeating([](PermissionCallback permission_callback) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(permission_callback),
                                      PermissionStatus::GRANTED));
      }));
  mojo::Remote<Geolocation> geolocation;
  service_remote()->CreateGeolocation(
      geolocation.BindNewPipeAndPassReceiver(), true,
      base::BindOnce([](blink::mojom::PermissionStatus status) {
        EXPECT_EQ(blink::mojom::PermissionStatus::GRANTED, status);
      }));

  base::RunLoop loop;
  geolocation.set_disconnect_handler(base::BindOnce(
      [] { ADD_FAILURE() << "Connection error handler called unexpectedly"; }));

  geolocation->QueryNextPosition(base::BindOnce(
      [](base::OnceClosure callback, GeopositionPtr geoposition) {
        EXPECT_DOUBLE_EQ(kMockLatitude, geoposition->latitude);
        EXPECT_DOUBLE_EQ(kMockLongitude, geoposition->longitude);
        std::move(callback).Run();
      },
      loop.QuitClosure()));
  loop.Run();
}

TEST_F(GeolocationServiceTest, PermissionDeniedAsync) {
  CreateEmbeddedFrameAndGeolocationService(/*allow_via_feature_policy=*/true);
  permission_manager()->SetRequestId(42);
  permission_manager()->SetRequestCallback(
      base::BindRepeating([](PermissionCallback permission_callback) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(permission_callback),
                                      PermissionStatus::DENIED));
      }));
  mojo::Remote<Geolocation> geolocation;
  service_remote()->CreateGeolocation(
      geolocation.BindNewPipeAndPassReceiver(), true,
      base::BindOnce([](blink::mojom::PermissionStatus status) {
        EXPECT_EQ(blink::mojom::PermissionStatus::DENIED, status);
      }));

  base::RunLoop loop;
  geolocation.set_disconnect_handler(loop.QuitClosure());

  geolocation->QueryNextPosition(base::BindOnce([](GeopositionPtr geoposition) {
    ADD_FAILURE() << "Position updated unexpectedly";
  }));
  loop.Run();
}

TEST_F(GeolocationServiceTest, ServiceClosedBeforePermissionResponse) {
  CreateEmbeddedFrameAndGeolocationService(/*allow_via_feature_policy=*/true);
  permission_manager()->SetRequestId(42);
  mojo::Remote<Geolocation> geolocation;
  service_remote()->CreateGeolocation(
      geolocation.BindNewPipeAndPassReceiver(), true,
      base::BindOnce([](blink::mojom::PermissionStatus) {
        ADD_FAILURE() << "PositionStatus received unexpectedly.";
      }));
  // Don't immediately respond to the request.
  permission_manager()->SetRequestCallback(base::DoNothing());

  base::RunLoop loop;
  service_remote().reset();

  geolocation->QueryNextPosition(base::BindOnce([](GeopositionPtr geoposition) {
    ADD_FAILURE() << "Position updated unexpectedly";
  }));
  loop.RunUntilIdle();
}

}  // namespace content
