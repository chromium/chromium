// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/fake_v4l2_impl.h"

#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#include <queue>

#include "base/bind.h"
#include "base/bits.h"
#include "base/ranges/algorithm.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "media/base/video_frame.h"

#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + (c))
#endif

namespace media {

static const int kInvalidId = -1;
static const int kSuccessReturnValue = 0;
static const int kErrorReturnValue = -1;
static const uint32_t kMaxBufferCount = 5;
static const int kDefaultWidth = 640;
static const int kDefaultHeight = 480;
static const unsigned int kMaxWidth = 3840;
static const unsigned int kMaxHeight = 2160;

// 20 fps.
static const int kDefaultFrameInternvalNumerator = 50;
static const int kDefaultFrameInternvalDenominator = 1000;

__u32 RoundUpToMultipleOfPageSize(__u32 size) {
  CHECK(base::bits::IsPowerOfTwo(getpagesize()));
  return base::bits::AlignUp(size, base::checked_cast<__u32>(getpagesize()));
}

struct FakeV4L2Buffer {
  FakeV4L2Buffer(__u32 index, __u32 offset, __u32 length)
      : index(index),
        offset(offset),
        length(length),
        flags(V4L2_BUF_FLAG_MAPPED),
        sequence(0) {}

  const __u32 index;
  const __u32 offset;
  const __u32 length;
  __u32 flags;
  timeval timestamp;
  __u32 sequence;
  std::unique_ptr<uint8_t[]> data;
};

class FakeV4L2Impl::OpenedDevice {
 public:
  explicit OpenedDevice(const FakeV4L2DeviceConfig& config, int open_flags)
      : config_(config),
        open_flags_(open_flags),
        wait_for_outgoing_queue_event_(
            base::WaitableEvent::ResetPolicy::AUTOMATIC),
        frame_production_thread_("FakeV4L2Impl FakeProductionThread") {
    selected_format_.width = kDefaultWidth;
    selected_format_.height = kDefaultHeight;
    selected_format_.pixelformat = V4L2_PIX_FMT_YUV420;
    selected_format_.field = V4L2_FIELD_NONE;
    selected_format_.bytesperline = kDefaultWidth;
    selected_format_.sizeimage = VideoFrame::AllocationSize(
        PIXEL_FORMAT_I420, gfx::Size(kDefaultWidth, kDefaultHeight));
    selected_format_.colorspace = V4L2_COLORSPACE_REC709;
    selected_format_.priv = 0;

    timeperframe_.numerator = kDefaultFrameInternvalNumerator;
    timeperframe_.denominator = kDefaultFrameInternvalDenominator;
  }

  ~OpenedDevice() { DCHECK(!frame_production_thread_.IsRunning()); }

  const std::string& device_id() const { return config_.descriptor.device_id; }

  int open_flags() const { return open_flags_; }

  FakeV4L2Buffer* LookupBufferFromOffset(off_t offset) {
    auto buffer_iter =
        base::ranges::find(device_buffers_, offset, &FakeV4L2Buffer::offset);
    if (buffer_iter == device_buffers_.end())
      return nullptr;
    return &(*buffer_iter);
  }

  bool BlockUntilOutputQueueHasBuffer(int timeout_in_milliseconds) {
    {
      base::AutoLock lock(outgoing_queue_lock_);
      if (!outgoing_queue_.empty()) {
        return true;
      }
    }
    return wait_for_outgoing_queue_event_.TimedWait(
        base::Milliseconds(timeout_in_milliseconds));
  }

  int enum_fmt(v4l2_fmtdesc* fmtdesc) {
    if (fmtdesc->index > 0u) {
      // We only support a single format for now.
      return EINVAL;
    }
    if (fmtdesc->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
      // We only support video capture.
      return EINVAL;
    }
    fmtdesc->flags = 0u;
    strcpy(reinterpret_cast<char*>(fmtdesc->description), "YUV420");
    fmtdesc->pixelformat = V4L2_PIX_FMT_YUV420;
    memset(fmtdesc->reserved, 0, sizeof(fmtdesc->reserved));
    return kSuccessReturnValue;
  }

  int querycap(v4l2_capability* cap) {
    strcpy(reinterpret_cast<char*>(cap->driver), "FakeV4L2");
    CHECK(config_.descriptor.display_name().size() < 31);
    strcpy(reinterpret_cast<char*>(cap->driver),
           config_.descriptor.display_name().c_str());
    cap->bus_info[0] = 0;
    // Provide arbitrary version info
    cap->version = KERNEL_VERSION(1, 0, 0);
    cap->capabilities = V4L2_CAP_VIDEO_CAPTURE;
    memset(cap->reserved, 0, sizeof(cap->reserved));
    return kSuccessReturnValue;
  }

