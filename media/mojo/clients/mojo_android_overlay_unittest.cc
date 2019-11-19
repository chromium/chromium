// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "gpu/ipc/common/gpu_surface_tracker.h"
#include "media/base/mock_filters.h"
#include "media/mojo/clients/mojo_android_overlay.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"

using ::testing::StrictMock;
using ::testing::_;

namespace media {

class MojoAndroidOverlayTest : public ::testing::Test {
 public:
  class MockAndroidOverlay : public StrictMock<mojom::AndroidOverlay> {
   public:
    MOCK_METHOD1(ScheduleLayout, void(const gfx::Rect& rect));
  };

  // Handy class with client-level callback mocks.
  class ClientCallbacks {
   public:
    virtual void OnReady(AndroidOverlay*) = 0;
    virtual void OnFailed(AndroidOverlay*) = 0;
    virtual void OnDestroyed(AndroidOverlay*) = 0;
  };

  class MockClientCallbacks : public StrictMock<ClientCallbacks> {
   public:
    MOCK_METHOD1(OnReady, void(AndroidOverlay*));
    MOCK_METHOD1(OnFailed, void(AndroidOverlay*));
    MOCK_METHOD1(OnDestroyed, void(AndroidOverlay*));
    MOCK_METHOD2(OnPowerEfficient, void(AndroidOverlay*, bool));
  };

  class MockAndroidOverlayProvider
      : public StrictMock<mojom::AndroidOverlayProvider> {
   public:
    // These argument types lack default constructors, so gmock can't mock them.
    void CreateOverlay(
        mojo::PendingReceiver<mojom::AndroidOverlay> overlay_receiver,
        mojo::PendingRemote<mojom::AndroidOverlayClient> client,
        mojom::AndroidOverlayConfigPtr config) override {
      overlay_receiver_ = std::move(overlay_receiver);
      client_.Bind(std::move(client));
      config_ = std::move(config);
      OverlayCreated();
    }

    MOCK_METHOD0(OverlayCreated, void(void));

    mojo::PendingReceiver<mojom::AndroidOverlay> overlay_receiver_;
    mojo::Remote<mojom::AndroidOverlayClient> client_;
    mojom::AndroidOverlayConfigPtr config_;
  };

 public:
  MojoAndroidOverlayTest()
      : provider_receiver_(&mock_provider_),
        overlay_receiver_(&mock_overlay_) {}

  ~MojoAndroidOverlayTest() override {}

  void SetUp() override {
    // Set up default config.
    config_.rect = gfx::Rect(100, 200, 300, 400);
    config_.ready_cb = base::Bind(&MockClientCallbacks::OnReady,
                                  base::Unretained(&callbacks_));
    config_.failed_cb = base::Bind(&MockClientCallbacks::OnFailed,
                                   base::Unretained(&callbacks_));
    config_.power_cb = base::Bind(&MockClientCallbacks::OnPowerEfficient,
                                  base::Unretained(&callbacks_));

    // Make sure that we have an implementation of GpuSurfaceLookup.
    gpu::GpuSurfaceTracker::Get();
  }

  void TearDown() override {
    overlay_client_.reset();

    // If we registered a surface, then unregister it.
    if (surface_texture_) {
      gpu::GpuSurfaceTracker::Get()->RemoveSurface(surface_key_);
      // Drop the surface before the surface texture.
      surface_ = gl::ScopedJavaSurface();
    }

    base::RunLoop().RunUntilIdle();
  }

  // Create an overlay in |overlay_client_| using the current config, but do
  // not bind anything to |overlay_receiver_| yet.
  void CreateOverlay() {
    EXPECT_CALL(mock_provider_, OverlayCreated());

    base::UnguessableToken routing_token = base::UnguessableToken::Create();

    overlay_client_.reset(
        new MojoAndroidOverlay(provider_receiver_.BindNewPipeAndPassRemote(),
                               std::move(config_), routing_token));
    overlay_client_->AddSurfaceDestroyedCallback(base::Bind(
        &MockClientCallbacks::OnDestroyed, base::Unretained(&callbacks_)));
    base::RunLoop().RunUntilIdle();
  }

  // Create an overlay, then provide it with |mock_overlay_|.
  void CreateAndInitializeOverlay() {
    CreateOverlay();

    // Bind an overlay to the request.
    overlay_receiver_.Bind(std::move(mock_provider_.overlay_receiver_));
    base::RunLoop().RunUntilIdle();
  }

  // Notify |overlay_client_| that the surface is ready.
  void CreateSurface() {
    EXPECT_CALL(callbacks_, OnReady(overlay_client_.get()));

    // We have to actually add a valid surface, else the client will get mad
    // when it tries to retrieve it.
    surface_texture_ = gl::SurfaceTexture::Create(0);
    surface_ = gl::ScopedJavaSurface(surface_texture_.get());
    surface_key_ = gpu::GpuSurfaceTracker::Get()->AddSurfaceForNativeWidget(
        gpu::GpuSurfaceTracker::SurfaceRecord(
            gfx::kNullAcceleratedWidget, surface_.j_surface().obj(),
            false /* can_be_used_with_surface_control */));

    mock_provider_.client_->OnSurfaceReady(surface_key_);
    base::RunLoop().RunUntilIdle();

    // Verify that we actually got back the right surface.
    JNIEnv* env = base::android::AttachCurrentThread();
    ASSERT_TRUE(env->IsSameObject(surface_.j_surface().obj(),
                                  overlay_client_->GetJavaSurface().obj()));
  }

