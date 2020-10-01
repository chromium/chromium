// Copyright 2014 The Chromium Authors. All rights reserved.
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

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "media/base/color_plane_layout.h"
#include "media/base/scopedfd_helper.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/macros.h"

#define IOCTL_OR_ERROR_RETURN_VALUE(type, arg, value, type_str) \
  do {                                                          \
    if (device_->Ioctl(type, arg) != 0) {                       \
      VPLOGF(1) << "ioctl() failed: " << type_str;              \
      return value;                                             \
    }                                                           \
  } while (0)

#define IOCTL_OR_ERROR_RETURN_FALSE(type, arg) \
  IOCTL_OR_ERROR_RETURN_VALUE(type, arg, false, #type)

namespace media {

namespace {

enum v4l2_buf_type ToSingleV4L2Planar(enum v4l2_buf_type type) {
  switch (type) {
    case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
      return V4L2_BUF_TYPE_VIDEO_OUTPUT;
    case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
      return V4L2_BUF_TYPE_VIDEO_CAPTURE;
    default:
      return type;
  }
}

base::Optional<gfx::GpuMemoryBufferHandle> CreateHandle(
    const VideoFrame* frame) {
  gfx::GpuMemoryBufferHandle handle = CreateGpuMemoryBufferHandle(frame);

  if (handle.is_null() || handle.type != gfx::NATIVE_PIXMAP)
    return base::nullopt;
  return handle;
}

void FillV4L2BufferByGpuMemoryBufferHandle(
    const Fourcc& fourcc,
    const gfx::Size& coded_size,
    const gfx::GpuMemoryBufferHandle& gmb_handle,
    V4L2WritableBufferRef* buffer) {
  DCHECK_EQ(buffer->Memory(), V4L2_MEMORY_DMABUF);
  const size_t num_planes =
      V4L2Device::GetNumPlanesOfV4L2PixFmt(fourcc.ToV4L2PixFmt());
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
                         size_t num_buffers,
                         v4l2_memory memory_type) {
  DCHECK(queue);
  if (queue->AllocateBuffers(num_buffers, memory_type) == 0u)
    return false;

  if (queue->AllocatedBuffersCount() != num_buffers) {
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
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    scoped_refptr<V4L2Device> device,
    const PortConfig& input_config,
    const PortConfig& output_config,
    v4l2_memory input_memory_type,
    v4l2_memory output_memory_type,
    OutputMode output_mode,
    VideoRotation relative_rotation,
    size_t num_buffers,
    ErrorCB error_cb)
    : ImageProcessorBackend(input_config,
                            output_config,
                            output_mode,
                            relative_rotation,
                            std::move(error_cb),
                            std::move(backend_task_runner)),
      input_memory_type_(input_memory_type),
      output_memory_type_(output_memory_type),
      device_(device),
      num_buffers_(num_buffers),
      // We poll V4L2 device on this task runner, which blocks the task runner.
      // Therefore we use dedicated SingleThreadTaskRunner here.
      poll_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED)) {
  DVLOGF(2);
  DETACH_FROM_SEQUENCE(poll_sequence_checker_);
  DCHECK_NE(output_memory_type_, V4L2_MEMORY_USERPTR);

  backend_weak_this_ = backend_weak_this_factory_.GetWeakPtr();
  poll_weak_this_ = poll_weak_this_factory_.GetWeakPtr();
}

void V4L2ImageProcessorBackend::Destroy() {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);

  backend_weak_this_factory_.InvalidateWeakPtrs();

  if (input_queue_) {
    input_queue_->Streamoff();
    input_queue_->DeallocateBuffers();
    input_queue_ = nullptr;
  }
  if (output_queue_) {
    output_queue_->Streamoff();
    output_queue_->DeallocateBuffers();
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

  error_cb_.Run();
}

namespace {

v4l2_memory InputStorageTypeToV4L2Memory(VideoFrame::StorageType storage_type) {
  switch (storage_type) {
    case VideoFrame::STORAGE_OWNED_MEMORY:
    case VideoFrame::STORAGE_UNOWNED_MEMORY:
    case VideoFrame::STORAGE_SHMEM:
    case VideoFrame::STORAGE_MOJO_SHARED_BUFFER:
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
    const std::vector<OutputMode>& preferred_output_modes,
    VideoRotation relative_rotation,
    ErrorCB error_cb,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner) {
  for (const auto& output_mode : preferred_output_modes) {
    auto image_processor = V4L2ImageProcessorBackend::CreateWithOutputMode(
        device, num_buffers, input_config, output_config, output_mode,
        relative_rotation, error_cb, backend_task_runner);
    if (image_processor)
      return image_processor;
  }

  return nullptr;
}

// static
std::unique_ptr<ImageProcessorBackend>
V4L2ImageProcessorBackend::CreateWithOutputMode(
    scoped_refptr<V4L2Device> device,
    size_t num_buffers,
    const PortConfig& input_config,
    const PortConfig& output_config,
    const OutputMode& output_mode,
    VideoRotation relative_rotation,
    ErrorCB error_cb,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner) {
  VLOGF(2);
  DCHECK_GT(num_buffers, 0u);

  if (!device) {
    VLOGF(2) << "Failed creating V4L2Device";
    return nullptr;
  }

  // V4L2ImageProcessorBackend supports either DmaBuf-backed or memory-based
  // video frame for input.
  VideoFrame::StorageType input_storage_type = VideoFrame::STORAGE_UNKNOWN;
  for (auto input_type : input_config.preferred_storage_types) {
    v4l2_memory v4l2_memory_type = InputStorageTypeToV4L2Memory(input_type);
    if (v4l2_memory_type == V4L2_MEMORY_USERPTR ||
        v4l2_memory_type == V4L2_MEMORY_DMABUF) {
      input_storage_type = input_type;
      break;
    }
  }
  if (input_storage_type == VideoFrame::STORAGE_UNKNOWN) {
    VLOGF(2) << "Unsupported input storage type";
    return nullptr;
  }

  // V4L2ImageProcessorBackend only supports DmaBuf-backed video frame for
  // output.
  VideoFrame::StorageType output_storage_type = VideoFrame::STORAGE_UNKNOWN;
  for (auto output_type : output_config.preferred_storage_types) {
    v4l2_memory v4l2_memory_type = InputStorageTypeToV4L2Memory(output_type);
    if (v4l2_memory_type == V4L2_MEMORY_MMAP ||
        v4l2_memory_type == V4L2_MEMORY_DMABUF) {
      output_storage_type = output_type;
      break;
    }
  }
  if (output_storage_type == VideoFrame::STORAGE_UNKNOWN) {
    VLOGF(2) << "Unsupported output storage type";
    return nullptr;
  }

  const v4l2_memory input_memory_type =
      InputStorageTypeToV4L2Memory(input_storage_type);
  if (input_memory_type == 0) {
    VLOGF(1) << "Unsupported input storage type: " << input_storage_type;
    return nullptr;
  }

  const v4l2_memory output_memory_type =
      output_mode == OutputMode::ALLOCATE
          ? V4L2_MEMORY_MMAP
          : InputStorageTypeToV4L2Memory(output_storage_type);

  if (!device->IsImageProcessingSupported()) {
    VLOGF(1) << "V4L2ImageProcessorBackend not supported in this platform";
    return nullptr;
  }

  // V4L2IP now doesn't support rotation case, so return nullptr.
  if (relative_rotation != VIDEO_ROTATION_0) {
    VLOGF(1) << "Currently V4L2IP doesn't support rotation";
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

  auto image_processor = std::unique_ptr<
      V4L2ImageProcessorBackend, std::default_delete<ImageProcessorBackend>>(
      new V4L2ImageProcessorBackend(
          backend_task_runner, std::move(device),
          PortConfig(input_config.fourcc, negotiated_input_size, input_planes,
                     input_config.visible_rect, {input_storage_type}),
          PortConfig(output_config.fourcc, negotiated_output_size,
                     output_planes, output_config.visible_rect,
                     {output_storage_type}),
          input_memory_type, output_memory_type, output_mode, relative_rotation,
          num_buffers, std::move(error_cb)));

  // Initialize at |backend_task_runner_|.
  bool success = false;
  base::WaitableEvent done;
  auto init_cb = base::BindOnce(
      [](base::WaitableEvent* done, bool* success, bool value) {
        *success = value;
        done->Signal();
      },
      base::Unretained(&done), base::Unretained(&success));
  // Using base::Unretained() is safe because it is blocking call.
  backend_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&V4L2ImageProcessorBackend::Initialize,
                                base::Unretained(image_processor.get()),
                                std::move(init_cb)));
  done.Wait();
  if (!success) {
    // This needs to be destroyed on |backend_task_runner|.
    backend_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            base::DoNothing::Once<std::unique_ptr<ImageProcessorBackend>>(),
            std::move(image_processor)));
    return nullptr;
  }

  return image_processor;
}

void V4L2ImageProcessorBackend::Initialize(InitCB init_cb) {
  DVLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);

  // Capabilities check.
  struct v4l2_capability caps;
  memset(&caps, 0, sizeof(caps));
  const __u32 kCapsRequired = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
  if (device_->Ioctl(VIDIOC_QUERYCAP, &caps) != 0) {
    VPLOGF(1) << "ioctl() failed: VIDIOC_QUERYCAP";
    std::move(init_cb).Run(false);
    return;
  }
  if ((caps.capabilities & kCapsRequired) != kCapsRequired) {
    VLOGF(1) << "Initialize(): ioctl() failed: VIDIOC_QUERYCAP: "
             << "caps check failed: 0x" << std::hex << caps.capabilities;
    std::move(init_cb).Run(false);
    return;
  }

  if (!CreateInputBuffers() || !CreateOutputBuffers()) {
    std::move(init_cb).Run(false);
    return;
  }

  // Enqueue a poll task with no devices to poll on - will wait only for the
  // poll interrupt.
  DVLOGF(3) << "starting device poll";
  poll_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V4L2ImageProcessorBackend::DevicePollTask,
                                poll_weak_this_, false));

  VLOGF(2) << "V4L2ImageProcessorBackend initialized for "
           << "input: " << input_config_.ToString()
           << ", output: " << output_config_.ToString();

  std::move(init_cb).Run(true);
}

