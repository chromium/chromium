// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_jpeg_encode_accelerator.h"

#include <errno.h>
#include <linux/videodev2.h>
#include <string.h>
#include <sys/mman.h>

#include <algorithm>
#include <array>
#include <memory>
#include <tuple>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notimplemented.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/ozone/public/ozone_platform.h"

#define IOCTL_OR_ERROR_RETURN_VALUE(type, arg, value, type_name) \
  do {                                                           \
    if (device_->Ioctl(type, arg) != 0) {                        \
      VPLOGF(1) << "ioctl() failed: " << type_name;              \
      NotifyError(kInvalidBitstreamBufferId, PLATFORM_FAILURE);  \
      return value;                                              \
    }                                                            \
  } while (0)

#define IOCTL_OR_ERROR_RETURN(type, arg) \
  IOCTL_OR_ERROR_RETURN_VALUE(type, arg, ((void)0), #type)

#define IOCTL_OR_ERROR_RETURN_FALSE(type, arg) \
  IOCTL_OR_ERROR_RETURN_VALUE(type, arg, false, #type)

#define IOCTL_OR_LOG_ERROR(type, arg)                           \
  do {                                                          \
    if (device_->Ioctl(type, arg) != 0) {                       \
      VPLOGF(1) << "ioctl() failed: " << #type;                 \
      NotifyError(kInvalidBitstreamBufferId, PLATFORM_FAILURE); \
    }                                                           \
  } while (0)

namespace media {

V4L2JpegEncodeAccelerator::JobRecord::JobRecord(
    scoped_refptr<VideoFrame> input_frame,
    scoped_refptr<VideoFrame> output_frame,
    int quality,
    int32_t task_id,
    base::WritableSharedMemoryMapping exif_mapping)
    : input_frame(input_frame),
      output_frame(output_frame),
      quality(quality),
      task_id(task_id),
      exif_mapping(std::move(exif_mapping)) {}

V4L2JpegEncodeAccelerator::JobRecord::JobRecord(
    scoped_refptr<VideoFrame> input_frame,
    int quality,
    int32_t task_id,
    base::WritableSharedMemoryMapping exif_mapping,
    base::WritableSharedMemoryMapping output_mapping)
    : input_frame(input_frame),
      quality(quality),
      task_id(task_id),
      output_mapping(std::move(output_mapping)),
      exif_mapping(std::move(exif_mapping)) {}

V4L2JpegEncodeAccelerator::JobRecord::~JobRecord() {}

V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::EncodedInstanceDmaBuf(
    V4L2JpegEncodeAccelerator* parent)
    : parent_(parent),
      input_streamon_(false),
      output_streamon_(false),
      input_buffer_pixelformat_(0),
      input_buffer_num_planes_(0),
      output_buffer_pixelformat_(0) {}

V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::~EncodedInstanceDmaBuf() {}

void V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::DestroyTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_->encoder_sequence_);
  while (!input_job_queue_.empty())
    input_job_queue_.pop();
  while (!running_job_queue_.empty())
    running_job_queue_.pop_front();

  DestroyInputBuffers();
  DestroyOutputBuffers();
}

bool V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_->encoder_sequence_);
  device_ = base::MakeRefCounted<V4L2Device>();
  client_native_pixmap_factory_ = ui::CreateClientNativePixmapFactoryOzone();
  output_buffer_pixelformat_ = V4L2_PIX_FMT_JPEG;
  if (!device_->Open(V4L2Device::Type::kJpegEncoder,
                     output_buffer_pixelformat_)) {
    VLOGF(1) << "Failed to open device";
    return false;
  }

  // Capabilities check.
  struct v4l2_capability caps = {};
  const __u32 kCapsRequired = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_M2M_MPLANE;
  if (device_->Ioctl(VIDIOC_QUERYCAP, &caps) != 0) {
    VPLOGF(1) << "ioctl() failed: VIDIOC_QUERYCAP";
    return false;
  }
  if ((caps.capabilities & kCapsRequired) != kCapsRequired) {
    VLOGF(1) << "VIDIOC_QUERYCAP, caps check failed: 0x" << std::hex
             << caps.capabilities;
    return false;
  }

  return true;
}

