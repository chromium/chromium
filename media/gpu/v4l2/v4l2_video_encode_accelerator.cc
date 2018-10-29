// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_video_encode_accelerator.h"

#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <utility>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/scopedfd_helper.h"
#include "media/base/unaligned_shared_memory.h"
#include "media/base/video_types.h"
#include "media/gpu/gpu_video_encode_accelerator_helpers.h"
#include "media/gpu/v4l2/v4l2_image_processor.h"
#include "media/video/h264_parser.h"

#define VLOGF(level) VLOG(level) << __func__ << "(): "
#define DVLOGF(level) DVLOG(level) << __func__ << "(): "
#define VPLOGF(level) VPLOG(level) << __func__ << "(): "

#define NOTIFY_ERROR(x)                      \
  do {                                       \
    VLOGF(1) << "Setting error state:" << x; \
    SetErrorState(x);                        \
  } while (0)

#define IOCTL_OR_ERROR_RETURN_VALUE(type, arg, value, type_str) \
  do {                                                          \
    if (device_->Ioctl(type, arg) != 0) {                       \
      VPLOGF(1) << "ioctl() failed: " << type_str;              \
      NOTIFY_ERROR(kPlatformFailureError);                      \
      return value;                                             \
    }                                                           \
  } while (0)

#define IOCTL_OR_ERROR_RETURN(type, arg) \
  IOCTL_OR_ERROR_RETURN_VALUE(type, arg, ((void)0), #type)

#define IOCTL_OR_ERROR_RETURN_FALSE(type, arg) \
  IOCTL_OR_ERROR_RETURN_VALUE(type, arg, false, #type)

#define IOCTL_OR_LOG_ERROR(type, arg)              \
  do {                                             \
    if (device_->Ioctl(type, arg) != 0)            \
      VPLOGF(1) << "ioctl() failed: " << #type;    \
  } while (0)

namespace {
const uint8_t kH264StartCode[] = {0, 0, 0, 1};
const size_t kH264StartCodeSize = sizeof(kH264StartCode);

// Copy a H.264 NALU of size |src_size| (without start code), located at |src|,
// into a buffer starting at |dst| of size |dst_size|, prepending it with
// a H.264 start code (as long as both fit). After copying, update |dst| to
// point to the address immediately after the copied data, and update |dst_size|
// to contain remaining destination buffer size.
static void CopyNALUPrependingStartCode(const uint8_t* src,
                                        size_t src_size,
                                        uint8_t** dst,
                                        size_t* dst_size) {
  size_t size_to_copy = kH264StartCodeSize + src_size;
  if (size_to_copy > *dst_size) {
    VLOGF(1) << "Could not copy a NALU, not enough space in destination buffer";
    return;
  }

  memcpy(*dst, kH264StartCode, kH264StartCodeSize);
  memcpy(*dst + kH264StartCodeSize, src, src_size);

  *dst += size_to_copy;
  *dst_size -= size_to_copy;
}
}  // namespace

namespace media {

struct V4L2VideoEncodeAccelerator::BitstreamBufferRef {
  BitstreamBufferRef(int32_t id, std::unique_ptr<UnalignedSharedMemory> shm)
      : id(id), shm(std::move(shm)) {}
  const int32_t id;
  const std::unique_ptr<UnalignedSharedMemory> shm;
};

V4L2VideoEncodeAccelerator::InputRecord::InputRecord() : at_device(false) {}

V4L2VideoEncodeAccelerator::InputRecord::InputRecord(const InputRecord&) =
    default;

V4L2VideoEncodeAccelerator::InputRecord::~InputRecord() {}

V4L2VideoEncodeAccelerator::OutputRecord::OutputRecord()
    : at_device(false), address(nullptr), length(0) {}

V4L2VideoEncodeAccelerator::OutputRecord::~OutputRecord() {}

V4L2VideoEncodeAccelerator::InputFrameInfo::InputFrameInfo()
    : InputFrameInfo(nullptr, false) {}

V4L2VideoEncodeAccelerator::InputFrameInfo::InputFrameInfo(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe)
    : frame(frame), force_keyframe(force_keyframe) {}

V4L2VideoEncodeAccelerator::InputFrameInfo::InputFrameInfo(
    const InputFrameInfo&) = default;

V4L2VideoEncodeAccelerator::InputFrameInfo::~InputFrameInfo() {}

V4L2VideoEncodeAccelerator::V4L2VideoEncodeAccelerator(
    const scoped_refptr<V4L2Device>& device)
    : child_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      output_buffer_byte_size_(0),
      device_input_format_(PIXEL_FORMAT_UNKNOWN),
      input_planes_count_(0),
      output_format_fourcc_(0),
      encoder_state_(kUninitialized),
      device_(device),
      input_streamon_(false),
      input_buffer_queued_count_(0),
      input_memory_type_(V4L2_MEMORY_USERPTR),
      output_streamon_(false),
      output_buffer_queued_count_(0),
      is_flush_supported_(false),
      encoder_thread_("V4L2EncoderThread"),
      device_poll_thread_("V4L2EncoderDevicePollThread"),
      weak_this_ptr_factory_(this) {
  weak_this_ = weak_this_ptr_factory_.GetWeakPtr();
}

V4L2VideoEncodeAccelerator::~V4L2VideoEncodeAccelerator() {
  DCHECK(!encoder_thread_.IsRunning());
  DCHECK(!device_poll_thread_.IsRunning());
  VLOGF(2);

  DestroyInputBuffers();
  DestroyOutputBuffers();
}

bool V4L2VideoEncodeAccelerator::Initialize(const Config& config,
                                            Client* client) {
  VLOGF(2) << ": " << config.AsHumanReadableString();

  visible_size_ = config.input_visible_size;

  client_ptr_factory_.reset(new base::WeakPtrFactory<Client>(client));
  client_ = client_ptr_factory_->GetWeakPtr();

  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(encoder_state_, kUninitialized);

  output_format_fourcc_ =
      V4L2Device::VideoCodecProfileToV4L2PixFmt(config.output_profile, false);
  if (!output_format_fourcc_) {
    VLOGF(1) << "invalid output_profile="
             << GetProfileName(config.output_profile);
    return false;
  }

  if (!device_->Open(V4L2Device::Type::kEncoder, output_format_fourcc_)) {
    VLOGF(1) << "Failed to open device for profile="
             << GetProfileName(config.output_profile)
             << ", fourcc=" << FourccToString(output_format_fourcc_);
    return false;
  }

  // Ask if V4L2_ENC_CMD_STOP (Flush) is supported.
  struct v4l2_encoder_cmd cmd = {};
  cmd.cmd = V4L2_ENC_CMD_STOP;
  is_flush_supported_ = (device_->Ioctl(VIDIOC_TRY_ENCODER_CMD, &cmd) == 0);
  if (!is_flush_supported_)
    VLOGF(2) << "V4L2_ENC_CMD_STOP is not supported.";

  struct v4l2_capability caps;
  memset(&caps, 0, sizeof(caps));
  const __u32 kCapsRequired = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QUERYCAP, &caps);
  if ((caps.capabilities & kCapsRequired) != kCapsRequired) {
    VLOGF(1) << "caps check failed: 0x" << std::hex << caps.capabilities;
    return false;
  }

  if (!SetFormats(config.input_format, config.output_profile)) {
    VLOGF(1) << "Failed setting up formats";
    return false;
  }

  if (config.input_format != device_input_format_) {
    VLOGF(2) << "Input format not supported by the HW, will try to convert to "
             << VideoPixelFormatToString(device_input_format_);

    if (!V4L2ImageProcessor::IsSupported()) {
      VLOGF(1) << "Image processor not available";
      return false;
    }

    scoped_refptr<V4L2Device> device = V4L2Device::Create();
    image_processor_.reset(
        new V4L2ImageProcessor(device, V4L2_MEMORY_USERPTR, V4L2_MEMORY_MMAP));

    // Convert from |config.input_format| to |device_input_format_|, keeping the
    // size at |visible_size_| and requiring the output buffers to be of at
    // least |input_allocated_size_|. Unretained is safe because |this| owns
    // image processor and there will be no callbacks after processor destroys.
    if (!image_processor_->Initialize(
            config.input_format, device_input_format_, visible_size_,
            visible_size_, visible_size_, input_allocated_size_,
            kImageProcBufferCount,
            base::Bind(&V4L2VideoEncodeAccelerator::ImageProcessorError,
                       base::Unretained(this)))) {
      VLOGF(1) << "Failed initializing image processor";
      return false;
    }
    // The output of image processor is the input of encoder. Output coded
    // width of processor must be the same as input coded width of encoder.
    // Output coded height of processor can be larger but not smaller than the
    // input coded height of encoder. For example, suppose input size of encoder
    // is 320x193. It is OK if the output of processor is 320x208.
    if (image_processor_->output_allocated_size().width() !=
            input_allocated_size_.width() ||
        image_processor_->output_allocated_size().height() <
            input_allocated_size_.height()) {
      VLOGF(1) << "Invalid image processor output coded size "
               << image_processor_->output_allocated_size().ToString()
               << ", encode input coded size is "
               << input_allocated_size_.ToString();
      return false;
    }

    for (int i = 0; i < kImageProcBufferCount; i++)
      free_image_processor_output_buffers_.push_back(i);
  }

  if (!InitControls(config))
    return false;

  if (!CreateOutputBuffers())
    return false;

  if (!encoder_thread_.Start()) {
    VLOGF(1) << "encoder thread failed to start";
    return false;
  }

  RequestEncodingParametersChange(
      config.initial_bitrate, config.initial_framerate.value_or(
                                  VideoEncodeAccelerator::kDefaultFramerate));

  encoder_state_ = kInitialized;

  child_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Client::RequireBitstreamBuffers, client_, kInputBufferCount,
                 image_processor_.get()
                     ? image_processor_->input_allocated_size()
                     : input_allocated_size_,
                 output_buffer_byte_size_));
  return true;
}

