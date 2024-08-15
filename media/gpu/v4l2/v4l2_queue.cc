// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_queue.h"

#include <errno.h>
#include <fcntl.h>
#include <libdrm/drm_fourcc.h>
#include <linux/media.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "base/containers/contains.h"
#include "base/not_fatal_until.h"
#include "base/posix/eintr_wrapper.h"
#include "base/trace_event/trace_event.h"
#include "media/gpu/chromeos/native_pixmap_frame_resource.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/macros.h"

namespace media {

namespace {

// TODO(jkardatzke): Remove this when it is in linux/videodev2.h.
#define V4L2_MEMORY_FLAG_SECURE 0x2

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
  format.fmt.pix_mp.num_planes = GetNumPlanesOfV4L2PixFmt(fourcc);
  format.fmt.pix_mp.plane_fmt[0].sizeimage = buffer_size;

  return format;
}

const char* V4L2BufferTypeToString(const enum v4l2_buf_type buf_type) {
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

int64_t V4L2BufferTimestampInMilliseconds(
    const struct v4l2_buffer* v4l2_buffer) {
  struct timespec ts;
  TIMEVAL_TO_TIMESPEC(&v4l2_buffer->timestamp, &ts);

  return base::TimeDelta::FromTimeSpec(ts).InMilliseconds();
}

// For decoding and encoding data to be processed is enqueued in the
// V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE queue.  Once that data has been either
// decompressed or compressed, the finished buffer is dequeued from the
// V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE queue.  This occurs asynchronously so
// there is no way to measure how long the hardware took to process the data.
// We can use the length of time that a buffer is enqueued as a proxy for
// how busy the hardware is.
void V4L2ProcessingTrace(const struct v4l2_buffer* v4l2_buffer, bool start) {
  constexpr char kTracingCategory[] = "media,gpu";
  constexpr char kQueueBuffer[] = "V4L2 Queue Buffer";
  constexpr char kDequeueBuffer[] = "V4L2 Dequeue Buffer";
  constexpr char kVideoDecoding[] = "V4L2 Video Decoding";

  bool tracing_enabled = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(kTracingCategory, &tracing_enabled);
  if (!tracing_enabled) {
    return;
  }

  const char* name = start ? kQueueBuffer : kDequeueBuffer;
  TRACE_EVENT_INSTANT1(kTracingCategory, name, TRACE_EVENT_SCOPE_THREAD, "type",
                       v4l2_buffer->type);

  // TODO(mcasas): Consider using TimeValToTimeDelta().
  const int64_t timestamp = V4L2BufferTimestampInMilliseconds(v4l2_buffer);
  if (timestamp <= 0) {
    return;
  }

  if (start && v4l2_buffer->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(kTracingCategory, kVideoDecoding,
                                      TRACE_ID_LOCAL(timestamp), "timestamp",
                                      timestamp);
  } else if (!start &&
             v4l2_buffer->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
    TRACE_EVENT_NESTABLE_ASYNC_END1(kTracingCategory, kVideoDecoding,
                                    TRACE_ID_LOCAL(timestamp), "timestamp",
                                    timestamp);
  }
}

// Returns a vector of dmabuf file descriptors, exported for V4L2 buffer with
// |index|, assuming the buffer contains |num_planes| V4L2 planes and is of
// |buf_type|. Returns an empty vector on failure. The caller is responsible for
// closing the file descriptors after use.
std::vector<base::ScopedFD> GetDmabufsForV4L2Buffer(
    const IoctlAsCallback& ioctl_cb,
    int index,
    size_t num_planes,
    enum v4l2_buf_type buf_type) {
  DVLOGF(3);
  DCHECK(V4L2_TYPE_IS_MULTIPLANAR(buf_type));

  std::vector<base::ScopedFD> dmabuf_fds;
  for (size_t i = 0; i < num_planes; ++i) {
    struct v4l2_exportbuffer expbuf;
    memset(&expbuf, 0, sizeof(expbuf));
    expbuf.type = buf_type;
    expbuf.index = index;
    expbuf.plane = i;
    expbuf.flags = O_CLOEXEC;
    if (ioctl_cb.Run(VIDIOC_EXPBUF, &expbuf) != 0) {
      RecordVidiocIoctlErrorUMA(VidiocIoctlRequests::kVidiocExpbuf);
      dmabuf_fds.clear();
      break;
    }

    dmabuf_fds.push_back(base::ScopedFD(expbuf.fd));
  }

  return dmabuf_fds;
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
  static std::unique_ptr<V4L2Buffer> Create(
      const IoctlAsCallback& ioctl_cb,
      const MmapAsCallback& mmap_cb,
      enum v4l2_buf_type type,
      enum v4l2_memory memory,
      const struct v4l2_format& format,
      size_t buffer_id);

  V4L2Buffer(const V4L2Buffer&) = delete;
  V4L2Buffer& operator=(const V4L2Buffer&) = delete;

  ~V4L2Buffer();

  void* GetPlaneMapping(const size_t plane);
  size_t GetMemoryUsage() const;
  const struct v4l2_buffer& v4l2_buffer() const { return v4l2_buffer_; }
  const scoped_refptr<FrameResource>& GetFrameResource();

 private:
  V4L2Buffer(const IoctlAsCallback& ioctl_cb,
             const MmapAsCallback& mmap_cb,
             enum v4l2_buf_type type,
             enum v4l2_memory memory,
             const struct v4l2_format& format,
             size_t buffer_id);
  bool Query();
  scoped_refptr<FrameResource> CreateFrame();

  const IoctlAsCallback ioctl_cb_;
  const MmapAsCallback mmap_cb_;
  std::vector<void*> plane_mappings_;

  // V4L2 data as queried by QUERYBUF.
  struct v4l2_buffer v4l2_buffer_;
  // WARNING: do not change this to a vector or something smaller than
  // VIDEO_MAX_PLANES (the maximum number of planes V4L2 supports). The
  // element overhead is small and may avoid memory corruption bugs.
  struct v4l2_plane v4l2_planes_[VIDEO_MAX_PLANES];

  struct v4l2_format format_;
  scoped_refptr<FrameResource> frame_;
  base::WeakPtrFactory<V4L2Buffer> weak_factory_{this};
};

std::unique_ptr<V4L2Buffer> V4L2Buffer::Create(
    const IoctlAsCallback& ioctl_cb,
    const MmapAsCallback& mmap_cb,
    enum v4l2_buf_type type,
    enum v4l2_memory memory,
    const struct v4l2_format& format,
    size_t buffer_id) {
  // Not using std::make_unique because constructor is private.
  std::unique_ptr<V4L2Buffer> buffer(new V4L2Buffer(std::move(ioctl_cb),
                                                    std::move(mmap_cb), type,
                                                    memory, format, buffer_id));
  if (!buffer->Query()) {
    return nullptr;
  }

  return buffer;
}

V4L2Buffer::V4L2Buffer(const IoctlAsCallback& ioctl_cb,
                       const MmapAsCallback& mmap_cb,
                       enum v4l2_buf_type type,
                       enum v4l2_memory memory,
                       const struct v4l2_format& format,
                       size_t buffer_id)
    : ioctl_cb_(ioctl_cb), mmap_cb_(mmap_cb), format_(format) {
  DCHECK(V4L2_TYPE_IS_MULTIPLANAR(type));
  DCHECK_LE(format.fmt.pix_mp.num_planes, std::size(v4l2_planes_));

  memset(&v4l2_buffer_, 0, sizeof(v4l2_buffer_));
  memset(v4l2_planes_, 0, sizeof(v4l2_planes_));
  v4l2_buffer_.m.planes = v4l2_planes_;
  // Just in case we got more planes than we want.
  v4l2_buffer_.length =
      std::min(static_cast<size_t>(format.fmt.pix_mp.num_planes),
               std::size(v4l2_planes_));
  v4l2_buffer_.index = buffer_id;
  v4l2_buffer_.type = type;
  v4l2_buffer_.memory = memory;
  plane_mappings_.resize(v4l2_buffer_.length);
}

V4L2Buffer::~V4L2Buffer() {
  if (v4l2_buffer_.memory == V4L2_MEMORY_MMAP) {
    for (size_t i = 0; i < plane_mappings_.size(); i++) {
      if (plane_mappings_[i] != nullptr) {
        munmap(plane_mappings_[i], v4l2_buffer_.m.planes[i].length);
      }
    }
  }
}

bool V4L2Buffer::Query() {
  int ret = ioctl_cb_.Run(VIDIOC_QUERYBUF, &v4l2_buffer_);
  if (ret) {
    RecordVidiocIoctlErrorUMA(VidiocIoctlRequests::kVidiocQuerybuf);
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
  if (p) {
    return p;
  }

  // Do this check here to avoid repeating it after a buffer has been
  // successfully mapped (we know we are of MMAP type by then).
  if (v4l2_buffer_.memory != V4L2_MEMORY_MMAP) {
    VLOGF(1) << "Cannot create mapping on non-MMAP buffer";
    return nullptr;
  }

  p = mmap_cb_.Run(nullptr, v4l2_buffer_.m.planes[plane].length,
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

scoped_refptr<FrameResource> V4L2Buffer::CreateFrame() {
  auto layout = V4L2FormatToVideoFrameLayout(format_);
  if (!layout) {
    VLOGF(1) << "Cannot create frame layout for V4L2 buffers";
    return nullptr;
  }

  std::vector<base::ScopedFD> dmabuf_fds = GetDmabufsForV4L2Buffer(
      ioctl_cb_, v4l2_buffer_.index, v4l2_buffer_.length,
      static_cast<enum v4l2_buf_type>(v4l2_buffer_.type));
  if (dmabuf_fds.empty()) {
    VLOGF(1) << "Failed to get DMABUFs of V4L2 buffer";
    return nullptr;
  }

  // DMA buffer fds should not be invalid
  for (const auto& dmabuf_fd : dmabuf_fds) {
    if (!dmabuf_fd.is_valid()) {
      DLOG(ERROR) << "Fail to get DMABUFs of V4L2 buffer - invalid fd";
      return nullptr;
    }
  }

  // Duplicate the fd of the last v4l2 plane until the number of fds are the
  // same as the number of color planes.
  while (dmabuf_fds.size() < layout->planes().size()) {
    int duped_fd = HANDLE_EINTR(dup(dmabuf_fds.back().get()));
    if (duped_fd == -1) {
      DLOG(ERROR) << "Failed duplicating dmabuf fd";
      return nullptr;
    }

    dmabuf_fds.emplace_back(duped_fd);
  }

  gfx::Size size(format_.fmt.pix_mp.width, format_.fmt.pix_mp.height);

  return NativePixmapFrameResource::Create(
      *layout, gfx::Rect(size), size, std::move(dmabuf_fds), base::TimeDelta());
}

const scoped_refptr<FrameResource>& V4L2Buffer::GetFrameResource() {
  // We can create the FrameResource only when using MMAP buffers.
  if (v4l2_buffer_.memory != V4L2_MEMORY_MMAP) {
    VLOGF(1) << "Cannot create video frame from non-MMAP buffer";
    // Allow NOTREACHED() on invalid argument because this is an internal
    // method.
    NOTREACHED_IN_MIGRATION();
  }

  // Create the video frame instance if requiring it for the first time.
  if (!frame_) {
    frame_ = CreateFrame();
  }

  return frame_;
}

// A thread-safe pool of buffer indexes, allowing buffers to be obtained and
// returned from different threads. All the methods of this class are
// thread-safe. Users should keep a scoped_refptr to instances of this class
// in order to ensure the list remains alive as long as they need it.
class V4L2BuffersList : public base::RefCountedThreadSafe<V4L2BuffersList> {
 public:
  V4L2BuffersList() = default;

  V4L2BuffersList(const V4L2BuffersList&) = delete;
  V4L2BuffersList& operator=(const V4L2BuffersList&) = delete;

  // Return a buffer to this list. Also can be called to set the initial pool
  // of buffers.
  // Note that it is illegal to return the same buffer twice.
  void ReturnBuffer(size_t buffer_id);
  // Get any of the buffers in the list. There is no order guarantee whatsoever.
  std::optional<size_t> GetFreeBuffer();
  // Get the buffer with specified index.
  std::optional<size_t> GetFreeBuffer(size_t requested_buffer_id);
  // Number of buffers currently in this list.
  size_t size() const;

 private:
  friend class base::RefCountedThreadSafe<V4L2BuffersList>;
  ~V4L2BuffersList() = default;

  mutable base::Lock lock_;
  std::set<size_t> free_buffers_ GUARDED_BY(lock_);
};

void V4L2BuffersList::ReturnBuffer(size_t buffer_id) {
  base::AutoLock auto_lock(lock_);

  auto inserted = free_buffers_.emplace(buffer_id);
  DCHECK(inserted.second);
}

std::optional<size_t> V4L2BuffersList::GetFreeBuffer() {
  base::AutoLock auto_lock(lock_);

  auto iter = free_buffers_.begin();
  if (iter == free_buffers_.end()) {
    DVLOGF(4) << "No free buffer available!";
    return std::nullopt;
  }

  size_t buffer_id = *iter;
  free_buffers_.erase(iter);

  return buffer_id;
}

std::optional<size_t> V4L2BuffersList::GetFreeBuffer(
    size_t requested_buffer_id) {
  base::AutoLock auto_lock(lock_);

  return (free_buffers_.erase(requested_buffer_id) > 0)
             ? std::make_optional(requested_buffer_id)
             : std::nullopt;
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

  V4L2BufferRefBase(const V4L2BufferRefBase&) = delete;
  V4L2BufferRefBase& operator=(const V4L2BufferRefBase&) = delete;

  ~V4L2BufferRefBase();

  bool QueueBuffer(scoped_refptr<FrameResource> frame);
  void* GetPlaneMapping(const size_t plane);

  const scoped_refptr<FrameResource>& GetFrameResource();
  // Checks that the number of passed FDs is adequate for the current format
  // and buffer configuration. Only useful for DMABUF buffers.
  bool CheckNumFDsForFormat(const size_t num_fds) const;

  // Data from the buffer, that users can query and/or write.
  struct v4l2_buffer v4l2_buffer_;
  // WARNING: do not change this to a vector or something smaller than
  // VIDEO_MAX_PLANES (the maximum number of planes V4L2 supports). The
  // element overhead is small and may avoid memory corruption bugs.
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
};

V4L2BufferRefBase::V4L2BufferRefBase(const struct v4l2_buffer& v4l2_buffer,
                                     base::WeakPtr<V4L2Queue> queue)
    : queue_(std::move(queue)), return_to_(queue_->free_buffers_) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(V4L2_TYPE_IS_MULTIPLANAR(v4l2_buffer.type));
  DCHECK_LE(v4l2_buffer.length, std::size(v4l2_planes_));
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
  if (!queued) {
    return_to_->ReturnBuffer(BufferId());
  }
}

bool V4L2BufferRefBase::QueueBuffer(scoped_refptr<FrameResource> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!queue_) {
    return false;
  }

  queued = queue_->QueueBuffer(&v4l2_buffer_, std::move(frame));

  return queued;
}

void* V4L2BufferRefBase::GetPlaneMapping(const size_t plane) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!queue_) {
    return nullptr;
  }

  return queue_->buffers_[BufferId()]->GetPlaneMapping(plane);
}

const scoped_refptr<FrameResource>& V4L2BufferRefBase::GetFrameResource() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Used so we can return a const scoped_refptr& in all cases.
  static const scoped_refptr<FrameResource> null_frame_resource;

