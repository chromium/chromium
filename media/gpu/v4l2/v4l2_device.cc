// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_device.h"

#include <fcntl.h>
#include <poll.h>
#include <linux/media.h>
#include <sys/ioctl.h>

#include <algorithm>
#include <set>

#include <libdrm/drm_fourcc.h>
#include <linux/videodev2.h>
#include <string.h>
#include <sys/mman.h>
#include <sstream>

#include "base/bind.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/color_plane_layout.h"
#include "media/base/video_types.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/buffer_affinity_tracker.h"
#include "media/gpu/v4l2/generic_v4l2_device.h"
#include "ui/gfx/generic_shared_memory_id.h"
#include "ui/gfx/native_pixmap_handle.h"

#if defined(ARCH_CPU_ARMEL)
#include "media/gpu/v4l2/tegra_v4l2_device.h"
#endif
#if defined(AML_V4L2)
#include "media/gpu/v4l2/aml_v4l2_device.h"
#endif

#define REQUEST_DEVICE "/dev/media-dec0"

namespace media {

namespace {

// Maximum number of requests that can be created.
constexpr size_t kMaxNumRequests = 32;

gfx::Rect V4L2RectToGfxRect(const v4l2_rect& rect) {
  return gfx::Rect(rect.left, rect.top, rect.width, rect.height);
}

struct v4l2_format BuildV4L2Format(const enum v4l2_buf_type type,
                                   uint32_t fourcc,
                                   const gfx::Size& size,
                                   size_t buffer_size) {
  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = type;
  format.fmt.pix_mp.pixelformat = fourcc;
  format.fmt.pix_mp.width = size.width();
  format.fmt.pix_mp.height = size.height();
  format.fmt.pix_mp.num_planes = V4L2Device::GetNumPlanesOfV4L2PixFmt(fourcc);
  format.fmt.pix_mp.plane_fmt[0].sizeimage = buffer_size;

  return format;
}

}  // namespace

V4L2ExtCtrl::V4L2ExtCtrl(uint32_t id) {
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = id;
}

V4L2ExtCtrl::V4L2ExtCtrl(uint32_t id, int32_t val) : V4L2ExtCtrl(id) {
  ctrl.value = val;
}

// Class used to store the state of a buffer that should persist between
// reference creations. This includes:
// * Result of initial VIDIOC_QUERYBUF ioctl,
// * Plane mappings.
//
// Also provides helper functions.
class V4L2Buffer {
 public:
  static std::unique_ptr<V4L2Buffer> Create(scoped_refptr<V4L2Device> device,
                                            enum v4l2_buf_type type,
                                            enum v4l2_memory memory,
                                            const struct v4l2_format& format,
                                            size_t buffer_id);
  ~V4L2Buffer();

  void* GetPlaneMapping(const size_t plane);
  size_t GetMemoryUsage() const;
  const struct v4l2_buffer& v4l2_buffer() const { return v4l2_buffer_; }
  scoped_refptr<VideoFrame> GetVideoFrame();

 private:
  V4L2Buffer(scoped_refptr<V4L2Device> device,
             enum v4l2_buf_type type,
             enum v4l2_memory memory,
             const struct v4l2_format& format,
             size_t buffer_id);
  bool Query();
  scoped_refptr<VideoFrame> CreateVideoFrame();

  scoped_refptr<V4L2Device> device_;
  std::vector<void*> plane_mappings_;

  // V4L2 data as queried by QUERYBUF.
  struct v4l2_buffer v4l2_buffer_;
  // WARNING: do not change this to a vector or something smaller than
  // VIDEO_MAX_PLANES, otherwise the Tegra libv4l2 will write data beyond
  // the number of allocated planes, resulting in memory corruption.
  struct v4l2_plane v4l2_planes_[VIDEO_MAX_PLANES];

  struct v4l2_format format_;
  scoped_refptr<VideoFrame> video_frame_;

  DISALLOW_COPY_AND_ASSIGN(V4L2Buffer);
};

std::unique_ptr<V4L2Buffer> V4L2Buffer::Create(scoped_refptr<V4L2Device> device,
                                               enum v4l2_buf_type type,
                                               enum v4l2_memory memory,
                                               const struct v4l2_format& format,
                                               size_t buffer_id) {
  // Not using std::make_unique because constructor is private.
  std::unique_ptr<V4L2Buffer> buffer(
      new V4L2Buffer(device, type, memory, format, buffer_id));

  if (!buffer->Query())
    return nullptr;

  return buffer;
}

V4L2Buffer::V4L2Buffer(scoped_refptr<V4L2Device> device,
                       enum v4l2_buf_type type,
                       enum v4l2_memory memory,
                       const struct v4l2_format& format,
                       size_t buffer_id)
    : device_(device), format_(format) {
  DCHECK(V4L2_TYPE_IS_MULTIPLANAR(type));
  DCHECK_LE(format.fmt.pix_mp.num_planes, base::size(v4l2_planes_));

  memset(&v4l2_buffer_, 0, sizeof(v4l2_buffer_));
  memset(v4l2_planes_, 0, sizeof(v4l2_planes_));
  v4l2_buffer_.m.planes = v4l2_planes_;
  // Just in case we got more planes than we want.
  v4l2_buffer_.length =
      std::min(static_cast<size_t>(format.fmt.pix_mp.num_planes),
               base::size(v4l2_planes_));
  v4l2_buffer_.index = buffer_id;
  v4l2_buffer_.type = type;
  v4l2_buffer_.memory = memory;
  plane_mappings_.resize(v4l2_buffer_.length);
}

V4L2Buffer::~V4L2Buffer() {
  if (v4l2_buffer_.memory == V4L2_MEMORY_MMAP) {
    for (size_t i = 0; i < plane_mappings_.size(); i++)
      if (plane_mappings_[i] != nullptr)
        device_->Munmap(plane_mappings_[i], v4l2_buffer_.m.planes[i].length);
  }
}

bool V4L2Buffer::Query() {
  int ret = device_->Ioctl(VIDIOC_QUERYBUF, &v4l2_buffer_);
  if (ret) {
    VPLOGF(1) << "VIDIOC_QUERYBUF failed: ";
    return false;
  }

  DCHECK(plane_mappings_.size() == v4l2_buffer_.length);

  return true;
}

void* V4L2Buffer::GetPlaneMapping(const size_t plane) {
  if (plane >= plane_mappings_.size()) {
    VLOGF(1) << "Invalid plane " << plane << " requested.";
    return nullptr;
  }

  void* p = plane_mappings_[plane];
  if (p)
    return p;

  // Do this check here to avoid repeating it after a buffer has been
  // successfully mapped (we know we are of MMAP type by then).
  if (v4l2_buffer_.memory != V4L2_MEMORY_MMAP) {
    VLOGF(1) << "Cannot create mapping on non-MMAP buffer";
    return nullptr;
  }

  p = device_->Mmap(nullptr, v4l2_buffer_.m.planes[plane].length,
                    PROT_READ | PROT_WRITE, MAP_SHARED,
                    v4l2_buffer_.m.planes[plane].m.mem_offset);
  if (p == MAP_FAILED) {
    VPLOGF(1) << "mmap() failed: ";
    return nullptr;
  }

  plane_mappings_[plane] = p;
  return p;
}

size_t V4L2Buffer::GetMemoryUsage() const {
  size_t usage = 0;
  for (size_t i = 0; i < v4l2_buffer_.length; i++) {
    usage += v4l2_buffer_.m.planes[i].length;
  }
  return usage;
}

scoped_refptr<VideoFrame> V4L2Buffer::CreateVideoFrame() {
  auto layout = V4L2Device::V4L2FormatToVideoFrameLayout(format_);
  if (!layout) {
    VLOGF(1) << "Cannot create frame layout for V4L2 buffers";
    return nullptr;
  }

  std::vector<base::ScopedFD> dmabuf_fds = device_->GetDmabufsForV4L2Buffer(
      v4l2_buffer_.index, v4l2_buffer_.length,
      static_cast<enum v4l2_buf_type>(v4l2_buffer_.type));
  if (dmabuf_fds.empty()) {
    VLOGF(1) << "Failed to get DMABUFs of V4L2 buffer";
    return nullptr;
  }

  // Duplicate the fd of the last v4l2 plane until the number of fds are the
  // same as the number of color planes.
  while (dmabuf_fds.size() != layout->planes().size()) {
    int duped_fd = -1;
    // Fd in dmabuf_fds is invalid with TegraV4L2Device. An invalid fd is added
    // in the case.
    if (dmabuf_fds.back().is_valid()) {
      duped_fd = HANDLE_EINTR(dup(dmabuf_fds.back().get()));
      if (duped_fd == -1) {
        DLOG(ERROR) << "Failed duplicating dmabuf fd";
        return nullptr;
      }
    }

    dmabuf_fds.emplace_back(duped_fd);
  }

  gfx::Size size(format_.fmt.pix_mp.width, format_.fmt.pix_mp.height);

  return VideoFrame::WrapExternalDmabufs(
      *layout, gfx::Rect(size), size, std::move(dmabuf_fds), base::TimeDelta());
}

scoped_refptr<VideoFrame> V4L2Buffer::GetVideoFrame() {
  // We can create the VideoFrame only when using MMAP buffers.
  if (v4l2_buffer_.memory != V4L2_MEMORY_MMAP) {
    VLOGF(1) << "Cannot create video frame from non-MMAP buffer";
    // Allow NOTREACHED() on invalid argument because this is an internal
    // method.
    NOTREACHED();
  }

  // Create the video frame instance if requiring it for the first time.
  if (!video_frame_)
    video_frame_ = CreateVideoFrame();

  return video_frame_;
}

// A thread-safe pool of buffer indexes, allowing buffers to be obtained and
// returned from different threads. All the methods of this class are
// thread-safe. Users should keep a scoped_refptr to instances of this class
// in order to ensure the list remains alive as long as they need it.
class V4L2BuffersList : public base::RefCountedThreadSafe<V4L2BuffersList> {
 public:
  V4L2BuffersList() = default;
  // Return a buffer to this list. Also can be called to set the initial pool
  // of buffers.
  // Note that it is illegal to return the same buffer twice.
  void ReturnBuffer(size_t buffer_id);
  // Get any of the buffers in the list. There is no order guarantee whatsoever.
  base::Optional<size_t> GetFreeBuffer();
  // Get the buffer with specified index.
  base::Optional<size_t> GetFreeBuffer(size_t requested_buffer_id);
  // Number of buffers currently in this list.
  size_t size() const;

 private:
  friend class base::RefCountedThreadSafe<V4L2BuffersList>;
  ~V4L2BuffersList() = default;

