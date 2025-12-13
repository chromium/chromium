// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_DAWN_SERVICE_SERIALIZER_H_
#define GPU_COMMAND_BUFFER_SERVICE_DAWN_SERVICE_SERIALIZER_H_

#include <dawn/wire/WireClient.h>

#include <atomic>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/common/webgpu_cmd_format.h"

namespace gpu {

class DecoderClient;

namespace webgpu {

class DawnServiceSerializer : public dawn::wire::CommandSerializer {
 public:
  explicit DawnServiceSerializer(DecoderClient* client);
  ~DawnServiceSerializer() override;

  // dawn::wire::CommandSerializer implementation.
  // Note that currently, this API requires thread specific handling which
  // incurs costs of checking thread ids and thread_local data. If performance
  // is identified to be an issue here, we may consider developing a new API for
  // the wire to address the issue.
  size_t GetMaximumAllocationSize() const final;
  void* GetCmdSpace(size_t size) final;
  bool Flush() final;

  // This helper only exists for now to continue supporting the non-spontaneous
  // wire server mode if the feature flag is toggled off. This should only ever
  // be called on the main thread (and hence only handles the main thread's
  // CommandBuffer) and will be removed once we fully migrate to the spontaneous
  // handling.
  // TODO(crbug.com/412761856): Remove when spontaneous mode is validated.
  bool NeedsFlush() const;

 private:
  struct CommandBuffer {
    explicit CommandBuffer(size_t size);
    ~CommandBuffer();

    std::vector<uint8_t> buffer;
    size_t put_offset;
  };

  // Common helpers that are used with the internal CommandBuffer struct used
  // both in the main and worker threads.
  void* GetCmdSpace(CommandBuffer& cmd_buffer, size_t size);
  void Flush(CommandBuffer& cmd_buffer);

  // Main thread handlers for dawn::wire::CommandSerializer implementation.
  void* GetCmdSpaceMain(size_t size);
  void FlushMain();

  // Worker thread handlers for dawn::wire::CommandSerializer implementation.
  void* GetCmdSpaceWorker(size_t size);
  void FlushWorker();
  // Helper used to toggle whether a particular worker thread is pending. This
  // returns true if the worker's state was changed.
  bool SetWorkerPending(bool pending);

  scoped_refptr<base::SingleThreadTaskRunner> gpu_main_thread_runner_;
  raw_ptr<DecoderClient, DanglingUntriaged> client_;

  // The main thread's CommandBuffer doesn't need a lock because it will only
  // ever be used on the main thread, and assertions are used to verify that
  // invariant.
  CommandBuffer main_cmds_;

  // The worker threads' CommandBuffer needs a lock because multiple workers may
  // try to allocate and serialize to the buffer at the same time. The condition
  // variable is used to synchronize between the threads when a Flush is needed.
  base::Lock worker_lock_;
  base::ConditionVariable worker_cv_;
  CommandBuffer worker_cmds_;
  size_t pending_workers_ = 0;

  base::WeakPtrFactory<DawnServiceSerializer> weak_ptr_factory_{this};
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_DAWN_SERVICE_SERIALIZER_H_
