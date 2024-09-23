// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/legacy/v4l2_video_decode_accelerator.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_types.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/native_pixmap_frame_resource.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#ifdef SUPPORT_MT21_PIXEL_FORMAT_SOFTWARE_DECOMPRESSION
#include "media/gpu/chromeos/video_frame_resource.h"
#endif
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_image_processor_backend.h"
#include "media/gpu/v4l2/v4l2_utils.h"
#include "media/gpu/v4l2/v4l2_vda_helpers.h"
#include "media/gpu/video_frame_mapper.h"
#include "media/gpu/video_frame_mapper_factory.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_pixmap_handle.h"

#define NOTIFY_ERROR(x)                      \
  do {                                       \
    VLOGF(1) << "Setting error state:" << x; \
    SetErrorState(x);                        \
  } while (0)

#define IOCTL_OR_ERROR_RETURN_VALUE(type, arg, value, type_str) \
  do {                                                          \
    if (device_->Ioctl(type, arg) != 0) {                       \
      PLOG(ERROR) << "ioctl() failed: " << type_str;            \
      NOTIFY_ERROR(PLATFORM_FAILURE);                           \
      return value;                                             \
    }                                                           \
  } while (0)

#define IOCTL_OR_ERROR_RETURN(type, arg) \
  IOCTL_OR_ERROR_RETURN_VALUE(type, arg, ((void)0), #type)

#define IOCTL_OR_ERROR_RETURN_FALSE(type, arg) \
  IOCTL_OR_ERROR_RETURN_VALUE(type, arg, false, #type)

#define IOCTL_OR_LOG_ERROR(type, arg)           \
  do {                                          \
    if (device_->Ioctl(type, arg) != 0)         \
      VPLOGF(1) << "ioctl() failed: " << #type; \
  } while (0)

namespace media {

namespace {

bool IsVp9KSVCStream(uint32_t input_format_fourcc,
                     const DecoderBuffer& decoder_buffer) {
  return input_format_fourcc == V4L2_PIX_FMT_VP9 &&
         decoder_buffer.has_side_data() &&
         !decoder_buffer.side_data()->spatial_layers.empty();
}

}  // namespace

static const std::vector<uint32_t> kSupportedInputFourCCs = {
    V4L2_PIX_FMT_H264,
    V4L2_PIX_FMT_VP8,
    V4L2_PIX_FMT_VP9,
};

// static
base::AtomicRefCount V4L2VideoDecodeAccelerator::num_instances_(0);

struct V4L2VideoDecodeAccelerator::BitstreamBufferRef {
  BitstreamBufferRef(
      base::WeakPtr<Client>& client,
      scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
      scoped_refptr<DecoderBuffer> buffer,
      int32_t input_id);
  ~BitstreamBufferRef();

  const base::WeakPtr<Client> client;
  const scoped_refptr<base::SequencedTaskRunner> client_task_runner;
  scoped_refptr<DecoderBuffer> buffer;
  size_t bytes_used;
  const int32_t input_id;
};

V4L2VideoDecodeAccelerator::BitstreamBufferRef::BitstreamBufferRef(
    base::WeakPtr<Client>& client,
    scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
    scoped_refptr<DecoderBuffer> buffer,
    int32_t input_id)
    : client(client),
      client_task_runner(client_task_runner),
      buffer(std::move(buffer)),
      bytes_used(0),
      input_id(input_id) {}

V4L2VideoDecodeAccelerator::BitstreamBufferRef::~BitstreamBufferRef() {
  if (input_id >= 0) {
    client_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&Client::NotifyEndOfBitstreamBuffer, client, input_id));
  }
}

V4L2VideoDecodeAccelerator::OutputRecord::OutputRecord()
    : picture_id(-1), cleared(false) {}

V4L2VideoDecodeAccelerator::OutputRecord::OutputRecord(OutputRecord&&) =
    default;

V4L2VideoDecodeAccelerator::OutputRecord::~OutputRecord() {}

V4L2VideoDecodeAccelerator::PictureRecord::PictureRecord(bool cleared,
                                                         const Picture& picture)
    : cleared(cleared), picture(picture) {}

V4L2VideoDecodeAccelerator::PictureRecord::~PictureRecord() {}

V4L2VideoDecodeAccelerator::V4L2VideoDecodeAccelerator(
    scoped_refptr<V4L2Device> device)
    : can_use_decoder_(num_instances_.Increment() < kMaxNumOfInstances),
      child_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      decoder_thread_("V4L2DecoderThread"),
      decoder_state_(kUninitialized),
      output_mode_(Config::OutputMode::kAllocate),
      device_(std::move(device)),
      decoder_delay_bitstream_buffer_id_(-1),
      decoder_decode_buffer_tasks_scheduled_(0),
      decoder_flushing_(false),
      decoder_cmd_supported_(false),
      flush_awaiting_last_output_buffer_(false),
      reset_pending_(false),
      output_dpb_size_(0),
      picture_clearing_count_(0),
      device_poll_thread_("V4L2DevicePollThread"),
      input_format_fourcc_(0),
      weak_this_factory_(this) {
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

V4L2VideoDecodeAccelerator::~V4L2VideoDecodeAccelerator() {
  DCHECK(!decoder_thread_.IsRunning());
  DCHECK(!device_poll_thread_.IsRunning());
  DVLOGF(2);

  // These maps have members that should be manually destroyed, e.g. file
  // descriptors, mmap() segments, etc.
  DCHECK(output_buffer_map_.empty());

  num_instances_.Decrement();
}

bool V4L2VideoDecodeAccelerator::Initialize(const Config& config,
                                            Client* client) {
  VLOGF(2) << "profile: " << config.profile
           << ", output_mode=" << static_cast<int>(config.output_mode);
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(decoder_state_, kUninitialized);

  if (!can_use_decoder_) {
    VLOGF(1) << "Reached the maximum number of decoder instances";
    return false;
  }

  if (config.is_encrypted()) {
    NOTREACHED_IN_MIGRATION()
        << "Encrypted streams are not supported for this VDA";
    return false;
  }

  if (config.output_mode != Config::OutputMode::kAllocate &&
      config.output_mode != Config::OutputMode::kImport) {
    NOTREACHED_IN_MIGRATION()
        << "Only ALLOCATE and IMPORT OutputModes are supported";
    return false;
  }

  client_ptr_factory_.reset(new base::WeakPtrFactory<Client>(client));
  client_ = client_ptr_factory_->GetWeakPtr();
  // If we haven't been set up to decode on separate sequence via
  // TryToSetupDecodeOnSeparateSequence(), use the main thread/client for
  // decode tasks.
  if (!decode_task_runner_) {
    decode_task_runner_ = child_task_runner_;
    DCHECK(!decode_client_);
    decode_client_ = client_;
  }

  decoder_state_ = kInitialized;

  if (!decoder_thread_.Start()) {
    LOG(ERROR) << "decoder thread failed to start";
    return false;
  }

  bool result = false;
  base::WaitableEvent done;
  decoder_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2VideoDecodeAccelerator::InitializeTask,
                     base::Unretained(this), config, &result, &done));
  done.Wait();

  return result;
}

void V4L2VideoDecodeAccelerator::InitializeTask(const Config& config,
                                                bool* result,
                                                base::WaitableEvent* done) {
  DVLOGF(3);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(result, nullptr);
  DCHECK_NE(done, nullptr);
  DCHECK_EQ(decoder_state_, kInitialized);
  TRACE_EVENT0("media,gpu", "V4L2VDA::InitializeTask");

  // The client can keep going as soon as the configuration is checked.
  // Store the result to the local value to see the result even after |*result|
  // is released.
  bool config_result = CheckConfig(config);
  *result = config_result;
  done->Signal();

  // No need to keep going is configuration is not supported.
  if (!config_result)
    return;

  container_color_space_ = config.container_color_space;

  frame_splitter_ =
      v4l2_vda_helpers::InputBufferFragmentSplitter::CreateFromProfile(
          config.profile);
  if (!frame_splitter_) {
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "media::V4l2VideoDecodeAccelerator", decoder_thread_.task_runner());

  // Subscribe to the resolution change event.
  struct v4l2_event_subscription sub;
  memset(&sub, 0, sizeof(sub));
  sub.type = V4L2_EVENT_SOURCE_CHANGE;
  IOCTL_OR_ERROR_RETURN(VIDIOC_SUBSCRIBE_EVENT, &sub);

  if (!CreateInputBuffers()) {
    LOG(ERROR) << "Failed CreatingInputBuffers()";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  decoder_cmd_supported_ = IsDecoderCmdSupported();

  StartDevicePoll();
}

bool V4L2VideoDecodeAccelerator::CheckConfig(const Config& config) {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  input_format_fourcc_ = VideoCodecProfileToV4L2PixFmt(config.profile, false);

  if (input_format_fourcc_ == V4L2_PIX_FMT_INVALID ||
      !device_->Open(V4L2Device::Type::kDecoder, input_format_fourcc_)) {
    VLOGF(1) << "Failed to open device for profile: " << config.profile
             << " fourcc: " << FourccToString(input_format_fourcc_);
    return false;
  }

  // Capabilities check.
  struct v4l2_capability caps;
  const __u32 kCapsRequired = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QUERYCAP, &caps);
  if ((caps.capabilities & kCapsRequired) != kCapsRequired) {
    VLOGF(1) << "ioctl() failed: VIDIOC_QUERYCAP"
             << ", caps check failed: 0x" << std::hex << caps.capabilities;
    return false;
  }

  output_mode_ = config.output_mode;

  input_queue_ = device_->GetQueue(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
  if (!input_queue_)
    return false;

  output_queue_ = device_->GetQueue(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
  if (!output_queue_)
    return false;

  if (!SetupFormats())
    return false;

  // We have confirmed that |config| is supported, tell the good news to the
  // client.
  return true;
}

void V4L2VideoDecodeAccelerator::Decode(BitstreamBuffer bitstream_buffer) {
  Decode(bitstream_buffer.ToDecoderBuffer(), bitstream_buffer.id());
}

void V4L2VideoDecodeAccelerator::Decode(scoped_refptr<DecoderBuffer> buffer,
                                        int32_t bitstream_id) {
  DVLOGF(4) << "input_id=" << bitstream_id
            << ", size=" << (buffer ? buffer->size() : 0);
  DCHECK(decode_task_runner_->RunsTasksInCurrentSequence());

  if (bitstream_id < 0) {
    LOG(ERROR) << "Invalid bitstream buffer, id: " << bitstream_id;
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  // DecodeTask() will take care of running a DecodeBufferTask().
  decoder_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2VideoDecodeAccelerator::DecodeTask,
                     base::Unretained(this), std::move(buffer), bitstream_id));
}

void V4L2VideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<PictureBuffer>& buffers) {
  VLOGF(2) << "buffer_count=" << buffers.size();
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  decoder_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2VideoDecodeAccelerator::AssignPictureBuffersTask,
                     base::Unretained(this), buffers));
}

