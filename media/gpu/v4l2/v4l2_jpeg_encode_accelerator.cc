// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_jpeg_encode_accelerator.h"

#include <errno.h>
#include <linux/videodev2.h>
#include <string.h>
#include <sys/mman.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/numerics/ranges.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/linux/platform_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "third_party/libyuv/include/libyuv.h"

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

V4L2JpegEncodeAccelerator::I420BufferRecord::I420BufferRecord()
    : at_device(false) {
  memset(address, 0, sizeof(address));
  memset(length, 0, sizeof(length));
}

V4L2JpegEncodeAccelerator::I420BufferRecord::~I420BufferRecord() {}

V4L2JpegEncodeAccelerator::JpegBufferRecord::JpegBufferRecord()
    : at_device(false) {
  memset(address, 0, sizeof(address));
  memset(length, 0, sizeof(length));
}

V4L2JpegEncodeAccelerator::JpegBufferRecord::~JpegBufferRecord() {}

V4L2JpegEncodeAccelerator::JobRecord::JobRecord(
    scoped_refptr<VideoFrame> input_frame,
    scoped_refptr<VideoFrame> output_frame,
    int quality,
    int32_t task_id,
    BitstreamBuffer* exif_buffer)
    : input_frame(input_frame),
      output_frame(output_frame),
      quality(quality),
      task_id(task_id),
      output_shm(base::subtle::PlatformSharedMemoryRegion(), 0, true),  // dummy
      exif_shm(nullptr) {
  if (exif_buffer) {
    exif_shm.reset(new UnalignedSharedMemory(exif_buffer->TakeRegion(),
                                             exif_buffer->size(), false));
    exif_offset = exif_buffer->offset();
  }
}

V4L2JpegEncodeAccelerator::JobRecord::JobRecord(
    scoped_refptr<VideoFrame> input_frame,
    int quality,
    BitstreamBuffer* exif_buffer,
    BitstreamBuffer output_buffer)
    : input_frame(input_frame),
      quality(quality),
      task_id(output_buffer.id()),
      output_shm(output_buffer.TakeRegion(), output_buffer.size(), false),
      output_offset(output_buffer.offset()),
      exif_shm(nullptr) {
  if (exif_buffer) {
    exif_shm.reset(new UnalignedSharedMemory(exif_buffer->TakeRegion(),
                                             exif_buffer->size(), false));
    exif_offset = exif_buffer->offset();
  }
}

V4L2JpegEncodeAccelerator::JobRecord::~JobRecord() {}

V4L2JpegEncodeAccelerator::EncodedInstance::EncodedInstance(
    V4L2JpegEncodeAccelerator* parent)
    : parent_(parent),
      input_streamon_(false),
      output_streamon_(false),
      input_buffer_pixelformat_(0),
      input_buffer_num_planes_(0),
      output_buffer_pixelformat_(0) {}

V4L2JpegEncodeAccelerator::EncodedInstance::~EncodedInstance() {}

void V4L2JpegEncodeAccelerator::EncodedInstance::DestroyTask() {
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
  while (!input_job_queue_.empty())
    input_job_queue_.pop();
  while (!running_job_queue_.empty())
    running_job_queue_.pop();

  DestroyInputBuffers();
  DestroyOutputBuffers();
}

