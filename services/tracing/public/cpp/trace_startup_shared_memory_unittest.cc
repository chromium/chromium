// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory_switch.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/process/launch.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "components/tracing/common/tracing_switches.h"
#include "mojo/core/embedder/embedder.h"
#include "services/tracing/public/cpp/perfetto/shared_memory.h"
#include "services/tracing/public/cpp/trace_startup.h"
#include "services/tracing/public/cpp/trace_startup_config.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include "base/posix/global_descriptors.h"
#endif

namespace tracing {
namespace {

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
constexpr base::GlobalDescriptors::Key kArbitraryDescriptorKey = 42;
#endif

}  // namespace

TEST(TraceStartupSharedMemoryTest, Create) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kTraceStartup);
  auto shared_memory = CreateTracingOutputSharedMemory();

  ASSERT_TRUE(shared_memory.IsValid());
  EXPECT_EQ(kDefaultSharedMemorySizeBytes, shared_memory.GetSize());
}

MULTIPROCESS_TEST_MAIN(InitFromLaunchParameters) {
// On POSIX we generally use the descriptor map to look up inherited handles.
// On most POSIX platforms we have to manually make sure the mapping is updated,
// for the purposes of this test.
//
// Note:
//  - This doesn't apply on Apple platforms (which use Rendezvous Keys)
//  - On Android the global descriptor table is managed by the launcher
//    service, so we don't have to manually update the mapping here.
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID)
  base::GlobalDescriptors::GetInstance()->Set(
      kArbitraryDescriptorKey,
      kArbitraryDescriptorKey + base::GlobalDescriptors::kBaseDescriptor);
#endif

  EXPECT_FALSE(IsTracingInitialized());

  // On Windows and Fuchsia getting shmem handle from --trace-buffer-handle can
  // only be done once and subsequent calls `UnsafeSharedMemoryRegionFrom()`
  // calls will not get a valid `shmem_region`. So we skip tracing init, to
  // avoid `ConnectProducer()` grabbing the shmem first.
#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_FUCHSIA)
  base::FeatureList::InitInstance("", "");
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("StartupTraceTest");
  tracing::InitTracingPostFeatureList(/*enable_consumer=*/false);

  // Simulate launching with the serialized parameters.
  EnableStartupTracingIfNeeded();
  EXPECT_TRUE(IsTracingInitialized());
  EXPECT_TRUE(base::trace_event::TraceLog::GetInstance()->IsEnabled());
#endif  // !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_FUCHSIA)

  auto* command_line = base::CommandLine::ForCurrentProcess();
  base::UnsafeSharedMemoryRegion unsafe_shm;
  EXPECT_TRUE(command_line->HasSwitch(switches::kTraceBufferHandle));
  auto shmem_region = base::shared_memory::UnsafeSharedMemoryRegionFrom(
      command_line->GetSwitchValueASCII(switches::kTraceBufferHandle));
  EXPECT_TRUE(shmem_region->IsValid());
  EXPECT_EQ(kDefaultSharedMemorySizeBytes, shmem_region->GetSize());

  return 0;
}

class TraceStartupSharedMemoryTest : public ::testing::TestWithParam<bool> {
 protected:
  void Initialize() {
    startup_config_ = base::WrapUnique(new TraceStartupConfig());
  }

  std::unique_ptr<TraceStartupConfig> startup_config_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         TraceStartupSharedMemoryTest,
                         ::testing::Values(/*launch_options.elevated=*/false
#if BUILDFLAG(IS_WIN)
                                           ,
                                           /*launch_options.elevated=*/true
#endif
                                           ));

TEST_P(TraceStartupSharedMemoryTest, PassSharedMemoryRegion) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kTraceStartup);

  Initialize();
  ASSERT_TRUE(startup_config_->IsEnabled());

  auto shm = CreateTracingOutputSharedMemory();
  ASSERT_TRUE(shm.IsValid());

  // Initialize the command line and launch options.
  base::CommandLine command_line =
      base::GetMultiProcessTestChildBaseCommandLine();
  command_line.AppendSwitchASCII("type", "test-child");
  base::LaunchOptions launch_options;

  // On windows, check both the elevated and non-elevated launches.
#if BUILDFLAG(IS_WIN)
  launch_options.start_hidden = true;
  launch_options.elevated = GetParam();
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
  base::ScopedFD descriptor_to_share;
#endif

  // Update the launch parameters.
  AddTraceOutputToLaunchParameters(shm,
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
                                   kArbitraryDescriptorKey, descriptor_to_share,
#endif
                                   &command_line, &launch_options);

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
  // On posix, AddToLaunchParameters() ignores the launch options and instead
  // returns the descriptor to be shared. This is because the browser child
  // launcher helper manages a separate list of files to share via the zygote,
  // if available. If, like in this test scenario, there's ultimately no zygote
  // to use, launch helper updates the launch options to share the descriptor
  // mapping relative to a base descriptor.
  launch_options.fds_to_remap.emplace_back(descriptor_to_share.get(),
                                           kArbitraryDescriptorKey);
#if !BUILDFLAG(IS_ANDROID)
  for (auto& pair : launch_options.fds_to_remap) {
    pair.second += base::GlobalDescriptors::kBaseDescriptor;
  }
#endif  // !BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)

  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);

  // services/test/run_all_unittests.cc sets up ipc_thread, and android's
  // MultiprocessTestClientLauncher.launchClient asserts that child_process
  // cannot be called from main thread, so send this task to the io task runner.
  bool success = mojo::core::GetIOTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WaitableEvent* wait, const base::CommandLine& command_line,
             const base::LaunchOptions& launch_options) {
            // Launch the child process.
            base::Process process = base::SpawnMultiProcessTestChild(
                "InitFromLaunchParameters", command_line, launch_options);

            // The child process returns non-zero if it could not open the
            // shared memory region based on the launch parameters.
            int exit_code = -1;
            EXPECT_TRUE(WaitForMultiprocessTestChildExit(
                process, TestTimeouts::action_timeout(), &exit_code));
            EXPECT_EQ(0, exit_code);
          },
          &wait, command_line, launch_options));

  EXPECT_TRUE(success);
  wait.TimedWait(TestTimeouts::action_timeout());
}

}  // namespace tracing