bool V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::SetUpJpegParameters(
    int quality,
    gfx::Size coded_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_->encoder_sequence_);

  struct v4l2_ext_controls ctrls = {};
  struct v4l2_ext_control ctrl = {};
  struct v4l2_query_ext_ctrl queryctrl = {};

  ctrls.which = V4L2_CTRL_WHICH_CUR_VAL;
  ctrls.count = 0;
  const bool use_modern_s_ext_ctrls =
      device_->Ioctl(VIDIOC_S_EXT_CTRLS, &ctrls) == 0;

  ctrls.which =
      use_modern_s_ext_ctrls ? V4L2_CTRL_WHICH_CUR_VAL : V4L2_CTRL_CLASS_JPEG;
  ctrls.controls = &ctrl;
  ctrls.count = 1;

  switch (output_buffer_pixelformat_) {
    case V4L2_PIX_FMT_JPEG:
      queryctrl.id = V4L2_CID_JPEG_COMPRESSION_QUALITY;
      queryctrl.type = V4L2_CTRL_TYPE_INTEGER;
      IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QUERY_EXT_CTRL, &queryctrl);

      // interpolate the quality value
      // Map quality value from range 1-100 to min-max.
      quality = queryctrl.minimum +
                (quality - 1) * (queryctrl.maximum - queryctrl.minimum) / 99;
      ctrl.id = V4L2_CID_JPEG_COMPRESSION_QUALITY;
      ctrl.value = quality;
      VLOG(1) << "JPEG Quality: max:" << queryctrl.maximum
              << ", min:" << queryctrl.minimum << ", value:" << quality;
      IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_EXT_CTRLS, &ctrls);

      queryctrl.id = V4L2_CID_JPEG_ACTIVE_MARKER;
      queryctrl.type = V4L2_CTRL_TYPE_BITMASK;
      // Driver may not have implemented V4L2_CID_JPEG_ACTIVE_MARKER.
      // Ignore any error and assume the driver implements the JPEG stream
      // the way we want it.
      std::ignore = device_->Ioctl(VIDIOC_QUERY_EXT_CTRL, &queryctrl);

      // Ask for JPEG markers we want. Since not all may be implemented,
      // ask for the common subset of what we want and what is supported.
      ctrl.id = V4L2_CID_JPEG_ACTIVE_MARKER;
      ctrl.value = queryctrl.maximum &
                   (V4L2_JPEG_ACTIVE_MARKER_APP0 | V4L2_JPEG_ACTIVE_MARKER_DQT |
                    V4L2_JPEG_ACTIVE_MARKER_DHT);
      std::ignore = device_->Ioctl(VIDIOC_S_EXT_CTRLS, &ctrls);
      break;

    default:
      NOTREACHED();
  }

  return true;
}

size_t
V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::InputBufferQueuedCount() {
  return kBufferCount - free_input_buffers_.size();
}

size_t
V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::OutputBufferQueuedCount() {
  return kBufferCount - free_output_buffers_.size();
}

bool V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::CreateBuffers(
    gfx::Size coded_size,
    const VideoFrameLayout& input_layout,
    size_t output_buffer_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_->encoder_sequence_);

  // The order of set output/input formats matters.
  // rk3399 reset input format when we set output format.
  if (!SetOutputBufferFormat(coded_size, output_buffer_size)) {
    return false;
  }

  if (!SetInputBufferFormat(coded_size, input_layout)) {
    return false;
  }

  if (!RequestInputBuffers()) {
    return false;
  }

  if (!RequestOutputBuffers()) {
    return false;
  }

  return true;
}