bool V4L2JpegEncodeAccelerator::EncodedInstance::Initialize() {
  device_ = V4L2Device::Create();

  if (!device_) {
    VLOGF(1) << "Failed to Create V4L2Device";
    return false;
  }

  output_buffer_pixelformat_ = V4L2_PIX_FMT_JPEG_RAW;
  if (!device_->Open(V4L2Device::Type::kJpegEncoder,
                     output_buffer_pixelformat_)) {
    VLOGF(1) << "Failed to open device";
    return false;
  }

  // Capabilities check.
  struct v4l2_capability caps;
  const __u32 kCapsRequired = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_M2M_MPLANE;
  memset(&caps, 0, sizeof(caps));
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

// static
void V4L2JpegEncodeAccelerator::EncodedInstance::FillQuantizationTable(
    int quality,
    const uint8_t* basic_table,
    uint8_t* dst_table) {
  unsigned int temp;

  if (quality < 50)
    quality = 5000 / quality;
  else
    quality = 200 - quality * 2;

  for (size_t i = 0; i < kDctSize; i++) {
    temp = ((unsigned int)basic_table[kZigZag8x8[i]] * quality + 50) / 100;
    /* limit the values to the valid range */
    dst_table[i] = base::ClampToRange(temp, 1u, 255u);
  }
}

void V4L2JpegEncodeAccelerator::EncodedInstance::PrepareJpegMarkers(
    gfx::Size coded_size) {
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
  // Quantization Tables.
  // i = 0 for Luminance
  // i = 1 for Chrominance
  const int kNumDQT = 2;
  for (size_t i = 0; i < kNumDQT; ++i) {
    const uint8_t kQuantSegment[] = {
        0xFF, JPEG_DQT, 0x00,
        0x03 + kDctSize,         // Segment length:67 (2-byte).
        static_cast<uint8_t>(i)  // Precision (4-bit high) = 0,
                                 // Index (4-bit low) = i.
    };
    for (size_t j = 0; j < sizeof(kQuantSegment); ++j) {
      jpeg_markers_.push_back(kQuantSegment[j]);
    }

    for (size_t j = 0; j < kDctSize; ++j) {
      jpeg_markers_.push_back(quantization_table_[i].value[j]);
    }
  }

  // Start of Frame - Baseline.
  const int kNumOfComponents = 3;
  const uint8_t kStartOfFrame[] = {
      0xFF,
      JPEG_SOF0,  // Baseline.
      0x00,
      0x11,  // Segment length:17 (2-byte).
      8,     // Data precision.
      static_cast<uint8_t>((coded_size.height() >> 8) & 0xFF),
      static_cast<uint8_t>(coded_size.height() & 0xFF),
      static_cast<uint8_t>((coded_size.width() >> 8) & 0xFF),
      static_cast<uint8_t>(coded_size.width() & 0xFF),
      kNumOfComponents,
  };
  for (size_t i = 0; i < sizeof(kStartOfFrame); ++i) {
    jpeg_markers_.push_back(kStartOfFrame[i]);
  }
  // i = 0 for Y Plane
  // i = 1 for U Plane
  // i = 2 for V Plane
  for (size_t i = 0; i < kNumOfComponents; ++i) {
    // These are the values for U and V planes.
    uint8_t h_sample_factor = 1;
    uint8_t v_sample_factor = 1;
    uint8_t quant_table_number = 1;
    if (!i) {
      // These are the values for Y plane.
      h_sample_factor = 2;
      v_sample_factor = 2;
      quant_table_number = 0;
    }

    jpeg_markers_.push_back(i + 1);
    // Horizontal Sample Factor (4-bit high),
    // Vertical Sample Factor (4-bit low).
    jpeg_markers_.push_back((h_sample_factor << 4) | v_sample_factor);
    jpeg_markers_.push_back(quant_table_number);
  }

  // Huffman Tables.
  static const uint8_t kDcSegment[] = {
      0xFF, JPEG_DHT, 0x00,
      0x1F,  // Segment length:31 (2-byte).
  };
  static const uint8_t kAcSegment[] = {
      0xFF, JPEG_DHT, 0x00,
      0xB5,  // Segment length:181 (2-byte).
  };

  // i = 0 for Luminance
  // i = 1 for Chrominance
  const int kNumHuffmanTables = 2;
  for (size_t i = 0; i < kNumHuffmanTables; ++i) {
    // DC Table.
    for (size_t j = 0; j < sizeof(kDcSegment); ++j) {
      jpeg_markers_.push_back(kDcSegment[j]);
    }

    // Type (4-bit high) = 0:DC, Index (4-bit low).
    jpeg_markers_.push_back(static_cast<uint8_t>(i));

    const JpegHuffmanTable& dcTable = kDefaultDcTable[i];
    for (size_t j = 0; j < kNumDcRunSizeBits; ++j)
      jpeg_markers_.push_back(dcTable.code_length[j]);
    for (size_t j = 0; j < kNumDcCodeWordsHuffVal; ++j)
      jpeg_markers_.push_back(dcTable.code_value[j]);

    // AC Table.
    for (size_t j = 0; j < sizeof(kAcSegment); ++j) {
      jpeg_markers_.push_back(kAcSegment[j]);
    }

    // Type (4-bit high) = 1:AC, Index (4-bit low).
    jpeg_markers_.push_back(0x10 | static_cast<uint8_t>(i));

    const JpegHuffmanTable& acTable = kDefaultAcTable[i];
    for (size_t j = 0; j < kNumAcRunSizeBits; ++j)
      jpeg_markers_.push_back(acTable.code_length[j]);
    for (size_t j = 0; j < kNumAcCodeWordsHuffVal; ++j)
      jpeg_markers_.push_back(acTable.code_value[j]);
  }

  // Start of Scan.
  static const uint8_t kStartOfScan[] = {
      0xFF, JPEG_SOS, 0x00,
      0x0C,  // Segment Length:12 (2-byte).
      0x03   // Number of components in scan.
  };
  for (size_t i = 0; i < sizeof(kStartOfScan); ++i) {
    jpeg_markers_.push_back(kStartOfScan[i]);
  }

  // i = 0 for Y Plane
  // i = 1 for U Plane
  // i = 2 for V Plane
  for (uint8_t i = 0; i < kNumOfComponents; ++i) {
    uint8_t dc_table_number = 1;
    uint8_t ac_table_number = 1;
    if (!i) {
      dc_table_number = 0;
      ac_table_number = 0;
    }

    jpeg_markers_.push_back(i + 1);
    // DC Table Selector (4-bit high), AC Table Selector (4-bit low).
    jpeg_markers_.push_back((dc_table_number << 4) | ac_table_number);
  }
  jpeg_markers_.push_back(0x00);  // 0 for Baseline.
  jpeg_markers_.push_back(0x3F);  // 63 for Baseline.
  jpeg_markers_.push_back(0x00);  // 0 for Baseline.
}

bool V4L2JpegEncodeAccelerator::EncodedInstance::SetUpJpegParameters(
    int quality,
    gfx::Size coded_size) {
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());

  struct v4l2_ext_controls ctrls;
  struct v4l2_ext_control ctrl;

  memset(&ctrls, 0, sizeof(ctrls));
  memset(&ctrl, 0, sizeof(ctrl));

  ctrls.ctrl_class = V4L2_CTRL_CLASS_JPEG;
  ctrls.controls = &ctrl;
  ctrls.count = 1;

  switch (output_buffer_pixelformat_) {
    case V4L2_PIX_FMT_JPEG_RAW:
      FillQuantizationTable(quality, kDefaultQuantTable[0].value,
                            quantization_table_[0].value);
      FillQuantizationTable(quality, kDefaultQuantTable[1].value,
                            quantization_table_[1].value);

      ctrl.id = V4L2_CID_JPEG_LUMA_QUANTIZATION;
      ctrl.size = kDctSize;
      ctrl.ptr = quantization_table_[0].value;
      IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_EXT_CTRLS, &ctrls);

      ctrl.id = V4L2_CID_JPEG_CHROMA_QUANTIZATION;
      ctrl.size = kDctSize;
      ctrl.ptr = quantization_table_[1].value;
      IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_EXT_CTRLS, &ctrls);

      // We need to prepare our own JPEG Markers.
      PrepareJpegMarkers(coded_size);
      break;

    default:
      NOTREACHED();
  }

  return true;
}

size_t V4L2JpegEncodeAccelerator::EncodedInstance::InputBufferQueuedCount() {
  return input_buffer_map_.size() - free_input_buffers_.size();
}

size_t V4L2JpegEncodeAccelerator::EncodedInstance::OutputBufferQueuedCount() {
  return output_buffer_map_.size() - free_output_buffers_.size();
}

bool V4L2JpegEncodeAccelerator::EncodedInstance::CreateBuffers(
    gfx::Size coded_size,
    size_t output_buffer_size) {
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());

  // The order of set output/input formats matters.
  // rk3399 reset input format when we set output format.
  if (!SetOutputBufferFormat(coded_size, output_buffer_size)) {
    return false;
  }

  if (!SetInputBufferFormat(coded_size)) {
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

bool V4L2JpegEncodeAccelerator::EncodedInstance::SetInputBufferFormat(
    gfx::Size coded_size) {
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
  DCHECK(!input_streamon_);
  DCHECK(input_job_queue_.empty());

  constexpr uint32_t input_pix_fmt_candidates[] = {
      V4L2_PIX_FMT_YUV420M,
      V4L2_PIX_FMT_YUV420,
  };

  struct v4l2_format format;
  input_buffer_pixelformat_ = 0;
  for (const auto input_pix_fmt : input_pix_fmt_candidates) {
    DCHECK_EQ(Fourcc::FromV4L2PixFmt(input_pix_fmt).ToVideoPixelFormat(),
              PIXEL_FORMAT_I420);
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    format.fmt.pix_mp.num_planes = kMaxI420Plane;
    format.fmt.pix_mp.pixelformat = input_pix_fmt;
    format.fmt.pix_mp.field = V4L2_FIELD_ANY;
    format.fmt.pix_mp.width = coded_size.width();
    format.fmt.pix_mp.height = coded_size.height();

    if (device_->Ioctl(VIDIOC_S_FMT, &format) == 0 &&
        format.fmt.pix_mp.pixelformat == input_pix_fmt) {
      // Save V4L2 returned values.
      input_buffer_pixelformat_ = format.fmt.pix_mp.pixelformat;
      input_buffer_num_planes_ = format.fmt.pix_mp.num_planes;
      input_buffer_height_ = format.fmt.pix_mp.height;
      break;
    }
  }

  if (input_buffer_pixelformat_ == 0) {
    VLOGF(1) << "Neither YUV420 nor YUV420M is supported.";
    return false;
  }

  if (format.fmt.pix_mp.width != static_cast<uint32_t>(coded_size.width()) ||
      format.fmt.pix_mp.height != static_cast<uint32_t>(coded_size.height())) {
    VLOGF(1) << "Width " << coded_size.width() << "->"
             << format.fmt.pix_mp.width << ",Height " << coded_size.height()
             << "->" << format.fmt.pix_mp.height;
    return false;
  }
  for (int i = 0; i < format.fmt.pix_mp.num_planes; i++) {
    bytes_per_line_[i] = format.fmt.pix_mp.plane_fmt[i].bytesperline;
    VLOGF(3) << "Bytes Per Line:" << bytes_per_line_[i];
  }

  return true;
}

bool V4L2JpegEncodeAccelerator::EncodedInstance::SetOutputBufferFormat(
    gfx::Size coded_size,
    size_t buffer_size) {
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
  DCHECK(!output_streamon_);
  DCHECK(running_job_queue_.empty());

  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
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

bool V4L2JpegEncodeAccelerator::EncodedInstance::RequestInputBuffers() {
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  format.fmt.pix_mp.pixelformat = input_buffer_pixelformat_;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_G_FMT, &format);

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = kBufferCount;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_REQBUFS, &reqbufs);

  DCHECK(input_buffer_map_.empty());
  input_buffer_map_.resize(reqbufs.count);

  for (size_t i = 0; i < input_buffer_map_.size(); ++i) {
    free_input_buffers_.push_back(i);

    struct v4l2_buffer buffer;
    struct v4l2_plane planes[kMaxI420Plane];
    memset(&buffer, 0, sizeof(buffer));
    memset(planes, 0, sizeof(planes));
    buffer.index = i;
    buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.m.planes = planes;
    buffer.length = base::size(planes);
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QUERYBUF, &buffer);

    if (input_buffer_num_planes_ != buffer.length) {
      return false;
    }
    for (size_t j = 0; j < buffer.length; ++j) {
      if (base::checked_cast<int64_t>(planes[j].length) <
          VideoFrame::PlaneSize(
              PIXEL_FORMAT_I420, j,
              gfx::Size(format.fmt.pix_mp.width, format.fmt.pix_mp.height))
              .GetArea()) {
        return false;
      }
      void* address =
          device_->Mmap(NULL, planes[j].length, PROT_READ | PROT_WRITE,
                        MAP_SHARED, planes[j].m.mem_offset);
      if (address == MAP_FAILED) {
        VPLOGF(1) << "mmap() failed";
        return false;
      }
      input_buffer_map_[i].address[j] = address;
      input_buffer_map_[i].length[j] = planes[j].length;
    }
  }

  return true;
}