void V4L2VideoEncodeAccelerator::ImageProcessorError() {
  VLOGF(1) << "Image processor error";
  NOTIFY_ERROR(kPlatformFailureError);
}

void V4L2VideoEncodeAccelerator::Encode(const scoped_refptr<VideoFrame>& frame,
                                        bool force_keyframe) {
  DVLOGF(4) << "force_keyframe=" << force_keyframe;
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  if (image_processor_) {
    if (free_image_processor_output_buffers_.size() > 0) {
      int output_buffer_index = free_image_processor_output_buffers_.back();
      free_image_processor_output_buffers_.pop_back();
      // Unretained is safe because |this| owns image processor and there will
      // be no callbacks after processor destroys.
      if (!image_processor_->Process(
              frame, output_buffer_index, std::vector<base::ScopedFD>(),
              base::BindOnce(&V4L2VideoEncodeAccelerator::FrameProcessed,
                             base::Unretained(this), force_keyframe,
                             frame->timestamp(), output_buffer_index))) {
        NOTIFY_ERROR(kPlatformFailureError);
      }
    } else {
      image_processor_input_queue_.emplace(frame, force_keyframe);
    }
  } else {
    encoder_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&V4L2VideoEncodeAccelerator::EncodeTask,
                       base::Unretained(this), frame, force_keyframe));
  }
}

void V4L2VideoEncodeAccelerator::UseOutputBitstreamBuffer(
    const BitstreamBuffer& buffer) {
  DVLOGF(4) << "id=" << buffer.id();
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  if (buffer.size() < output_buffer_byte_size_) {
    NOTIFY_ERROR(kInvalidArgumentError);
    return;
  }

  auto shm = std::make_unique<UnalignedSharedMemory>(buffer.handle(),
                                                     buffer.size(), false);
  if (!shm->MapAt(buffer.offset(), buffer.size())) {
    NOTIFY_ERROR(kPlatformFailureError);
    return;
  }

  std::unique_ptr<BitstreamBufferRef> buffer_ref(
      new BitstreamBufferRef(buffer.id(), std::move(shm)));
  encoder_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&V4L2VideoEncodeAccelerator::UseOutputBitstreamBufferTask,
                 base::Unretained(this), base::Passed(&buffer_ref)));
}

void V4L2VideoEncodeAccelerator::RequestEncodingParametersChange(
    uint32_t bitrate,
    uint32_t framerate) {
  VLOGF(2) << "bitrate=" << bitrate << ", framerate=" << framerate;
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  encoder_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(
          &V4L2VideoEncodeAccelerator::RequestEncodingParametersChangeTask,
          base::Unretained(this), bitrate, framerate));
}