  mutable base::Lock lock_;
  std::set<size_t> free_buffers_ GUARDED_BY(lock_);
  DISALLOW_COPY_AND_ASSIGN(V4L2BuffersList);
};

void V4L2BuffersList::ReturnBuffer(size_t buffer_id) {
  base::AutoLock auto_lock(lock_);

  auto inserted = free_buffers_.emplace(buffer_id);
  DCHECK(inserted.second);
}

base::Optional<size_t> V4L2BuffersList::GetFreeBuffer() {
  base::AutoLock auto_lock(lock_);

  auto iter = free_buffers_.begin();
  if (iter == free_buffers_.end()) {
    DVLOGF(4) << "No free buffer available!";
    return base::nullopt;
  }

  size_t buffer_id = *iter;
  free_buffers_.erase(iter);

  return buffer_id;
}

base::Optional<size_t> V4L2BuffersList::GetFreeBuffer(
    size_t requested_buffer_id) {
  base::AutoLock auto_lock(lock_);

  return (free_buffers_.erase(requested_buffer_id) > 0)
             ? base::make_optional(requested_buffer_id)
             : base::nullopt;
}

size_t V4L2BuffersList::size() const {
  base::AutoLock auto_lock(lock_);

  return free_buffers_.size();
}

// Module-private class that let users query/write V4L2 buffer information.
// It also makes some private V4L2Queue methods available to this module only.
class V4L2BufferRefBase {
 public:
  V4L2BufferRefBase(const struct v4l2_buffer& v4l2_buffer,
                    base::WeakPtr<V4L2Queue> queue);
  ~V4L2BufferRefBase();

  bool QueueBuffer(scoped_refptr<VideoFrame> video_frame);
  void* GetPlaneMapping(const size_t plane);

  scoped_refptr<VideoFrame> GetVideoFrame();
  // Checks that the number of passed FDs is adequate for the current format
  // and buffer configuration. Only useful for DMABUF buffers.
  bool CheckNumFDsForFormat(const size_t num_fds) const;

  // Data from the buffer, that users can query and/or write.
  struct v4l2_buffer v4l2_buffer_;
  // WARNING: do not change this to a vector or something smaller than
  // VIDEO_MAX_PLANES, otherwise the Tegra libv4l2 will write data beyond
  // the number of allocated planes, resulting in memory corruption.
  struct v4l2_plane v4l2_planes_[VIDEO_MAX_PLANES];

 private:
  size_t BufferId() const { return v4l2_buffer_.index; }

  friend class V4L2WritableBufferRef;
  // A weak pointer to the queue this buffer belongs to. Will remain valid as
  // long as the underlying V4L2 buffer is valid too.
  // This can only be accessed from the sequence protected by sequence_checker_.
  // Thread-safe methods (like ~V4L2BufferRefBase) must *never* access this.
  base::WeakPtr<V4L2Queue> queue_;
  // Where to return this buffer if it goes out of scope without being queued.
  scoped_refptr<V4L2BuffersList> return_to_;
  bool queued = false;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(V4L2BufferRefBase);
};

V4L2BufferRefBase::V4L2BufferRefBase(const struct v4l2_buffer& v4l2_buffer,
                                     base::WeakPtr<V4L2Queue> queue)
    : queue_(std::move(queue)), return_to_(queue_->free_buffers_) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(V4L2_TYPE_IS_MULTIPLANAR(v4l2_buffer.type));
  DCHECK_LE(v4l2_buffer.length, base::size(v4l2_planes_));
  DCHECK(return_to_);

  memcpy(&v4l2_buffer_, &v4l2_buffer, sizeof(v4l2_buffer_));
  memcpy(v4l2_planes_, v4l2_buffer.m.planes,
         sizeof(struct v4l2_plane) * v4l2_buffer.length);
  v4l2_buffer_.m.planes = v4l2_planes_;
}

V4L2BufferRefBase::~V4L2BufferRefBase() {
  // We are the last reference and are only accessing the thread-safe
  // return_to_, so we are safe to call from any sequence.
  // If we have been queued, then the queue is our owner so we don't need to
  // return to the free buffers list.
  if (!queued)
    return_to_->ReturnBuffer(BufferId());
}

bool V4L2BufferRefBase::QueueBuffer(scoped_refptr<VideoFrame> video_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!queue_)
    return false;

  queued = queue_->QueueBuffer(&v4l2_buffer_, std::move(video_frame));

  return queued;
}

void* V4L2BufferRefBase::GetPlaneMapping(const size_t plane) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!queue_)
    return nullptr;

  return queue_->buffers_[BufferId()]->GetPlaneMapping(plane);
}

scoped_refptr<VideoFrame> V4L2BufferRefBase::GetVideoFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Used so we can return a const scoped_refptr& in all cases.
  static const scoped_refptr<VideoFrame> null_videoframe;

  if (!queue_)
    return null_videoframe;

  DCHECK_LE(BufferId(), queue_->buffers_.size());

  return queue_->buffers_[BufferId()]->GetVideoFrame();
}

bool V4L2BufferRefBase::CheckNumFDsForFormat(const size_t num_fds) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!queue_)
    return false;

  // We have not used SetFormat(), assume this is ok.
  // Hopefully we standardize SetFormat() in the future.
  if (!queue_->current_format_)
    return true;

  const size_t required_fds = queue_->current_format_->fmt.pix_mp.num_planes;
  // Sanity check.
  DCHECK_EQ(v4l2_buffer_.length, required_fds);
  if (num_fds < required_fds) {
    VLOGF(1) << "Insufficient number of FDs given for the current format. "
             << num_fds << " provided, " << required_fds << " required.";
    return false;
  }

  const auto* planes = v4l2_buffer_.m.planes;
  for (size_t i = v4l2_buffer_.length - 1; i >= num_fds; --i) {
    // Assume that an fd is a duplicate of a previous plane's fd if offset != 0.
    // Otherwise, if offset == 0, return error as it is likely pointing to
    // a new plane.
    if (planes[i].data_offset == 0) {
      VLOGF(1) << "Additional dmabuf fds point to a new buffer.";
      return false;
    }
  }

  return true;
}

V4L2WritableBufferRef::V4L2WritableBufferRef(
    const struct v4l2_buffer& v4l2_buffer,
    base::WeakPtr<V4L2Queue> queue)
    : buffer_data_(
          std::make_unique<V4L2BufferRefBase>(v4l2_buffer, std::move(queue))) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

V4L2WritableBufferRef::V4L2WritableBufferRef(V4L2WritableBufferRef&& other)
    : buffer_data_(std::move(other.buffer_data_)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(other.sequence_checker_);
}

V4L2WritableBufferRef::~V4L2WritableBufferRef() {
  // Only valid references should be sequence-checked
  if (buffer_data_) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }
}

V4L2WritableBufferRef& V4L2WritableBufferRef::operator=(
    V4L2WritableBufferRef&& other) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(other.sequence_checker_);

  if (this == &other)
    return *this;

  buffer_data_ = std::move(other.buffer_data_);

  return *this;
}

scoped_refptr<VideoFrame> V4L2WritableBufferRef::GetVideoFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  return buffer_data_->GetVideoFrame();
}

enum v4l2_memory V4L2WritableBufferRef::Memory() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  return static_cast<enum v4l2_memory>(buffer_data_->v4l2_buffer_.memory);
}

bool V4L2WritableBufferRef::DoQueue(V4L2RequestRef* request_ref,
                                    scoped_refptr<VideoFrame> video_frame) && {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  if (request_ref && buffer_data_->queue_->SupportsRequests())
    request_ref->ApplyQueueBuffer(&(buffer_data_->v4l2_buffer_));

  bool queued = buffer_data_->QueueBuffer(std::move(video_frame));

  // Clear our own reference.
  buffer_data_.reset();

  return queued;
}

bool V4L2WritableBufferRef::QueueMMap(
    V4L2RequestRef* request_ref) && {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  // Move ourselves so our data gets freed no matter when we return
  V4L2WritableBufferRef self(std::move(*this));

  if (self.Memory() != V4L2_MEMORY_MMAP) {
    VLOGF(1) << "Called on invalid buffer type!";
    return false;
  }

  return std::move(self).DoQueue(request_ref, nullptr);
}

bool V4L2WritableBufferRef::QueueUserPtr(
    const std::vector<void*>& ptrs,
    V4L2RequestRef* request_ref) && {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  // Move ourselves so our data gets freed no matter when we return
  V4L2WritableBufferRef self(std::move(*this));

  if (self.Memory() != V4L2_MEMORY_USERPTR) {
    VLOGF(1) << "Called on invalid buffer type!";
    return false;
  }

  if (ptrs.size() != self.PlanesCount()) {
    VLOGF(1) << "Provided " << ptrs.size() << " pointers while we require "
             << self.buffer_data_->v4l2_buffer_.length << ".";
    return false;
  }

  for (size_t i = 0; i < ptrs.size(); i++)
    self.buffer_data_->v4l2_buffer_.m.planes[i].m.userptr =
        reinterpret_cast<unsigned long>(ptrs[i]);

  return std::move(self).DoQueue(request_ref, nullptr);
}

bool V4L2WritableBufferRef::QueueDMABuf(
    const std::vector<base::ScopedFD>& fds,
    V4L2RequestRef* request_ref) && {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  // Move ourselves so our data gets freed no matter when we return
  V4L2WritableBufferRef self(std::move(*this));

  if (self.Memory() != V4L2_MEMORY_DMABUF) {
    VLOGF(1) << "Called on invalid buffer type!";
    return false;
  }

  if (!self.buffer_data_->CheckNumFDsForFormat(fds.size()))
    return false;

  size_t num_planes = self.PlanesCount();
  for (size_t i = 0; i < num_planes; i++)
    self.buffer_data_->v4l2_buffer_.m.planes[i].m.fd = fds[i].get();

  return std::move(self).DoQueue(request_ref, nullptr);
}