bool V4L2JpegEncodeAccelerator::EncodedInstance::RequestOutputBuffers() {
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = kBufferCount;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_REQBUFS, &reqbufs);

  DCHECK(output_buffer_map_.empty());
  output_buffer_map_.resize(reqbufs.count);

  for (size_t i = 0; i < output_buffer_map_.size(); ++i) {
    free_output_buffers_.push_back(i);

    struct v4l2_buffer buffer;
    struct v4l2_plane planes[kMaxJpegPlane];
    memset(&buffer, 0, sizeof(buffer));
    memset(planes, 0, sizeof(planes));
    buffer.index = i;
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buffer.m.planes = planes;
    buffer.length = base::size(planes);
    buffer.memory = V4L2_MEMORY_MMAP;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QUERYBUF, &buffer);
    if (buffer.length != kMaxJpegPlane) {
      return false;
    }
    void* address =
        device_->Mmap(NULL, planes[0].length, PROT_READ | PROT_WRITE,
                      MAP_SHARED, planes[0].m.mem_offset);
    if (address == MAP_FAILED) {
      VPLOGF(1) << "mmap() failed";
      return false;
    }
    output_buffer_map_[i].address[0] = address;
    output_buffer_map_[i].length[0] = planes[0].length;
  }

  return true;
}

void V4L2JpegEncodeAccelerator::EncodedInstance::DestroyInputBuffers() {
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
  free_input_buffers_.clear();

  if (input_buffer_map_.empty())
    return;

  if (input_streamon_) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMOFF, &type);
    input_streamon_ = false;
  }

  for (const auto& input_record : input_buffer_map_) {
    for (size_t i = 0; i < input_buffer_num_planes_; ++i) {
      device_->Munmap(input_record.address[i], input_record.length[i]);
    }
  }

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  IOCTL_OR_LOG_ERROR(VIDIOC_REQBUFS, &reqbufs);

  input_buffer_map_.clear();
  input_buffer_num_planes_ = 0;
}

void V4L2JpegEncodeAccelerator::EncodedInstance::DestroyOutputBuffers() {
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
  free_output_buffers_.clear();

  if (output_buffer_map_.empty())
    return;

  if (output_streamon_) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMOFF, &type);
    output_streamon_ = false;
  }

  for (const auto& output_record : output_buffer_map_) {
    device_->Munmap(output_record.address[0], output_record.length[0]);
  }

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  IOCTL_OR_LOG_ERROR(VIDIOC_REQBUFS, &reqbufs);

  output_buffer_map_.clear();
}

void V4L2JpegEncodeAccelerator::EncodedInstance::ServiceDevice() {
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());

  if (!running_job_queue_.empty()) {
    Dequeue();
  }

  EnqueueInput();
  EnqueueOutput();

  DVLOGF(3) << "buffer counts: INPUT[" << input_job_queue_.size()
            << "] => DEVICE[" << free_input_buffers_.size() << "/"
            << input_buffer_map_.size() << "->" << free_output_buffers_.size()
            << "/" << output_buffer_map_.size() << "]";
}

void V4L2JpegEncodeAccelerator::EncodedInstance::EnqueueInput() {
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
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

void V4L2JpegEncodeAccelerator::EncodedInstance::EnqueueOutput() {
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
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

bool V4L2JpegEncodeAccelerator::EncodedInstance::EnqueueInputRecord() {
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
  DCHECK(!input_job_queue_.empty());
  DCHECK(!free_input_buffers_.empty());

  // Enqueue an input (VIDEO_OUTPUT) buffer for an input video frame.
  std::unique_ptr<JobRecord> job_record = std::move(input_job_queue_.front());
  input_job_queue_.pop();
  const int index = free_input_buffers_.back();
  I420BufferRecord& input_record = input_buffer_map_[index];
  DCHECK(!input_record.at_device);

  // Copy image from user memory to MMAP memory.
  uint8_t* src_y = job_record->input_frame->data(VideoFrame::kYPlane);
  uint8_t* src_u = job_record->input_frame->data(VideoFrame::kUPlane);
  uint8_t* src_v = job_record->input_frame->data(VideoFrame::kVPlane);
  size_t src_y_stride = job_record->input_frame->stride(VideoFrame::kYPlane);
  size_t src_u_stride = job_record->input_frame->stride(VideoFrame::kUPlane);
  size_t src_v_stride = job_record->input_frame->stride(VideoFrame::kVPlane);

  size_t input_coded_width = job_record->input_frame->coded_size().width();
  size_t input_coded_height = job_record->input_frame->coded_size().height();

  size_t dst_y_stride = bytes_per_line_[0];
  size_t dst_u_stride;
  size_t dst_v_stride;

  uint8_t* dst_y = static_cast<uint8_t*>(input_record.address[0]);
  uint8_t* dst_u;
  uint8_t* dst_v;

  if (input_buffer_num_planes_ == 1) {
    dst_u_stride = dst_y_stride / 2;
    dst_v_stride = dst_y_stride / 2;
    dst_u = dst_y + dst_y_stride * input_buffer_height_;
    dst_v = dst_u + dst_u_stride * input_buffer_height_ / 2;
  } else {
    DCHECK(input_buffer_num_planes_ == 3);

    dst_u_stride = bytes_per_line_[1];
    dst_v_stride = bytes_per_line_[2];

    dst_u = static_cast<uint8_t*>(input_record.address[1]);
    dst_v = static_cast<uint8_t*>(input_record.address[2]);
  }

  if (libyuv::I420Copy(src_y, src_y_stride, src_u, src_u_stride, src_v,
                       src_v_stride, dst_y, dst_y_stride, dst_u, dst_u_stride,
                       dst_v, dst_v_stride, input_coded_width,
                       input_coded_height)) {
    VLOGF(1) << "I420Copy failed";
    return false;
  }

  struct v4l2_buffer qbuf;
  struct v4l2_plane planes[kMaxI420Plane];
  memset(&qbuf, 0, sizeof(qbuf));
  memset(planes, 0, sizeof(planes));
  qbuf.index = index;
  qbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  qbuf.memory = V4L2_MEMORY_MMAP;
  qbuf.length = base::size(planes);
  for (size_t i = 0; i < input_buffer_num_planes_; i++) {
    // sets this to 0 means the size of the plane.
    planes[i].bytesused = 0;
  }
  qbuf.m.planes = planes;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QBUF, &qbuf);
  input_record.at_device = true;
  running_job_queue_.push(std::move(job_record));
  free_input_buffers_.pop_back();

  DVLOGF(3) << "enqueued frame id=" << job_record->task_id << " to device.";
  return true;
}

bool V4L2JpegEncodeAccelerator::EncodedInstance::EnqueueOutputRecord() {
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
  DCHECK(!free_output_buffers_.empty());

  // Enqueue an output (VIDEO_CAPTURE) buffer.
  const int index = free_output_buffers_.back();
  JpegBufferRecord& output_record = output_buffer_map_[index];
  DCHECK(!output_record.at_device);
  struct v4l2_buffer qbuf;
  struct v4l2_plane planes[kMaxJpegPlane];
  memset(&qbuf, 0, sizeof(qbuf));
  memset(planes, 0, sizeof(planes));
  qbuf.index = index;
  qbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  qbuf.memory = V4L2_MEMORY_MMAP;
  qbuf.length = base::size(planes);
  qbuf.m.planes = planes;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QBUF, &qbuf);
  output_record.at_device = true;
  free_output_buffers_.pop_back();
  return true;
}