void V4L2VideoEncodeAccelerator::Destroy() {
  VLOGF(2);
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  // We're destroying; cancel all callbacks.
  client_ptr_factory_.reset();
  weak_this_ptr_factory_.InvalidateWeakPtrs();

  image_processor_ = nullptr;

  // If the encoder thread is running, destroy using posted task.
  if (encoder_thread_.IsRunning()) {
    encoder_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&V4L2VideoEncodeAccelerator::DestroyTask,
                                  base::Unretained(this)));
    // DestroyTask() will put the encoder into kError state and cause all tasks
    // to no-op.
    encoder_thread_.Stop();
  } else {
    // Otherwise, call the destroy task directly.
    DestroyTask();
  }

  // If a flush is pending, notify client that it did not finish.
  if (flush_callback_)
    std::move(flush_callback_).Run(false);

  // Set to kError state just in case.
  encoder_state_ = kError;

  delete this;
}

void V4L2VideoEncodeAccelerator::Flush(FlushCallback flush_callback) {
  VLOGF(2);
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  encoder_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2VideoEncodeAccelerator::FlushTask,
                     base::Unretained(this), base::Passed(&flush_callback)));
}

void V4L2VideoEncodeAccelerator::FlushTask(FlushCallback flush_callback) {
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());

  if (flush_callback_ || encoder_state_ != kEncoding) {
    VLOGF(1) << "Flush failed: there is a pending flush, "
             << "or VEA is not in kEncoding state";
    NOTIFY_ERROR(kIllegalStateError);
    child_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(flush_callback), false));
    return;
  }
  flush_callback_ = std::move(flush_callback);
  // Push a null frame to indicate Flush.
  EncodeTask(nullptr, false);
}

bool V4L2VideoEncodeAccelerator::IsFlushSupported() {
  return is_flush_supported_;
}

VideoEncodeAccelerator::SupportedProfiles
V4L2VideoEncodeAccelerator::GetSupportedProfiles() {
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (!device)
    return SupportedProfiles();

  return device->GetSupportedEncodeProfiles();
}

void V4L2VideoEncodeAccelerator::FrameProcessed(
    bool force_keyframe,
    base::TimeDelta timestamp,
    int output_buffer_index,
    scoped_refptr<VideoFrame> frame) {
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DVLOGF(4) << "force_keyframe=" << force_keyframe
            << ", output_buffer_index=" << output_buffer_index;
  DCHECK_GE(output_buffer_index, 0);

  frame->AddDestructionObserver(BindToCurrentLoop(
      base::Bind(&V4L2VideoEncodeAccelerator::ReuseImageProcessorOutputBuffer,
                 weak_this_, output_buffer_index)));

  encoder_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2VideoEncodeAccelerator::EncodeTask,
                                base::Unretained(this), frame, force_keyframe));
}

void V4L2VideoEncodeAccelerator::ReuseImageProcessorOutputBuffer(
    int output_buffer_index) {
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DVLOGF(4) << "output_buffer_index=" << output_buffer_index;
  free_image_processor_output_buffers_.push_back(output_buffer_index);
  if (!image_processor_input_queue_.empty()) {
    InputFrameInfo frame_info = image_processor_input_queue_.front();
    image_processor_input_queue_.pop();
    Encode(frame_info.frame, frame_info.force_keyframe);
  }
}

size_t V4L2VideoEncodeAccelerator::CopyIntoOutputBuffer(
    const uint8_t* bitstream_data,
    size_t bitstream_size,
    std::unique_ptr<BitstreamBufferRef> buffer_ref) {
  uint8_t* dst_ptr = static_cast<uint8_t*>(buffer_ref->shm->memory());
  size_t remaining_dst_size = buffer_ref->shm->size();

  if (!inject_sps_and_pps_) {
    if (bitstream_size <= remaining_dst_size) {
      memcpy(dst_ptr, bitstream_data, bitstream_size);
      return bitstream_size;
    } else {
      VLOGF(1) << "Output data did not fit in the BitstreamBuffer";
      return 0;
    }
  }

  // Cache the newest SPS and PPS found in the stream, and inject them before
  // each IDR found.
  H264Parser parser;
  parser.SetStream(bitstream_data, bitstream_size);
  H264NALU nalu;

  while (parser.AdvanceToNextNALU(&nalu) == H264Parser::kOk) {
    // nalu.size is always without the start code, regardless of the NALU type.
    if (nalu.size + kH264StartCodeSize > remaining_dst_size) {
      VLOGF(1) << "Output data did not fit in the BitstreamBuffer";
      break;
    }

    switch (nalu.nal_unit_type) {
      case H264NALU::kSPS:
        cached_sps_.resize(nalu.size);
        memcpy(cached_sps_.data(), nalu.data, nalu.size);
        cached_h264_header_size_ =
            cached_sps_.size() + cached_pps_.size() + 2 * kH264StartCodeSize;
        break;

      case H264NALU::kPPS:
        cached_pps_.resize(nalu.size);
        memcpy(cached_pps_.data(), nalu.data, nalu.size);
        cached_h264_header_size_ =
            cached_sps_.size() + cached_pps_.size() + 2 * kH264StartCodeSize;
        break;

      case H264NALU::kIDRSlice:
        // Only inject if we have both headers cached, and enough space for both
        // the headers and the NALU itself.
        if (cached_sps_.empty() || cached_pps_.empty() ||
            cached_h264_header_size_ + nalu.size + kH264StartCodeSize >
                remaining_dst_size) {
          VLOGF(1) << "Not enough space to inject a stream header before IDR";
          break;
        }

        CopyNALUPrependingStartCode(cached_sps_.data(), cached_sps_.size(),
                                    &dst_ptr, &remaining_dst_size);
        CopyNALUPrependingStartCode(cached_pps_.data(), cached_pps_.size(),
                                    &dst_ptr, &remaining_dst_size);
        VLOGF(2) << "Stream header injected before IDR";
        break;
    }

    CopyNALUPrependingStartCode(nalu.data, nalu.size, &dst_ptr,
                                &remaining_dst_size);
  }

  return buffer_ref->shm->size() - remaining_dst_size;
}

void V4L2VideoEncodeAccelerator::EncodeTask(
    const scoped_refptr<VideoFrame>& frame,
    bool force_keyframe) {
  DVLOGF(4) << "force_keyframe=" << force_keyframe;
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(encoder_state_, kUninitialized);

  if (encoder_state_ == kError) {
    DVLOGF(1) << "early out: kError state";
    return;
  }

  encoder_input_queue_.emplace(frame, force_keyframe);
  Enqueue();
}

void V4L2VideoEncodeAccelerator::UseOutputBitstreamBufferTask(
    std::unique_ptr<BitstreamBufferRef> buffer_ref) {
  DVLOGF(4) << "id=" << buffer_ref->id;
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());

  encoder_output_queue_.push_back(std::move(buffer_ref));
  Enqueue();

  if (encoder_state_ == kInitialized) {
    // Finish setting up our OUTPUT queue.  See: Initialize().
    // VIDIOC_REQBUFS on OUTPUT queue.
    if (!CreateInputBuffers())
      return;
    if (!StartDevicePoll())
      return;
    encoder_state_ = kEncoding;
  }
}

