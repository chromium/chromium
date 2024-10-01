// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/platform_video_frame_utils.h"

#include <drm_fourcc.h>
#include <xf86drm.h>

#include <limits>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/dcheck_is_on.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/base/color_plane_layout.h"
#include "media/base/format_utils.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_util.h"
#include "media/gpu/buffer_validation.h"
#include "media/gpu/macros.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/gfx/linux/gbm_buffer.h"
#include "ui/gfx/linux/gbm_defines.h"
#include "ui/gfx/linux/gbm_device.h"
#include "ui/gfx/linux/gbm_util.h"
#include "ui/gfx/linux/gbm_wrapper.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/switches.h"

namespace media {

namespace {
struct DrmVersionDeleter {
  void operator()(drmVersion* version) const { drmFreeVersion(version); }
};

using ScopedDrmVersionPtr = std::unique_ptr<drmVersion, DrmVersionDeleter>;

// Returns the gbm device using the |drm_node_file_prefix|.
static std::unique_ptr<ui::GbmDevice> CreateGbmDevice(
    std::string_view drm_node_file_prefix,
    int first_drm_file_index,
    const char* only_supported_driver_name_for_testing = nullptr) {
  constexpr int kMaximumNumberOfDrmNodes = 100;

  for (int i = first_drm_file_index;
       i < first_drm_file_index + kMaximumNumberOfDrmNodes; ++i) {
    const base::FilePath dev_path(FILE_PATH_LITERAL(
        base::StrCat({drm_node_file_prefix, base::NumberToString(i)})));

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_V4L2_CODEC)
    const bool is_render_node = base::Contains(drm_node_file_prefix, "render");

    // TODO(b/313513760): don't guard base::File::FLAG_WRITE behind
    // BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_V4L2_CODEC) once the hardware video
    // decoding sandbox allows R+W access to the render nodes.
    // base::File::FLAG_WRITE is needed on Linux for gbm_create_device().
    const uint32_t kDrmNodeFileFlags =
        base::File::FLAG_OPEN | base::File::FLAG_READ |
        (is_render_node ? base::File::FLAG_WRITE : 0);
#else
    const uint32_t kDrmNodeFileFlags =
        base::File::FLAG_OPEN | base::File::FLAG_READ;
#endif

    base::File drm_node_file(dev_path, kDrmNodeFileFlags);
    if (!drm_node_file.IsValid()) {
      return nullptr;
    }

    ScopedDrmVersionPtr version(drmGetVersion(drm_node_file.GetPlatformFile()));
    if (!version) {
      continue;
    }
    const std::string driver_name(
        version->name,
        base::checked_cast<std::string::size_type>(version->name_len));

    // Skips the virtual graphics memory manager device.
    if (base::EqualsCaseInsensitiveASCII(driver_name, "vgem")) {
      continue;
    }

    if (only_supported_driver_name_for_testing == nullptr ||
        (driver_name == only_supported_driver_name_for_testing)) {
      // |gbm_device| expects its owner to keep |drm_node_file| open during the
      // former's lifetime. We give it away here since GbmDeviceWrapper is a
      // singleton that fully owns |gbm_device|.
      auto gbm_device = ui::CreateGbmDevice(drm_node_file.GetPlatformFile());
      if (gbm_device) {
        drm_node_file.TakePlatformFile();
        return gbm_device;
      }
    }
  }

  return nullptr;
}

// GbmDeviceWrapper is a singleton that provides thread-safe access to a
// ui::GbmDevice for the purposes of creating native BOs. The ui::GbmDevice is
// initialized with the first non-vgem render node found that works starting at
// /dev/dri/renderD128. Render node doesn't exist when minigbm buffer allocation
// is done using dumb driver with vkms. If this happens, the ui::GbmDevice is
// initialized with the first primary node found that works starting at
// /dev/dri/card0. Note that we have our own FD to the render node or the
// primary node (i.e., it's not shared with other components). Therefore, there
// should not be any concurrency issues if other components in the GPU process
// (like the VA-API driver) access the render node or the primary node using
// their own FD.
class GbmDeviceWrapper {
 public:
  GbmDeviceWrapper(const GbmDeviceWrapper&) = delete;
  GbmDeviceWrapper& operator=(const GbmDeviceWrapper&) = delete;