size_t V4L2JpegEncodeAccelerator::EncodedInstance::FinalizeJpegImage(
    uint8_t* dst_ptr,
    const JpegBufferRecord& output_buffer,
    size_t buffer_size,
    std::unique_ptr<UnalignedSharedMemory> exif_shm) {
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
  size_t idx;

  // Fill SOI and EXIF markers.
  dst_ptr[0] = 0xFF;
  dst_ptr[1] = JPEG_SOI;
  idx = 2;

  if (exif_shm) {
    uint8_t* exif_buffer = static_cast<uint8_t*>(exif_shm->memory());
    size_t exif_buffer_size = exif_shm->size();
    // Application Segment for Exif data.
    uint16_t exif_segment_size = static_cast<uint16_t>(exif_buffer_size + 2);
    const uint8_t kAppSegment[] = {
        0xFF, JPEG_APP1, static_cast<uint8_t>(exif_segment_size / 256),
        static_cast<uint8_t>(exif_segment_size % 256)};
    memcpy(dst_ptr + idx, kAppSegment, sizeof(kAppSegment));
    idx += sizeof(kAppSegment);
    memcpy(dst_ptr + idx, exif_buffer, exif_buffer_size);
    idx += exif_buffer_size;
  } else {
    // Application Segment - JFIF standard 1.01.
    static const uint8_t kAppSegment[] = {
        0xFF, JPEG_APP0, 0x00,
        0x10,  // Segment length:16 (2-byte).
        0x4A,  // J
        0x46,  // F
        0x49,  // I
        0x46,  // F
        0x00,  // 0
        0x01,  // Major version.
        0x01,  // Minor version.
        0x01,  // Density units 0:no units, 1:pixels per inch,
               // 2: pixels per cm.
        0x00,
        0x48,  // X density (2-byte).
        0x00,
        0x48,  // Y density (2-byte).
        0x00,  // Thumbnail width.
        0x00   // Thumbnail height.
    };
    memcpy(dst_ptr + idx, kAppSegment, sizeof(kAppSegment));
    idx += sizeof(kAppSegment);
  }

  switch (output_buffer_pixelformat_) {
    case V4L2_PIX_FMT_JPEG_RAW:
      // Fill the other jpeg markers for RAW JPEG.
      memcpy(dst_ptr + idx, jpeg_markers_.data(), jpeg_markers_.size());
      idx += jpeg_markers_.size();
      // Fill Compressed stream.
      memcpy(dst_ptr + idx, output_buffer.address[0], buffer_size);
      idx += buffer_size;
      // Fill EOI. Before Fill EOI we checked if the V4L2 device filled EOI
      // first.
      if (dst_ptr[idx - 2] != 0xFF && dst_ptr[idx - 1] != JPEG_EOI) {
        dst_ptr[idx] = 0xFF;
        dst_ptr[idx + 1] = JPEG_EOI;
        idx += 2;
      }
      break;

    default:
      NOTREACHED() << "Unsupported output pixel format";
  }

  return idx;
}

void V4L2JpegEncodeAccelerator::EncodedInstance::Dequeue() {
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
  // Dequeue completed input (VIDEO_OUTPUT) buffers,
  // and recycle to the free list.
  struct v4l2_buffer dqbuf;
  struct v4l2_plane planes[kMaxI420Plane];
  while (InputBufferQueuedCount() > 0) {
    DCHECK(input_streamon_);
    memset(&dqbuf, 0, sizeof(dqbuf));
    memset(planes, 0, sizeof(planes));
    dqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    dqbuf.memory = V4L2_MEMORY_MMAP;
    dqbuf.length = base::size(planes);
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
    I420BufferRecord& input_record = input_buffer_map_[dqbuf.index];
    DCHECK(input_record.at_device);
    input_record.at_device = false;
    free_input_buffers_.push_back(dqbuf.index);

    if (dqbuf.flags & V4L2_BUF_FLAG_ERROR) {
      VLOGF(1) << "Error in dequeued input buffer.";
      NotifyError(kInvalidBitstreamBufferId, PARSE_IMAGE_FAILED);
      running_job_queue_.pop();
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
    memset(&dqbuf, 0, sizeof(dqbuf));
    memset(planes, 0, sizeof(planes));
    dqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    dqbuf.memory = V4L2_MEMORY_MMAP;
    dqbuf.length = base::size(planes);
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
    JpegBufferRecord& output_record = output_buffer_map_[dqbuf.index];
    DCHECK(output_record.at_device);
    output_record.at_device = false;
    free_output_buffers_.push_back(dqbuf.index);

    // Jobs are always processed in FIFO order.
    std::unique_ptr<JobRecord> job_record =
        std::move(running_job_queue_.front());
    running_job_queue_.pop();

    if (dqbuf.flags & V4L2_BUF_FLAG_ERROR) {
      VLOGF(1) << "Error in dequeued output buffer.";
      NotifyError(kInvalidBitstreamBufferId, PARSE_IMAGE_FAILED);
      return;
    }

    size_t jpeg_size = FinalizeJpegImage(
        static_cast<uint8_t*>(job_record->output_shm.memory()), output_record,
        planes[0].bytesused, std::move(job_record->exif_shm));
    if (!jpeg_size) {
      NotifyError(job_record->task_id, PLATFORM_FAILURE);
      return;
    }
    DVLOGF(4) << "Encoding finished, returning bitstream buffer, id="
              << job_record->task_id;

    parent_->VideoFrameReady(job_record->task_id, jpeg_size);
  }
}

void V4L2JpegEncodeAccelerator::EncodedInstance::NotifyError(int32_t task_id,
                                                             Status status) {
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
  parent_->NotifyError(task_id, status);
}

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
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
  while (!input_job_queue_.empty())
    input_job_queue_.pop();
  while (!running_job_queue_.empty())
    running_job_queue_.pop();

  DestroyInputBuffers();
  DestroyOutputBuffers();
}