bool V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::SetInputBufferFormat(
    gfx::Size coded_size,
    const VideoFrameLayout& input_layout) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_->encoder_sequence_);
  DCHECK(!input_streamon_);
  DCHECK(input_job_queue_.empty());

  constexpr uint32_t input_pix_fmt_candidates[] = {V4L2_PIX_FMT_NV12M,
                                                   V4L2_PIX_FMT_NV12};

  struct v4l2_format format = {};
  input_buffer_pixelformat_ = 0;
  for (const auto input_pix_fmt : input_pix_fmt_candidates) {
    DCHECK_EQ(Fourcc::FromV4L2PixFmt(input_pix_fmt)->ToVideoPixelFormat(),
              PIXEL_FORMAT_NV12);
    format = {};
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    format.fmt.pix_mp.num_planes = kMaxNV12Plane;
    format.fmt.pix_mp.pixelformat = input_pix_fmt;
    format.fmt.pix_mp.field = V4L2_FIELD_ANY;
    // set the input buffer resolution with padding and use selection API to
    // crop the coded size.
    format.fmt.pix_mp.width = input_layout.planes()[0].stride;
    format.fmt.pix_mp.height = coded_size.height();

    for (size_t i = 0; i < input_layout.num_planes(); i++) {
      UNSAFE_TODO(format.fmt.pix_mp.plane_fmt[i].sizeimage =
                      input_layout.planes()[i].size);
      UNSAFE_TODO(format.fmt.pix_mp.plane_fmt[i].bytesperline =
                      input_layout.planes()[i].stride);
    }

    if (device_->Ioctl(VIDIOC_S_FMT, &format) == 0 &&
        format.fmt.pix_mp.pixelformat == input_pix_fmt) {
      device_input_layout_ = V4L2FormatToVideoFrameLayout(format);

      // Save V4L2 returned values.
      input_buffer_pixelformat_ = format.fmt.pix_mp.pixelformat;
      input_buffer_num_planes_ = format.fmt.pix_mp.num_planes;
      break;
    }
  }

  if (input_buffer_pixelformat_ == 0) {
    VLOGF(1) << "Neither NV12 nor NV12M is supported.";
    return false;
  }

  // It can't allow different width.
  if (format.fmt.pix_mp.width !=
      static_cast<uint32_t>(input_layout.planes()[0].stride)) {
    LOG(WARNING) << "Different stride:" << format.fmt.pix_mp.width
                 << "!=" << input_layout.planes()[0].stride;
    return false;
  }

  // We can allow our buffer to have larger height than encoder's requirement
  // because we set the 2nd plane by data_offset now.
  if (format.fmt.pix_mp.height > static_cast<uint32_t>(coded_size.height())) {
    if (input_buffer_pixelformat_ == V4L2_PIX_FMT_NV12M) {
      // Calculate the real buffer height of the DMA buffer from minigbm.
      uint32_t height_with_padding =
          input_layout.planes()[0].size / input_layout.planes()[0].stride;
      if (format.fmt.pix_mp.height > height_with_padding) {
        LOG(WARNING) << "Encoder requires larger height:"
                     << format.fmt.pix_mp.height << ">" << height_with_padding;
        return false;
      }
    } else {
      LOG(WARNING) << "Encoder requires larger height:"
                   << format.fmt.pix_mp.height << ">" << coded_size.height();
      return false;
    }
  }

  if ((uint32_t)coded_size.width() != format.fmt.pix_mp.width ||
      (uint32_t)coded_size.height() != format.fmt.pix_mp.height) {
    v4l2_selection selection = {};
    selection.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    selection.target = V4L2_SEL_TGT_CROP;
    selection.flags = V4L2_SEL_FLAG_GE | V4L2_SEL_FLAG_LE;
    selection.r.left = 0;
    selection.r.top = 0;
    selection.r.width = coded_size.width();
    selection.r.height = coded_size.height();
    if (device_->Ioctl(VIDIOC_S_SELECTION, &selection) != 0) {
      LOG(WARNING) << "VIDIOC_S_SELECTION Fail";
      return false;
    }
  }

  return true;
}

bool V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::SetOutputBufferFormat(
    gfx::Size coded_size,
    size_t buffer_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_->encoder_sequence_);
  DCHECK(!output_streamon_);
  DCHECK(running_job_queue_.empty());

  struct v4l2_format format = {};
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  format.fmt.pix_mp.num_planes = kMaxJpegPlane;
  format.fmt.pix_mp.pixelformat = output_buffer_pixelformat_;
  format.fmt.pix_mp.field = V4L2_FIELD_ANY;
  format.fmt.pix_mp.plane_fmt[0].sizeimage = buffer_size;
  format.fmt.pix_mp.width = coded_size.width();
  format.fmt.pix_mp.height = coded_size.height();
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_FMT, &format);
  DCHECK_EQ(format.fmt.pix_mp.pixelformat, output_buffer_pixelformat_);

  return true;
}

bool V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::RequestInputBuffers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_->encoder_sequence_);
  struct v4l2_format format = {};
  format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  format.fmt.pix_mp.pixelformat = input_buffer_pixelformat_;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_G_FMT, &format);

  struct v4l2_requestbuffers reqbufs = {};
  reqbufs.count = kBufferCount;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbufs.memory = V4L2_MEMORY_DMABUF;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_REQBUFS, &reqbufs);

  DCHECK(free_input_buffers_.empty());
  for (size_t i = 0; i < reqbufs.count; ++i) {
    free_input_buffers_.push_back(i);
  }

  return true;
}

bool V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::RequestOutputBuffers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_->encoder_sequence_);
  struct v4l2_requestbuffers reqbufs = {};
  reqbufs.count = kBufferCount;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_DMABUF;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_REQBUFS, &reqbufs);

  DCHECK(free_output_buffers_.empty());
  for (size_t i = 0; i < reqbufs.count; ++i) {
    free_output_buffers_.push_back(i);
  }

  return true;
}