bool V4L2WritableBufferRef::QueueDMABuf(scoped_refptr<VideoFrame> video_frame,
                                        V4L2RequestRef* request_ref) && {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  // Move ourselves so our data gets freed no matter when we return
  V4L2WritableBufferRef self(std::move(*this));

  if (self.Memory() != V4L2_MEMORY_DMABUF) {
    VLOGF(1) << "Called on invalid buffer type!";
    return false;
  }

  // TODO(andrescj): consider replacing this by a DCHECK.
  if (video_frame->storage_type() != VideoFrame::STORAGE_GPU_MEMORY_BUFFER &&
      video_frame->storage_type() != VideoFrame::STORAGE_DMABUFS) {
    VLOGF(1) << "Only GpuMemoryBuffer and dma-buf VideoFrames are supported";
    return false;
  }

  // The FDs duped by CreateGpuMemoryBufferHandle() will be closed after the
  // call to DoQueue() which uses the VIDIOC_QBUF ioctl and so ends up
  // increasing the reference count of the dma-buf. Thus, closing the FDs is
  // safe.
  // TODO(andrescj): for dma-buf VideoFrames, duping the FDs is unnecessary.
  // Consider handling that path separately.
  gfx::GpuMemoryBufferHandle gmb_handle =
      CreateGpuMemoryBufferHandle(video_frame.get());
  if (gmb_handle.type != gfx::GpuMemoryBufferType::NATIVE_PIXMAP) {
    VLOGF(1) << "Failed to create GpuMemoryBufferHandle for frame!";
    return false;
  }
  const std::vector<gfx::NativePixmapPlane>& planes =
      gmb_handle.native_pixmap_handle.planes;

  if (!self.buffer_data_->CheckNumFDsForFormat(planes.size()))
    return false;

  size_t num_planes = self.PlanesCount();
  for (size_t i = 0; i < num_planes; i++)
    self.buffer_data_->v4l2_buffer_.m.planes[i].m.fd = planes[i].fd.get();

  return std::move(self).DoQueue(request_ref, std::move(video_frame));
}

bool V4L2WritableBufferRef::QueueDMABuf(
    const std::vector<gfx::NativePixmapPlane>& planes,
    V4L2RequestRef* request_ref) && {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  // Move ourselves so our data gets freed no matter when we return
  V4L2WritableBufferRef self(std::move(*this));

  if (self.Memory() != V4L2_MEMORY_DMABUF) {
    VLOGF(1) << "Called on invalid buffer type!";
    return false;
  }

  if (!self.buffer_data_->CheckNumFDsForFormat(planes.size()))
    return false;

  size_t num_planes = self.PlanesCount();
  for (size_t i = 0; i < num_planes; i++)
    self.buffer_data_->v4l2_buffer_.m.planes[i].m.fd = planes[i].fd.get();

  return std::move(self).DoQueue(request_ref, nullptr);
}

size_t V4L2WritableBufferRef::PlanesCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  return buffer_data_->v4l2_buffer_.length;
}

size_t V4L2WritableBufferRef::GetPlaneSize(const size_t plane) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  if (plane >= PlanesCount()) {
    VLOGF(1) << "Invalid plane " << plane << " requested.";
    return 0;
  }

  return buffer_data_->v4l2_buffer_.m.planes[plane].length;
}

void V4L2WritableBufferRef::SetPlaneSize(const size_t plane,
                                         const size_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  enum v4l2_memory memory = Memory();
  if (memory == V4L2_MEMORY_MMAP) {
    DCHECK_EQ(buffer_data_->v4l2_buffer_.m.planes[plane].length, size);
    return;
  }
  DCHECK(memory == V4L2_MEMORY_USERPTR || memory == V4L2_MEMORY_DMABUF);

  if (plane >= PlanesCount()) {
    VLOGF(1) << "Invalid plane " << plane << " requested.";
    return;
  }

  buffer_data_->v4l2_buffer_.m.planes[plane].length = size;
}

void* V4L2WritableBufferRef::GetPlaneMapping(const size_t plane) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  return buffer_data_->GetPlaneMapping(plane);
}

void V4L2WritableBufferRef::SetTimeStamp(const struct timeval& timestamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  buffer_data_->v4l2_buffer_.timestamp = timestamp;
}

const struct timeval& V4L2WritableBufferRef::GetTimeStamp() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  return buffer_data_->v4l2_buffer_.timestamp;
}

void V4L2WritableBufferRef::SetPlaneBytesUsed(const size_t plane,
                                              const size_t bytes_used) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  if (plane >= PlanesCount()) {
    VLOGF(1) << "Invalid plane " << plane << " requested.";
    return;
  }

  if (bytes_used > GetPlaneSize(plane)) {
    VLOGF(1) << "Set bytes used " << bytes_used << " larger than plane size "
             << GetPlaneSize(plane) << ".";
    return;
  }

  buffer_data_->v4l2_buffer_.m.planes[plane].bytesused = bytes_used;
}

size_t V4L2WritableBufferRef::GetPlaneBytesUsed(const size_t plane) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  if (plane >= PlanesCount()) {
    VLOGF(1) << "Invalid plane " << plane << " requested.";
    return 0;
  }

  return buffer_data_->v4l2_buffer_.m.planes[plane].bytesused;
}

void V4L2WritableBufferRef::SetPlaneDataOffset(const size_t plane,
                                               const size_t data_offset) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  if (plane >= PlanesCount()) {
    VLOGF(1) << "Invalid plane " << plane << " requested.";
    return;
  }

  buffer_data_->v4l2_buffer_.m.planes[plane].data_offset = data_offset;
}

size_t V4L2WritableBufferRef::BufferId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  return buffer_data_->v4l2_buffer_.index;
}

void V4L2WritableBufferRef::SetConfigStore(uint32_t config_store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  buffer_data_->v4l2_buffer_.config_store = config_store;
}

V4L2ReadableBuffer::V4L2ReadableBuffer(const struct v4l2_buffer& v4l2_buffer,
                                       base::WeakPtr<V4L2Queue> queue,
                                       scoped_refptr<VideoFrame> video_frame)
    : buffer_data_(
          std::make_unique<V4L2BufferRefBase>(v4l2_buffer, std::move(queue))),
      video_frame_(std::move(video_frame)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

scoped_refptr<VideoFrame> V4L2ReadableBuffer::GetVideoFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  if (buffer_data_->v4l2_buffer_.memory == V4L2_MEMORY_DMABUF && video_frame_)
    return video_frame_;

  return buffer_data_->GetVideoFrame();
}

V4L2ReadableBuffer::~V4L2ReadableBuffer() {
  // This method is thread-safe. Since we are the destructor, we are guaranteed
  // to be called from the only remaining reference to us. Also, we are just
  // calling the destructor of buffer_data_, which is also thread-safe.
  DCHECK(buffer_data_);
}

bool V4L2ReadableBuffer::IsLast() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  return buffer_data_->v4l2_buffer_.flags & V4L2_BUF_FLAG_LAST;
}

bool V4L2ReadableBuffer::IsKeyframe() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  return buffer_data_->v4l2_buffer_.flags & V4L2_BUF_FLAG_KEYFRAME;
}

struct timeval V4L2ReadableBuffer::GetTimeStamp() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  return buffer_data_->v4l2_buffer_.timestamp;
}

size_t V4L2ReadableBuffer::PlanesCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  return buffer_data_->v4l2_buffer_.length;
}

const void* V4L2ReadableBuffer::GetPlaneMapping(const size_t plane) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  return buffer_data_->GetPlaneMapping(plane);
}

size_t V4L2ReadableBuffer::GetPlaneBytesUsed(const size_t plane) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  if (plane >= PlanesCount()) {
    VLOGF(1) << "Invalid plane " << plane << " requested.";
    return 0;
  }

  return buffer_data_->v4l2_planes_[plane].bytesused;
}

size_t V4L2ReadableBuffer::GetPlaneDataOffset(const size_t plane) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  if (plane >= PlanesCount()) {
    VLOGF(1) << "Invalid plane " << plane << " requested.";
    return 0;
  }

  return buffer_data_->v4l2_planes_[plane].data_offset;
}

size_t V4L2ReadableBuffer::BufferId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  return buffer_data_->v4l2_buffer_.index;
}

// This class is used to expose buffer reference classes constructors to
// this module. This is to ensure that nobody else can create buffer references.
class V4L2BufferRefFactory {
 public:
  static V4L2WritableBufferRef CreateWritableRef(
      const struct v4l2_buffer& v4l2_buffer,
      base::WeakPtr<V4L2Queue> queue) {
    return V4L2WritableBufferRef(v4l2_buffer, std::move(queue));
  }

  static V4L2ReadableBufferRef CreateReadableRef(
      const struct v4l2_buffer& v4l2_buffer,
      base::WeakPtr<V4L2Queue> queue,
      scoped_refptr<VideoFrame> video_frame) {
    return new V4L2ReadableBuffer(v4l2_buffer, std::move(queue),
                                  std::move(video_frame));
  }
};

// Helper macros that print the queue type with logs.
#define VPQLOGF(level) \
  VPLOGF(level) << "(" << V4L2Device::V4L2BufferTypeToString(type_) << ") "
#define VQLOGF(level) \
  VLOGF(level) << "(" << V4L2Device::V4L2BufferTypeToString(type_) << ") "
#define DVQLOGF(level) \
  DVLOGF(level) << "(" << V4L2Device::V4L2BufferTypeToString(type_) << ") "

V4L2Queue::V4L2Queue(scoped_refptr<V4L2Device> dev,
                     enum v4l2_buf_type type,
                     base::OnceClosure destroy_cb)
    : type_(type),
      affinity_tracker_(0),
      device_(dev),
      destroy_cb_(std::move(destroy_cb)),
      weak_this_factory_(this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check if this queue support requests.
  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = type;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  if (device_->Ioctl(VIDIOC_REQBUFS, &reqbufs) != 0) {
    VPLOGF(1) << "Request support checks's VIDIOC_REQBUFS ioctl failed.";
    return;
  }

  if (reqbufs.capabilities & V4L2_BUF_CAP_SUPPORTS_REQUESTS) {
    supports_requests_ = true;
    DVLOGF(4) << "Queue supports request API.";
  }
}

V4L2Queue::~V4L2Queue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_streaming_) {
    VQLOGF(1) << "Queue is still streaming, trying to stop it...";
    Streamoff();
  }

  DCHECK(queued_buffers_.empty());
  DCHECK(!free_buffers_);

  if (!buffers_.empty()) {
    VQLOGF(1) << "Buffers are still allocated, trying to deallocate them...";
    DeallocateBuffers();
  }

  std::move(destroy_cb_).Run();
}

base::Optional<struct v4l2_format> V4L2Queue::SetFormat(uint32_t fourcc,
                                                        const gfx::Size& size,
                                                        size_t buffer_size) {
  struct v4l2_format format = BuildV4L2Format(type_, fourcc, size, buffer_size);
  if (device_->Ioctl(VIDIOC_S_FMT, &format) != 0 ||
      format.fmt.pix_mp.pixelformat != fourcc) {
    VPQLOGF(2) << "Failed to set format (format_fourcc=0x" << std::hex << fourcc
               << ")";
    return base::nullopt;
  }

  current_format_ = format;
  return current_format_;
}

base::Optional<struct v4l2_format> V4L2Queue::TryFormat(uint32_t fourcc,
                                                        const gfx::Size& size,
                                                        size_t buffer_size) {
  struct v4l2_format format = BuildV4L2Format(type_, fourcc, size, buffer_size);
  if (device_->Ioctl(VIDIOC_TRY_FMT, &format) != 0 ||
      format.fmt.pix_mp.pixelformat != fourcc) {
    VPQLOGF(2) << "Tried format not supported (format_fourcc=0x" << std::hex
               << fourcc << ")";
    return base::nullopt;
  }

  return format;
}

