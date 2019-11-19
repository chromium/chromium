// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_mjpeg_decode_accelerator.h"

#include <errno.h>
#include <linux/videodev2.h>
#include <string.h>
#include <sys/mman.h>

#include <array>
#include <memory>
#include <utility>

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/files/scoped_file.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process_metrics.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/unaligned_shared_memory.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/linux/platform_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "media/gpu/video_frame_mapper.h"
#include "media/gpu/video_frame_mapper_factory.h"
#include "media/parsers/jpeg_parser.h"
#include "third_party/libyuv/include/libyuv.h"

#define IOCTL_OR_ERROR_RETURN_VALUE(type, arg, value, type_name) \
  do {                                                           \
    if (device_->Ioctl(type, arg) != 0) {                        \
      VPLOGF(1) << "ioctl() failed: " << type_name;              \
      PostNotifyError(kInvalidTaskId, PLATFORM_FAILURE);         \
      return value;                                              \
    }                                                            \
  } while (0)

#define IOCTL_OR_ERROR_RETURN(type, arg) \
  IOCTL_OR_ERROR_RETURN_VALUE(type, arg, ((void)0), #type)

#define IOCTL_OR_ERROR_RETURN_FALSE(type, arg) \
  IOCTL_OR_ERROR_RETURN_VALUE(type, arg, false, #type)

#define IOCTL_OR_LOG_ERROR(type, arg)                    \
  do {                                                   \
    if (device_->Ioctl(type, arg) != 0) {                \
      VPLOGF(1) << "ioctl() failed: " << #type;          \
      PostNotifyError(kInvalidTaskId, PLATFORM_FAILURE); \
    }                                                    \
  } while (0)

#define READ_U8_OR_RETURN_FALSE(reader, out)                               \
  do {                                                                     \
    uint8_t _out;                                                          \
    if (!reader.ReadU8(&_out)) {                                           \
      DVLOGF(1)                                                            \
          << "Error in stream: unexpected EOS while trying to read " #out; \
      return false;                                                        \
    }                                                                      \
    *(out) = _out;                                                         \
  } while (0)

#define READ_U16_OR_RETURN_FALSE(reader, out)                              \
  do {                                                                     \
    uint16_t _out;                                                         \
    if (!reader.ReadU16(&_out)) {                                          \
      DVLOGF(1)                                                            \
          << "Error in stream: unexpected EOS while trying to read " #out; \
      return false;                                                        \
    }                                                                      \
    *(out) = _out;                                                         \
  } while (0)

namespace {

// Input pixel format (i.e. V4L2_PIX_FMT_JPEG) has only one physical plane.
const size_t kMaxInputPlanes = 1;

// This class can only handle V4L2_PIX_FMT_JPEG as input, so kMaxInputPlanes
// can only be 1.
static_assert(kMaxInputPlanes == 1,
              "kMaxInputPlanes must be 1 as input must be V4L2_PIX_FMT_JPEG");
}  // namespace

namespace media {

// This is default huffman segment for 8-bit precision luminance and
// chrominance. The default huffman segment is constructed with the tables from
// JPEG standard section K.3. Actually there are no default tables. They are
// typical tables. These tables are useful for many applications. Lots of
// softwares use them as standard tables such as ffmpeg.
const uint8_t kDefaultDhtSeg[] = {
    0xFF, 0xC4, 0x01, 0xA2, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02,
    0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x01, 0x00, 0x03,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
    0x0A, 0x0B, 0x10, 0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05,
    0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7D, 0x01, 0x02, 0x03, 0x00, 0x04,
    0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22,
    0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1, 0x15,
    0x52, 0xD1, 0xF0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x34, 0x35, 0x36,
    0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A,
    0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66,
    0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A,
    0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95,
    0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8,
    0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2,
    0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5,
    0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
    0xE8, 0xE9, 0xEA, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9,
    0xFA, 0x11, 0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04, 0x07, 0x05,
    0x04, 0x04, 0x00, 0x01, 0x02, 0x77, 0x00, 0x01, 0x02, 0x03, 0x11, 0x04,
    0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71, 0x13, 0x22,
    0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33,
    0x52, 0xF0, 0x15, 0x62, 0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34, 0xE1, 0x25,
    0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x35, 0x36,
    0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A,
    0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66,
    0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A,
    0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92, 0x93, 0x94,
    0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
    0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA,
    0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4,
    0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
    0xE8, 0xE9, 0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA};

class V4L2MjpegDecodeAccelerator::JobRecord {
 public:
  virtual ~JobRecord() = default;

  // Task ID passed from Decode() call.
  virtual int32_t task_id() const = 0;
  // Input buffer size.
  virtual size_t size() const = 0;
  // Input buffer offset.
  virtual off_t offset() const = 0;
  // Maps input buffer.
  virtual bool map() = 0;
  // Pointer to the input content. Only valid if map() is already called.
  virtual const void* memory() const = 0;

  // Output frame buffer.
  virtual const scoped_refptr<VideoFrame>& out_frame() = 0;

 protected:
  JobRecord() = default;

  DISALLOW_COPY_AND_ASSIGN(JobRecord);
};

// Job record when the client uses BitstreamBuffer as input in Decode().
class JobRecordBitstreamBuffer : public V4L2MjpegDecodeAccelerator::JobRecord {
 public:
  JobRecordBitstreamBuffer(BitstreamBuffer bitstream_buffer,
                           scoped_refptr<VideoFrame> video_frame)
      : task_id_(bitstream_buffer.id()),
        shm_(bitstream_buffer.TakeRegion(),
             bitstream_buffer.size(),
             false /* read_only */),
        offset_(bitstream_buffer.offset()),
        out_frame_(video_frame) {}

  int32_t task_id() const override { return task_id_; }
  size_t size() const override { return shm_.size(); }
  off_t offset() const override { return offset_; }
  bool map() override { return shm_.MapAt(offset(), size()); }
  const void* memory() const override { return shm_.memory(); }

  const scoped_refptr<VideoFrame>& out_frame() override { return out_frame_; }

 private:
  int32_t task_id_;
  UnalignedSharedMemory shm_;
  off_t offset_;
  scoped_refptr<VideoFrame> out_frame_;

  DISALLOW_COPY_AND_ASSIGN(JobRecordBitstreamBuffer);
};

// Job record when the client uses DMA buffer as input in Decode().
class JobRecordDmaBuf : public V4L2MjpegDecodeAccelerator::JobRecord {
 public:
  JobRecordDmaBuf(int32_t task_id,
                  base::ScopedFD src_dmabuf_fd,
                  size_t src_size,
                  off_t src_offset,
                  scoped_refptr<VideoFrame> dst_frame)
      : task_id_(task_id),
        dmabuf_fd_(std::move(src_dmabuf_fd)),
        size_(src_size),
        offset_(src_offset),
        mapped_addr_(nullptr),
        out_frame_(std::move(dst_frame)) {}

  ~JobRecordDmaBuf() {
    if (mapped_addr_) {
      const int ret = munmap(mapped_addr_, size());
      DPCHECK(ret == 0);
    }
  }

  int32_t task_id() const override { return task_id_; }
  size_t size() const override { return size_; }
  off_t offset() const override { return offset_; }

  bool map() override {
    if (mapped_addr_)
      return true;
    // The DMA-buf FD should be mapped as read-only since it may only have read
    // permission, e.g. when it comes from camera driver.
    DCHECK(dmabuf_fd_.is_valid());
    DCHECK_GT(size(), 0u);
    void* addr = mmap(nullptr, size(), PROT_READ, MAP_SHARED, dmabuf_fd_.get(),
                      offset());
    if (addr == MAP_FAILED)
      return false;
    mapped_addr_ = addr;
    return true;
  }

  const void* memory() const override {
    DCHECK(mapped_addr_);
    return mapped_addr_;
  }

  const scoped_refptr<VideoFrame>& out_frame() override { return out_frame_; }

 private:
  int32_t task_id_;
  base::ScopedFD dmabuf_fd_;
  size_t size_;
  off_t offset_;
  void* mapped_addr_;
  scoped_refptr<VideoFrame> out_frame_;

  DISALLOW_COPY_AND_ASSIGN(JobRecordDmaBuf);
};

V4L2MjpegDecodeAccelerator::BufferRecord::BufferRecord() : at_device(false) {
  memset(address, 0, sizeof(address));
  memset(length, 0, sizeof(length));
}

V4L2MjpegDecodeAccelerator::BufferRecord::~BufferRecord() {}

V4L2MjpegDecodeAccelerator::V4L2MjpegDecodeAccelerator(
    const scoped_refptr<V4L2Device>& device,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : output_buffer_pixelformat_(0),
      output_buffer_num_planes_(0),
      child_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_task_runner_(io_task_runner),
      client_(nullptr),
      device_(device),
      decoder_thread_("V4L2MjpegDecodeThread"),
      device_poll_thread_("V4L2MjpegDecodeDevicePollThread"),
      input_streamon_(false),
      output_streamon_(false),
      weak_factory_(this) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

V4L2MjpegDecodeAccelerator::~V4L2MjpegDecodeAccelerator() {
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  if (decoder_thread_.IsRunning()) {
    decoder_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&V4L2MjpegDecodeAccelerator::DestroyTask,
                                  base::Unretained(this)));
    decoder_thread_.Stop();
  }
  weak_factory_.InvalidateWeakPtrs();
  DCHECK(!device_poll_thread_.IsRunning());
}

void V4L2MjpegDecodeAccelerator::DestroyTask() {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  while (!input_jobs_.empty())
    input_jobs_.pop();
  while (!running_jobs_.empty())
    running_jobs_.pop();

  // Stop streaming and the device_poll_thread_.
  StopDevicePoll();

  DestroyInputBuffers();
  DestroyOutputBuffers();
}

void V4L2MjpegDecodeAccelerator::VideoFrameReady(int32_t task_id) {
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  client_->VideoFrameReady(task_id);
}

void V4L2MjpegDecodeAccelerator::NotifyError(int32_t task_id, Error error) {
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  VLOGF(1) << "Notifying of error " << error << " for task id " << task_id;
  client_->NotifyError(task_id, error);
}

void V4L2MjpegDecodeAccelerator::PostNotifyError(int32_t task_id, Error error) {
  child_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V4L2MjpegDecodeAccelerator::NotifyError,
                                weak_ptr_, task_id, error));
}