void V4L2VideoEncodeAccelerator::DestroyTask() {
  VLOGF(2);

  // DestroyTask() should run regardless of encoder_state_.

  // Stop streaming and the device_poll_thread_.
  StopDevicePoll();

  // Set our state to kError, and early-out all tasks.
  encoder_state_ = kError;
}

void V4L2VideoEncodeAccelerator::ServiceDeviceTask() {
  DVLOGF(3);
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(encoder_state_, kUninitialized);
  DCHECK_NE(encoder_state_, kInitialized);

  if (encoder_state_ == kError) {
    DVLOGF(1) << "early out: kError state";
    return;
  }

  Dequeue();
  Enqueue();

  // Clear the interrupt fd.
  if (!device_->ClearDevicePollInterrupt())
    return;

  // Device can be polled as soon as either input or output buffers are queued.
  bool poll_device =
      (input_buffer_queued_count_ + output_buffer_queued_count_ > 0);

  // ServiceDeviceTask() should only ever be scheduled from DevicePollTask(),
  // so either:
  // * device_poll_thread_ is running normally
  // * device_poll_thread_ scheduled us, but then a DestroyTask() shut it down,
  //   in which case we're in kError state, and we should have early-outed
  //   already.
  DCHECK(device_poll_thread_.message_loop());
  // Queue the DevicePollTask() now.
  device_poll_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2VideoEncodeAccelerator::DevicePollTask,
                                base::Unretained(this), poll_device));

  DVLOGF(3) << encoder_input_queue_.size() << "] => DEVICE["
            << free_input_buffers_.size() << "+"
            << input_buffer_queued_count_ << "/"
            << input_buffer_map_.size() << "->"
            << free_output_buffers_.size() << "+"
            << output_buffer_queued_count_ << "/"
            << output_buffer_map_.size() << "] => OUT["
            << encoder_output_queue_.size() << "]";
}

void V4L2VideoEncodeAccelerator::Enqueue() {
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());

  DVLOGF(4) << "free_input_buffers: " << free_input_buffers_.size()
            << "input_queue: " << encoder_input_queue_.size();

  // Enqueue all the inputs we can.
  const int old_inputs_queued = input_buffer_queued_count_;
  while (!encoder_input_queue_.empty() && !free_input_buffers_.empty()) {
    // A null frame indicates a flush.
    if (encoder_input_queue_.front().frame == nullptr) {
      DVLOGF(3) << "All input frames needed to be flushed are enqueued.";
      encoder_input_queue_.pop();

      // If we are not streaming, the device is not running and there is no need
      // to call V4L2_ENC_CMD_STOP to request a flush. This also means there is
      // nothing left to process, so we can return flush success back to the
      // client.
      if (!input_streamon_) {
        child_task_runner_->PostTask(
            FROM_HERE, base::BindOnce(std::move(flush_callback_), true));
        return;
      }
      struct v4l2_encoder_cmd cmd;
      memset(&cmd, 0, sizeof(cmd));
      cmd.cmd = V4L2_ENC_CMD_STOP;
      if (device_->Ioctl(VIDIOC_ENCODER_CMD, &cmd) != 0) {
        VPLOGF(1) << "ioctl() failed: VIDIOC_ENCODER_CMD";
        NOTIFY_ERROR(kPlatformFailureError);
        child_task_runner_->PostTask(
            FROM_HERE, base::BindOnce(std::move(flush_callback_), false));
        return;
      }
      encoder_state_ = kFlushing;
      break;
    }
    if (!EnqueueInputRecord())
      return;
  }
  if (old_inputs_queued == 0 && input_buffer_queued_count_ != 0) {
    // We just started up a previously empty queue.
    // Queue state changed; signal interrupt.
    if (!device_->SetDevicePollInterrupt())
      return;
    // Start VIDIOC_STREAMON if we haven't yet.
    if (!input_streamon_) {
      __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
      IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMON, &type);
      input_streamon_ = true;
    }
  }

  // Enqueue all the outputs we can.
  const int old_outputs_queued = output_buffer_queued_count_;
  while (!free_output_buffers_.empty() && !encoder_output_queue_.empty()) {
    if (!EnqueueOutputRecord())
      return;
  }
  if (old_outputs_queued == 0 && output_buffer_queued_count_ != 0) {
    // We just started up a previously empty queue.
    // Queue state changed; signal interrupt.
    if (!device_->SetDevicePollInterrupt())
      return;
    // Start VIDIOC_STREAMON if we haven't yet.
    if (!output_streamon_) {
      __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
      IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMON, &type);
      output_streamon_ = true;
    }
  }
}

