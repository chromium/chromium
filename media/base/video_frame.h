// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_FRAME_H_
#define MEDIA_BASE_VIDEO_FRAME_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/process/memory.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/types/id_type.h"
#include "base/types/pass_key.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "crypto/hash.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_frame_metadata.h"
#include "media/base/video_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/hdr_metadata.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/files/scoped_file.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
#include "gpu/vulkan/vulkan_ycbcr_info.h"
#endif

namespace gfx {
struct GpuMemoryBufferHandle;
class ClientNativePixmapFactory;
}

namespace gpu {
class LegacyGpuMemoryBufferForVideo;
}

namespace media {

class MEDIA_EXPORT VideoFrame : public base::RefCountedThreadSafe<VideoFrame> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  static constexpr size_t kFrameSizeAlignment = 16;
  static constexpr size_t kFrameSizePadding = 16;

  static constexpr size_t kFrameAddressAlignment =
      VideoFrameLayout::kBufferAddressAlignment;

  static constexpr size_t kMaxPlanes = 4;

  enum Plane : uint8_t {
    kY = 0,
    kARGB = kY,
    kU = 1,
    kUV = kU,
    kV = 2,
    kATriPlanar = kV,
    kA = 3,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // Defines the pixel storage type. Differentiates between directly accessible
  // |data_| and pixels that are only indirectly accessible and not via mappable
  // memory.
  // Note that VideoFrames of any StorageType can also have Texture backing,
  // with "classical" GPU Driver-only textures identified as STORAGE_OPAQUE.
  enum StorageType {
    STORAGE_UNKNOWN = 0,
    STORAGE_OPAQUE = 1,  // We don't know how VideoFrame's pixels are stored.
    STORAGE_UNOWNED_MEMORY = 2,  // External, non owned data pointers.
    STORAGE_OWNED_MEMORY = 3,  // VideoFrame has allocated its own data buffer.
    STORAGE_SHMEM = 4,         // Backed by read-only shared memory.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    STORAGE_DMABUFS = 5,  // Each plane is stored into a DmaBuf.
#endif
    STORAGE_GPU_MEMORY_BUFFER = 6,
    STORAGE_MAX = STORAGE_GPU_MEMORY_BUFFER,
  };

  // CB to be called on the mailbox backing this frame when the frame is
  // destroyed.
  using ReleaseMailboxCB = base::OnceCallback<void(const gpu::SyncToken&)>;

  // Interface representing client operations on a SyncToken, i.e. insert one in
  // the GPU Command Buffer and wait for it.
  class SyncTokenClient {
   public:
    SyncTokenClient() = default;
    SyncTokenClient(const SyncTokenClient&) = delete;
    SyncTokenClient& operator=(const SyncTokenClient&) = delete;

    virtual void GenerateSyncToken(gpu::SyncToken* sync_token) = 0;
    virtual void WaitSyncToken(const gpu::SyncToken& sync_token) = 0;

   protected:
    virtual ~SyncTokenClient() = default;
  };

  // This class will allow VF clients to be able to get CPU mapped memory and
  // other metadata either from GMB or MappableSI backing the VF. Note that this
  // class will go away once GMB is removed. Clients can directly called a new
  // method like VideoFrame::MapSharedImage() to get a
  // ClientSharedImage::ScopedMapping object.
  class MEDIA_EXPORT ScopedMapping {
   public:
    virtual ~ScopedMapping() = default;

    // Returns a span pointing to the plane's memory.
    virtual base::span<uint8_t> GetMemoryAsSpan(uint32_t plane_index) = 0;

    // Returns plane stride.
    virtual size_t Stride(uint32_t plane_index) = 0;

    // Returns the size of the buffer.
    virtual gfx::Size Size() = 0;
  };

  enum class FrameControlType {
    kNone,
    kEos,
  };

  // Clients must use the static factory/wrapping methods to create a new frame.
  VideoFrame(base::PassKey<VideoFrame>,
             const VideoFrameLayout& layout,
             StorageType storage_type,
             const gfx::Rect& visible_rect,
             const gfx::Size& natural_size,
             base::TimeDelta timestamp,
             FrameControlType frame_control_type = FrameControlType::kNone);
  VideoFrame() = delete;
  VideoFrame(const VideoFrame&) = delete;
  VideoFrame& operator=(const VideoFrame&) = delete;

  // Returns true if size is valid for a VideoFrame.
  static bool IsValidSize(const gfx::Size& coded_size,
                          const gfx::Rect& visible_rect,
                          const gfx::Size& natural_size);

