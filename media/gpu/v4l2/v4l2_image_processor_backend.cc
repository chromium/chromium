// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_image_processor_backend.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <limits>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/base/color_plane_layout.h"
#include "media/base/media_util.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/chromeos/video_frame_resource.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_utils.h"

namespace media {

namespace {

std::optional<gfx::GpuMemoryBufferHandle> CreateHandle(
    const FrameResource* frame) {
  gfx::GpuMemoryBufferHandle handle = frame->CreateGpuMemoryBufferHandle();

  if (handle.is_null() || handle.type != gfx::NATIVE_PIXMAP)
    return std::nullopt;
  return handle;
}

void FillV4L2BufferByGpuMemoryBufferHandle(
    const Fourcc& fourcc,
    const gfx::Size& coded_size,
    const gfx::GpuMemoryBufferHandle& gmb_handle,
    V4L2WritableBufferRef* buffer) {
  DCHECK_EQ(buffer->Memory(), V4L2_MEMORY_DMABUF);
  const size_t num_planes = GetNumPlanesOfV4L2PixFmt(fourcc.ToV4L2PixFmt());
  const std::vector<gfx::NativePixmapPlane>& planes =
      gmb_handle.native_pixmap_handle.planes;

  for (size_t i = 0; i < num_planes; ++i) {
    if (fourcc.IsMultiPlanar()) {
      // TODO(crbug.com/901264): The way to pass an offset within a DMA-buf
      // is not defined in V4L2 specification, so we abuse data_offset for
      // now. Fix it when we have the right interface, including any
      // necessary validation and potential alignment
      buffer->SetPlaneDataOffset(i, planes[i].offset);

      // V4L2 counts data_offset as used bytes
      buffer->SetPlaneSize(i, planes[i].size + planes[i].offset);
      // Workaround: filling length should not be needed. This is a bug of
      // videobuf2 library.
      buffer->SetPlaneBytesUsed(i, planes[i].size + planes[i].offset);
    } else {
      // There is no need of filling data_offset for a single-planar format.
      buffer->SetPlaneBytesUsed(i, planes[i].size);
    }
  }
}

bool AllocateV4L2Buffers(V4L2Queue* queue,
                         const size_t num_buffers,
                         v4l2_memory memory_type) {
  DCHECK(queue);

  size_t requested_buffers = num_buffers;

  // If we are using DMABUFs, then we will try to keep using the same V4L2
  // buffer for a given input or output frame. In that case, allocate as many
  // V4L2 buffers as we can to avoid running out of them. Unused buffers won't
  // use backed memory and are thus virtually free.
  if (memory_type == V4L2_MEMORY_DMABUF)
    requested_buffers = VIDEO_MAX_FRAME;

  // Note that MDP does not support incoherent buffer allocations.
  if (queue->AllocateBuffers(requested_buffers, memory_type,
                             /*incoherent=*/false) == 0u)
    return false;

  if (queue->AllocatedBuffersCount() < num_buffers) {
    VLOGF(1) << "Failed to allocate buffers. Allocated number="
             << queue->AllocatedBuffersCount()
             << ", Requested number=" << num_buffers;
    return false;
  }

  return true;
}
}  // namespace

V4L2ImageProcessorBackend::JobRecord::JobRecord()
    : output_buffer_id(std::numeric_limits<size_t>::max()) {}

V4L2ImageProcessorBackend::JobRecord::~JobRecord() = default;

V4L2ImageProcessorBackend::V4L2ImageProcessorBackend(
    scoped_refptr<V4L2Device> device,
    const PortConfig& input_config,
    const PortConfig& output_config,
    v4l2_memory input_memory_type,
    v4l2_memory output_memory_type,
    OutputMode output_mode,
    ErrorCB error_cb)
    : ImageProcessorBackend(
          input_config,
          output_config,
          output_mode,
          std::move(error_cb),
          // This class doesn't use a backend runner because the V4L2 operations
          // are fast and non-blocking(except for poll() which happens in its
          // own TaskRunner)
          base::SequencedTaskRunner::GetCurrentDefault()),
      input_memory_type_(input_memory_type),
      output_memory_type_(output_memory_type),
      device_(device),
      // We poll V4L2 device on this task runner, which blocks the task runner.
      // Therefore we use dedicated SingleThreadTaskRunner here.
      poll_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED)) {
  DVLOGF(2);
  DETACH_FROM_SEQUENCE(poll_sequence_checker_);
  DCHECK_NE(output_memory_type_, V4L2_MEMORY_USERPTR);

  VLOGF(2) << "V4L2ImageProcessorBackend constructed with input: "
           << input_config_.ToString()
           << ", output: " << output_config_.ToString();

  weak_this_ = weak_this_factory_.GetWeakPtr();
  poll_weak_this_ = poll_weak_this_factory_.GetWeakPtr();
}