  if (!queue_) {
    return null_frame_resource;
  }

  DCHECK_LE(BufferId(), queue_->buffers_.size());

  return queue_->buffers_[BufferId()]->GetFrameResource();
}

bool V4L2BufferRefBase::CheckNumFDsForFormat(const size_t num_fds) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!queue_) {
    return false;
  }

  // We have not used SetFormat(), assume this is ok.
  // Hopefully we standardize SetFormat() in the future.
  if (!queue_->current_format_) {
    return true;
  }

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

  if (this == &other) {
    return *this;
  }

  buffer_data_ = std::move(other.buffer_data_);

  return *this;
}

scoped_refptr<FrameResource> V4L2WritableBufferRef::GetFrameResource() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  return buffer_data_->GetFrameResource();
}

enum v4l2_memory V4L2WritableBufferRef::Memory() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  return static_cast<enum v4l2_memory>(buffer_data_->v4l2_buffer_.memory);
}

bool V4L2WritableBufferRef::DoQueue(V4L2RequestRef* request_ref,
                                    scoped_refptr<FrameResource> frame) && {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  if (request_ref && buffer_data_->queue_->SupportsRequests() &&
      !request_ref->ApplyQueueBuffer(&(buffer_data_->v4l2_buffer_))) {
    return false;
  }

  bool queued = buffer_data_->QueueBuffer(std::move(frame));

  // Clear our own reference.
  buffer_data_.reset();

  return queued;
}