  static GbmDeviceWrapper* Get() {
    static base::NoDestructor<GbmDeviceWrapper> gbm_device_wrapper;
    return gbm_device_wrapper.get();
  }

  // Creates a native BO and returns it as a GpuMemoryBufferHandle. Returns
  // gfx::GpuMemoryBufferHandle() on failure.
  gfx::GpuMemoryBufferHandle CreateGpuMemoryBuffer(
      gfx::BufferFormat format,
      const gfx::Size& size,
      gfx::BufferUsage buffer_usage) {
    base::AutoLock lock(lock_);

    if (!IsInitialized()) {
      return gfx::GpuMemoryBufferHandle();
    }

    const int fourcc_format = ui::GetFourCCFormatFromBufferFormat(format);
    if (fourcc_format == DRM_FORMAT_INVALID)
      return gfx::GpuMemoryBufferHandle();

    std::unique_ptr<ui::GbmBuffer> buffer =
        CreateGbmBuffer(fourcc_format, size, buffer_usage);
    if (!buffer)
      return gfx::GpuMemoryBufferHandle();

    gfx::NativePixmapHandle native_pixmap_handle = buffer->ExportHandle();
    if (native_pixmap_handle.planes.empty())
      return gfx::GpuMemoryBufferHandle();

    gfx::GpuMemoryBufferHandle gmb_handle;
    gmb_handle.type = gfx::GpuMemoryBufferType::NATIVE_PIXMAP;
    gmb_handle.id = GetNextGpuMemoryBufferId();
    gmb_handle.native_pixmap_handle = std::move(native_pixmap_handle);
    return gmb_handle;
  }

  std::unique_ptr<ui::GbmBuffer> ImportGpuMemoryBuffer(
      gfx::BufferFormat format,
      const gfx::Size& size,
      gfx::NativePixmapHandle handle) {
    CHECK_LE(handle.planes.size(), base::checked_cast<size_t>(GBM_MAX_PLANES));
    base::AutoLock lock(lock_);
    if (!IsInitialized()) {
      return nullptr;
    }
    const int fourcc_format = ui::GetFourCCFormatFromBufferFormat(format);
    if (fourcc_format == DRM_FORMAT_INVALID)
      return nullptr;
    return gbm_device_->CreateBufferFromHandle(fourcc_format, size,
                                               std::move(handle));
  }

 private:
  GbmDeviceWrapper() {
    constexpr char kRenderNodeFilePrefix[] = "/dev/dri/renderD";
    constexpr int kMinRenderNodeNum = 128;

    auto gbm_device_from_render_node =
        CreateGbmDevice(kRenderNodeFilePrefix, kMinRenderNodeNum);

    if (gbm_device_from_render_node) {
      gbm_device_ = std::move(gbm_device_from_render_node);
      return;
    }

    // For V4L2 testing with VISL, dumb driver is used with vkms for minigbm
    // backend. In this case, the primary node needs to be used instead of the
    // render node.
    // TODO(b/316993034): Remove this when having a render node for vkms.
#if BUILDFLAG(USE_V4L2_CODEC)
    const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
    CHECK(cmd_line);

    if (cmd_line->HasSwitch(switches::kEnablePrimaryNodeAccessForVkmsTesting)) {
      constexpr char kPrimaryNodeFilePrefix[] = "/dev/dri/card";
      const int kMinPrimaryNodeNum = 0;

      auto gbm_device_from_primary_node =
          CreateGbmDevice(kPrimaryNodeFilePrefix, kMinPrimaryNodeNum, "vkms");

      if (gbm_device_from_primary_node) {
        gbm_device_ = std::move(gbm_device_from_primary_node);
      }
    }
#endif
  }
  ~GbmDeviceWrapper() = default;