bool V4L2MjpegDecodeAccelerator::Initialize(
    chromeos_camera::MjpegDecodeAccelerator::Client* client) {
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  if (!device_->Open(V4L2Device::Type::kJpegDecoder, V4L2_PIX_FMT_JPEG)) {
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

  // Subscribe to the source change event.
  struct v4l2_event_subscription sub;
  memset(&sub, 0, sizeof(sub));
  sub.type = V4L2_EVENT_SOURCE_CHANGE;
  if (device_->Ioctl(VIDIOC_SUBSCRIBE_EVENT, &sub) != 0) {
    VPLOGF(1) << "ioctl() failed: VIDIOC_SUBSCRIBE_EVENT";
    return false;
  }

  if (!decoder_thread_.Start()) {
    VLOGF(1) << "decoder thread failed to start";
    return false;
  }
  client_ = client;
  decoder_task_runner_ = decoder_thread_.task_runner();

  decoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V4L2MjpegDecodeAccelerator::StartDevicePoll,
                                base::Unretained(this)));

  VLOGF(2) << "V4L2MjpegDecodeAccelerator initialized.";
  return true;
}

void V4L2MjpegDecodeAccelerator::Decode(BitstreamBuffer bitstream_buffer,
                                        scoped_refptr<VideoFrame> video_frame) {
  DVLOGF(4) << "input_id=" << bitstream_buffer.id()
            << ", size=" << bitstream_buffer.size();
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (bitstream_buffer.id() < 0) {
    VLOGF(1) << "Invalid bitstream_buffer, id: " << bitstream_buffer.id();
    PostNotifyError(bitstream_buffer.id(), INVALID_ARGUMENT);
    return;
  }

  // Validate output video frame.
  if (!video_frame->IsMappable() && !video_frame->HasDmaBufs()) {
    VLOGF(1) << "Unsupported output frame storage type";
    PostNotifyError(bitstream_buffer.id(), INVALID_ARGUMENT);
    return;
  }
  if ((video_frame->visible_rect().width() & 1) ||
      (video_frame->visible_rect().height() & 1)) {
    VLOGF(1) << "Output frame visible size has odd dimension";
    PostNotifyError(bitstream_buffer.id(), PLATFORM_FAILURE);
    return;
  }

  std::unique_ptr<JobRecord> job_record(new JobRecordBitstreamBuffer(
      std::move(bitstream_buffer), std::move(video_frame)));

  decoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V4L2MjpegDecodeAccelerator::DecodeTask,
                                base::Unretained(this), std::move(job_record)));
}