void V4L2VideoDecodeAccelerator::AssignPictureBuffersTask(
    const std::vector<PictureBuffer>& buffers) {
  VLOGF(2);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(decoder_state_, kAwaitingPictureBuffers);
  DCHECK(output_queue_);
  TRACE_EVENT1("media,gpu", "V4L2VDA::AssignPictureBuffersTask", "buffers_size",
               buffers.size());

  if (IsDestroyPending())
    return;

  uint32_t req_buffer_count = output_dpb_size_ + kDpbOutputBufferExtraCount;
  if (image_processor_device_)
    req_buffer_count += kDpbOutputBufferExtraCountForImageProcessor;

  if (buffers.size() < req_buffer_count) {
    LOG(ERROR) << "Failed to provide requested picture buffers. (Got "
               << buffers.size() << ", requested " << req_buffer_count << ")";
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  const bool prefer_software_mt21 =
#ifdef SUPPORT_MT21_PIXEL_FORMAT_SOFTWARE_DECOMPRESSION
      base::FeatureList::IsEnabled(media::kPreferSoftwareMT21);
#else
      false;
#endif
  enum v4l2_memory memory;
  if (!image_processor_device_ && !prefer_software_mt21 &&
      output_mode_ == Config::OutputMode::kImport) {
    memory = V4L2_MEMORY_DMABUF;
  } else {
    memory = V4L2_MEMORY_MMAP;
  }

  if (output_queue_->AllocateBuffers(buffers.size(), memory,
                                     prefer_software_mt21) == 0) {
    LOG(ERROR) << "Failed to request buffers!";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  if (output_queue_->AllocatedBuffersCount() != buffers.size()) {
    LOG(ERROR) << "Could not allocate requested number of output buffers";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  DCHECK(output_buffer_map_.empty());
  DCHECK(output_wait_map_.empty());
  output_buffer_map_.resize(buffers.size());
  if (image_processor_device_ && output_mode_ == Config::OutputMode::kAllocate) {
    if (!CreateImageProcessor())
      return;
  }

  // Reserve all buffers until ImportBufferForPictureTask() is called
  std::vector<V4L2WritableBufferRef> v4l2_buffers;
  while (auto buffer_opt = output_queue_->GetFreeBuffer())
    v4l2_buffers.push_back(std::move(*buffer_opt));

  // Now setup the output record for each buffer and import it if needed.
  for (auto&& buffer : v4l2_buffers) {
    const int i = buffer.BufferId();

    OutputRecord& output_record = output_buffer_map_[i];
    DCHECK_EQ(output_record.picture_id, -1);
    DCHECK(!output_record.cleared);

    output_record.picture_id = buffers[i].id();

    // We move the buffer into output_wait_map_, so get a reference to
    // its video frame if we need it to create the native pixmap for import.
    scoped_refptr<FrameResource> frame;
    if (output_mode_ == Config::OutputMode::kAllocate &&
        !image_processor_device_)
      frame = buffer.GetFrameResource();

    // The buffer will remain here until ImportBufferForPicture is called,
    // either by the client, or by ourselves, if we are allocating.
    DCHECK_EQ(output_wait_map_.count(buffers[i].id()), 0u);
    output_wait_map_.emplace(buffers[i].id(), std::move(buffer));

    if (output_mode_ == Config::OutputMode::kAllocate) {
      gfx::NativePixmapHandle native_pixmap;

      // If we are using an image processor, the DMABufs that we need to import
      // are those of the image processor's buffers, not the decoders. So
      // pass an empty native pixmap in that case.
      if (!image_processor_device_) {
        // TODO(nhebert): drop usage of CreateGpuMemoryBufferHandle(), which
        // duplicates FD's, when a NativePixmap-based FrameResource is
        // available.
        native_pixmap =
            frame->CreateGpuMemoryBufferHandle().native_pixmap_handle;
      }

      ImportBufferForPictureTask(output_record.picture_id,
                                 std::move(native_pixmap));
    }  // else we'll get triggered via ImportBufferForPicture() from client.

    DVLOGF(3) << "buffer[" << i << "]: picture_id=" << output_record.picture_id;
  }

  if (output_mode_ == Config::OutputMode::kAllocate) {
    ScheduleDecodeBufferTaskIfNeeded();
  }
}

void V4L2VideoDecodeAccelerator::ImportBufferForPicture(
    int32_t picture_buffer_id,
    VideoPixelFormat pixel_format,
    gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle) {
  DVLOGF(3) << "picture_buffer_id=" << picture_buffer_id;
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  if (output_mode_ != Config::OutputMode::kImport) {
    LOG(ERROR) << "Cannot import in non-import mode";
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  decoder_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &V4L2VideoDecodeAccelerator::ImportBufferForPictureForImportTask,
          base::Unretained(this), picture_buffer_id, pixel_format,
          std::move(gpu_memory_buffer_handle.native_pixmap_handle)));
}

void V4L2VideoDecodeAccelerator::ImportBufferForPictureForImportTask(
    int32_t picture_buffer_id,
    VideoPixelFormat pixel_format,
    gfx::NativePixmapHandle handle) {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  // |output_format_fourcc_| is the output format of the decoder. It is not
  // the final output format from the image processor (if exists).
  // Use |egl_image_format_fourcc_|, it will be the final output format.
  if (pixel_format != egl_image_format_fourcc_->ToVideoPixelFormat()) {
    LOG(ERROR) << "Unsupported import format: " << pixel_format << ", expected "
               << VideoPixelFormatToString(
                      egl_image_format_fourcc_->ToVideoPixelFormat());
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  for (const auto& plane : handle.planes) {
    DVLOGF(3) << ": offset=" << plane.offset << ", stride=" << plane.stride;
  }

  ImportBufferForPictureTask(picture_buffer_id, std::move(handle));
}

void V4L2VideoDecodeAccelerator::ImportBufferForPictureTask(
    int32_t picture_buffer_id,
    gfx::NativePixmapHandle handle) {
  DVLOGF(3) << "picture_buffer_id=" << picture_buffer_id
            << ", handle.planes.size()=" << handle.planes.size();
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  TRACE_EVENT2("media,gpu", "V4L2VDA::ImportBufferForPictureTask",
               "picture_buffer_id", picture_buffer_id, "handle.planes",
               handle.planes.size());

  if (IsDestroyPending())
    return;

  const auto iter = base::ranges::find(output_buffer_map_, picture_buffer_id,
                                       &OutputRecord::picture_id);
  if (iter == output_buffer_map_.end()) {
    // It's possible that we've already posted a DismissPictureBuffer for this
    // picture, but it has not yet executed when this ImportBufferForPicture was
    // posted to us by the client. In that case just ignore this (we've already
    // dismissed it and accounted for that).
    DVLOGF(3) << "got picture id=" << picture_buffer_id
              << " not in use (anymore?).";
    return;
  }

  if (!output_wait_map_.count(iter->picture_id)) {
    LOG(ERROR) << "Passed buffer is not waiting to be imported";
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  // TODO(crbug.com/41469754): ARC++ may adjust the size of the buffer due to
  // allocator constraints, but the VDA API does not provide a way for it to
  // communicate the actual buffer size. If we are importing, make sure that the
  // actual buffer size is coherent with what we expect, and adjust our size if
  // needed.
  if (output_mode_ == Config::OutputMode::kImport) {
    DCHECK_GT(handle.planes.size(), 0u);
    const gfx::Size handle_size = v4l2_vda_helpers::NativePixmapSizeFromHandle(
        handle, *egl_image_format_fourcc_, egl_image_size_);

    // If this is the first picture, then adjust the EGL width.
    // Otherwise just check that it remains the same.
    if (decoder_state_ == kAwaitingPictureBuffers) {
      DCHECK_GE(handle_size.width(), egl_image_size_.width());
      DVLOGF(3) << "Original egl_image_size=" << egl_image_size_.ToString()
                << ", adjusted buffer size=" << handle_size.ToString();
      egl_image_size_ = handle_size;
    }
    DCHECK_EQ(egl_image_size_, handle_size);

#ifdef SUPPORT_MT21_PIXEL_FORMAT_SOFTWARE_DECOMPRESSION
    if (base::FeatureList::IsEnabled(media::kPreferSoftwareMT21) &&
        !mt21_decompressor_) {
      mt21_decompressor_ = std::make_unique<MT21Decompressor>(coded_size_);
    }
#endif

    // For allocate mode, the IP will already have been created in
    // AssignPictureBuffersTask.
    // Note: usage of the MT21 software decompressor disables the image
    // processor.
    if (image_processor_device_ && !image_processor_
#ifdef SUPPORT_MT21_PIXEL_FORMAT_SOFTWARE_DECOMPRESSION
        && !mt21_decompressor_
#endif
    ) {
      DCHECK_EQ(kAwaitingPictureBuffers, decoder_state_);
      // This is the first buffer import. Create the image processor and change
      // the decoder state. The client may adjust the coded width. We don't have
      // the final coded size in AssignPictureBuffers yet. Use the adjusted
      // coded width to create the image processor.
      if (!CreateImageProcessor())
        return;
    }
  }

  if (reset_pending_) {
    FinishReset();
  }

  if (decoder_state_ == kAwaitingPictureBuffers) {
    decoder_state_ = kDecoding;
    DVLOGF(3) << "Change state to kDecoding";
  }

  // If we are importing, create the output FrameResource that we will render
  // into.
  if (output_mode_ == Config::OutputMode::kImport) {
    DCHECK_GT(handle.planes.size(), 0u);
    DCHECK(!iter->output_frame);
    // Duplicate the buffer FDs for the output frame.
    std::vector<base::ScopedFD> duped_fds;
    std::vector<ColorPlaneLayout> color_planes;
    for (const gfx::NativePixmapPlane& plane : handle.planes) {
      duped_fds.emplace_back(HANDLE_EINTR(dup(plane.fd.get())));
      if (!duped_fds.back().is_valid()) {
        PLOG(ERROR) << "Failed to duplicate plane FD!";
        NOTIFY_ERROR(PLATFORM_FAILURE);
        return;
      }
      color_planes.push_back(
          ColorPlaneLayout(base::checked_cast<int32_t>(plane.stride),
                           base::checked_cast<size_t>(plane.offset),
                           base::checked_cast<size_t>(plane.size)));
    }
    auto layout = VideoFrameLayout::CreateWithPlanes(
        egl_image_format_fourcc_->ToVideoPixelFormat(), egl_image_size_,
        std::move(color_planes));
    if (!layout) {
      LOG(ERROR) << "Cannot create layout!";
      NOTIFY_ERROR(INVALID_ARGUMENT);
      return;
    }

    iter->output_frame = NativePixmapFrameResource::Create(
        *layout, gfx::Rect(visible_size_), visible_size_, std::move(duped_fds),
        base::TimeDelta());
  }

  // The buffer can now be used for decoding
  DCHECK_EQ(output_wait_map_.count(picture_buffer_id), 1u);
  output_wait_map_.erase(picture_buffer_id);
  if (decoder_state_ != kChangingResolution) {
    Enqueue();
    ScheduleDecodeBufferTaskIfNeeded();
  }
}

void V4L2VideoDecodeAccelerator::ReusePictureBuffer(int32_t picture_buffer_id) {
  DVLOGF(4) << "picture_buffer_id=" << picture_buffer_id;
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  decoder_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2VideoDecodeAccelerator::ReusePictureBufferTask,
                     base::Unretained(this), picture_buffer_id));
}

void V4L2VideoDecodeAccelerator::Flush() {
  VLOGF(2);
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  decoder_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2VideoDecodeAccelerator::FlushTask,
                                base::Unretained(this)));
}

void V4L2VideoDecodeAccelerator::Reset() {
  VLOGF(2);
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  decoder_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2VideoDecodeAccelerator::ResetTask,
                                base::Unretained(this)));
}

