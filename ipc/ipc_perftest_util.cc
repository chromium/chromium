// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_perftest_util.h"

#include <tuple>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_proxy.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/test/multiprocess_test_helper.h"

namespace IPC {

scoped_refptr<base::SingleThreadTaskRunner> GetIOThreadTaskRunner() {
  scoped_refptr<base::TaskRunner> runner = mojo::core::GetIOTaskRunner();
  return scoped_refptr<base::SingleThreadTaskRunner>(
      static_cast<base::SingleThreadTaskRunner*>(runner.get()));
}

LockThreadAffinity::LockThreadAffinity(int cpu_number)
    : affinity_set_ok_(false) {
#if BUILDFLAG(IS_WIN)
  const DWORD_PTR thread_mask = static_cast<DWORD_PTR>(1) << cpu_number;
  old_affinity_ = SetThreadAffinityMask(GetCurrentThread(), thread_mask);
  affinity_set_ok_ = old_affinity_ != 0;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  cpu_set_t cpuset;
  UNSAFE_TODO(CPU_ZERO(&cpuset));
  UNSAFE_TODO(CPU_SET(cpu_number, &cpuset));
  auto get_result = sched_getaffinity(0, sizeof(old_cpuset_), &old_cpuset_);
  DCHECK_EQ(0, get_result);
  auto set_result = sched_setaffinity(0, sizeof(cpuset), &cpuset);
  // Check for get_result failure, even though it should always succeed.
  affinity_set_ok_ = (set_result == 0) && (get_result == 0);
#endif
  if (!affinity_set_ok_)
    LOG(WARNING) << "Failed to set thread affinity to CPU " << cpu_number;
}

LockThreadAffinity::~LockThreadAffinity() {
  if (!affinity_set_ok_)
    return;
#if BUILDFLAG(IS_WIN)
  auto set_result = SetThreadAffinityMask(GetCurrentThread(), old_affinity_);
  DCHECK_NE(0u, set_result);
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  auto set_result = sched_setaffinity(0, sizeof(old_cpuset_), &old_cpuset_);
  DCHECK_EQ(0, set_result);
#endif
}

MojoPerfTestClient::MojoPerfTestClient() {
  mojo::core::test::MultiprocessTestHelper::ChildSetup();
}

MojoPerfTestClient::~MojoPerfTestClient() = default;

int MojoPerfTestClient::Run(MojoHandle handle) {
  handle_ = mojo::MakeScopedHandle(mojo::MessagePipeHandle(handle));
  LockThreadAffinity thread_locker(kSharedCore);

  base::RunLoop run_loop;
  std::unique_ptr<ChannelProxy> channel = IPC::ChannelProxy::Create(
      handle_.release(), Channel::MODE_CLIENT, nullptr, GetIOThreadTaskRunner(),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  run_loop.Run();
  return 0;
}

ReflectorImpl::ReflectorImpl(mojo::ScopedMessagePipeHandle handle,
                             base::OnceClosure quit_closure)
    : quit_closure_(std::move(quit_closure)),
      receiver_(
          this,
          mojo::PendingReceiver<IPC::mojom::Reflector>(std::move(handle))) {}

ReflectorImpl::~ReflectorImpl() {
  std::ignore = receiver_.Unbind().PassPipe().release();
}

void ReflectorImpl::Ping(const std::string& value, PingCallback callback) {
  std::move(callback).Run(value);
}

void ReflectorImpl::SyncPing(const std::string& value, PingCallback callback) {
  std::move(callback).Run(value);
}

void ReflectorImpl::Quit() {
  if (quit_closure_)
    std::move(quit_closure_).Run();
}

}  // namespace IPC