// static
bool V4L2ImageProcessorBackend::IsSupported() {
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (!device)
    return false;

  return device->IsImageProcessingSupported();
}

// static
std::vector<uint32_t> V4L2ImageProcessorBackend::GetSupportedInputFormats() {
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (!device)
    return std::vector<uint32_t>();

  return device->GetSupportedImageProcessorPixelformats(
      V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
}

// static
std::vector<uint32_t> V4L2ImageProcessorBackend::GetSupportedOutputFormats() {
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (!device)
    return std::vector<uint32_t>();

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
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (!device ||
      !device->Open(V4L2Device::Type::kImageProcessor, input_pixelformat))
    return false;

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
              << V4L2Device::V4L2FormatToString(format);
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

void V4L2ImageProcessorBackend::ProcessLegacy(scoped_refptr<VideoFrame> frame,
                                              LegacyFrameReadyCB cb) {
  DVLOGF(4) << "ts=" << frame->timestamp().InMilliseconds();
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);

  if (output_memory_type_ != V4L2_MEMORY_MMAP) {
    NOTREACHED();
    return;
  }

  auto job_record = std::make_unique<JobRecord>();
  job_record->input_frame = frame;
  job_record->legacy_ready_cb = std::move(cb);

  input_job_queue_.emplace(std::move(job_record));
  ProcessJobsTask();
}

void V4L2ImageProcessorBackend::Process(scoped_refptr<VideoFrame> input_frame,
                                        scoped_refptr<VideoFrame> output_frame,
                                        FrameReadyCB cb) {
  DVLOGF(4) << "ts=" << input_frame->timestamp().InMilliseconds();
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);

  auto job_record = std::make_unique<JobRecord>();
  job_record->input_frame = std::move(input_frame);
  job_record->output_frame = std::move(output_frame);
  job_record->ready_cb = std::move(cb);

  input_job_queue_.emplace(std::move(job_record));
  ProcessJobsTask();
}