void V4L2VideoDecodeAccelerator::Destroy() {
  VLOGF(2);
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  // Signal any waiting/sleeping tasks to early exit as soon as possible to
  // avoid waiting too long for the decoder_thread_ to Stop().
  destroy_pending_.Signal();

  // We're destroying; cancel all callbacks.
  client_ptr_factory_.reset();
  weak_this_factory_.InvalidateWeakPtrs();

  // If the decoder thread is running, destroy using posted task.
  if (decoder_thread_.IsRunning()) {
    decoder_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&V4L2VideoDecodeAccelerator::DestroyTask,
                                  base::Unretained(this)));
    // DestroyTask() will cause the decoder_thread_ to flush all tasks.
    decoder_thread_.Stop();
  }

  delete this;
  VLOGF(2) << "Destroyed.";
}

bool V4L2VideoDecodeAccelerator::TryToSetupDecodeOnSeparateSequence(
    const base::WeakPtr<Client>& decode_client,
    const scoped_refptr<base::SequencedTaskRunner>& decode_task_runner) {
  VLOGF(2);
  decode_client_ = decode_client;
  decode_task_runner_ = decode_task_runner;
  return true;
}

// static
VideoDecodeAccelerator::SupportedProfiles
V4L2VideoDecodeAccelerator::GetSupportedProfiles() {
  auto device = base::MakeRefCounted<V4L2Device>();
  return device->GetSupportedDecodeProfiles(kSupportedInputFourCCs);
}

void V4L2VideoDecodeAccelerator::DecodeTask(scoped_refptr<DecoderBuffer> buffer,
                                            int32_t bitstream_id) {
  DVLOGF(4) << "input_id=" << bitstream_id;
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(decoder_state_, kUninitialized);

  if (IsDestroyPending())
    return;

  if (IsVp9KSVCStream(input_format_fourcc_, *buffer)) {
    LOG(ERROR) << "VDA does not support decoding VP9 k-SVC stream";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  std::unique_ptr<BitstreamBufferRef> bitstream_record(new BitstreamBufferRef(
      decode_client_, decode_task_runner_, std::move(buffer), bitstream_id));

  // Skip empty buffer.
  if (!bitstream_record->buffer)
    return;

  if (decoder_state_ == kResetting || decoder_flushing_) {
    // In the case that we're resetting or flushing, we need to delay decoding
    // the BitstreamBuffers that come after the Reset() or Flush() call.  When
    // we're here, we know that this DecodeTask() was scheduled by a Decode()
    // call that came after (in the client thread) the Reset() or Flush() call;
    // thus set up the delay if necessary.
    if (decoder_delay_bitstream_buffer_id_ == -1)
      decoder_delay_bitstream_buffer_id_ = bitstream_record->input_id;
  } else if (decoder_state_ == kError) {
    VLOGF(2) << "early out: kError state";
    return;
  }

  decoder_input_queue_.push_back(std::move(bitstream_record));
  decoder_decode_buffer_tasks_scheduled_++;
  DecodeBufferTask();
}

void V4L2VideoDecodeAccelerator::DecodeBufferTask() {
  DVLOGF(4);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(decoder_state_, kUninitialized);
  TRACE_EVENT0("media,gpu", "V4L2VDA::DecodeBufferTask");

  if (IsDestroyPending())
    return;

  decoder_decode_buffer_tasks_scheduled_--;

  if (decoder_state_ != kInitialized && decoder_state_ != kDecoding) {
    DVLOGF(3) << "early out: state=" << decoder_state_;
    return;
  }

  if (decoder_current_bitstream_buffer_ == NULL) {
    if (decoder_input_queue_.empty()) {
      // We're waiting for a new buffer -- exit without scheduling a new task.
      return;
    }
    if (decoder_delay_bitstream_buffer_id_ ==
        decoder_input_queue_.front()->input_id) {
      // We're asked to delay decoding on this and subsequent buffers.
      return;
    }

    // Setup to use the next buffer.
    decoder_current_bitstream_buffer_ = std::move(decoder_input_queue_.front());
    decoder_input_queue_.pop_front();
    const auto& buffer = decoder_current_bitstream_buffer_->buffer;
    if (buffer) {
      DVLOGF(4) << "reading input_id="
                << decoder_current_bitstream_buffer_->input_id
                << ", addr=" << buffer->data() << ", size=" << buffer->size();
    } else {
      DCHECK_EQ(decoder_current_bitstream_buffer_->input_id, kFlushBufferId);
      DVLOGF(4) << "reading input_id=kFlushBufferId";
    }
  }
  bool schedule_task = false;
  size_t decoded_size = 0;
  const auto& buffer = decoder_current_bitstream_buffer_->buffer;
  if (!buffer) {
    // This is a dummy buffer, queued to flush the pipe.  Flush.
    DCHECK_EQ(decoder_current_bitstream_buffer_->input_id, kFlushBufferId);
    // Enqueue a buffer guaranteed to be empty.  To do that, we flush the
    // current input, enqueue no data to the next frame, then flush that down.
    schedule_task = true;
    if (current_input_buffer_ &&
        current_input_buffer_->GetTimeStamp().tv_sec != kFlushBufferId)
      schedule_task = FlushInputFrame();

    if (schedule_task && AppendToInputFrame(NULL, 0) && FlushInputFrame()) {
      VLOGF(2) << "enqueued flush buffer";
      schedule_task = true;
    } else {
      // If we failed to enqueue the empty buffer (due to pipeline
      // backpressure), don't advance the bitstream buffer queue, and don't
      // schedule the next task.  This bitstream buffer queue entry will get
      // reprocessed when the pipeline frees up.
      schedule_task = false;
    }
  } else if (buffer->empty()) {
    // This is a buffer queued from the client that has zero size.  Skip.
    // TODO(sandersd): This shouldn't be possible, empty buffers are never
    // enqueued.
    schedule_task = true;
  } else {
    // This is a buffer queued from the client, with actual contents.  Decode.
    const uint8_t* const data =
        buffer->data() + decoder_current_bitstream_buffer_->bytes_used;
    const size_t data_size =
        buffer->size() - decoder_current_bitstream_buffer_->bytes_used;

    if (!frame_splitter_->AdvanceFrameFragment(data, data_size,
                                               &decoded_size)) {
      LOG(ERROR) << "Invalid Stream";
      NOTIFY_ERROR(UNREADABLE_INPUT);
      return;
    }
    // AdvanceFrameFragment should not return a size larger than the buffer
    // size, even on invalid data.
    CHECK_LE(decoded_size, data_size);

    switch (decoder_state_) {
      case kInitialized:
        schedule_task = DecodeBufferInitial(data, decoded_size, &decoded_size);
        break;
      case kDecoding:
        schedule_task = DecodeBufferContinue(data, decoded_size);
        break;
      default:
        LOG(ERROR) << "Illegal State";
        NOTIFY_ERROR(ILLEGAL_STATE);
        return;
    }
  }
  if (decoder_state_ == kError) {
    // Failed during decode.
    return;
  }

  if (schedule_task) {
    decoder_current_bitstream_buffer_->bytes_used += decoded_size;
    if ((buffer ? buffer->size() : 0) ==
        decoder_current_bitstream_buffer_->bytes_used) {
      // Our current bitstream buffer is done; return it.
      int32_t input_id = decoder_current_bitstream_buffer_->input_id;
      DVLOGF(4) << "finished input_id=" << input_id;
      // BitstreamBufferRef destructor calls NotifyEndOfBitstreamBuffer().
      decoder_current_bitstream_buffer_.reset();
    }
    ScheduleDecodeBufferTaskIfNeeded();
  }
}

void V4L2VideoDecodeAccelerator::ScheduleDecodeBufferTaskIfNeeded() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  // If we're behind on tasks, schedule another one.
  int buffers_to_decode = decoder_input_queue_.size();
  if (decoder_current_bitstream_buffer_ != NULL)
    buffers_to_decode++;
  if (decoder_decode_buffer_tasks_scheduled_ < buffers_to_decode) {
    decoder_decode_buffer_tasks_scheduled_++;
    decoder_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&V4L2VideoDecodeAccelerator::DecodeBufferTask,
                                  base::Unretained(this)));
  }
}

bool V4L2VideoDecodeAccelerator::DecodeBufferInitial(const void* data,
                                                     size_t size,
                                                     size_t* endpos) {
  DVLOGF(3) << "data=" << data << ", size=" << size;
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(decoder_state_, kInitialized);
  // Initial decode.  We haven't been able to get output stream format info yet.
  // Get it, and start decoding.

  // Copy in and send to HW.
  if (!AppendToInputFrame(data, size))
    return false;

  // If we only have a partial frame, don't flush and process yet.
  if (frame_splitter_->IsPartialFramePending())
    return true;

  if (!FlushInputFrame())
    return false;

  // Recycle buffers.
  Dequeue();

  *endpos = size;

  // If an initial resolution change event is not done yet, a driver probably
  // needs more stream to decode format.
  // Return true and schedule next buffer without changing status to kDecoding.
  // If the initial resolution change is done and coded size is known, we may
  // still have to wait for AssignPictureBuffers() and output buffers to be
  // allocated.
  if (coded_size_.IsEmpty() || output_buffer_map_.empty()) {
    return true;
  }

  decoder_state_ = kDecoding;
  ScheduleDecodeBufferTaskIfNeeded();
  return true;
}

bool V4L2VideoDecodeAccelerator::DecodeBufferContinue(const void* data,
                                                      size_t size) {
  DVLOGF(4) << "data=" << data << ", size=" << size;
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(decoder_state_, kDecoding);

  // Both of these calls will set kError state if they fail.
  // Only flush the frame if it's complete.
  return (AppendToInputFrame(data, size) &&
          (frame_splitter_->IsPartialFramePending() || FlushInputFrame()));
}

bool V4L2VideoDecodeAccelerator::AppendToInputFrame(const void* data,
                                                    size_t size) {
  DVLOGF(4);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(decoder_state_, kUninitialized);
  DCHECK_NE(decoder_state_, kResetting);
  DCHECK_NE(decoder_state_, kError);
  // This routine can handle data == NULL and size == 0, which occurs when
  // we queue an empty buffer for the purposes of flushing the pipe.

  // Flush if we're too big
  if (current_input_buffer_) {
    size_t plane_size = current_input_buffer_->GetPlaneSize(0);
    size_t bytes_used = current_input_buffer_->GetPlaneBytesUsed(0);
    if (bytes_used + size > plane_size) {
      if (!FlushInputFrame())
        return false;
    }
  }

  // Try to get an available input buffer.
  if (!current_input_buffer_) {
    DCHECK(decoder_current_bitstream_buffer_ != NULL);
    DCHECK(input_queue_);

    // See if we can get more free buffers from HW.
    if (input_queue_->FreeBuffersCount() == 0)
      Dequeue();

    current_input_buffer_ = input_queue_->GetFreeBuffer();
    if (!current_input_buffer_) {
      // No buffer available yet.
      DVLOGF(4) << "stalled for input buffers";
      return false;
    }
    struct timeval timestamp = {
        .tv_sec = decoder_current_bitstream_buffer_->input_id};
    current_input_buffer_->SetTimeStamp(timestamp);
  }

  DCHECK(data != NULL || size == 0);
  if (size == 0) {
    // If we asked for an empty buffer, return now.  We return only after
    // getting the next input buffer, since we might actually want an empty
    // input buffer for flushing purposes.
    return true;
  }

  // Copy in to the buffer.
  size_t plane_size = current_input_buffer_->GetPlaneSize(0);
  size_t bytes_used = current_input_buffer_->GetPlaneBytesUsed(0);

  if (size > plane_size - bytes_used) {
    LOG(ERROR) << "over-size frame, erroring";
    NOTIFY_ERROR(UNREADABLE_INPUT);
    return false;
  }
  void* mapping = current_input_buffer_->GetPlaneMapping(0);
  memcpy(reinterpret_cast<uint8_t*>(mapping) + bytes_used, data, size);
  current_input_buffer_->SetPlaneBytesUsed(0, bytes_used + size);

  return true;
}