  // Returns true if and only if the |size| is within limits, i.e., neither
  // dimension exceeds limits::kMaxDimension and the total area doesn't exceed
  // limits::kMaxCanvas. Prefer the overload that accepts the |coded_size|,
  // |visible_rect|, and |natural_size| if trying to validate the VideoFrame.
  static bool IsValidCodedSize(const gfx::Size& size);

  // Returns true if frame configuration is valid.
  static bool IsValidConfig(VideoPixelFormat format,
                            StorageType storage_type,
                            const gfx::Size& coded_size,
                            const gfx::Rect& visible_rect,
                            const gfx::Size& natural_size);

  // Compute a strided layout for `format` and `coded_size`, and return a fully
  // specified layout including offsets and plane sizes. Except that VideoFrame
  // knows how to compute plane sizes, this method should be in
  // `VideoFrameLayout`, probably just folded into `CreateWithStrides()`.
  static std::optional<VideoFrameLayout> CreateFullySpecifiedLayoutWithStrides(
      VideoPixelFormat format,
      const gfx::Size& coded_size);

  // Creates a new frame in system memory with given parameters. Buffers for the
  // frame are allocated but not initialized. The caller must not make
  // assumptions about the actual underlying size(s), but check the returned
  // VideoFrame instead.
  static scoped_refptr<VideoFrame> CreateFrame(VideoPixelFormat format,
                                               const gfx::Size& coded_size,
                                               const gfx::Rect& visible_rect,
                                               const gfx::Size& natural_size,
                                               base::TimeDelta timestamp);

  // Used by Chromecast only.
  // Create a new frame that doesn't contain any valid video content. This frame
  // is meant to be sent to compositor to inform that the compositor should
  // punch a transparent hole so the video underlay will be visible.
  // |overlay_plane_id| is stored in metadata() as |tracking_token|.
  static scoped_refptr<VideoFrame> CreateVideoHoleFrame(
      const base::UnguessableToken& overlay_plane_id,
      const gfx::Size& natural_size,
      base::TimeDelta timestamp);

  // Creates a frame that doesn't contain any valid video content.
  // |tracking_token| is stored in the frame metadata and can be used to look up
  // the underlying resource or VideoFrame source. The resulting frame's
  // |storage_type_| will be STORAGE_OPAQUE.
  static scoped_refptr<VideoFrame> WrapTrackingToken(
      VideoPixelFormat format,
      const base::UnguessableToken& tracking_token,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      base::TimeDelta timestamp);

  // Offers the same functionality as CreateFrame, and additionally zeroes out
  // the initial allocated buffers.
  static scoped_refptr<VideoFrame> CreateZeroInitializedFrame(
      VideoPixelFormat format,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      base::TimeDelta timestamp);

  // Creates a new frame in system memory with given parameters. Buffers for the
  // frame are allocated but not initialized. The caller should specify the
  // physical buffer size and strides if needed in |layout| parameter.
  static scoped_refptr<VideoFrame> CreateFrameWithLayout(
      const VideoFrameLayout& layout,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      base::TimeDelta timestamp,
      bool zero_initialize_memory);

  // Wraps a native texture shared image with a VideoFrame.
  // |mailbox_holder_release_cb| will be called with a sync token as the
  // argument when the VideoFrame is to be destroyed.
  static scoped_refptr<VideoFrame> WrapSharedImage(
      VideoPixelFormat format,
      scoped_refptr<gpu::ClientSharedImage> shared_image,
      gpu::SyncToken sync_token,
      ReleaseMailboxCB mailbox_holder_release_cb,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      base::TimeDelta timestamp);

  // Wraps a mappable shared image with a VideoFrame. Mappable Shared Images are
  // backed by CPU mappable gpu buffers or shared memory buffers.
  // TODO(crbug.com/40263579): Once all VideoFrame clients are fully converted
  // to use MappableSI, look into refactoring this method and
  // ::WrapSharedImage() into one. |mailbox_holder_release_cb| will be called
  // with a sync token as the argument when the VideoFrame is to be destroyed.
  static scoped_refptr<VideoFrame> WrapMappableSharedImage(
      scoped_refptr<gpu::ClientSharedImage> shared_image,
      gpu::SyncToken sync_token,
      ReleaseMailboxCB mailbox_holder_release_cb,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      base::TimeDelta timestamp);

  // Wraps packed image data residing in a memory buffer with a VideoFrame.
  // The image data resides in |data| and is assumed to be packed tightly in a
  // buffer of logical dimensions |coded_size| with the appropriate bit depth
  // and plane count as given by |format|. Returns NULL on failure.
  static scoped_refptr<VideoFrame> WrapExternalData(
      VideoPixelFormat format,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      base::span<const uint8_t> data,
      base::TimeDelta timestamp);