void V4L2VideoEncodeAccelerator::Dequeue() {
  DVLOGF(4);
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());

  // Dequeue completed input (VIDEO_OUTPUT) buffers, and recycle to the free
  // list.
  struct v4l2_buffer dqbuf;
  struct v4l2_plane planes[VIDEO_MAX_PLANES];
  while (input_buffer_queued_count_ > 0) {
    DVLOGF(4) << "inputs queued: " << input_buffer_queued_count_;
    DCHECK(input_streamon_);
    memset(&dqbuf, 0, sizeof(dqbuf));
    memset(&planes, 0, sizeof(planes));
    dqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    dqbuf.memory = input_memory_type_;
    dqbuf.m.planes = planes;
    dqbuf.length = input_planes_count_;
    if (device_->Ioctl(VIDIOC_DQBUF, &dqbuf) != 0) {
      if (errno == EAGAIN) {
        // EAGAIN if we're just out of buffers to dequeue.
        break;
      }
      VPLOGF(1) << "ioctl() failed: VIDIOC_DQBUF";
      NOTIFY_ERROR(kPlatformFailureError);
      return;
    }
    InputRecord& input_record = input_buffer_map_[dqbuf.index];
    DCHECK(input_record.at_device);
    input_record.at_device = false;

    input_record.frame = NULL;
    free_input_buffers_.push_back(dqbuf.index);
    input_buffer_queued_count_--;
  }

  // Dequeue completed output (VIDEO_CAPTURE) buffers, and recycle to the
  // free list.  Notify the client that an output buffer is complete.
  while (output_buffer_queued_count_ > 0) {
    DCHECK(output_streamon_);
    memset(&dqbuf, 0, sizeof(dqbuf));
    memset(planes, 0, sizeof(planes));
    dqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    dqbuf.memory = V4L2_MEMORY_MMAP;
    dqbuf.m.planes = planes;
    dqbuf.length = 1;
    if (device_->Ioctl(VIDIOC_DQBUF, &dqbuf) != 0) {
      if (errno == EAGAIN) {
        // EAGAIN if we're just out of buffers to dequeue.
        break;
      }
      VPLOGF(1) << "ioctl() failed: VIDIOC_DQBUF";
      NOTIFY_ERROR(kPlatformFailureError);
      return;
    }
    const bool key_frame = ((dqbuf.flags & V4L2_BUF_FLAG_KEYFRAME) != 0);
    OutputRecord& output_record = output_buffer_map_[dqbuf.index];
    DCHECK(output_record.at_device);
    DCHECK(output_record.buffer_ref);

    int32_t bitstream_buffer_id = output_record.buffer_ref->id;
    size_t output_data_size = CopyIntoOutputBuffer(
        static_cast<uint8_t*>(output_record.address) +
            dqbuf.m.planes[0].data_offset,
        base::checked_cast<size_t>(dqbuf.m.planes[0].bytesused -
                                   dqbuf.m.planes[0].data_offset),
        std::move(output_record.buffer_ref));

    DVLOGF(4) << "returning "
              << "bitstream_buffer_id=" << bitstream_buffer_id
              << ", size=" << output_data_size << ", key_frame=" << key_frame;

    child_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&Client::BitstreamBufferReady, client_, bitstream_buffer_id,
                   BitstreamBufferMetadata(
                       output_data_size, key_frame,
                       base::TimeDelta::FromMicroseconds(
                           dqbuf.timestamp.tv_usec +
                           dqbuf.timestamp.tv_sec *
                               base::Time::kMicrosecondsPerSecond))));
    if ((encoder_state_ == kFlushing) && (dqbuf.flags & V4L2_BUF_FLAG_LAST)) {
      // Notify client that flush has finished successfully. The flush callback
      // should be called after notifying the last buffer is ready.
      DVLOGF(3) << "Flush completed. Start the encoder again.";
      encoder_state_ = kEncoding;
      child_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(std::move(flush_callback_), true));
      // Start the encoder again.
      struct v4l2_encoder_cmd cmd;
      memset(&cmd, 0, sizeof(cmd));
      cmd.cmd = V4L2_ENC_CMD_START;
      IOCTL_OR_ERROR_RETURN(VIDIOC_ENCODER_CMD, &cmd);
    }

    output_record.at_device = false;
    free_output_buffers_.push_back(dqbuf.index);
    output_buffer_queued_count_--;
  }
}

bool V4L2VideoEncodeAccelerator::EnqueueInputRecord() {
  DVLOGF(4);
  DCHECK(!free_input_buffers_.empty());
  DCHECK(!encoder_input_queue_.empty());

  // Enqueue an input (VIDEO_OUTPUT) buffer.
  InputFrameInfo frame_info = encoder_input_queue_.front();
  if (frame_info.force_keyframe) {
    std::vector<struct v4l2_ext_control> ctrls;
    struct v4l2_ext_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME;
    ctrls.push_back(ctrl);
    if (!SetExtCtrls(ctrls)) {
      VLOGF(1) << "Failed requesting keyframe";
      NOTIFY_ERROR(kPlatformFailureError);
      return false;
    }
  }

  scoped_refptr<VideoFrame> frame = frame_info.frame;
  const int index = free_input_buffers_.back();
  InputRecord& input_record = input_buffer_map_[index];
  DCHECK(!input_record.at_device);
  struct v4l2_buffer qbuf;
  struct v4l2_plane qbuf_planes[VIDEO_MAX_PLANES];
  memset(&qbuf, 0, sizeof(qbuf));
  memset(qbuf_planes, 0, sizeof(qbuf_planes));
  qbuf.index = index;
  qbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  qbuf.m.planes = qbuf_planes;
  qbuf.timestamp.tv_sec = static_cast<time_t>(frame->timestamp().InSeconds());
  qbuf.timestamp.tv_usec =
      frame->timestamp().InMicroseconds() -
      frame->timestamp().InSeconds() * base::Time::kMicrosecondsPerSecond;

  DCHECK_EQ(device_input_format_, frame->format());

  std::vector<int> fds;
  if (input_memory_type_ == V4L2_MEMORY_DMABUF) {
    auto& scoped_fds = frame->DmabufFds();
    if (scoped_fds.size() != input_planes_count_) {
      VLOGF(1) << "Invalid number of planes in the frame";
      return false;
    }
    for (auto& fd : scoped_fds)
      fds.push_back(fd.get());
  }

  for (size_t i = 0; i < input_planes_count_; ++i) {
    qbuf.m.planes[i].bytesused = base::checked_cast<__u32>(
        VideoFrame::PlaneSize(frame->format(), i, input_allocated_size_)
            .GetArea());

    switch (input_memory_type_) {
      case V4L2_MEMORY_USERPTR:
        qbuf.m.planes[i].length = qbuf.m.planes[i].bytesused;
        qbuf.m.planes[i].m.userptr =
            reinterpret_cast<unsigned long>(frame->data(i));
        DCHECK(qbuf.m.planes[i].m.userptr);
        break;

      case V4L2_MEMORY_DMABUF:
        qbuf.m.planes[i].m.fd = fds[i];
        DCHECK_NE(qbuf.m.planes[i].m.fd, -1);
        break;

      default:
        NOTREACHED();
        return false;
    }
  }

  qbuf.memory = input_memory_type_;
  qbuf.length = input_planes_count_;

  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QBUF, &qbuf);
  input_record.at_device = true;
  input_record.frame = frame;
  encoder_input_queue_.pop();
  free_input_buffers_.pop_back();
  input_buffer_queued_count_++;
  return true;
}

bool V4L2VideoEncodeAccelerator::EnqueueOutputRecord() {
  DVLOGF(4);
  DCHECK(!free_output_buffers_.empty());
  DCHECK(!encoder_output_queue_.empty());

  // Enqueue an output (VIDEO_CAPTURE) buffer.
  const int index = free_output_buffers_.back();
  OutputRecord& output_record = output_buffer_map_[index];
  DCHECK(!output_record.at_device);
  DCHECK(!output_record.buffer_ref);
  struct v4l2_buffer qbuf;
  struct v4l2_plane qbuf_planes[1];
  memset(&qbuf, 0, sizeof(qbuf));
  memset(qbuf_planes, 0, sizeof(qbuf_planes));
  qbuf.index = index;
  qbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  qbuf.memory = V4L2_MEMORY_MMAP;
  qbuf.m.planes = qbuf_planes;
  qbuf.length = 1;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QBUF, &qbuf);
  output_record.at_device = true;
  output_record.buffer_ref = std::move(encoder_output_queue_.back());
  encoder_output_queue_.pop_back();
  free_output_buffers_.pop_back();
  output_buffer_queued_count_++;
  return true;
}