bool V4L2VideoDecodeAccelerator::FlushInputFrame() {
  DVLOGF(4);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(decoder_state_, kUninitialized);
  DCHECK_NE(decoder_state_, kResetting);
  DCHECK_NE(decoder_state_, kError);

  if (!current_input_buffer_)
    return true;

  const int32_t input_buffer_id = current_input_buffer_->GetTimeStamp().tv_sec;

  DCHECK(input_buffer_id != kFlushBufferId ||
         current_input_buffer_->GetPlaneBytesUsed(0) == 0);
  // * if input_id >= 0, this input buffer was prompted by a bitstream buffer we
  //   got from the client.  We can skip it if it is empty.
  // * if input_id < 0 (should be kFlushBufferId in this case), this input
  //   buffer was prompted by a flush buffer, and should be queued even when
  //   empty.
  if (input_buffer_id >= 0 &&
      current_input_buffer_->GetPlaneBytesUsed(0) == 0) {
    current_input_buffer_.reset();
    return true;
  }

  // Queue it.
  DVLOGF(4) << "submitting input_id=" << input_buffer_id;
  input_ready_queue_.push(std::move(*current_input_buffer_));
  current_input_buffer_.reset();
  // Enqueue once since there's new available input for it.
  Enqueue();

  TRACE_COUNTER_ID1("media,gpu", "V4L2VDA input ready buffers", this,
                    input_ready_queue_.size());

  return (decoder_state_ != kError);
}

void V4L2VideoDecodeAccelerator::ServiceDeviceTask(bool event_pending) {
  DVLOGF(4);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(decoder_state_, kUninitialized);
  TRACE_EVENT0("media,gpu", "V4L2VDA::ServiceDeviceTask");

  if (IsDestroyPending())
    return;

  DCHECK(input_queue_);
  DCHECK(output_queue_);

  if (decoder_state_ == kResetting) {
    DVLOGF(3) << "early out: kResetting state";
    return;
  } else if (decoder_state_ == kError) {
    DVLOGF(3) << "early out: kError state";
    return;
  } else if (decoder_state_ == kChangingResolution) {
    DVLOGF(3) << "early out: kChangingResolution state";
    return;
  }

  bool resolution_change_pending = false;
  if (event_pending)
    resolution_change_pending = DequeueResolutionChangeEvent();

  if (!resolution_change_pending && coded_size_.IsEmpty()) {
    // Some platforms do not send an initial resolution change event.
    // To work around this, we need to keep checking if the initial resolution
    // is known already by explicitly querying the format after each decode,
    // regardless of whether we received an event.
    // This needs to be done on initial resolution change,
    // i.e. when coded_size_.IsEmpty().

    // Try GetFormatInfo to check if an initial resolution change can be done.
    struct v4l2_format format;
    gfx::Size visible_size;
    bool again;
    if (GetFormatInfo(&format, &visible_size, &again) && !again) {
      resolution_change_pending = true;
      DequeueResolutionChangeEvent();
    }
  }

  Dequeue();
  Enqueue();

  // Clear the interrupt fd.
  if (!device_->ClearDevicePollInterrupt()) {
    LOG(ERROR) << "Failed Clear the interrupt fd";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  bool poll_device = false;
  // Add fd, if we should poll on it.
  // Can be polled as soon as either input or output buffers are queued.
  if (input_queue_->QueuedBuffersCount() + output_queue_->QueuedBuffersCount() >
      0)
    poll_device = true;

  // ServiceDeviceTask() should only ever be scheduled from DevicePollTask(),
  // so either:
  // * device_poll_thread_ is running normally
  // * device_poll_thread_ scheduled us, but then a ResetTask() or DestroyTask()
  //   shut it down, in which case we're either in kResetting or kError states
  //   respectively, and we should have early-outed already.
  DCHECK(device_poll_thread_.task_runner());
  // Queue the DevicePollTask() now.
  device_poll_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2VideoDecodeAccelerator::DevicePollTask,
                                base::Unretained(this), poll_device));

  DVLOGF(3) << "ServiceDeviceTask(): buffer counts: DEC["
            << decoder_input_queue_.size() << "->" << input_ready_queue_.size()
            << "] => DEVICE[" << input_queue_->FreeBuffersCount() << "+"
            << input_queue_->QueuedBuffersCount() << "/"
            << input_queue_->AllocatedBuffersCount() << "->"
            << output_queue_->FreeBuffersCount() << "+"
            << output_queue_->QueuedBuffersCount() << "/"
            << output_buffer_map_.size() << "] => PROCESSOR["
            << buffers_at_ip_.size() << "] => CLIENT["
            << buffers_at_client_.size() << "]";

  ScheduleDecodeBufferTaskIfNeeded();
  if (resolution_change_pending)
    StartResolutionChange();
}

void V4L2VideoDecodeAccelerator::Enqueue() {
  DVLOGF(4);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(decoder_state_, kUninitialized);

  if (IsDestroyPending()) {
    return;
  }

  // There's no reason why this class should attempt to enqueue buffers while
  // it's in the process of a resolution change.
  CHECK_NE(decoder_state_, kChangingResolution);

  DCHECK(input_queue_);
  DCHECK(output_queue_);

  // Drain the pipe of completed decode buffers.
  const int old_inputs_queued = input_queue_->QueuedBuffersCount();
  while (!input_ready_queue_.empty()) {
    bool flush_handled = false;
    int32_t input_id = input_ready_queue_.front().GetTimeStamp().tv_sec;
    if (input_id == kFlushBufferId) {
      // Send the flush command after all input buffers are dequeued. This makes
      // sure all previous resolution changes have been handled because the
      // driver must hold the input buffer that triggers resolution change. The
      // driver cannot decode data in it without new output buffers. If we send
      // the flush now and a queued input buffer triggers resolution change
      // later, the driver will send an output buffer that has
      // V4L2_BUF_FLAG_LAST. But some queued input buffer have not been decoded
      // yet. Also, V4L2VDA calls STREAMOFF and STREAMON after resolution
      // change. They implicitly send a V4L2_DEC_CMD_STOP and V4L2_DEC_CMD_START
      // to the decoder.
      if (input_queue_->QueuedBuffersCount() > 0)
        break;

      if (coded_size_.IsEmpty() || !input_queue_->IsStreaming()) {
        // In these situations, we should call NotifyFlushDone() immediately:
        // (1) If coded_size_.IsEmpty(), no output buffer could have been
        // allocated and there is nothing to flush.
        // (2) If input stream is off, we will never get the output buffer
        // with V4L2_BUF_FLAG_LAST.
        VLOGF(2) << "Nothing to flush. Notify flush done directly.";
        NotifyFlushDone();
        flush_handled = true;
      } else if (decoder_cmd_supported_) {
        if (!SendDecoderCmdStop())
          return;
        flush_handled = true;
      }
    }
    if (flush_handled) {
      // Recycle the buffer directly if we already handled the flush request.
      input_ready_queue_.pop();
    } else {
      // Enqueue an input buffer, or an empty flush buffer if decoder cmd
      // is not supported and there may be buffers to be flushed.
      auto buffer = std::move(input_ready_queue_.front());
      input_ready_queue_.pop();
      if (!EnqueueInputRecord(std::move(buffer)))
        return;
    }
  }

  if (old_inputs_queued == 0 && input_queue_->QueuedBuffersCount() != 0) {
    // We just started up a previously empty queue.
    // Queue state changed; signal interrupt.
    if (!device_->SetDevicePollInterrupt()) {
      PLOG(ERROR) << "SetDevicePollInterrupt failed";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }
    // Start VIDIOC_STREAMON if we haven't yet.
    if (!input_queue_->Streamon()) {
      LOG(ERROR) << "Failed Stream on input queue";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }
  }

  // OUTPUT queue must be started before CAPTURE queue as per codec API.
  if (!input_queue_->IsStreaming())
    return;

  // Enqueue all the outputs we can.
  const int old_outputs_queued = output_queue_->QueuedBuffersCount();
  while (auto buffer_opt = output_queue_->GetFreeBuffer()) {
    if (!EnqueueOutputRecord(std::move(*buffer_opt)))
      return;
  }
  if (old_outputs_queued == 0 && output_queue_->QueuedBuffersCount() != 0) {
    // We just started up a previously empty queue.
    // Queue state changed; signal interrupt.
    if (!device_->SetDevicePollInterrupt()) {
      PLOG(ERROR) << "SetDevicePollInterrupt(): failed";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }

    if (!output_queue_->Streamon()) {
      PLOG(ERROR) << "Failed Stream on output queue";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }
  }
}

bool V4L2VideoDecodeAccelerator::DequeueResolutionChangeEvent() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(decoder_state_, kUninitialized);
  DVLOGF(3);

  while (std::optional<struct v4l2_event> event = device_->DequeueEvent()) {
    if (event->type == V4L2_EVENT_SOURCE_CHANGE) {
      if (event->u.src_change.changes & V4L2_EVENT_SRC_CH_RESOLUTION) {
        VLOGF(2) << "got resolution change event.";
        return true;
      }
    } else {
      VLOGF(1) << "got an event (" << event->type
               << ") we haven't subscribed to.";
    }
  }
  return false;
}

void V4L2VideoDecodeAccelerator::Dequeue() {
  DVLOGF(4);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(decoder_state_, kUninitialized);
  DCHECK(input_queue_);
  DCHECK(output_queue_);

  while (input_queue_->QueuedBuffersCount() > 0) {
    if (!DequeueInputBuffer())
      break;
  }
  while (output_queue_->QueuedBuffersCount() > 0) {
    if (!DequeueOutputBuffer())
      break;
  }
  NotifyFlushDoneIfNeeded();
}

bool V4L2VideoDecodeAccelerator::DequeueInputBuffer() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(input_queue_);
  DCHECK_GT(input_queue_->QueuedBuffersCount(), 0u);

  // Dequeue a completed input (VIDEO_OUTPUT) buffer, and recycle to the free
  // list.
  auto ret = input_queue_->DequeueBuffer();

  if (ret.first == false) {
    LOG(ERROR) << "Error in Dequeue input buffer";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  } else if (!ret.second) {
    // we're just out of buffers to dequeue.
    return false;
  }

  return true;
}