void V4L2MjpegDecodeAccelerator::Decode(
    int32_t task_id,
    base::ScopedFD src_dmabuf_fd,
    size_t src_size,
    off_t src_offset,
    scoped_refptr<media::VideoFrame> dst_frame) {
  DVLOGF(4) << "task_id=" << task_id;
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (task_id < 0) {
    VLOGF(1) << "Invalid task id: " << task_id;
    PostNotifyError(task_id, INVALID_ARGUMENT);
    return;
  }

  // Validate input arguments.
  if (!src_dmabuf_fd.is_valid()) {
    VLOGF(1) << "Invalid input buffer FD";
    PostNotifyError(task_id, INVALID_ARGUMENT);
    return;
  }
  if (src_size == 0) {
    VLOGF(1) << "Input buffer size is zero";
    PostNotifyError(task_id, INVALID_ARGUMENT);
    return;
  }
  const size_t page_size = base::GetPageSize();
  if (src_offset < 0 || src_offset % page_size != 0) {
    VLOGF(1) << "Input buffer offset (" << src_offset
             << ") should be non-negative and aligned to page size ("
             << page_size << ")";
    PostNotifyError(task_id, INVALID_ARGUMENT);
    return;
  }

  // Validate output video frame.
  if (!dst_frame->IsMappable() && !dst_frame->HasDmaBufs()) {
    VLOGF(1) << "Unsupported output frame storage type";
    PostNotifyError(task_id, INVALID_ARGUMENT);
    return;
  }
  if ((dst_frame->visible_rect().width() & 1) ||
      (dst_frame->visible_rect().height() & 1)) {
    VLOGF(1) << "Output frame visible size has odd dimension";
    PostNotifyError(task_id, PLATFORM_FAILURE);
    return;
  }

  std::unique_ptr<JobRecord> job_record(
      new JobRecordDmaBuf(task_id, std::move(src_dmabuf_fd), src_size,
                          src_offset, std::move(dst_frame)));

  decoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V4L2MjpegDecodeAccelerator::DecodeTask,
                                base::Unretained(this), std::move(job_record)));
}

// static
bool V4L2MjpegDecodeAccelerator::IsSupported() {
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (!device)
    return false;

  return device->IsJpegDecodingSupported();
}

void V4L2MjpegDecodeAccelerator::DecodeTask(
    std::unique_ptr<JobRecord> job_record) {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  if (!job_record->map()) {
    VPLOGF(1) << "could not map input buffer";
    PostNotifyError(job_record->task_id(), UNREADABLE_INPUT);
    return;
  }
  input_jobs_.push(std::move(job_record));

  ServiceDeviceTask(false);
}

size_t V4L2MjpegDecodeAccelerator::InputBufferQueuedCount() {
  return input_buffer_map_.size() - free_input_buffers_.size();
}

size_t V4L2MjpegDecodeAccelerator::OutputBufferQueuedCount() {
  return output_buffer_map_.size() - free_output_buffers_.size();
}

bool V4L2MjpegDecodeAccelerator::ShouldRecreateInputBuffers() {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  if (input_jobs_.empty())
    return false;

  JobRecord* job_record = input_jobs_.front().get();
  // Check input buffer size is enough
  // TODO(kamesan): use safe arithmetic to handle overflows.
  return (input_buffer_map_.empty() ||
          (job_record->size() + sizeof(kDefaultDhtSeg)) >
              input_buffer_map_.front().length[0]);
}