void V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::DestroyInputBuffers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_->encoder_sequence_);
  free_input_buffers_.clear();

  if (input_streamon_) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMOFF, &type);
    input_streamon_ = false;
  }

  struct v4l2_requestbuffers reqbufs = {};
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbufs.memory = V4L2_MEMORY_DMABUF;
  IOCTL_OR_LOG_ERROR(VIDIOC_REQBUFS, &reqbufs);

  input_buffer_num_planes_ = 0;
}

void V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::DestroyOutputBuffers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_->encoder_sequence_);
  free_output_buffers_.clear();

  if (output_streamon_) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMOFF, &type);
    output_streamon_ = false;
  }

  struct v4l2_requestbuffers reqbufs = {};
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_DMABUF;
  IOCTL_OR_LOG_ERROR(VIDIOC_REQBUFS, &reqbufs);
}

void V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::ServiceDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_->encoder_sequence_);

  if (!running_job_queue_.empty()) {
    Dequeue();
  }

  EnqueueInput();
  EnqueueOutput();

  DVLOGF(3) << "buffer counts: INPUT[" << input_job_queue_.size()
            << "] => DEVICE[" << free_input_buffers_.size() << "/"
            << "->" << free_output_buffers_.size() << "]";
}

void V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::EnqueueInput() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_->encoder_sequence_);
  while (!input_job_queue_.empty() && !free_input_buffers_.empty()) {
    if (!EnqueueInputRecord())
      return;
  }

  if (!input_streamon_ && InputBufferQueuedCount()) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMON, &type);
    input_streamon_ = true;
  }
}

void V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::EnqueueOutput() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_->encoder_sequence_);
  while (running_job_queue_.size() > OutputBufferQueuedCount() &&
         !free_output_buffers_.empty()) {
    if (!EnqueueOutputRecord())
      return;
  }

  if (!output_streamon_ && OutputBufferQueuedCount()) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMON, &type);
    output_streamon_ = true;
  }
}

bool V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::EnqueueInputRecord() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_->encoder_sequence_);
  DCHECK(!input_job_queue_.empty());
  DCHECK(!free_input_buffers_.empty());

  // Enqueue an input (VIDEO_OUTPUT) buffer for an input video frame.
  std::unique_ptr<JobRecord> job_record = std::move(input_job_queue_.front());
  input_job_queue_.pop();
  const int index = free_input_buffers_.back();

  struct v4l2_buffer qbuf = {};
  struct v4l2_plane planes[kMaxNV12Plane] = {};
  qbuf.index = index;
  qbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  qbuf.memory = V4L2_MEMORY_DMABUF;
  qbuf.length = std::size(planes);
  qbuf.m.planes = planes;

  const auto& frame = job_record->input_frame;
  const auto num_fds = frame->NumDmabufFds();
  DCHECK(num_fds > 0);
  for (size_t i = 0; i < input_buffer_num_planes_; i++) {
    if (device_input_layout_->is_multi_planar()) {
      UNSAFE_TODO(qbuf.m.planes[i].bytesused = base::checked_cast<__u32>(
                      VideoFrame::PlaneSize(frame->format(), i,
                                            device_input_layout_->coded_size())
                          .GetArea()));
    } else {
      UNSAFE_TODO(qbuf.m.planes[i].bytesused = VideoFrame::AllocationSize(
                      frame->format(), device_input_layout_->coded_size()));
    }

    // If there are fewer FD's than planes, then re-use the last FD for the
    // additional planes.
    const size_t dmabuf_index = std::min<size_t>(i, num_fds - 1);
    const auto& layout_planes = frame->layout().planes();
    UNSAFE_TODO(qbuf.m.planes[i].m.fd = frame->GetDmabufFd(dmabuf_index));
    UNSAFE_TODO(qbuf.m.planes[i].data_offset = layout_planes[i].offset);
    UNSAFE_TODO(qbuf.m.planes[i].bytesused += qbuf.m.planes[i].data_offset);
    UNSAFE_TODO(qbuf.m.planes[i].length =
                    layout_planes[i].size + qbuf.m.planes[i].data_offset);
  }

  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QBUF, &qbuf);
  running_job_queue_.push_back(std::move(job_record));
  free_input_buffers_.pop_back();
  return true;
}

bool V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::EnqueueOutputRecord() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_->encoder_sequence_);
  DCHECK(!free_output_buffers_.empty());

  // Enqueue an output (VIDEO_CAPTURE) buffer.
  const int index = free_output_buffers_.back();
  struct v4l2_buffer qbuf = {};
  struct v4l2_plane planes[kMaxJpegPlane] = {};
  qbuf.index = index;
  qbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  qbuf.memory = V4L2_MEMORY_DMABUF;
  qbuf.length = std::size(planes);
  qbuf.m.planes = planes;

  auto& job_record = running_job_queue_[OutputBufferQueuedCount()];
  for (size_t i = 0; i < qbuf.length; i++) {
    UNSAFE_TODO(planes[i].m.fd = job_record->output_frame->GetDmabufFd(i));
  }
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QBUF, &qbuf);
  free_output_buffers_.pop_back();
  return true;
}