bool V4L2VideoDecodeAccelerator::DequeueOutputBuffer() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(output_queue_);
  DCHECK_GT(output_queue_->QueuedBuffersCount(), 0u);
  DCHECK(output_queue_->IsStreaming());

  // Dequeue a completed output (VIDEO_CAPTURE) buffer, and queue to the
  // completed queue.
  auto ret = output_queue_->DequeueBuffer();
  if (ret.first == false) {
    LOG(ERROR) << "Error in Dequeue output buffer";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }
  if (!ret.second) {
    return false;
  }

  V4L2ReadableBufferRef buf(std::move(ret.second));

  DCHECK_LT(buf->BufferId(), output_buffer_map_.size());
  OutputRecord& output_record = output_buffer_map_[buf->BufferId()];
  DCHECK_NE(output_record.picture_id, -1);
  // Zero-bytes buffers are returned as part of a flush and can be dismissed.
  if (buf->GetPlaneBytesUsed(0) > 0) {
    int32_t bitstream_buffer_id = buf->GetTimeStamp().tv_sec;
    DCHECK_GE(bitstream_buffer_id, 0);
    DVLOGF(4) << "Dequeue output buffer: dqbuf index=" << buf->BufferId()
              << " bitstream input_id=" << bitstream_buffer_id;
    if (image_processor_device_
#ifdef SUPPORT_MT21_PIXEL_FORMAT_SOFTWARE_DECOMPRESSION
        || mt21_decompressor_
#endif
    ) {
      if (!ProcessFrame(bitstream_buffer_id, buf)) {
        LOG(ERROR) << "Processing frame failed";
        NOTIFY_ERROR(PLATFORM_FAILURE);
        return false;
      }
    } else {
      SendBufferToClient(buf->BufferId(), bitstream_buffer_id, buf);
    }
  }
  if (buf->IsLast()) {
    DVLOGF(3) << "Got last output buffer. Waiting last buffer="
              << flush_awaiting_last_output_buffer_;
    if (flush_awaiting_last_output_buffer_) {
      flush_awaiting_last_output_buffer_ = false;
      struct v4l2_decoder_cmd cmd;
      memset(&cmd, 0, sizeof(cmd));
      cmd.cmd = V4L2_DEC_CMD_START;
      IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_DECODER_CMD, &cmd);
    }
  }

  return true;
}

bool V4L2VideoDecodeAccelerator::EnqueueInputRecord(
    V4L2WritableBufferRef buffer) {
  DVLOGF(4);

  // Enqueue an input (VIDEO_OUTPUT) buffer.
  int32_t input_id = buffer.GetTimeStamp().tv_sec;
  size_t bytes_used = buffer.GetPlaneBytesUsed(0);
  if (!std::move(buffer).QueueMMap()) {
    LOG(ERROR) << "Error in Queue input buffer";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }
  DVLOGF(4) << "enqueued input_id=" << input_id << " size=" << bytes_used;
  return true;
}

bool V4L2VideoDecodeAccelerator::EnqueueOutputRecord(
    V4L2WritableBufferRef buffer) {
  OutputRecord& output_record = output_buffer_map_[buffer.BufferId()];
  DCHECK_NE(output_record.picture_id, -1);

  bool ret = false;
  switch (buffer.Memory()) {
    case V4L2_MEMORY_MMAP:
      ret = std::move(buffer).QueueMMap();
      break;
    case V4L2_MEMORY_DMABUF:
      ret = std::move(buffer).QueueDMABuf(output_record.output_frame);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  if (!ret) {
    LOG(ERROR) << "Error in Dequeue output buffer";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  return true;
}

void V4L2VideoDecodeAccelerator::ReusePictureBufferTask(
    int32_t picture_buffer_id) {
  DVLOGF(4) << "picture_buffer_id=" << picture_buffer_id;
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  if (IsDestroyPending())
    return;

  // We run ReusePictureBufferTask even if we're in kResetting.
  if (decoder_state_ == kError) {
    DVLOGF(4) << "early out: kError state";
    return;
  }

  if (decoder_state_ == kChangingResolution) {
    DVLOGF(4) << "early out: kChangingResolution";
    return;
  }

  auto iter = buffers_at_client_.find(picture_buffer_id);
  if (iter == buffers_at_client_.end()) {
    // It's possible that we've already posted a DismissPictureBuffer for this
    // picture, but it has not yet executed when this ReusePictureBuffer was
    // posted to us by the client. In that case just ignore this (we've already
    // dismissed it and accounted for that).
    DVLOGF(3) << "got picture id= " << picture_buffer_id
              << " not in use (anymore?).";
    return;
  }

  buffers_at_client_.erase(iter);

  // We got a buffer back, so enqueue it back.
  Enqueue();

  TRACE_COUNTER_ID2(
      "media,gpu", "V4L2 output buffers", this, "in client",
      buffers_at_client_.size(), "in vda",
      output_buffer_map_.size() - buffers_at_client_.size());
  TRACE_COUNTER_ID2(
      "media,gpu", "V4L2 output buffers in vda", this, "free",
      output_queue_->FreeBuffersCount(), "in device or IP",
      output_queue_->QueuedBuffersCount() + buffers_at_ip_.size());
}

void V4L2VideoDecodeAccelerator::FlushTask() {
  VLOGF(2);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  if (IsDestroyPending())
    return;

  if (decoder_state_ == kError) {
    VLOGF(2) << "early out: kError state";
    return;
  }

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("media,gpu", "V4L2VDA::FlushTask",
                                    TRACE_ID_LOCAL(this));

  // We don't support stacked flushing.
  DCHECK(!decoder_flushing_);

  // Queue up an empty buffer -- this triggers the flush.
  decoder_input_queue_.push_back(std::make_unique<BitstreamBufferRef>(
      decode_client_, decode_task_runner_, nullptr, kFlushBufferId));
  decoder_flushing_ = true;
  SendPictureReady();  // Send all pending PictureReady.

  ScheduleDecodeBufferTaskIfNeeded();
}

void V4L2VideoDecodeAccelerator::NotifyFlushDoneIfNeeded() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(input_queue_);
  if (!decoder_flushing_)
    return;

  // Pipeline is empty when:
  // * Decoder input queue is empty of non-delayed buffers.
  // * There is no currently filling input buffer.
  // * Input holding queue is empty.
  // * All input (VIDEO_OUTPUT) buffers are returned.
  // * All image processor buffers are returned.
  if (!decoder_input_queue_.empty()) {
    if (decoder_input_queue_.front()->input_id !=
        decoder_delay_bitstream_buffer_id_) {
      DVLOGF(3) << "Some input bitstream buffers are not queued.";
      return;
    }
  }
  if (current_input_buffer_) {
    DVLOGF(3) << "Current input buffer != -1";
    return;
  }
  if ((input_ready_queue_.size() + input_queue_->QueuedBuffersCount()) != 0) {
    DVLOGF(3) << "Some input buffers are not dequeued.";
    return;
  }
  if (!buffers_at_ip_.empty()) {
    DVLOGF(3) << "Waiting for image processor to complete.";
    return;
  }
  if (flush_awaiting_last_output_buffer_) {
    DVLOGF(3) << "Waiting for last output buffer.";
    return;
  }

  // TODO(posciak): https://crbug.com/270039. Exynos requires a
  // streamoff-streamon sequence after flush to continue, even if we are not
  // resetting. This would make sense, because we don't really want to resume
  // from a non-resume point (e.g. not from an IDR) if we are flushed.
  // MSE player however triggers a Flush() on chunk end, but never Reset(). One
  // could argue either way, or even say that Flush() is not needed/harmful when
  // transitioning to next chunk.
  // For now, do the streamoff-streamon cycle to satisfy Exynos and not freeze
  // when doing MSE. This should be harmless otherwise.
  if (!(StopDevicePoll() && StopOutputStream() && StopInputStream()))
    return;

  if (!StartDevicePoll())
    return;

  NotifyFlushDone();
  // While we were flushing, we early-outed DecodeBufferTask()s.
  ScheduleDecodeBufferTaskIfNeeded();
}

void V4L2VideoDecodeAccelerator::NotifyFlushDone() {
  TRACE_EVENT_NESTABLE_ASYNC_END0("media,gpu", "V4L2VDA::FlushTask",
                                  TRACE_ID_LOCAL(this));
  decoder_delay_bitstream_buffer_id_ = -1;
  decoder_flushing_ = false;
  VLOGF(2) << "returning flush";
  child_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::NotifyFlushDone, client_));
}

bool V4L2VideoDecodeAccelerator::IsDecoderCmdSupported() {
  // CMD_STOP should always succeed. If the decoder is started, the command can
  // flush it. If the decoder is stopped, the command does nothing. We use this
  // to know if a driver supports V4L2_DEC_CMD_STOP to flush.
  struct v4l2_decoder_cmd cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.cmd = V4L2_DEC_CMD_STOP;
  if (device_->Ioctl(VIDIOC_TRY_DECODER_CMD, &cmd) != 0) {
    VLOGF(2) "V4L2_DEC_CMD_STOP is not supported.";
    return false;
  }

  return true;
}

bool V4L2VideoDecodeAccelerator::SendDecoderCmdStop() {
  VLOGF(2);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(!flush_awaiting_last_output_buffer_);

  struct v4l2_decoder_cmd cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.cmd = V4L2_DEC_CMD_STOP;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_DECODER_CMD, &cmd);
  flush_awaiting_last_output_buffer_ = true;

  return true;
}

void V4L2VideoDecodeAccelerator::ResetTask() {
  VLOGF(2);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  if (IsDestroyPending())
    return;

  if (decoder_state_ == kError) {
    VLOGF(2) << "early out: kError state";
    return;
  }

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("media,gpu", "V4L2VDA::ResetTask",
                                    TRACE_ID_LOCAL(this));

  decoder_current_bitstream_buffer_.reset();
  while (!decoder_input_queue_.empty())
    decoder_input_queue_.pop_front();

  current_input_buffer_.reset();

  // If we are in the middle of switching resolutions or awaiting picture
  // buffers, postpone reset until it's done. We don't have to worry about
  // timing of this wrt to decoding, because output pipe is already
  // stopped if we are changing resolution. We will come back here after
  // we are done.
  DCHECK(!reset_pending_);
  if (decoder_state_ == kChangingResolution ||
      decoder_state_ == kAwaitingPictureBuffers) {
    reset_pending_ = true;
    return;
  }
  FinishReset();
}

void V4L2VideoDecodeAccelerator::FinishReset() {
  VLOGF(2);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  reset_pending_ = false;
  // After the output stream is stopped, the codec should not post any
  // resolution change events. So we dequeue the resolution change event
  // afterwards. The event could be posted before or while stopping the output
  // stream. The codec will expect the buffer of new size after the seek, so
  // we need to handle the resolution change event first.
  if (!(StopDevicePoll() && StopOutputStream()))
    return;

  if (DequeueResolutionChangeEvent()) {
    reset_pending_ = true;
    StartResolutionChange();
    return;
  }

  if (!StopInputStream())
    return;

  // Drop all buffers in image processor.
  if (image_processor_ && !ResetImageProcessor()) {
    LOG(ERROR) << "Fail to reset image processor";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  // If we were flushing, we'll never return any more BitstreamBuffers or
  // PictureBuffers; they have all been dropped and returned by now.
  NotifyFlushDoneIfNeeded();

  // Mark that we're resetting, then enqueue a ResetDoneTask().  All intervening
  // jobs will early-out in the kResetting state.
  decoder_state_ = kResetting;
  SendPictureReady();  // Send all pending PictureReady.
  decoder_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2VideoDecodeAccelerator::ResetDoneTask,
                                base::Unretained(this)));
}