bool V4L2VideoEncodeAccelerator::StartDevicePoll() {
  DVLOGF(3);
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(!device_poll_thread_.IsRunning());

  // Start up the device poll thread and schedule its first DevicePollTask().
  if (!device_poll_thread_.Start()) {
    VLOGF(1) << "StartDevicePoll(): Device thread failed to start";
    NOTIFY_ERROR(kPlatformFailureError);
    return false;
  }
  // Enqueue a poll task with no devices to poll on -- it will wait only on the
  // interrupt fd.
  device_poll_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2VideoEncodeAccelerator::DevicePollTask,
                                base::Unretained(this), false));

  return true;
}

bool V4L2VideoEncodeAccelerator::StopDevicePoll() {
  DVLOGF(3);

  // Signal the DevicePollTask() to stop, and stop the device poll thread.
  if (!device_->SetDevicePollInterrupt())
    return false;
  device_poll_thread_.Stop();
  // Clear the interrupt now, to be sure.
  if (!device_->ClearDevicePollInterrupt())
    return false;

  if (input_streamon_) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_STREAMOFF, &type);
  }
  input_streamon_ = false;

  if (output_streamon_) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_STREAMOFF, &type);
  }
  output_streamon_ = false;

  // Reset all our accounting info.
  while (!encoder_input_queue_.empty())
    encoder_input_queue_.pop();
  free_input_buffers_.clear();
  for (size_t i = 0; i < input_buffer_map_.size(); ++i) {
    InputRecord& input_record = input_buffer_map_[i];
    input_record.at_device = false;
    input_record.frame = NULL;
    free_input_buffers_.push_back(i);
  }
  input_buffer_queued_count_ = 0;

  free_output_buffers_.clear();
  for (size_t i = 0; i < output_buffer_map_.size(); ++i) {
    OutputRecord& output_record = output_buffer_map_[i];
    output_record.at_device = false;
    output_record.buffer_ref.reset();
    free_output_buffers_.push_back(i);
  }
  output_buffer_queued_count_ = 0;

  encoder_output_queue_.clear();

  DVLOGF(3) << "device poll stopped";
  return true;
}

void V4L2VideoEncodeAccelerator::DevicePollTask(bool poll_device) {
  DVLOGF(4);
  DCHECK(device_poll_thread_.task_runner()->BelongsToCurrentThread());

  bool event_pending;
  if (!device_->Poll(poll_device, &event_pending)) {
    NOTIFY_ERROR(kPlatformFailureError);
    return;
  }

  // All processing should happen on ServiceDeviceTask(), since we shouldn't
  // touch encoder state from this thread.
  encoder_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2VideoEncodeAccelerator::ServiceDeviceTask,
                                base::Unretained(this)));
}

void V4L2VideoEncodeAccelerator::NotifyError(Error error) {
  VLOGF(1) << "error=" << error;

  if (!child_task_runner_->BelongsToCurrentThread()) {
    child_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&V4L2VideoEncodeAccelerator::NotifyError,
                                  weak_this_, error));
    return;
  }

  if (client_) {
    client_->NotifyError(error);
    client_ptr_factory_.reset();
  }
}

void V4L2VideoEncodeAccelerator::SetErrorState(Error error) {
  // We can touch encoder_state_ only if this is the encoder thread or the
  // encoder thread isn't running.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      encoder_thread_.task_runner();
  if (task_runner && !task_runner->BelongsToCurrentThread()) {
    task_runner->PostTask(FROM_HERE,
                          base::Bind(&V4L2VideoEncodeAccelerator::SetErrorState,
                                     base::Unretained(this), error));
    return;
  }

  // Post NotifyError only if we are already initialized, as the API does
  // not allow doing so before that.
  if (encoder_state_ != kError && encoder_state_ != kUninitialized)
    NotifyError(error);

  encoder_state_ = kError;
}

void V4L2VideoEncodeAccelerator::RequestEncodingParametersChangeTask(
    uint32_t bitrate,
    uint32_t framerate) {
  VLOGF(2) << "bitrate=" << bitrate << ", framerate=" << framerate;
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());

  DCHECK_GT(bitrate, 0u);
  DCHECK_GT(framerate, 0u);

  std::vector<struct v4l2_ext_control> ctrls;
  struct v4l2_ext_control ctrl;
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_MPEG_VIDEO_BITRATE;
  ctrl.value = bitrate;
  ctrls.push_back(ctrl);
  if (!SetExtCtrls(ctrls)) {
    VLOGF(1) << "Failed changing bitrate";
    NOTIFY_ERROR(kPlatformFailureError);
    return;
  }

  struct v4l2_streamparm parms;
  memset(&parms, 0, sizeof(parms));
  parms.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  // Note that we are provided "frames per second" but V4L2 expects "time per
  // frame"; hence we provide the reciprocal of the framerate here.
  parms.parm.output.timeperframe.numerator = 1;
  parms.parm.output.timeperframe.denominator = framerate;
  IOCTL_OR_ERROR_RETURN(VIDIOC_S_PARM, &parms);
}

bool V4L2VideoEncodeAccelerator::SetOutputFormat(
    VideoCodecProfile output_profile) {
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DCHECK(!input_streamon_);
  DCHECK(!output_streamon_);

  output_buffer_byte_size_ = GetEncodeBitstreamBufferSize();

  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  format.fmt.pix_mp.width = visible_size_.width();
  format.fmt.pix_mp.height = visible_size_.height();
  format.fmt.pix_mp.pixelformat = output_format_fourcc_;
  format.fmt.pix_mp.plane_fmt[0].sizeimage =
      base::checked_cast<__u32>(output_buffer_byte_size_);
  format.fmt.pix_mp.num_planes = 1;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_FMT, &format);

  // Device might have adjusted the required output size.
  size_t adjusted_output_buffer_size =
      base::checked_cast<size_t>(format.fmt.pix_mp.plane_fmt[0].sizeimage);
  output_buffer_byte_size_ = adjusted_output_buffer_size;

  return true;
}