std::pair<base::Optional<struct v4l2_format>, int> V4L2Queue::GetFormat() {
  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = type_;
  if (device_->Ioctl(VIDIOC_G_FMT, &format) != 0) {
    VPQLOGF(2) << "Failed to get format";
    return std::make_pair(base::nullopt, errno);
  }

  return std::make_pair(format, 0);
}

base::Optional<gfx::Rect> V4L2Queue::GetVisibleRect() {
  // Some drivers prior to 4.13 only accept the non-MPLANE variant when using
  // VIDIOC_G_SELECTION. This block can be removed once we stop supporting
  // kernels < 4.13.
  // For details, see the note at
  // https://www.kernel.org/doc/html/latest/media/uapi/v4l/vidioc-g-selection.html
  enum v4l2_buf_type compose_type;
  switch (type_) {
    case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
      compose_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      break;
    case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
      compose_type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
      break;
    default:
      compose_type = type_;
      break;
  }

  struct v4l2_selection selection;
  memset(&selection, 0, sizeof(selection));
  selection.type = compose_type;
  selection.target = V4L2_SEL_TGT_COMPOSE;
  if (device_->Ioctl(VIDIOC_G_SELECTION, &selection) == 0) {
    DVQLOGF(3) << "VIDIOC_G_SELECTION is supported";
    return V4L2RectToGfxRect(selection.r);
  }

  // TODO(acourbot) using VIDIOC_G_CROP is considered legacy and can be
  // removed once no active devices use it anymore.
  DVQLOGF(3) << "Fallback to VIDIOC_G_CROP";
  struct v4l2_crop crop;
  memset(&crop, 0, sizeof(crop));
  crop.type = type_;
  if (device_->Ioctl(VIDIOC_G_CROP, &crop) == 0) {
    return V4L2RectToGfxRect(crop.c);
  }

  VQLOGF(1) << "Failed to get visible rect";
  return base::nullopt;
}

size_t V4L2Queue::AllocateBuffers(size_t count, enum v4l2_memory memory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!free_buffers_);
  DCHECK_EQ(queued_buffers_.size(), 0u);

  if (IsStreaming()) {
    VQLOGF(1) << "Cannot allocate buffers while streaming.";
    return 0;
  }

  if (buffers_.size() != 0) {
    VQLOGF(1)
        << "Cannot allocate new buffers while others are still allocated.";
    return 0;
  }

  if (count == 0) {
    VQLOGF(1) << "Attempting to allocate 0 buffers.";
    return 0;
  }

  // First query the number of planes in the buffers we are about to request.
  // This should not be required, but Tegra's VIDIOC_QUERYBUF will fail on
  // output buffers if the number of specified planes does not exactly match the
  // format.
  base::Optional<v4l2_format> format = GetFormat().first;
  if (!format) {
    VQLOGF(1) << "Cannot get format.";
    return 0;
  }
  planes_count_ = format->fmt.pix_mp.num_planes;
  DCHECK_LE(planes_count_, static_cast<size_t>(VIDEO_MAX_PLANES));

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = count;
  reqbufs.type = type_;
  reqbufs.memory = memory;
  DVQLOGF(3) << "Requesting " << count << " buffers.";

  int ret = device_->Ioctl(VIDIOC_REQBUFS, &reqbufs);
  if (ret) {
    VPQLOGF(1) << "VIDIOC_REQBUFS failed";
    return 0;
  }
  DVQLOGF(3) << "queue " << type_ << ": got " << reqbufs.count << " buffers.";

  memory_ = memory;

  free_buffers_ = new V4L2BuffersList();

  // Now query all buffer information.
  for (size_t i = 0; i < reqbufs.count; i++) {
    auto buffer = V4L2Buffer::Create(device_, type_, memory_, *format, i);

    if (!buffer) {
      DeallocateBuffers();

      return 0;
    }

    buffers_.emplace_back(std::move(buffer));
    free_buffers_->ReturnBuffer(i);
  }

  affinity_tracker_.resize(buffers_.size());

  DCHECK(free_buffers_);
  DCHECK_EQ(free_buffers_->size(), buffers_.size());
  DCHECK_EQ(queued_buffers_.size(), 0u);

  return buffers_.size();
}

bool V4L2Queue::DeallocateBuffers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsStreaming()) {
    VQLOGF(1) << "Cannot deallocate buffers while streaming.";
    return false;
  }

  if (buffers_.size() == 0)
    return true;

  weak_this_factory_.InvalidateWeakPtrs();
  buffers_.clear();
  affinity_tracker_.resize(0);
  free_buffers_ = nullptr;

  // Free all buffers.
  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = type_;
  reqbufs.memory = memory_;

  int ret = device_->Ioctl(VIDIOC_REQBUFS, &reqbufs);
  if (ret) {
    VPQLOGF(1) << "VIDIOC_REQBUFS failed";
    return false;
  }

  DCHECK(!free_buffers_);
  DCHECK_EQ(queued_buffers_.size(), 0u);

  return true;
}

size_t V4L2Queue::GetMemoryUsage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  size_t usage = 0;
  for (const auto& buf : buffers_) {
    usage += buf->GetMemoryUsage();
  }
  return usage;
}

v4l2_memory V4L2Queue::GetMemoryType() const {
  return memory_;
}

base::Optional<V4L2WritableBufferRef> V4L2Queue::GetFreeBuffer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // No buffers allocated at the moment?
  if (!free_buffers_)
    return base::nullopt;

  auto buffer_id = free_buffers_->GetFreeBuffer();
  if (!buffer_id.has_value())
    return base::nullopt;

  return V4L2BufferRefFactory::CreateWritableRef(
      buffers_[buffer_id.value()]->v4l2_buffer(),
      weak_this_factory_.GetWeakPtr());
}

base::Optional<V4L2WritableBufferRef> V4L2Queue::GetFreeBuffer(
    size_t requested_buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // No buffers allocated at the moment?
  if (!free_buffers_)
    return base::nullopt;

  auto buffer_id = free_buffers_->GetFreeBuffer(requested_buffer_id);
  if (!buffer_id.has_value())
    return base::nullopt;

  return V4L2BufferRefFactory::CreateWritableRef(
      buffers_[buffer_id.value()]->v4l2_buffer(),
      weak_this_factory_.GetWeakPtr());
}

base::Optional<V4L2WritableBufferRef> V4L2Queue::GetFreeBufferForFrame(
    const VideoFrame& frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // No buffers allocated at the moment?
  if (!free_buffers_)
    return base::nullopt;

  if (memory_ != V4L2_MEMORY_DMABUF) {
    DVLOGF(1) << "Queue is not DMABUF";
    return base::nullopt;
  }

  gfx::GenericSharedMemoryId id;
  if (auto gmb = frame.GetGpuMemoryBuffer()) {
    id = gmb->GetId();
  } else if (frame.HasDmaBufs()) {
    id = gfx::GenericSharedMemoryId(frame.DmabufFds()[0].get());
  } else {
    DVLOGF(1) << "Unsupported frame provided";
    return base::nullopt;
  }

  const auto v4l2_id = affinity_tracker_.get_buffer_for_id(id);
  if (!v4l2_id) {
    return base::nullopt;
  }

  return GetFreeBuffer(*v4l2_id);
}

bool V4L2Queue::QueueBuffer(struct v4l2_buffer* v4l2_buffer,
                            scoped_refptr<VideoFrame> video_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int ret = device_->Ioctl(VIDIOC_QBUF, v4l2_buffer);
  if (ret) {
    VPQLOGF(1) << "VIDIOC_QBUF failed";
    return false;
  }

  auto inserted =
      queued_buffers_.emplace(v4l2_buffer->index, std::move(video_frame));
  DCHECK_EQ(inserted.second, true);

  device_->SchedulePoll();

  return true;
}

std::pair<bool, V4L2ReadableBufferRef> V4L2Queue::DequeueBuffer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // No need to dequeue if no buffers queued.
  if (QueuedBuffersCount() == 0)
    return std::make_pair(true, nullptr);

  if (!IsStreaming()) {
    VQLOGF(1) << "Attempting to dequeue a buffer while not streaming.";
    return std::make_pair(true, nullptr);
  }

  struct v4l2_buffer v4l2_buffer;
  memset(&v4l2_buffer, 0, sizeof(v4l2_buffer));
  // WARNING: do not change this to a vector or something smaller than
  // VIDEO_MAX_PLANES, otherwise the Tegra libv4l2 will write data beyond
  // the number of allocated planes, resulting in memory corruption.
  struct v4l2_plane planes[VIDEO_MAX_PLANES];
  memset(planes, 0, sizeof(planes));
  v4l2_buffer.type = type_;
  v4l2_buffer.memory = memory_;
  v4l2_buffer.m.planes = planes;
  v4l2_buffer.length = planes_count_;
  int ret = device_->Ioctl(VIDIOC_DQBUF, &v4l2_buffer);
  if (ret) {
    // TODO(acourbot): we should not have to check for EPIPE as codec clients
    // should not call this method after the last buffer is dequeued.
    switch (errno) {
      case EAGAIN:
      case EPIPE:
        // This is not an error so we'll need to continue polling but won't
        // provide a buffer.
        device_->SchedulePoll();
        return std::make_pair(true, nullptr);
      default:
        VPQLOGF(1) << "VIDIOC_DQBUF failed";
        return std::make_pair(false, nullptr);
    }
  }

  auto it = queued_buffers_.find(v4l2_buffer.index);
  DCHECK(it != queued_buffers_.end());
  scoped_refptr<VideoFrame> queued_frame = std::move(it->second);
  queued_buffers_.erase(it);

  if (QueuedBuffersCount() > 0)
    device_->SchedulePoll();

  DCHECK(free_buffers_);
  return std::make_pair(true, V4L2BufferRefFactory::CreateReadableRef(
                                  v4l2_buffer, weak_this_factory_.GetWeakPtr(),
                                  std::move(queued_frame)));
}

bool V4L2Queue::IsStreaming() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return is_streaming_;
}

bool V4L2Queue::Streamon() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_streaming_)
    return true;

  int arg = static_cast<int>(type_);
  int ret = device_->Ioctl(VIDIOC_STREAMON, &arg);
  if (ret) {
    VPQLOGF(1) << "VIDIOC_STREAMON failed";
    return false;
  }

  is_streaming_ = true;

  return true;
}