bool V4L2WritableBufferRef::QueueMMap(V4L2RequestRef* request_ref) && {
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

bool V4L2WritableBufferRef::QueueUserPtr(const std::vector<void*>& ptrs,
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

  for (size_t i = 0; i < ptrs.size(); i++) {
    self.buffer_data_->v4l2_buffer_.m.planes[i].m.userptr =
        reinterpret_cast<unsigned long>(ptrs[i]);
  }

  return std::move(self).DoQueue(request_ref, nullptr);
}

bool V4L2WritableBufferRef::QueueDMABuf(const std::vector<base::ScopedFD>& fds,
                                        V4L2RequestRef* request_ref) && {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  // Move ourselves so our data gets freed no matter when we return
  V4L2WritableBufferRef self(std::move(*this));

  if (self.Memory() != V4L2_MEMORY_DMABUF) {
    VLOGF(1) << "Called on invalid buffer type!";
    return false;
  }

  if (!self.buffer_data_->CheckNumFDsForFormat(fds.size())) {
    return false;
  }

  size_t num_planes = self.PlanesCount();
  for (size_t i = 0; i < num_planes; i++) {
    self.buffer_data_->v4l2_buffer_.m.planes[i].m.fd = fds[i].get();
  }

  return std::move(self).DoQueue(request_ref, nullptr);
}

bool V4L2WritableBufferRef::QueueDMABuf(scoped_refptr<FrameResource> frame,
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
  if (frame->storage_type() != VideoFrame::STORAGE_GPU_MEMORY_BUFFER &&
      frame->storage_type() != VideoFrame::STORAGE_DMABUFS) {
    VLOGF(1) << "Only frames with GpuMemoryBuffer and dma-buf are supported";
    return false;
  }

  // The FDs duped by CreateGpuMemoryBufferHandle() will be closed after the
  // call to DoQueue() which uses the VIDIOC_QBUF ioctl and so ends up
  // increasing the reference count of the dma-buf. Thus, closing the FDs is
  // safe.
  // TODO(andrescj): for dma-buf frames, duping the FDs is unnecessary.
  // Consider handling that path separately.
  gfx::GpuMemoryBufferHandle gmb_handle = frame->CreateGpuMemoryBufferHandle();
  if (gmb_handle.type != gfx::GpuMemoryBufferType::NATIVE_PIXMAP) {
    VLOGF(1) << "Failed to create GpuMemoryBufferHandle for frame!";
    return false;
  }
  const std::vector<gfx::NativePixmapPlane>& planes =
      gmb_handle.native_pixmap_handle.planes;

  if (!self.buffer_data_->CheckNumFDsForFormat(planes.size())) {
    return false;
  }

  size_t num_planes = self.PlanesCount();
  for (size_t i = 0; i < num_planes; i++) {
    self.buffer_data_->v4l2_buffer_.m.planes[i].m.fd = planes[i].fd.get();
  }

  return std::move(self).DoQueue(request_ref, std::move(frame));
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

  if (!self.buffer_data_->CheckNumFDsForFormat(planes.size())) {
    return false;
  }

  size_t num_planes = self.PlanesCount();
  for (size_t i = 0; i < num_planes; i++) {
    self.buffer_data_->v4l2_buffer_.m.planes[i].m.fd = planes[i].fd.get();
  }

  return std::move(self).DoQueue(request_ref, nullptr);
}

bool V4L2WritableBufferRef::QueueDMABuf(uint64_t secure_handle,
                                        V4L2RequestRef* request_ref) && {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  // Move ourselves so our data gets freed no matter when we return
  V4L2WritableBufferRef self(std::move(*this));

  if (self.Memory() != V4L2_MEMORY_DMABUF) {
    VLOGF(1) << "Called on invalid buffer type!";
    return false;
  }

  // Set the FD for the secure handle.
  bool set_fd = self.buffer_data_->queue_->SetBufferFdForSecureHandle(
      secure_handle, &self.buffer_data_->v4l2_buffer_);
  if (!set_fd) {
    return false;
  }

  // The FD should already be set in the plane data, so submit it.
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

V4L2ReadableBuffer::V4L2ReadableBuffer(const struct v4l2_buffer& v4l2_buffer,
                                       base::WeakPtr<V4L2Queue> queue,
                                       scoped_refptr<FrameResource> frame)
    : buffer_data_(
          std::make_unique<V4L2BufferRefBase>(v4l2_buffer, std::move(queue))),
      frame_(std::move(frame)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

scoped_refptr<FrameResource> V4L2ReadableBuffer::GetFrameResource() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);

  if (buffer_data_->v4l2_buffer_.memory == V4L2_MEMORY_DMABUF && frame_) {
    return frame_;
  }
  return buffer_data_->GetFrameResource();
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

bool V4L2ReadableBuffer::IsError() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data_);
  // "The driver may also set V4L2_BUF_FLAG_ERROR in the flags field. It
  //  indicates a non-critical (recoverable) streaming error. In such case the
  //  application may continue as normal, but should be aware that data in the
  //  dequeued buffer might be corrupted." IOW it is more a discard-this-buffer
  //  marker than a fatal error indication, so it's down to the caller to take
  //  action if needed/desired.
  // https://www.kernel.org/doc/html/v5.15/userspace-api/media/v4l/vidioc-qbuf.html#description
  return buffer_data_->v4l2_buffer_.flags & V4L2_BUF_FLAG_ERROR;
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

struct SecureBufferData {
  SecureBufferData(uint64_t in_secure_handle, base::ScopedFD in_fd)
      : secure_handle(in_secure_handle), fd(std::move(in_fd)) {}
  SecureBufferData(SecureBufferData&& other) = default;
  ~SecureBufferData() {}
  // true if the secure buffer stores decrypted data from an active
  // DecoderBuffer.
  bool owned_by_decoder_buffer = false;
  // List of all the buffer indexes that have this FD/secure_handle currently
  // attached to it.
  std::vector<size_t> queued_buffer_indexes;
  uint64_t secure_handle;
  base::ScopedFD fd;
};

// Helper macros that print the queue type with logs.
#define VPQLOGF(level) \
  VPLOGF(level) << "(" << V4L2BufferTypeToString(type_) << ") "
#define VQLOGF(level) \
  VLOGF(level) << "(" << V4L2BufferTypeToString(type_) << ") "
#define DVQLOGF(level) \
  DVLOGF(level) << "(" << V4L2BufferTypeToString(type_) << ") "

V4L2Queue::V4L2Queue(const IoctlAsCallback& ioctl_cb,
                     const base::RepeatingClosure& schedule_poll_cb,
                     const MmapAsCallback& mmap_cb,
                     const AllocateSecureBufferAsCallback& allocate_secure_cb,
                     enum v4l2_buf_type type,
                     base::OnceClosure destroy_cb)
    : type_(type),
      ioctl_cb_(ioctl_cb),
      schedule_poll_cb_(schedule_poll_cb),
      mmap_cb_(mmap_cb),
      allocate_secure_cb_(allocate_secure_cb),
      destroy_cb_(std::move(destroy_cb)),
      weak_this_factory_(this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  struct v4l2_requestbuffers reqbufs = {
      .count = 0, .type = type_, .memory = V4L2_MEMORY_MMAP};
  supports_requests_ = (ioctl_cb_.Run(VIDIOC_REQBUFS, &reqbufs) == kIoctlOk) &&
                       (reqbufs.capabilities & V4L2_BUF_CAP_SUPPORTS_REQUESTS);

  // Stateful backends for example do not support requests.
  VPLOG_IF(4, supports_requests_)
      << "This queue does " << (supports_requests_ ? "" : "not")
      << " support requests.";
}

V4L2Queue::~V4L2Queue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_streaming_ && !Streamoff()) {
    VQLOGF(1) << "Failed to stop queue";
  }

  DCHECK(queued_buffers_.empty());

  if (!buffers_.empty() && !DeallocateBuffers()) {
    VQLOGF(1) << "Failed to deallocate queue buffers";
  }

  std::move(destroy_cb_).Run();
}