std::string V4L2ImageProcessorBackend::type() const {
  return "V4L2ImageProcessor";
}

void V4L2ImageProcessorBackend::Destroy() {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  weak_this_factory_.InvalidateWeakPtrs();

  if (input_queue_) {
    if (!input_queue_->Streamoff())
      VLOGF(1) << "Failed to turn stream off";
    if (!input_queue_->DeallocateBuffers())
      VLOGF(1) << "Failed to deallocate buffers";
    input_queue_ = nullptr;
  }
  if (output_queue_) {
    if (!output_queue_->Streamoff())
      VLOGF(1) << "Failed to turn stream off";
    if (!output_queue_->DeallocateBuffers())
      VLOGF(1) << "Failed to deallocate buffers";
    output_queue_ = nullptr;
  }

  // Reset all our accounting info.
  input_job_queue_ = {};
  running_jobs_ = {};

  // Stop the running DevicePollTask() if it exists.
  if (!device_->SetDevicePollInterrupt())
    NotifyError();

  // After stopping queue, we don't schedule new DevicePollTask() to
  // |poll_task_runner_|. Now clean up |poll_task_runner_|.
  poll_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2ImageProcessorBackend::DestroyOnPollSequence,
                     poll_weak_this_));
}

void V4L2ImageProcessorBackend::DestroyOnPollSequence() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(poll_sequence_checker_);

  poll_weak_this_factory_.InvalidateWeakPtrs();

  delete this;
}

V4L2ImageProcessorBackend::~V4L2ImageProcessorBackend() {
  VLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(poll_sequence_checker_);
}

void V4L2ImageProcessorBackend::NotifyError() {
  VLOGF(1);
  // Note: |error_cb_| must be thread safe for this to work from any task runner
  // and it is, but it should be enforced somehow.
  error_cb_.Run();
}

namespace {

v4l2_memory InputStorageTypeToV4L2Memory(VideoFrame::StorageType storage_type) {
  switch (storage_type) {
    case VideoFrame::STORAGE_OWNED_MEMORY:
    case VideoFrame::STORAGE_UNOWNED_MEMORY:
    case VideoFrame::STORAGE_SHMEM:
      return V4L2_MEMORY_USERPTR;
    case VideoFrame::STORAGE_DMABUFS:
    case VideoFrame::STORAGE_GPU_MEMORY_BUFFER:
      return V4L2_MEMORY_DMABUF;
    default:
      return static_cast<v4l2_memory>(0);
  }
}

}  // namespace