void V4L2VideoDecodeAccelerator::ResetDoneTask() {
  VLOGF(2);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  if (IsDestroyPending())
    return;

  if (decoder_state_ == kError) {
    VLOGF(2) << "early out: kError state";
    return;
  }

  TRACE_EVENT_NESTABLE_ASYNC_END0("media,gpu", "V4L2VDA::ResetTask",
                                  TRACE_ID_LOCAL(this));

  // Start poll thread if NotifyFlushDoneIfNeeded has not already.
  if (!device_poll_thread_.IsRunning()) {
    if (!StartDevicePoll())
      return;
  }

  frame_splitter_->Reset();

  // Jobs drained, we're finished resetting.
  DCHECK_EQ(decoder_state_, kResetting);
  decoder_state_ = kInitialized;

  decoder_delay_bitstream_buffer_id_ = -1;
  child_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::NotifyResetDone, client_));

  // While we were resetting, we early-outed DecodeBufferTask()s.
  ScheduleDecodeBufferTaskIfNeeded();
}

void V4L2VideoDecodeAccelerator::DestroyTask() {
  VLOGF(2);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  TRACE_EVENT0("media,gpu", "V4L2VDA::DestroyTask");

  // DestroyTask() should run regardless of decoder_state_.

  decoder_state_ = kDestroying;

  StopDevicePoll();
  StopOutputStream();
  StopInputStream();

  decoder_current_bitstream_buffer_.reset();
  current_input_buffer_.reset();
  decoder_decode_buffer_tasks_scheduled_ = 0;
  while (!decoder_input_queue_.empty())
    decoder_input_queue_.pop_front();
  decoder_flushing_ = false;

  // First liberate all the frames held by the client.
  buffers_at_client_.clear();

  // The image processor's thread was the user of the image processor device,
  // so let it keep the last reference and destroy it in its own thread.
  image_processor_device_ = nullptr;
  image_processor_ = nullptr;
  while (!buffers_at_ip_.empty())
    buffers_at_ip_.pop();

#ifdef SUPPORT_MT21_PIXEL_FORMAT_SOFTWARE_DECOMPRESSION
  mt21_decompressor_ = nullptr;
#endif

  DestroyInputBuffers();
  DestroyOutputBuffers();

  input_queue_ = nullptr;
  output_queue_ = nullptr;

  frame_splitter_ = nullptr;

  // Clear the V4L2 devices in the decoder thread so the V4L2Device's
  // destructor is called from the thread that used it.
  device_ = nullptr;

  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

bool V4L2VideoDecodeAccelerator::StartDevicePoll() {
  DVLOGF(3);
  DCHECK(!device_poll_thread_.IsRunning());
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  // Start up the device poll thread and schedule its first DevicePollTask().
  if (!device_poll_thread_.Start()) {
    LOG(ERROR) << "Device thread failed to start";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }
  cancelable_service_device_task_.Reset(base::BindRepeating(
      &V4L2VideoDecodeAccelerator::ServiceDeviceTask, base::Unretained(this)));
  cancelable_service_device_task_callback_ =
      cancelable_service_device_task_.callback();
  device_poll_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2VideoDecodeAccelerator::DevicePollTask,
                                base::Unretained(this), 0));

  return true;
}

bool V4L2VideoDecodeAccelerator::StopDevicePoll() {
  DVLOGF(3);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  if (!device_poll_thread_.IsRunning())
    return true;

  // Signal the DevicePollTask() to stop, and stop the device poll thread.
  if (!device_->SetDevicePollInterrupt()) {
    PLOG(ERROR) << "SetDevicePollInterrupt(): failed";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }
  device_poll_thread_.Stop();
  // Must be done after the Stop() above to ensure
  // |cancelable_service_device_task_callback_| is not copied.
  cancelable_service_device_task_.Cancel();
  cancelable_service_device_task_callback_ = base::NullCallback();
  // Clear the interrupt now, to be sure.
  if (!device_->ClearDevicePollInterrupt()) {
    PLOG(ERROR) << "ClearDevicePollInterrupt: failed";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }
  DVLOGF(3) << "device poll stopped";
  return true;
}

bool V4L2VideoDecodeAccelerator::StopOutputStream() {
  VLOGF(2);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  if (!output_queue_ || !output_queue_->IsStreaming())
    return true;

  if (!output_queue_->Streamoff()) {
    VLOGF(1) << "Failed streaming off output queue";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  // Output stream is stopped. No need to wait for the buffer anymore.
  flush_awaiting_last_output_buffer_ = false;

  return true;
}

bool V4L2VideoDecodeAccelerator::StopInputStream() {
  VLOGF(2);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  if (!input_queue_ || !input_queue_->IsStreaming())
    return true;

  if (!input_queue_->Streamoff()) {
    LOG(ERROR) << "Failed streaming off input queue";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  // Reset accounting info for input.
  while (!input_ready_queue_.empty())
    input_ready_queue_.pop();

  return true;
}

void V4L2VideoDecodeAccelerator::StartResolutionChange() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(decoder_state_, kUninitialized);
  DCHECK_NE(decoder_state_, kResetting);

  VLOGF(2) << "Initiate resolution change";

  if (!(StopDevicePoll() && StopOutputStream()))
    return;

  decoder_state_ = kChangingResolution;
  SendPictureReady();  // Send all pending PictureReady.

  if (!buffers_at_ip_.empty()) {
    VLOGF(2) << "Wait image processor to finish before destroying buffers.";
    return;
  }

  buffers_at_client_.clear();

  image_processor_ = nullptr;

#ifdef SUPPORT_MT21_PIXEL_FORMAT_SOFTWARE_DECOMPRESSION
  mt21_decompressor_ = nullptr;
#endif

  if (!DestroyOutputBuffers()) {
    LOG(ERROR) << "Failed destroying output buffers.";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  FinishResolutionChange();
}

void V4L2VideoDecodeAccelerator::FinishResolutionChange() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(decoder_state_, kChangingResolution);
  VLOGF(2);

  if (decoder_state_ == kError) {
    VLOGF(2) << "early out: kError state";
    return;
  }

  struct v4l2_format format;
  bool again;
  gfx::Size visible_size;
  bool ret = GetFormatInfo(&format, &visible_size, &again);
  if (!ret || again) {
    LOG(ERROR) << "Couldn't get format information after resolution change";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  if (!CreateBuffersForFormat(format, visible_size)) {
    LOG(ERROR) << "Couldn't reallocate buffers after resolution change";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  if (!StartDevicePoll())
    return;
}

void V4L2VideoDecodeAccelerator::DevicePollTask(bool poll_device) {
  DVLOGF(4);
  DCHECK(device_poll_thread_.task_runner()->BelongsToCurrentThread());
  TRACE_EVENT0("media,gpu", "V4L2VDA::DevicePollTask");

  bool event_pending = false;

  if (!device_->Poll(poll_device, &event_pending)) {
    LOG(ERROR) << "Failed during poll";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  // All processing should happen on ServiceDeviceTask(), since we shouldn't
  // touch decoder state from this thread.
  decoder_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(cancelable_service_device_task_callback_, event_pending));
}

bool V4L2VideoDecodeAccelerator::IsDestroyPending() {
  return destroy_pending_.IsSignaled();
}

void V4L2VideoDecodeAccelerator::NotifyError(Error error) {
  VLOGF(1);

  // Notifying the client should only happen from the client's thread.
  if (!child_task_runner_->BelongsToCurrentThread()) {
    child_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&V4L2VideoDecodeAccelerator::NotifyError,
                                  weak_this_, error));
    return;
  }

  // Notify the decoder's client an error has occurred.
  if (client_) {
    client_->NotifyError(error);
    client_ptr_factory_.reset();
  }
}

void V4L2VideoDecodeAccelerator::SetErrorState(Error error) {
  // We can touch decoder_state_ only if this is the decoder thread or the
  // decoder thread isn't running.
  if (decoder_thread_.task_runner() &&
      !decoder_thread_.task_runner()->BelongsToCurrentThread()) {
    decoder_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&V4L2VideoDecodeAccelerator::SetErrorState,
                                  base::Unretained(this), error));
    return;
  }

  // Notifying the client of an error will only happen if we are already
  // initialized, as the API does not allow doing so before that. Subsequent
  // errors and errors while destroying will be suppressed.
  if (decoder_state_ != kError && decoder_state_ != kUninitialized &&
      decoder_state_ != kDestroying)
    NotifyError(error);

  decoder_state_ = kError;
}

bool V4L2VideoDecodeAccelerator::GetFormatInfo(struct v4l2_format* format,
                                               gfx::Size* visible_size,
                                               bool* again) {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  *again = false;

  auto ret = output_queue_->GetFormat();
  switch (ret.second) {
    case 0:
      *format = *ret.first;
      break;
    case EINVAL:
      // EINVAL means we haven't seen sufficient stream to decode the format.
      *again = true;
      return true;
    default:
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return false;
  }

  // Make sure we are still getting the format we set on initialization.
  if (format->fmt.pix_mp.pixelformat != output_format_fourcc_->ToV4L2PixFmt()) {
    VLOGF(1) << "Unexpected format from G_FMT on output";
    return false;
  }

  gfx::Size coded_size(format->fmt.pix_mp.width, format->fmt.pix_mp.height);
  if (visible_size != nullptr)
    *visible_size = GetVisibleSize(coded_size);

  return true;
}

bool V4L2VideoDecodeAccelerator::CreateBuffersForFormat(
    const struct v4l2_format& format,
    const gfx::Size& visible_size) {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  size_t egl_image_planes_count;

  coded_size_.SetSize(format.fmt.pix_mp.width, format.fmt.pix_mp.height);
  visible_size_ = visible_size;
  egl_image_size_ = coded_size_;
  if (image_processor_device_) {
    egl_image_planes_count = 0;
    auto output_size = coded_size_;
    if (!V4L2ImageProcessorBackend::TryOutputFormat(
            output_format_fourcc_->ToV4L2PixFmt(),
            egl_image_format_fourcc_->ToV4L2PixFmt(), coded_size_, &output_size,
            &egl_image_planes_count)) {
      VLOGF(1) << "Fail to get output size and plane count of processor";
      return false;
    }
    // This is very restrictive because it assumes the IP has the same alignment
    // criteria as the video decoder that will produce the input video frames.
    // In practice, this applies to all Image Processors, i.e. Mediatek devices.
    DCHECK_EQ(coded_size_, output_size);
  } else {
    egl_image_planes_count = format.fmt.pix_mp.num_planes;
  }
  VLOGF(2) << "new resolution: " << coded_size_.ToString()
           << ", visible size: " << visible_size_.ToString()
           << ", decoder output planes count: " << format.fmt.pix_mp.num_planes
           << ", EGLImage size: " << egl_image_size_.ToString()
           << ", EGLImage plane count: " << egl_image_planes_count;

  return CreateOutputBuffers();
}

gfx::Size V4L2VideoDecodeAccelerator::GetVisibleSize(
    const gfx::Size& coded_size) {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  auto ret = output_queue_->GetVisibleRect();
  if (!ret) {
    return coded_size;
  }
  gfx::Rect rect = std::move(*ret);
  DVLOGF(3) << "visible rectangle is " << rect.ToString();
  if (!gfx::Rect(coded_size).Contains(rect)) {
    DVLOGF(3) << "visible rectangle " << rect.ToString()
              << " is not inside coded size " << coded_size.ToString();
    return coded_size;
  }
  if (rect.IsEmpty()) {
    VLOGF(1) << "visible size is empty";
    return coded_size;
  }

  // Chrome assume picture frame is coded at (0, 0).
  if (!rect.origin().IsOrigin()) {
    VLOGF(1) << "Unexpected visible rectangle " << rect.ToString()
             << ", top-left is not origin";
    return coded_size;
  }

  return rect.size();
}