  int s_ctrl(v4l2_control* control) { return kSuccessReturnValue; }

  int s_ext_ctrls(v4l2_ext_controls* control) { return kSuccessReturnValue; }

  int queryctrl(v4l2_queryctrl* control) {
    switch (control->id) {
      case V4L2_CID_PAN_ABSOLUTE:
        if (!config_.descriptor.control_support().pan)
          return EINVAL;
        break;
      case V4L2_CID_TILT_ABSOLUTE:
        if (!config_.descriptor.control_support().tilt)
          return EINVAL;
        break;
      case V4L2_CID_ZOOM_ABSOLUTE:
        if (!config_.descriptor.control_support().zoom)
          return EINVAL;
        break;
      default:
        return EINVAL;
    }
    control->flags = 0;
    control->minimum = 100;
    control->maximum = 400;
    control->step = 1;
    return 0;
  }

  int s_fmt(v4l2_format* format) {
    if (format->type != V4L2_BUF_TYPE_VIDEO_CAPTURE ||
        format->fmt.pix.width > kMaxWidth ||
        format->fmt.pix.height > kMaxHeight) {
      return EINVAL;
    }
    v4l2_pix_format& pix_format = format->fmt.pix;
    // We only support YUV420 output for now. Tell this to the client by
    // overwriting whatever format it requested.
    pix_format.pixelformat = V4L2_PIX_FMT_YUV420;
    // We only support non-interlaced output
    pix_format.field = V4L2_FIELD_NONE;
    // We do not support padding bytes
    pix_format.bytesperline = pix_format.width;
    pix_format.sizeimage = VideoFrame::AllocationSize(
        PIXEL_FORMAT_I420, gfx::Size(pix_format.width, pix_format.height));
    // Arbitrary colorspace
    pix_format.colorspace = V4L2_COLORSPACE_REC709;
    pix_format.priv = 0;
    selected_format_ = pix_format;
    return kSuccessReturnValue;
  }

