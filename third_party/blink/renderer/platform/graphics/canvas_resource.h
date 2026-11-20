// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/byte_size.h"
#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "base/feature_list.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/shared_image_pool.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "skia/buildflags.h"
#include "third_party/blink/public/platform/web_graphics_shared_image_interface_provider.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"

class SkSurface;

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_H_

namespace blink {

PLATFORM_EXPORT BASE_DECLARE_FEATURE(kCanvas2DReclaimUnusedResources);
PLATFORM_EXPORT BASE_DECLARE_FEATURE(kAppendCpuUsages);
PLATFORM_EXPORT BASE_DECLARE_FEATURE(kCanvasResourceIsWebGPUCompatible);

inline constexpr base::TimeDelta kUnusedResourceExpirationTime =
    base::Seconds(5);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
enum class CanvasResourceProviderType {
  kTexture [[deprecated]] = 0,
  kBitmap = 1,
  kSharedBitmap [[deprecated]] = 2,
  kTextureGpuMemoryBuffer [[deprecated]] = 3,
  kBitmapGpuMemoryBuffer [[deprecated]] = 4,
  kSharedImage = 5,
  kDirectGpuMemoryBuffer [[deprecated]] = 6,
  kPassThrough [[deprecated]] = 7,
  kSwapChain [[deprecated]] = 8,
  kSkiaDawnSharedImage [[deprecated]] = 9,
  kExternalBitmap [[deprecated]] = 10,
  kMaxValue = kExternalBitmap,
};
#pragma GCC diagnostic pop

// Resource type for SharedImage
class PLATFORM_EXPORT CanvasResource final : public gpu::ClientImage {
 public:
  using ReleaseCallback = base::OnceCallback<void(
      scoped_refptr<blink::CanvasResource> canvas_resource,
      const gpu::SyncToken& sync_token,
      bool is_lost)>;

  explicit CanvasResource(scoped_refptr<gpu::ClientSharedImage> shared_image);

  static scoped_refptr<CanvasResource> CreateForTesting(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      gpu::SharedImageUsageSet shared_image_usage_flags,
      bool is_software,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      base::WeakPtr<WebGraphicsSharedImageInterfaceProvider>);

  void InitializeSoftware(base::WeakPtr<WebGraphicsSharedImageInterfaceProvider>
                              shared_image_interface_provider);
  void Initialize();
  bool IsInitialized() const { return is_initialized_; }

  // Save (and wait on) this sync token on the context used by this resource for
  // rendering.
  // TODO(crbug.com/40286368): completely defer the waiting to the
  // zero-parameter variant of WaitSyncToken().
  void WaitSyncToken(const gpu::SyncToken&);

  const gpu::SyncToken& acquire_sync_token() const {
    return acquire_sync_token_;
  }

  // Called when the resource is marked lost. Losing a resource does not mean
  // that the backing memory has been destroyed, since the resource itself keeps
  // a ref on that memory.
  // It means that the consumer (commonly the compositor) can not provide a sync
  // token for the resource to be safely recycled and its the GL state may be
  // inconsistent with when the resource was given to the compositor. So it
  // should not be recycled for writing again but can be safely read from.
  void NotifyResourceLost();

  bool IsLost() const {
    DCHECK(!is_cross_thread());
    return resource_is_lost_;
  }

  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    const std::string& parent_path) const override;

  // Uploads the contents of |sk_surface| to the resource's backing memory.
  // Should be called only if the resource is using software raster.
  void UploadSoftwareRenderingResults(SkSurface* sk_surface);

  bool is_cross_thread() const {
    return base::PlatformThread::CurrentRef() != owning_thread_ref_;
  }

  const gpu::SyncToken& sync_token() const { return GetSyncToken(); }

  ~CanvasResource() override;

 private:
  SkImageInfo CreateSkImageInfo() const;

  const base::PlatformThreadRef owning_thread_ref_;

  // These members are either only accessed on the owning thread, or are only
  // updated on the owning thread and then are read on a different thread.
  // We ensure to correctly update their state in Transfer, which is called
  // before a resource is used on a different thread.
  bool resource_is_lost_ = false;
  gpu::SyncToken acquire_sync_token_;

  // Accessed on any thread.
  bool is_initialized_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_H_