void V4L2ImageProcessorBackend::ProcessJobsTask() {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);

  while (!input_job_queue_.empty()) {
    if (!input_queue_->IsStreaming()) {
      const VideoFrame& input_frame =
          *(input_job_queue_.front()->input_frame.get());
      const gfx::Size input_buffer_size(input_frame.stride(0),
                                        input_frame.coded_size().height());
      if (!ReconfigureV4L2Format(input_buffer_size, input_frame.visible_rect(),
                                 V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)) {
        NotifyError();
        return;
      }
    }

    if (input_job_queue_.front()
            ->output_frame &&  // output_frame is nullptr in ALLOCATE mode.
        !output_queue_->IsStreaming()) {
      const VideoFrame& output_frame =
          *(input_job_queue_.front()->output_frame.get());
      const gfx::Size output_buffer_size(output_frame.stride(0),
                                         output_frame.coded_size().height());
      if (!ReconfigureV4L2Format(output_buffer_size,
                                 output_frame.visible_rect(),
                                 V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)) {
        NotifyError();
        return;
      }
    }

    // We need one input and one output buffer to schedule the job
    auto input_buffer = input_queue_->GetFreeBuffer();
    auto output_buffer = output_queue_->GetFreeBuffer();
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);

  input_job_queue_ = {};
  running_jobs_ = {};
}