std::optional<struct v4l2_format> V4L2Queue::SetFormat(uint32_t fourcc,
                                                       const gfx::Size& size,
                                                       size_t buffer_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  struct v4l2_format format = BuildV4L2Format(type_, fourcc, size, buffer_size);
  if (ioctl_cb_.Run(VIDIOC_S_FMT, &format) != 0 ||
      format.fmt.pix_mp.pixelformat != fourcc) {
    RecordVidiocIoctlErrorUMA(VidiocIoctlRequests::kVidiocSFmt);
    VPQLOGF(2) << "Failed to set format fourcc: " << FourccToString(fourcc);
    return std::nullopt;
  }

  current_format_ = format;
  return current_format_;
}

std::optional<struct v4l2_format> V4L2Queue::TryFormat(uint32_t fourcc,
                                                       const gfx::Size& size,
                                                       size_t buffer_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  struct v4l2_format format = BuildV4L2Format(type_, fourcc, size, buffer_size);
  if (ioctl_cb_.Run(VIDIOC_TRY_FMT, &format) != 0 ||
      format.fmt.pix_mp.pixelformat != fourcc) {
    VPQLOGF(2) << "Failed to try format fourcc: " << FourccToString(fourcc);
    return std::nullopt;
  }

  return format;
}

std::pair<std::optional<struct v4l2_format>, int> V4L2Queue::GetFormat() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = type_;
  if (ioctl_cb_.Run(VIDIOC_G_FMT, &format) != 0) {
    RecordVidiocIoctlErrorUMA(VidiocIoctlRequests::kVidiocGFmt);
    VPQLOGF(2) << "Failed to get format";
    return std::make_pair(std::nullopt, errno);
  }

  return std::make_pair(format, 0);
}