bool V4L2MjpegDecodeAccelerator::RecreateInputBuffers() {
  VLOGF(2);
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());

  // If running queue is not empty, we should wait until pending frames finish.
  if (!running_jobs_.empty())
    return true;

  DestroyInputBuffers();

  if (!CreateInputBuffers()) {
    VLOGF(1) << "Create input buffers failed.";
    return false;
  }

  return true;
}

bool V4L2MjpegDecodeAccelerator::RecreateOutputBuffers() {
  VLOGF(2);
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());

  DestroyOutputBuffers();

  if (!CreateOutputBuffers()) {
    VLOGF(1) << "Create output buffers failed.";
    return false;
  }

  return true;
}

bool V4L2MjpegDecodeAccelerator::CreateInputBuffers() {
  VLOGF(2);
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  DCHECK(!input_streamon_);
  DCHECK(!input_jobs_.empty());
  JobRecord* job_record = input_jobs_.front().get();
  // The input image may miss huffman table. We didn't parse the image before,
  // so we create more to avoid the situation of not enough memory.
  // Reserve twice size to avoid recreating input buffer frequently.
  // TODO(kamesan): use safe arithmetic to handle overflows.
  size_t reserve_size = (job_record->size() + sizeof(kDefaultDhtSeg)) * 2;
  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  format.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_JPEG;
  format.fmt.pix_mp.plane_fmt[0].sizeimage = reserve_size;
  format.fmt.pix_mp.field = V4L2_FIELD_ANY;
  format.fmt.pix_mp.num_planes = kMaxInputPlanes;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_FMT, &format);
  DCHECK_EQ(format.fmt.pix_mp.pixelformat, V4L2_PIX_FMT_JPEG);

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
    struct v4l2_plane planes[VIDEO_MAX_PLANES];
    memset(&buffer, 0, sizeof(buffer));
    memset(planes, 0, sizeof(planes));
    buffer.index = i;
    buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    buffer.m.planes = planes;
    buffer.length = base::size(planes);
    buffer.memory = V4L2_MEMORY_MMAP;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QUERYBUF, &buffer);
    if (buffer.length != kMaxInputPlanes) {
      return false;
    }
    for (size_t j = 0; j < buffer.length; ++j) {
      void* address =
          device_->Mmap(NULL, planes[j].length, PROT_READ | PROT_WRITE,
                        MAP_SHARED, planes[j].m.mem_offset);
      if (address == MAP_FAILED) {
        VPLOGF(1) << "mmap() failed";
        PostNotifyError(kInvalidTaskId, PLATFORM_FAILURE);
        return false;
      }
      input_buffer_map_[i].address[j] = address;
      input_buffer_map_[i].length[j] = planes[j].length;
    }
  }

  return true;
}

bool V4L2MjpegDecodeAccelerator::CreateOutputBuffers() {
  VLOGF(2);
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  DCHECK(!output_streamon_);
  DCHECK(!running_jobs_.empty());
  JobRecord* job_record = running_jobs_.front().get();

  size_t frame_size = VideoFrame::AllocationSize(
      PIXEL_FORMAT_I420, job_record->out_frame()->coded_size());
  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  format.fmt.pix_mp.width = job_record->out_frame()->coded_size().width();
  format.fmt.pix_mp.height = job_record->out_frame()->coded_size().height();
  format.fmt.pix_mp.num_planes = 1;
  format.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV420;
  format.fmt.pix_mp.plane_fmt[0].sizeimage = frame_size;
  format.fmt.pix_mp.field = V4L2_FIELD_ANY;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_FMT, &format);
  output_buffer_pixelformat_ = format.fmt.pix_mp.pixelformat;
  output_buffer_coded_size_.SetSize(format.fmt.pix_mp.width,
                                    format.fmt.pix_mp.height);
  output_buffer_num_planes_ = format.fmt.pix_mp.num_planes;
  for (size_t i = 0; i < output_buffer_num_planes_; ++i)
    output_strides_[i] = format.fmt.pix_mp.plane_fmt[i].bytesperline;

  VideoPixelFormat output_format =
      Fourcc::FromV4L2PixFmt(output_buffer_pixelformat_).ToVideoPixelFormat();
  if (output_format == PIXEL_FORMAT_UNKNOWN) {
    VLOGF(1) << "unknown V4L2 pixel format: "
             << FourccToString(output_buffer_pixelformat_);
    PostNotifyError(kInvalidTaskId, PLATFORM_FAILURE);
    return false;
  }

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
    struct v4l2_plane planes[VIDEO_MAX_PLANES];
    memset(&buffer, 0, sizeof(buffer));
    memset(planes, 0, sizeof(planes));
    buffer.index = i;
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.m.planes = planes;
    buffer.length = base::size(planes);
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QUERYBUF, &buffer);

    if (output_buffer_num_planes_ != buffer.length) {
      return false;
    }
    for (size_t j = 0; j < buffer.length; ++j) {
      if (base::checked_cast<int64_t>(planes[j].length) <
          VideoFrame::PlaneSize(
              output_format, j,
              gfx::Size(format.fmt.pix_mp.width, format.fmt.pix_mp.height))
              .GetArea()) {
        return false;
      }
      void* address =
          device_->Mmap(NULL, planes[j].length, PROT_READ | PROT_WRITE,
                        MAP_SHARED, planes[j].m.mem_offset);
      if (address == MAP_FAILED) {
        VPLOGF(1) << "mmap() failed";
        PostNotifyError(kInvalidTaskId, PLATFORM_FAILURE);
        return false;
      }
      output_buffer_map_[i].address[j] = address;
      output_buffer_map_[i].length[j] = planes[j].length;
    }
  }

  return true;
}