  int g_parm(v4l2_streamparm* parm) {
    if (parm->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
      return EINVAL;
    v4l2_captureparm& captureparm = parm->parm.capture;
    captureparm.capability = V4L2_CAP_TIMEPERFRAME;
    captureparm.timeperframe = timeperframe_;
    captureparm.extendedmode = 0;
    captureparm.readbuffers = 3;  // arbitrary choice
    memset(captureparm.reserved, 0, sizeof(captureparm.reserved));
    return kSuccessReturnValue;
  }

  int s_parm(v4l2_streamparm* parm) {
    if (parm->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
      return EINVAL;
    v4l2_captureparm& captureparm = parm->parm.capture;
    captureparm.capability = V4L2_CAP_TIMEPERFRAME;
    timeperframe_ = captureparm.timeperframe;
    captureparm.extendedmode = 0;
    captureparm.readbuffers = 3;  // arbitrary choice
    memset(captureparm.reserved, 0, sizeof(captureparm.reserved));
    return kSuccessReturnValue;
  }

  int reqbufs(v4l2_requestbuffers* bufs) {
    if (bufs->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
      return EINVAL;
    if (bufs->memory != V4L2_MEMORY_MMAP) {
      // We only support device-owned buffers
      return EINVAL;
    }
    incoming_queue_ = std::queue<FakeV4L2Buffer*>();
    outgoing_queue_ = std::queue<FakeV4L2Buffer*>();
    device_buffers_.clear();
    uint32_t target_buffer_count = std::min(bufs->count, kMaxBufferCount);
    bufs->count = target_buffer_count;
    __u32 current_offset = 0;
    for (uint32_t i = 0; i < target_buffer_count; i++) {
      device_buffers_.emplace_back(i, current_offset,
                                   selected_format_.sizeimage);
      current_offset += RoundUpToMultipleOfPageSize(selected_format_.sizeimage);
    }
    memset(bufs->reserved, 0, sizeof(bufs->reserved));
    return kSuccessReturnValue;
  }

  int querybuf(v4l2_buffer* buf) {
    if (buf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
      return EINVAL;
    if (buf->index >= device_buffers_.size())
      return EINVAL;
    auto& buffer = device_buffers_[buf->index];
    buf->memory = V4L2_MEMORY_MMAP;
    buf->flags = buffer.flags;
    buf->m.offset = buffer.offset;
    buf->length = buffer.length;
    return kSuccessReturnValue;
  }

  int qbuf(v4l2_buffer* buf) {
    if (buf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
      return EINVAL;
    if (buf->memory != V4L2_MEMORY_MMAP)
      return EINVAL;
    if (buf->index >= device_buffers_.size())
      return EINVAL;
    auto& buffer = device_buffers_[buf->index];
    buffer.flags = V4L2_BUF_FLAG_MAPPED & V4L2_BUF_FLAG_QUEUED;
    buf->flags = buffer.flags;

    base::AutoLock lock(incoming_queue_lock_);
    incoming_queue_.push(&buffer);
    return kSuccessReturnValue;
  }

  int dqbuf(v4l2_buffer* buf) {
    if (buf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
      return EINVAL;
    if (buf->memory != V4L2_MEMORY_MMAP)
      return EINVAL;
    bool outgoing_queue_is_empty = true;
    {
      base::AutoLock lock(outgoing_queue_lock_);
      outgoing_queue_is_empty = outgoing_queue_.empty();
    }
    if (outgoing_queue_is_empty) {
      if (open_flags_ & O_NONBLOCK)
        return EAGAIN;
      wait_for_outgoing_queue_event_.Wait();
    }
    base::AutoLock lock(outgoing_queue_lock_);
    auto* buffer = outgoing_queue_.front();
    outgoing_queue_.pop();
    buffer->flags = V4L2_BUF_FLAG_MAPPED & V4L2_BUF_FLAG_DONE;
    buf->index = buffer->index;
    buf->bytesused = VideoFrame::AllocationSize(
        PIXEL_FORMAT_I420,
        gfx::Size(selected_format_.width, selected_format_.height));
    buf->flags = buffer->flags;
    buf->field = V4L2_FIELD_NONE;
    buf->timestamp = buffer->timestamp;
    buf->sequence = buffer->sequence;
    buf->m.offset = buffer->offset;
    buf->length = buffer->length;

    return kSuccessReturnValue;
  }

  int streamon(const int* type) {
    if (*type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
      return EINVAL;
    frame_production_thread_.Start();
    should_quit_frame_production_loop_.UnsafeResetForTesting();
    frame_production_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeV4L2Impl::OpenedDevice::RunFrameProductionLoop,
                       base::Unretained(this)));
    return kSuccessReturnValue;
  }

  int streamoff(const int* type) {
    if (*type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
      return EINVAL;
    should_quit_frame_production_loop_.Set();
    frame_production_thread_.Stop();
    incoming_queue_ = std::queue<FakeV4L2Buffer*>();
    outgoing_queue_ = std::queue<FakeV4L2Buffer*>();
    return kSuccessReturnValue;
  }

  int enum_framesizes(v4l2_frmsizeenum* frame_size) {
    if (frame_size->index > 0)
      return -1;

    frame_size->type = V4L2_FRMSIZE_TYPE_DISCRETE;
    frame_size->discrete.width = kDefaultWidth;
    frame_size->discrete.height = kDefaultHeight;
    return 0;
  }

  int enum_frameintervals(v4l2_frmivalenum* frame_interval) {
    if (frame_interval->index > 0 || frame_interval->width != kDefaultWidth ||
        frame_interval->height != kDefaultWidth) {
      return -1;
    }

    frame_interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
    frame_interval->discrete.numerator = kDefaultFrameInternvalNumerator;
    frame_interval->discrete.denominator = kDefaultFrameInternvalDenominator;
    return 0;
  }

 private:
  void RunFrameProductionLoop() {
    while (!should_quit_frame_production_loop_.IsSet()) {
      MaybeProduceOneFrame();
      // Sleep for a bit.
      // We ignore the requested frame rate here, and just sleep for a fixed
      // duration.
      base::PlatformThread::Sleep(base::Milliseconds(100));
    }
  }

  void MaybeProduceOneFrame() {
    // We do not actually produce any frame data. Just move a buffer from
    // incoming queue to outgoing queue.
    base::AutoLock lock(incoming_queue_lock_);
    base::AutoLock lock2(outgoing_queue_lock_);
    if (incoming_queue_.empty()) {
      return;
    }

    auto* buffer = incoming_queue_.front();
    gettimeofday(&buffer->timestamp, NULL);
    static __u32 frame_counter = 0;
    buffer->sequence = frame_counter++;
    incoming_queue_.pop();
    outgoing_queue_.push(buffer);
    wait_for_outgoing_queue_event_.Signal();
  }

  const FakeV4L2DeviceConfig config_;
  const int open_flags_;
  v4l2_pix_format selected_format_;
  v4l2_fract timeperframe_;
  std::vector<FakeV4L2Buffer> device_buffers_;
  std::queue<FakeV4L2Buffer*> incoming_queue_;
  std::queue<FakeV4L2Buffer*> outgoing_queue_;
  base::WaitableEvent wait_for_outgoing_queue_event_;
  base::Thread frame_production_thread_;
  base::AtomicFlag should_quit_frame_production_loop_;
  base::Lock incoming_queue_lock_;
  base::Lock outgoing_queue_lock_;
};

FakeV4L2Impl::FakeV4L2Impl() : next_id_to_return_from_open_(1) {}

FakeV4L2Impl::~FakeV4L2Impl() = default;

void FakeV4L2Impl::AddDevice(const std::string& device_name,
                             const FakeV4L2DeviceConfig& config) {
  base::AutoLock lock(lock_);
  device_configs_.emplace(device_name, config);
}

int FakeV4L2Impl::open(const char* device_name, int flags) {
  if (!device_name)
    return kInvalidId;

  base::AutoLock lock(lock_);

  std::string device_name_as_string(device_name);
  auto device_configs_iter = device_configs_.find(device_name_as_string);
  if (device_configs_iter == device_configs_.end())
    return kInvalidId;

  auto id_iter = device_name_to_open_id_map_.find(device_name_as_string);
  if (id_iter != device_name_to_open_id_map_.end()) {
    // Device is already open
    return kInvalidId;
  }

  auto device_id = next_id_to_return_from_open_++;
  device_name_to_open_id_map_.emplace(device_name_as_string, device_id);
  opened_devices_.emplace(device_id, std::make_unique<OpenedDevice>(
                                         device_configs_iter->second, flags));
  return device_id;
}

int FakeV4L2Impl::close(int fd) {
  base::AutoLock lock(lock_);
  auto device_iter = opened_devices_.find(fd);
  if (device_iter == opened_devices_.end())
    return kErrorReturnValue;
  device_name_to_open_id_map_.erase(device_iter->second->device_id());
  opened_devices_.erase(device_iter->first);
  return kSuccessReturnValue;
}

int FakeV4L2Impl::ioctl(int fd, int request, void* argp) {
  base::AutoLock lock(lock_);
  auto device_iter = opened_devices_.find(fd);
  if (device_iter == opened_devices_.end())
    return EBADF;
  auto* opened_device = device_iter->second.get();

  switch (static_cast<uint32_t>(request)) {
    case VIDIOC_ENUM_FMT:
      return opened_device->enum_fmt(reinterpret_cast<v4l2_fmtdesc*>(argp));
    case VIDIOC_QUERYCAP:
      return opened_device->querycap(reinterpret_cast<v4l2_capability*>(argp));
    case VIDIOC_S_CTRL:
      return opened_device->s_ctrl(reinterpret_cast<v4l2_control*>(argp));
    case VIDIOC_S_EXT_CTRLS:
      return opened_device->s_ext_ctrls(
          reinterpret_cast<v4l2_ext_controls*>(argp));
    case VIDIOC_QUERYCTRL:
      return opened_device->queryctrl(reinterpret_cast<v4l2_queryctrl*>(argp));
    case VIDIOC_S_FMT:
      return opened_device->s_fmt(reinterpret_cast<v4l2_format*>(argp));
    case VIDIOC_G_PARM:
      return opened_device->g_parm(reinterpret_cast<v4l2_streamparm*>(argp));
    case VIDIOC_S_PARM:
      return opened_device->s_parm(reinterpret_cast<v4l2_streamparm*>(argp));
    case VIDIOC_REQBUFS:
      return opened_device->reqbufs(
          reinterpret_cast<v4l2_requestbuffers*>(argp));
    case VIDIOC_QUERYBUF:
      return opened_device->querybuf(reinterpret_cast<v4l2_buffer*>(argp));
    case VIDIOC_QBUF:
      return opened_device->qbuf(reinterpret_cast<v4l2_buffer*>(argp));
    case VIDIOC_DQBUF:
      return opened_device->dqbuf(reinterpret_cast<v4l2_buffer*>(argp));
    case VIDIOC_STREAMON:
      return opened_device->streamon(reinterpret_cast<int*>(argp));
    case VIDIOC_STREAMOFF:
      return opened_device->streamoff(reinterpret_cast<int*>(argp));
    case VIDIOC_ENUM_FRAMESIZES:
      return opened_device->enum_framesizes(
          reinterpret_cast<v4l2_frmsizeenum*>(argp));
    case VIDIOC_ENUM_FRAMEINTERVALS:
      return opened_device->enum_frameintervals(
          reinterpret_cast<v4l2_frmivalenum*>(argp));

    case VIDIOC_CROPCAP:
    case VIDIOC_DBG_G_REGISTER:
    case VIDIOC_DBG_S_REGISTER:
    case VIDIOC_ENCODER_CMD:
    case VIDIOC_TRY_ENCODER_CMD:
    case VIDIOC_ENUMAUDIO:
    case VIDIOC_ENUMAUDOUT:
    case VIDIOC_ENUMINPUT:
    case VIDIOC_ENUMOUTPUT:
    case VIDIOC_ENUMSTD:
    case VIDIOC_G_AUDIO:
    case VIDIOC_S_AUDIO:
    case VIDIOC_G_AUDOUT:
    case VIDIOC_S_AUDOUT:
    case VIDIOC_G_CROP:
    case VIDIOC_S_CROP:
    case VIDIOC_G_CTRL:
    case VIDIOC_G_ENC_INDEX:
    case VIDIOC_G_EXT_CTRLS:
    case VIDIOC_TRY_EXT_CTRLS:
    case VIDIOC_G_FBUF:
    case VIDIOC_S_FBUF:
    case VIDIOC_G_FMT:
    case VIDIOC_TRY_FMT:
    case VIDIOC_G_FREQUENCY:
    case VIDIOC_S_FREQUENCY:
    case VIDIOC_G_INPUT:
    case VIDIOC_S_INPUT:
    case VIDIOC_G_JPEGCOMP:
    case VIDIOC_S_JPEGCOMP:
    case VIDIOC_G_MODULATOR:
    case VIDIOC_S_MODULATOR:
    case VIDIOC_G_OUTPUT:
    case VIDIOC_S_OUTPUT:
    case VIDIOC_G_PRIORITY:
    case VIDIOC_S_PRIORITY:
    case VIDIOC_G_SLICED_VBI_CAP:
    case VIDIOC_G_STD:
    case VIDIOC_S_STD:
    case VIDIOC_G_TUNER:
    case VIDIOC_S_TUNER:
    case VIDIOC_LOG_STATUS:
    case VIDIOC_OVERLAY:
    case VIDIOC_QUERYMENU:
    case VIDIOC_QUERYSTD:
    case VIDIOC_S_HW_FREQ_SEEK:
      // Unsupported |request| code.
      NOTREACHED() << "Unsupported request code " << request;
      return kErrorReturnValue;
  }

  // Invalid |request|.
  NOTREACHED();
  return kErrorReturnValue;
}

// We ignore |start| in this implementation
void* FakeV4L2Impl::mmap(void* /*start*/,
                         size_t length,
                         int prot,
                         int flags,
                         int fd,
                         off_t offset) {
  base::AutoLock lock(lock_);
  if (flags & MAP_FIXED) {
    errno = EINVAL;
    return MAP_FAILED;
  }
  if (prot != (PROT_READ | PROT_WRITE)) {
    errno = EINVAL;
    return MAP_FAILED;
  }
  auto device_iter = opened_devices_.find(fd);
  if (device_iter == opened_devices_.end()) {
    errno = EBADF;
    return MAP_FAILED;
  }
  auto* opened_device = device_iter->second.get();
  auto* buffer = opened_device->LookupBufferFromOffset(offset);
  if (!buffer || (buffer->length != length)) {
    errno = EINVAL;
    return MAP_FAILED;
  }
  if (!buffer->data)
    buffer->data = std::make_unique<uint8_t[]>(length);
  return buffer->data.get();
}

int FakeV4L2Impl::munmap(void* start, size_t length) {
  base::AutoLock lock(lock_);
  return kSuccessReturnValue;
}

int FakeV4L2Impl::poll(struct pollfd* ufds, unsigned int nfds, int timeout) {
  base::AutoLock lock(lock_);
  if (nfds != 1) {
    // We only support polling of a single device.
    errno = EINVAL;
    return kErrorReturnValue;
  }
  pollfd& ufd = ufds[0];
  auto device_iter = opened_devices_.find(ufd.fd);
  if (device_iter == opened_devices_.end()) {
    errno = EBADF;
    return kErrorReturnValue;
  }
  auto* opened_device = device_iter->second.get();
  if (ufd.events != POLLIN) {
    // We only support waiting for data to become readable.
    errno = EINVAL;
    return kErrorReturnValue;
  }
  if (!opened_device->BlockUntilOutputQueueHasBuffer(timeout)) {
    return 0;
  }
  ufd.revents |= POLLIN;
  return 1;
}

}  // namespace media
