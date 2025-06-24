// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_
#define GPU_IPC_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_

#include <memory>
#include <unordered_set>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/hash.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/unsafe_shared_memory_pool.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "gpu/ipc/common/gpu_ipc_common_export.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OZONE)
namespace gfx {
class ClientNativePixmapFactory;
}  // namespace gfx
#endif

namespace gpu {
using GpuMemoryBufferConfigurationKey = gfx::BufferUsageAndFormat;
using GpuMemoryBufferConfigurationSet =
    std::unordered_set<GpuMemoryBufferConfigurationKey>;
}  // namespace gpu

namespace std {
template <>
struct hash<gpu::GpuMemoryBufferConfigurationKey> {
  size_t operator()(const gpu::GpuMemoryBufferConfigurationKey& key) const {
    return base::HashInts(static_cast<int>(key.format),
                          static_cast<int>(key.usage));
  }
};
}  // namespace std

namespace arc {
class GpuArcVideoEncodeAccelerator;
}

namespace media {
class VaapiJpegEncodeAccelerator;
class V4L2JpegEncodeAccelerator;
}  // namespace media

namespace gpu {
class ClientSharedImage;

// Provides a common factory for GPU memory buffer implementations.
class GPU_IPC_COMMON_EXPORT GpuMemoryBufferSupport {
 public:
  GpuMemoryBufferSupport();

  GpuMemoryBufferSupport(const GpuMemoryBufferSupport&) = delete;
  GpuMemoryBufferSupport& operator=(const GpuMemoryBufferSupport&) = delete;

  virtual ~GpuMemoryBufferSupport();

  // Returns the set of supported configurations.
  static GpuMemoryBufferConfigurationSet
  GetNativeGpuMemoryBufferConfigurations();

  // Returns whether the provided buffer format is supported.
  static bool IsNativeGpuMemoryBufferConfigurationSupportedForTesting(
      gfx::BufferFormat format,
      gfx::BufferUsage usage) {
    return IsNativeGpuMemoryBufferConfigurationSupported(format, usage);
  }

  // Returns whether the provided buffer format is supported.
  bool IsConfigurationSupportedForTest(gfx::GpuMemoryBufferType type,
                                       gfx::BufferFormat format,
                                       gfx::BufferUsage usage);

  // Creates a GpuMemoryBufferImpl from the given |handle| for VideoFrames.
  // |size| and |format| should match what was used to allocate the |handle|.
  // NOTE: DO NOT ADD ANY USAGES OF THIS METHOD.
  // TODO(crbug.com/40263579): Remove this method once all usages are
  // eliminated.
  std::unique_ptr<GpuMemoryBufferImpl>
  CreateGpuMemoryBufferImplFromHandleForVideoFrame(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) {
    return CreateGpuMemoryBufferImplFromHandle(std::move(handle), size, format,
                                               usage, base::NullCallback());
  }

  std::unique_ptr<GpuMemoryBufferImpl>
  CreateGpuMemoryBufferImplFromHandleForTesting(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      GpuMemoryBufferImpl::DestructionCallback callback) {
    return CreateGpuMemoryBufferImplFromHandle(std::move(handle), size, format,
                                               usage, std::move(callback));
  }

 private:
  // TODO(crbug.com/404905709): Eliminate these class' creation of GMBs and
  // remove this friending.
  friend class arc::GpuArcVideoEncodeAccelerator;
  friend class media::VaapiJpegEncodeAccelerator;
  friend class media::V4L2JpegEncodeAccelerator;

  // ClientSharedImage is the only entity that should be creating GMBs via
  // GpuMemoryBufferSupport.
  friend class ClientSharedImage;

  // Creates a GpuMemoryBufferImpl from the given |handle|. |size| and |format|
  // should match what was used to allocate the |handle|. |callback|, if
  // non-null, is called when instance is deleted, which is not necessarily on
  // the same thread as this function was called on and instance was created on.
  // |copy_native_buffer_to_shmem_callback| and |pool| are only needed if the
  // created buffer is a windows DXGI buffer and it needs to be mapped at the
  // consumer.
  virtual std::unique_ptr<GpuMemoryBufferImpl>
  CreateGpuMemoryBufferImplFromHandle(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      GpuMemoryBufferImpl::DestructionCallback callback,
      GpuMemoryBufferImpl::CopyNativeBufferToShMemCallback
          copy_native_buffer_to_shmem_callback =
              GpuMemoryBufferImpl::CopyNativeBufferToShMemCallback(),
      scoped_refptr<base::UnsafeSharedMemoryPool> pool = nullptr);

  // Returns whether the provided buffer format is supported.
  static bool IsNativeGpuMemoryBufferConfigurationSupported(
      gfx::BufferFormat format,
      gfx::BufferUsage usage);

#if BUILDFLAG(IS_OZONE)
  std::unique_ptr<gfx::ClientNativePixmapFactory> client_native_pixmap_factory_;
#endif
};

// Helper class to manage allocated GMB info and to provide interface to dump
// the memory consumed by that GMB.
class GPU_IPC_COMMON_EXPORT AllocatedBufferInfo {
 public:
  AllocatedBufferInfo(const gfx::GpuMemoryBufferHandle& handle,
                      const gfx::Size& size,
                      gfx::BufferFormat format);
  ~AllocatedBufferInfo();

  gfx::GpuMemoryBufferType type() const { return type_; }

  // Add a memory dump for this buffer to |pmd|. Returns false if adding the
  // dump failed.
  bool OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    int client_id,
                    uint64_t client_tracing_process_id) const;

 private:
  gfx::GpuMemoryBufferId buffer_id_;
  gfx::GpuMemoryBufferType type_;
  size_t size_in_bytes_;
  base::UnguessableToken shared_memory_guid_;
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_
