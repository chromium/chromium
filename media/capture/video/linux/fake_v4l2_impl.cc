// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/fake_v4l2_impl.h"

#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#include <bit>
#include <queue>
#include <vector>

#include "base/bits.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "media/base/test_data_util.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"

#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + (c))
#endif

namespace media {

constexpr char kMjpegFrameFile[] = "one_frame_1280x720.mjpeg";
constexpr unsigned int kMjpegFrameWidth = 1280;
constexpr unsigned int kMjpegFrameHeight = 720;

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

namespace {

int Error(int error_code) {
  errno = error_code;
  return kErrorReturnValue;
}

__u32 RoundUpToMultipleOfPageSize(__u32 size) {
  CHECK(std::has_single_bit(base::checked_cast<__u32>(getpagesize())));
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

VideoPixelFormat V4L2FourccToPixelFormat(uint32_t fourcc) {
  switch (fourcc) {
    case V4L2_PIX_FMT_YUV420:
      return PIXEL_FORMAT_I420;
    case V4L2_PIX_FMT_NV12:
      return PIXEL_FORMAT_NV12;
    case V4L2_PIX_FMT_Y16:
      return PIXEL_FORMAT_Y16;
    case V4L2_PIX_FMT_Z16:
      return PIXEL_FORMAT_Y16;
    case V4L2_PIX_FMT_YUYV:
      return PIXEL_FORMAT_YUY2;
    case V4L2_PIX_FMT_RGB24:
      return PIXEL_FORMAT_RGB24;
    case V4L2_PIX_FMT_MJPEG:
      return PIXEL_FORMAT_MJPEG;
    default:
      return PIXEL_FORMAT_I420;
  }
}

size_t GetV4L2FrameSize(uint32_t pixelformat,
                        unsigned int width,
                        unsigned int height) {
  auto size = VideoFrame::AllocationSize(V4L2FourccToPixelFormat(pixelformat),
                                         gfx::Size(width, width));
  if (pixelformat == V4L2_PIX_FMT_MJPEG) {
    DCHECK_EQ(width, kMjpegFrameWidth);
    DCHECK_EQ(height, kMjpegFrameHeight);
    auto file_path = media::GetTestDataFilePath(kMjpegFrameFile);
    if (!file_path.empty()) {
      FILE* fp = fopen(file_path.value().c_str(), "rb");
      if (fp) {
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);
        fclose(fp);
      }
    }
  }
  return size;
}

}  // namespace

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
    selected_format_.pixelformat = config_.v4l2_pixel_format;
    if (config_.v4l2_pixel_format == V4L2_PIX_FMT_MJPEG) {
      selected_format_.width = kMjpegFrameWidth;
      selected_format_.height = kMjpegFrameHeight;
    }
    selected_format_.field = V4L2_FIELD_NONE;
    selected_format_.bytesperline = kDefaultWidth;
    selected_format_.sizeimage =
        GetV4L2FrameSize(selected_format_.pixelformat, selected_format_.width,
                         selected_format_.height);
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

  void EnqueueEvents() {
    for (uint32_t control_id : control_event_subscriptions_) {
      pending_events_.emplace();
      v4l2_event& event = pending_events_.back();
      event.type = V4L2_EVENT_CTRL;
      event.id = control_id;
    }
  }

  bool HasPendingEvents() const { return !pending_events_.empty(); }