// static
std::unique_ptr<ImageProcessorBackend> V4L2ImageProcessorBackend::Create(
    scoped_refptr<V4L2Device> device,
    size_t num_buffers,
    const PortConfig& input_config,
    const PortConfig& output_config,
    OutputMode output_mode,
    ErrorCB error_cb) {
  VLOGF(2);
  DCHECK_GT(num_buffers, 0u);

  // Most of the users of this class are decoders that only want a pixel format
  // conversion (with the same coded dimensions and visible rectangles). Video
  // encoding, however, can try and ask for cropping (this is common for camera
  // capture for example). Although the V4L2 ImageProcessor might support it,
  // it's a better idea to use libyuv instead.
  if (input_config.size != output_config.size ||
      input_config.visible_rect != output_config.visible_rect) {
    VLOGF(2) << "V4L2ImageProcessor cannot adapt size/visible_rects, input "
             << input_config.ToString() << ", output "
             << output_config.ToString();
    return nullptr;
  }

  if (!device) {
    VLOGF(2) << "Failed creating V4L2Device";
    return nullptr;
  }

  const v4l2_memory input_memory_type =
      InputStorageTypeToV4L2Memory(input_config.storage_type);
  if (input_memory_type != V4L2_MEMORY_USERPTR &&
      input_memory_type != V4L2_MEMORY_DMABUF) {
    VLOGF(2) << "Unsupported input storage type";
    return nullptr;
  }

  // When |output_mode| is ALLOCATE, then |output_config.storage_type| is
  // ignored. The output memory type will be V4L2_MEMORY_MMAP.
  const v4l2_memory output_memory_type =
      output_mode == OutputMode::ALLOCATE
          ? V4L2_MEMORY_MMAP
          : InputStorageTypeToV4L2Memory(output_config.storage_type);
  if (output_memory_type != V4L2_MEMORY_MMAP &&
      output_memory_type != V4L2_MEMORY_DMABUF) {
    VLOGF(2) << "Unsupported output storage type";
    return nullptr;
  }

  if (!device->IsImageProcessingSupported()) {
    VLOGF(1) << "V4L2ImageProcessorBackend not supported in this platform";
    return nullptr;
  }

  if (!device->Open(V4L2Device::Type::kImageProcessor,
                    input_config.fourcc.ToV4L2PixFmt())) {
    VLOGF(1) << "Failed to open device with input fourcc: "
             << input_config.fourcc.ToString();
    return nullptr;
  }

  // Try to set input format.
  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  format.fmt.pix_mp.width = input_config.size.width();
  format.fmt.pix_mp.height = input_config.size.height();
  format.fmt.pix_mp.pixelformat = input_config.fourcc.ToV4L2PixFmt();
  if (device->Ioctl(VIDIOC_S_FMT, &format) != 0 ||
      format.fmt.pix_mp.pixelformat != input_config.fourcc.ToV4L2PixFmt()) {
    VLOGF(1) << "Failed to negotiate input format";
    return nullptr;
  }

  const v4l2_pix_format_mplane& pix_mp = format.fmt.pix_mp;
  const gfx::Size negotiated_input_size(pix_mp.width, pix_mp.height);
  if (!gfx::Rect(negotiated_input_size).Contains(input_config.visible_rect)) {
    VLOGF(1) << "Negotiated input allocated size: "
             << negotiated_input_size.ToString()
             << " should contain visible size: "
             << input_config.visible_rect.size().ToString();
    return nullptr;
  }
  std::vector<ColorPlaneLayout> input_planes(pix_mp.num_planes);
  for (size_t i = 0; i < pix_mp.num_planes; ++i) {
    input_planes[i].stride = pix_mp.plane_fmt[i].bytesperline;
    // offset will be specified for a buffer in each VIDIOC_QBUF.
    input_planes[i].offset = 0;
    input_planes[i].size = pix_mp.plane_fmt[i].sizeimage;
  }

  // Try to set output format.
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  v4l2_pix_format_mplane& out_pix_mp = format.fmt.pix_mp;
  out_pix_mp.width = output_config.size.width();
  out_pix_mp.height = output_config.size.height();
  out_pix_mp.pixelformat = output_config.fourcc.ToV4L2PixFmt();
  out_pix_mp.num_planes = output_config.planes.size();
  for (size_t i = 0; i < output_config.planes.size(); ++i) {
    out_pix_mp.plane_fmt[i].sizeimage = output_config.planes[i].size;
    out_pix_mp.plane_fmt[i].bytesperline = output_config.planes[i].stride;
  }
  if (device->Ioctl(VIDIOC_S_FMT, &format) != 0 ||
      format.fmt.pix_mp.pixelformat != output_config.fourcc.ToV4L2PixFmt()) {
    VLOGF(1) << "Failed to negotiate output format";
    return nullptr;
  }

  out_pix_mp = format.fmt.pix_mp;
  const gfx::Size negotiated_output_size(out_pix_mp.width, out_pix_mp.height);
  if (!gfx::Rect(negotiated_output_size)
           .Contains(gfx::Rect(output_config.size))) {
    VLOGF(1) << "Negotiated output allocated size: "
             << negotiated_output_size.ToString()
             << " should contain original output allocated size: "
             << output_config.size.ToString();
    return nullptr;
  }
  std::vector<ColorPlaneLayout> output_planes(out_pix_mp.num_planes);
  for (size_t i = 0; i < pix_mp.num_planes; ++i) {
    output_planes[i].stride = pix_mp.plane_fmt[i].bytesperline;
    // offset will be specified for a buffer in each VIDIOC_QBUF.
    output_planes[i].offset = 0;
    output_planes[i].size = pix_mp.plane_fmt[i].sizeimage;
  }

  // Capabilities check.
  struct v4l2_capability caps {};
  const __u32 kCapsRequired = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
  if (device->Ioctl(VIDIOC_QUERYCAP, &caps) != 0) {
    VPLOGF(1) << "VIDIOC_QUERYCAP failed";
    return nullptr;
  }
  if ((caps.capabilities & kCapsRequired) != kCapsRequired) {
    VLOGF(1) << "VIDIOC_QUERYCAP failed: "
             << "caps check failed: 0x" << std::hex << caps.capabilities;
    return nullptr;
  }

  // Set a few standard controls to default values.
  struct v4l2_control rotation = {.id = V4L2_CID_ROTATE, .value = 0};
  if (device->Ioctl(VIDIOC_S_CTRL, &rotation) != 0) {
    VPLOGF(1) << "V4L2_CID_ROTATE failed";
    return nullptr;
  }

  struct v4l2_control hflip = {.id = V4L2_CID_HFLIP, .value = 0};
  if (device->Ioctl(VIDIOC_S_CTRL, &hflip) != 0) {
    VPLOGF(1) << "V4L2_CID_HFLIP failed";
    return nullptr;
  }

  struct v4l2_control vflip = {.id = V4L2_CID_VFLIP, .value = 0};
  if (device->Ioctl(VIDIOC_S_CTRL, &vflip) != 0) {
    VPLOGF(1) << "V4L2_CID_VFLIP failed";
    return nullptr;
  }

  struct v4l2_control alpha = {.id = V4L2_CID_ALPHA_COMPONENT, .value = 255};
  if (device->Ioctl(VIDIOC_S_CTRL, &alpha) != 0)
    VPLOGF(1) << "V4L2_CID_ALPHA_COMPONENT failed";

  std::unique_ptr<V4L2ImageProcessorBackend> image_processor(
      new V4L2ImageProcessorBackend(
          std::move(device), input_config, output_config, input_memory_type,
          output_memory_type, output_mode, std::move(error_cb)));

  if (!image_processor->CreateInputBuffers(num_buffers) ||
      !image_processor->CreateOutputBuffers(num_buffers)) {
    return nullptr;
  }

  // Enqueue a poll task with no devices to poll on - will wait only for the
  // poll interrupt.
  DVLOGF(3) << "starting device poll";
  image_processor->TriggerPoll(/*poll_device=*/false);
  return image_processor;
}