  // Destroy the overlay.  This includes onSurfaceDestroyed cases.
  void DestroyOverlay() {
    mock_provider_.client_->OnDestroyed();
    base::RunLoop().RunUntilIdle();
  }

  // Mojo stuff.
  base::test::SingleThreadTaskEnvironment task_environment;

  // The mock provider that |overlay_client_| will talk to.
  // |interface_provider_| will bind it.
  MockAndroidOverlayProvider mock_provider_;

  // Receiver for |mock_provider_|.
  mojo::Receiver<mojom::AndroidOverlayProvider> provider_receiver_;

  // The mock overlay impl that |mock_provider_| will provide.
  MockAndroidOverlay mock_overlay_;
  mojo::Receiver<mojom::AndroidOverlay> overlay_receiver_;

  // The client under test.
  std::unique_ptr<AndroidOverlay> overlay_client_;

  // If we create a surface, then these are the SurfaceTexture that owns it,
  // the surface itself, and the key that we registered with GpuSurfaceLookup,
  // respectively.  We could probably mock out GpuSurfaceLookup, but we'd still
  // have to provide a (mock) ScopedJavaSurface, which isn't easy.
  scoped_refptr<gl::SurfaceTexture> surface_texture_;
  gl::ScopedJavaSurface surface_;
  int surface_key_ = 0;

  // Inital config for |CreateOverlay|.
  // Set to sane values, but feel free to modify before CreateOverlay().
  AndroidOverlayConfig config_;
  MockClientCallbacks callbacks_;
};

// Verify basic create => init => ready => destroyed.
TEST_F(MojoAndroidOverlayTest, CreateInitReadyDestroy) {
  CreateAndInitializeOverlay();
  CreateSurface();
  EXPECT_CALL(callbacks_, OnDestroyed(overlay_client_.get()));
  DestroyOverlay();
}

// Verify that initialization failure results in an onDestroyed callback.
TEST_F(MojoAndroidOverlayTest, InitFailure) {
  CreateOverlay();
  EXPECT_CALL(callbacks_, OnFailed(overlay_client_.get()));
  DestroyOverlay();
}

// Verify that we can destroy the overlay before providing a surface.
TEST_F(MojoAndroidOverlayTest, CreateInitDestroy) {
  CreateAndInitializeOverlay();
  EXPECT_CALL(callbacks_, OnFailed(overlay_client_.get()));
  DestroyOverlay();
}

// Test that layouts happen.
TEST_F(MojoAndroidOverlayTest, LayoutOverlay) {
  CreateAndInitializeOverlay();
  CreateSurface();

  gfx::Rect new_layout(5, 6, 7, 8);
  EXPECT_CALL(mock_overlay_, ScheduleLayout(new_layout));
  overlay_client_->ScheduleLayout(new_layout);
}

// Test that layouts are ignored before the client is notified about a surface.
TEST_F(MojoAndroidOverlayTest, LayoutBeforeSurfaceIsIgnored) {
  CreateAndInitializeOverlay();

  gfx::Rect new_layout(5, 6, 7, 8);
  EXPECT_CALL(mock_overlay_, ScheduleLayout(_)).Times(0);
  overlay_client_->ScheduleLayout(new_layout);
}

// Test |secure| makes it to the mojo config when it is true
TEST_F(MojoAndroidOverlayTest, FlagsAreSentViaMojoWhenTrue) {
  config_.secure = true;
  config_.power_efficient = true;
  CreateOverlay();
  ASSERT_TRUE(mock_provider_.config_->secure);
  ASSERT_TRUE(mock_provider_.config_->power_efficient);
}

// Test |secure| makes it to the mojo config when it is false
TEST_F(MojoAndroidOverlayTest, FlagsAreSentViaMojoWhenFalse) {
  config_.secure = false;
  config_.power_efficient = false;
  CreateOverlay();
  ASSERT_FALSE(mock_provider_.config_->secure);
  ASSERT_FALSE(mock_provider_.config_->power_efficient);
}

// Make sure that power efficient cbs are relayed to the application.
TEST_F(MojoAndroidOverlayTest, PowerEfficientCallbackWorks) {
  CreateOverlay();
  EXPECT_CALL(callbacks_, OnPowerEfficient(overlay_client_.get(), true));
  mock_provider_.client_->OnPowerEfficientState(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(callbacks_, OnPowerEfficient(overlay_client_.get(), false));
  mock_provider_.client_->OnPowerEfficientState(false);
  base::RunLoop().RunUntilIdle();
}

}  // namespace media