size_t V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::FinalizeJpegImage(
    scoped_refptr<VideoFrame> output_frame,
    size_t buffer_size,
    size_t max_buffer_capacity,
    base::WritableSharedMemoryMapping exif_mapping) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_->encoder_sequence_);

  if (buffer_size > max_buffer_capacity) {
    LOG(ERROR) << "buffer_size (" << buffer_size
               << ") exceeds max_buffer_capacity (" << max_buffer_capacity
               << ")";
    return 0;
  }

  size_t idx = 0;

  auto output_gmb_handle = CreateGpuMemoryBufferHandle(output_frame.get());
  DCHECK(!output_gmb_handle.is_null());

  // In this case, we use the R_8 buffer with height == 1 to represent a data
  // container. As a result, we use plane.stride as size of the data here since
  // plane.size might be larger due to height alignment.
  const gfx::Size native_pixmap_size(
      base::checked_cast<int32_t>(output_frame->layout().planes()[0].stride),
      1);

  std::unique_ptr<gfx::ClientNativePixmap> native_pixmap =
      client_native_pixmap_factory_->ImportFromHandle(
          std::move(output_gmb_handle).native_pixmap_handle(),
          native_pixmap_size, viz::SinglePlaneFormat::kR_8,
          gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE);
  if (!native_pixmap) {
    VLOGF(1) << "Failed to import native pixmap";
    return 0;
  }

  bool isMapped = native_pixmap->Map();
  if (!isMapped) {
    VLOGF(1) << "Failed to map native pixmap";
    return 0;
  }

  // Fill SOI and EXIF markers.
  static const uint8_t kJpegStart[] = {0xFF, JPEG_SOI};

  if (exif_mapping.IsValid()) {
    uint8_t* exif_buffer = exif_mapping.GetMemoryAs<uint8_t>();
    size_t exif_buffer_size = exif_mapping.size();
    // Application Segment for Exif data.
    uint16_t exif_segment_size = static_cast<uint16_t>(exif_buffer_size + 2);
    const uint8_t kAppSegment[] = {
        0xFF, JPEG_APP1, static_cast<uint8_t>(exif_segment_size / 256),
        static_cast<uint8_t>(exif_segment_size % 256)};

    // V4L2_PIX_FMT_JPEG refers to a valid JPEG bitstream. It does not
    // imply a standard JFIF bitstream with JFIF-APP0 markers.
    // Move data after SOI to make room for APP1 marker and EXIF data.
    // If an APP0 marker is found directly after the SOI marker, skip
    // over it.
    // The JPEG from V4L2_PIX_FMT_JPEG is
    // SOI-marker1-marker2-...-SOS-compressed stream-EOI
    // |......| <- src_data_offset = len(SOI) + len(APP0) (if APP0 found)
    // |...................| <- data_offset = len(SOI) + len(APP1)
    if (buffer_size < sizeof(kJpegStart)) {
      LOG(WARNING) << "JPEG buffer is too small";
      return 0;
    }

    // SAFETY: GetMemoryAddress(0) returns a pointer to a mapped region of at
    // least max_buffer_capacity, which is verified in Dequeue() to be bounded
    // by the buffer length and the mapped size.
    auto dst_span = UNSAFE_BUFFERS(base::span<uint8_t>(
        static_cast<uint8_t*>(native_pixmap->GetMemoryAddress(0)),
        max_buffer_capacity));
    uint8_t* dst_ptr = dst_span.data();

    size_t data_offset =
        sizeof(kJpegStart) + sizeof(kAppSegment) + exif_buffer_size;
    size_t src_data_offset = sizeof(kJpegStart);

    // Avoid parsing headers directly in memory shared with another process.
    // Copy the first few bytes to a local buffer to avoid TOCTOU.
    std::array<uint8_t, 6> header = {};
    const size_t copy_size = std::min(buffer_size, header.size());
    base::span(header).first(copy_size).copy_from(dst_span.first(copy_size));

    // Check for APP0 segment following SOI marker and skip over it if found
    if (copy_size >= header.size() && header[2] == JPEG_MARKER_PREFIX &&
        header[3] == JPEG_APP0) {
      src_data_offset += 2 + ((header[4] << 8) | header[5]);
      if (src_data_offset >= buffer_size) {
        LOG(WARNING) << "APP0 segment from encoder extends beyond JPEG buffer";
        return 0;
      }
    }
    buffer_size -= src_data_offset;
    if (buffer_size + data_offset > max_buffer_capacity) {
      LOG(WARNING) << "JPEG buffer is too small for the EXIF metadata";
      return 0;
    }
    UNSAFE_TODO(
        memmove(dst_ptr + data_offset, dst_ptr + src_data_offset, buffer_size));

    UNSAFE_TODO(memcpy(dst_ptr, kJpegStart, sizeof(kJpegStart)));
    idx += sizeof(kJpegStart);
    UNSAFE_TODO(memcpy(dst_ptr + idx, kAppSegment, sizeof(kAppSegment)));
    idx += sizeof(kAppSegment);
    UNSAFE_TODO(memcpy(dst_ptr + idx, exif_buffer, exif_buffer_size));
    idx += exif_buffer_size;
  }

  switch (output_buffer_pixelformat_) {
    case V4L2_PIX_FMT_JPEG:
      idx += buffer_size;
      break;

    default:
      NOTREACHED() << "Unsupported output pixel format";
  }

  native_pixmap->Unmap();

  return idx;
}