bool V4L2ImageProcessorBackend::ApplyCrop(const gfx::Rect& visible_rect,
                                          enum v4l2_buf_type type) {
  struct v4l2_rect rect {};
  rect.left = visible_rect.x();
  rect.top = visible_rect.y();
  rect.width = visible_rect.width();
  rect.height = visible_rect.height();

  struct v4l2_selection selection_arg {};
  // Multiplanar buffer types are messed up in S_SELECTION API, so all drivers
  // don't necessarily work with MPLANE types. This issue is resolved with
  // kernel 4.13. As we use kernel < 4.13 today, we use single planar buffer
  // types. See
  // https://linuxtv.org/downloads/v4l-dvb-apis/uapi/v4l/vidioc-g-selection.html.
  selection_arg.type = ToSingleV4L2Planar(type);
  selection_arg.target =
      V4L2_TYPE_IS_OUTPUT(type) ? V4L2_SEL_TGT_CROP : V4L2_SEL_TGT_COMPOSE;

  selection_arg.r = rect;
  if (device_->Ioctl(VIDIOC_S_SELECTION, &selection_arg) == 0) {
    DVLOGF(2) << "VIDIOC_S_SELECTION is supported";
    rect = selection_arg.r;
  } else {
    DVLOGF(2) << "Fallback to VIDIOC_S/G_CROP";
    struct v4l2_crop crop {};
    crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    crop.c = rect;
    if (device_->Ioctl(VIDIOC_S_CROP, &crop) != 0) {
      VPLOGF(1) << "VIDIOC_S_CROP failed: ";
      return false;
    }
    rect = crop.c;
  }

  const gfx::Rect adjusted_visible_rect(rect.left, rect.top, rect.width,
                                        rect.height);
  if (visible_rect != adjusted_visible_rect) {
    VLOGF(1) << "Unsupported visible rectangle: " << visible_rect.ToString()
             << ", the rectangle adjusted by the driver: "
             << adjusted_visible_rect.ToString();
    return false;
  }
  return true;
}

bool V4L2ImageProcessorBackend::ReconfigureV4L2Format(
    const gfx::Size& size,
    const gfx::Rect& visible_rect,
    enum v4l2_buf_type type) {
  v4l2_format format{};
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
  if (!ApplyCrop(visible_rect, type)) {
    return false;
  }

  auto queue = device_->GetQueue(type);
  const size_t num_buffers = queue->AllocatedBuffersCount();
  const v4l2_memory memory_type = queue->GetMemoryType();
  DCHECK_GT(num_buffers, 0u);
  return queue->DeallocateBuffers() &&
         AllocateV4L2Buffers(queue.get(), num_buffers, memory_type);
}

bool V4L2ImageProcessorBackend::CreateInputBuffers() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);
  DCHECK_EQ(input_queue_, nullptr);

  struct v4l2_control control;
  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_ROTATE;
  control.value = 0;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_CTRL, &control);

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_HFLIP;
  control.value = 0;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_CTRL, &control);

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_VFLIP;
  control.value = 0;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_CTRL, &control);

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_ALPHA_COMPONENT;
  control.value = 255;
  if (device_->Ioctl(VIDIOC_S_CTRL, &control) != 0)
    DVLOGF(4) << "V4L2_CID_ALPHA_COMPONENT is not supported";

  if (!ApplyCrop(input_config_.visible_rect, V4L2_BUF_TYPE_VIDEO_OUTPUT)) {
    VLOGF(2) << "Failed to apply crop to input queue";
    return false;
  }

  input_queue_ = device_->GetQueue(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
  return input_queue_ && AllocateV4L2Buffers(input_queue_.get(), num_buffers_,
                                             input_memory_type_);
}

bool V4L2ImageProcessorBackend::CreateOutputBuffers() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);
  DCHECK_EQ(output_queue_, nullptr);

  if (!ApplyCrop(output_config_.visible_rect, V4L2_BUF_TYPE_VIDEO_CAPTURE)) {
    return false;
  }

  output_queue_ = device_->GetQueue(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
  return output_queue_ && AllocateV4L2Buffers(output_queue_.get(), num_buffers_,
                                              output_memory_type_);
}