bool V4L2VideoDecodeAccelerator::CreateInputBuffers() {
  VLOGF(2);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  // We always run this as we prepare to initialize.
  DCHECK_EQ(decoder_state_, kInitialized);
  DCHECK(input_queue_);

  if (input_queue_->AllocateBuffers(kInputBufferCount, V4L2_MEMORY_MMAP,
                                    /*incoherent=*/false) == 0) {
    LOG(ERROR) << "Failed allocating input buffers";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  return true;
}

bool V4L2VideoDecodeAccelerator::SetupFormats() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(decoder_state_, kInitialized);
  DCHECK(!input_queue_->IsStreaming());
  DCHECK(!output_queue_->IsStreaming());

  size_t input_size;
  gfx::Size max_resolution, min_resolution;
  GetSupportedResolution(base::BindRepeating(&V4L2Device::Ioctl, device_),
                         input_format_fourcc_, &min_resolution,
                         &max_resolution);
  if (max_resolution.width() > 1920 && max_resolution.height() > 1088)
    input_size = kInputBufferMaxSizeFor4k;
  else
    input_size = kInputBufferMaxSizeFor1080p;

  struct v4l2_fmtdesc fmtdesc;
  memset(&fmtdesc, 0, sizeof(fmtdesc));
  fmtdesc.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  bool is_format_supported = false;
  while (device_->Ioctl(VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
    if (fmtdesc.pixelformat == input_format_fourcc_) {
      is_format_supported = true;
      break;
    }
    ++fmtdesc.index;
  }

  if (!is_format_supported) {
    VLOGF(1) << "Input fourcc " << input_format_fourcc_
             << " not supported by device.";
    return false;
  }

  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  format.fmt.pix_mp.pixelformat = input_format_fourcc_;
  format.fmt.pix_mp.plane_fmt[0].sizeimage = input_size;
  format.fmt.pix_mp.num_planes = 1;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_FMT, &format);
  DCHECK_EQ(format.fmt.pix_mp.pixelformat, input_format_fourcc_);

  // We have to set up the format for output, because the driver may not allow
  // changing it once we start streaming; whether it can support our chosen
  // output format or not may depend on the input format.
  memset(&fmtdesc, 0, sizeof(fmtdesc));
  fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  while (device_->Ioctl(VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
    auto fourcc = Fourcc::FromV4L2PixFmt(fmtdesc.pixelformat);
    if (fourcc && device_->CanCreateEGLImageFrom(*fourcc)) {
      output_format_fourcc_ = *fourcc;
      break;
    }
    ++fmtdesc.index;
  }

  DCHECK(!image_processor_device_);
  if (!output_format_fourcc_) {
    VLOGF(2) << "Could not find a usable output format. Try image processor";
#ifdef SUPPORT_MT21_PIXEL_FORMAT_SOFTWARE_DECOMPRESSION
    if (base::FeatureList::IsEnabled(media::kPreferSoftwareMT21)) {
      output_format_fourcc_ = Fourcc(Fourcc::MT21);
      egl_image_format_fourcc_ = Fourcc(Fourcc::NV12);
    } else {
#else
    {
#endif
      if (!V4L2ImageProcessorBackend::IsSupported()) {
        VLOGF(1) << "Image processor not available";
        return false;
      }
      output_format_fourcc_ =
          v4l2_vda_helpers::FindImageProcessorInputFormat(device_.get());
      if (!output_format_fourcc_) {
        VLOGF(1) << "Can't find a usable input format from image processor";
        return false;
      }
      egl_image_format_fourcc_ =
          v4l2_vda_helpers::FindImageProcessorOutputFormat(device_.get());
      if (!egl_image_format_fourcc_) {
        VLOGF(1) << "Can't find a usable output format from image processor";
        return false;
      }
      image_processor_device_ = base::MakeRefCounted<V4L2Device>();
    }
  } else {
    egl_image_format_fourcc_ = output_format_fourcc_;
  }
  VLOGF(2) << "Output format=" << output_format_fourcc_->ToString();

  // Just set the fourcc for output; resolution, etc., will come from the
  // driver once it extracts it from the stream.
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  format.fmt.pix_mp.pixelformat = output_format_fourcc_->ToV4L2PixFmt();
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_FMT, &format);
  DCHECK_EQ(format.fmt.pix_mp.pixelformat,
            output_format_fourcc_->ToV4L2PixFmt());

  return true;
}

bool V4L2VideoDecodeAccelerator::ResetImageProcessor() {
  VLOGF(2);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  if (!image_processor_->Reset())
    return false;

  while (!buffers_at_ip_.empty())
    buffers_at_ip_.pop();

  return true;
}

bool V4L2VideoDecodeAccelerator::CreateImageProcessor() {
  VLOGF(2);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(!image_processor_);
  const ImageProcessor::OutputMode image_processor_output_mode =
      (output_mode_ == Config::OutputMode::kAllocate
           ? ImageProcessor::OutputMode::ALLOCATE
           : ImageProcessor::OutputMode::IMPORT);

  // Start with a brand new image processor device, since the old one was
  // already opened and attempting to open it again is not supported.
  image_processor_device_ = base::MakeRefCounted<V4L2Device>();

  image_processor_ = v4l2_vda_helpers::CreateImageProcessor(
      *output_format_fourcc_, *egl_image_format_fourcc_, coded_size_,
      coded_size_, gfx::Rect(visible_size_),
      VideoFrame::StorageType::STORAGE_DMABUFS, output_buffer_map_.size(),
      image_processor_device_, image_processor_output_mode,
      decoder_thread_.task_runner(),
      // Unretained(this) is safe for ErrorCB because |decoder_thread_| is owned
      // by this V4L2VideoDecodeAccelerator and |this| must be valid when
      // ErrorCB is executed.
      base::BindRepeating(&V4L2VideoDecodeAccelerator::ImageProcessorError,
                          base::Unretained(this)));

  if (!image_processor_) {
    VLOGF(1) << "Error creating image processor";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  VLOGF(2) << "ImageProcessor is created: " << image_processor_->backend_type();
  return true;
}

bool V4L2VideoDecodeAccelerator::ProcessFrame(int32_t bitstream_buffer_id,
                                              V4L2ReadableBufferRef buf) {
  DVLOGF(4);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  OutputRecord& output_record = output_buffer_map_[buf->BufferId()];

  // Keep reference to the IP input until the frame is processed
  buffers_at_ip_.push(std::make_pair(bitstream_buffer_id, buf));

#ifdef SUPPORT_MT21_PIXEL_FORMAT_SOFTWARE_DECOMPRESSION
  if (base::FeatureList::IsEnabled(media::kPreferSoftwareMT21)) {
    if (!mt21_decompressor_) {
      LOG(ERROR) << "PreferSoftwareMT21 enabled, but MT21 decompressor was not "
                    "created!";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return false;
    }

    if (output_mode_ != Config::OutputMode::kImport) {
      LOG(ERROR) << "Software MT21 does not support ALLOCATE output mode!";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return false;
    }

    if (buf->PlanesCount() != 2) {
      LOG(ERROR) << "Wrong number of planes for MT21!";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return false;
    }

    std::unique_ptr<VideoFrameMapper> output_frame_mapper;
    output_frame_mapper = VideoFrameMapperFactory::CreateMapper(
        PIXEL_FORMAT_NV12, VideoFrame::STORAGE_DMABUFS,
        /*force_linear_buffer_mapper=*/true);
    if (!output_frame_mapper) {
      output_frame_mapper = VideoFrameMapperFactory::CreateMapper(
          PIXEL_FORMAT_NV12, VideoFrame::STORAGE_GPU_MEMORY_BUFFER,
          /*force_linear_buffer_mapper=*/true);
    }
    if (!output_frame_mapper) {
      LOG(ERROR) << "Failed to instantiate MT21 frame mapper!";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return false;
    }

    scoped_refptr<FrameResource> mapped_output_frame =
        VideoFrameResource::Create(output_frame_mapper->MapFrame(
            output_record.output_frame, PROT_READ | PROT_WRITE));
    if (!mapped_output_frame) {
      LOG(ERROR) << "Failed to map MT21 frame!";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return false;
    }

    {
      TRACE_EVENT0("media,gpu", "V4L2VDA::MT21ToNV12");
      mt21_decompressor_->MT21ToNV12(
          static_cast<const uint8_t*>(buf->GetPlaneMapping(0)),
          static_cast<const uint8_t*>(buf->GetPlaneMapping(1)),
          buf->GetPlaneBytesUsed(0), buf->GetPlaneBytesUsed(1),
          mapped_output_frame->GetWritableVisibleData(VideoFrame::Plane::kY),
          mapped_output_frame->GetWritableVisibleData(VideoFrame::Plane::kUV));
    }

    FrameProcessed(bitstream_buffer_id, buf->BufferId(), mapped_output_frame);

    return true;
  }
#endif

  scoped_refptr<FrameResource> input_frame = buf->GetFrameResource();
  if (!input_frame) {
    VLOGF(1) << "Could not get the input frame for the image processor!";
    return false;
  }

  // The |input_frame| has a potentially incorrect visible rectangle and natural
  // size: that frame gets created by V4L2Buffer::CreateVideoFrame() which uses
  // v4l2_format::fmt.pix_mp.width and v4l2_format::fmt.pix_mp.height as the
  // visible rectangle and natural size. However, those dimensions actually
  // correspond to the coded size. Therefore, we should wrap |input_frame| into
  // another frame with the right visible rectangle and natural size.
  DCHECK(input_frame->visible_rect().origin().IsOrigin());
  const gfx::Rect visible_rect = image_processor_->input_config().visible_rect;
  const gfx::Size natural_size = visible_rect.size();
  if (!gfx::Rect(input_frame->coded_size()).Contains(visible_rect) ||
      !input_frame->visible_rect().Contains(visible_rect)) {
    VLOGF(1) << "The visible size is too large!";
    return false;
  }
  if (!gfx::Rect(input_frame->natural_size())
           .Contains(gfx::Rect(natural_size))) {
    VLOGF(1) << "The natural size is too large!";
    return false;
  }
  scoped_refptr<FrameResource> cropped_input_frame =
      input_frame->CreateWrappingFrame(visible_rect, natural_size);
  if (!cropped_input_frame) {
    VLOGF(1) << "Could not wrap the input frame for the image processor!";
    return false;
  }

  // Unretained(this) is safe for FrameReadyCB because |decoder_thread_| is
  // owned by this V4L2VideoDecodeAccelerator and |this| must be valid when
  // FrameReadyCB is executed.
  if (image_processor_->output_mode() == ImageProcessor::OutputMode::IMPORT) {
    image_processor_->Process(
        std::move(cropped_input_frame), output_record.output_frame,
        base::BindOnce(&V4L2VideoDecodeAccelerator::FrameProcessed,
                       base::Unretained(this), bitstream_buffer_id,
                       buf->BufferId()));
  } else {
    image_processor_->Process(
        std::move(cropped_input_frame),
        base::BindOnce(&V4L2VideoDecodeAccelerator::FrameProcessed,
                       base::Unretained(this), bitstream_buffer_id));
  }
  return true;
}

bool V4L2VideoDecodeAccelerator::CreateOutputBuffers() {
  VLOGF(2);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(decoder_state_ == kInitialized ||
         decoder_state_ == kChangingResolution);
  DCHECK(output_queue_);
  DCHECK(!output_queue_->IsStreaming());
  DCHECK(output_buffer_map_.empty());

  // Number of output buffers we need.
  auto ctrl = device_->GetCtrl(V4L2_CID_MIN_BUFFERS_FOR_CAPTURE);
  if (!ctrl)
    return false;
  output_dpb_size_ = ctrl->value;

  // Output format setup in Initialize().

  uint32_t buffer_count = output_dpb_size_ + kDpbOutputBufferExtraCount;
  if (image_processor_device_)
    buffer_count += kDpbOutputBufferExtraCountForImageProcessor;

  DVLOGF(3) << "buffer_count=" << buffer_count
            << ", coded_size=" << coded_size_.ToString();

  // With ALLOCATE mode the client can sample it as RGB and doesn't need to
  // know the precise format.
  VideoPixelFormat pixel_format =
      (output_mode_ == Config::OutputMode::kImport)
          ? egl_image_format_fourcc_->ToVideoPixelFormat()
          : PIXEL_FORMAT_UNKNOWN;

  child_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::ProvidePictureBuffersWithVisibleRect,
                                client_, buffer_count, pixel_format,
                                egl_image_size_, gfx::Rect(visible_size_)));

  // Go into kAwaitingPictureBuffers to prevent us from doing any more decoding
  // or event handling while we are waiting for AssignPictureBuffers(). Not
  // having Pictures available would not have prevented us from making decoding
  // progress entirely e.g. in the case of H.264 where we could further decode
  // non-slice NALUs and could even get another resolution change before we were
  // done with this one. After we get the buffers, we'll go back into kIdle and
  // kick off further event processing, and eventually go back into kDecoding
  // once no more events are pending (if any).
  decoder_state_ = kAwaitingPictureBuffers;

  return true;
}