bool V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::Initialize() {
  device_ = V4L2Device::Create();
  gpu_memory_buffer_support_ = std::make_unique<gpu::GpuMemoryBufferSupport>();

  if (!device_) {
    VLOGF(1) << "Failed to Create V4L2Device";
    return false;
  }

  output_buffer_pixelformat_ = V4L2_PIX_FMT_JPEG_RAW;
  if (!device_->Open(V4L2Device::Type::kJpegEncoder,
                     output_buffer_pixelformat_)) {
    VLOGF(1) << "Failed to open device";
    return false;
  }

  // Capabilities check.
  struct v4l2_capability caps;
  const __u32 kCapsRequired = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_M2M_MPLANE;
  memset(&caps, 0, sizeof(caps));
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

// static
void V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::FillQuantizationTable(
    int quality,
    const uint8_t* basic_table,
    uint8_t* dst_table) {
  unsigned int temp;

  if (quality < 50)
    quality = 5000 / quality;
  else
    quality = 200 - quality * 2;

  for (size_t i = 0; i < kDctSize; i++) {
    temp = ((unsigned int)basic_table[kZigZag8x8[i]] * quality + 50) / 100;
    /* limit the values to the valid range */
    dst_table[i] = base::ClampToRange(temp, 1u, 255u);
  }
}

void V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::PrepareJpegMarkers(
    gfx::Size coded_size) {
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
  // Quantization Tables.
  // i = 0 for Luminance
  // i = 1 for Chrominance
  const int kNumDQT = 2;
  for (size_t i = 0; i < kNumDQT; ++i) {
    const uint8_t kQuantSegment[] = {
        0xFF, JPEG_DQT, 0x00,
        0x03 + kDctSize,         // Segment length:67 (2-byte).
        static_cast<uint8_t>(i)  // Precision (4-bit high) = 0,
                                 // Index (4-bit low) = i.
    };
    for (size_t j = 0; j < sizeof(kQuantSegment); ++j) {
      jpeg_markers_.push_back(kQuantSegment[j]);
    }

    for (size_t j = 0; j < kDctSize; ++j) {
      jpeg_markers_.push_back(quantization_table_[i].value[j]);
    }
  }

  // Start of Frame - Baseline.
  const int kNumOfComponents = 3;
  const uint8_t kStartOfFrame[] = {
      0xFF,
      JPEG_SOF0,  // Baseline.
      0x00,
      0x11,  // Segment length:17 (2-byte).
      8,     // Data precision.
      static_cast<uint8_t>((coded_size.height() >> 8) & 0xFF),
      static_cast<uint8_t>(coded_size.height() & 0xFF),
      static_cast<uint8_t>((coded_size.width() >> 8) & 0xFF),
      static_cast<uint8_t>(coded_size.width() & 0xFF),
      kNumOfComponents,
  };
  for (size_t i = 0; i < sizeof(kStartOfFrame); ++i) {
    jpeg_markers_.push_back(kStartOfFrame[i]);
  }
  // i = 0 for Y Plane
  // i = 1 for U Plane
  // i = 2 for V Plane
  for (size_t i = 0; i < kNumOfComponents; ++i) {
    // These are the values for U and V planes.
    uint8_t h_sample_factor = 1;
    uint8_t v_sample_factor = 1;
    uint8_t quant_table_number = 1;
    if (!i) {
      // These are the values for Y plane.
      h_sample_factor = 2;
      v_sample_factor = 2;
      quant_table_number = 0;
    }

    jpeg_markers_.push_back(i + 1);
    // Horizontal Sample Factor (4-bit high),
    // Vertical Sample Factor (4-bit low).
    jpeg_markers_.push_back((h_sample_factor << 4) | v_sample_factor);
    jpeg_markers_.push_back(quant_table_number);
  }

  // Huffman Tables.
  static const uint8_t kDcSegment[] = {
      0xFF, JPEG_DHT, 0x00,
      0x1F,  // Segment length:31 (2-byte).
  };
  static const uint8_t kAcSegment[] = {
      0xFF, JPEG_DHT, 0x00,
      0xB5,  // Segment length:181 (2-byte).
  };

  // i = 0 for Luminance
  // i = 1 for Chrominance
  const int kNumHuffmanTables = 2;
  for (size_t i = 0; i < kNumHuffmanTables; ++i) {
    // DC Table.
    for (size_t j = 0; j < sizeof(kDcSegment); ++j) {
      jpeg_markers_.push_back(kDcSegment[j]);
    }

    // Type (4-bit high) = 0:DC, Index (4-bit low).
    jpeg_markers_.push_back(static_cast<uint8_t>(i));

    const JpegHuffmanTable& dcTable = kDefaultDcTable[i];
    for (size_t j = 0; j < kNumDcRunSizeBits; ++j)
      jpeg_markers_.push_back(dcTable.code_length[j]);
    for (size_t j = 0; j < kNumDcCodeWordsHuffVal; ++j)
      jpeg_markers_.push_back(dcTable.code_value[j]);

    // AC Table.
    for (size_t j = 0; j < sizeof(kAcSegment); ++j) {
      jpeg_markers_.push_back(kAcSegment[j]);
    }

    // Type (4-bit high) = 1:AC, Index (4-bit low).
    jpeg_markers_.push_back(0x10 | static_cast<uint8_t>(i));

    const JpegHuffmanTable& acTable = kDefaultAcTable[i];
    for (size_t j = 0; j < kNumAcRunSizeBits; ++j)
      jpeg_markers_.push_back(acTable.code_length[j]);
    for (size_t j = 0; j < kNumAcCodeWordsHuffVal; ++j)
      jpeg_markers_.push_back(acTable.code_value[j]);
  }

  // Start of Scan.
  static const uint8_t kStartOfScan[] = {
      0xFF, JPEG_SOS, 0x00,
      0x0C,  // Segment Length:12 (2-byte).
      0x03   // Number of components in scan.
  };
  for (size_t i = 0; i < sizeof(kStartOfScan); ++i) {
    jpeg_markers_.push_back(kStartOfScan[i]);
  }

  // i = 0 for Y Plane
  // i = 1 for U Plane
  // i = 2 for V Plane
  for (uint8_t i = 0; i < kNumOfComponents; ++i) {
    uint8_t dc_table_number = 1;
    uint8_t ac_table_number = 1;
    if (!i) {
      dc_table_number = 0;
      ac_table_number = 0;
    }

    jpeg_markers_.push_back(i + 1);
    // DC Table Selector (4-bit high), AC Table Selector (4-bit low).
    jpeg_markers_.push_back((dc_table_number << 4) | ac_table_number);
  }
  jpeg_markers_.push_back(0x00);  // 0 for Baseline.
  jpeg_markers_.push_back(0x3F);  // 63 for Baseline.
  jpeg_markers_.push_back(0x00);  // 0 for Baseline.
}

bool V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::SetUpJpegParameters(
    int quality,
    gfx::Size coded_size) {
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());

  struct v4l2_ext_controls ctrls;
  struct v4l2_ext_control ctrl;

  memset(&ctrls, 0, sizeof(ctrls));
  memset(&ctrl, 0, sizeof(ctrl));

  ctrls.ctrl_class = V4L2_CTRL_CLASS_JPEG;
  ctrls.controls = &ctrl;
  ctrls.count = 1;

  switch (output_buffer_pixelformat_) {
    case V4L2_PIX_FMT_JPEG_RAW:
      FillQuantizationTable(quality, kDefaultQuantTable[0].value,
                            quantization_table_[0].value);
      FillQuantizationTable(quality, kDefaultQuantTable[1].value,
                            quantization_table_[1].value);

      ctrl.id = V4L2_CID_JPEG_LUMA_QUANTIZATION;
      ctrl.size = kDctSize;
      ctrl.ptr = quantization_table_[0].value;
      IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_EXT_CTRLS, &ctrls);

      ctrl.id = V4L2_CID_JPEG_CHROMA_QUANTIZATION;
      ctrl.size = kDctSize;
      ctrl.ptr = quantization_table_[1].value;
      IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_EXT_CTRLS, &ctrls);

      // We need to prepare our own JPEG Markers.
      PrepareJpegMarkers(coded_size);
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
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());

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
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
  DCHECK(!input_streamon_);
  DCHECK(input_job_queue_.empty());

  constexpr uint32_t input_pix_fmt_candidates[] = {V4L2_PIX_FMT_NV12M,
                                                   V4L2_PIX_FMT_NV12};

  struct v4l2_format format;
  input_buffer_pixelformat_ = 0;
  for (const auto input_pix_fmt : input_pix_fmt_candidates) {
    DCHECK_EQ(Fourcc::FromV4L2PixFmt(input_pix_fmt).ToVideoPixelFormat(),
              PIXEL_FORMAT_NV12);
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    format.fmt.pix_mp.num_planes = kMaxNV12Plane;
    format.fmt.pix_mp.pixelformat = input_pix_fmt;
    format.fmt.pix_mp.field = V4L2_FIELD_ANY;
    format.fmt.pix_mp.width = coded_size.width();
    format.fmt.pix_mp.height = coded_size.height();

    auto num_planes = input_layout.num_planes();
    for (size_t i = 0; i < num_planes; i++) {
      format.fmt.pix_mp.plane_fmt[i].sizeimage = input_layout.planes()[i].size;
      format.fmt.pix_mp.plane_fmt[i].bytesperline =
          input_layout.planes()[i].stride;
    }

    if (device_->Ioctl(VIDIOC_S_FMT, &format) == 0 &&
        format.fmt.pix_mp.pixelformat == input_pix_fmt) {
      device_input_layout_ = V4L2Device::V4L2FormatToVideoFrameLayout(format);

      // Save V4L2 returned values.
      input_buffer_pixelformat_ = format.fmt.pix_mp.pixelformat;
      input_buffer_num_planes_ = format.fmt.pix_mp.num_planes;
      input_buffer_height_ = format.fmt.pix_mp.height;
      break;
    }
  }

  if (input_buffer_pixelformat_ == 0) {
    VLOGF(1) << "Neither NV12 nor NV12M is supported.";
    return false;
  }

  if (format.fmt.pix_mp.width != static_cast<uint32_t>(coded_size.width()) ||
      format.fmt.pix_mp.height != static_cast<uint32_t>(coded_size.height())) {
    VLOGF(1) << "Width " << coded_size.width() << "->"
             << format.fmt.pix_mp.width << ",Height " << coded_size.height()
             << "->" << format.fmt.pix_mp.height;
    return false;
  }
  return true;
}