bool V4L2Queue::Streamoff() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // We do not check the value of IsStreaming(), because we may have queued
  // buffers to the queue and wish to get them back - in such as case, we may
  // need to do a VIDIOC_STREAMOFF on a stopped queue.

  int arg = static_cast<int>(type_);
  int ret = device_->Ioctl(VIDIOC_STREAMOFF, &arg);
  if (ret) {
    VPQLOGF(1) << "VIDIOC_STREAMOFF failed";
    return false;
  }

  for (const auto& it : queued_buffers_) {
    DCHECK(free_buffers_);
    free_buffers_->ReturnBuffer(it.first);
  }

  queued_buffers_.clear();

  is_streaming_ = false;

  return true;
}

size_t V4L2Queue::AllocatedBuffersCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return buffers_.size();
}

size_t V4L2Queue::FreeBuffersCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return free_buffers_ ? free_buffers_->size() : 0;
}

size_t V4L2Queue::QueuedBuffersCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return queued_buffers_.size();
}

#undef VDQLOGF
#undef VPQLOGF
#undef VQLOGF

bool V4L2Queue::SupportsRequests() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return supports_requests_;
}

base::Optional<struct v4l2_format> V4L2Queue::SetModifierFormat(
    uint64_t modifier,
    const gfx::Size& size) {
  if (DRM_FORMAT_MOD_QCOM_COMPRESSED == modifier) {
    const uint32_t v4l2_pix_fmt_nv12_ubwc = v4l2_fourcc('Q', '1', '2', '8');
    auto format = SetFormat(v4l2_pix_fmt_nv12_ubwc, size, 0);
    if (!format)
      VPLOGF(1) << "Failed to set magic modifier format.";
    return format;
  }
  return base::nullopt;
}

// This class is used to expose V4L2Queue's constructor to this module. This is
// to ensure that nobody else can create instances of it.
class V4L2QueueFactory {
 public:
  static scoped_refptr<V4L2Queue> CreateQueue(scoped_refptr<V4L2Device> dev,
                                              enum v4l2_buf_type type,
                                              base::OnceClosure destroy_cb) {
    return new V4L2Queue(std::move(dev), type, std::move(destroy_cb));
  }
};

V4L2Device::V4L2Device() {
  DETACH_FROM_SEQUENCE(client_sequence_checker_);
}

V4L2Device::~V4L2Device() = default;

scoped_refptr<V4L2Queue> V4L2Device::GetQueue(enum v4l2_buf_type type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  switch (type) {
    // Supported queue types.
    case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
    case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
      break;
    default:
      VLOGF(1) << "Unsupported V4L2 queue type: " << type;
      return nullptr;
  }

  // TODO(acourbot): we should instead query the device for available queues,
  // and allocate them accordingly. This will do for now though.
  auto it = queues_.find(type);
  if (it != queues_.end())
    return scoped_refptr<V4L2Queue>(it->second);

  scoped_refptr<V4L2Queue> queue = V4L2QueueFactory::CreateQueue(
      this, type, base::BindOnce(&V4L2Device::OnQueueDestroyed, this, type));

  queues_[type] = queue.get();
  return queue;
}

void V4L2Device::OnQueueDestroyed(v4l2_buf_type buf_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  auto it = queues_.find(buf_type);
  DCHECK(it != queues_.end());
  queues_.erase(it);
}

// static
scoped_refptr<V4L2Device> V4L2Device::Create() {
  DVLOGF(3);

  scoped_refptr<V4L2Device> device;

#if defined(ARCH_CPU_ARMEL)
  device = new TegraV4L2Device();
  if (device->Initialize())
    return device;
#endif

#if defined(AML_V4L2)
  device = new AmlV4L2Device();
  if (device->Initialize())
    return device;
#endif

  device = new GenericV4L2Device();
  if (device->Initialize())
    return device;

  VLOGF(1) << "Failed to create a V4L2Device";
  return nullptr;
}

// static
uint32_t V4L2Device::VideoCodecProfileToV4L2PixFmt(VideoCodecProfile profile,
                                                   bool slice_based) {
  if (profile >= H264PROFILE_MIN && profile <= H264PROFILE_MAX) {
    if (slice_based)
      return V4L2_PIX_FMT_H264_SLICE;
    else
      return V4L2_PIX_FMT_H264;
  } else if (profile >= VP8PROFILE_MIN && profile <= VP8PROFILE_MAX) {
    if (slice_based)
      return V4L2_PIX_FMT_VP8_FRAME;
    else
      return V4L2_PIX_FMT_VP8;
  } else if (profile >= VP9PROFILE_MIN && profile <= VP9PROFILE_MAX) {
    if (slice_based)
      return V4L2_PIX_FMT_VP9_FRAME;
    else
      return V4L2_PIX_FMT_VP9;
  } else {
    LOG(ERROR) << "Unknown profile: " << GetProfileName(profile);
    return 0;
  }
}

// static
VideoCodecProfile V4L2Device::V4L2ProfileToVideoCodecProfile(VideoCodec codec,
                                                             uint32_t profile) {
  switch (codec) {
    case kCodecH264:
      switch (profile) {
        case V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE:
        case V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_BASELINE:
          return H264PROFILE_BASELINE;
        case V4L2_MPEG_VIDEO_H264_PROFILE_MAIN:
          return H264PROFILE_MAIN;
        case V4L2_MPEG_VIDEO_H264_PROFILE_EXTENDED:
          return H264PROFILE_EXTENDED;
        case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH:
          return H264PROFILE_HIGH;
        case V4L2_MPEG_VIDEO_H264_PROFILE_STEREO_HIGH:
          return H264PROFILE_STEREOHIGH;
        case V4L2_MPEG_VIDEO_H264_PROFILE_MULTIVIEW_HIGH:
          return H264PROFILE_MULTIVIEWHIGH;
      }
      break;
    case kCodecVP8:
      switch (profile) {
        case V4L2_MPEG_VIDEO_VP8_PROFILE_0:
        case V4L2_MPEG_VIDEO_VP8_PROFILE_1:
        case V4L2_MPEG_VIDEO_VP8_PROFILE_2:
        case V4L2_MPEG_VIDEO_VP8_PROFILE_3:
          return VP8PROFILE_ANY;
      }
      break;
    case kCodecVP9:
      switch (profile) {
        case V4L2_MPEG_VIDEO_VP9_PROFILE_0:
          return VP9PROFILE_PROFILE0;
        case V4L2_MPEG_VIDEO_VP9_PROFILE_1:
          return VP9PROFILE_PROFILE1;
        case V4L2_MPEG_VIDEO_VP9_PROFILE_2:
          return VP9PROFILE_PROFILE2;
        case V4L2_MPEG_VIDEO_VP9_PROFILE_3:
          return VP9PROFILE_PROFILE3;
      }
      break;
    default:
      VLOGF(2) << "Unknown codec: " << codec;
  }
  VLOGF(2) << "Unknown profile: " << profile;
  return VIDEO_CODEC_PROFILE_UNKNOWN;
}

std::vector<VideoCodecProfile> V4L2Device::V4L2PixFmtToVideoCodecProfiles(
    uint32_t pix_fmt,
    bool is_encoder) {
  auto get_supported_profiles = [this](
                                    VideoCodec codec,
                                    std::vector<VideoCodecProfile>* profiles) {
    uint32_t query_id = 0;
    switch (codec) {
      case kCodecH264:
        query_id = V4L2_CID_MPEG_VIDEO_H264_PROFILE;
        break;
      case kCodecVP8:
        query_id = V4L2_CID_MPEG_VIDEO_VP8_PROFILE;
        break;
      case kCodecVP9:
        query_id = V4L2_CID_MPEG_VIDEO_VP9_PROFILE;
        break;
      default:
        return false;
    }

    v4l2_queryctrl query_ctrl;
    memset(&query_ctrl, 0, sizeof(query_ctrl));
    query_ctrl.id = query_id;
    if (Ioctl(VIDIOC_QUERYCTRL, &query_ctrl) != 0) {
      return false;
    }
    v4l2_querymenu query_menu;
    memset(&query_menu, 0, sizeof(query_menu));
    query_menu.id = query_ctrl.id;
    for (query_menu.index = query_ctrl.minimum;
         static_cast<int>(query_menu.index) <= query_ctrl.maximum;
         query_menu.index++) {
      if (Ioctl(VIDIOC_QUERYMENU, &query_menu) == 0) {
        const VideoCodecProfile profile =
            V4L2Device::V4L2ProfileToVideoCodecProfile(codec, query_menu.index);
        if (profile != VIDEO_CODEC_PROFILE_UNKNOWN)
          profiles->push_back(profile);
      }
    }
    return true;
  };

  std::vector<VideoCodecProfile> profiles;
  switch (pix_fmt) {
    case V4L2_PIX_FMT_H264:
    case V4L2_PIX_FMT_H264_SLICE:
      if (!get_supported_profiles(kCodecH264, &profiles)) {
        DLOG(WARNING) << "Driver doesn't support QUERY H264 profiles, "
                      << "use default values, Base, Main, High";
        profiles = {
            H264PROFILE_BASELINE,
            H264PROFILE_MAIN,
            H264PROFILE_HIGH,
        };
      }
      break;
    case V4L2_PIX_FMT_VP8:
    case V4L2_PIX_FMT_VP8_FRAME:
      profiles = {VP8PROFILE_ANY};
      break;
    case V4L2_PIX_FMT_VP9:
    case V4L2_PIX_FMT_VP9_FRAME:
      if (!get_supported_profiles(kCodecVP9, &profiles)) {
        DLOG(WARNING) << "Driver doesn't support QUERY VP9 profiles, "
                      << "use default values, Profile0";
        profiles = {VP9PROFILE_PROFILE0};
      }
      break;
    default:
      VLOGF(1) << "Unhandled pixelformat " << FourccToString(pix_fmt);
      return {};
  }

  // Erase duplicated profiles.
  std::sort(profiles.begin(), profiles.end());
  profiles.erase(std::unique(profiles.begin(), profiles.end()), profiles.end());
  return profiles;
}

// static
uint32_t V4L2Device::V4L2PixFmtToDrmFormat(uint32_t format) {
  switch (format) {
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV12M:
      return DRM_FORMAT_NV12;

    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YUV420M:
      return DRM_FORMAT_YUV420;

    case V4L2_PIX_FMT_YVU420:
      return DRM_FORMAT_YVU420;

    case V4L2_PIX_FMT_RGB32:
      return DRM_FORMAT_ARGB8888;

    default:
      DVLOGF(1) << "Unrecognized format " << FourccToString(format);
      return 0;
  }
}