void V4L2MjpegDecodeAccelerator::DestroyInputBuffers() {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());

  free_input_buffers_.clear();

  if (input_buffer_map_.empty())
    return;

  if (input_streamon_) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMOFF, &type);
    input_streamon_ = false;
  }

  for (const auto& input_record : input_buffer_map_) {
    for (size_t i = 0; i < kMaxInputPlanes; ++i) {
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
}

void V4L2MjpegDecodeAccelerator::DestroyOutputBuffers() {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());

  free_output_buffers_.clear();

  if (output_buffer_map_.empty())
    return;

  if (output_streamon_) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMOFF, &type);
    output_streamon_ = false;
  }

  for (const auto& output_record : output_buffer_map_) {
    for (size_t i = 0; i < output_buffer_num_planes_; ++i) {
      device_->Munmap(output_record.address[i], output_record.length[i]);
    }
  }

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  IOCTL_OR_LOG_ERROR(VIDIOC_REQBUFS, &reqbufs);

  output_buffer_map_.clear();
  output_buffer_num_planes_ = 0;
}

void V4L2MjpegDecodeAccelerator::DevicePollTask() {
  DCHECK(device_poll_task_runner_->BelongsToCurrentThread());

  bool event_pending;
  if (!device_->Poll(true, &event_pending)) {
    VPLOGF(1) << "Poll device error.";
    PostNotifyError(kInvalidTaskId, PLATFORM_FAILURE);
    return;
  }

  // All processing should happen on ServiceDeviceTask(), since we shouldn't
  // touch decoder state from this thread.
  decoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V4L2MjpegDecodeAccelerator::ServiceDeviceTask,
                                base::Unretained(this), event_pending));
}

bool V4L2MjpegDecodeAccelerator::DequeueSourceChangeEvent() {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());

  struct v4l2_event ev;
  memset(&ev, 0, sizeof(ev));

  if (device_->Ioctl(VIDIOC_DQEVENT, &ev) == 0) {
    if (ev.type == V4L2_EVENT_SOURCE_CHANGE) {
      VLOGF(2) << ": got source change event: " << ev.u.src_change.changes;
      if (ev.u.src_change.changes &
          (V4L2_EVENT_SRC_CH_RESOLUTION | V4L2_EVENT_SRC_CH_PIXELFORMAT)) {
        return true;
      }
      VLOGF(1) << "unexpected source change event.";
    } else {
      VLOGF(1) << "got an event (" << ev.type << ") we haven't subscribed to.";
    }
  } else {
    VLOGF(1) << "dequeue event failed.";
  }
  PostNotifyError(kInvalidTaskId, PLATFORM_FAILURE);
  return false;
}

void V4L2MjpegDecodeAccelerator::ServiceDeviceTask(bool event_pending) {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  // If DestroyTask() shuts |device_poll_thread_| down, we should early-out.
  if (!device_poll_thread_.IsRunning())
    return;

  if (!running_jobs_.empty())
    Dequeue();

  if (ShouldRecreateInputBuffers() && !RecreateInputBuffers())
    return;

  if (event_pending) {
    if (!DequeueSourceChangeEvent())
      return;
    if (!RecreateOutputBuffers())
      return;
  }

  EnqueueInput();
  EnqueueOutput();

  if (!running_jobs_.empty()) {
    device_poll_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&V4L2MjpegDecodeAccelerator::DevicePollTask,
                                  base::Unretained(this)));
  }

  DVLOGF(3) << "buffer counts: INPUT[" << input_jobs_.size() << "] => DEVICE["
            << free_input_buffers_.size() << "/" << input_buffer_map_.size()
            << "->" << free_output_buffers_.size() << "/"
            << output_buffer_map_.size() << "]";
}

void V4L2MjpegDecodeAccelerator::EnqueueInput() {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  while (!input_jobs_.empty() && !free_input_buffers_.empty()) {
    // If input buffers are required to re-create, do not enqueue input record
    // until all pending frames are handled by device.
    if (ShouldRecreateInputBuffers())
      break;
    if (!EnqueueInputRecord())
      return;
  }
  // Check here because we cannot STREAMON before QBUF in earlier kernel.
  // (kernel version < 3.14)
  if (!input_streamon_ && InputBufferQueuedCount()) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMON, &type);
    input_streamon_ = true;
  }
}

void V4L2MjpegDecodeAccelerator::EnqueueOutput() {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  // Output record can be enqueued because the output coded sizes of the frames
  // currently in the pipeline are all the same.
  while (running_jobs_.size() > OutputBufferQueuedCount() &&
         !free_output_buffers_.empty()) {
    if (!EnqueueOutputRecord())
      return;
  }
  // Check here because we cannot STREAMON before QBUF in earlier kernel.
  // (kernel version < 3.14)
  if (!output_streamon_ && OutputBufferQueuedCount()) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMON, &type);
    output_streamon_ = true;
  }
}

