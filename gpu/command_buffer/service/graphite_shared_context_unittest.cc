// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/graphite_shared_context.h"

#include "base/threading/thread.h"
#include "gpu/command_buffer/common/shm_count.h"
#include "skia/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/dawn/include/dawn/dawn_proc.h"
#include "third_party/dawn/include/dawn/native/DawnNative.h"  // nogncheck
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/gpu/graphite/ContextOptions.h"
#include "third_party/skia/include/gpu/graphite/PrecompileContext.h"
#include "third_party/skia/include/gpu/graphite/Recording.h"
#include "third_party/skia/include/gpu/graphite/Surface.h"
#include "third_party/skia/include/gpu/graphite/dawn/DawnBackendContext.h"

namespace gpu {
namespace {

using testing::NiceMock;

constexpr size_t kMaxPendingRecordings = 100;

class MockGpuProcessShmCount : public GpuProcessShmCount {
 public:
  MOCK_METHOD(void, Increment, (), (override));
  MOCK_METHOD(void, Decrement, (), (override));
};

class MockBackendFlushCallback {
 public:
  MOCK_METHOD(void, Flush, ());
};

// Test fixture for GraphiteSharedContext with thread safety enabled.
class GraphiteSharedContextTest : public testing::TestWithParam<bool> {
 protected:
  GraphiteSharedContextTest() {
    InitializeGraphiteDawn();

    if (is_thread_safe()) {
      secondary_thread_ = std::make_unique<base::Thread>("Secondary_Thread");
      secondary_thread_->StartAndWaitForTesting();
    }
  }

  ~GraphiteSharedContextTest() {
    if (secondary_thread_) {
      secondary_thread_->Stop();
    }
  }

  bool is_thread_safe() const { return GetParam(); }

  void InitializeGraphiteDawn() {
    dawnProcSetProcs(&dawn::native::GetProcs());

    wgpu::InstanceDescriptor instance_desc = {};
    static constexpr auto kTimedWaitAny =
        wgpu::InstanceFeatureName::TimedWaitAny;
    instance_desc.requiredFeatureCount = 1;
    instance_desc.requiredFeatures = &kTimedWaitAny;
    dawn::native::Instance dawn_instance(&instance_desc);

    wgpu::RequestAdapterOptions options = {};
#if BUILDFLAG(IS_APPLE)
    options.backendType = wgpu::BackendType::Metal;
#elif BUILDFLAG(IS_WIN)
    options.backendType = wgpu::BackendType::D3D11;
#else
    // Android, ChromeOS, Fuchsia, Linux all use Vulkan.
    options.backendType = wgpu::BackendType::Vulkan;
    // Force Swiftshader on Linux due to threading issues with native drivers.
    options.forceFallbackAdapter = !!BUILDFLAG(IS_LINUX);
#endif
    options.featureLevel = wgpu::FeatureLevel::Core;

    std::vector<dawn::native::Adapter> adapters =
        dawn_instance.EnumerateAdapters(&options);
    CHECK(!adapters.empty());

    wgpu::DeviceDescriptor device_desc = {};

    wgpu::Device device =
        wgpu::Adapter(adapters[0].Get()).CreateDevice(&device_desc);
    CHECK(device);

    skgpu::graphite::DawnBackendContext backend_context = {};
    backend_context.fInstance = wgpu::Instance(dawn_instance.Get());
    backend_context.fDevice = device;
    backend_context.fQueue = device.GetQueue();

    skgpu::graphite::ContextOptions context_options = {};
    graphite_shared_context_ = std::make_unique<GraphiteSharedContext>(
        skgpu::graphite::ContextFactory::MakeDawn(backend_context,
                                                  context_options),
        &use_shader_cache_shm_count_, is_thread_safe(), kMaxPendingRecordings,
        base::BindRepeating(&MockBackendFlushCallback::Flush,
                            base::Unretained(&backend_flush_callback_)));
  }

  MockGpuProcessShmCount use_shader_cache_shm_count_;
  NiceMock<MockBackendFlushCallback> backend_flush_callback_;
  std::unique_ptr<GraphiteSharedContext> graphite_shared_context_;
  std::unique_ptr<base::Thread> secondary_thread_;
};

TEST_P(GraphiteSharedContextTest, IsThreadSafe) {
  // GraphiteSharedContext graphite_shared_context_ is create with
  // |is_thread_safe| = true in  SetUp(). |lock_| should be allocated and
  // IsThreadSafe() is read back as true.
  EXPECT_EQ(graphite_shared_context_->IsThreadSafe(), is_thread_safe());
}

// Test that multiple threads can safely call methods on GraphiteSharedContext.
TEST_P(GraphiteSharedContextTest, ConcurrentAccess) {
  if (!is_thread_safe()) {
    GTEST_SKIP() << "Concurrent access only supported with thread safe context";
  }

  // Warming up the secondary thread with a no-op function.
  secondary_thread_->task_runner()->PostTask(FROM_HERE, base::BindOnce([]() {
                                               // Do nothing.
                                             }));
  // Flush and wait until it completes
  secondary_thread_->FlushForTesting();

  auto run_graphite_functions =
      [](GraphiteSharedContext* graphite_shared_context) {
        // Call a method that acquires the lock
        std::unique_ptr<skgpu::graphite::Recorder> recorder =
            graphite_shared_context->makeRecorder();
        EXPECT_TRUE(recorder);

        for (int i = 0; i < 2; ++i) {
          for (int j = 0; j < 10; ++j) {
            std::unique_ptr<skgpu::graphite::Recording> recording =
                recorder->snap();
            skgpu::graphite::InsertRecordingInfo info = {};
            info.fRecording = recording.get();
            EXPECT_TRUE(recording);

            bool insert_success =
                graphite_shared_context->insertRecording(info);
            EXPECT_TRUE(insert_success);
          }

          graphite_shared_context->submit();
        }

        EXPECT_FALSE(graphite_shared_context->isDeviceLost());
      };

  // Call graphite::context functions on the secondary thread.
  secondary_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(run_graphite_functions,
                     base::Unretained(graphite_shared_context_.get())));