// static
int32_t V4L2Device::VideoCodecProfileToV4L2H264Profile(
    VideoCodecProfile profile) {
  switch (profile) {
    case H264PROFILE_BASELINE:
      return V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE;
    case H264PROFILE_MAIN:
      return V4L2_MPEG_VIDEO_H264_PROFILE_MAIN;
    case H264PROFILE_EXTENDED:
      return V4L2_MPEG_VIDEO_H264_PROFILE_EXTENDED;
    case H264PROFILE_HIGH:
      return V4L2_MPEG_VIDEO_H264_PROFILE_HIGH;
    case H264PROFILE_HIGH10PROFILE:
      return V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_10;
    case H264PROFILE_HIGH422PROFILE:
      return V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_422;
    case H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_444_PREDICTIVE;
    case H264PROFILE_SCALABLEBASELINE:
      return V4L2_MPEG_VIDEO_H264_PROFILE_SCALABLE_BASELINE;
    case H264PROFILE_SCALABLEHIGH:
      return V4L2_MPEG_VIDEO_H264_PROFILE_SCALABLE_HIGH;
    case H264PROFILE_STEREOHIGH:
      return V4L2_MPEG_VIDEO_H264_PROFILE_STEREO_HIGH;
    case H264PROFILE_MULTIVIEWHIGH:
      return V4L2_MPEG_VIDEO_H264_PROFILE_MULTIVIEW_HIGH;
    default:
      DVLOGF(1) << "Add more cases as needed";
      return -1;
  }
}

// static
int32_t V4L2Device::H264LevelIdcToV4L2H264Level(uint8_t level_idc) {
  switch (level_idc) {
    case 10:
      return V4L2_MPEG_VIDEO_H264_LEVEL_1_0;
    case 9:
      return V4L2_MPEG_VIDEO_H264_LEVEL_1B;
    case 11:
      return V4L2_MPEG_VIDEO_H264_LEVEL_1_1;
    case 12:
      return V4L2_MPEG_VIDEO_H264_LEVEL_1_2;
    case 13:
      return V4L2_MPEG_VIDEO_H264_LEVEL_1_3;
    case 20:
      return V4L2_MPEG_VIDEO_H264_LEVEL_2_0;
    case 21:
      return V4L2_MPEG_VIDEO_H264_LEVEL_2_1;
    case 22:
      return V4L2_MPEG_VIDEO_H264_LEVEL_2_2;
    case 30:
      return V4L2_MPEG_VIDEO_H264_LEVEL_3_0;
    case 31:
      return V4L2_MPEG_VIDEO_H264_LEVEL_3_1;
    case 32:
      return V4L2_MPEG_VIDEO_H264_LEVEL_3_2;
    case 40:
      return V4L2_MPEG_VIDEO_H264_LEVEL_4_0;
    case 41:
      return V4L2_MPEG_VIDEO_H264_LEVEL_4_1;
    case 42:
      return V4L2_MPEG_VIDEO_H264_LEVEL_4_2;
    case 50:
      return V4L2_MPEG_VIDEO_H264_LEVEL_5_0;
    case 51:
      return V4L2_MPEG_VIDEO_H264_LEVEL_5_1;
    default:
      DVLOGF(1) << "Unrecognized level_idc: " << static_cast<int>(level_idc);
      return -1;
  }
}

// static
gfx::Size V4L2Device::AllocatedSizeFromV4L2Format(
    const struct v4l2_format& format) {
  gfx::Size coded_size;
  gfx::Size visible_size;
  VideoPixelFormat frame_format = PIXEL_FORMAT_UNKNOWN;
  size_t bytesperline = 0;
  // Total bytes in the frame.
  size_t sizeimage = 0;

  if (V4L2_TYPE_IS_MULTIPLANAR(format.type)) {
    DCHECK_GT(format.fmt.pix_mp.num_planes, 0);
    bytesperline =
        base::checked_cast<int>(format.fmt.pix_mp.plane_fmt[0].bytesperline);
    for (size_t i = 0; i < format.fmt.pix_mp.num_planes; ++i) {
      sizeimage +=
          base::checked_cast<int>(format.fmt.pix_mp.plane_fmt[i].sizeimage);
    }
    visible_size.SetSize(base::checked_cast<int>(format.fmt.pix_mp.width),
                         base::checked_cast<int>(format.fmt.pix_mp.height));
    const uint32_t pix_fmt = format.fmt.pix_mp.pixelformat;
    const auto frame_fourcc = Fourcc::FromV4L2PixFmt(pix_fmt);
    if (!frame_fourcc) {
      VLOGF(1) << "Unsupported format " << FourccToString(pix_fmt);
      return coded_size;
    }
    frame_format = frame_fourcc->ToVideoPixelFormat();
  } else {
    bytesperline = base::checked_cast<int>(format.fmt.pix.bytesperline);
    sizeimage = base::checked_cast<int>(format.fmt.pix.sizeimage);
    visible_size.SetSize(base::checked_cast<int>(format.fmt.pix.width),
                         base::checked_cast<int>(format.fmt.pix.height));
    const uint32_t fourcc = format.fmt.pix.pixelformat;
    const auto frame_fourcc = Fourcc::FromV4L2PixFmt(fourcc);
    if (!frame_fourcc) {
      VLOGF(1) << "Unsupported format " << FourccToString(fourcc);
      return coded_size;
    }
    frame_format = frame_fourcc ? frame_fourcc->ToVideoPixelFormat()
                                : PIXEL_FORMAT_UNKNOWN;
  }

  // V4L2 does not provide per-plane bytesperline (bpl) when different
  // components are sharing one physical plane buffer. In this case, it only
  // provides bpl for the first component in the plane. So we can't depend on it
  // for calculating height, because bpl may vary within one physical plane
  // buffer. For example, YUV420 contains 3 components in one physical plane,
  // with Y at 8 bits per pixel, and Cb/Cr at 4 bits per pixel per component,
  // but we only get 8 pits per pixel from bytesperline in physical plane 0.
  // So we need to get total frame bpp from elsewhere to calculate coded height.

  // We need bits per pixel for one component only to calculate
  // coded_width from bytesperline.
  int plane_horiz_bits_per_pixel =
      VideoFrame::PlaneHorizontalBitsPerPixel(frame_format, 0);

  // Adding up bpp for each component will give us total bpp for all components.
  int total_bpp = 0;
  for (size_t i = 0; i < VideoFrame::NumPlanes(frame_format); ++i)
    total_bpp += VideoFrame::PlaneBitsPerPixel(frame_format, i);

  if (sizeimage == 0 || bytesperline == 0 || plane_horiz_bits_per_pixel == 0 ||
      total_bpp == 0 || (bytesperline * 8) % plane_horiz_bits_per_pixel != 0) {
    VLOGF(1) << "Invalid format provided";
    return coded_size;
  }

  // Coded width can be calculated by taking the first component's bytesperline,
  // which in V4L2 always applies to the first component in physical plane
  // buffer.
  int coded_width = bytesperline * 8 / plane_horiz_bits_per_pixel;
  // Sizeimage is coded_width * coded_height * total_bpp. In the case that we
  // don't have exact alignment due to padding in the driver, round up so that
  // the buffer is large enough.
  std::div_t res = std::div(sizeimage * 8, coded_width * total_bpp);
  int coded_height = res.quot + std::min(res.rem, 1);

  coded_size.SetSize(coded_width, coded_height);
  DVLOGF(3) << "coded_size=" << coded_size.ToString();

  // Sanity checks. Calculated coded size has to contain given visible size
  // and fulfill buffer byte size requirements.
  DCHECK(gfx::Rect(coded_size).Contains(gfx::Rect(visible_size)));
  DCHECK_LE(sizeimage, VideoFrame::AllocationSize(frame_format, coded_size));

  return coded_size;
}

// static
const char* V4L2Device::V4L2MemoryToString(const v4l2_memory memory) {
  switch (memory) {
    case V4L2_MEMORY_MMAP:
      return "V4L2_MEMORY_MMAP";
    case V4L2_MEMORY_USERPTR:
      return "V4L2_MEMORY_USERPTR";
    case V4L2_MEMORY_DMABUF:
      return "V4L2_MEMORY_DMABUF";
    case V4L2_MEMORY_OVERLAY:
      return "V4L2_MEMORY_OVERLAY";
    default:
      return "UNKNOWN";
  }
}

// static
const char* V4L2Device::V4L2BufferTypeToString(
    const enum v4l2_buf_type buf_type) {
  switch (buf_type) {
    case V4L2_BUF_TYPE_VIDEO_OUTPUT:
      return "OUTPUT";
    case V4L2_BUF_TYPE_VIDEO_CAPTURE:
      return "CAPTURE";
    case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
      return "OUTPUT_MPLANE";
    case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
      return "CAPTURE_MPLANE";
    default:
      return "UNKNOWN";
  }
}

// static
std::string V4L2Device::V4L2FormatToString(const struct v4l2_format& format) {
  std::ostringstream s;
  s << "v4l2_format type: " << format.type;
  if (format.type == V4L2_BUF_TYPE_VIDEO_CAPTURE ||
      format.type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
    //  single-planar
    const struct v4l2_pix_format& pix = format.fmt.pix;
    s << ", width_height: " << gfx::Size(pix.width, pix.height).ToString()
      << ", pixelformat: " << FourccToString(pix.pixelformat)
      << ", field: " << pix.field << ", bytesperline: " << pix.bytesperline
      << ", sizeimage: " << pix.sizeimage;
  } else if (V4L2_TYPE_IS_MULTIPLANAR(format.type)) {
    const struct v4l2_pix_format_mplane& pix_mp = format.fmt.pix_mp;
    // As long as num_planes's type is uint8_t, ostringstream treats it as a
    // char instead of an integer, which is not what we want. Casting
    // pix_mp.num_planes unsigned int solves the issue.
    s << ", width_height: " << gfx::Size(pix_mp.width, pix_mp.height).ToString()
      << ", pixelformat: " << FourccToString(pix_mp.pixelformat)
      << ", field: " << pix_mp.field
      << ", num_planes: " << static_cast<unsigned int>(pix_mp.num_planes);
    for (size_t i = 0; i < pix_mp.num_planes; ++i) {
      const struct v4l2_plane_pix_format& plane_fmt = pix_mp.plane_fmt[i];
      s << ", plane_fmt[" << i << "].sizeimage: " << plane_fmt.sizeimage
        << ", plane_fmt[" << i << "].bytesperline: " << plane_fmt.bytesperline;
    }
  } else {
    s << " unsupported yet.";
  }
  return s.str();
}