bool V4L2MjpegDecodeAccelerator::ConvertOutputImage(
    const BufferRecord& output_buffer,
    scoped_refptr<VideoFrame> dst_frame) {
  // The coded size of the hardware buffer should be at least as large as the
  // video frame's visible size.
  const int dst_width = dst_frame->visible_rect().width();
  const int dst_height = dst_frame->visible_rect().height();
  DCHECK_GE(output_buffer_coded_size_.width(), dst_width);
  DCHECK_GE(output_buffer_coded_size_.height(), dst_height);

  // Dmabuf-backed frame needs to be mapped for SW access.
  if (dst_frame->HasDmaBufs()) {
    std::unique_ptr<VideoFrameMapper> frame_mapper =
        VideoFrameMapperFactory::CreateMapper(dst_frame->format(),
                                              VideoFrame::STORAGE_DMABUFS);
    if (!frame_mapper) {
      VLOGF(1) << "Failed to create video frame mapper";
      return false;
    }
    dst_frame = frame_mapper->Map(std::move(dst_frame));
    if (!dst_frame) {
      VLOGF(1) << "Failed to map DMA-buf video frame";
      return false;
    }
  }

  // Extract destination pointers and strides.
  std::array<uint8_t*, VideoFrame::kMaxPlanes> dst_ptrs{};
  std::array<int, VideoFrame::kMaxPlanes> dst_strides{};
  for (size_t i = 0; i < dst_frame->layout().num_planes(); i++) {
    dst_ptrs[i] = dst_frame->visible_data(i);
    dst_strides[i] = base::checked_cast<int>(dst_frame->stride(i));
  }

  // Use ConvertToI420 to convert all splane formats to I420.
  if (output_buffer_num_planes_ == 1 &&
      dst_frame->format() == PIXEL_FORMAT_I420) {
    DCHECK_EQ(dst_frame->layout().num_planes(), 3u);
    const VideoPixelFormat format =
        Fourcc::FromV4L2PixFmt(output_buffer_pixelformat_).ToVideoPixelFormat();
    if (format == PIXEL_FORMAT_UNKNOWN) {
      VLOGF(1) << "Unknown V4L2 format: "
               << FourccToString(output_buffer_pixelformat_);
      return false;
    }
    const size_t src_size =
        VideoFrame::AllocationSize(format, output_buffer_coded_size_);
    if (libyuv::ConvertToI420(
            static_cast<uint8_t*>(output_buffer.address[0]), src_size,
            dst_ptrs[0], dst_strides[0], dst_ptrs[1], dst_strides[1],
            dst_ptrs[2], dst_strides[2], 0 /*x*/, 0 /*y*/,
            output_buffer_coded_size_.width(),
            output_buffer_coded_size_.height(), dst_width, dst_height,
            libyuv::kRotate0, output_buffer_pixelformat_)) {
      VLOGF(1) << "ConvertToI420 failed. Source format: "
               << FourccToString(output_buffer_pixelformat_);
      return false;
    }
    return true;
  }

  // Extract source pointers and strides.
  std::array<const uint8_t*, VideoFrame::kMaxPlanes> src_ptrs{};
  std::array<int, VideoFrame::kMaxPlanes> src_strides{};
  for (size_t i = 0; i < output_buffer_num_planes_; i++) {
    src_ptrs[i] = static_cast<uint8_t*>(output_buffer.address[i]);
    src_strides[i] = output_strides_[i];
  }

  if (output_buffer_pixelformat_ == V4L2_PIX_FMT_YUV420M) {
    DCHECK_EQ(output_buffer_num_planes_, 3u);
    switch (dst_frame->format()) {
      case PIXEL_FORMAT_I420:
        DCHECK_EQ(dst_frame->layout().num_planes(), 3u);
        if (libyuv::I420Copy(src_ptrs[0], src_strides[0], src_ptrs[1],
                             src_strides[1], src_ptrs[2], src_strides[2],
                             dst_ptrs[0], dst_strides[0], dst_ptrs[1],
                             dst_strides[1], dst_ptrs[2], dst_strides[2],
                             dst_width, dst_height)) {
          VLOGF(1) << "I420Copy failed";
          return false;
        }
        break;
      case PIXEL_FORMAT_YV12:
        DCHECK_EQ(dst_frame->layout().num_planes(), 3u);
        if (libyuv::I420Copy(src_ptrs[0], src_strides[0], src_ptrs[1],
                             src_strides[1], src_ptrs[2], src_strides[2],
                             dst_ptrs[0], dst_strides[0], dst_ptrs[2],
                             dst_strides[2], dst_ptrs[1], dst_strides[1],
                             dst_width, dst_height)) {
          VLOGF(1) << "I420Copy failed";
          return false;
        }
        break;
      case PIXEL_FORMAT_NV12:
        DCHECK_EQ(dst_frame->layout().num_planes(), 2u);
        if (libyuv::I420ToNV12(src_ptrs[0], src_strides[0], src_ptrs[1],
                               src_strides[1], src_ptrs[2], src_strides[2],
                               dst_ptrs[0], dst_strides[0], dst_ptrs[1],
                               dst_strides[1], dst_width, dst_height)) {
          VLOGF(1) << "I420ToNV12 failed";
          return false;
        }
        break;
      default:
        VLOGF(1) << "Can't convert image from I420 to " << dst_frame->format();
        return false;
    }
  } else if (output_buffer_pixelformat_ == V4L2_PIX_FMT_YUV422M) {
    DCHECK_EQ(output_buffer_num_planes_, 3u);
    switch (dst_frame->format()) {
      case PIXEL_FORMAT_I420:
        DCHECK_EQ(dst_frame->layout().num_planes(), 3u);
        if (libyuv::I422ToI420(src_ptrs[0], src_strides[0], src_ptrs[1],
                               src_strides[1], src_ptrs[2], src_strides[2],
                               dst_ptrs[0], dst_strides[0], dst_ptrs[1],
                               dst_strides[1], dst_ptrs[2], dst_strides[2],
                               dst_width, dst_height)) {
          VLOGF(1) << "I422ToI420 failed";
          return false;
        }
        break;
      case PIXEL_FORMAT_YV12:
        DCHECK_EQ(dst_frame->layout().num_planes(), 3u);
        if (libyuv::I422ToI420(src_ptrs[0], src_strides[0], src_ptrs[1],
                               src_strides[1], src_ptrs[2], src_strides[2],
                               dst_ptrs[0], dst_strides[0], dst_ptrs[2],
                               dst_strides[2], dst_ptrs[1], dst_strides[1],
                               dst_width, dst_height)) {
          VLOGF(1) << "I422ToI420 failed";
          return false;
        }
        break;
      case PIXEL_FORMAT_NV12:
        DCHECK_EQ(dst_frame->layout().num_planes(), 2u);
        if (libyuv::I422ToNV21(src_ptrs[0], src_strides[0], src_ptrs[2],
                               src_strides[2], src_ptrs[1], src_strides[1],
                               dst_ptrs[0], dst_strides[0], dst_ptrs[1],
                               dst_strides[1], dst_width, dst_height)) {
          VLOGF(1) << "I422ToNV21 failed";
          return false;
        }
        break;
      default:
        VLOGF(1) << "Can't convert image from I422 to " << dst_frame->format();
        return false;
    }
  } else {
    VLOGF(1) << "Unsupported source buffer format: "
             << FourccToString(output_buffer_pixelformat_);
    return false;
  }
  return true;
}

