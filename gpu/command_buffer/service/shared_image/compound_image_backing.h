// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_COMPOUND_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_COMPOUND_IMAGE_BACKING_H_

#include <vector>

#include "base/containers/enum_set.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_memory_image_backing.h"
#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {

class D3DImageBackingFactoryTest;
class SharedImageBackingFactory;
class SharedImageCopyManager;
class SharedImageFactory;

// TODO(kylechar): Merge with OzoneImageBacking::AccessStream enum.
enum class SharedImageAccessStream {
  kSkia,
  kOverlay,
  kGL,
  kDawn,
  kMemory,
  kVaapi
};

// Used to represent what access streams a backing can be used for.
using AccessStreamSet = base::EnumSet<SharedImageAccessStream,
                                      SharedImageAccessStream::kSkia,
                                      SharedImageAccessStream::kVaapi>;

// A compound backing that combines a shared memory backing and real GPU
// backing. The real GPU backing must implement `UploadFromMemory()` and not
// have its own shared memory segment.
// TODO(crbug.com/40213543): Support multiple GPU backings.
class GPU_GLES2_EXPORT CompoundImageBacking
    : public ClearTrackingSharedImageBacking {
 public:
  using CreateBackingCallback =
      base::OnceCallback<void(std::unique_ptr<SharedImageBacking>&)>;

  static bool IsValidSharedMemoryBufferFormat(const gfx::Size& size,
                                              viz::SharedImageFormat format);

  // Remove the SCANOUT flag if |kAllowShmOverlays|.
  static SharedImageUsageSet GetGpuSharedImageUsage(SharedImageUsageSet usage);

  // Creates a backing that contains a shared memory backing and GPU backing
  // provided by `shared_image_factory` based on `usage`. Eventually, instead of
  // creating a shm+gpu backing, this method will have various strategy to
  // allocate different combination of backings based on the `usage`.
  static std::unique_ptr<SharedImageBacking> Create(
      SharedImageFactory* shared_image_factory,
      scoped_refptr<SharedImageCopyManager> copy_manager,
      const Mailbox& mailbox,
      gfx::GpuMemoryBufferHandle handle,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label);

  // Creates a backing that contains a shared memory backing and GPU backing
  // provided by `shared_image_factory` based on `usage`. Eventually, instead of
  // creating a shm+gpu backing, this method will have various strategy to
  // allocate different combination of backings based on the `usage`.
  // We additionally pass a |buffer_usage| parameter here in order to create a
  // CPU mappable by creating a shared memory handle.
  // TODO(crbug.com/40276878): Remove this method once we figure out the mapping
  // between SharedImageUsage and BufferUsage and no longer need to use
  // BufferUsage.
  static std::unique_ptr<SharedImageBacking> Create(
      SharedImageFactory* shared_image_factory,
      scoped_refptr<SharedImageCopyManager> copy_manager,
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label,
      gfx::BufferUsage buffer_usage);

  ~CompoundImageBacking() override;

  // Called by wrapped representations before access. This will update
  // the backing that is going to be accessed if most recent pixels are in
  // a different backing.
  void NotifyBeginAccess(SharedImageBacking* backing,
                         RepresentationAccessMode mode);

  // SharedImageBacking implementation.
  SharedImageBackingType GetType() const override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  bool CopyToGpuMemoryBuffer() override;
  void CopyToGpuMemoryBufferAsync(
      base::OnceCallback<void(bool)> callback) override;
  gfx::Rect ClearedRect() const override;

  // CompoundImageBacking now supports partial clear for upcoming use
  // cases as it evolves. The cleared rect is now tracked on the compound
  // backing as well as its underlying backings.
  // Some important things to note that is :
  // 1. When a CompoundImageBacking is backed by a single gpu backing, clear
  // rect of CompoundImageBacking will track and reflect clear rect of the
  // underlying backing.
  // 2. When CompoundImageBacking contains more than 1 gpu backing, clear rect
  // of the CompoundImageBacking will track and reflect clear rect of the
  // most recently written backing. Note that when a read is performed from a
  // stale backing, the latest backing content as well as its clear rect will
  // be copied into it.
  // 3. Anytime a copy is performend between backings, the src backing's cleared
  // rect will be xfered to the dst backing.
  // 4. If there is a shm backing, entire CompoundImageBacking as well all the
  // created gpu backings will be marked as cleared always.

  void SetClearedRect(const gfx::Rect& cleared_rect) override;
  void OnAddSecondaryReference() override;

  // CompoundImageBacking is registered as the primary backing while creating a
  // SharedImageRepresentationFactoryRef whereas the underlying
  // elements/backings it holds are not. Since the MarkForDestruction() method
  // in SharedImageRepresentationFactoryRef only runs for primary backing,
  // CompoundImageBacking needs to propagate this call to all its elements.
  void MarkForDestruction() override;
  gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle() override;

 protected:
  // SharedImageBacking implementation.
  std::unique_ptr<DawnImageRepresentation> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      const wgpu::Device& device,
      wgpu::BackendType backend_type,
      std::vector<wgpu::TextureFormat> view_formats,
      scoped_refptr<SharedContextState> context_state) override;
  std::unique_ptr<GLTextureImageRepresentation> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;
  std::unique_ptr<GLTexturePassthroughImageRepresentation>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override;
  std::unique_ptr<SkiaGaneshImageRepresentation> ProduceSkiaGanesh(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;
  std::unique_ptr<SkiaGraphiteImageRepresentation> ProduceSkiaGraphite(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;
  std::unique_ptr<OverlayImageRepresentation> ProduceOverlay(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

 private:
  friend class CompoundImageBackingTest;
  friend class D3DImageBackingFactoryTest;

  // Holds one element, aka SharedImageBacking and related information, that
  // makes up the compound.
  struct ElementHolder {
   public:
    ElementHolder();
    ElementHolder(const ElementHolder& other) = delete;
    ElementHolder& operator=(const ElementHolder& other) = delete;
    ElementHolder(ElementHolder&& other);
    ElementHolder& operator=(ElementHolder&& other);
    ~ElementHolder();

    // Will invoke `create_callback` to create backing if
    // required.
    void CreateBackingIfNecessary();

    // Returns the backing. Will call `CreateBackingIfNecessary()`.
    SharedImageBacking* GetBacking();

    AccessStreamSet access_streams;
    uint32_t content_id_ = 0;

    CreateBackingCallback create_callback;
    std::unique_ptr<SharedImageBacking> backing;
  };

  // Creates a backing that contains a shared memory backing and GPU backing
  // provided by `gpu_backing_factory`.
  static std::unique_ptr<SharedImageBacking> CreateSharedMemoryForTesting(
      SharedImageBackingFactory* gpu_backing_factory,
      scoped_refptr<SharedImageCopyManager> copy_manager,
      const Mailbox& mailbox,
      gfx::GpuMemoryBufferHandle handle,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label);

  // Creates a backing that contains a shared memory backing and GPU backing
  // provided by `gpu_backing_factory`. We additionally pass a |buffer_usage|
  // parameter here in order to create a CPU mappable by creating a shared
  // memory handle.
  // TODO(crbug.com/40276878): Remove this method once we figure out the mapping
  // between SharedImageUsage and BufferUsage and no longer need to use
  // BufferUsage.
  static std::unique_ptr<SharedImageBacking> CreateSharedMemoryForTesting(
      SharedImageBackingFactory* gpu_backing_factory,
      scoped_refptr<SharedImageCopyManager> copy_manager,
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label,
      gfx::BufferUsage buffer_usage);

  CompoundImageBacking(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label,
      std::unique_ptr<SharedImageBacking> shm_backing,
      base::WeakPtr<SharedImageFactory> shared_image_factory,
      base::WeakPtr<SharedImageBackingFactory> gpu_backing_factory,
      scoped_refptr<SharedImageCopyManager> copy_manager,
      std::optional<gfx::BufferUsage> buffer_usage = std::nullopt);

  base::trace_event::MemoryAllocatorDump* OnMemoryDump(
      const std::string& dump_name,
      base::trace_event::MemoryAllocatorDumpGuid client_guid,
      base::trace_event::ProcessMemoryDump* pmd,
      uint64_t client_tracing_id) override;

  // Returns a SkPixmap for shared memory backing.
  const std::vector<SkPixmap>& GetSharedMemoryPixmaps();

  // Returns the shared memory element used for access stream
  // SharedImageAccessStream::kMemory. There can be only 1 shared memory element
  // at most.
  ElementHolder& GetShmElement();

  // Gets the element corresponding to the backing.
  ElementHolder* GetElement(const SharedImageBacking* backing);

  // Finds the element which has the most recent data/content irrespective of
  // the stream. There could be multiple elements which has the most recent
  // data. This method finds the first element which has most recent data.
  ElementHolder* GetElementWithLatestContent();

  // Gets or allocates a backing for a given |stream|.
  // If a backing with a given |stream| is present, it will either return the
  // backing with the latest content OR will return any supported backing (the
  // first one it finds).
  // If no backing is found, then it will allocate an appropriate backing which
  // can support the |stream|.
  SharedImageBacking* GetOrAllocateBacking(SharedImageAccessStream stream);

  // Returns the gpu backing from the list of |element_| which has a shm and a
  // gpu backing.
  SharedImageBacking* GetGpuBacking();

  bool HasLatestContent(ElementHolder& element);

  // Sets the element used for `stream` as having the latest content. If
  // `write_access` is true then only that element has the latest content.
  void SetLatestContent(SharedImageAccessStream stream, bool write_access);

  // Runs CreateSharedImage() on `factory` and stores the result in `backing`.
  // If successful this will update the estimated size of compound backing.
  void CreateBackingFromBackingFactory(
      base::WeakPtr<SharedImageBackingFactory> factory,
      std::string debug_label,
      std::unique_ptr<SharedImageBacking>& backing);

  void OnCopyToGpuMemoryBufferComplete(bool success);

  // This is required for CompoundImageBacking to be able to query an
  // appropriate SharedImageBackingFactory dynamically based on clients
  // required usage(Produce*) which typically happens after the backing
  // creation time. WeakPtr since backings can outlive SharedImageFactory.
  // Note that CompoundImageBacking is not thread-safe at this moment and
  // we would need to switch WeakPtr to something else if we make it
  // thread-safe.
  base::WeakPtr<SharedImageFactory> shared_image_factory_;

  uint32_t latest_content_id_ = 1;

  // Holds all of the "element" backings that make up this compound backing. For
  // each there is a backing, set of streams and tracking for latest content.
  //
  // It's expected that for each access stream there is exactly one element used
  // to access it. Note that it's possible the backing for a given access stream
  // can't actually support that type of usage, in which case the backing will
  // be null or the ProduceX() call will just fail.
  // As of now, CompoundImageBacking only has 2 backings,i.e., 1 shm and 1 gpu
  // backing. In future, it will evolve into a dynamic CompoundImageBacking
  // where it can have any number of gpu backings and at most 1 cpu backing.
  std::vector<ElementHolder> elements_;

  base::OnceCallback<void(bool)> pending_copy_to_gmb_callback_;

  scoped_refptr<SharedImageCopyManager> copy_manager_;

  bool has_shm_backing_ = false;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_COMPOUND_IMAGE_BACKING_H_
