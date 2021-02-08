// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/platform_video_frame_utils.h"

#include <drm_fourcc.h>
#include <xf86drm.h>

#include <limits>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/no_destructor.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "media/base/color_plane_layout.h"
#include "media/base/format_utils.h"
#include "media/base/scopedfd_helper.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_util.h"
#include "media/gpu/buffer_validation.h"
#include "media/gpu/macros.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/gfx/linux/gbm_buffer.h"
#include "ui/gfx/linux/gbm_device.h"
#include "ui/gfx/linux/gbm_util.h"
#include "ui/gfx/linux/gbm_wrapper.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/native_pixmap.h"

namespace media {

namespace {

// GbmDeviceWrapper is a singleton that provides thread-safe access to a
// ui::GbmDevice for the purposes of creating native BOs. The ui::GbmDevice is
// initialized with the first non-vgem render node found that works starting at
// /dev/dri/renderD128. Note that we have our own FD to the render node (i.e.,
// it's not shared with other components). Therefore, there should not be any
// concurrency issues if other components in the GPU process (like the VA-API
// driver) access the render node using their own FD.
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
    if (!gbm_device_)
      return gfx::GpuMemoryBufferHandle();

    const int fourcc_format = ui::GetFourCCFormatFromBufferFormat(format);
    if (fourcc_format == DRM_FORMAT_INVALID)
      return gfx::GpuMemoryBufferHandle();
    const uint32_t flags = ui::BufferUsageToGbmFlags(buffer_usage);
    std::unique_ptr<ui::GbmBuffer> buffer =
        gbm_device_->CreateBuffer(fourcc_format, size, flags);
    if (!buffer)
      return gfx::GpuMemoryBufferHandle();

    gfx::NativePixmapHandle native_pixmap_handle = buffer->ExportHandle();
    if (native_pixmap_handle.planes.empty())
      return gfx::GpuMemoryBufferHandle();

    CHECK_LT(next_gpu_memory_buffer_id_, std::numeric_limits<int>::max());
    const gfx::GpuMemoryBufferId gpu_memory_buffer_id(
        next_gpu_memory_buffer_id_++);

    gfx::GpuMemoryBufferHandle gmb_handle;
    gmb_handle.type = gfx::GpuMemoryBufferType::NATIVE_PIXMAP;
    gmb_handle.id = gpu_memory_buffer_id;
    gmb_handle.native_pixmap_handle = std::move(native_pixmap_handle);
    return gmb_handle;
  }

 private:
  GbmDeviceWrapper() {
    constexpr char kRenderNodeFilePattern[] = "/dev/dri/renderD%d";
    // This loop ends on either the first card that does not exist or the first
    // one that results in the creation of a gbm device.
    for (int i = 128;; i++) {
      base::FilePath dev_path(FILE_PATH_LITERAL(
          base::StringPrintf(kRenderNodeFilePattern, i).c_str()));
      render_node_file_ =
          base::File(dev_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
      if (!render_node_file_.IsValid())
        return;
      // Skip the virtual graphics memory manager device.
      drmVersionPtr version =
          drmGetVersion(render_node_file_.GetPlatformFile());
      if (!version)
        continue;
      std::string version_name(
          version->name,
          base::checked_cast<std::string::size_type>(version->name_len));
      drmFreeVersion(version);
      if (base::LowerCaseEqualsASCII(version_name, "vgem"))
        continue;
      gbm_device_ = ui::CreateGbmDevice(render_node_file_.GetPlatformFile());
      if (gbm_device_)
        return;
    }
  }
  ~GbmDeviceWrapper() = default;

  friend class base::NoDestructor<GbmDeviceWrapper>;

  base::Lock lock_;
  base::File render_node_file_ GUARDED_BY(lock_);
  std::unique_ptr<ui::GbmDevice> gbm_device_ GUARDED_BY(lock_);
  int next_gpu_memory_buffer_id_ GUARDED_BY(lock_) = 0;
};

gfx::GpuMemoryBufferHandle AllocateGpuMemoryBufferHandle(
    gpu::GpuMemoryBufferFactory* factory,
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    gfx::BufferUsage buffer_usage,
    base::ScopedClosureRunner& destroy_cb) {
  DCHECK(factory ||
         buffer_usage == gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE);
  gfx::GpuMemoryBufferHandle gmb_handle;
  auto buffer_format = VideoPixelFormatToGfxBufferFormat(pixel_format);
  if (!buffer_format)
    return gmb_handle;

  if (!factory) {
    return GbmDeviceWrapper::Get()->CreateGpuMemoryBuffer(
        *buffer_format, coded_size, buffer_usage);
  }

  int gpu_memory_buffer_id;
  {
    static base::NoDestructor<base::Lock> id_lock;
    static int next_gpu_memory_buffer_id = 0;
    base::AutoLock lock(*id_lock);
    CHECK_LT(next_gpu_memory_buffer_id, std::numeric_limits<int>::max());
    gpu_memory_buffer_id = next_gpu_memory_buffer_id++;
  }

  gmb_handle = factory->CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId(gpu_memory_buffer_id), coded_size,
      /*framebuffer_size=*/GetRectSizeFromOrigin(visible_rect), *buffer_format,
      buffer_usage, gpu::kPlatformVideoFramePoolClientId,
      gfx::kNullAcceleratedWidget);
  DCHECK(gmb_handle.is_null() || gmb_handle.type != gfx::NATIVE_PIXMAP ||
         VideoFrame::NumPlanes(pixel_format) ==
             gmb_handle.native_pixmap_handle.planes.size());
  if (gmb_handle.is_null())
    return gmb_handle;
  destroy_cb.ReplaceClosure(
      base::BindOnce(&gpu::GpuMemoryBufferFactory::DestroyGpuMemoryBuffer,
                     base::Unretained(factory), gmb_handle.id,
                     gpu::kPlatformVideoFramePoolClientId));