bool V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::SetOutputBufferFormat(
    gfx::Size coded_size,
    size_t buffer_size) {
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
  DCHECK(!output_streamon_);
  DCHECK(running_job_queue_.empty());

  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
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
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  format.fmt.pix_mp.pixelformat = input_buffer_pixelformat_;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_G_FMT, &format);

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
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
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
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
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
  free_input_buffers_.clear();

  if (input_streamon_) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMOFF, &type);
    input_streamon_ = false;
  }

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbufs.memory = V4L2_MEMORY_DMABUF;
  IOCTL_OR_LOG_ERROR(VIDIOC_REQBUFS, &reqbufs);

  input_buffer_num_planes_ = 0;
}

void V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::DestroyOutputBuffers() {
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
  free_output_buffers_.clear();

  if (output_streamon_) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMOFF, &type);
    output_streamon_ = false;
  }

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_DMABUF;
  IOCTL_OR_LOG_ERROR(VIDIOC_REQBUFS, &reqbufs);
}

void V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::ServiceDevice() {
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());

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
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
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
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
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
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
  DCHECK(!input_job_queue_.empty());
  DCHECK(!free_input_buffers_.empty());

  // Enqueue an input (VIDEO_OUTPUT) buffer for an input video frame.
  std::unique_ptr<JobRecord> job_record = std::move(input_job_queue_.front());
  input_job_queue_.pop();
  const int index = free_input_buffers_.back();

  struct v4l2_buffer qbuf;
  struct v4l2_plane planes[kMaxNV12Plane];
  memset(&qbuf, 0, sizeof(qbuf));
  memset(planes, 0, sizeof(planes));
  qbuf.index = index;
  qbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  qbuf.memory = V4L2_MEMORY_DMABUF;
  qbuf.length = base::size(planes);
  qbuf.m.planes = planes;

  const auto& frame = job_record->input_frame;
  for (size_t i = 0; i < input_buffer_num_planes_; i++) {
    if (device_input_layout_->is_multi_planar()) {
      qbuf.m.planes[i].bytesused = base::checked_cast<__u32>(
          VideoFrame::PlaneSize(frame->format(), i,
                                device_input_layout_->coded_size())
              .GetArea());
    } else {
      qbuf.m.planes[i].bytesused = VideoFrame::AllocationSize(
          frame->format(), device_input_layout_->coded_size());
    }

    const auto& fds = frame->DmabufFds();
    const auto& planes = frame->layout().planes();
    qbuf.m.planes[i].m.fd = (i < fds.size()) ? fds[i].get() : fds.back().get();
    qbuf.m.planes[i].data_offset = planes[i].offset;
    qbuf.m.planes[i].bytesused += qbuf.m.planes[i].data_offset;
    qbuf.m.planes[i].length = planes[i].size + qbuf.m.planes[i].data_offset;
  }

  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QBUF, &qbuf);
  running_job_queue_.push(std::move(job_record));
  free_input_buffers_.pop_back();
  return true;
}

