// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_IMPLEMENTATION_BASE_H_
#define GPU_COMMAND_BUFFER_CLIENT_IMPLEMENTATION_BASE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/trace_event/memory_dump_provider.h"
#include "gpu/command_buffer/client/client_transfer_cache.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_impl_export.h"
#include "gpu/command_buffer/client/gpu_control_client.h"
#include "gpu/command_buffer/client/query_tracker.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/context_result.h"

class GrDirectContext;

namespace gpu {

namespace gles2 {
class QueryTracker;
}

class CommandBufferHelper;
class GpuControl;
class MappedMemoryManager;
struct SharedMemoryLimits;

// Base class with functionality shared between GLES2Implementation and
// RasterImplementation.
class GLES2_IMPL_EXPORT ImplementationBase
    : public base::trace_event::MemoryDumpProvider,
      public ContextSupport,
      public GpuControlClient {
 public:
  // The maximum result size from simple GL get commands.
  static const uint32_t kMaxSizeOfSimpleResult =
      16 * sizeof(uint32_t);  // NOLINT.

  // used for testing only. If more things are reseved add them here.
  static const uint32_t kStartingOffset = kMaxSizeOfSimpleResult;

  // Alignment of allocations.
  static const unsigned int kAlignment = 16;

  // The bucket used for results. Public for testing only.
  static const uint32_t kResultBucketId = 1;

  ImplementationBase(CommandBufferHelper* helper,
                     TransferBufferInterface* transfer_buffer,
                     GpuControl* gpu_control);

  ImplementationBase(const ImplementationBase&) = delete;
  ImplementationBase& operator=(const ImplementationBase&) = delete;

  ~ImplementationBase() override;

  void FreeUnusedSharedMemory();
  void FreeEverything();

  // TODO(danakj): Move to ContextSupport once ContextProvider doesn't need to
  // intercept it.
  void SetLostContextCallback(base::OnceClosure callback);

  const Capabilities& capabilities() const { return capabilities_; }

  // ContextSupport implementation.
  void FlushPendingWork() override;
  void SignalSyncToken(const gpu::SyncToken& sync_token,
                       base::OnceClosure callback) override;
  bool IsSyncTokenSignaled(const gpu::SyncToken& sync_token) override;
  void SignalQuery(uint32_t query, base::OnceClosure callback) override;
  void GetGpuFence(uint32_t gpu_fence_id,
                   base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)>
                       callback) override;
  void SetGrContext(GrDirectContext* gr) override;
  bool HasGrContextSupport() const override;
  void WillCallGLFromSkia() override;
  void DidCallGLFromSkia() override;

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // Used by child classes to implement gpu::InterfaceBase
  void GenSyncToken(GLbyte* sync_token);
  void GenUnverifiedSyncToken(GLbyte* sync_token);
  void VerifySyncTokens(GLbyte** sync_tokens, GLsizei count);
  void WaitSyncToken(const GLbyte* sync_token);

 protected:
  gpu::ContextResult Initialize(const SharedMemoryLimits& limits);

  // Waits for all commands to execute.
  bool WaitForCmd();

  // Gets the value of the result.
  template <typename T>
  ScopedResultPtr<T> GetResultAs() {
    return ScopedResultPtr<T>(transfer_buffer_);
  }

  int32_t GetResultShmId();

  // TODO(gman): These bucket functions really seem like they belong in
  // CommandBufferHelper (or maybe BucketHelper?). Unfortunately they need
  // a transfer buffer to function which is currently managed by this class.

  // Gets the contents of a bucket.
  bool GetBucketContents(uint32_t bucket_id, std::vector<int8_t>* data);

  // Sets the contents of a bucket.
  void SetBucketContents(uint32_t bucket_id, const void* data, uint32_t size);

  // Sets the contents of a bucket as a string.
  void SetBucketAsCString(uint32_t bucket_id, const char* str);

  // Gets the contents of a bucket as a string. Returns false if there is no
  // string available which is a separate case from the empty string.
  bool GetBucketAsString(uint32_t bucket_id, std::string* str);

  // Sets the contents of a bucket as a string.
  void SetBucketAsString(uint32_t bucket_id, const std::string& str);

  bool GetVerifiedSyncTokenForIPC(const SyncToken& sync_token,
                                  SyncToken* verified_sync_token);

  void RunIfContextNotLost(base::OnceClosure callback);

  raw_ptr<TransferBufferInterface> transfer_buffer_;

  std::unique_ptr<MappedMemoryManager> mapped_memory_;

  std::unique_ptr<gles2::QueryTracker> query_tracker_;

  base::OnceClosure lost_context_callback_;
  bool lost_context_callback_run_ = false;

  const raw_ptr<GpuControl> gpu_control_;

  Capabilities capabilities_;

 private:
  virtual void IssueShallowFlush() = 0;
  virtual void SetGLError(GLenum error,
                          const char* function_name,
                          const char* msg) = 0;

  raw_ptr<CommandBufferHelper> helper_;

  base::WeakPtrFactory<ImplementationBase> weak_ptr_factory_{this};
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_IMPLEMENTATION_BASE_H_