std::optional<gfx::Rect> V4L2Queue::GetVisibleRect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  struct v4l2_selection selection = {.type = type_,
                                     .target = V4L2_SEL_TGT_COMPOSE};
  if (ioctl_cb_.Run(VIDIOC_G_SELECTION, &selection) != 0) {
    RecordVidiocIoctlErrorUMA(VidiocIoctlRequests::kVidiocGSelection);
    VQLOGF(1) << "Failed to get visible rect";
    return std::nullopt;
  }
  return V4L2RectToGfxRect(selection.r);
}

size_t V4L2Queue::AllocateBuffers(size_t count,
                                  enum v4l2_memory memory,
                                  bool incoherent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!free_buffers_);
  DCHECK(queued_buffers_.empty());

  incoherent_ = incoherent;

  if (IsStreaming()) {
    VQLOGF(1) << "Cannot allocate buffers while streaming.";
    return 0;
  }

  if (buffers_.size() != 0) {
    VQLOGF(1)
        << "Cannot allocate new buffers while others are still allocated.";
    return 0;
  }
  // Should have been cleared in DeallocateBuffers() if it was ever filled in.
  DCHECK(free_buffers_indexes_.empty());

  if (count == 0) {
    VQLOGF(1) << "Attempting to allocate 0 buffers.";
    return 0;
  }

  // First query the number of planes in the buffers we are about to request.
  std::optional<v4l2_format> format = GetFormat().first;
  if (!format) {
    VQLOGF(1) << "Cannot get format.";
    return 0;
  }
  planes_count_ = format->fmt.pix_mp.num_planes;
  DCHECK_LE(planes_count_, static_cast<size_t>(VIDEO_MAX_PLANES));

  __u8 flags = incoherent ? V4L2_MEMORY_FLAG_NON_COHERENT : 0;
  if (allocate_secure_cb_) {
    flags |= V4L2_MEMORY_FLAG_SECURE;
  }
  struct v4l2_requestbuffers reqbufs = {
      .count = base::checked_cast<decltype(v4l2_requestbuffers::count)>(count),
      .type = type_,
      .memory = memory,
      .flags = flags};
  DVQLOGF(3) << "Requesting " << count << " buffers ("
             << (incoherent ? "incoherent" : "coherent") << ")";

  int ret = ioctl_cb_.Run(VIDIOC_REQBUFS, &reqbufs);
  if (ret) {
    RecordVidiocIoctlErrorUMA(VidiocIoctlRequests::kVidiocReqbufs);
    VPQLOGF(1) << "VIDIOC_REQBUFS failed";
    return 0;
  }
  DVQLOGF(3) << "Allocated " << reqbufs.count << " buffers.";

  memory_ = memory;

  free_buffers_ = new V4L2BuffersList();

  // Now query all buffer information.
  for (size_t i = 0; i < reqbufs.count; i++) {
    auto buffer =
        V4L2Buffer::Create(ioctl_cb_, mmap_cb_, type_, memory_, *format, i);

    if (!buffer) {
      if (!DeallocateBuffers()) {
        VQLOGF(1) << "Failed to deallocate queue buffers";
      }

      return 0;
    }

    if (type_ == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE && allocate_secure_cb_) {
      CHECK_EQ(memory_, V4L2_MEMORY_DMABUF);
      // Invoke the callback for secure buffer allocation. We only use dmabufs
      // for the OUTPUT queue when doing secure playback.
      allocate_secure_cb_.Run(buffer->v4l2_buffer().m.planes[0].length,
                              base::BindOnce(&V4L2Queue::SecureBufferAllocated,
                                             weak_this_factory_.GetWeakPtr()));
    }

    buffers_.emplace_back(std::move(buffer));
    free_buffers_->ReturnBuffer(i);
  }

  DCHECK(free_buffers_);
  DCHECK_EQ(free_buffers_->size(), buffers_.size());
  DCHECK(queued_buffers_.empty());

  return buffers_.size();
}

