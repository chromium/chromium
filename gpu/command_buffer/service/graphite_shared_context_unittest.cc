// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/graphite_shared_context.h"

#include "base/threading/thread.h"
#include "skia/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/gpu/graphite/ContextOptions.h"
#include "third_party/skia/include/gpu/graphite/PrecompileContext.h"
#include "third_party/skia/include/gpu/graphite/Recording.h"

#if BUILDFLAG(SKIA_USE_DAWN)
#include "third_party/dawn/include/dawn/dawn_proc.h"
#include "third_party/dawn/include/dawn/native/DawnNative.h"  // nogncheck
#include "third_party/skia/include/gpu/graphite/dawn/DawnBackendContext.h"
#endif

namespace gpu {
#if BUILDFLAG(SKIA_USE_DAWN)

std::unique_ptr<skgpu::graphite::Context> MakeGraphiteContext() {
  static std::unique_ptr<dawn::native::Instance> instance;
  DawnProcTable backend_procs = dawn::native::GetProcs();
  dawnProcSetProcs(&backend_procs);
  wgpu::InstanceDescriptor instance_desc{};
  instance_desc.capabilities.timedWaitAnyEnable = true;
  instance = std::make_unique<dawn::native::Instance>(&instance_desc);

  wgpu::DawnTogglesDescriptor toggles_desc;
  toggles_desc.enabledToggleCount = 0;

#if BUILDFLAG(IS_MAC)
  wgpu::BackendType backend = wgpu::BackendType::Metal;
#elif BUILDFLAG(IS_WIN)
  wgpu::BackendType backend = wgpu::BackendType::D3D11;
#else
  wgpu::BackendType backend = wgpu::BackendType::Vulkan;
#endif
  wgpu::RequestAdapterOptions options;
  options.featureLevel = wgpu::FeatureLevel::Core;
  options.nextInChain = &toggles_desc;

  std::vector<dawn::native::Adapter> adapters =
      instance->EnumerateAdapters(&options);
  CHECK(!adapters.empty());

  dawn::native::Adapter matched_adaptor;
  for (const auto& adapter : adapters) {
    wgpu::Adapter wgpuAdapter = adapter.Get();
    wgpu::AdapterInfo props;
    wgpuAdapter.GetInfo(&props);
    if (backend == props.backendType) {
      matched_adaptor = adapter;
      break;
    }
  }

  if (!matched_adaptor) {
    return nullptr;
  }

  wgpu::DeviceDescriptor device_desc;
  device_desc.requiredFeatureCount = 0;
  device_desc.nextInChain = &toggles_desc;

  wgpu::Device device =
      wgpu::Device::Acquire(matched_adaptor.CreateDevice(&device_desc));
  CHECK(device);

  skgpu::graphite::DawnBackendContext backend_context;
  backend_context.fInstance = wgpu::Instance(instance->Get());
  backend_context.fDevice = device;
  backend_context.fQueue = device.GetQueue();
  skgpu::graphite::ContextOptions context_options = {};

  return skgpu::graphite::ContextFactory::MakeDawn(backend_context,
                                                   context_options);
}

// Test fixture for GraphiteSharedContext with thread safety enabled.
class GraphiteSharedContextThreadSafeTest : public testing::Test {
 public:
  void SetUp() override {
    std::unique_ptr<skgpu::graphite::Context> graphite_context =
        MakeGraphiteContext();

    // Only test with a Dawn adapter that is thread-safe. Skip the test if this
    // adapter is not available.
    if (!graphite_context) {
      GTEST_SKIP() << "Graphite context creation failed, skipping test.";
    }

    graphite_shared_context_ = std::make_unique<GraphiteSharedContext>(
        std::move(graphite_context), /*is_thread_safe=*/true);

    secondary_thread_.StartAndWaitForTesting();
  }

  void TearDown() override { secondary_thread_.Stop(); }

  base::Thread* secondary_thread() { return &secondary_thread_; }

  GraphiteSharedContext* graphite_shared_context() {
    return graphite_shared_context_.get();
  }

  // Function to be executed by each thread.  This will call the methods on
  // GraphiteSharedContext.
  void RunGraphiteFunctions() {
    // Call a method that acquires the lock
    std::unique_ptr<skgpu::graphite::Recorder> recorder =
        graphite_shared_context()->makeRecorder();
    EXPECT_TRUE(recorder);

    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 10; ++j) {
        std::unique_ptr<skgpu::graphite::Recording> recording =
            recorder->snap();
        skgpu::graphite::InsertRecordingInfo info = {};
        info.fRecording = recording.get();
        EXPECT_TRUE(recording);

        bool insert_success = graphite_shared_context()->insertRecording(info);
        EXPECT_TRUE(insert_success);
      }

      bool submit_success = graphite_shared_context()->submit();
      EXPECT_TRUE(submit_success);
    }

    EXPECT_FALSE(graphite_shared_context()->isDeviceLost());
  }

 private:
  std::unique_ptr<GraphiteSharedContext> graphite_shared_context_;
  base::Thread secondary_thread_ = base::Thread("Secondary_Thread");
};

TEST_F(GraphiteSharedContextThreadSafeTest, IsThreadSafe) {
  // GraphiteSharedContext graphite_shared_context_ is create with
  // |is_thread_safe| = true in  SetUp(). |lock_| should be allocated and
  // IsThreadSafe() is read back as true.
  EXPECT_TRUE(graphite_shared_context()->IsThreadSafe());
}

// Test that multiple threads can safely call methods on GraphiteSharedContext.
TEST_F(GraphiteSharedContextThreadSafeTest, ConcurrentAccess) {
  // Warming up the secondary thread with a no-op function.
  secondary_thread()->task_runner()->PostTask(FROM_HERE, base::BindOnce([]() {
                                                // Do nothing.
                                              }));
  // Flush and wait until it completes
  secondary_thread()->FlushForTesting();

  // Call graphite::context functions on the secondary thread.
  secondary_thread()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GraphiteSharedContextThreadSafeTest::RunGraphiteFunctions,
                     base::Unretained(this)));

  // Call graphite::context functions at the same time on the main thread.
  // We should not encounter any failures or deadlocks on either threads.
  RunGraphiteFunctions();

  // Just in case the secondary thread is slower, Wait until finished before
  // exit.
  secondary_thread()->FlushForTesting();
}

#endif  // #if BUILDFLAG(SKIA_USE_DAWN)
}  // namespace gpu