  int enum_fmt(v4l2_fmtdesc* fmtdesc) {
    if (fmtdesc->index > 0u) {
      // We only support a single format for now.
      return Error(EINVAL);
    }
    if (fmtdesc->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
      // We only support video capture.
      return Error(EINVAL);
    }
    fmtdesc->flags = 0u;
    strcpy(reinterpret_cast<char*>(fmtdesc->description),
           FourccToString(config_.v4l2_pixel_format).c_str());
    fmtdesc->pixelformat = config_.v4l2_pixel_format;
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

  int g_ctrl(v4l2_control* control) { return Error(EINVAL); }

  int s_ctrl(v4l2_control* control) { return kSuccessReturnValue; }

  int s_ext_ctrls(v4l2_ext_controls* control) { return kSuccessReturnValue; }

  int queryctrl(v4l2_queryctrl* control) {
    switch (control->id) {
      case V4L2_CID_PAN_ABSOLUTE:
        if (!config_.descriptor.control_support().pan)
          return Error(EINVAL);
        break;
      case V4L2_CID_TILT_ABSOLUTE:
        if (!config_.descriptor.control_support().tilt)
          return Error(EINVAL);
        break;
      case V4L2_CID_ZOOM_ABSOLUTE:
        if (!config_.descriptor.control_support().zoom)
          return Error(EINVAL);
        break;
      default:
        return Error(EINVAL);
    }
    control->flags = 0;
    control->minimum = 100;
    control->maximum = 400;
    control->step = 1;
    return kSuccessReturnValue;
  }

  int s_fmt(v4l2_format* format) {
    if (format->type != V4L2_BUF_TYPE_VIDEO_CAPTURE ||
        format->fmt.pix.width > kMaxWidth ||
        format->fmt.pix.height > kMaxHeight) {
      return Error(EINVAL);
    }
    v4l2_pix_format& pix_format = format->fmt.pix;
    // We only support YUV420 output for now. Tell this to the client by
    // overwriting whatever format it requested.
    pix_format.pixelformat = config_.v4l2_pixel_format;
    // We only support non-interlaced output
    pix_format.field = V4L2_FIELD_NONE;
    // We do not support padding bytes
    pix_format.bytesperline = pix_format.width;
    pix_format.sizeimage = GetV4L2FrameSize(
        config_.v4l2_pixel_format, pix_format.width, pix_format.height);
    // Arbitrary colorspace
    pix_format.colorspace = V4L2_COLORSPACE_REC709;
    pix_format.priv = 0;
    selected_format_ = pix_format;
    return kSuccessReturnValue;
  }

  int g_parm(v4l2_streamparm* parm) {
    if (parm->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
      return Error(EINVAL);
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
      return Error(EINVAL);
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
      return Error(EINVAL);
    if (bufs->memory != V4L2_MEMORY_MMAP) {
      // We only support device-owned buffers
      return Error(EINVAL);
    }
    incoming_queue_ = std::queue<raw_ptr<FakeV4L2Buffer, CtnExperimental>>();
    outgoing_queue_ = std::queue<raw_ptr<FakeV4L2Buffer, CtnExperimental>>();
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
      return Error(EINVAL);
    if (buf->index >= device_buffers_.size())
      return Error(EINVAL);
    auto& buffer = device_buffers_[buf->index];
    buf->memory = V4L2_MEMORY_MMAP;
    buf->flags = buffer.flags;
    buf->m.offset = buffer.offset;
    buf->length = buffer.length;
    return kSuccessReturnValue;
  }

  int qbuf(v4l2_buffer* buf) {
    if (buf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
      return Error(EINVAL);
    if (buf->memory != V4L2_MEMORY_MMAP)
      return Error(EINVAL);
    if (buf->index >= device_buffers_.size())
      return Error(EINVAL);
    auto& buffer = device_buffers_[buf->index];
    buffer.flags = V4L2_BUF_FLAG_MAPPED & V4L2_BUF_FLAG_QUEUED;
    buf->flags = buffer.flags;

    base::AutoLock lock(incoming_queue_lock_);
    incoming_queue_.push(&buffer);
    return kSuccessReturnValue;
  }

  int dqbuf(v4l2_buffer* buf) {
    if (buf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
      return Error(EINVAL);
    if (buf->memory != V4L2_MEMORY_MMAP)
      return Error(EINVAL);
    bool outgoing_queue_is_empty = true;
    {
      base::AutoLock lock(outgoing_queue_lock_);
      outgoing_queue_is_empty = outgoing_queue_.empty();
    }
    if (outgoing_queue_is_empty) {
      if (open_flags_ & O_NONBLOCK)
        return Error(EAGAIN);
      wait_for_outgoing_queue_event_.Wait();
    }
    base::AutoLock lock(outgoing_queue_lock_);
    auto* buffer = outgoing_queue_.front().get();
    outgoing_queue_.pop();
    buffer->flags = V4L2_BUF_FLAG_MAPPED & V4L2_BUF_FLAG_DONE;
    buf->index = buffer->index;
    buf->bytesused =
        GetV4L2FrameSize(selected_format_.pixelformat, selected_format_.width,
                         selected_format_.height);
    if (selected_format_.pixelformat == V4L2_PIX_FMT_MJPEG) {
      DCHECK_EQ(selected_format_.width, kMjpegFrameWidth);
      DCHECK_EQ(selected_format_.height, kMjpegFrameHeight);
      auto file_path = media::GetTestDataFilePath(kMjpegFrameFile);
      if (!file_path.empty()) {
        FILE* fp = fopen(file_path.value().c_str(), "rb");
        if (fp) {
          fseek(fp, 0, SEEK_END);
          long len = ftell(fp);
          if (len <= static_cast<long>(buffer->length)) {
            fseek(fp, 0, SEEK_SET);
            auto read_size = fread(buffer->data.get(), 1, len, fp);
            buf->bytesused = read_size;
          }

          fclose(fp);
        }
      }
    }

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
      return Error(EINVAL);
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
      return Error(EINVAL);
    should_quit_frame_production_loop_.Set();
    frame_production_thread_.Stop();
    incoming_queue_ = std::queue<raw_ptr<FakeV4L2Buffer, CtnExperimental>>();
    outgoing_queue_ = std::queue<raw_ptr<FakeV4L2Buffer, CtnExperimental>>();
    return kSuccessReturnValue;
  }

  int enum_framesizes(v4l2_frmsizeenum* frame_size) {
    if (frame_size->index > 0)
      return kErrorReturnValue;

    frame_size->type = V4L2_FRMSIZE_TYPE_DISCRETE;
    frame_size->discrete.width = kDefaultWidth;
    frame_size->discrete.height = kDefaultHeight;
    return kSuccessReturnValue;
  }

  int enum_frameintervals(v4l2_frmivalenum* frame_interval) {
    if (frame_interval->index > 0 || frame_interval->width != kDefaultWidth ||
        frame_interval->height != kDefaultWidth) {
      return kErrorReturnValue;
    }

    frame_interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
    frame_interval->discrete.numerator = kDefaultFrameInternvalNumerator;
    frame_interval->discrete.denominator = kDefaultFrameInternvalDenominator;
    return kSuccessReturnValue;
  }

  int dqevent(v4l2_event* event) {
    if (pending_events_.empty()) {
      return Error(EINVAL);
    }
    *event = pending_events_.front();
    pending_events_.pop();
    event->pending = pending_events_.size();
    return kSuccessReturnValue;
  }

  int subscribe_event(v4l2_event_subscription* event_subscription) {
    if (event_subscription->type != V4L2_EVENT_CTRL) {
      NOTIMPLEMENTED();
      return Error(EINVAL);
    }
    EXPECT_NE(event_subscription->id, 0u);
    control_event_subscriptions_.insert(event_subscription->id);
    return kSuccessReturnValue;
  }

  int unsubscribe_event(v4l2_event_subscription* event_subscription) {
    if (event_subscription->type != V4L2_EVENT_CTRL) {
      NOTIMPLEMENTED();
      return Error(EINVAL);
    }
    if (event_subscription->id == 0) {
      control_event_subscriptions_.clear();
    } else {
      control_event_subscriptions_.erase(event_subscription->id);
    }
    return kSuccessReturnValue;
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

    auto* buffer = incoming_queue_.front().get();
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
  base::flat_set<uint32_t> control_event_subscriptions_;
  std::vector<FakeV4L2Buffer> device_buffers_;
  std::queue<raw_ptr<FakeV4L2Buffer, CtnExperimental>> incoming_queue_;
  std::queue<raw_ptr<FakeV4L2Buffer, CtnExperimental>> outgoing_queue_;
  std::queue<v4l2_event> pending_events_;
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
    return Error(EBADF);
  auto* opened_device = device_iter->second.get();

  switch (static_cast<uint32_t>(request)) {
    case VIDIOC_ENUM_FMT:
      return opened_device->enum_fmt(reinterpret_cast<v4l2_fmtdesc*>(argp));
    case VIDIOC_QUERYCAP:
      return opened_device->querycap(reinterpret_cast<v4l2_capability*>(argp));
    case VIDIOC_G_CTRL:
      return opened_device->g_ctrl(reinterpret_cast<v4l2_control*>(argp));
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
    case VIDIOC_DQEVENT:
      return opened_device->dqevent(reinterpret_cast<v4l2_event*>(argp));
    case VIDIOC_SUBSCRIBE_EVENT:
      return opened_device->subscribe_event(
          reinterpret_cast<v4l2_event_subscription*>(argp));
    case VIDIOC_UNSUBSCRIBE_EVENT:
      return opened_device->unsubscribe_event(
          reinterpret_cast<v4l2_event_subscription*>(argp));

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
    case VIDIOC_S_DV_TIMINGS:
    case VIDIOC_G_DV_TIMINGS:
    case VIDIOC_CREATE_BUFS:
    case VIDIOC_PREPARE_BUF:
    case VIDIOC_G_SELECTION:
    case VIDIOC_S_SELECTION:
    case VIDIOC_DECODER_CMD:
    case VIDIOC_TRY_DECODER_CMD:
    case VIDIOC_ENUM_DV_TIMINGS:
    case VIDIOC_QUERY_DV_TIMINGS:
    case VIDIOC_DV_TIMINGS_CAP:
    case VIDIOC_ENUM_FREQ_BANDS:
      // Unsupported |request| code.
      LOG(ERROR) << "Unsupported request code " << request;
      return kErrorReturnValue;
  }

  // Invalid |request|.
  NOTREACHED();
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
    return Error(EINVAL);
  }
  pollfd& ufd = ufds[0];
  auto device_iter = opened_devices_.find(ufd.fd);
  if (device_iter == opened_devices_.end()) {
    return Error(EBADF);
  }
  auto* opened_device = device_iter->second.get();
  if (!(ufd.events & POLLIN)) {
    // We only support waiting for data to become readable.
    return Error(EINVAL);
  }
  if (!opened_device->BlockUntilOutputQueueHasBuffer(timeout)) {
    return 0;
  }
  ufd.revents |= POLLIN;
  if (ufd.events & POLLPRI) {
    if (opened_device->HasPendingEvents()) {
      ufd.revents |= POLLPRI;
    } else {
      // For the next poll.
      opened_device->EnqueueEvents();
    }
  }
  return 1;
}

}  // namespace media