// static
bool V4L2ImageProcessorBackend::IsSupported() {
  auto device = base::MakeRefCounted<V4L2Device>();
  return device->IsImageProcessingSupported();
}

// static
std::vector<uint32_t> V4L2ImageProcessorBackend::GetSupportedInputFormats() {
  auto device = base::MakeRefCounted<V4L2Device>();
  return device->GetSupportedImageProcessorPixelformats(
      V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
}

// static
std::vector<uint32_t> V4L2ImageProcessorBackend::GetSupportedOutputFormats() {
  auto device = base::MakeRefCounted<V4L2Device>();
  return device->GetSupportedImageProcessorPixelformats(
      V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
}

// static
bool V4L2ImageProcessorBackend::TryOutputFormat(uint32_t input_pixelformat,
                                                uint32_t output_pixelformat,
                                                const gfx::Size& input_size,
                                                gfx::Size* output_size,
                                                size_t* num_planes) {
  DVLOGF(3) << "input_format=" << FourccToString(input_pixelformat)
            << " input_size=" << input_size.ToString()
            << " output_format=" << FourccToString(output_pixelformat)
            << " output_size=" << output_size->ToString();
  auto device = base::MakeRefCounted<V4L2Device>();
  if (!device->Open(V4L2Device::Type::kImageProcessor, input_pixelformat)) {
    return false;
  }

  // Set input format.
  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  format.fmt.pix_mp.width = input_size.width();
  format.fmt.pix_mp.height = input_size.height();
  format.fmt.pix_mp.pixelformat = input_pixelformat;
  if (device->Ioctl(VIDIOC_S_FMT, &format) != 0 ||
      format.fmt.pix_mp.pixelformat != input_pixelformat) {
    DVLOGF(4) << "Failed to set image processor input format: "
              << V4L2FormatToString(format);
    return false;
  }

  // Try output format.
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  format.fmt.pix_mp.width = output_size->width();
  format.fmt.pix_mp.height = output_size->height();
  format.fmt.pix_mp.pixelformat = output_pixelformat;
  if (device->Ioctl(VIDIOC_TRY_FMT, &format) != 0 ||
      format.fmt.pix_mp.pixelformat != output_pixelformat) {
    return false;
  }

  *num_planes = format.fmt.pix_mp.num_planes;
  *output_size = V4L2Device::AllocatedSizeFromV4L2Format(format);
  DVLOGF(3) << "Adjusted output_size=" << output_size->ToString()
            << ", num_planes=" << *num_planes;
  return true;
}

void V4L2ImageProcessorBackend::ProcessLegacyFrame(
    scoped_refptr<FrameResource> frame,
    LegacyFrameResourceReadyCB cb) {
  DVLOGF(4) << "ts=" << frame->timestamp().InMilliseconds();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK_EQ(output_memory_type_, V4L2_MEMORY_MMAP);

  auto job_record = std::make_unique<JobRecord>();
  job_record->input_frame = std::move(frame);
  job_record->legacy_ready_cb = std::move(cb);
  if (MediaTraceIsEnabled()) {
    job_record->start_time = base::TimeTicks::Now();
  }

  input_job_queue_.emplace(std::move(job_record));
  ProcessJobs();
}

void V4L2ImageProcessorBackend::ProcessFrame(
    scoped_refptr<FrameResource> input_frame,
    scoped_refptr<FrameResource> output_frame,
    FrameResourceReadyCB cb) {
  DVLOGF(4) << "ts=" << input_frame->timestamp().InMilliseconds();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto job_record = std::make_unique<JobRecord>();
  job_record->input_frame = std::move(input_frame);
  job_record->output_frame = std::move(output_frame);
  job_record->ready_cb = std::move(cb);
  if (MediaTraceIsEnabled()) {
    job_record->start_time = base::TimeTicks::Now();
  }

  input_job_queue_.emplace(std::move(job_record));
  ProcessJobs();
}

void V4L2ImageProcessorBackend::ProcessJobs() {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  while (!input_job_queue_.empty()) {
    if (!input_queue_->IsStreaming()) {
      const FrameResource& input_frame =
          *(input_job_queue_.front()->input_frame.get());
      const gfx::Size input_buffer_size(input_frame.stride(0),
                                        input_frame.coded_size().height());
      if (!ReconfigureV4L2Format(input_buffer_size,
                                 V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)) {
        NotifyError();
        return;
      }
    }

    if (input_job_queue_.front()
            ->output_frame &&  // output_frame is nullptr in ALLOCATE mode.
        !output_queue_->IsStreaming()) {
      const FrameResource& output_frame =
          *(input_job_queue_.front()->output_frame.get());
      const gfx::Size output_buffer_size(output_frame.stride(0),
                                         output_frame.coded_size().height());
      if (!ReconfigureV4L2Format(output_buffer_size,
                                 V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)) {
        NotifyError();
        return;
      }
    }

    // We need one input and one output buffer to schedule the job
    std::optional<V4L2WritableBufferRef> input_buffer;
    // If we are using DMABUF frames, try to always obtain the same V4L2 buffer.
    if (input_memory_type_ == V4L2_MEMORY_DMABUF) {
      const FrameResource& input_frame =
          *(input_job_queue_.front()->input_frame.get());
      input_buffer =
          input_queue_->GetFreeBufferForFrame(input_frame.GetSharedMemoryId());
    }
    if (!input_buffer)
      input_buffer = input_queue_->GetFreeBuffer();

    std::optional<V4L2WritableBufferRef> output_buffer;
    // If we are using DMABUF frames, try to always obtain the same V4L2 buffer.
    if (output_memory_type_ == V4L2_MEMORY_DMABUF) {
      const FrameResource& output_frame =
          *(input_job_queue_.front()->output_frame.get());
      output_buffer = output_queue_->GetFreeBufferForFrame(
          output_frame.GetSharedMemoryId());
    }
    if (!output_buffer)
      output_buffer = output_queue_->GetFreeBuffer();

    if (!input_buffer || !output_buffer)
      break;

    auto job_record = std::move(input_job_queue_.front());
    input_job_queue_.pop();
    EnqueueInput(job_record.get(), std::move(*input_buffer));
    EnqueueOutput(job_record.get(), std::move(*output_buffer));
    running_jobs_.emplace(std::move(job_record));
  }
}

void V4L2ImageProcessorBackend::Reset() {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  input_job_queue_ = {};
  running_jobs_ = {};
}

bool V4L2ImageProcessorBackend::ReconfigureV4L2Format(const gfx::Size& size,
                                                      enum v4l2_buf_type type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = type;
  if (device_->Ioctl(VIDIOC_G_FMT, &format) != 0) {
    VPLOGF(1) << "ioctl() failed: VIDIOC_G_FMT";
    return false;
  }

  if (static_cast<int>(format.fmt.pix_mp.width) == size.width() &&
      static_cast<int>(format.fmt.pix_mp.height) == size.height()) {
    return true;
  }
  format.fmt.pix_mp.width = size.width();
  format.fmt.pix_mp.height = size.height();
  if (device_->Ioctl(VIDIOC_S_FMT, &format) != 0) {
    VPLOGF(1) << "ioctl() failed: VIDIOC_S_FMT";
    return false;
  }

  auto queue = device_->GetQueue(type);
  const size_t num_buffers = queue->AllocatedBuffersCount();
  const v4l2_memory memory_type = queue->GetMemoryType();
  DCHECK_GT(num_buffers, 0u);
  return queue->DeallocateBuffers() &&
         AllocateV4L2Buffers(queue.get(), num_buffers, memory_type);
}

bool V4L2ImageProcessorBackend::CreateInputBuffers(size_t num_buffers) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(input_queue_, nullptr);

  input_queue_ = device_->GetQueue(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
  return input_queue_ && AllocateV4L2Buffers(input_queue_.get(), num_buffers,
                                             input_memory_type_);
}

bool V4L2ImageProcessorBackend::CreateOutputBuffers(size_t num_buffers) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(output_queue_, nullptr);

  output_queue_ = device_->GetQueue(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
  return output_queue_ && AllocateV4L2Buffers(output_queue_.get(), num_buffers,
                                              output_memory_type_);
}

void V4L2ImageProcessorBackend::TriggerPoll(bool poll_device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  poll_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V4L2ImageProcessorBackend::DevicePollTask,
                                poll_weak_this_, poll_device));
}

