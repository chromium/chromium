// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/discardable_memory_utils.h"

#include <utility>

#include "base/memory/discardable_memory.h"
#include "content/child/child_process.h"
#include "content/child/child_thread_impl.h"

namespace content {

scoped_refptr<discardable_memory::ClientDiscardableSharedMemoryManager>
CreateDiscardableMemoryAllocator() {
  DVLOG(1) << "Using shared memory for discardable memory";

  mojo::PendingRemote<discardable_memory::mojom::DiscardableSharedMemoryManager>
      manager_remote;
  ChildThread::Get()->BindHostReceiver(
      manager_remote.InitWithNewPipeAndPassReceiver());
  return base::MakeRefCounted<
      discardable_memory::ClientDiscardableSharedMemoryManager>(
      std::move(manager_remote), ChildProcess::current()->io_task_runner());
}

}  // namespace content