  return gmb_handle;
}
}  // namespace

scoped_refptr<VideoFrame> CreateGpuMemoryBufferVideoFrame(
    gpu::GpuMemoryBufferFactory* factory,
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    gfx::BufferUsage buffer_usage) {
  base::ScopedClosureRunner destroy_cb((base::DoNothing()));
  auto gmb_handle =
      AllocateGpuMemoryBufferHandle(factory, pixel_format, coded_size,
                                    visible_rect, buffer_usage, destroy_cb);
  if (gmb_handle.is_null() || gmb_handle.type != gfx::NATIVE_PIXMAP)
    return nullptr;

  auto buffer_format = VideoPixelFormatToGfxBufferFormat(pixel_format);
  DCHECK(buffer_format);
  gpu::GpuMemoryBufferSupport support;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer =
      support.CreateGpuMemoryBufferImplFromHandle(
          std::move(gmb_handle), coded_size, *buffer_format, buffer_usage,
          base::NullCallback());
  if (!gpu_memory_buffer)
    return nullptr;

  // The empty mailbox is ok because this VideoFrame is not rendered.
  const gpu::MailboxHolder mailbox_holders[VideoFrame::kMaxPlanes] = {};
  auto frame = VideoFrame::WrapExternalGpuMemoryBuffer(
      visible_rect, natural_size, std::move(gpu_memory_buffer), mailbox_holders,
      base::NullCallback(), timestamp);

  if (frame)
    frame->AddDestructionObserver(destroy_cb.Release());
  return frame;
}

scoped_refptr<VideoFrame> CreatePlatformVideoFrame(
    gpu::GpuMemoryBufferFactory* factory,
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    gfx::BufferUsage buffer_usage) {
  base::ScopedClosureRunner destroy_cb((base::DoNothing()));
  auto gmb_handle =
      AllocateGpuMemoryBufferHandle(factory, pixel_format, coded_size,
                                    visible_rect, buffer_usage, destroy_cb);
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

  frame->AddDestructionObserver(destroy_cb.Release());
  return frame;
}

base::Optional<VideoFrameLayout> GetPlatformVideoFrameLayout(
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    gfx::BufferUsage buffer_usage) {
  // |visible_rect| and |natural_size| do not matter here. |coded_size| is set
  // as a dummy variable.
  auto frame = CreatePlatformVideoFrame(
      gpu_memory_buffer_factory, pixel_format, coded_size,
      gfx::Rect(coded_size), coded_size, base::TimeDelta(), buffer_usage);
  return frame ? base::make_optional<VideoFrameLayout>(frame->layout())
               : base::nullopt;
}

gfx::GpuMemoryBufferHandle CreateGpuMemoryBufferHandle(
    const VideoFrame* video_frame) {
  DCHECK(video_frame);

  gfx::GpuMemoryBufferHandle handle;
  switch (video_frame->storage_type()) {
    case VideoFrame::STORAGE_GPU_MEMORY_BUFFER:
      handle = video_frame->GetGpuMemoryBuffer()->CloneHandle();
      // TODO(crbug.com/1097956): handle a failure gracefully.
      CHECK_EQ(handle.type, gfx::NATIVE_PIXMAP)
          << "The cloned handle has an unexpected type: " << handle.type;
      CHECK(!handle.native_pixmap_handle.planes.empty())
          << "The cloned handle has no planes";
      break;
    case VideoFrame::STORAGE_DMABUFS: {
      const size_t num_planes = VideoFrame::NumPlanes(video_frame->format());
      std::vector<base::ScopedFD> duped_fds =
          DuplicateFDs(video_frame->DmabufFds());
      // TODO(crbug.com/1036174): Replace this duplication with a check.
      // Duplicate the fd of the last plane until the number of fds are the same
      // as the number of planes.
      while (num_planes != duped_fds.size()) {
        int duped_fd = -1;
        duped_fd = HANDLE_EINTR(dup(duped_fds.back().get()));
        // TODO(crbug.com/1097956): handle a failure gracefully.
        PCHECK(duped_fd >= 0) << "Failed duplicating a dma-buf fd";
        duped_fds.emplace_back(duped_fd);
      }

      handle.type = gfx::NATIVE_PIXMAP;
      DCHECK_EQ(video_frame->layout().planes().size(), num_planes);
      handle.native_pixmap_handle.modifier = video_frame->layout().modifier();
      for (size_t i = 0; i < num_planes; ++i) {
        const auto& plane = video_frame->layout().planes()[i];
        handle.native_pixmap_handle.planes.emplace_back(
            plane.stride, plane.offset, plane.size, std::move(duped_fds[i]));
      }
    } break;
    default:
      NOTREACHED() << "Unsupported storage type: "
                   << video_frame->storage_type();
  }
  if (!handle.is_null() && handle.type == gfx::NATIVE_PIXMAP &&
      !VerifyGpuMemoryBufferHandle(video_frame->format(),
                                   video_frame->coded_size(), handle)) {
    VLOGF(1) << "Created GpuMemoryBufferHandle is invalid";
  }
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

}  // namespace media