void V4L2ImageProcessorBackend::DevicePollTask(bool poll_device) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(poll_sequence_checker_);

  bool event_pending;
  if (!device_->Poll(poll_device, &event_pending)) {
    NotifyError();
    return;
  }

  // All processing should happen on ServiceDeviceTask(), since we shouldn't
  // touch processor state from this thread.
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V4L2ImageProcessorBackend::ServiceDeviceTask,
                                backend_weak_this_));
}

void V4L2ImageProcessorBackend::ServiceDeviceTask() {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);
  DCHECK(input_queue_);

  Dequeue();
  ProcessJobsTask();

  if (!device_->ClearDevicePollInterrupt()) {
    NotifyError();
    return;
  }

  bool poll_device = (input_queue_->QueuedBuffersCount() > 0 ||
                      output_queue_->QueuedBuffersCount() > 0);

  poll_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V4L2ImageProcessorBackend::DevicePollTask,
                                poll_weak_this_, poll_device));

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);
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
    base::Optional<base::WeakPtr<V4L2ImageProcessorBackend>> image_processor,
    V4L2ReadableBufferRef buf) {
  DVLOGF(4);
  DCHECK(image_processor);

  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&V4L2ImageProcessorBackend::V4L2VFRecycleTask,
                                *image_processor, std::move(buf)));
}

void V4L2ImageProcessorBackend::V4L2VFRecycleTask(V4L2ReadableBufferRef buf) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);

  // Release the buffer reference so we can directly call ProcessJobsTask()
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
  ProcessJobsTask();
}

void V4L2ImageProcessorBackend::Dequeue() {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);
  DCHECK(input_queue_);
  DCHECK(output_queue_);
  DCHECK(input_queue_->IsStreaming());

  // Dequeue completed input (VIDEO_OUTPUT) buffers,
  // and recycle to the free list.
  while (input_queue_->QueuedBuffersCount() > 0) {
    bool res;
    V4L2ReadableBufferRef buffer;
    std::tie(res, buffer) = input_queue_->DequeueBuffer();
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

    bool res;
    V4L2ReadableBufferRef buffer;
    std::tie(res, buffer) = output_queue_->DequeueBuffer();
    if (!res) {
      NotifyError();
      return;
    } else if (!buffer) {
      break;
    }

    // Jobs are always processed in FIFO order.
    if (running_jobs_.empty() ||
        running_jobs_.front()->output_buffer_id != buffer->BufferId()) {
      DVLOGF(3) << "previous Reset() abondoned the job, ignore.";
      continue;
    }
    std::unique_ptr<JobRecord> job_record = std::move(running_jobs_.front());
    running_jobs_.pop();

    scoped_refptr<VideoFrame> output_frame;
    switch (output_memory_type_) {
      case V4L2_MEMORY_MMAP:
        // Wrap the V4L2 VideoFrame into another one with a destruction observer
        // so we can reuse the MMAP buffer once the client is done with it.
        {
          const auto& orig_frame = buffer->GetVideoFrame();
          output_frame = VideoFrame::WrapVideoFrame(
              orig_frame, orig_frame->format(), orig_frame->visible_rect(),
              orig_frame->natural_size());
          // Because VideoFrame destruction callback might be executed on any
          // sequence, we use a thunk to post the task to
          // |backend_task_runner_|.
          output_frame->AddDestructionObserver(
              base::BindOnce(&V4L2ImageProcessorBackend::V4L2VFRecycleThunk,
                             backend_task_runner_, backend_weak_this_, buffer));
          break;
        }
      case V4L2_MEMORY_DMABUF:
        output_frame = std::move(job_record->output_frame);
        break;

      default:
        NOTREACHED();
        return;
    }

    output_frame->set_timestamp(job_record->input_frame->timestamp());

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);
  DCHECK(input_queue_);

  switch (input_memory_type_) {
    case V4L2_MEMORY_USERPTR: {
      const size_t num_planes = V4L2Device::GetNumPlanesOfV4L2PixFmt(
          input_config_.fourcc.ToV4L2PixFmt());
      std::vector<void*> user_ptrs(num_planes);
      for (size_t i = 0; i < num_planes; ++i) {
        int bytes_used =
            VideoFrame::PlaneSize(job_record->input_frame->format(), i,
                                  input_config_.size)
                .GetArea();
        buffer.SetPlaneBytesUsed(i, bytes_used);
        user_ptrs[i] = job_record->input_frame->data(i);
      }
      std::move(buffer).QueueUserPtr(user_ptrs);
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
      std::move(buffer).QueueDMABuf(input_handle->native_pixmap_handle.planes);
      break;
    }
    default:
      NOTREACHED();
      return false;
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);

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
      return false;
  }
}

}  // namespace media