void V4L2VideoDecodeAccelerator::DestroyInputBuffers() {
  VLOGF(2);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  if (!input_queue_)
    return;

  if (!input_queue_->DeallocateBuffers()) {
    VLOGF(1) << "Failed deallocating V4L2 input buffers";
    NOTIFY_ERROR(PLATFORM_FAILURE);
  }
}

bool V4L2VideoDecodeAccelerator::DestroyOutputBuffers() {
  VLOGF(2);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(!output_queue_ || !output_queue_->IsStreaming());
  bool success = true;

  if (!output_queue_ || output_buffer_map_.empty())
    return true;

  // Release all buffers waiting for an import buffer event
  output_wait_map_.clear();

  for (size_t i = 0; i < output_buffer_map_.size(); ++i) {
    OutputRecord& output_record = output_buffer_map_[i];

    DVLOGF(3) << "dismissing PictureBuffer id=" << output_record.picture_id;
    child_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Client::DismissPictureBuffer, client_,
                                  output_record.picture_id));
  }

  if (!output_queue_->DeallocateBuffers()) {
    LOG(ERROR) << "Failed deallocating output buffers";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    success = false;
  }

  output_buffer_map_.clear();

  return success;
}

void V4L2VideoDecodeAccelerator::SendBufferToClient(
    size_t output_buffer_index,
    int32_t bitstream_buffer_id,
    V4L2ReadableBufferRef vda_buffer,
    scoped_refptr<FrameResource> frame) {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_GE(bitstream_buffer_id, 0);
  OutputRecord& output_record = output_buffer_map_[output_buffer_index];

  DCHECK_EQ(buffers_at_client_.count(output_record.picture_id), 0u);
  // We need to keep the VDA buffer for now, as the IP still needs to be told
  // which buffer to use so we cannot use this buffer index before the client
  // has returned the corresponding IP buffer.
  buffers_at_client_.emplace(
      output_record.picture_id,
      std::make_pair(std::move(vda_buffer), std::move(frame)));
  const Picture picture(output_record.picture_id, bitstream_buffer_id,
                        gfx::Rect(visible_size_));
  pending_picture_ready_.emplace(output_record.cleared, picture);
  SendPictureReady();
  // This picture will be cleared next time we see it.
  output_record.cleared = true;
}

void V4L2VideoDecodeAccelerator::SendPictureReady() {
  DVLOGF(4);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  bool send_now = (decoder_state_ == kChangingResolution ||
                   decoder_state_ == kResetting || decoder_flushing_);
  while (pending_picture_ready_.size() > 0) {
    bool cleared = pending_picture_ready_.front().cleared;
    const Picture& picture = pending_picture_ready_.front().picture;
    if (cleared && picture_clearing_count_ == 0) {
      // This picture is cleared. It can be posted to a thread different than
      // the main GPU thread to reduce latency. This should be the case after
      // all pictures are cleared at the beginning.
      decode_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&Client::PictureReady, decode_client_, picture));
      pending_picture_ready_.pop();
    } else if (!cleared || send_now) {
      DVLOGF(4) << "cleared=" << pending_picture_ready_.front().cleared
                << ", decoder_state_=" << decoder_state_
                << ", decoder_flushing_=" << decoder_flushing_
                << ", picture_clearing_count_=" << picture_clearing_count_;
      // If the picture is not cleared, post it to the child thread because it
      // has to be cleared in the child thread. A picture only needs to be
      // cleared once. If the decoder is changing resolution, resetting or
      // flushing, send all pictures to ensure PictureReady arrive before
      // ProvidePictureBuffers, NotifyResetDone, or NotifyFlushDone.
      child_task_runner_->PostTaskAndReply(
          FROM_HERE, base::BindOnce(&Client::PictureReady, client_, picture),
          // Unretained is safe. If Client::PictureReady gets to run, |this| is
          // alive. Destroy() will wait the decode thread to finish.
          base::BindOnce(&V4L2VideoDecodeAccelerator::PictureCleared,
                         base::Unretained(this)));
      picture_clearing_count_++;
      pending_picture_ready_.pop();
    } else {
      // This picture is cleared. But some pictures are about to be cleared on
      // the child thread. To preserve the order, do not send this until those
      // pictures are cleared.
      break;
    }
  }
}

void V4L2VideoDecodeAccelerator::PictureCleared() {
  DVLOGF(4) << "clearing count=" << picture_clearing_count_;
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_GT(picture_clearing_count_, 0);
  picture_clearing_count_--;
  SendPictureReady();
}

void V4L2VideoDecodeAccelerator::FrameProcessed(
    int32_t bitstream_buffer_id,
    size_t ip_buffer_index,
    scoped_refptr<FrameResource> frame) {
  DVLOGF(4) << "ip_buffer_index=" << ip_buffer_index
            << ", bitstream_buffer_id=" << bitstream_buffer_id;
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  // TODO(crbug.com/40609453): Remove this workaround once reset callback is
  // implemented.
  if (buffers_at_ip_.empty() ||
      buffers_at_ip_.front().first != bitstream_buffer_id ||
      output_buffer_map_.empty()) {
    // This can happen if image processor is reset.
    // V4L2VideoDecodeAccelerator::Reset() makes
    // |buffers_at_ip_| empty.
    // During ImageProcessor::Reset(), some FrameProcessed() can have been
    // posted to |decoder_thread|. |bitsream_buffer_id| is pushed to
    // |buffers_at_ip_| in ProcessFrame(). Although we
    // are not sure a new bitstream buffer id is pushed after Reset() and before
    // FrameProcessed(), We should skip the case of mismatch of bitstream buffer
    // id for safety.
    // For |output_buffer_map_|, it is cleared in Destroy(). Destroy() destroys
    // ImageProcessor which may call FrameProcessed() in parallel similar to
    // Reset() case.
    DVLOGF(4) << "Ignore processed frame for bitstream_buffer_id="
              << bitstream_buffer_id;
    return;
  }
  DCHECK_GE(ip_buffer_index, 0u);
  DCHECK_LT(ip_buffer_index, output_buffer_map_.size());

  // This is the output record for the buffer received from the IP, which index
  // may differ from the buffer used by the VDA.
  OutputRecord& ip_output_record = output_buffer_map_[ip_buffer_index];
  DVLOGF(4) << "picture_id=" << ip_output_record.picture_id;
  DCHECK_NE(ip_output_record.picture_id, -1);

  // Remove our job from the IP jobs queue
  DCHECK_GT(buffers_at_ip_.size(), 0u);
  DCHECK(buffers_at_ip_.front().first == bitstream_buffer_id);
  // This is the VDA buffer used as input of the IP.
  V4L2ReadableBufferRef vda_buffer = std::move(buffers_at_ip_.front().second);
  buffers_at_ip_.pop();

  SendBufferToClient(ip_buffer_index, bitstream_buffer_id,
                     std::move(vda_buffer), std::move(frame));
  // Flush or resolution change may be waiting image processor to finish.
  if (buffers_at_ip_.empty()) {
    NotifyFlushDoneIfNeeded();
    if (decoder_state_ == kChangingResolution)
      StartResolutionChange();
  }
}

void V4L2VideoDecodeAccelerator::ImageProcessorError() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  VLOGF(1) << "Image processor error";
  NOTIFY_ERROR(PLATFORM_FAILURE);
}

bool V4L2VideoDecodeAccelerator::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  // OnMemoryDump() must be performed on |decoder_thread_|.
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  // |input_queue| and |output_queue| are owned by |decoder_thread_|.
  size_t input_queue_buffers_count = 0;
  size_t input_queue_memory_usage = 0;
  std::string input_queue_buffers_memory_type;
  if (input_queue_) {
    input_queue_buffers_count = input_queue_->AllocatedBuffersCount();
    input_queue_buffers_memory_type =
        V4L2MemoryToString(input_queue_->GetMemoryType());
    if (output_queue_->GetMemoryType() == V4L2_MEMORY_MMAP)
      input_queue_memory_usage = input_queue_->GetMemoryUsage();
  }

  size_t output_queue_buffers_count = 0;
  size_t output_queue_memory_usage = 0;
  std::string output_queue_buffers_memory_type;
  if (output_queue_) {
    output_queue_buffers_count = output_queue_->AllocatedBuffersCount();
    output_queue_buffers_memory_type =
        V4L2MemoryToString(output_queue_->GetMemoryType());
    if (output_queue_->GetMemoryType() == V4L2_MEMORY_MMAP)
      output_queue_memory_usage = output_queue_->GetMemoryUsage();
  }

  const size_t total_usage =
      input_queue_memory_usage + output_queue_memory_usage;

  using ::base::trace_event::MemoryAllocatorDump;

  auto dump_name = base::StringPrintf("gpu/v4l2/decoder/0x%" PRIxPTR,
                                      reinterpret_cast<uintptr_t>(this));
  MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
  dump->AddScalar(MemoryAllocatorDump::kNameSize,
                  MemoryAllocatorDump::kUnitsBytes,
                  static_cast<uint64_t>(total_usage));
  dump->AddScalar("input_queue_memory_usage", MemoryAllocatorDump::kUnitsBytes,
                  static_cast<uint64_t>(input_queue_memory_usage));
  dump->AddScalar("input_queue_buffers_count",
                  MemoryAllocatorDump::kUnitsObjects,
                  static_cast<uint64_t>(input_queue_buffers_count));
  dump->AddString("input_queue_buffers_memory_type", "",
                  input_queue_buffers_memory_type);
  dump->AddScalar("output_queue_memory_usage", MemoryAllocatorDump::kUnitsBytes,
                  static_cast<uint64_t>(output_queue_memory_usage));
  dump->AddScalar("output_queue_buffers_count",
                  MemoryAllocatorDump::kUnitsObjects,
                  static_cast<uint64_t>(output_queue_buffers_count));
  dump->AddString("output_queue_buffers_memory_type", "",
                  output_queue_buffers_memory_type);

  return true;
}

}  // namespace media
