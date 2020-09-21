// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/command_line.h"
#include "base/memory/discardable_memory.h"
#include "content/child/child_process.h"
#include "content/child/child_thread_impl.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/discardable_memory_utils.h"

#if defined(OS_POSIX)
#include <sys/mman.h>
#include <sys/utsname.h>

#include "base/memory/madv_free_discardable_memory_allocator_posix.h"
#include "base/memory/madv_free_discardable_memory_posix.h"
#endif

#if defined(OS_ANDROID)
#include <third_party/ashmem/ashmem.h>
#endif

namespace content {

std::unique_ptr<discardable_memory::ClientDiscardableSharedMemoryManager>
CreateDiscardableMemoryAllocator() {
  DVLOG(1) << "Using shared memory for discardable memory";

  mojo::PendingRemote<discardable_memory::mojom::DiscardableSharedMemoryManager>
      manager_remote;
  ChildThread::Get()->BindHostReceiver(
      manager_remote.InitWithNewPipeAndPassReceiver());
  return std::make_unique<
      discardable_memory::ClientDiscardableSharedMemoryManager>(
      std::move(manager_remote), ChildProcess::current()->io_task_runner(),
      ChildProcess::current()->main_thread()->main_thread_runner());
}

}  // namespace content