  static scoped_refptr<VideoFrame> WrapExternalDataWithLayout(
      const VideoFrameLayout& layout,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      base::span<const uint8_t> data,
      base::TimeDelta timestamp);

  // Wraps external YUV data of the given parameters with a VideoFrame.
  // The returned VideoFrame does not own the data passed in.
  static scoped_refptr<VideoFrame> WrapExternalYuvData(
      VideoPixelFormat format,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      size_t y_stride,
      size_t u_stride,
      size_t v_stride,
      base::span<const uint8_t> y_data,
      base::span<const uint8_t> u_data,
      base::span<const uint8_t> v_data,
      base::TimeDelta timestamp);

  static scoped_refptr<VideoFrame> WrapExternalYuvDataWithLayout(
      const VideoFrameLayout& layout,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      base::span<const uint8_t> y_data,
      base::span<const uint8_t> u_data,
      base::span<const uint8_t> v_data,
      base::TimeDelta timestamp);

  // Wraps external YUVA data of the given parameters with a VideoFrame.
  // The returned VideoFrame does not own the data passed in.
  static scoped_refptr<VideoFrame> WrapExternalYuvaData(
      VideoPixelFormat format,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      size_t y_stride,
      size_t u_stride,
      size_t v_stride,
      size_t a_stride,
      base::span<const uint8_t> y_data,
      base::span<const uint8_t> u_data,
      base::span<const uint8_t> v_data,
      base::span<const uint8_t> a_data,
      base::TimeDelta timestamp);

  // Wraps external NV12 data of the given parameters with a VideoFrame.
  // The returned VideoFrame does not own the data passed in.
  static scoped_refptr<VideoFrame> WrapExternalYuvData(
      VideoPixelFormat format,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      size_t y_stride,
      size_t uv_stride,
      base::span<const uint8_t> y_data,
      base::span<const uint8_t> uv_data,
      base::TimeDelta timestamp);

#if BUILDFLAG(IS_CHROMEOS)
  // Wraps |handle|. For use in contexts where the GPUMemoryBufferHandle has no
  // SharedImage associated with it.
  static scoped_refptr<VideoFrame> WrapExternalGpuMemoryBufferHandle(
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      gfx::ClientNativePixmapFactory* client_native_pixmap_factory,
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& coded_size,
      viz::SharedImageFormat format,
      gfx::BufferUsage usage,
      base::TimeDelta timestamp);
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Wraps provided dmabufs
  // (https://www.kernel.org/doc/html/latest/driver-api/dma-buf.html) with a
  // VideoFrame. The frame will take ownership of |dmabuf_fds|, and will
  // automatically close() them on destruction. Callers can duplicate the file
  // descriptors if they need to retain a copy of the FDs for themselves. Note
  // that the FDs are consumed even in case of failure.
  // The image data is only accessible via dmabuf fds, which are usually passed
  // directly to a hardware device and/or to another process, or can also be
  // mapped via mmap() for CPU access.
  // Returns NULL on failure.
  static scoped_refptr<VideoFrame> WrapExternalDmabufs(
      const VideoFrameLayout& layout,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      std::vector<base::ScopedFD> dmabuf_fds,
      base::TimeDelta timestamp);
#endif

#if BUILDFLAG(IS_APPLE)
  // Wraps a provided IOSurface with a VideoFrame. The IOSurface is retained
  // and locked for the lifetime of the VideoFrame. This is for unaccelerated
  // (CPU-only) access to the IOSurface, and is not efficient. It is the path
  // that video capture uses when hardware acceleration is disabled.
  // https://crbug.com/1125879
  static scoped_refptr<VideoFrame> WrapUnacceleratedIOSurface(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      base::TimeDelta timestamp);
#endif

  // Wraps |frame|. |visible_rect| must be a sub rect within
  // frame->visible_rect().
  static scoped_refptr<VideoFrame> WrapVideoFrame(
      scoped_refptr<VideoFrame> frame,
      VideoPixelFormat format,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size);

  // Creates a frame which indicates end-of-stream.
  static scoped_refptr<VideoFrame> CreateEOSFrame();

  // Allocates YV12 frame based on |size|, and sets its data to the YUV(y,u,v).
  static scoped_refptr<VideoFrame> CreateColorFrame(const gfx::Size& size,
                                                    uint8_t y,
                                                    uint8_t u,
                                                    uint8_t v,
                                                    base::TimeDelta timestamp);

  // Allocates YV12 frame based on |size|, and sets its data to the YUV
  // equivalent of RGB(0,0,0).
  static scoped_refptr<VideoFrame> CreateBlackFrame(const gfx::Size& size);

  // Allocates YV12A frame based on |size|, and sets its data to the YUVA
  // equivalent of RGBA(0,0,0,0).
  static scoped_refptr<VideoFrame> CreateTransparentFrame(
      const gfx::Size& size);