  // Call graphite::context functions at the same time on the main thread.
  // We should not encounter any failures or deadlocks on either threads.
  run_graphite_functions(graphite_shared_context_.get());

  // Just in case the secondary thread is slower, Wait until finished before
  // exit.
  secondary_thread_->FlushForTesting();
}

TEST_P(GraphiteSharedContextTest, AsyncShaderCompilesFailed) {
  std::unique_ptr<skgpu::graphite::Recorder> recorder =
      graphite_shared_context_->makeRecorder();
  EXPECT_TRUE(recorder);

  auto ii = SkImageInfo::Make(64, 64, kN32_SkColorType, kPremul_SkAlphaType);
  sk_sp<SkSurface> surface1 = SkSurfaces::RenderTarget(recorder.get(), ii);
  surface1->getCanvas()->clear(SK_ColorRED);

  sk_sp<SkSurface> surface2 = SkSurfaces::RenderTarget(recorder.get(), ii);
  surface2->getCanvas()->drawImage(surface1->makeTemporaryImage(), 0, 0);

  std::unique_ptr<skgpu::graphite::Recording> recording = recorder->snap();
  EXPECT_TRUE(recording);

  skgpu::graphite::InsertRecordingInfo info = {};
  info.fRecording = recording.get();
  info.fSimulatedStatus =
      skgpu::graphite::InsertStatus::kAsyncShaderCompilesFailed;

  EXPECT_CALL(use_shader_cache_shm_count_, Increment()).Times(1);
  EXPECT_CALL(use_shader_cache_shm_count_, Decrement()).Times(1);

  EXPECT_FALSE(graphite_shared_context_->insertRecording(info));
}

TEST_P(GraphiteSharedContextTest, AddCommandsFailed) {
  std::unique_ptr<skgpu::graphite::Recorder> recorder =
      graphite_shared_context_->makeRecorder();
  EXPECT_TRUE(recorder);

  auto ii = SkImageInfo::Make(64, 64, kN32_SkColorType, kPremul_SkAlphaType);
  sk_sp<SkSurface> surface1 = SkSurfaces::RenderTarget(recorder.get(), ii);
  surface1->getCanvas()->clear(SK_ColorRED);

  sk_sp<SkSurface> surface2 = SkSurfaces::RenderTarget(recorder.get(), ii);
  surface2->getCanvas()->drawImage(surface1->makeTemporaryImage(), 0, 0);

  std::unique_ptr<skgpu::graphite::Recording> recording = recorder->snap();
  EXPECT_TRUE(recording);

  skgpu::graphite::InsertRecordingInfo info = {};
  info.fRecording = recording.get();
  info.fSimulatedStatus = skgpu::graphite::InsertStatus::kAddCommandsFailed;

  EXPECT_CALL(use_shader_cache_shm_count_, Increment()).Times(0);
  EXPECT_CALL(use_shader_cache_shm_count_, Decrement()).Times(0);

  EXPECT_FALSE(graphite_shared_context_->insertRecording(info));
}

TEST_P(GraphiteSharedContextTest, LowPendingRecordings) {
  std::unique_ptr<skgpu::graphite::Recorder> recorder =
      graphite_shared_context_->makeRecorder();
  EXPECT_TRUE(recorder);

  std::unique_ptr<skgpu::graphite::Recording> recording = recorder->snap();
  EXPECT_TRUE(recording);

  skgpu::graphite::InsertRecordingInfo info = {};
  info.fRecording = recording.get();

  // No flush is expected if the number of pending recordings is low.
  EXPECT_CALL(backend_flush_callback_, Flush()).Times(0);

  for (size_t i = 0; i < kMaxPendingRecordings - 1; ++i) {
    EXPECT_TRUE(graphite_shared_context_->insertRecording(info));
  }
}

TEST_P(GraphiteSharedContextTest, MaxPendingRecordings) {
  std::unique_ptr<skgpu::graphite::Recorder> recorder =
      graphite_shared_context_->makeRecorder();
  EXPECT_TRUE(recorder);

  std::unique_ptr<skgpu::graphite::Recording> recording = recorder->snap();
  EXPECT_TRUE(recording);

  skgpu::graphite::InsertRecordingInfo info = {};
  info.fRecording = recording.get();

  // Expect a flush when the number of pending recordings reaches the max.
  EXPECT_CALL(backend_flush_callback_, Flush()).Times(1);

  for (size_t i = 0; i < kMaxPendingRecordings; ++i) {
    EXPECT_TRUE(graphite_shared_context_->insertRecording(info));
  }
}

INSTANTIATE_TEST_SUITE_P(, GraphiteSharedContextTest, testing::Bool());

}  // namespace
}  // namespace gpu
