// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_PERFTEST_UTIL_H_
#define IPC_IPC_PERFTEST_UTIL_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/process_metrics.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_test.test-mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace IPC {

scoped_refptr<base::SingleThreadTaskRunner> GetIOThreadTaskRunner();

// This class locks the current thread to a particular CPU core. This is
// important because otherwise the different threads and processes of these
// tests end up on different CPU cores which means that all of the cores are
// lightly loaded so the OS (Windows and Linux) fails to ramp up the CPU
// frequency, leading to unpredictable and often poor performance.
class LockThreadAffinity {
 public:
  explicit LockThreadAffinity(int cpu_number);

  LockThreadAffinity(const LockThreadAffinity&) = delete;
  LockThreadAffinity& operator=(const LockThreadAffinity&) = delete;

  ~LockThreadAffinity();

 private:
  bool affinity_set_ok_;
#if BUILDFLAG(IS_WIN)
  DWORD_PTR old_affinity_;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  cpu_set_t old_cpuset_;
#endif
};

// Avoid core 0 due to conflicts with Intel's Power Gadget.
// Setting thread affinity will fail harmlessly on single/dual core machines.
const int kSharedCore = 2;

class MojoPerfTestClient {
 public:
  MojoPerfTestClient();

  ~MojoPerfTestClient();

  int Run(MojoHandle handle);

 private:
  base::SingleThreadTaskExecutor main_task_executor_;
  std::unique_ptr<Channel> channel_;
  mojo::ScopedMessagePipeHandle handle_;
};

class ReflectorImpl : public IPC::mojom::Reflector {
 public:
  explicit ReflectorImpl(mojo::ScopedMessagePipeHandle handle,
                         base::OnceClosure quit_closure);

  ~ReflectorImpl() override;

 private:
  // IPC::mojom::Reflector:
  void Ping(const std::string& value, PingCallback callback) override;

  void SyncPing(const std::string& value, PingCallback callback) override;

  void Quit() override;

  base::OnceClosure quit_closure_;
  mojo::Receiver<IPC::mojom::Reflector> receiver_;
};

}  // namespace IPC

#endif  // IPC_IPC_PERFTEST_UTIL_H_