  static size_t NumPlanes(VideoPixelFormat format);

  // Returns the required allocation size for a (tightly packed) frame of the
  // given coded size and format.
  static size_t AllocationSize(VideoPixelFormat format,
                               const gfx::Size& coded_size);

  // Returns |dimensions| adjusted to appropriate boundaries based on |format|.
  static gfx::Size DetermineAlignedSize(VideoPixelFormat format,
                                        const gfx::Size& dimensions);

  // Returns the plane gfx::Size (in bytes) for a plane of the given coded size
  // and format.
  static gfx::Size PlaneSize(VideoPixelFormat format,
                             size_t plane,
                             const gfx::Size& coded_size);

  // Returns the plane gfx::Size (in samples) for a plane of the given coded
  // size and format.
  static gfx::Size PlaneSizeInSamples(VideoPixelFormat format,
                                      size_t plane,
                                      const gfx::Size& coded_size);

  // Returns horizontal bits per pixel for given |plane| and |format|.
  static int PlaneHorizontalBitsPerPixel(VideoPixelFormat format, size_t plane);

  // Returns bits per pixel for given |plane| and |format|.
  static int PlaneBitsPerPixel(VideoPixelFormat format, size_t plane);

  // Returns the number of bytes per row for the given plane, format, and width.
  // The width may be aligned to format requirements.
  static size_t RowBytes(size_t plane, VideoPixelFormat format, int width);

  // Returns the number of bytes per element for given |plane| and |format|.
  static int BytesPerElement(VideoPixelFormat format, size_t plane);

  // Calculates strides for each plane based on |format| and |coded_size|.
  static std::vector<size_t> ComputeStrides(VideoPixelFormat format,
                                            const gfx::Size& coded_size);

  // Returns the number of rows for the given plane, format, and height.
  // The height may be aligned to format requirements.
  static size_t Rows(size_t plane, VideoPixelFormat format, int height);

  // Returns the number of columns for the given plane, format, and width.
  // The width may be aligned to format requirements.
  static size_t Columns(size_t plane, VideoPixelFormat format, int width);

  // Given a crypto/hash Hasher, hash in the pixels from a single VideoFrame.
  // If `visible_data_only` is true only the frame's visible area will be
  // hashed, if false then the entire coded frame area will be hashed.
  static void UpdateHashWithFrameForTesting(crypto::hash::Hasher& hasher,
                                            const VideoFrame& frame,
                                            bool visible_data_only = true);

  // Convenience wrapper around UpdateHashWithFrameForTesting(): produces the
  // SHA-256 hash of a single video frame's pixels, as a lowercase hex string.
  static std::string HexHashOfFrameForTesting(const VideoFrame& frame,
                                              bool visible_data_only = true);

  // Returns true if |frame| is accessible mapped in the VideoFrame memory
  // space.
  // static
  static bool IsStorageTypeMappable(VideoFrame::StorageType storage_type);

  // Returns true if |plane| is a valid plane index for the given |format|.
  static bool IsValidPlane(VideoPixelFormat format, size_t plane);

  // Returns the pixel size of each subsample for a given |plane| and |format|.
  // E.g. 2x2 for the U-plane in PIXEL_FORMAT_I420.
  static gfx::Size SampleSize(VideoPixelFormat format, size_t plane);

  // Returns a human readable string of StorageType.
  static std::string StorageTypeToString(VideoFrame::StorageType storage_type);

  // A video frame wrapping external data may be backed by a read-only shared
  // memory region. These methods are used to appropriately transform a
  // VideoFrame created with WrapExternalData, WrapExternalYuvaData, etc. The
  // storage type of the Video Frame will be changed to STORAGE_READ_ONLY_SHMEM.
  // Once the backing of a VideoFrame is set, it cannot be changed.
  //
  // The region is NOT owned by the video frame. Both the region and its
  // associated mapping must outlive this instance.
  void BackWithSharedMemory(const base::ReadOnlySharedMemoryRegion* region);

  // As above, but the VideoFrame owns the shared memory region as well as the
  // mapping. They will be destroyed with their VideoFrame.
  void BackWithOwnedSharedMemory(base::ReadOnlySharedMemoryRegion region,
                                 base::ReadOnlySharedMemoryMapping mapping);

  // Valid for shared memory backed VideoFrames.
  const base::ReadOnlySharedMemoryRegion* shm_region() const {
    DCHECK(IsValidSharedMemoryFrame());
    DCHECK(storage_type_ == STORAGE_SHMEM);
    return shm_region_;
  }

  // Returns true if |frame| is accessible and mapped in the VideoFrame memory
  // space. If false, clients should refrain from accessing data(),
  // visible_data() etc.
  bool IsMappable() const;