void V4L2MjpegDecodeAccelerator::Dequeue() {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());

  // Dequeue completed input (VIDEO_OUTPUT) buffers,
  // and recycle to the free list.
  struct v4l2_buffer dqbuf;
  struct v4l2_plane planes[VIDEO_MAX_PLANES];
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
      PostNotifyError(kInvalidTaskId, PLATFORM_FAILURE);
      return;
    }
    BufferRecord& input_record = input_buffer_map_[dqbuf.index];
    DCHECK(input_record.at_device);
    input_record.at_device = false;
    free_input_buffers_.push_back(dqbuf.index);

    if (dqbuf.flags & V4L2_BUF_FLAG_ERROR) {
      VLOGF(1) << "Error in dequeued input buffer.";
      PostNotifyError(kInvalidTaskId, UNSUPPORTED_JPEG);
      running_jobs_.pop();
    }
  }

  // Dequeue completed output (VIDEO_CAPTURE) buffers, recycle to the free list.
  // Return the finished buffer to the client via the job ready callback.
  // If dequeued input buffer has an error, the error frame has removed from
  // |running_jobs_|. We only have to dequeue output buffer when we actually
  // have pending frames in |running_jobs_| and also enqueued output buffers.
  while (!running_jobs_.empty() && OutputBufferQueuedCount() > 0) {
    DCHECK(output_streamon_);
    memset(&dqbuf, 0, sizeof(dqbuf));
    memset(planes, 0, sizeof(planes));
    dqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    // From experiments, using MMAP and memory copy is still faster than
    // USERPTR. Also, client doesn't need to consider the buffer alignment and
    // MjpegDecodeAccelerator API will be simpler.
    dqbuf.memory = V4L2_MEMORY_MMAP;
    dqbuf.length = base::size(planes);
    dqbuf.m.planes = planes;
    if (device_->Ioctl(VIDIOC_DQBUF, &dqbuf) != 0) {
      if (errno == EAGAIN) {
        // EAGAIN if we're just out of buffers to dequeue.
        break;
      }
      VPLOGF(1) << "ioctl() failed: output buffer VIDIOC_DQBUF failed.";
      PostNotifyError(kInvalidTaskId, PLATFORM_FAILURE);
      return;
    }
    BufferRecord& output_record = output_buffer_map_[dqbuf.index];
    DCHECK(output_record.at_device);
    output_record.at_device = false;
    free_output_buffers_.push_back(dqbuf.index);

    // Jobs are always processed in FIFO order.
    std::unique_ptr<JobRecord> job_record = std::move(running_jobs_.front());
    running_jobs_.pop();

    if (dqbuf.flags & V4L2_BUF_FLAG_ERROR) {
      VLOGF(1) << "Error in dequeued output buffer.";
      PostNotifyError(kInvalidTaskId, UNSUPPORTED_JPEG);
    } else {
      // Copy the decoded data from output buffer to the buffer provided by the
      // client. Do format conversion when output format is not
      // V4L2_PIX_FMT_YUV420.
      if (!ConvertOutputImage(output_record, job_record->out_frame())) {
        PostNotifyError(job_record->task_id(), PLATFORM_FAILURE);
        return;
      }
      DVLOGF(4) << "Decoding finished, returning bitstream buffer, id="
                << job_record->task_id();

      // Destroy |job_record| before posting VideoFrameReady to the client to
      // prevent race condition on the buffers.
      const int32_t task_id = job_record->task_id();
      job_record.reset();
      child_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&V4L2MjpegDecodeAccelerator::VideoFrameReady,
                         weak_ptr_, task_id));
    }
  }
}