bool V4L2Queue::DeallocateBuffers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsStreaming()) {
    VQLOGF(1) << "Cannot deallocate buffers while streaming.";
    return false;
  }

  if (buffers_.size() == 0) {
    return true;
  }

  weak_this_factory_.InvalidateWeakPtrs();
  buffers_.clear();
  free_buffers_indexes_.clear();
  free_buffers_ = nullptr;
  secure_buffers_.clear();

  // Free all buffers.
  __u8 flags = incoherent_ ? V4L2_MEMORY_FLAG_NON_COHERENT : 0;
  if (allocate_secure_cb_) {
    flags |= V4L2_MEMORY_FLAG_SECURE;
  }
  struct v4l2_requestbuffers reqbufs = {
      .count = 0, .type = type_, .memory = memory_, .flags = flags};

  int ret = ioctl_cb_.Run(VIDIOC_REQBUFS, &reqbufs);
  if (ret) {
    RecordVidiocIoctlErrorUMA(VidiocIoctlRequests::kVidiocReqbufs);
    VPQLOGF(1) << "VIDIOC_REQBUFS failed";
    return false;
  }

  DCHECK(!free_buffers_);
  DCHECK(queued_buffers_.empty());

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return memory_;
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
      scoped_refptr<FrameResource> frame) {
    return new V4L2ReadableBuffer(v4l2_buffer, std::move(queue),
                                  std::move(frame));
  }
};

CroStatus::Or<uint64_t> V4L2Queue::GetFreeSecureHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Go through the list of secure buffers and find one that is not owned or
  // queued.
  for (auto& buf : secure_buffers_) {
    if (!buf.owned_by_decoder_buffer && buf.queued_buffer_indexes.empty()) {
      buf.owned_by_decoder_buffer = true;
      return buf.secure_handle;
    }
  }
  return CroStatus::Codes::kSecureBufferPoolEmpty;
}

void V4L2Queue::ReleaseSecureHandle(uint64_t secure_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Find the matching secure buffer and release the ownership on it, it might
  // still be in use, but that would be tracked by the queue counter if so.
  for (auto& buf : secure_buffers_) {
    if (buf.secure_handle == secure_handle) {
      buf.owned_by_decoder_buffer = false;
      return;
    }
  }
}