bool V4L2VideoEncodeAccelerator::NegotiateInputFormat(
    VideoPixelFormat input_format) {
  VLOGF(2);
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DCHECK(!input_streamon_);
  DCHECK(!output_streamon_);

  device_input_format_ = PIXEL_FORMAT_UNKNOWN;
  input_planes_count_ = 0;

  uint32_t input_format_fourcc =
      V4L2Device::VideoPixelFormatToV4L2PixFmt(input_format);
  if (!input_format_fourcc) {
    VLOGF(1) << "Unsupported input format" << input_format_fourcc;
    return false;
  }

  size_t input_planes_count = VideoFrame::NumPlanes(input_format);
  DCHECK_LE(input_planes_count, static_cast<size_t>(VIDEO_MAX_PLANES));

  // First see if we the device can use the provided input_format directly.
  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  format.fmt.pix_mp.width = visible_size_.width();
  format.fmt.pix_mp.height = visible_size_.height();
  format.fmt.pix_mp.pixelformat = input_format_fourcc;
  format.fmt.pix_mp.num_planes = input_planes_count;
  if (device_->Ioctl(VIDIOC_S_FMT, &format) != 0) {
    // Error or format unsupported by device, try to negotiate a fallback.
    input_format_fourcc =
        device_->PreferredInputFormat(V4L2Device::Type::kEncoder);
    input_format =
        V4L2Device::V4L2PixFmtToVideoPixelFormat(input_format_fourcc);
    if (input_format == PIXEL_FORMAT_UNKNOWN) {
      VLOGF(1) << "Unsupported input format: "
               << FourccToString(input_format_fourcc);
      return false;
    }

    input_planes_count = VideoFrame::NumPlanes(input_format);
    DCHECK_LE(input_planes_count, static_cast<size_t>(VIDEO_MAX_PLANES));

    // Device might have adjusted parameters, reset them along with the format.
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    format.fmt.pix_mp.width = visible_size_.width();
    format.fmt.pix_mp.height = visible_size_.height();
    format.fmt.pix_mp.pixelformat = input_format_fourcc;
    format.fmt.pix_mp.num_planes = input_planes_count;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_FMT, &format);
    DCHECK_EQ(format.fmt.pix_mp.num_planes, input_planes_count);
  }

  // Take device-adjusted sizes for allocated size. If the size is adjusted
  // down, it means the input is too big and the hardware does not support it.
  input_allocated_size_ = V4L2Device::CodedSizeFromV4L2Format(format);
  if (!gfx::Rect(input_allocated_size_).Contains(gfx::Rect(visible_size_))) {
    VLOGF(1) << "Input size too big " << visible_size_.ToString()
             << ", adjusted to " << input_allocated_size_.ToString();
    return false;
  }

  device_input_format_ = input_format;
  input_planes_count_ = input_planes_count;
  return true;
}

bool V4L2VideoEncodeAccelerator::SetFormats(VideoPixelFormat input_format,
                                            VideoCodecProfile output_profile) {
  VLOGF(2);
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DCHECK(!input_streamon_);
  DCHECK(!output_streamon_);

  if (!SetOutputFormat(output_profile))
    return false;

  if (!NegotiateInputFormat(input_format))
    return false;

  struct v4l2_rect visible_rect;
  visible_rect.left = 0;
  visible_rect.top = 0;
  visible_rect.width = visible_size_.width();
  visible_rect.height = visible_size_.height();

  struct v4l2_selection selection_arg;
  memset(&selection_arg, 0, sizeof(selection_arg));
  selection_arg.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  selection_arg.target = V4L2_SEL_TGT_CROP;
  selection_arg.r = visible_rect;

  // The width and height might be adjusted by driver.
  // Need to read it back and set to visible_size_.
  if (device_->Ioctl(VIDIOC_S_SELECTION, &selection_arg) == 0) {
    DVLOGF(2) << "VIDIOC_S_SELECTION is supported";
    visible_rect = selection_arg.r;
  } else {
    VLOGF(2) << "Fallback to VIDIOC_S/G_CROP";
    struct v4l2_crop crop;
    memset(&crop, 0, sizeof(crop));
    crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    crop.c = visible_rect;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_CROP, &crop);
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_G_CROP, &crop);
    visible_rect = crop.c;
  }

  visible_size_.SetSize(visible_rect.width, visible_rect.height);
  VLOGF(2) << "After adjusted by driver, visible_size_="
           << visible_size_.ToString();

  return true;
}

bool V4L2VideoEncodeAccelerator::IsCtrlExposed(uint32_t ctrl_id) {
  struct v4l2_queryctrl query_ctrl;
  memset(&query_ctrl, 0, sizeof(query_ctrl));
  query_ctrl.id = ctrl_id;

  return device_->Ioctl(VIDIOC_QUERYCTRL, &query_ctrl) == 0;
}

bool V4L2VideoEncodeAccelerator::SetExtCtrls(
    std::vector<struct v4l2_ext_control> ctrls) {
  struct v4l2_ext_controls ext_ctrls;
  memset(&ext_ctrls, 0, sizeof(ext_ctrls));
  ext_ctrls.ctrl_class = V4L2_CTRL_CLASS_MPEG;
  ext_ctrls.count = ctrls.size();
  ext_ctrls.controls = &ctrls[0];
  return device_->Ioctl(VIDIOC_S_EXT_CTRLS, &ext_ctrls) == 0;
}