bool V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::EnqueueOutputRecord() {
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
  DCHECK(!free_output_buffers_.empty());

  // Enqueue an output (VIDEO_CAPTURE) buffer.
  const int index = free_output_buffers_.back();
  struct v4l2_buffer qbuf;
  struct v4l2_plane planes[kMaxJpegPlane];
  memset(&qbuf, 0, sizeof(qbuf));
  memset(planes, 0, sizeof(planes));
  qbuf.index = index;
  qbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  qbuf.memory = V4L2_MEMORY_DMABUF;
  qbuf.length = base::size(planes);
  qbuf.m.planes = planes;

  auto& job_record = running_job_queue_.back();
  for (size_t i = 0; i < qbuf.length; i++) {
    planes[i].m.fd = job_record->output_frame->DmabufFds()[i].get();
  }
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QBUF, &qbuf);
  free_output_buffers_.pop_back();
  return true;
}

size_t V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::FinalizeJpegImage(
    scoped_refptr<VideoFrame> output_frame,
    size_t buffer_size,
    std::unique_ptr<UnalignedSharedMemory> exif_shm) {
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
  size_t idx = 0;

  auto output_gmb_handle = CreateGpuMemoryBufferHandle(output_frame.get());
  DCHECK(!output_gmb_handle.is_null());

  // In this case, we use the R_8 buffer with height == 1 to represent a data
  // container. As a result, we use plane.stride as size of the data here since
  // plane.size might be larger due to height alignment.
  const gfx::Size output_gmb_buffer_size(
      base::checked_cast<int32_t>(output_frame->layout().planes()[0].stride),
      1);

  auto output_gmb_buffer =
      gpu_memory_buffer_support_->CreateGpuMemoryBufferImplFromHandle(
          std::move(output_gmb_handle), output_gmb_buffer_size,
          gfx::BufferFormat::R_8, gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE,
          base::DoNothing());

  bool isMapped = output_gmb_buffer->Map();
  if (!isMapped) {
    VLOGF(1) << "Failed to map gmb buffer";
    return 0;
  }
  uint8_t* dst_ptr = static_cast<uint8_t*>(output_gmb_buffer->memory(0));

  // Fill SOI and EXIF markers.
  static const uint8_t kJpegStart[] = {0xFF, JPEG_SOI};

  if (exif_shm) {
    uint8_t* exif_buffer = static_cast<uint8_t*>(exif_shm->memory());
    size_t exif_buffer_size = exif_shm->size();
    // Application Segment for Exif data.
    uint16_t exif_segment_size = static_cast<uint16_t>(exif_buffer_size + 2);
    const uint8_t kAppSegment[] = {
        0xFF, JPEG_APP1, static_cast<uint8_t>(exif_segment_size / 256),
        static_cast<uint8_t>(exif_segment_size % 256)};

    // Move compressed data first.
    size_t compressed_data_offset = sizeof(kJpegStart) + sizeof(kAppSegment) +
                                    exif_buffer_size + jpeg_markers_.size();
    memmove(dst_ptr + compressed_data_offset, dst_ptr, buffer_size);

    memcpy(dst_ptr, kJpegStart, sizeof(kJpegStart));
    idx += sizeof(kJpegStart);
    memcpy(dst_ptr + idx, kAppSegment, sizeof(kAppSegment));
    idx += sizeof(kAppSegment);
    memcpy(dst_ptr + idx, exif_buffer, exif_buffer_size);
    idx += exif_buffer_size;
  } else {
    // Application Segment - JFIF standard 1.01.
    static const uint8_t kAppSegment[] = {
        0xFF, JPEG_APP0, 0x00,
        0x10,  // Segment length:16 (2-byte).
        0x4A,  // J
        0x46,  // F
        0x49,  // I
        0x46,  // F
        0x00,  // 0
        0x01,  // Major version.
        0x01,  // Minor version.
        0x01,  // Density units 0:no units, 1:pixels per inch,
               // 2: pixels per cm.
        0x00,
        0x48,  // X density (2-byte).
        0x00,
        0x48,  // Y density (2-byte).
        0x00,  // Thumbnail width.
        0x00   // Thumbnail height.
    };

    // Move compressed data first.
    size_t compressed_data_offset =
        sizeof(kJpegStart) + sizeof(kAppSegment) + jpeg_markers_.size();
    memmove(dst_ptr + compressed_data_offset, dst_ptr, buffer_size);

    memcpy(dst_ptr, kJpegStart, sizeof(kJpegStart));
    idx += sizeof(kJpegStart);

    memcpy(dst_ptr + idx, kAppSegment, sizeof(kAppSegment));
    idx += sizeof(kAppSegment);
  }

  switch (output_buffer_pixelformat_) {
    case V4L2_PIX_FMT_JPEG_RAW:
      // Fill the other jpeg markers for RAW JPEG.
      memcpy(dst_ptr + idx, jpeg_markers_.data(), jpeg_markers_.size());
      idx += jpeg_markers_.size();
      // We already moved the compressed data.
      idx += buffer_size;
      // Fill EOI. Before Fill EOI we checked if the V4L2 device filled EOI
      // first.
      if (dst_ptr[idx - 2] != 0xFF && dst_ptr[idx - 1] != JPEG_EOI) {
        dst_ptr[idx] = 0xFF;
        dst_ptr[idx + 1] = JPEG_EOI;
        idx += 2;
      }
      break;

    default:
      NOTREACHED() << "Unsupported output pixel format";
  }

  output_gmb_buffer->Unmap();

  return idx;
}

void V4L2JpegEncodeAccelerator::EncodedInstanceDmaBuf::Dequeue() {
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
  // Dequeue completed input (VIDEO_OUTPUT) buffers,
  // and recycle to the free list.
  struct v4l2_buffer dqbuf;
  struct v4l2_plane planes[kMaxNV12Plane];
  while (InputBufferQueuedCount() > 0) {
    DCHECK(input_streamon_);
    memset(&dqbuf, 0, sizeof(dqbuf));
    memset(planes, 0, sizeof(planes));
    dqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    dqbuf.memory = V4L2_MEMORY_DMABUF;
    dqbuf.length = base::size(planes);
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
      running_job_queue_.pop();
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
    memset(&dqbuf, 0, sizeof(dqbuf));
    memset(planes, 0, sizeof(planes));
    dqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    dqbuf.memory = V4L2_MEMORY_DMABUF;
    dqbuf.length = base::size(planes);
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
    running_job_queue_.pop();

    if (dqbuf.flags & V4L2_BUF_FLAG_ERROR) {
      VLOGF(1) << "Error in dequeued output buffer.";
      NotifyError(kInvalidBitstreamBufferId, PARSE_IMAGE_FAILED);
      return;
    }

    size_t jpeg_size =
        FinalizeJpegImage(job_record->output_frame, planes[0].bytesused,
                          std::move(job_record->exif_shm));

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
  DCHECK(parent_->encoder_task_runner_->BelongsToCurrentThread());
  parent_->NotifyError(task_id, status);
}

V4L2JpegEncodeAccelerator::V4L2JpegEncodeAccelerator(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : child_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_task_runner_(io_task_runner),
      client_(nullptr),
      encoder_thread_("V4L2JpegEncodeThread"),
      weak_factory_(this) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

V4L2JpegEncodeAccelerator::~V4L2JpegEncodeAccelerator() {
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  if (encoder_thread_.IsRunning()) {
    encoder_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&V4L2JpegEncodeAccelerator::DestroyTask,
                                  base::Unretained(this)));
    encoder_thread_.Stop();
  }
  weak_factory_.InvalidateWeakPtrs();
}