std::optional<V4L2WritableBufferRef> V4L2Queue::GetFreeBuffer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // No buffers allocated at the moment?
  if (!free_buffers_) {
    return std::nullopt;
  }

  auto buffer_id = free_buffers_->GetFreeBuffer();
  if (!buffer_id.has_value()) {
    return std::nullopt;
  }

  return V4L2BufferRefFactory::CreateWritableRef(
      buffers_[buffer_id.value()]->v4l2_buffer(),
      weak_this_factory_.GetWeakPtr());
}

std::optional<V4L2WritableBufferRef> V4L2Queue::GetFreeBuffer(
    size_t requested_buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // No buffers allocated at the moment?
  if (!free_buffers_) {
    return std::nullopt;
  }

  auto buffer_id = free_buffers_->GetFreeBuffer(requested_buffer_id);
  if (!buffer_id.has_value()) {
    return std::nullopt;
  }

  return V4L2BufferRefFactory::CreateWritableRef(
      buffers_[buffer_id.value()]->v4l2_buffer(),
      weak_this_factory_.GetWeakPtr());
}

std::optional<V4L2WritableBufferRef> V4L2Queue::GetFreeBufferForFrame(
    const gfx::GenericSharedMemoryId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // No buffers allocated at the moment?
  if (!free_buffers_) {
    return std::nullopt;
  }

  if (memory_ != V4L2_MEMORY_DMABUF) {
    DVLOGF(1) << "Queue is not DMABUF";
    return std::nullopt;
  }

  if (!id.is_valid()) {
    DVLOGF(1) << "Provided identifier was not valid";
    return std::nullopt;
  }

  // If |id| has already been used in |buffers_|, then return that buffer.
  // Otherwise use the next buffer from |free_buffers_indexes_|.
  if (!base::Contains(free_buffers_indexes_, id)) {
    if (free_buffers_indexes_.size() >= buffers_.size()) {
      return std::nullopt;
    }
    // The value for |id| is simply the map size(): a poor man's way to have a
    // monotonically increasing counter.
    free_buffers_indexes_.emplace(id, free_buffers_indexes_.size());
  }
  return GetFreeBuffer(free_buffers_indexes_[id]);
}

bool V4L2Queue::QueueBuffer(struct v4l2_buffer* v4l2_buffer,
                            scoped_refptr<FrameResource> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  V4L2ProcessingTrace(v4l2_buffer, /*start=*/true);

  int ret = ioctl_cb_.Run(VIDIOC_QBUF, v4l2_buffer);
  if (ret) {
    RecordVidiocIoctlErrorUMA(VidiocIoctlRequests::kVidiocQbuf);
    VPQLOGF(1) << "VIDIOC_QBUF failed";
    return false;
  }

  const auto inserted =
      queued_buffers_.emplace(v4l2_buffer->index, std::move(frame));
  DCHECK(inserted.second);

  schedule_poll_cb_.Run();

  return true;
}

std::pair<bool, V4L2ReadableBufferRef> V4L2Queue::DequeueBuffer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // No need to dequeue if no buffers queued.
  if (QueuedBuffersCount() == 0) {
    return std::make_pair(true, nullptr);
  }

  if (!IsStreaming()) {
    VQLOGF(1) << "Attempting to dequeue a buffer while not streaming.";
    return std::make_pair(true, nullptr);
  }

  struct v4l2_buffer v4l2_buffer;
  memset(&v4l2_buffer, 0, sizeof(v4l2_buffer));
  // WARNING: do not change this to a vector or something smaller than
  // VIDEO_MAX_PLANES (the maximum number of planes V4L2 supports). The
  // element overhead is small and may avoid memory corruption bugs.
  struct v4l2_plane planes[VIDEO_MAX_PLANES];
  memset(planes, 0, sizeof(planes));
  v4l2_buffer.type = type_;
  v4l2_buffer.memory = memory_;
  v4l2_buffer.m.planes = planes;
  v4l2_buffer.length = planes_count_;
  int ret = ioctl_cb_.Run(VIDIOC_DQBUF, &v4l2_buffer);
  if (ret) {
    // TODO(acourbot): we should not have to check for EPIPE as codec clients
    // should not call this method after the last buffer is dequeued.
    switch (errno) {
      case EAGAIN:
      case EPIPE:
        // This is not an error so we'll need to continue polling but won't
        // provide a buffer.
        schedule_poll_cb_.Run();
        return std::make_pair(true, nullptr);
      default:
        RecordVidiocIoctlErrorUMA(VidiocIoctlRequests::kVidiocDqbuf);
        VPQLOGF(1) << "VIDIOC_DQBUF failed";
        return std::make_pair(false, nullptr);
    }
  }

  auto it = queued_buffers_.find(v4l2_buffer.index);
  CHECK(it != queued_buffers_.end(), base::NotFatalUntil::M130);
  scoped_refptr<FrameResource> queued_frame = std::move(it->second);
  queued_buffers_.erase(it);

  V4L2ProcessingTrace(&v4l2_buffer, /*start=*/false);

  if (QueuedBuffersCount() > 0) {
    schedule_poll_cb_.Run();
  }

  // See if we need to remove this from any of the secure buffer queue tracking.
  for (auto& buf : secure_buffers_) {
    std::erase_if(buf.queued_buffer_indexes, [v4l2_buffer](size_t idx) {
      return idx == v4l2_buffer.index;
    });
  }

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

  if (is_streaming_) {
    return true;
  }

  int arg = static_cast<int>(type_);
  int ret = ioctl_cb_.Run(VIDIOC_STREAMON, &arg);
  if (ret) {
    RecordVidiocIoctlErrorUMA(VidiocIoctlRequests::kVidiocStreamon);
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
  int ret = ioctl_cb_.Run(VIDIOC_STREAMOFF, &arg);
  if (ret) {
    RecordVidiocIoctlErrorUMA(VidiocIoctlRequests::kVidiocStreamoff);
    VPQLOGF(1) << "VIDIOC_STREAMOFF failed";
    return false;
  }

  for (const auto& it : queued_buffers_) {
    DCHECK(free_buffers_);
    free_buffers_->ReturnBuffer(it.first);
  }

  for (auto& buf : secure_buffers_) {
    buf.queued_buffer_indexes.clear();
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

std::optional<struct v4l2_format> V4L2Queue::SetModifierFormat(
    uint64_t modifier,
    const gfx::Size& size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (DRM_FORMAT_MOD_QCOM_COMPRESSED == modifier) {
    auto format = SetFormat(V4L2_PIX_FMT_QC08C, size, 0);

    if (!format) {
      VPLOGF(1) << "Failed to set magic modifier format.";
    }
    return format;
  }
  return std::nullopt;
}

bool V4L2Queue::SendStopCommand() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return SendCommand(V4L2_DEC_CMD_STOP);
}

bool V4L2Queue::SendStartCommand() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return SendCommand(V4L2_DEC_CMD_START);
}