  // Returns true if the video frame uses ClientSharedImage.
  bool HasSharedImage() const;

  // Returns true if the |storage_type_| is STOAGE_GPU_MEMORY_BUFFER which
  // indicates that the VideoFrame is backed by GMB or a MappableSharedImage
  // when its enabled.
  bool HasMappableGpuBuffer() const;

  // Returns true if the GpuMemoruBuffer backing the video frame is native
  // buffer and not shared memory buffer. A native GPU memory buffer is a
  // block of memory that is allocated and managed directly on the GPU's
  // memory which allows for hardware acceleration.
  bool HasNativeGpuMemoryBuffer() const;

  // Returns true if |is_mappable_si_enabled_| is true, means MappableSI is
  // enabled for this frame.
  bool IsMappableSharedImageEnabled() const;

  // Gets the ScopedMapping object which clients can use to access the CPU
  // visible memory and other metadata for the gpu buffer backing this
  // VideoFrame(via GpuMemoryBuffer or MappableSI).
  // TODO(crbug.com/40263579): Note that once MappableSI is fully launched and
  // enabled for VideoFrame, rename this method to MapSharedImage(). It can
  // then directly return ClientSharedImage::ScopedMapping object instead.
  std::unique_ptr<VideoFrame::ScopedMapping> MapGMBOrSharedImage() const;

  // Gets the ScopedMapping object which clients can use to access the CPU
  // visible memory and other metadata for the gpu buffer backing this
  // VideoFrame(via GpuMemoryBuffer or MappableSI).
  // This isn't guaranteed to be always async.
  // If 'AsyncMappingIsNonBlocking()' is 'false', this will run the callback
  // in the current sequence. Otherwise, the callback will be invoked in the
  // GpuMemoryThread.
  // Note: the frame must not be destroyed before the result callback is
  // executed.
  // TODO(crbug.com/40263579): Note that once MappableSI is fully launched and
  // enabled for VideoFrame, rename this method to MapSharedImageAsync(). It can
  // then directly return ClientSharedImage::ScopedMapping object instead.
  void MapGMBOrSharedImageAsync(
      base::OnceCallback<void(std::unique_ptr<VideoFrame::ScopedMapping>)>
          result_cb) const;

  // Returns true if the underlying SharedImage or GMB can be mapped truly
  // asynchronously: with an unblocking request to the GPU process.
  // Only call if `HasMappableGpuBuffer() == true`.
  bool AsyncMappingIsNonBlocking() const;

  // Gets the GpuMemoryBufferHandle backing the VideoFrame.
  gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle() const;

  // Returns true if the video frame was created with the given parameters.
  bool IsSameAllocation(VideoPixelFormat format,
                        const gfx::Size& coded_size,
                        const gfx::Rect& visible_rect,
                        const gfx::Size& natural_size) const;

  // Returns the color space of this frame's content.
  gfx::ColorSpace ColorSpace() const;
  void set_color_space(const gfx::ColorSpace& color_space) {
    color_space_ = color_space;
  }

  // Return the full-range RGB component of the color space of this frame's
  // content. This will replace several color spaces (Rec601, Rec709, and
  // Apple's Rec709) with sRGB, for compatibility with existing behavior.
  gfx::ColorSpace CompatRGBColorSpace() const;

  const std::optional<gfx::HDRMetadata>& hdr_metadata() const {
    return hdr_metadata_;
  }

  void set_hdr_metadata(const std::optional<gfx::HDRMetadata>& hdr_metadata) {
    hdr_metadata_ = hdr_metadata;
  }

  const VideoFrameLayout& layout() const { return layout_; }

  VideoPixelFormat format() const { return layout_.format(); }
  StorageType storage_type() const { return storage_type_; }

  // The full dimensions of the video frame data.
  const gfx::Size& coded_size() const { return layout_.coded_size(); }
  // A subsection of [0, 0, coded_size().width(), coded_size.height()]. This
  // can be set to "soft-apply" a cropping. It determines the pointers into
  // the data returned by visible_data().
  const gfx::Rect& visible_rect() const { return visible_rect_; }
  // Specifies that the |visible_rect| section of the frame is supposed to be
  // scaled to this size when being presented. This can be used to represent
  // anamorphic frames, or to "soft-apply" any custom scaling.
  const gfx::Size& natural_size() const { return natural_size_; }

  size_t stride(size_t plane) const {
    CHECK(IsValidPlane(format(), plane));
    CHECK_LT(plane, layout_.num_planes());
    return layout_.planes()[plane].stride;
  }