static bool AddHuffmanTable(const void* input_ptr,
                            size_t input_size,
                            void* output_ptr,
                            size_t output_size) {
  DCHECK(input_ptr);
  DCHECK(output_ptr);
  DCHECK_LE((input_size + sizeof(kDefaultDhtSeg)), output_size);

  base::BigEndianReader reader(static_cast<const char*>(input_ptr), input_size);
  bool has_marker_dht = false;
  bool has_marker_sos = false;
  uint8_t marker1, marker2;
  READ_U8_OR_RETURN_FALSE(reader, &marker1);
  READ_U8_OR_RETURN_FALSE(reader, &marker2);
  if (marker1 != JPEG_MARKER_PREFIX || marker2 != JPEG_SOI) {
    DVLOGF(1) << "The input is not a Jpeg";
    return false;
  }

  // copy SOI marker (0xFF, 0xD8)
  memcpy(output_ptr, input_ptr, 2);
  size_t current_offset = 2;

  while (!has_marker_sos && !has_marker_dht) {
    const char* start_addr = reader.ptr();
    READ_U8_OR_RETURN_FALSE(reader, &marker1);
    if (marker1 != JPEG_MARKER_PREFIX) {
      DVLOGF(1) << "marker1 != 0xFF";
      return false;
    }
    do {
      READ_U8_OR_RETURN_FALSE(reader, &marker2);
    } while (marker2 == JPEG_MARKER_PREFIX);  // skip fill bytes

    uint16_t size;
    READ_U16_OR_RETURN_FALSE(reader, &size);
    // The size includes the size field itself.
    if (size < sizeof(size)) {
      DVLOGF(1) << ": Ill-formed JPEG. Segment size (" << size
                << ") is smaller than size field (" << sizeof(size) << ")";
      return false;
    }
    size -= sizeof(size);

    switch (marker2) {
      case JPEG_DHT: {
        has_marker_dht = true;
        break;
      }
      case JPEG_SOS: {
        if (!has_marker_dht) {
          memcpy(static_cast<uint8_t*>(output_ptr) + current_offset,
                 kDefaultDhtSeg, sizeof(kDefaultDhtSeg));
          current_offset += sizeof(kDefaultDhtSeg);
        }
        has_marker_sos = true;
        break;
      }
      default:
        break;
    }

    if (!reader.Skip(size)) {
      DVLOGF(1) << "Ill-formed JPEG. Remaining size (" << reader.remaining()
                << ") is smaller than header specified (" << size << ")";
      return false;
    }

    size_t segment_size = static_cast<size_t>(reader.ptr() - start_addr);
    memcpy(static_cast<uint8_t*>(output_ptr) + current_offset, start_addr,
           segment_size);
    current_offset += segment_size;
  }
  if (reader.remaining()) {
    memcpy(static_cast<uint8_t*>(output_ptr) + current_offset, reader.ptr(),
           reader.remaining());
  }
  return true;
}

bool V4L2MjpegDecodeAccelerator::EnqueueInputRecord() {
  DCHECK(!input_jobs_.empty());
  DCHECK(!free_input_buffers_.empty());

  // Enqueue an input (VIDEO_OUTPUT) buffer for an input video frame.
  std::unique_ptr<JobRecord> job_record = std::move(input_jobs_.front());
  input_jobs_.pop();
  const int index = free_input_buffers_.back();
  BufferRecord& input_record = input_buffer_map_[index];
  DCHECK(!input_record.at_device);

  // It will add default huffman segment if it's missing.
  if (!AddHuffmanTable(job_record->memory(), job_record->size(),
                       input_record.address[0], input_record.length[0])) {
    PostNotifyError(job_record->task_id(), PARSE_JPEG_FAILED);
    return false;
  }

  struct v4l2_buffer qbuf;
  struct v4l2_plane planes[VIDEO_MAX_PLANES];
  memset(&qbuf, 0, sizeof(qbuf));
  memset(planes, 0, sizeof(planes));
  qbuf.index = index;
  qbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  qbuf.memory = V4L2_MEMORY_MMAP;
  qbuf.length = base::size(planes);
  // There is only one plane for V4L2_PIX_FMT_JPEG.
  planes[0].bytesused = input_record.length[0];
  qbuf.m.planes = planes;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QBUF, &qbuf);
  input_record.at_device = true;

  DVLOGF(3) << "enqueued frame id=" << job_record->task_id() << " to device.";
  running_jobs_.push(std::move(job_record));
  free_input_buffers_.pop_back();
  return true;
}

bool V4L2MjpegDecodeAccelerator::EnqueueOutputRecord() {
  DCHECK(!free_output_buffers_.empty());
  DCHECK_GT(output_buffer_num_planes_, 0u);

  // Enqueue an output (VIDEO_CAPTURE) buffer.
  const int index = free_output_buffers_.back();
  BufferRecord& output_record = output_buffer_map_[index];
  DCHECK(!output_record.at_device);
  struct v4l2_buffer qbuf;
  struct v4l2_plane planes[VIDEO_MAX_PLANES];
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

void V4L2MjpegDecodeAccelerator::StartDevicePoll() {
  DVLOGF(3) << ": starting device poll";
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  DCHECK(!device_poll_thread_.IsRunning());

  if (!device_poll_thread_.Start()) {
    VLOGF(1) << "Device thread failed to start";
    PostNotifyError(kInvalidTaskId, PLATFORM_FAILURE);
    return;
  }
  device_poll_task_runner_ = device_poll_thread_.task_runner();
}

bool V4L2MjpegDecodeAccelerator::StopDevicePoll() {
  DVLOGF(3) << "stopping device poll";
  // Signal the DevicePollTask() to stop, and stop the device poll thread.
  if (!device_->SetDevicePollInterrupt()) {
    VLOGF(1) << "SetDevicePollInterrupt failed.";
    PostNotifyError(kInvalidTaskId, PLATFORM_FAILURE);
    return false;
  }

  device_poll_thread_.Stop();

  // Clear the interrupt now, to be sure.
  if (!device_->ClearDevicePollInterrupt())
    return false;

  return true;
}

}  // namespace media