void V4L2ImageProcessorBackend::DevicePollTask(bool poll_device) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(poll_sequence_checker_);

  bool event_pending;
  if (!device_->Poll(poll_device, &event_pending)) {
    NotifyError();
    return;
  }

  // All processing should happen on ServiceDevice(), since we shouldn't
  // touch processor state from this thread.
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2ImageProcessorBackend::ServiceDevice, weak_this_));
}

void V4L2ImageProcessorBackend::ServiceDevice() {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(input_queue_);

  Dequeue();
  ProcessJobs();

  if (!device_->ClearDevicePollInterrupt()) {
    NotifyError();
    return;
  }

  const bool poll_device = (input_queue_->QueuedBuffersCount() > 0 ||
                            output_queue_->QueuedBuffersCount() > 0);
  TriggerPoll(poll_device);

  DVLOGF(3) << __func__ << ": buffer counts: INPUT[" << input_job_queue_.size()
            << "] => DEVICE[" << input_queue_->FreeBuffersCount() << "+"
            << input_queue_->QueuedBuffersCount() << "/"
            << input_queue_->AllocatedBuffersCount() << "->"
            << output_queue_->AllocatedBuffersCount() -
                   output_queue_->QueuedBuffersCount()
            << "+" << output_queue_->QueuedBuffersCount() << "/"
            << output_queue_->AllocatedBuffersCount() << "]";
}