bool V4L2VideoEncodeAccelerator::InitControls(const Config& config) {
  std::vector<struct v4l2_ext_control> ctrls;
  struct v4l2_ext_control ctrl;

  // Enable frame-level bitrate control. This is the only mandatory control.
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE;
  ctrl.value = 1;
  ctrls.push_back(ctrl);
  if (!SetExtCtrls(ctrls)) {
    VLOGF(1) << "Failed enabling bitrate control";
    NOTIFY_ERROR(kPlatformFailureError);
    return false;
  }

  ctrls.clear();
  if (output_format_fourcc_ == V4L2_PIX_FMT_H264) {
#ifndef V4L2_CID_MPEG_VIDEO_H264_SPS_PPS_BEFORE_IDR
#define V4L2_CID_MPEG_VIDEO_H264_SPS_PPS_BEFORE_IDR (V4L2_CID_MPEG_BASE + 388)
#endif
    // Request to inject SPS and PPS before each IDR, if the device supports
    // that feature. Otherwise we'll have to cache and inject ourselves.
    if (IsCtrlExposed(V4L2_CID_MPEG_VIDEO_H264_SPS_PPS_BEFORE_IDR)) {
      memset(&ctrl, 0, sizeof(ctrl));
      ctrl.id = V4L2_CID_MPEG_VIDEO_H264_SPS_PPS_BEFORE_IDR;
      ctrl.value = 1;
      ctrls.push_back(ctrl);
      if (!SetExtCtrls(ctrls)) {
        NOTIFY_ERROR(kPlatformFailureError);
        return false;
      }
      ctrls.clear();
      inject_sps_and_pps_ = false;
      DVLOGF(2) << "Device supports injecting SPS+PPS before each IDR";
    } else {
      inject_sps_and_pps_ = true;
      DVLOGF(2) << "Will inject SPS+PPS before each IDR, unsupported by device";
    }

    // Optional controls.
    // No B-frames, for lowest decoding latency.
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_MPEG_VIDEO_B_FRAMES;
    ctrl.value = 0;
    ctrls.push_back(ctrl);

    // Quantization parameter maximum value (for variable bitrate control).
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_MPEG_VIDEO_H264_MAX_QP;
    ctrl.value = 51;
    ctrls.push_back(ctrl);

    // Set H.264 profile.
    int32_t profile_value =
        V4L2Device::VideoCodecProfileToV4L2H264Profile(config.output_profile);
    if (profile_value < 0) {
      NOTIFY_ERROR(kInvalidArgumentError);
      return false;
    }
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_MPEG_VIDEO_H264_PROFILE;
    ctrl.value = profile_value;
    ctrls.push_back(ctrl);

    // Set H.264 output level from config. Use Level 4.0 as fallback default.
    int32_t level_value = V4L2Device::H264LevelIdcToV4L2H264Level(
        config.h264_output_level.value_or(
            VideoEncodeAccelerator::kDefaultH264Level));
    if (level_value < 0) {
      NOTIFY_ERROR(kInvalidArgumentError);
      return false;
    }
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_MPEG_VIDEO_H264_LEVEL;
    ctrl.value = level_value;
    ctrls.push_back(ctrl);

    // Ask not to put SPS and PPS into separate bitstream buffers.
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_MPEG_VIDEO_HEADER_MODE;
    ctrl.value = V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME;
    ctrls.push_back(ctrl);
  }

  // Enable macroblock-level bitrate control.
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_MPEG_VIDEO_MB_RC_ENABLE;
  ctrl.value = 1;
  ctrls.push_back(ctrl);

  // Disable periodic key frames.
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_MPEG_VIDEO_GOP_SIZE;
  ctrl.value = 0;
  ctrls.push_back(ctrl);

  // Ignore return value as these controls are optional.
  SetExtCtrls(ctrls);

  // Optional Exynos specific controls.
  ctrls.clear();
  // Enable "tight" bitrate mode. For this to work properly, frame- and mb-level
  // bitrate controls have to be enabled as well.
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_MPEG_MFC51_VIDEO_RC_REACTION_COEFF;
  ctrl.value = 1;
  ctrls.push_back(ctrl);

  // Force bitrate control to average over a GOP (for tight bitrate
  // tolerance).
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_MPEG_MFC51_VIDEO_RC_FIXED_TARGET_BIT;
  ctrl.value = 1;
  ctrls.push_back(ctrl);

  // Ignore return value as these controls are optional.
  SetExtCtrls(ctrls);

  return true;
}

bool V4L2VideoEncodeAccelerator::CreateInputBuffers() {
  VLOGF(2);
  // This function runs on encoder_thread_ after output buffers have been
  // provided by the client.
  DCHECK(encoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(!input_streamon_);

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  // Driver will modify to the appropriate number of buffers.
  reqbufs.count = kInputBufferCount;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  // TODO(posciak): Once we start doing zero-copy, we should decide based on
  // the current pipeline setup which memory type to use. This should probably
  // be decided based on an argument to Initialize().
  if (image_processor_.get())
    input_memory_type_ = V4L2_MEMORY_DMABUF;
  else
    input_memory_type_ = V4L2_MEMORY_USERPTR;

  reqbufs.memory = input_memory_type_;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_REQBUFS, &reqbufs);

  DCHECK(input_buffer_map_.empty());
  input_buffer_map_.resize(reqbufs.count);
  for (size_t i = 0; i < input_buffer_map_.size(); ++i)
    free_input_buffers_.push_back(i);

  return true;
}

bool V4L2VideoEncodeAccelerator::CreateOutputBuffers() {
  VLOGF(2);
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DCHECK(!output_streamon_);

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = kOutputBufferCount;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_REQBUFS, &reqbufs);

  DCHECK(output_buffer_map_.empty());
  output_buffer_map_ = std::vector<OutputRecord>(reqbufs.count);
  for (size_t i = 0; i < output_buffer_map_.size(); ++i) {
    struct v4l2_plane planes[1];
    struct v4l2_buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    memset(planes, 0, sizeof(planes));
    buffer.index = i;
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.m.planes = planes;
    buffer.length = arraysize(planes);
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QUERYBUF, &buffer);
    void* address = device_->Mmap(NULL,
                                  buffer.m.planes[0].length,
                                  PROT_READ | PROT_WRITE,
                                  MAP_SHARED,
                                  buffer.m.planes[0].m.mem_offset);
    if (address == MAP_FAILED) {
      VPLOGF(1) << "mmap() failed";
      return false;
    }
    output_buffer_map_[i].address = address;
    output_buffer_map_[i].length = buffer.m.planes[0].length;
    free_output_buffers_.push_back(i);
  }

  return true;
}

void V4L2VideoEncodeAccelerator::DestroyInputBuffers() {
  VLOGF(2);
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DCHECK(!input_streamon_);

  free_input_buffers_.clear();

  if (input_buffer_map_.empty())
    return;

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbufs.memory = input_memory_type_;
  IOCTL_OR_LOG_ERROR(VIDIOC_REQBUFS, &reqbufs);

  input_buffer_map_.clear();
}

void V4L2VideoEncodeAccelerator::DestroyOutputBuffers() {
  VLOGF(2);
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DCHECK(!output_streamon_);

  free_output_buffers_.clear();

  if (output_buffer_map_.empty())
    return;

  for (size_t i = 0; i < output_buffer_map_.size(); ++i) {
    if (output_buffer_map_[i].address != NULL)
      device_->Munmap(output_buffer_map_[i].address,
                      output_buffer_map_[i].length);
  }

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  IOCTL_OR_LOG_ERROR(VIDIOC_REQBUFS, &reqbufs);

  output_buffer_map_.clear();
}

}  // namespace media