bool V4L2Queue::SetBufferFdForSecureHandle(uint64_t secure_handle,
                                           struct v4l2_buffer* v4l2_buffer) {
  for (auto& buf : secure_buffers_) {
    if (buf.secure_handle == secure_handle) {
      if (!buf.owned_by_decoder_buffer) {
        return false;
      }
      buf.queued_buffer_indexes.emplace_back(v4l2_buffer->index);
      v4l2_buffer->m.planes[0].m.fd = buf.fd.get();
      return true;
    }
  }
  return false;
}

bool V4L2Queue::SendCommand(__u32 command) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(mcasas): Restrict this to V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, after
  // deprecating V4L2StatefulVideoDecoderBackend.

  struct v4l2_decoder_cmd cmd;
  memset(&cmd, 0, sizeof(cmd));  // Must use memset() due to unions.
  cmd.cmd = command;
  const bool success = ioctl_cb_.Run(VIDIOC_DECODER_CMD, &cmd) == kIoctlOk;
  PLOG_IF(ERROR, !success) << "Failed to issue command " << command
                           << " (V4L2_DEC_CMD_START: " << V4L2_DEC_CMD_START
                           << ", V4L2_DEC_CMD_STOP: " << V4L2_DEC_CMD_STOP
                           << ")";
  return success;
}

void V4L2Queue::SecureBufferAllocated(base::ScopedFD secure_fd,
                                      uint64_t secure_handle) {
  CHECK(secure_fd.is_valid());
  CHECK(secure_handle);
  secure_buffers_.emplace_back(secure_handle, std::move(secure_fd));
}

class V4L2Request {
 public:
  V4L2Request(const V4L2Request&) = delete;
  V4L2Request& operator=(const V4L2Request&) = delete;

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
  raw_ptr<V4L2RequestsQueue> request_queue_;
  int ref_counter_ = 0;
  base::ScopedFD request_fd_;

  friend class V4L2RequestsQueue;
  V4L2Request(base::ScopedFD&& request_fd, V4L2RequestsQueue* request_queue)
      : request_queue_(request_queue), request_fd_(std::move(request_fd)) {}

  friend class V4L2RequestRefBase;
  // Increases the number of request references.
  void IncRefCounter();
  // Decreases the number of request references.
  // When the counters reaches zero, the request is returned to the queue.
  int DecRefCounter();

  SEQUENCE_CHECKER(sequence_checker_);
};

void V4L2Request::IncRefCounter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ref_counter_++;
}

int V4L2Request::DecRefCounter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ref_counter_--;

  if (ref_counter_ < 1) {
    request_queue_->ReturnRequest(this);
  }

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

  if (HANDLE_EINTR(ioctl(request_fd_.get(), MEDIA_REQUEST_IOC_QUEUE)) != 0) {
    RecordMediaIoctlUMA(MediaIoctlRequests::kMediaRequestIocQueue);
    return false;
  }

  return true;
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
    RecordMediaIoctlUMA(MediaIoctlRequests::kMediaRequestIocReinit);
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

  if (request_) {
    request_->DecRefCounter();
  }
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

std::optional<V4L2SubmittedRequestRef> V4L2RequestRef::Submit() && {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(request_, nullptr);

  V4L2RequestRef self(std::move(*this));

  if (!self.request_->Submit()) {
    return std::nullopt;
  }

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

std::optional<base::ScopedFD> V4L2RequestsQueue::CreateRequestFD() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int request_fd;
  int ret = HANDLE_EINTR(
      ioctl(media_fd_.get(), MEDIA_IOC_REQUEST_ALLOC, &request_fd));
  if (ret < 0) {
    RecordMediaIoctlUMA(MediaIoctlRequests::kMediaIocRequestAlloc);
    VPLOGF(1) << "Failed to create request";
    return std::nullopt;
  }

  return base::ScopedFD(request_fd);
}

std::optional<V4L2RequestRef> V4L2RequestsQueue::GetFreeRequest() {
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
      return std::nullopt;
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
      return std::nullopt;
    }
    free_requests_.pop();
  }

  DCHECK(request_ptr);
  if (!request_ptr->Reset()) {
    VPLOGF(1) << "Failed to reset request";
    return std::nullopt;
  }

  return V4L2RequestRef(request_ptr);
}

void V4L2RequestsQueue::ReturnRequest(V4L2Request* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(request);

  if (request) {
    free_requests_.push(request);
  }
}

}  //  namespace media