  // Returns true iff this class has successfully Initialize()d.
  bool IsInitialized() const EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    return !!gbm_device_;
  }

  std::unique_ptr<ui::GbmBuffer> CreateGbmBuffer(int fourcc_format,
                                                 const gfx::Size& size,
                                                 gfx::BufferUsage buffer_usage)
      EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    uint32_t flags = ui::BufferUsageToGbmFlags(buffer_usage);
    std::unique_ptr<ui::GbmBuffer> buffer =
        gbm_device_->CreateBuffer(fourcc_format, size, flags);
    if (buffer)
      return buffer;

    // For certain use cases, allocated buffers must be able to be set via kms
    // on a CRTC. For those cases, the GBM_BO_USE_SCANOUT flag is required.
    // For other use cases, GBM_BO_USE_SCANOUT may be preferred but is
    // ultimately optional, so we can fall back to allocation without that
    // flag.
    constexpr auto kScanoutUsages = base::MakeFixedFlatSet<gfx::BufferUsage>(
        {gfx::BufferUsage::SCANOUT,
         gfx::BufferUsage::PROTECTED_SCANOUT,
#if !BUILDFLAG(USE_V4L2_CODEC)
         gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE,
#endif
         gfx::BufferUsage::SCANOUT_FRONT_RENDERING});
    if (!kScanoutUsages.contains(buffer_usage))
      flags &= ~GBM_BO_USE_SCANOUT;
    return gbm_device_->CreateBuffer(fourcc_format, size, flags);
  }

  friend class base::NoDestructor<GbmDeviceWrapper>;

  base::Lock lock_;
  std::unique_ptr<ui::GbmDevice> gbm_device_ GUARDED_BY(lock_);
};
}  // namespace

gfx::GpuMemoryBufferHandle AllocateGpuMemoryBufferHandle(
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    gfx::BufferUsage buffer_usage) {
  gfx::GpuMemoryBufferHandle gmb_handle;
  auto buffer_format = VideoPixelFormatToGfxBufferFormat(pixel_format);
  if (!buffer_format)
    return gmb_handle;
  return GbmDeviceWrapper::Get()->CreateGpuMemoryBuffer(
      *buffer_format, coded_size, buffer_usage);
}

gfx::GpuMemoryBufferId GetNextGpuMemoryBufferId() {
  static base::NoDestructor<base::Lock> id_lock;
  static int next_gpu_memory_buffer_id = 0;
  base::AutoLock lock(*id_lock);
  CHECK_LT(next_gpu_memory_buffer_id, std::numeric_limits<int>::max());
  return gfx::GpuMemoryBufferId(next_gpu_memory_buffer_id++);
}

scoped_refptr<VideoFrame> CreateGpuMemoryBufferVideoFrame(
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    gfx::BufferUsage buffer_usage) {
  auto gmb_handle =
      AllocateGpuMemoryBufferHandle(pixel_format, coded_size, buffer_usage);
  if (gmb_handle.is_null() || gmb_handle.type != gfx::NATIVE_PIXMAP)
    return nullptr;

  return CreateVideoFrameFromGpuMemoryBufferHandle(
      std::move(gmb_handle), pixel_format, coded_size, visible_rect,
      natural_size, timestamp, buffer_usage);
}

scoped_refptr<VideoFrame> CreateVideoFrameFromGpuMemoryBufferHandle(
    gfx::GpuMemoryBufferHandle gmb_handle,
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    gfx::BufferUsage buffer_usage) {
  const bool supports_zero_copy_webgpu_import =
      gmb_handle.native_pixmap_handle.supports_zero_copy_webgpu_import;

  auto buffer_format = VideoPixelFormatToGfxBufferFormat(pixel_format);
  DCHECK(buffer_format);
  gpu::GpuMemoryBufferSupport support;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer =
      support.CreateGpuMemoryBufferImplFromHandle(
          std::move(gmb_handle), coded_size, *buffer_format, buffer_usage,
          base::NullCallback());
  if (!gpu_memory_buffer)
    return nullptr;

  // It is not necessary to pass a SharedImage because this VideoFrame is not
  // rendered.
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapExternalGpuMemoryBuffer(
      visible_rect, natural_size, std::move(gpu_memory_buffer), timestamp);
  if (!frame)
    return nullptr;

  // We only support importing non-DISJOINT multi-planar GbmBuffer right now.
  // TODO(crbug.com/40201271): Add DISJOINT support.
  frame->metadata().is_webgpu_compatible = supports_zero_copy_webgpu_import;

  return frame;
}