  // Returns the number of bytes per row and number of rows for a given plane.
  //
  // As opposed to stride(), row_bytes() refers to the bytes representing
  // frame data scanlines (coded_size.width() pixels, without stride padding).
  int row_bytes(size_t plane) const;
  int rows(size_t plane) const;

  // Similar to row_bytes() and rows(), but instead refers to the visible area.
  int GetVisibleRowBytes(size_t plane) const;
  int GetVisibleRows(size_t plane) const;

  // Returns the number of columns for a given plane.
  int columns(size_t plane) const;

  // Returns pointer to the buffer for a given plane, if this is an
  // IsMappable() frame type. The memory is owned by VideoFrame object and must
  // not be freed by the caller.
  const uint8_t* data(size_t plane) const {
    auto span = data_span(plane);
    if (span.empty()) [[unlikely]] {
      return nullptr;
    }
    return span.data();
  }

  base::span<const uint8_t> data_span(size_t plane) const {
    CHECK(IsValidPlane(format(), plane));
    CHECK(IsMappable());
    return data_[plane];
  }

  base::span<uint8_t> writable_span(size_t plane) {
    // TODO(crbug.com/40265179): Also CHECK that the storage type isn't
    // STORAGE_UNOWNED_MEMORY once non-compliant usages are fixed.
    CHECK_NE(storage_type_, STORAGE_SHMEM);
    auto const_span = data_span(plane);
    // SAFETY: We take data() and size() from another span, which supposedly
    // refers to a valid range in memory.
    return UNSAFE_BUFFERS(
        base::span(const_cast<uint8_t*>(const_span.data()), const_span.size()));
  }

  uint8_t* writable_data(size_t plane) {
    // TODO(crbug.com/40265179): Also CHECK that the storage type isn't
    // STORAGE_UNOWNED_MEMORY once non-compliant usages are fixed.
    CHECK_NE(storage_type_, STORAGE_SHMEM);
    return const_cast<uint8_t*>(data(plane));
  }

  bool is_mappable_si_enabled() const {
    return wrapped_frame_ ? wrapped_frame_->is_mappable_si_enabled()
                          : is_mappable_si_enabled_;
  }

#if BUILDFLAG(IS_ANDROID)
  const std::optional<gpu::VulkanYCbCrInfo>& ycbcr_info() const {
    return wrapped_frame_ ? wrapped_frame_->ycbcr_info() : ycbcr_info_;
  }

  // Provide the sampler conversion information for the frame.
  void set_ycbcr_info(const std::optional<gpu::VulkanYCbCrInfo>& ycbcr_info) {
    ycbcr_info_ = ycbcr_info;
  }
#endif

  // Returns pointer to the data in the visible region of the frame, for
  // IsMappable() storage types. The returned pointer is offset into the
  // plane buffer specified by visible_rect().origin(). Memory is owned by
  // VideoFrame object and must not be freed by the caller.
  const uint8_t* visible_data(size_t plane) const;
  uint8_t* GetWritableVisibleData(size_t plane);

  // Returns spans of data in the visible region of the frame, for
  // IsMappable() storage types. The returned span is offset into the
  // plane buffer specified by visible_rect().origin().
  base::span<const uint8_t> GetVisiblePlaneData(size_t plane) const;
  base::span<uint8_t> GetWritableVisiblePlaneData(size_t plane);

  // Returns the `acquire_sync_token_`
  gpu::SyncToken acquire_sync_token() const;

  // Returns the ClientSharedImage.
  // Only valid to call if this is a NATIVE_TEXTURE frame and contains valid
  // ClientSharedImage pointer. Before using the shared_image, the caller must
  // wait for the included sync point.
  scoped_refptr<gpu::ClientSharedImage> shared_image() const;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // The number of DmaBufs will be equal or less than the number of planes of
  // the frame. If there are less, this means that the last FD contains the
  // remaining planes. Should be > 0 for STORAGE_DMABUFS.
  size_t NumDmabufFds() const;

  // Returns true if |frame| has DmaBufs.
  bool HasDmaBufs() const;

  // The returned FDs are still owned by the VideoFrame. This means that the
  // caller shall not close them, or use them after the VideoFrame is destroyed.
  // For such use cases, use dup() to obtain your own copy of the FDs.
  int GetDmabufFd(size_t i) const;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  // Sets the mailbox release callback.
  //
  // The callback may be run from ANY THREAD, and so it is up to the client to
  // ensure thread safety.
  //
  // WARNING: This method is not thread safe; it should only be called if you
  // are still the only owner of this VideoFrame.
  void SetReleaseMailboxCB(ReleaseMailboxCB release_mailbox_cb);

  // Tests whether a mailbox release callback is configured.
  bool HasReleaseMailboxCB() const;