void V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::Dequeue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_->encoder_sequence_);
  // Dequeue completed input (VIDEO_OUTPUT) buffers,
  // and recycle to the free list.
  while (InputBufferQueuedCount() > 0) {
    DCHECK(input_streamon_);
    struct v4l2_buffer dqbuf = {};
    struct v4l2_plane planes[kMaxNV12Plane] = {};
    dqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    dqbuf.memory = V4L2_MEMORY_DMABUF;
    dqbuf.length = std::size(planes);
    dqbuf.m.planes = planes;
    if (device_->Ioctl(VIDIOC_DQBUF, &dqbuf) != 0) {
      if (errno == EAGAIN) {
        // EAGAIN if we're just out of buffers to dequeue.
        break;
      }
      VPLOGF(1) << "ioctl() failed: input buffer VIDIOC_DQBUF failed.";
      NotifyError(kInvalidBitstreamBufferId, PLATFORM_FAILURE);
      return;
    }
    free_input_buffers_.push_back(dqbuf.index);

    if (dqbuf.flags & V4L2_BUF_FLAG_ERROR) {
      VLOGF(1) << "Error in dequeued input buffer.";
      NotifyError(kInvalidBitstreamBufferId, PARSE_IMAGE_FAILED);
      running_job_queue_.pop_front();
    }
  }

  // Dequeue completed output (VIDEO_CAPTURE) buffers, recycle to the free list.
  // Return the finished buffer to the client via the job ready callback.
  // If dequeued input buffer has an error, the error frame has removed from
  // |running_job_queue_|. We only have to dequeue output buffer when we
  // actually have pending frames in |running_job_queue_| and also enqueued
  // output buffers.
  while (!running_job_queue_.empty() && OutputBufferQueuedCount() > 0) {
    DCHECK(output_streamon_);
    struct v4l2_buffer dqbuf = {};
    struct v4l2_plane planes[kMaxJpegPlane] = {};
    dqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    dqbuf.memory = V4L2_MEMORY_DMABUF;
    dqbuf.length = std::size(planes);
    dqbuf.m.planes = planes;
    if (device_->Ioctl(VIDIOC_DQBUF, &dqbuf) != 0) {
      if (errno == EAGAIN) {
        // EAGAIN if we're just out of buffers to dequeue.
        break;
      }
      VPLOGF(1) << "ioctl() failed: output buffer VIDIOC_DQBUF failed.";
      NotifyError(kInvalidBitstreamBufferId, PLATFORM_FAILURE);
      return;
    }
    free_output_buffers_.push_back(dqbuf.index);

    // Jobs are always processed in FIFO order.
    std::unique_ptr<JobRecord> job_record =
        std::move(running_job_queue_.front());
    running_job_queue_.pop_front();

    if (dqbuf.flags & V4L2_BUF_FLAG_ERROR) {
      VLOGF(1) << "Error in dequeued output buffer.";
      NotifyError(kInvalidBitstreamBufferId, PARSE_IMAGE_FAILED);
      return;
    }

    const size_t buffer_size = planes[0].bytesused;
    // SECURITY: planes[0].length is the kernel-reported *dmabuf* size, but
    // FinalizeJpegImage() only mmap()s output_frame->layout().planes()[0].size
    // bytes (the caller-supplied plane.size). Bounds-check against the smaller
    // of the two so we never write past the mapped region.
    const size_t mapped_size =
        job_record->output_frame->layout().planes()[0].size;
    const size_t max_buffer_capacity =
        std::min(static_cast<size_t>(planes[0].length), mapped_size);
    const size_t jpeg_size = FinalizeJpegImage(
        job_record->output_frame, buffer_size, max_buffer_capacity,
        std::move(job_record->exif_mapping));

    if (!jpeg_size) {
      NotifyError(job_record->task_id, PLATFORM_FAILURE);
      return;
    }
    DVLOGF(4) << "Encoding finished, returning bitstream buffer, id="
              << job_record->task_id;

    parent_->VideoFrameReady(job_record->task_id, jpeg_size);
  }
}