scoped_refptr<VideoFrame> CreatePlatformVideoFrame(
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    gfx::BufferUsage buffer_usage) {
  auto gmb_handle =
      AllocateGpuMemoryBufferHandle(pixel_format, coded_size, buffer_usage);
  if (gmb_handle.is_null() || gmb_handle.type != gfx::NATIVE_PIXMAP)
    return nullptr;

  std::vector<ColorPlaneLayout> planes;
  for (const auto& plane : gmb_handle.native_pixmap_handle.planes)
    planes.emplace_back(plane.stride, plane.offset, plane.size);

  auto layout = VideoFrameLayout::CreateWithPlanes(
      pixel_format, coded_size, std::move(planes),
      VideoFrameLayout::kBufferAddressAlignment,
      gmb_handle.native_pixmap_handle.modifier);

  if (!layout)
    return nullptr;

  std::vector<base::ScopedFD> dmabuf_fds;
  for (auto& plane : gmb_handle.native_pixmap_handle.planes)
    dmabuf_fds.emplace_back(plane.fd.release());

  auto frame = VideoFrame::WrapExternalDmabufs(
      *layout, visible_rect, natural_size, std::move(dmabuf_fds), timestamp);
  if (!frame)
    return nullptr;

  return frame;
}

std::optional<VideoFrameLayout> GetPlatformVideoFrameLayout(
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    gfx::BufferUsage buffer_usage) {
  // |visible_rect| and |natural_size| do not matter here. |coded_size| is set
  // as a dummy variable.
  auto frame =
      CreatePlatformVideoFrame(pixel_format, coded_size, gfx::Rect(coded_size),
                               coded_size, base::TimeDelta(), buffer_usage);
  return frame ? std::make_optional<VideoFrameLayout>(frame->layout())
               : std::nullopt;
}

gfx::GpuMemoryBufferHandle CreateGpuMemoryBufferHandle(
    const VideoFrame* video_frame) {
  DCHECK(video_frame);

  gfx::GpuMemoryBufferHandle handle;
  switch (video_frame->storage_type()) {
    case VideoFrame::STORAGE_GPU_MEMORY_BUFFER:
      handle = video_frame->GetGpuMemoryBufferHandle();
      // TODO(crbug.com/1097956): handle a failure gracefully.
      CHECK_EQ(handle.type, gfx::NATIVE_PIXMAP)
          << "The cloned handle has an unexpected type: " << handle.type;
      CHECK(!handle.native_pixmap_handle.planes.empty())
          << "The cloned handle has no planes";
      break;
    case VideoFrame::STORAGE_DMABUFS: {
      const size_t num_planes = VideoFrame::NumPlanes(video_frame->format());
      std::vector<base::ScopedFD> duped_fds;
      for (size_t i = 0; i < num_planes; ++i) {
        // TODO(crbug.com/1036174): Replace this duplication with a check.
        // Duplicate the FD's that are present. If there are more planes than
        // FD's, then duplicate the fd of the last plane until the number of
        // fds are the same as the number of planes.
        int source_fd = (i < video_frame->NumDmabufFds())
                            ? video_frame->GetDmabufFd(i)
                            : duped_fds.back().get();

        base::ScopedFD dup_fd = base::ScopedFD(HANDLE_EINTR(dup(source_fd)));
        // TODO(crbug.com/1097956): handle a failure gracefully.
        PCHECK(dup_fd.is_valid());
        duped_fds.push_back(std::move(dup_fd));
      }

      handle.type = gfx::NATIVE_PIXMAP;
      handle.id = GetNextGpuMemoryBufferId();
      DCHECK_EQ(video_frame->layout().planes().size(), num_planes);
      handle.native_pixmap_handle.modifier = video_frame->layout().modifier();
      for (size_t i = 0; i < num_planes; ++i) {
        const auto& plane = video_frame->layout().planes()[i];
        handle.native_pixmap_handle.planes.emplace_back(
            plane.stride, plane.offset, plane.size, std::move(duped_fds[i]));
      }
    } break;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unsupported storage type: " << video_frame->storage_type();
  }
  CHECK_EQ(handle.type, gfx::NATIVE_PIXMAP);
  if (video_frame->format() == PIXEL_FORMAT_MJPEG)
    return handle;
#if DCHECK_IS_ON()
  const bool is_handle_valid =
      !handle.is_null() &&
      VerifyGpuMemoryBufferHandle(video_frame->format(),
                                  video_frame->coded_size(), handle);
  DLOG_IF(WARNING, !is_handle_valid)
      << __func__ << "(): Created GpuMemoryBufferHandle is invalid";
#endif  // DCHECK_IS_ON()
  return handle;
}