  // Adds a callback to be run when the VideoFrame is about to be destroyed.
  // The callback may be run from ANY THREAD, and so it is up to the client to
  // ensure thread safety.  Although read-only access to the members of this
  // VideoFrame is permitted while the callback executes (including
  // VideoFrameMetadata), clients should not assume the data pointers are
  // valid.
  void AddDestructionObserver(base::OnceClosure callback);

  // Returns a dictionary of optional metadata.  This contains information
  // associated with the frame that downstream clients might use for frame-level
  // logging, quality/performance optimizations, signaling, etc.
  const VideoFrameMetadata& metadata() const { return metadata_; }
  VideoFrameMetadata& metadata() { return metadata_; }
  void set_metadata(const VideoFrameMetadata& metadata) {
    metadata_ = metadata;
  }

  // Resets |metadata_|.
  void clear_metadata() { set_metadata(VideoFrameMetadata()); }

  // The time span between the current frame and the first frame of the stream.
  // This is the media timestamp, and not the reference time.
  // See VideoFrameMetadata::REFERENCE_TIME for details.
  base::TimeDelta timestamp() const { return timestamp_; }
  void set_timestamp(base::TimeDelta timestamp) { timestamp_ = timestamp; }

  // It uses |client| to insert a new sync token and potentially waits on an
  // older sync token. The final sync point will be used to release this
  // VideoFrame. Also returns the new sync token.
  // This method is thread safe. Both blink and compositor threads can call it.
  gpu::SyncToken UpdateReleaseSyncToken(SyncTokenClient* client);

  // Similar to UpdateReleaseSyncToken() but operates on the gpu::SyncToken
  // for mailbox.
  void UpdateAcquireSyncToken(gpu::SyncToken new_acquire_sync_token);

  // Returns a human-readable string describing |*this|.
  std::string AsHumanReadableString() const;

  // Unique identifier for this video frame generated at construction time. The
  // first ID is 1. The identifier is unique within a process % overflows (which
  // should be impossible in practice with a 64-bit unsigned integer).
  //
  // Note: callers may assume that ID will always correspond to a base::IdType
  // but should not blindly assume that the underlying type will always be
  // uint64_t (this is free to change in the future).
  using ID = ::base::IdTypeU64<class VideoFrameIdTag>;
  ID unique_id() const { return unique_id_; }

  // Returns the number of bits per channel.
  size_t BitDepth() const;

 protected:
  friend class base::RefCountedThreadSafe<VideoFrame>;
  virtual ~VideoFrame();

  // Creates a summary of the configuration settings provided as parameters.
  static std::string ConfigToString(const VideoPixelFormat format,
                                    const VideoFrame::StorageType storage_type,
                                    const gfx::Size& coded_size,
                                    const gfx::Rect& visible_rect,
                                    const gfx::Size& natural_size);

 private:
  // The constructor of VideoFrame should use IsValidConfigInternal()
  // instead of the public IsValidConfig() to check the config, because we can
  // create special video frames that won't pass the check by IsValidConfig().
  static bool IsValidConfigInternal(VideoPixelFormat format,
                                    FrameControlType frame_control_type,
                                    const gfx::Size& coded_size,
                                    const gfx::Rect& visible_rect,
                                    const gfx::Size& natural_size);

  static scoped_refptr<VideoFrame> CreateFrameInternal(
      VideoPixelFormat format,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      base::TimeDelta timestamp,
      bool zero_initialize_memory);

  static scoped_refptr<VideoFrame> CreateFrameForNativeTexturesInternal(
      VideoPixelFormat format,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      base::TimeDelta timestamp);

#if BUILDFLAG(IS_CHROMEOS)
  static scoped_refptr<VideoFrame> CreateFrameForGpuMemoryBufferInternal(
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      std::unique_ptr<gpu::LegacyGpuMemoryBufferForVideo> gpu_memory_buffer,
      base::TimeDelta timestamp);

  void MakeScopedMappingForGpuMemoryBuffer(
      base::OnceCallback<void(std::unique_ptr<VideoFrame::ScopedMapping>)>
          result_cb,
      bool success) const;
#endif
  void WrapScopedSharedImageMapping(
      base::OnceCallback<void(std::unique_ptr<VideoFrame::ScopedMapping>)>
          result_cb,
      std::unique_ptr<gpu::ClientSharedImage::ScopedMapping> mapping) const;

  // Return the alignment for the whole frame, calculated as the max of the
  // alignment for each individual plane.
  static gfx::Size CommonAlignment(VideoPixelFormat format);

  // Tries to allocate the requisite amount of memory for this frame. Returns
  // false if this would cause an out of memory error.
  [[nodiscard]] bool AllocateMemory(bool zero_initialize_memory);