void V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::NotifyError(
    int32_t task_id,
    Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_->encoder_sequence_);
  parent_->NotifyError(task_id, status);
}

V4L2JpegEncodeAccelerator::V4L2JpegEncodeAccelerator(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : io_task_runner_(io_task_runner),
      client_(nullptr),
      weak_factory_for_encoder_(this),
      weak_factory_(this) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DETACH_FROM_SEQUENCE(encoder_sequence_);
  weak_ptr_ = weak_factory_.GetWeakPtr();
  weak_ptr_for_encoder_ = weak_factory_for_encoder_.GetWeakPtr();
}

V4L2JpegEncodeAccelerator::~V4L2JpegEncodeAccelerator() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (encoder_task_runner_) {
    base::WaitableEvent waiter;
    // base::Unretained(this) is safe because we wait DestroyTask() is done.
    encoder_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&V4L2JpegEncodeAccelerator::DestroyTask,
                                  base::Unretained(this), &waiter));
    waiter.Wait();
  }
  weak_factory_.InvalidateWeakPtrs();
}

void V4L2JpegEncodeAccelerator::DestroyTask(base::WaitableEvent* waiter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_);

  while (!encoded_instances_dma_buf_.empty()) {
    encoded_instances_dma_buf_.front()->DestroyTask();
    encoded_instances_dma_buf_.pop();
  }

  weak_factory_for_encoder_.InvalidateWeakPtrs();
  waiter->Signal();
}

void V4L2JpegEncodeAccelerator::VideoFrameReady(int32_t task_id,
                                                size_t encoded_picture_size) {
  if (!io_task_runner_->BelongsToCurrentThread()) {
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&V4L2JpegEncodeAccelerator::VideoFrameReady,
                                  weak_ptr_, task_id, encoded_picture_size));
    return;
  }
  VLOGF(1) << "Encoding finished task id=" << task_id
           << " Compressed size:" << encoded_picture_size;
  client_->VideoFrameReady(task_id, encoded_picture_size);
}

void V4L2JpegEncodeAccelerator::NotifyError(int32_t task_id, Status status) {
  if (!io_task_runner_->BelongsToCurrentThread()) {
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&V4L2JpegEncodeAccelerator::NotifyError,
                                  weak_ptr_, task_id, status));

    return;
  }
  VLOGF(1) << "Notifying of error " << status << " for task id " << task_id;
  client_->NotifyError(task_id, status);
}

void V4L2JpegEncodeAccelerator::InitializeTask(
    chromeos_camera::JpegEncodeAccelerator::Client* client,
    chromeos_camera::JpegEncodeAccelerator::InitCB init_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_);

  auto encoded_device = std::make_unique<EncodedInstanceDmaBuf>(this);
  // We just check if we can initialize device here.
  if (!encoded_device->Initialize()) {
    VLOGF(1) << "Failed to initialize device";
    std::move(init_cb).Run(HW_JPEG_ENCODE_NOT_SUPPORTED);
    return;
  }

  VLOGF(2) << "V4L2JpegEncodeAccelerator initialized.";
  std::move(init_cb).Run(ENCODE_OK);
}

void V4L2JpegEncodeAccelerator::InitializeAsync(
    chromeos_camera::JpegEncodeAccelerator::Client* client,
    chromeos_camera::JpegEncodeAccelerator::InitCB init_cb) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  client_ = client;

  // base::WithBaseSyncPrimitives() and base::MayBlock() are necessary to
  // synchronously destroy encoder variables on |encoder_task_runner_| in
  // dedestructor.
  encoder_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT, base::WithBaseSyncPrimitives(),
       base::MayBlock()});
  DCHECK(encoder_task_runner_);

  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2JpegEncodeAccelerator::InitializeTask,
                     weak_ptr_for_encoder_, client,
                     base::BindPostTaskToCurrentDefault(std::move(init_cb))));
}

size_t V4L2JpegEncodeAccelerator::GetMaxCodedBufferSize(
    const gfx::Size& picture_size) {
  return picture_size.GetArea() * 3 / 2 + kJpegDefaultHeaderSize;
}