void V4L2ImageProcessorBackend::EnqueueInput(const JobRecord* job_record,
                                             V4L2WritableBufferRef buffer) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(input_queue_);

  const size_t old_inputs_queued = input_queue_->QueuedBuffersCount();
  if (!EnqueueInputRecord(job_record, std::move(buffer))) {
    NotifyError();
    return;
  }

  if (old_inputs_queued == 0 && input_queue_->QueuedBuffersCount() != 0) {
    // We started up a previously empty queue.
    // Queue state changed; signal interrupt.
    if (!device_->SetDevicePollInterrupt()) {
      NotifyError();
      return;
    }
    // VIDIOC_STREAMON if we haven't yet.
    if (!input_queue_->Streamon())
      return;
  }
}

void V4L2ImageProcessorBackend::EnqueueOutput(JobRecord* job_record,
                                              V4L2WritableBufferRef buffer) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(output_queue_);

  const int old_outputs_queued = output_queue_->QueuedBuffersCount();
  if (!EnqueueOutputRecord(job_record, std::move(buffer))) {
    NotifyError();
    return;
  }

  if (old_outputs_queued == 0 && output_queue_->QueuedBuffersCount() != 0) {
    // We just started up a previously empty queue.
    // Queue state changed; signal interrupt.
    if (!device_->SetDevicePollInterrupt()) {
      NotifyError();
      return;
    }
    // Start VIDIOC_STREAMON if we haven't yet.
    if (!output_queue_->Streamon())
      return;
  }
}