void V4L2JpegEncodeAccelerator::DestroyTask() {
  DCHECK(encoder_task_runner_->BelongsToCurrentThread());

  while (!encoded_instances_.empty()) {
    encoded_instances_.front()->DestroyTask();
    encoded_instances_.pop();
  }

  while (!encoded_instances_dma_buf_.empty()) {
    encoded_instances_dma_buf_.front()->DestroyTask();
    encoded_instances_dma_buf_.pop();
  }
}

void V4L2JpegEncodeAccelerator::VideoFrameReady(int32_t task_id,
                                                size_t encoded_picture_size) {
  if (!child_task_runner_->BelongsToCurrentThread()) {
    child_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&V4L2JpegEncodeAccelerator::VideoFrameReady,
                                  weak_ptr_, task_id, encoded_picture_size));
    return;
  }
  VLOGF(1) << "Encoding finished task id=" << task_id
           << " Compressed size:" << encoded_picture_size;
  client_->VideoFrameReady(task_id, encoded_picture_size);
}

void V4L2JpegEncodeAccelerator::NotifyError(int32_t task_id, Status status) {
  if (!child_task_runner_->BelongsToCurrentThread()) {
    child_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&V4L2JpegEncodeAccelerator::NotifyError,
                                  weak_ptr_, task_id, status));

    return;
  }
  VLOGF(1) << "Notifying of error " << status << " for task id " << task_id;
  client_->NotifyError(task_id, status);
}

chromeos_camera::JpegEncodeAccelerator::Status
V4L2JpegEncodeAccelerator::Initialize(
    chromeos_camera::JpegEncodeAccelerator::Client* client) {
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  std::unique_ptr<EncodedInstanceDmaBuf> encoded_device(
      new EncodedInstanceDmaBuf(this));

  // We just check if we can initialize device here.
  if (!encoded_device->Initialize()) {
    VLOGF(1) << "Failed to initialize device";
    return HW_JPEG_ENCODE_NOT_SUPPORTED;
  }

  if (!encoder_thread_.Start()) {
    VLOGF(1) << "encoder thread failed to start";
    return THREAD_CREATION_FAILED;
  }

  client_ = client;

  encoder_task_runner_ = encoder_thread_.task_runner();

  VLOGF(2) << "V4L2JpegEncodeAccelerator initialized.";
  return ENCODE_OK;
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
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  DVLOGF(4) << "task_id=" << output_buffer.id()
            << ", size=" << output_buffer.size();

  if (quality <= 0 || quality > 100) {
    VLOGF(1) << "quality is not in range. " << quality;
    NotifyError(output_buffer.id(), INVALID_ARGUMENT);
    return;
  }

  if (video_frame->format() != VideoPixelFormat::PIXEL_FORMAT_I420) {
    VLOGF(1) << "Format is not I420";
    NotifyError(output_buffer.id(), INVALID_ARGUMENT);
    return;
  }

  if (exif_buffer) {
    VLOGF(4) << "EXIF size " << exif_buffer->size();
    if (exif_buffer->size() > kMaxMarkerSizeAllowed) {
      NotifyError(output_buffer.id(), INVALID_ARGUMENT);
      return;
    }
  }

  std::unique_ptr<JobRecord> job_record(new JobRecord(
      video_frame, quality, exif_buffer, std::move(output_buffer)));

  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2JpegEncodeAccelerator::EncodeTaskLegacy,
                     base::Unretained(this), base::Passed(&job_record)));
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

  if (exif_buffer) {
    VLOGF(4) << "EXIF size " << exif_buffer->size();
    if (exif_buffer->size() > kMaxMarkerSizeAllowed) {
      NotifyError(task_id, INVALID_ARGUMENT);
      return;
    }
  }

  std::unique_ptr<JobRecord> job_record(
      new JobRecord(input_frame, output_frame, quality, task_id, exif_buffer));

  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2JpegEncodeAccelerator::EncodeTask,
                     base::Unretained(this), base::Passed(&job_record)));
}

void V4L2JpegEncodeAccelerator::EncodeTaskLegacy(
    std::unique_ptr<JobRecord> job_record) {
  DCHECK(encoder_task_runner_->BelongsToCurrentThread());
  if (!job_record->output_shm.MapAt(job_record->output_offset,
                                    job_record->output_shm.size())) {
    VPLOGF(1) << "could not map I420 bitstream_buffer";
    NotifyError(job_record->task_id, PLATFORM_FAILURE);
    return;
  }
  if (job_record->exif_shm &&
      !job_record->exif_shm->MapAt(job_record->exif_offset,
                                   job_record->exif_shm->size())) {
    VPLOGF(1) << "could not map exif bitstream_buffer";
    NotifyError(job_record->task_id, PLATFORM_FAILURE);
    return;
  }

  // Check if the parameters of input frame changes.
  // If it changes, we open a new device and put the job in it.
  // If it doesn't change, we use the same device as last used.
  gfx::Size coded_size = job_record->input_frame->coded_size();
  if (latest_input_buffer_coded_size_legacy_ != coded_size ||
      latest_quality_legacy_ != job_record->quality) {
    std::unique_ptr<EncodedInstance> encoded_device(new EncodedInstance(this));
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

    if (!encoded_device->CreateBuffers(coded_size,
                                       job_record->output_shm.size())) {
      VLOGF(1) << "Create buffers failed.";
      NotifyError(job_record->task_id, PLATFORM_FAILURE);
      return;
    }

    latest_input_buffer_coded_size_legacy_ = coded_size;
    latest_quality_legacy_ = job_record->quality;

    encoded_instances_.push(std::move(encoded_device));
  }

  // Always use latest opened device for new job.
  encoded_instances_.back()->input_job_queue_.push(std::move(job_record));

  ServiceDeviceTaskLegacy();
}

void V4L2JpegEncodeAccelerator::EncodeTask(
    std::unique_ptr<JobRecord> job_record) {
  DCHECK(encoder_task_runner_->BelongsToCurrentThread());
  if (job_record->exif_shm &&
      !job_record->exif_shm->MapAt(job_record->exif_offset,
                                   job_record->exif_shm->size())) {
    VPLOGF(1) << "could not map exif bitstream_buffer";
    NotifyError(job_record->task_id, PLATFORM_FAILURE);
    return;
  }

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

void V4L2JpegEncodeAccelerator::ServiceDeviceTaskLegacy() {
  DCHECK(encoder_task_runner_->BelongsToCurrentThread());

  // Always service the first device to keep the input order.
  encoded_instances_.front()->ServiceDevice();

  // If we have more than 1 devices, we can remove the oldest one after all jobs
  // finished.
  if (encoded_instances_.size() > 1) {
    if (encoded_instances_.front()->running_job_queue_.empty() &&
        encoded_instances_.front()->input_job_queue_.empty()) {
      encoded_instances_.pop();
    }
  }

  if (!encoded_instances_.front()->running_job_queue_.empty() ||
      !encoded_instances_.front()->input_job_queue_.empty()) {
    encoder_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&V4L2JpegEncodeAccelerator::ServiceDeviceTaskLegacy,
                       base::Unretained(this)));
  }
}

void V4L2JpegEncodeAccelerator::ServiceDeviceTask() {
  DCHECK(encoder_task_runner_->BelongsToCurrentThread());

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
                                  base::Unretained(this)));
  }
}

}  // namespace media
