// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/gpu_host/gpu_host.h"

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "components/viz/host/gpu_client.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "components/viz/test/gpu_host_impl_test_api.h"
#include "gpu/config/gpu_info.h"
#include "services/ws/gpu_host/gpu_host_delegate.h"
#include "services/ws/gpu_host/gpu_host_test_api.h"
#include "services/ws/public/mojom/gpu.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/init/gl_factory.h"

namespace ws {
namespace gpu_host {
namespace test {
namespace {

// No-opt implementation of GpuHostDelegate.
class TestGpuHostDelegate : public GpuHostDelegate {
 public:
  TestGpuHostDelegate() {}
  ~TestGpuHostDelegate() override {}

  // GpuHostDelegate:
  void OnGpuServiceInitialized() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestGpuHostDelegate);
};

// Test implementation of GpuService. For testing behaviour of calls made by
// viz::GpuClient.
class TestGpuService : public viz::GpuServiceImpl {
 public:
  explicit TestGpuService(
      scoped_refptr<base::SingleThreadTaskRunner> io_runner);
  ~TestGpuService() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestGpuService);
};

TestGpuService::TestGpuService(
    scoped_refptr<base::SingleThreadTaskRunner> io_runner)
    : GpuServiceImpl(gpu::GPUInfo(),
                     nullptr /* watchdog_thread */,
                     std::move(io_runner),
                     gpu::GpuFeatureInfo(),
                     gpu::GpuPreferences(),
                     base::nullopt,
                     base::nullopt,
                     nullptr /* vulkan_implementation */,
                     /*exit_callback=*/base::DoNothing()) {}

}  // namespace

class GpuHostTest : public testing::Test {
 public:
  GpuHostTest() : io_thread_("IOThread") {
    CHECK(io_thread_.Start());
    gpu_service_ = std::make_unique<TestGpuService>(io_thread_.task_runner());
  }
  ~GpuHostTest() override {
    gpu_service_ = nullptr;
    io_thread_.Stop();
  }

  base::WeakPtr<viz::GpuClient> AddGpuClient();
  void DestroyHost();

  // testing::Test
  void SetUp() override;
  void TearDown() override;

 private:
  base::MessageLoop message_loop_;

  base::Thread io_thread_;
  TestGpuHostDelegate gpu_host_delegate_;
  discardable_memory::DiscardableSharedMemoryManager
      discardable_memory_manager_;
  std::unique_ptr<TestGpuService> gpu_service_;
  viz::mojom::GpuServicePtr gpu_service_ptr_;
  std::unique_ptr<GpuHost> gpu_host_;

  DISALLOW_COPY_AND_ASSIGN(GpuHostTest);
};

base::WeakPtr<viz::GpuClient> GpuHostTest::AddGpuClient() {
  gpu_host_->Add(mojom::GpuRequest());
  return GpuHostTestApi(gpu_host_.get()).GetLastGpuClient();
}

void GpuHostTest::DestroyHost() {
  gpu_host_.reset();
}

void GpuHostTest::SetUp() {
  testing::Test::SetUp();
  gpu_host_ = std::make_unique<GpuHost>(&gpu_host_delegate_, nullptr,
                                        &discardable_memory_manager_);
  gpu_service_->Bind(mojo::MakeRequest(&gpu_service_ptr_));
  GpuHostTestApi(gpu_host_.get()).SetGpuService(std::move(gpu_service_ptr_));
}

void GpuHostTest::TearDown() {
  gpu_host_ = nullptr;
  gl::init::ShutdownGL(false);
  testing::Test::TearDown();
}

// Tests to verify, that if a GpuHost is deleted before viz::GpuClient receives
// a callback, that viz::GpuClient is torn down and does not attempt to use
// GpuInfo after deletion. This should not crash on asan-builds.
TEST_F(GpuHostTest, GpuClientDestructionOrder) {
  base::WeakPtr<viz::GpuClient> client_ref = AddGpuClient();
  EXPECT_NE(nullptr, client_ref);
  DestroyHost();
  EXPECT_EQ(nullptr, client_ref);
}

TEST_F(GpuHostTest, GpuClientDestroyedWhileChannelRequestInFlight) {
  base::WeakPtr<viz::GpuClient> client_ref = AddGpuClient();
  mojom::Gpu* gpu = client_ref.get();
  bool callback_called = false;
  gpu->EstablishGpuChannel(
      base::Bind([](bool* callback_called, int, mojo::ScopedMessagePipeHandle,
                    const gpu::GPUInfo&,
                    const gpu::GpuFeatureInfo&) { *callback_called = true; },
                 &callback_called));
  EXPECT_FALSE(callback_called);
  DestroyHost();
  EXPECT_TRUE(callback_called);
}

}  // namespace test
}  // namespace gpu_host
}  // namespace ws