// static
void V4L2ImageProcessorBackend::V4L2VFRecycleThunk(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::optional<base::WeakPtr<V4L2ImageProcessorBackend>> image_processor,
    V4L2ReadableBufferRef buf) {
  DVLOGF(4);
  DCHECK(image_processor);

  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&V4L2ImageProcessorBackend::V4L2VFRecycleTask,
                                *image_processor, std::move(buf)));
}

void V4L2ImageProcessorBackend::V4L2VFRecycleTask(V4L2ReadableBufferRef buf) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Release the buffer reference so we can directly call ProcessJobs()
  // knowing that we have an extra output buffer.
#if DCHECK_IS_ON()
  size_t original_free_buffers_count = output_queue_->FreeBuffersCount();
#endif
  buf = nullptr;
#if DCHECK_IS_ON()
  DCHECK_EQ(output_queue_->FreeBuffersCount(), original_free_buffers_count + 1);
#endif

  // A CAPTURE buffer has just been returned to the free list, let's see if
  // we can make progress on some jobs.
  ProcessJobs();
}

void V4L2ImageProcessorBackend::Dequeue() {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(input_queue_);
  DCHECK(output_queue_);
  DCHECK(input_queue_->IsStreaming());

  // Dequeue completed input (VIDEO_OUTPUT) buffers,
  // and recycle to the free list.
  while (input_queue_->QueuedBuffersCount() > 0) {
    auto [res, buffer] = input_queue_->DequeueBuffer();
    if (!res) {
      NotifyError();
      return;
    }
    if (!buffer) {
      // No error occurred, we are just out of buffers to dequeue.
      break;
    }
  }

  // Dequeue completed output (VIDEO_CAPTURE) buffers.
  // Return the finished buffer to the client via the job ready callback.
  while (output_queue_->QueuedBuffersCount() > 0) {
    DCHECK(output_queue_->IsStreaming());

    auto [res, buffer] = output_queue_->DequeueBuffer();
    if (!res) {
      NotifyError();
      return;
    } else if (!buffer) {
      break;
    }

    // Jobs are always processed in FIFO order.
    if (running_jobs_.empty() ||
        running_jobs_.front()->output_buffer_id != buffer->BufferId()) {
      DVLOGF(3) << "previous Reset() abandoned the job, ignore.";
      continue;
    }
    std::unique_ptr<JobRecord> job_record = std::move(running_jobs_.front());
    running_jobs_.pop();

    scoped_refptr<FrameResource> output_frame;
    switch (output_memory_type_) {
      case V4L2_MEMORY_MMAP:
        // Wrap the V4L2 frame into another one with a destruction observer so
        // we can reuse the MMAP buffer once the client is done with it.
        {
          const auto& orig_frame = buffer->GetFrameResource();
          // Need to wrap the original frame since the timestamp needs to be
          // set.
          output_frame = orig_frame->CreateWrappingFrame();
          // Because VideoFrame destruction callback might be executed on any
          // sequence, we use a thunk to post the task to the current task
          // runner.
          output_frame->AddDestructionObserver(
              base::BindOnce(&V4L2ImageProcessorBackend::V4L2VFRecycleThunk,
                             task_runner(), weak_this_, buffer));
          break;
        }
      case V4L2_MEMORY_DMABUF:
        output_frame = std::move(job_record->output_frame);
        break;

      default:
        NOTREACHED();
    }

    const auto timestamp = job_record->input_frame->timestamp();
    output_frame->set_timestamp(timestamp);
    output_frame->set_color_space(job_record->input_frame->ColorSpace());

    if (job_record->start_time) {
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
          "media", "V4L2ImageProcessorBackend::Process", TRACE_ID_LOCAL(this),
          job_record->start_time.value());
      TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP1(
          "media", "V4L2ImageProcessorBackend::Process", TRACE_ID_LOCAL(this),
          base::TimeTicks::Now(), "timestamp", timestamp.InMilliseconds());
    }

    if (!job_record->legacy_ready_cb.is_null()) {
      std::move(job_record->legacy_ready_cb)
          .Run(buffer->BufferId(), std::move(output_frame));
    } else {
      std::move(job_record->ready_cb).Run(std::move(output_frame));
    }
  }
}