// static
std::string V4L2Device::V4L2BufferToString(const struct v4l2_buffer& buffer) {
  std::ostringstream s;
  s << "v4l2_buffer type: " << buffer.type << ", memory: " << buffer.memory
    << ", index: " << buffer.index << " bytesused: " << buffer.bytesused
    << ", length: " << buffer.length;
  if (buffer.type == V4L2_BUF_TYPE_VIDEO_CAPTURE ||
      buffer.type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
    //  single-planar
    if (buffer.memory == V4L2_MEMORY_MMAP) {
      s << ", m.offset: " << buffer.m.offset;
    } else if (buffer.memory == V4L2_MEMORY_USERPTR) {
      s << ", m.userptr: " << buffer.m.userptr;
    } else if (buffer.memory == V4L2_MEMORY_DMABUF) {
      s << ", m.fd: " << buffer.m.fd;
    }
  } else if (V4L2_TYPE_IS_MULTIPLANAR(buffer.type)) {
    for (size_t i = 0; i < buffer.length; ++i) {
      const struct v4l2_plane& plane = buffer.m.planes[i];
      s << ", m.planes[" << i << "](bytesused: " << plane.bytesused
        << ", length: " << plane.length
        << ", data_offset: " << plane.data_offset;
      if (buffer.memory == V4L2_MEMORY_MMAP) {
        s << ", m.mem_offset: " << plane.m.mem_offset;
      } else if (buffer.memory == V4L2_MEMORY_USERPTR) {
        s << ", m.userptr: " << plane.m.userptr;
      } else if (buffer.memory == V4L2_MEMORY_DMABUF) {
        s << ", m.fd: " << plane.m.fd;
      }
      s << ")";
    }
  } else {
    s << " unsupported yet.";
  }
  return s.str();
}

// static
base::Optional<VideoFrameLayout> V4L2Device::V4L2FormatToVideoFrameLayout(
    const struct v4l2_format& format) {
  if (!V4L2_TYPE_IS_MULTIPLANAR(format.type)) {
    VLOGF(1) << "v4l2_buf_type is not multiplanar: " << std::hex << "0x"
             << format.type;
    return base::nullopt;
  }
  const v4l2_pix_format_mplane& pix_mp = format.fmt.pix_mp;
  const uint32_t& pix_fmt = pix_mp.pixelformat;
  const auto video_fourcc = Fourcc::FromV4L2PixFmt(pix_fmt);
  if (!video_fourcc) {
    VLOGF(1) << "Failed to convert pixel format to VideoPixelFormat: "
             << FourccToString(pix_fmt);
    return base::nullopt;
  }
  const VideoPixelFormat video_format = video_fourcc->ToVideoPixelFormat();
  const size_t num_buffers = pix_mp.num_planes;
  const size_t num_color_planes = VideoFrame::NumPlanes(video_format);
  if (num_color_planes == 0) {
    VLOGF(1) << "Unsupported video format for NumPlanes(): "
             << VideoPixelFormatToString(video_format);
    return base::nullopt;
  }
  if (num_buffers > num_color_planes) {
    VLOGF(1) << "pix_mp.num_planes: " << num_buffers
             << " should not be larger than NumPlanes("
             << VideoPixelFormatToString(video_format)
             << "): " << num_color_planes;
    return base::nullopt;
  }
  // Reserve capacity in advance to prevent unnecessary vector reallocation.
  std::vector<ColorPlaneLayout> planes;
  planes.reserve(num_color_planes);
  for (size_t i = 0; i < num_buffers; ++i) {
    const v4l2_plane_pix_format& plane_format = pix_mp.plane_fmt[i];
    planes.emplace_back(static_cast<int32_t>(plane_format.bytesperline), 0u,
                        plane_format.sizeimage);
  }
  // For the case that #color planes > #buffers, it fills stride of color
  // plane which does not map to buffer.
  // Right now only some pixel formats are supported: NV12, YUV420, YVU420.
  if (num_color_planes > num_buffers) {
    const int32_t y_stride = planes[0].stride;
    // Note that y_stride is from v4l2 bytesperline and its type is uint32_t.
    // It is safe to cast to size_t.
    const size_t y_stride_abs = static_cast<size_t>(y_stride);
    switch (pix_fmt) {
      case V4L2_PIX_FMT_NV12:
        // The stride of UV is the same as Y in NV12.
        // The height is half of Y plane.
        planes.emplace_back(y_stride, y_stride_abs * pix_mp.height,
                            y_stride_abs * pix_mp.height / 2);
        DCHECK_EQ(2u, planes.size());
        break;
      case V4L2_PIX_FMT_YUV420:
      case V4L2_PIX_FMT_YVU420: {
        // The spec claims that two Cx rows (including padding) is exactly as
        // long as one Y row (including padding). So stride of Y must be even
        // number.
        if (y_stride % 2 != 0 || pix_mp.height % 2 != 0) {
          VLOGF(1) << "Plane-Y stride and height should be even; stride: "
                   << y_stride << ", height: " << pix_mp.height;
          return base::nullopt;
        }
        const int32_t half_stride = y_stride / 2;
        const size_t plane_0_area = y_stride_abs * pix_mp.height;
        const size_t plane_1_area = plane_0_area / 4;
        planes.emplace_back(half_stride, plane_0_area, plane_1_area);
        planes.emplace_back(half_stride, plane_0_area + plane_1_area,
                            plane_1_area);
        DCHECK_EQ(3u, planes.size());
        break;
      }
      default:
        VLOGF(1) << "Cannot derive stride for each plane for pixel format "
                 << FourccToString(pix_fmt);
        return base::nullopt;
    }
  }

  // Some V4L2 devices expect buffers to be page-aligned. We cannot detect
  // such devices individually, so set this as a video frame layout property.
  constexpr size_t buffer_alignment = 0x1000;
  if (num_buffers == 1) {
    return VideoFrameLayout::CreateWithPlanes(
        video_format, gfx::Size(pix_mp.width, pix_mp.height), std::move(planes),
        buffer_alignment);
  } else {
    return VideoFrameLayout::CreateMultiPlanar(
        video_format, gfx::Size(pix_mp.width, pix_mp.height), std::move(planes),
        buffer_alignment);
  }
}

// static
size_t V4L2Device::GetNumPlanesOfV4L2PixFmt(uint32_t pix_fmt) {
  base::Optional<Fourcc> fourcc = Fourcc::FromV4L2PixFmt(pix_fmt);
  if (fourcc && fourcc->IsMultiPlanar()) {
    return VideoFrame::NumPlanes(fourcc->ToVideoPixelFormat());
  }
  return 1u;
}

void V4L2Device::GetSupportedResolution(uint32_t pixelformat,
                                        gfx::Size* min_resolution,
                                        gfx::Size* max_resolution) {
  max_resolution->SetSize(0, 0);
  min_resolution->SetSize(0, 0);
  v4l2_frmsizeenum frame_size;
  memset(&frame_size, 0, sizeof(frame_size));
  frame_size.pixel_format = pixelformat;
  for (; Ioctl(VIDIOC_ENUM_FRAMESIZES, &frame_size) == 0; ++frame_size.index) {
    if (frame_size.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
      if (frame_size.discrete.width >=
              base::checked_cast<uint32_t>(max_resolution->width()) &&
          frame_size.discrete.height >=
              base::checked_cast<uint32_t>(max_resolution->height())) {
        max_resolution->SetSize(frame_size.discrete.width,
                                frame_size.discrete.height);
      }
      if (min_resolution->IsEmpty() ||
          (frame_size.discrete.width <=
               base::checked_cast<uint32_t>(min_resolution->width()) &&
           frame_size.discrete.height <=
               base::checked_cast<uint32_t>(min_resolution->height()))) {
        min_resolution->SetSize(frame_size.discrete.width,
                                frame_size.discrete.height);
      }
    } else if (frame_size.type == V4L2_FRMSIZE_TYPE_STEPWISE ||
               frame_size.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
      max_resolution->SetSize(frame_size.stepwise.max_width,
                              frame_size.stepwise.max_height);
      min_resolution->SetSize(frame_size.stepwise.min_width,
                              frame_size.stepwise.min_height);
      break;
    }
  }
  if (max_resolution->IsEmpty()) {
    max_resolution->SetSize(1920, 1088);
    VLOGF(1) << "GetSupportedResolution failed to get maximum resolution for "
             << "fourcc " << FourccToString(pixelformat) << ", fall back to "
             << max_resolution->ToString();
  }
  if (min_resolution->IsEmpty()) {
    min_resolution->SetSize(16, 16);
    VLOGF(1) << "GetSupportedResolution failed to get minimum resolution for "
             << "fourcc " << FourccToString(pixelformat) << ", fall back to "
             << min_resolution->ToString();
  }
}

std::vector<uint32_t> V4L2Device::EnumerateSupportedPixelformats(
    v4l2_buf_type buf_type) {
  std::vector<uint32_t> pixelformats;

  v4l2_fmtdesc fmtdesc;
  memset(&fmtdesc, 0, sizeof(fmtdesc));
  fmtdesc.type = buf_type;

  for (; Ioctl(VIDIOC_ENUM_FMT, &fmtdesc) == 0; ++fmtdesc.index) {
    DVLOGF(3) << "Found " << fmtdesc.description << std::hex << " (0x"
              << fmtdesc.pixelformat << ")";
    pixelformats.push_back(fmtdesc.pixelformat);
  }

  return pixelformats;
}

VideoDecodeAccelerator::SupportedProfiles
V4L2Device::EnumerateSupportedDecodeProfiles(const size_t num_formats,
                                             const uint32_t pixelformats[]) {
  VideoDecodeAccelerator::SupportedProfiles profiles;

  const auto& supported_pixelformats =
      EnumerateSupportedPixelformats(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);

  for (uint32_t pixelformat : supported_pixelformats) {
    if (std::find(pixelformats, pixelformats + num_formats, pixelformat) ==
        pixelformats + num_formats)
      continue;

    VideoDecodeAccelerator::SupportedProfile profile;
    GetSupportedResolution(pixelformat, &profile.min_resolution,
                           &profile.max_resolution);

    const auto video_codec_profiles =
        V4L2PixFmtToVideoCodecProfiles(pixelformat, false);

    for (const auto& video_codec_profile : video_codec_profiles) {
      profile.profile = video_codec_profile;
      profiles.push_back(profile);

      DVLOGF(3) << "Found decoder profile " << GetProfileName(profile.profile)
                << ", resolutions: " << profile.min_resolution.ToString() << " "
                << profile.max_resolution.ToString();
    }
  }

  return profiles;
}

VideoEncodeAccelerator::SupportedProfiles
V4L2Device::EnumerateSupportedEncodeProfiles() {
  VideoEncodeAccelerator::SupportedProfiles profiles;

  const auto& supported_pixelformats =
      EnumerateSupportedPixelformats(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);

  for (const auto& pixelformat : supported_pixelformats) {
    VideoEncodeAccelerator::SupportedProfile profile;
    profile.max_framerate_numerator = 30;
    profile.max_framerate_denominator = 1;
    gfx::Size min_resolution;
    GetSupportedResolution(pixelformat, &min_resolution,
                           &profile.max_resolution);

    const auto video_codec_profiles =
        V4L2PixFmtToVideoCodecProfiles(pixelformat, true);

    for (const auto& video_codec_profile : video_codec_profiles) {
      profile.profile = video_codec_profile;
      profiles.push_back(profile);

      DVLOGF(3) << "Found encoder profile " << GetProfileName(profile.profile)
                << ", max resolution: " << profile.max_resolution.ToString();
    }
  }

  return profiles;
}