void V4L2JpegEncodeAccelerator::Encode(
    scoped_refptr<media::VideoFrame> video_frame,
    int quality,
    BitstreamBuffer* exif_buffer,
    BitstreamBuffer output_buffer) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void V4L2JpegEncodeAccelerator::EncodeWithDmaBuf(
    scoped_refptr<VideoFrame> input_frame,
    scoped_refptr<VideoFrame> output_frame,
    int quality,
    int32_t task_id,
    BitstreamBuffer* exif_buffer) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (quality <= 0 || quality > 100) {
    VLOGF(1) << "quality is not in range. " << quality;
    NotifyError(task_id, INVALID_ARGUMENT);
    return;
  }

  if (input_frame->format() != VideoPixelFormat::PIXEL_FORMAT_NV12) {
    VLOGF(1) << "Format is not NV12";
    NotifyError(task_id, INVALID_ARGUMENT);
    return;
  }

  base::WritableSharedMemoryMapping exif_mapping;
  if (exif_buffer) {
    VLOGF(4) << "EXIF size " << exif_buffer->size();
    if (exif_buffer->size() > kMaxMarkerSizeAllowed) {
      NotifyError(task_id, INVALID_ARGUMENT);
      return;
    }

    base::UnsafeSharedMemoryRegion exif_region = exif_buffer->TakeRegion();
    exif_mapping =
        exif_region.MapAt(exif_buffer->offset(), exif_buffer->size());
    if (!exif_mapping.IsValid()) {
      VPLOGF(1) << "could not map exif bitstream_buffer";
      NotifyError(task_id, PLATFORM_FAILURE);
      return;
    }
  }

  std::unique_ptr<JobRecord> job_record(new JobRecord(
      input_frame, output_frame, quality, task_id, std::move(exif_mapping)));

  encoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V4L2JpegEncodeAccelerator::EncodeTask,
                                weak_ptr_for_encoder_, std::move(job_record)));
}

void V4L2JpegEncodeAccelerator::EncodeTask(
    std::unique_ptr<JobRecord> job_record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_);

  // Check if the parameters of input frame changes.
  // If it changes, we open a new device and put the job in it.
  // If it doesn't change, we use the same device as last used.
  gfx::Size coded_size = job_record->input_frame->coded_size();
  if (latest_input_buffer_coded_size_ != coded_size ||
      latest_quality_ != job_record->quality) {
    std::unique_ptr<EncodedInstanceDmaBuf> encoded_device(
        new EncodedInstanceDmaBuf(this));
    VLOGF(1) << "Open Device for quality " << job_record->quality
             << ", width: " << coded_size.width()
             << ", height: " << coded_size.height();
    if (!encoded_device->Initialize()) {
      VLOGF(1) << "Failed to initialize device";
      NotifyError(job_record->task_id, PLATFORM_FAILURE);
      return;
    }

    if (!encoded_device->SetUpJpegParameters(job_record->quality, coded_size)) {
      VLOGF(1) << "SetUpJpegParameters failed";
      NotifyError(job_record->task_id, PLATFORM_FAILURE);
      return;
    }

    // The output buffer size is coded in the first plane's size.
    if (!encoded_device->CreateBuffers(
            coded_size, job_record->input_frame->layout(),
            job_record->output_frame->layout().planes()[0].size)) {
      VLOGF(1) << "Create buffers failed.";
      NotifyError(job_record->task_id, PLATFORM_FAILURE);
      return;
    }

    latest_input_buffer_coded_size_ = coded_size;
    latest_quality_ = job_record->quality;

    encoded_instances_dma_buf_.push(std::move(encoded_device));
  }

  // Always use latest opened device for new job.
  encoded_instances_dma_buf_.back()->input_job_queue_.push(
      std::move(job_record));

  ServiceDeviceTask();
}

void V4L2JpegEncodeAccelerator::ServiceDeviceTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_);

  // Always service the first device to keep the input order.
  encoded_instances_dma_buf_.front()->ServiceDevice();

  // If we have more than 1 devices, we can remove the oldest one after all jobs
  // finished.
  if (encoded_instances_dma_buf_.size() > 1) {
    if (encoded_instances_dma_buf_.front()->running_job_queue_.empty() &&
        encoded_instances_dma_buf_.front()->input_job_queue_.empty()) {
      encoded_instances_dma_buf_.pop();
    }
  }

  if (!encoded_instances_dma_buf_.front()->running_job_queue_.empty() ||
      !encoded_instances_dma_buf_.front()->input_job_queue_.empty()) {
    encoder_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&V4L2JpegEncodeAccelerator::ServiceDeviceTask,
                                  weak_ptr_for_encoder_));
  }
}

}  // namespace media