scoped_refptr<gfx::NativePixmapDmaBuf> CreateNativePixmapDmaBuf(
    const VideoFrame* video_frame) {
  DCHECK(video_frame);

  // Create a native pixmap from the frame's memory buffer handle.
  gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle =
      CreateGpuMemoryBufferHandle(video_frame);
  if (gpu_memory_buffer_handle.is_null() ||
      gpu_memory_buffer_handle.type != gfx::NATIVE_PIXMAP) {
    VLOGF(1) << "Failed to create native GpuMemoryBufferHandle";
    return nullptr;
  }

  auto buffer_format =
      VideoPixelFormatToGfxBufferFormat(video_frame->layout().format());
  if (!buffer_format) {
    VLOGF(1) << "Unexpected video frame format";
    return nullptr;
  }

  auto native_pixmap = base::MakeRefCounted<gfx::NativePixmapDmaBuf>(
      video_frame->coded_size(), *buffer_format,
      std::move(gpu_memory_buffer_handle.native_pixmap_handle));

  DCHECK(native_pixmap->AreDmaBufFdsValid());
  return native_pixmap;
}

gfx::GenericSharedMemoryId GetSharedMemoryId(const VideoFrame& frame) {
  if (auto* gmb = frame.GetGpuMemoryBuffer()) {
    return gmb->GetId();
  }
  if (frame.HasDmaBufs()) {
    return gfx::GenericSharedMemoryId(frame.GetDmabufFd(0));
  }
  NOTREACHED_IN_MIGRATION() << "The frame is not backed by shared memory";
  return gfx::GenericSharedMemoryId();  // Invalid
}

bool CanImportGpuMemoryBufferHandle(
    const gfx::Size& size,
    gfx::BufferFormat format,
    const gfx::GpuMemoryBufferHandle& gmb_handle) {
  if (gmb_handle.type != gfx::GpuMemoryBufferType::NATIVE_PIXMAP) {
    VLOGF(1) << "The handle type (" << gmb_handle.type << ") is unsupported";
    return false;
  }
  const auto pixel_format = GfxBufferFormatToVideoPixelFormat(format);
  if (!pixel_format) {
    VLOGF(1) << "Unsupported buffer format: "
             << gfx::BufferFormatToString(format);
    return false;
  }
  if (!VerifyGpuMemoryBufferHandle(*pixel_format, size, gmb_handle)) {
    VLOGF(1) << "Invalid GpuMemoryBufferHandle provided";
    return false;
  }
  gfx::NativePixmapHandle native_pixmap_handle =
      gfx::CloneHandleForIPC(gmb_handle.native_pixmap_handle);
  if (native_pixmap_handle.planes.empty()) {
    VLOGF(1) << "Could not duplicate the NativePixmapHandle";
    return false;
  }
  return !!GbmDeviceWrapper::Get()->ImportGpuMemoryBuffer(
      format, size, std::move(native_pixmap_handle));
}

}  // namespace media