bool V4L2Device::StartPolling(V4L2DevicePoller::EventCallback event_callback,
                              base::RepeatingClosure error_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  if (!device_poller_) {
    device_poller_ =
        std::make_unique<V4L2DevicePoller>(this, "V4L2DeviceThreadPoller");
  }

  bool ret = device_poller_->StartPolling(std::move(event_callback),
                                          std::move(error_callback));

  if (!ret)
    device_poller_ = nullptr;

  return ret;
}

bool V4L2Device::StopPolling() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  return !device_poller_ || device_poller_->StopPolling();
}

void V4L2Device::SchedulePoll() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  if (!device_poller_ || !device_poller_->IsPolling())
    return;

  device_poller_->SchedulePoll();
}

base::Optional<struct v4l2_event> V4L2Device::DequeueEvent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  struct v4l2_event event;
  memset(&event, 0, sizeof(event));

  if (Ioctl(VIDIOC_DQEVENT, &event) != 0) {
    VPLOGF(3) << "Failed to dequeue event";
    return base::nullopt;
  }

  return event;
}

V4L2RequestsQueue* V4L2Device::GetRequestsQueue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  if (requests_queue_creation_called_)
    return requests_queue_.get();

  requests_queue_creation_called_ = true;
  int media_fd = open(REQUEST_DEVICE, O_RDWR, 0);
  if (media_fd < 0) {
    VPLOGF(1) << "Failed to open media device.";
    return nullptr;
  }

  // Not using std::make_unique because constructor is private.
  std::unique_ptr<V4L2RequestsQueue> requests_queue(new V4L2RequestsQueue(
      base::ScopedFD(media_fd)));
  requests_queue_ = std::move(requests_queue);

  return requests_queue_.get();
}

bool V4L2Device::IsCtrlExposed(uint32_t ctrl_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  struct v4l2_queryctrl query_ctrl;
  memset(&query_ctrl, 0, sizeof(query_ctrl));
  query_ctrl.id = ctrl_id;

  return Ioctl(VIDIOC_QUERYCTRL, &query_ctrl) == 0;
}

bool V4L2Device::SetExtCtrls(uint32_t ctrl_class,
                             std::vector<V4L2ExtCtrl> ctrls,
                             V4L2RequestRef* request_ref) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  if (ctrls.empty())
    return true;

  struct v4l2_ext_controls ext_ctrls;
  memset(&ext_ctrls, 0, sizeof(ext_ctrls));
  ext_ctrls.ctrl_class = ctrl_class;
  ext_ctrls.count = ctrls.size();
  ext_ctrls.controls = &ctrls[0].ctrl;

  if (request_ref)
    request_ref->ApplyCtrls(&ext_ctrls);

  return Ioctl(VIDIOC_S_EXT_CTRLS, &ext_ctrls) == 0;
}

base::Optional<struct v4l2_ext_control> V4L2Device::GetCtrl(uint32_t ctrl_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  struct v4l2_ext_control ctrl;
  memset(&ctrl, 0, sizeof(ctrl));
  struct v4l2_ext_controls ext_ctrls;
  memset(&ext_ctrls, 0, sizeof(ext_ctrls));

  ctrl.id = ctrl_id;
  ext_ctrls.controls = &ctrl;
  ext_ctrls.count = 1;

  if (Ioctl(VIDIOC_G_EXT_CTRLS, &ext_ctrls) != 0) {
    VPLOGF(3) << "Failed to get control";
    return base::nullopt;
  }

  return ctrl;
}

class V4L2Request {
 public:
  // Apply the passed controls to the request.
  bool ApplyCtrls(struct v4l2_ext_controls* ctrls);
  // Apply the passed buffer to the request..
  bool ApplyQueueBuffer(struct v4l2_buffer* buffer);
  // Submits the request to the driver.
  bool Submit();
  // Indicates if the request has completed.
  bool IsCompleted();
  // Waits for the request to complete for a determined timeout. Returns false
  // if the request is not ready or other error. Default timeout is 500ms.
  bool WaitForCompletion(int poll_timeout_ms = 500);
  // Resets the request.
  bool Reset();

 private:
  V4L2RequestsQueue* request_queue_;
  int ref_counter_ = 0;
  base::ScopedFD request_fd_;

  friend class V4L2RequestsQueue;
  V4L2Request(base::ScopedFD&& request_fd, V4L2RequestsQueue* request_queue) :
   request_queue_(request_queue), request_fd_(std::move(request_fd)) {}

  friend class V4L2RequestRefBase;
  // Increases the number of request references.
  void IncRefCounter();
  // Decreases the number of request references.
  // When the counters reaches zero, the request is returned to the queue.
  int DecRefCounter();

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(V4L2Request);
};

void V4L2Request::IncRefCounter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ref_counter_++;
}

int V4L2Request::DecRefCounter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ref_counter_--;

  if (ref_counter_< 1)
    request_queue_->ReturnRequest(this);

  return ref_counter_;
}

bool V4L2Request::ApplyCtrls(struct v4l2_ext_controls* ctrls) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(ctrls, nullptr);

  if (!request_fd_.is_valid()) {
    VPLOGF(1) << "Invalid request";
    return false;
  }

  ctrls->which = V4L2_CTRL_WHICH_REQUEST_VAL;
  ctrls->request_fd = request_fd_.get();

  return true;
}

bool V4L2Request::ApplyQueueBuffer(struct v4l2_buffer* buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(buffer, nullptr);

  if (!request_fd_.is_valid()) {
    VPLOGF(1) << "Invalid request";
    return false;
  }

  buffer->flags |= V4L2_BUF_FLAG_REQUEST_FD;
  buffer->request_fd = request_fd_.get();

  return true;
}

bool V4L2Request::Submit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!request_fd_.is_valid()) {
    VPLOGF(1) << "No valid request file descriptor to submit request.";
    return false;
  }

  return HANDLE_EINTR(ioctl(request_fd_.get(), MEDIA_REQUEST_IOC_QUEUE)) == 0;
}

bool V4L2Request::IsCompleted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return WaitForCompletion(0);
}

bool V4L2Request::WaitForCompletion(int poll_timeout_ms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!request_fd_.is_valid()) {
    VPLOGF(1) << "Invalid request";
    return false;
  }

  struct pollfd poll_fd = {request_fd_.get(), POLLPRI, 0};

  // Poll the request to ensure its previous task is done
  switch (poll(&poll_fd, 1, poll_timeout_ms)) {
    case 1:
      return true;
    case 0:
      // Not an error - we just timed out.
      DVLOGF(4) << "Request poll(" << poll_timeout_ms << ") timed out";
      return false;
    case -1:
      VPLOGF(1) << "Failed to poll request";
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

bool V4L2Request::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!request_fd_.is_valid()) {
    VPLOGF(1) << "Invalid request";
    return false;
  }

  // Reinit the request to make sure we can use it for a new submission.
  if (HANDLE_EINTR(ioctl(request_fd_.get(), MEDIA_REQUEST_IOC_REINIT)) < 0) {
    VPLOGF(1) << "Failed to reinit request.";
    return false;
  }

  return true;
}

V4L2RequestRefBase::V4L2RequestRefBase(V4L2RequestRefBase&& req_base) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  request_ = req_base.request_;
  req_base.request_ = nullptr;
}

V4L2RequestRefBase::V4L2RequestRefBase(V4L2Request* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (request) {
    request_ = request;
    request_->IncRefCounter();
  }
}

V4L2RequestRefBase::~V4L2RequestRefBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (request_)
    request_->DecRefCounter();
}

bool V4L2RequestRef::ApplyCtrls(struct v4l2_ext_controls* ctrls) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(request_, nullptr);

  return request_->ApplyCtrls(ctrls);
}

bool V4L2RequestRef::ApplyQueueBuffer(struct v4l2_buffer* buffer) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(request_, nullptr);

  return request_->ApplyQueueBuffer(buffer);
}

base::Optional<V4L2SubmittedRequestRef> V4L2RequestRef::Submit() && {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(request_, nullptr);

  V4L2RequestRef self(std::move(*this));

  if (!self.request_->Submit())
    return base::nullopt;

  return V4L2SubmittedRequestRef(self.request_);
}

bool V4L2SubmittedRequestRef::IsCompleted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(request_, nullptr);

  return request_->IsCompleted();
}

V4L2RequestsQueue::V4L2RequestsQueue(base::ScopedFD&& media_fd) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  media_fd_ = std::move(media_fd);
}

V4L2RequestsQueue::~V4L2RequestsQueue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  requests_.clear();
  media_fd_.reset();
}

base::Optional<base::ScopedFD> V4L2RequestsQueue::CreateRequestFD() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int request_fd;
  int ret = HANDLE_EINTR(
        ioctl(media_fd_.get(), MEDIA_IOC_REQUEST_ALLOC, &request_fd));
  if (ret < 0) {
    VPLOGF(1) << "Failed to create request";
    return base::nullopt;
  }

  return base::ScopedFD(request_fd);
}

base::Optional<V4L2RequestRef> V4L2RequestsQueue::GetFreeRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  V4L2Request* request_ptr =
      free_requests_.empty() ? nullptr : free_requests_.front();
  if (request_ptr && request_ptr->IsCompleted()) {
    // Previous request is already completed, just recycle it.
    free_requests_.pop();
  } else if (requests_.size() < kMaxNumRequests) {
    // No request yet, or not completed, but we can allocate a new one.
    auto request_fd = CreateRequestFD();
    if (!request_fd.has_value()) {
      VLOGF(1) << "Error while creating a new request FD!";
      return base::nullopt;
    }
    // Not using std::make_unique because constructor is private.
    std::unique_ptr<V4L2Request> request(
        new V4L2Request(std::move(*request_fd), this));
    request_ptr = request.get();
    requests_.push_back(std::move(request));
    VLOGF(4) << "Allocated new request, total number: " << requests_.size();
  } else {
    // Request is not completed and we have reached the maximum number.
    // Wait for it to complete.
    VLOGF(1) << "Waiting for request completion. This probably means a "
             << "request is blocking.";
    if (!request_ptr->WaitForCompletion()) {
      VLOG(1) << "Timeout while waiting for request to complete.";
      return base::nullopt;
    }
    free_requests_.pop();
  }

  DCHECK(request_ptr);
  if (!request_ptr->Reset()) {
    VPLOGF(1) << "Failed to reset request";
    return base::nullopt;
  }

  return V4L2RequestRef(request_ptr);
}

void V4L2RequestsQueue::ReturnRequest(V4L2Request* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(request);

  if (request)
    free_requests_.push(request);
}

}  //  namespace media