  // Return plane sizes for the given layout.
  //
  // It first considers buffer size layout object provides. If layout's
  // number of buffers equals to number of planes, and buffer size is assigned
  // (non-zero), it returns buffers' size.
  //
  // Otherwise, it uses the first (num_buffers - 1) assigned buffers' size as
  // plane size. Then for the rest unassigned planes, calculates their size
  // based on format, coded size and stride for the plane.
  static std::vector<size_t> CalculatePlaneSize(const VideoFrameLayout& layout);

  // Calculates plane size for `layout_`.
  std::vector<size_t> CalculatePlaneSize() const;

  // Returns true iff the frame has a shared memory storage type, and the
  // associated regions are valid.
  bool IsValidSharedMemoryFrame() const;

  template <typename T>
  base::span<T> GetVisibleDataInternal(base::span<T> data, size_t plane) const;

  // VideFrameLayout (includes format, coded_size, and strides).
  const VideoFrameLayout layout_;

  // Set by WrapVideoFrame to soft-apply a new set of format, visible rectangle,
  // and natural size on |wrapped_frame_|
  scoped_refptr<VideoFrame> wrapped_frame_;
  // This is set when WrapVideoFrame() was given an already wrapped frame,
  // and it needs to be preserved for proper destruction later
  // (e.g. calling |done_callbacks_|).
  scoped_refptr<VideoFrame> intermediate_wrapped_frame_;

  // Storage type for the different planes.
  StorageType storage_type_;  // TODO(mcasas): make const

  // Width, height, and offsets of the visible portion of the video frame. Must
  // be a subrect of |coded_size_|. Can be odd with respect to the sample
  // boundaries, e.g. for formats with subsampled chroma.
  const gfx::Rect visible_rect_;

  // Width and height of the visible portion of the video frame
  // (|visible_rect_.size()|) with aspect ratio taken into account.
  const gfx::Size natural_size_;

  // Array of data pointers to each plane.
  // TODO(mcasas): we don't know on ctor if we own |data_| or not. Change
  // to std::unique_ptr<uint8_t, AlignedFreeDeleter> after refactoring
  // VideoFrame.
  std::array<base::span<const uint8_t>, kMaxPlanes> data_;

  // Sync token associated with the `shared_image_`.
  gpu::SyncToken acquire_sync_token_;
  ReleaseMailboxCB mailbox_holder_release_cb_;

  // Native texture shared image that is only set when the VideoFrame is
  // created via VideoFrame::WrapSharedImage().
  scoped_refptr<gpu::ClientSharedImage> shared_image_;

  // Shared memory handle, if this frame is STORAGE_SHMEM.  The region pointed
  // to is unowned.
  raw_ptr<const base::ReadOnlySharedMemoryRegion> shm_region_ = nullptr;

  // Used if this is a STORAGE_SHMEM frame with owned shared memory.
  // In that case, shm_region_ will refer to this region.
  base::ReadOnlySharedMemoryRegion owned_shm_region_;
  base::ReadOnlySharedMemoryMapping owned_shm_mapping_;

#if BUILDFLAG(IS_CHROMEOS)
  // GPU memory buffer, if this frame is STORAGE_GPU_MEMORY_BUFFER.
  std::unique_ptr<gpu::LegacyGpuMemoryBufferForVideo> gpu_memory_buffer_;
#endif

  // This field will be set by clients when using MappableSI instead of
  // GpuMemoryBuffers. Clients will set this flag while creating a VideoFrame.
  bool is_mappable_si_enabled_ = false;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Dmabufs for the frame, used when storage is STORAGE_DMABUFS. Size is either
  // equal or less than the number of planes of the frame. If it is less, then
  // the memory area represented by the last FD contains the remaining planes.
  std::vector<base::ScopedFD> dmabuf_fds_;
#endif

  base::Lock done_callbacks_lock_;
  std::vector<base::OnceClosure> done_callbacks_
      GUARDED_BY(done_callbacks_lock_);

  base::TimeDelta timestamp_;

  base::Lock release_sync_token_lock_;
  gpu::SyncToken release_sync_token_ GUARDED_BY(release_sync_token_lock_);

  VideoFrameMetadata metadata_;

  // Generated at construction time.
  const ID unique_id_;

  gfx::ColorSpace color_space_;
  std::optional<gfx::HDRMetadata> hdr_metadata_;

#if BUILDFLAG(IS_ANDROID)
  // Sampler conversion information which is used in vulkan context for android.
  std::optional<gpu::VulkanYCbCrInfo> ycbcr_info_;
#endif

  // Allocation which makes up |data_| planes for self-allocated frames.
  std::unique_ptr<uint8_t, base::UncheckedFreeDeleter> private_data_;
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_FRAME_H_