bool V4L2ImageProcessorBackend::EnqueueInputRecord(
    const JobRecord* job_record,
    V4L2WritableBufferRef buffer) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(input_queue_);

  switch (input_memory_type_) {
    case V4L2_MEMORY_USERPTR: {
      const size_t num_planes =
          GetNumPlanesOfV4L2PixFmt(input_config_.fourcc.ToV4L2PixFmt());
      std::vector<void*> user_ptrs(num_planes);
      for (size_t i = 0; i < num_planes; ++i) {
        int bytes_used =
            VideoFrame::PlaneSize(job_record->input_frame->format(), i,
                                  input_config_.size)
                .GetArea();
        buffer.SetPlaneBytesUsed(i, bytes_used);
        user_ptrs[i] = const_cast<uint8_t*>(job_record->input_frame->data(i));
      }
      if (!std::move(buffer).QueueUserPtr(user_ptrs)) {
        VPLOGF(1) << "Failed to queue a DMABUF buffer to input queue";
        NotifyError();
        return false;
      }
      break;
    }
    case V4L2_MEMORY_DMABUF: {
      auto input_handle = CreateHandle(job_record->input_frame.get());
      if (!input_handle) {
        VLOGF(1) << "Failed to create native GpuMemoryBufferHandle";
        NotifyError();
        return false;
      }

      FillV4L2BufferByGpuMemoryBufferHandle(
          input_config_.fourcc, input_config_.size, *input_handle, &buffer);
      if (!std::move(buffer).QueueDMABuf(
              input_handle->native_pixmap_handle.planes)) {
        VPLOGF(1) << "Failed to queue a DMABUF buffer to input queue";
        NotifyError();
        return false;
      }
      break;
    }
    default:
      NOTREACHED();
  }
  DVLOGF(4) << "enqueued frame ts="
            << job_record->input_frame->timestamp().InMilliseconds()
            << " to device.";

  return true;
}

bool V4L2ImageProcessorBackend::EnqueueOutputRecord(
    JobRecord* job_record,
    V4L2WritableBufferRef buffer) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  job_record->output_buffer_id = buffer.BufferId();

  switch (buffer.Memory()) {
    case V4L2_MEMORY_MMAP:
      return std::move(buffer).QueueMMap();
    case V4L2_MEMORY_DMABUF: {
      auto output_handle = CreateHandle(job_record->output_frame.get());
      if (!output_handle) {
        VLOGF(1) << "Failed to create native GpuMemoryBufferHandle";
        NotifyError();
        return false;
      }

      FillV4L2BufferByGpuMemoryBufferHandle(
          output_config_.fourcc, output_config_.size, *output_handle, &buffer);
      return std::move(buffer).QueueDMABuf(
          output_handle->native_pixmap_handle.planes);
    }
    default:
      NOTREACHED();
  }
}

}  // namespace media
