// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/geolocation_service_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/permission_controller.h"
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
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

using blink::mojom::GeolocationService;
using blink::mojom::PermissionStatus;
using device::mojom::Geolocation;
using device::mojom::GeopositionPtr;

namespace content {
namespace {

using PermissionCallback = base::OnceCallback<void(
    const std::vector<blink::mojom::PermissionStatus>&)>;

double kMockLatitude = 1.0;
double kMockLongitude = 10.0;

class TestPermissionManager : public MockPermissionManager {
 public:
  TestPermissionManager() = default;
  ~TestPermissionManager() override = default;

  void RequestPermissionsFromCurrentDocument(
      const std::vector<blink::PermissionType>& permissions,
      content::RenderFrameHost* render_frame_host,
      bool user_gesture,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback)
      override {
    ASSERT_EQ(permissions.size(), 1u);
    EXPECT_EQ(permissions[0], blink::PermissionType::GEOLOCATION);
    EXPECT_TRUE(user_gesture);
    request_callback_.Run(std::move(callback));
  }

  void SetRequestCallback(
      base::RepeatingCallback<void(PermissionCallback)> request_callback) {
    request_callback_ = std::move(request_callback);
  }

 private:
  base::RepeatingCallback<void(PermissionCallback)> request_callback_;
};

class GeolocationServiceTest : public RenderViewHostImplTestHarness {
 public:
  GeolocationServiceTest(const GeolocationServiceTest&) = delete;
  GeolocationServiceTest& operator=(const GeolocationServiceTest&) = delete;

 protected:
  GeolocationServiceTest() {}

  ~GeolocationServiceTest() override {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    NavigateAndCommit(GURL("https://www.google.com/maps"));
    browser_context_ = std::make_unique<content::TestBrowserContext>();
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

  void CreateEmbeddedFrameAndGeolocationService(
      bool allow_via_permissions_policy) {
    const GURL kEmbeddedUrl("https://embeddables.com/someframe");
    blink::ParsedPermissionsPolicy frame_policy = {};
    if (allow_via_permissions_policy) {
      frame_policy.push_back(
          {blink::mojom::PermissionsPolicyFeature::kGeolocation,
           std::vector{blink::OriginWithPossibleWildcards(
               url::Origin::Create(kEmbeddedUrl),
               /*has_subdomain_wildcard=*/false)},
           false, false});
    }
    RenderFrameHost* embedded_rfh =
        RenderFrameHostTester::For(main_rfh())
            ->AppendChildWithPolicy("", frame_policy);
    RenderFrameHostTester::For(embedded_rfh)->InitializeRenderFrameIfNeeded();
    auto navigation_simulator = NavigationSimulator::CreateRendererInitiated(
        kEmbeddedUrl, embedded_rfh);
    navigation_simulator->Commit();
    embedded_rfh = navigation_simulator->GetFinalRenderFrameHost();

    embedded_rfh->GetProcess()
        ->GetBrowserContext()
        ->SetPermissionControllerForTesting(
            std::make_unique<PermissionControllerImpl>(browser_context_.get()));
    service_ =
        std::make_unique<GeolocationServiceImpl>(context_.get(), embedded_rfh);
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
};

}  // namespace

TEST_F(GeolocationServiceTest, PermissionGrantedPolicyViolation) {
  // The embedded frame is not allowed.
  CreateEmbeddedFrameAndGeolocationService(
      /*allow_via_permissions_policy=*/false);

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
  CreateEmbeddedFrameAndGeolocationService(
      /*allow_via_permissions_policy=*/true);

  permission_manager()->SetRequestCallback(
      base::BindRepeating([](PermissionCallback callback) {
        std::move(callback).Run(std::vector{PermissionStatus::GRANTED});
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
  CreateEmbeddedFrameAndGeolocationService(
      /*allow_via_permissions_policy=*/true);
  permission_manager()->SetRequestCallback(
      base::BindRepeating([](PermissionCallback callback) {
        std::move(callback).Run(std::vector{PermissionStatus::GRANTED});
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
  CreateEmbeddedFrameAndGeolocationService(
      /*allow_via_permissions_policy=*/true);
  permission_manager()->SetRequestCallback(
      base::BindRepeating([](PermissionCallback callback) {
        std::move(callback).Run(std::vector{PermissionStatus::DENIED});
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
  CreateEmbeddedFrameAndGeolocationService(
      /*allow_via_permissions_policy=*/true);
  permission_manager()->SetRequestCallback(
      base::BindRepeating([](PermissionCallback permission_callback) {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(permission_callback),
                                      std::vector{PermissionStatus::GRANTED}));
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
  CreateEmbeddedFrameAndGeolocationService(
      /*allow_via_permissions_policy=*/true);
  permission_manager()->SetRequestCallback(
      base::BindRepeating([](PermissionCallback permission_callback) {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(permission_callback),
                                      std::vector{PermissionStatus::DENIED}));
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
  CreateEmbeddedFrameAndGeolocationService(
      /*allow_via_permissions_policy=*/true);
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
