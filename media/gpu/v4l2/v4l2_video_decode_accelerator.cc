// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_video_decode_accelerator.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/base/scopedfd_helper.h"
#include "media/base/unaligned_shared_memory.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_types.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_image_processor.h"
#include "media/gpu/v4l2/v4l2_stateful_workaround.h"
#include "media/gpu/v4l2/v4l2_vda_helpers.h"
#include "media/video/h264_parser.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/scoped_binders.h"

#define NOTIFY_ERROR(x)                      \
  do {                                       \
    VLOGF(1) << "Setting error state:" << x; \
    SetErrorState(x);                        \
  } while (0)

#define IOCTL_OR_ERROR_RETURN_VALUE(type, arg, value, type_str) \
  do {                                                          \
    if (device_->Ioctl(type, arg) != 0) {                       \
      VPLOGF(1) << "ioctl() failed: " << type_str;              \
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

// static
const uint32_t V4L2VideoDecodeAccelerator::supported_input_fourccs_[] = {
    V4L2_PIX_FMT_H264, V4L2_PIX_FMT_VP8, V4L2_PIX_FMT_VP9,
};

struct V4L2VideoDecodeAccelerator::BitstreamBufferRef {
  BitstreamBufferRef(
      base::WeakPtr<Client>& client,
      scoped_refptr<base::SingleThreadTaskRunner>& client_task_runner,
      scoped_refptr<DecoderBuffer> buffer,
      int32_t input_id);
  ~BitstreamBufferRef();

  const base::WeakPtr<Client> client;
  const scoped_refptr<base::SingleThreadTaskRunner> client_task_runner;
  scoped_refptr<DecoderBuffer> buffer;
  size_t bytes_used;
  const int32_t input_id;
};

V4L2VideoDecodeAccelerator::BitstreamBufferRef::BitstreamBufferRef(
    base::WeakPtr<Client>& client,
    scoped_refptr<base::SingleThreadTaskRunner>& client_task_runner,
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
    : egl_image(EGL_NO_IMAGE_KHR),
      picture_id(-1),
      texture_id(0),
      cleared(false) {}

V4L2VideoDecodeAccelerator::OutputRecord::OutputRecord(OutputRecord&&) =
    default;

V4L2VideoDecodeAccelerator::OutputRecord::~OutputRecord() {}

V4L2VideoDecodeAccelerator::PictureRecord::PictureRecord(bool cleared,
                                                         const Picture& picture)
    : cleared(cleared), picture(picture) {}

V4L2VideoDecodeAccelerator::PictureRecord::~PictureRecord() {}

V4L2VideoDecodeAccelerator::V4L2VideoDecodeAccelerator(
    EGLDisplay egl_display,
    const GetGLContextCallback& get_gl_context_cb,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const scoped_refptr<V4L2Device>& device)
    : child_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      decoder_thread_("V4L2DecoderThread"),
      decoder_state_(kUninitialized),
      output_mode_(Config::OutputMode::ALLOCATE),
      device_(device),
      decoder_delay_bitstream_buffer_id_(-1),
      decoder_decode_buffer_tasks_scheduled_(0),
      decoder_flushing_(false),
      decoder_cmd_supported_(false),
      flush_awaiting_last_output_buffer_(false),
      reset_pending_(false),
      decoder_partial_frame_pending_(false),
      output_dpb_size_(0),
      output_planes_count_(0),
      picture_clearing_count_(0),
      device_poll_thread_("V4L2DevicePollThread"),
      egl_display_(egl_display),
      get_gl_context_cb_(get_gl_context_cb),
      make_context_current_cb_(make_context_current_cb),
      video_profile_(VIDEO_CODEC_PROFILE_UNKNOWN),
      input_format_fourcc_(0),
      output_format_fourcc_(0),
      egl_image_format_fourcc_(0),
      egl_image_planes_count_(0),
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
}

bool V4L2VideoDecodeAccelerator::Initialize(const Config& config,
                                            Client* client) {
  VLOGF(2) << "profile: " << config.profile
           << ", output_mode=" << static_cast<int>(config.output_mode);
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(decoder_state_, kUninitialized);

  if (config.is_encrypted()) {
    NOTREACHED() << "Encrypted streams are not supported for this VDA";
    return false;
  }

  if (config.output_mode != Config::OutputMode::ALLOCATE &&
      config.output_mode != Config::OutputMode::IMPORT) {
    NOTREACHED() << "Only ALLOCATE and IMPORT OutputModes are supported";
    return false;
  }

  client_ptr_factory_.reset(new base::WeakPtrFactory<Client>(client));
  client_ = client_ptr_factory_->GetWeakPtr();
  // If we haven't been set up to decode on separate thread via
  // TryToSetupDecodeOnSeparateThread(), use the main thread/client for
  // decode tasks.
  if (!decode_task_runner_) {
    decode_task_runner_ = child_task_runner_;
    DCHECK(!decode_client_);
    decode_client_ = client_;
  }

  video_profile_ = config.profile;

  // We need the context to be initialized to query extensions.
  if (make_context_current_cb_) {
    if (egl_display_ == EGL_NO_DISPLAY) {
      VLOGF(1) << "could not get EGLDisplay";
      return false;
    }

    if (!make_context_current_cb_.Run()) {
      VLOGF(1) << "could not make context current";
      return false;
    }

// TODO(posciak): https://crbug.com/450898.
#if defined(ARCH_CPU_ARMEL)
    if (!gl::g_driver_egl.ext.b_EGL_KHR_fence_sync) {
      VLOGF(1) << "context does not have EGL_KHR_fence_sync";
      return false;
    }
#endif
  } else {
    DVLOGF(2) << "No GL callbacks provided, initializing without GL support";
  }

  decoder_state_ = kInitialized;

  if (!decoder_thread_.Start()) {
    VLOGF(1) << "decoder thread failed to start";
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

  if (video_profile_ >= H264PROFILE_MIN && video_profile_ <= H264PROFILE_MAX) {
    decoder_h264_parser_.reset(new H264Parser());
  }

  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "media::V4l2VideoDecodeAccelerator", decoder_thread_.task_runner());

  // Subscribe to the resolution change event.
  struct v4l2_event_subscription sub;
  memset(&sub, 0, sizeof(sub));
  sub.type = V4L2_EVENT_SOURCE_CHANGE;
  IOCTL_OR_ERROR_RETURN(VIDIOC_SUBSCRIBE_EVENT, &sub);

  if (!CreateInputBuffers()) {
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  decoder_cmd_supported_ = IsDecoderCmdSupported();

  StartDevicePoll();
}

bool V4L2VideoDecodeAccelerator::CheckConfig(const Config& config) {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  input_format_fourcc_ =
      V4L2Device::VideoCodecProfileToV4L2PixFmt(video_profile_, false);

  if (!input_format_fourcc_ ||
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

  workarounds_ =
      CreateV4L2StatefulWorkarounds(V4L2Device::Type::kDecoder, config.profile);

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
            << ", size=" << (buffer ? buffer->data_size() : 0);
  DCHECK(decode_task_runner_->BelongsToCurrentThread());

  if (bitstream_id < 0) {
    VLOGF(1) << "Invalid bitstream buffer, id: " << bitstream_id;
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
    VLOGF(1) << "Failed to provide requested picture buffers. (Got "
             << buffers.size() << ", requested " << req_buffer_count << ")";
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  enum v4l2_memory memory;
  if (!image_processor_device_ && output_mode_ == Config::OutputMode::IMPORT)
    memory = V4L2_MEMORY_DMABUF;
  else
    memory = V4L2_MEMORY_MMAP;

  if (output_queue_->AllocateBuffers(buffers.size(), memory) == 0) {
    VLOGF(1) << "Failed to request buffers!";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  if (output_queue_->AllocatedBuffersCount() != buffers.size()) {
    VLOGF(1) << "Could not allocate requested number of output buffers";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  DCHECK(output_buffer_map_.empty());
  DCHECK(output_wait_map_.empty());
  output_buffer_map_.resize(buffers.size());
  if (image_processor_device_ && output_mode_ == Config::OutputMode::ALLOCATE) {
    if (!CreateImageProcessor())
      return;
  }

  // Reserve all buffers until ImportBufferForPictureTask() is called
  while (output_queue_->FreeBuffersCount() > 0) {
    V4L2WritableBufferRef buffer(output_queue_->GetFreeBuffer());
    DCHECK(buffer.IsValid());
    int i = buffer.BufferId();

    DCHECK_EQ(output_wait_map_.count(buffers[i].id()), 0u);
    output_wait_map_.emplace(buffers[i].id(), std::move(buffer));
  }

  for (size_t i = 0; i < buffers.size(); i++) {
    DCHECK(buffers[i].size() == egl_image_size_);

    OutputRecord& output_record = output_buffer_map_[i];
    DCHECK_EQ(output_record.egl_image, EGL_NO_IMAGE_KHR);
    DCHECK_EQ(output_record.picture_id, -1);
    DCHECK(!output_record.cleared);

    output_record.picture_id = buffers[i].id();
    output_record.texture_id = buffers[i].service_texture_ids().empty()
                                   ? 0
                                   : buffers[i].service_texture_ids()[0];

    if (output_mode_ == Config::OutputMode::ALLOCATE) {
      std::vector<base::ScopedFD> dmabuf_fds;
      dmabuf_fds = egl_image_device_->GetDmabufsForV4L2Buffer(
          i, egl_image_planes_count_, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
      if (dmabuf_fds.empty()) {
        VLOGF(1) << "Failed to get DMABUFs for EGLImage.";
        NOTIFY_ERROR(PLATFORM_FAILURE);
        return;
      }
      int plane_horiz_bits_per_pixel = VideoFrame::PlaneHorizontalBitsPerPixel(
          Fourcc::FromV4L2PixFmt(egl_image_format_fourcc_).ToVideoPixelFormat(),
          0);
      ImportBufferForPictureTask(
          output_record.picture_id, std::move(dmabuf_fds),
          egl_image_size_.width() * plane_horiz_bits_per_pixel / 8);
    }  // else we'll get triggered via ImportBufferForPicture() from client.

    DVLOGF(3) << "buffer[" << i << "]: picture_id=" << output_record.picture_id;
  }

  if (output_mode_ == Config::OutputMode::ALLOCATE) {
    ScheduleDecodeBufferTaskIfNeeded();
  }
}

void V4L2VideoDecodeAccelerator::CreateEGLImageFor(
    size_t buffer_index,
    int32_t picture_buffer_id,
    std::vector<base::ScopedFD> dmabuf_fds,
    GLuint texture_id,
    const gfx::Size& size,
    uint32_t fourcc) {
  DVLOGF(3) << "index=" << buffer_index;
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(texture_id, 0u);

  if (!get_gl_context_cb_ || !make_context_current_cb_) {
    VLOGF(1) << "GL callbacks required for binding to EGLImages";
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  gl::GLContext* gl_context = get_gl_context_cb_.Run();
  if (!gl_context || !make_context_current_cb_.Run()) {
    VLOGF(1) << "No GL context";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  gl::ScopedTextureBinder bind_restore(GL_TEXTURE_EXTERNAL_OES, 0);

  EGLImageKHR egl_image = egl_image_device_->CreateEGLImage(
      egl_display_, gl_context->GetHandle(), texture_id, size, buffer_index,
      fourcc, dmabuf_fds);
  if (egl_image == EGL_NO_IMAGE_KHR) {
    VLOGF(1) << "could not create EGLImageKHR,"
             << " index=" << buffer_index << " texture_id=" << texture_id;
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  decoder_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2VideoDecodeAccelerator::AssignEGLImage,
                                base::Unretained(this), buffer_index,
                                picture_buffer_id, egl_image));
}

void V4L2VideoDecodeAccelerator::AssignEGLImage(size_t buffer_index,
                                                int32_t picture_buffer_id,
                                                EGLImageKHR egl_image) {
  DVLOGF(3) << "index=" << buffer_index << ", picture_id=" << picture_buffer_id;
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  if (IsDestroyPending())
    return;

  // It's possible that while waiting for the EGLImages to be allocated and
  // assigned, we have already decoded more of the stream and saw another
  // resolution change. This is a normal situation, in such a case either there
  // is no output record with this index awaiting an EGLImage to be assigned to
  // it, or the record is already updated to use a newer PictureBuffer and is
  // awaiting an EGLImage associated with a different picture_buffer_id. If so,
  // just discard this image, we will get the one we are waiting for later.
  if (buffer_index >= output_buffer_map_.size() ||
      output_buffer_map_[buffer_index].picture_id != picture_buffer_id) {
    DVLOGF(4) << "Picture set already changed, dropping EGLImage";
    child_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&V4L2Device::DestroyEGLImage),
                       device_, egl_display_, egl_image));
    return;
  }

  OutputRecord& output_record = output_buffer_map_[buffer_index];
  DCHECK_EQ(output_record.egl_image, EGL_NO_IMAGE_KHR);

  output_record.egl_image = egl_image;

  // Make ourselves available if CreateEGLImageFor has been called from
  // ImportBufferForPictureTask.
  if (!image_processor_) {
    DCHECK_EQ(output_wait_map_.count(picture_buffer_id), 1u);
    output_wait_map_.erase(picture_buffer_id);
    if (decoder_state_ != kChangingResolution) {
      Enqueue();
      ScheduleDecodeBufferTaskIfNeeded();
    }
  }
}

void V4L2VideoDecodeAccelerator::ImportBufferForPicture(
    int32_t picture_buffer_id,
    VideoPixelFormat pixel_format,
    gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle) {
  DVLOGF(3) << "picture_buffer_id=" << picture_buffer_id;
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  if (output_mode_ != Config::OutputMode::IMPORT) {
    VLOGF(1) << "Cannot import in non-import mode";
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
  if (pixel_format !=
      Fourcc::FromV4L2PixFmt(egl_image_format_fourcc_).ToVideoPixelFormat()) {
    VLOGF(1) << "Unsupported import format: " << pixel_format;
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  std::vector<base::ScopedFD> dmabuf_fds;
  for (auto& plane : handle.planes) {
    dmabuf_fds.push_back(std::move(plane.fd));
  }

  // If the driver does not accept as many fds as we received from the client,
  // we have to check if the additional fds are actually duplicated fds pointing
  // to previous planes; if so, we can close the duplicates and keep only the
  // original fd(s).
  // Assume that an fd is a duplicate of a previous plane's fd if offset != 0.
  // Otherwise, if offset == 0, return error as it may be pointing to a new
  // plane.
  for (size_t i = dmabuf_fds.size() - 1; i >= egl_image_planes_count_; i--) {
    if (handle.planes[i].offset == 0) {
      VLOGF(1) << "The dmabuf fd points to a new buffer, ";
      NOTIFY_ERROR(INVALID_ARGUMENT);
      return;
    }
    // Drop safely, because this fd is duplicate dmabuf fd pointing to previous
    // buffer and the appropriate address can be accessed by associated offset.
    dmabuf_fds.pop_back();
  }

  for (const auto& plane : handle.planes) {
    DVLOGF(3) << ": offset=" << plane.offset << ", stride=" << plane.stride;
  }

  ImportBufferForPictureTask(picture_buffer_id, std::move(dmabuf_fds),
                             handle.planes[0].stride);
}

void V4L2VideoDecodeAccelerator::ImportBufferForPictureTask(
    int32_t picture_buffer_id,
    std::vector<base::ScopedFD> dmabuf_fds,
    int32_t stride) {
  DVLOGF(3) << "picture_buffer_id=" << picture_buffer_id
            << ", dmabuf_fds.size()=" << dmabuf_fds.size()
            << ", stride=" << stride;
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  TRACE_EVENT2("media,gpu", "V4L2VDA::ImportBufferForPictureTask",
               "picture_buffer_id", picture_buffer_id, "dmabuf_fds_size",
               dmabuf_fds.size());

  if (IsDestroyPending())
    return;

  const auto iter =
      std::find_if(output_buffer_map_.begin(), output_buffer_map_.end(),
                   [picture_buffer_id](const OutputRecord& output_record) {
                     return output_record.picture_id == picture_buffer_id;
                   });
  if (iter == output_buffer_map_.end()) {
    // It's possible that we've already posted a DismissPictureBuffer for this
    // picture, but it has not yet executed when this ImportBufferForPicture was
    // posted to us by the client. In that case just ignore this (we've already
    // dismissed it and accounted for that).
    DVLOGF(3) << "got picture id=" << picture_buffer_id
              << " not in use (anymore?).";
    return;
  }

  // TODO(crbug.com/982172): This must be done in AssignPictureBuffers().
  // However the size of PictureBuffer might not be adjusted by ARC++. So we
  // keep this until ARC++ side is fixed.
  int plane_horiz_bits_per_pixel = VideoFrame::PlaneHorizontalBitsPerPixel(
      Fourcc::FromV4L2PixFmt(egl_image_format_fourcc_).ToVideoPixelFormat(), 0);
  if (plane_horiz_bits_per_pixel == 0 ||
      (stride * 8) % plane_horiz_bits_per_pixel != 0) {
    VLOGF(1) << "Invalid format " << egl_image_format_fourcc_ << " or stride "
             << stride;
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  int adjusted_coded_width = stride * 8 / plane_horiz_bits_per_pixel;
  // If this is the first picture, then adjust the EGL width.
  // Otherwise just check that it remains the same.
  if (decoder_state_ == kAwaitingPictureBuffers) {
    DCHECK_GE(adjusted_coded_width, egl_image_size_.width());
    egl_image_size_.set_width(adjusted_coded_width);
  }
  DCHECK_EQ(egl_image_size_.width(), adjusted_coded_width);

  if (image_processor_device_ && !image_processor_) {
    DCHECK_EQ(kAwaitingPictureBuffers, decoder_state_);
    // This is the first buffer import. Create the image processor and change
    // the decoder state. The client may adjust the coded width. We don't have
    // the final coded size in AssignPictureBuffers yet. Use the adjusted coded
    // width to create the image processor.
    DVLOGF(3) << "Original egl_image_size=" << egl_image_size_.ToString()
              << ", adjusted coded width=" << adjusted_coded_width;
    if (!CreateImageProcessor())
      return;
  }

  if (reset_pending_) {
    FinishReset();
  }

  if (decoder_state_ == kAwaitingPictureBuffers) {
    decoder_state_ = kDecoding;
    DVLOGF(3) << "Change state to kDecoding";
  }

  if (output_mode_ == Config::OutputMode::IMPORT) {
    DCHECK_EQ(egl_image_planes_count_, dmabuf_fds.size());
    DCHECK(!iter->output_frame);

    auto layout = VideoFrameLayout::Create(
        Fourcc::FromV4L2PixFmt(output_format_fourcc_).ToVideoPixelFormat(),
        coded_size_);
    if (!layout) {
      VLOGF(1) << "Cannot create layout!";
      NOTIFY_ERROR(INVALID_ARGUMENT);
      return;
    }
    iter->output_frame = VideoFrame::WrapExternalDmabufs(
        *layout, gfx::Rect(visible_size_), visible_size_,
        DuplicateFDs(dmabuf_fds), base::TimeDelta());
  }

  if (iter->texture_id != 0) {
    if (iter->egl_image != EGL_NO_IMAGE_KHR) {
      child_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(base::IgnoreResult(&V4L2Device::DestroyEGLImage),
                         device_, egl_display_, iter->egl_image));
    }

    size_t index = iter - output_buffer_map_.begin();
    // If we are not using an image processor, create the EGL image ahead of
    // time since we already have its DMABUF fds. It is guaranteed that
    // CreateEGLImageFor will run before the picture is passed to the client
    // because the picture will need to be cleared on the child thread first.
    if (!image_processor_) {
      child_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&V4L2VideoDecodeAccelerator::CreateEGLImageFor,
                         weak_this_, index, picture_buffer_id,
                         std::move(dmabuf_fds), iter->texture_id,
                         egl_image_size_, egl_image_format_fourcc_));

      // Early return, AssignEGLImage will make the buffer available for
      // decoding once the EGL image is created.
      return;
    }
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
  // Must be run on child thread, as we'll insert a sync in the EGL context.
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  std::unique_ptr<gl::GLFenceEGL> egl_fence;

  if (make_context_current_cb_) {
    if (!make_context_current_cb_.Run()) {
      VLOGF(1) << "could not make context current";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }

// TODO(posciak): https://crbug.com/450898.
#if defined(ARCH_CPU_ARMEL)
    egl_fence = gl::GLFenceEGL::Create();
    if (!egl_fence) {
      VLOGF(1) << "gl::GLFenceEGL::Create() failed";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }
#endif
  }

  decoder_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2VideoDecodeAccelerator::ReusePictureBufferTask,
                     base::Unretained(this), picture_buffer_id,
                     std::move(egl_fence)));
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

bool V4L2VideoDecodeAccelerator::TryToSetupDecodeOnSeparateThread(
    const base::WeakPtr<Client>& decode_client,
    const scoped_refptr<base::SingleThreadTaskRunner>& decode_task_runner) {
  VLOGF(2);
  decode_client_ = decode_client;
  decode_task_runner_ = decode_task_runner;
  return true;
}

// static
VideoDecodeAccelerator::SupportedProfiles
V4L2VideoDecodeAccelerator::GetSupportedProfiles() {
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (!device)
    return SupportedProfiles();

  return device->GetSupportedDecodeProfiles(
      base::size(supported_input_fourccs_), supported_input_fourccs_);
}

void V4L2VideoDecodeAccelerator::DecodeTask(scoped_refptr<DecoderBuffer> buffer,
                                            int32_t bitstream_id) {
  DVLOGF(4) << "input_id=" << bitstream_id;
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(decoder_state_, kUninitialized);

  if (IsDestroyPending())
    return;

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
                << ", addr=" << buffer->data()
                << ", size=" << buffer->data_size();
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
    if (current_input_buffer_.IsValid() &&
        current_input_buffer_.GetTimeStamp().tv_sec != kFlushBufferId)
      schedule_task = FlushInputFrame();

    if (schedule_task && AppendToInputFrame(NULL, 0) && FlushInputFrame()) {
      VLOGF(2) << "enqueued flush buffer";
      decoder_partial_frame_pending_ = false;
      schedule_task = true;
    } else {
      // If we failed to enqueue the empty buffer (due to pipeline
      // backpressure), don't advance the bitstream buffer queue, and don't
      // schedule the next task.  This bitstream buffer queue entry will get
      // reprocessed when the pipeline frees up.
      schedule_task = false;
    }
  } else if (buffer->data_size() == 0) {
    // This is a buffer queued from the client that has zero size.  Skip.
    // TODO(sandersd): This shouldn't be possible, empty buffers are never
    // enqueued.
    schedule_task = true;
  } else {
    // This is a buffer queued from the client, with actual contents.  Decode.
    const uint8_t* const data =
        buffer->data() + decoder_current_bitstream_buffer_->bytes_used;
    const size_t data_size =
        buffer->data_size() - decoder_current_bitstream_buffer_->bytes_used;
    if (!AdvanceFrameFragment(data, data_size, &decoded_size)) {
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
    if ((buffer ? buffer->data_size() : 0) ==
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

bool V4L2VideoDecodeAccelerator::AdvanceFrameFragment(const uint8_t* data,
                                                      size_t size,
                                                      size_t* endpos) {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  for (auto& workaround : workarounds_) {
    auto result = workaround->Apply(data, size, endpos);
    if (result == V4L2StatefulWorkaround::Result::NotifyError) {
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return false;
    }
  }

  if (video_profile_ >= H264PROFILE_MIN && video_profile_ <= H264PROFILE_MAX) {
    // For H264, we need to feed HW one frame at a time.  This is going to take
    // some parsing of our input stream.
    decoder_h264_parser_->SetStream(data, size);
    H264NALU nalu;
    H264Parser::Result result;
    *endpos = 0;

    // Keep on peeking the next NALs while they don't indicate a frame
    // boundary.
    for (;;) {
      bool end_of_frame = false;
      result = decoder_h264_parser_->AdvanceToNextNALU(&nalu);
      if (result == H264Parser::kInvalidStream ||
          result == H264Parser::kUnsupportedStream)
        return false;
      if (result == H264Parser::kEOStream) {
        // We've reached the end of the buffer before finding a frame boundary.
        decoder_partial_frame_pending_ = true;
        *endpos = size;
        return true;
      }
      switch (nalu.nal_unit_type) {
        case H264NALU::kNonIDRSlice:
        case H264NALU::kIDRSlice:
          if (nalu.size < 1)
            return false;
          // For these two, if the "first_mb_in_slice" field is zero, start a
          // new frame and return.  This field is Exp-Golomb coded starting on
          // the eighth data bit of the NAL; a zero value is encoded with a
          // leading '1' bit in the byte, which we can detect as the byte being
          // (unsigned) greater than or equal to 0x80.
          if (nalu.data[1] >= 0x80) {
            end_of_frame = true;
            break;
          }
          break;
        case H264NALU::kSEIMessage:
        case H264NALU::kSPS:
        case H264NALU::kPPS:
        case H264NALU::kAUD:
        case H264NALU::kEOSeq:
        case H264NALU::kEOStream:
        case H264NALU::kReserved14:
        case H264NALU::kReserved15:
        case H264NALU::kReserved16:
        case H264NALU::kReserved17:
        case H264NALU::kReserved18:
          // These unconditionally signal a frame boundary.
          end_of_frame = true;
          break;
        default:
          // For all others, keep going.
          break;
      }
      if (end_of_frame) {
        if (!decoder_partial_frame_pending_ && *endpos == 0) {
          // The frame was previously restarted, and we haven't filled the
          // current frame with any contents yet.  Start the new frame here and
          // continue parsing NALs.
        } else {
          // The frame wasn't previously restarted and/or we have contents for
          // the current frame; signal the start of a new frame here: we don't
          // have a partial frame anymore.
          decoder_partial_frame_pending_ = false;
          return true;
        }
      }
      *endpos = (nalu.data + nalu.size) - data;
    }
    NOTREACHED();
    return false;
  } else {
    DCHECK_GE(video_profile_, VP8PROFILE_MIN);
    DCHECK_LE(video_profile_, VP9PROFILE_MAX);
    // For VP8/9, we can just dump the entire buffer.  No fragmentation needed,
    // and we never return a partial frame.
    *endpos = size;
    decoder_partial_frame_pending_ = false;
    return true;
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
  if (decoder_partial_frame_pending_)
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
          (decoder_partial_frame_pending_ || FlushInputFrame()));
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
  if (current_input_buffer_.IsValid()) {
    size_t plane_size = current_input_buffer_.GetPlaneSize(0);
    size_t bytes_used = current_input_buffer_.GetPlaneBytesUsed(0);
    if (bytes_used + size > plane_size) {
      if (!FlushInputFrame())
        return false;
    }
  }

  // Try to get an available input buffer.
  if (!current_input_buffer_.IsValid()) {
    DCHECK(decoder_current_bitstream_buffer_ != NULL);
    DCHECK(input_queue_);

    // See if we can get more free buffers from HW.
    if (input_queue_->FreeBuffersCount() == 0)
      Dequeue();

    current_input_buffer_ = input_queue_->GetFreeBuffer();
    if (!current_input_buffer_.IsValid()) {
      // No buffer available yet.
      DVLOGF(4) << "stalled for input buffers";
      return false;
    }
    struct timeval timestamp = {
        .tv_sec = decoder_current_bitstream_buffer_->input_id};
    current_input_buffer_.SetTimeStamp(timestamp);
  }

  DCHECK(data != NULL || size == 0);
  if (size == 0) {
    // If we asked for an empty buffer, return now.  We return only after
    // getting the next input buffer, since we might actually want an empty
    // input buffer for flushing purposes.
    return true;
  }

  // Copy in to the buffer.
  size_t plane_size = current_input_buffer_.GetPlaneSize(0);
  size_t bytes_used = current_input_buffer_.GetPlaneBytesUsed(0);

  if (size > plane_size - bytes_used) {
    VLOGF(1) << "over-size frame, erroring";
    NOTIFY_ERROR(UNREADABLE_INPUT);
    return false;
  }
  void* mapping = current_input_buffer_.GetPlaneMapping(0);
  memcpy(reinterpret_cast<uint8_t*>(mapping) + bytes_used, data, size);
  current_input_buffer_.SetPlaneBytesUsed(0, bytes_used + size);

  return true;
}

bool V4L2VideoDecodeAccelerator::FlushInputFrame() {
  DVLOGF(4);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(decoder_state_, kUninitialized);
  DCHECK_NE(decoder_state_, kResetting);
  DCHECK_NE(decoder_state_, kError);

  if (!current_input_buffer_.IsValid())
    return true;

  const int32_t input_buffer_id = current_input_buffer_.GetTimeStamp().tv_sec;

  DCHECK(input_buffer_id != kFlushBufferId ||
         current_input_buffer_.GetPlaneBytesUsed(0) == 0);
  // * if input_id >= 0, this input buffer was prompted by a bitstream buffer we
  //   got from the client.  We can skip it if it is empty.
  // * if input_id < 0 (should be kFlushBufferId in this case), this input
  //   buffer was prompted by a flush buffer, and should be queued even when
  //   empty.
  if (input_buffer_id >= 0 && current_input_buffer_.GetPlaneBytesUsed(0) == 0) {
    current_input_buffer_ = V4L2WritableBufferRef();
    return true;
  }

  // Queue it.
  DVLOGF(4) << "submitting input_id=" << input_buffer_id;
  input_ready_queue_.push(std::move(current_input_buffer_));
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

void V4L2VideoDecodeAccelerator::CheckGLFences() {
  DVLOGF(4);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  while (!buffers_awaiting_fence_.empty()) {
    if (buffers_awaiting_fence_.front().first->HasCompleted()) {
      // Buffer at the front of the queue goes back to V4L2Queue's free list
      // and can be reused.
      buffers_awaiting_fence_.pop();
    } else {
      // If we have no free buffers available, then preemptively schedule a
      // call to Enqueue() in a short time, otherwise we may starve out of
      // buffers. The delay chosen roughly corresponds to the time a frame is
      // displayed, which should be optimal in most cases.
      if (output_queue_->FreeBuffersCount() == 0) {
        constexpr int64_t resched_delay = 17;

        decoder_thread_.task_runner()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&V4L2VideoDecodeAccelerator::Enqueue,
                           base::Unretained(this)),
            base::TimeDelta::FromMilliseconds(resched_delay));
      }
      break;
    }
  }
}

void V4L2VideoDecodeAccelerator::Enqueue() {
  DVLOGF(4);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(decoder_state_, kUninitialized);

  // Early return if we are running after DestroyTask() or a resolution change.
  // This can happen due to the PostDelayedTask() in CheckGLFences().
  if (IsDestroyPending() || decoder_state_ == kChangingResolution)
    return;

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
        NofityFlushDone();
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
      if (!EnqueueInputRecord())
        return;
    }
  }

  if (old_inputs_queued == 0 && input_queue_->QueuedBuffersCount() != 0) {
    // We just started up a previously empty queue.
    // Queue state changed; signal interrupt.
    if (!device_->SetDevicePollInterrupt()) {
      VPLOGF(1) << "SetDevicePollInterrupt failed";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }
    // Start VIDIOC_STREAMON if we haven't yet.
    if (!input_queue_->Streamon()) {
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }
  }

  // OUTPUT queue must be started before CAPTURE queue as per codec API.
  if (!input_queue_->IsStreaming())
    return;

  // Enqueue all the outputs we can.
  const int old_outputs_queued = output_queue_->QueuedBuffersCount();
  // Release output buffers which GL fences have been signaled.
  CheckGLFences();
  while (output_queue_->FreeBuffersCount() > 0) {
    if (!EnqueueOutputRecord())
      return;
  }
  if (old_outputs_queued == 0 && output_queue_->QueuedBuffersCount() != 0) {
    // We just started up a previously empty queue.
    // Queue state changed; signal interrupt.
    if (!device_->SetDevicePollInterrupt()) {
      VPLOGF(1) << "SetDevicePollInterrupt(): failed";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }

    if (!output_queue_->Streamon()) {
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }
  }
}

bool V4L2VideoDecodeAccelerator::DequeueResolutionChangeEvent() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(decoder_state_, kUninitialized);
  DVLOGF(3);

  struct v4l2_event ev;
  memset(&ev, 0, sizeof(ev));

  while (device_->Ioctl(VIDIOC_DQEVENT, &ev) == 0) {
    if (ev.type == V4L2_EVENT_SOURCE_CHANGE) {
      if (ev.u.src_change.changes & V4L2_EVENT_SRC_CH_RESOLUTION) {
        VLOGF(2) << "got resolution change event.";
        return true;
      }
    } else {
      VLOGF(1) << "got an event (" << ev.type << ") we haven't subscribed to.";
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
    if (image_processor_device_) {
      if (!ProcessFrame(bitstream_buffer_id, buf)) {
        VLOGF(1) << "Processing frame failed";
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

bool V4L2VideoDecodeAccelerator::EnqueueInputRecord() {
  DVLOGF(4);
  DCHECK(!input_ready_queue_.empty());

  // Enqueue an input (VIDEO_OUTPUT) buffer.
  auto buffer = std::move(input_ready_queue_.front());
  input_ready_queue_.pop();
  int32_t input_id = buffer.GetTimeStamp().tv_sec;
  size_t bytes_used = buffer.GetPlaneBytesUsed(0);
  if (!std::move(buffer).QueueMMap()) {
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }
  DVLOGF(4) << "enqueued input_id=" << input_id << " size=" << bytes_used;
  return true;
}

bool V4L2VideoDecodeAccelerator::EnqueueOutputRecord() {
  DCHECK(output_queue_);
  V4L2WritableBufferRef buffer = output_queue_->GetFreeBuffer();
  DCHECK(buffer.IsValid());

  OutputRecord& output_record = output_buffer_map_[buffer.BufferId()];
  DCHECK_NE(output_record.picture_id, -1);

  bool ret = false;
  switch (buffer.Memory()) {
    case V4L2_MEMORY_MMAP:
      ret = std::move(buffer).QueueMMap();
      break;
    case V4L2_MEMORY_DMABUF: {
      const auto& fds = output_record.output_frame->DmabufFds();
      DCHECK_EQ(output_planes_count_, fds.size());
      ret = std::move(buffer).QueueDMABuf(fds);
      break;
    }
    default:
      NOTREACHED();
  }

  if (!ret) {
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  return true;
}

void V4L2VideoDecodeAccelerator::ReusePictureBufferTask(
    int32_t picture_buffer_id,
    std::unique_ptr<gl::GLFenceEGL> egl_fence) {
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
    // dismissed it and accounted for that) and let the fence object get
    // destroyed.
    DVLOGF(3) << "got picture id= " << picture_buffer_id
              << " not in use (anymore?).";
    return;
  }

  // Take ownership of the EGL fence and keep the buffer out of the game until
  // the fence signals.
  if (egl_fence)
    buffers_awaiting_fence_.emplace(
        std::make_pair(std::move(egl_fence), std::move(iter->second)));

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

  TRACE_EVENT_ASYNC_BEGIN0("media,gpu", "V4L2VDA::FlushTask", this);

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
  if (current_input_buffer_.IsValid()) {
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

  NofityFlushDone();
  // While we were flushing, we early-outed DecodeBufferTask()s.
  ScheduleDecodeBufferTaskIfNeeded();
}

void V4L2VideoDecodeAccelerator::NofityFlushDone() {
  TRACE_EVENT_ASYNC_END0("media,gpu", "V4L2VDA::FlushTask", this);
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

  TRACE_EVENT_ASYNC_BEGIN0("media,gpu", "V4L2VDA::ResetTask", this);

  decoder_current_bitstream_buffer_.reset();
  while (!decoder_input_queue_.empty())
    decoder_input_queue_.pop_front();

  current_input_buffer_ = V4L2WritableBufferRef();

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
    VLOGF(1) << "Fail to reset image processor";
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

  TRACE_EVENT_ASYNC_END0("media,gpu", "V4L2VDA::ResetTask", this);

  // Start poll thread if NotifyFlushDoneIfNeeded has not already.
  if (!device_poll_thread_.IsRunning()) {
    if (!StartDevicePoll())
      return;
  }

  // Reset format-specific bits.
  if (video_profile_ >= H264PROFILE_MIN && video_profile_ <= H264PROFILE_MAX) {
    decoder_h264_parser_.reset(new H264Parser());
  }

  // Jobs drained, we're finished resetting.
  DCHECK_EQ(decoder_state_, kResetting);
  decoder_state_ = kInitialized;

  decoder_partial_frame_pending_ = false;
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
  current_input_buffer_ = V4L2WritableBufferRef();
  decoder_decode_buffer_tasks_scheduled_ = 0;
  while (!decoder_input_queue_.empty())
    decoder_input_queue_.pop_front();
  decoder_flushing_ = false;

  // First liberate all the frames held by the client.
  buffers_at_client_.clear();

  egl_image_device_ = nullptr;

  // The image processor's thread was the user of the image processor device,
  // so let it keep the last reference and destroy it in its own thread.
  image_processor_device_ = nullptr;
  image_processor_ = nullptr;
  while (!buffers_at_ip_.empty())
    buffers_at_ip_.pop();

  DestroyInputBuffers();
  DestroyOutputBuffers();

  input_queue_ = nullptr;
  output_queue_ = nullptr;

  decoder_h264_parser_ = nullptr;
  workarounds_.clear();

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
    VLOGF(1) << "Device thread failed to start";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }
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
    VPLOGF(1) << "SetDevicePollInterrupt(): failed";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }
  device_poll_thread_.Stop();
  // Clear the interrupt now, to be sure.
  if (!device_->ClearDevicePollInterrupt()) {
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

  if (!DestroyOutputBuffers()) {
    VLOGF(1) << "Failed destroying output buffers.";
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
    VLOGF(1) << "Couldn't get format information after resolution change";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  if (!CreateBuffersForFormat(format, visible_size)) {
    VLOGF(1) << "Couldn't reallocate buffers after resolution change";
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
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  // All processing should happen on ServiceDeviceTask(), since we shouldn't
  // touch decoder state from this thread.
  decoder_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2VideoDecodeAccelerator::ServiceDeviceTask,
                                base::Unretained(this), event_pending));
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
  memset(format, 0, sizeof(*format));
  format->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  if (device_->Ioctl(VIDIOC_G_FMT, format) != 0) {
    if (errno == EINVAL) {
      // EINVAL means we haven't seen sufficient stream to decode the format.
      *again = true;
      return true;
    } else {
      VPLOGF(1) << "ioctl() failed: VIDIOC_G_FMT";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return false;
    }
  }

  // Make sure we are still getting the format we set on initialization.
  if (format->fmt.pix_mp.pixelformat != output_format_fourcc_) {
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
  output_planes_count_ = format.fmt.pix_mp.num_planes;
  coded_size_.SetSize(format.fmt.pix_mp.width, format.fmt.pix_mp.height);
  visible_size_ = visible_size;
  if (image_processor_device_) {
    egl_image_size_ = visible_size_;
    egl_image_planes_count_ = 0;
    if (!V4L2ImageProcessor::TryOutputFormat(
            output_format_fourcc_, egl_image_format_fourcc_, coded_size_,
            &egl_image_size_, &egl_image_planes_count_)) {
      VLOGF(1) << "Fail to get output size and plane count of processor";
      return false;
    }
  } else {
    egl_image_size_ = coded_size_;
    egl_image_planes_count_ = output_planes_count_;
  }
  VLOGF(2) << "new resolution: " << coded_size_.ToString()
           << ", visible size: " << visible_size_.ToString()
           << ", decoder output planes count: " << output_planes_count_
           << ", EGLImage size: " << egl_image_size_.ToString()
           << ", EGLImage plane count: " << egl_image_planes_count_;

  return CreateOutputBuffers();
}

gfx::Size V4L2VideoDecodeAccelerator::GetVisibleSize(
    const gfx::Size& coded_size) {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  struct v4l2_rect* visible_rect;
  struct v4l2_selection selection_arg;
  memset(&selection_arg, 0, sizeof(selection_arg));
  selection_arg.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  selection_arg.target = V4L2_SEL_TGT_COMPOSE;

  if (device_->Ioctl(VIDIOC_G_SELECTION, &selection_arg) == 0) {
    DVLOGF(3) << "VIDIOC_G_SELECTION is supported";
    visible_rect = &selection_arg.r;
  } else {
    DVLOGF(3) << "Fallback to VIDIOC_G_CROP";
    struct v4l2_crop crop_arg;
    memset(&crop_arg, 0, sizeof(crop_arg));
    crop_arg.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    if (device_->Ioctl(VIDIOC_G_CROP, &crop_arg) != 0) {
      VPLOGF(1) << "ioctl() VIDIOC_G_CROP failed";
      return coded_size;
    }
    visible_rect = &crop_arg.c;
  }

  gfx::Rect rect(visible_rect->left, visible_rect->top, visible_rect->width,
                 visible_rect->height);
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

  if (input_queue_->AllocateBuffers(kInputBufferCount, V4L2_MEMORY_MMAP) == 0) {
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
  device_->GetSupportedResolution(input_format_fourcc_, &min_resolution,
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
    if (device_->CanCreateEGLImageFrom(fmtdesc.pixelformat)) {
      output_format_fourcc_ = fmtdesc.pixelformat;
      break;
    }
    ++fmtdesc.index;
  }

  DCHECK(!image_processor_device_);
  if (output_format_fourcc_ == 0) {
    VLOGF(2) << "Could not find a usable output format. Try image processor";
    if (!V4L2ImageProcessor::IsSupported()) {
      VLOGF(1) << "Image processor not available";
      return false;
    }
    output_format_fourcc_ =
        v4l2_vda_helpers::FindImageProcessorInputFormat(device_.get());
    if (output_format_fourcc_ == 0) {
      VLOGF(1) << "Can't find a usable input format from image processor";
      return false;
    }
    egl_image_format_fourcc_ =
        v4l2_vda_helpers::FindImageProcessorOutputFormat(device_.get());
    if (egl_image_format_fourcc_ == 0) {
      VLOGF(1) << "Can't find a usable output format from image processor";
      return false;
    }
    image_processor_device_ = V4L2Device::Create();
    if (!image_processor_device_) {
      VLOGF(1) << "Could not create a V4L2Device for image processor";
      return false;
    }
    egl_image_device_ = image_processor_device_;
  } else {
    egl_image_format_fourcc_ = output_format_fourcc_;
    egl_image_device_ = device_;
  }
  VLOGF(2) << "Output format=" << output_format_fourcc_;

  // Just set the fourcc for output; resolution, etc., will come from the
  // driver once it extracts it from the stream.
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  format.fmt.pix_mp.pixelformat = output_format_fourcc_;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_FMT, &format);
  DCHECK_EQ(format.fmt.pix_mp.pixelformat, output_format_fourcc_);

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
      (output_mode_ == Config::OutputMode::ALLOCATE
           ? ImageProcessor::OutputMode::ALLOCATE
           : ImageProcessor::OutputMode::IMPORT);

  image_processor_ = v4l2_vda_helpers::CreateImageProcessor(
      output_format_fourcc_, egl_image_format_fourcc_, coded_size_,
      egl_image_size_, visible_size_, output_buffer_map_.size(),
      image_processor_device_, image_processor_output_mode,
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

  return true;
}

bool V4L2VideoDecodeAccelerator::ProcessFrame(int32_t bitstream_buffer_id,
                                              V4L2ReadableBufferRef buf) {
  DVLOGF(4);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  OutputRecord& output_record = output_buffer_map_[buf->BufferId()];

  scoped_refptr<VideoFrame> input_frame = buf->GetVideoFrame();
  if (!input_frame) {
    VLOGF(1) << "Failed wrapping input frame!";
    return false;
  }

  // Keep reference to the IP input until the frame is processed
  buffers_at_ip_.push(std::make_pair(bitstream_buffer_id, buf));

  // Unretained(this) is safe for FrameReadyCB because |decoder_thread_| is
  // owned by this V4L2VideoDecodeAccelerator and |this| must be valid when
  // FrameReadyCB is executed.
  if (image_processor_->output_mode() == ImageProcessor::OutputMode::IMPORT) {
    image_processor_->Process(
        input_frame, output_record.output_frame,
        base::BindOnce(&V4L2VideoDecodeAccelerator::FrameProcessed,
                       base::Unretained(this), bitstream_buffer_id,
                       buf->BufferId()));
  } else {
    image_processor_->Process(
        input_frame,
        base::BindOnce(&V4L2VideoDecodeAccelerator::FrameProcessed,
                       base::Unretained(this), bitstream_buffer_id));
  }
  return true;
}

bool V4L2VideoDecodeAccelerator::CreateOutputBuffers() {
  VLOGF(2);
  DCHECK(decoder_state_ == kInitialized ||
         decoder_state_ == kChangingResolution);
  DCHECK(output_queue_);
  DCHECK(!output_queue_->IsStreaming());
  DCHECK(output_buffer_map_.empty());

  // Number of output buffers we need.
  struct v4l2_control ctrl;
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_G_CTRL, &ctrl);
  output_dpb_size_ = ctrl.value;

  // Output format setup in Initialize().

  uint32_t buffer_count = output_dpb_size_ + kDpbOutputBufferExtraCount;
  if (image_processor_device_)
    buffer_count += kDpbOutputBufferExtraCountForImageProcessor;

  DVLOGF(3) << "buffer_count=" << buffer_count
            << ", coded_size=" << egl_image_size_.ToString();

  // With ALLOCATE mode the client can sample it as RGB and doesn't need to
  // know the precise format.
  VideoPixelFormat pixel_format =
      (output_mode_ == Config::OutputMode::IMPORT)
          ? Fourcc::FromV4L2PixFmt(egl_image_format_fourcc_)
                .ToVideoPixelFormat()
          : PIXEL_FORMAT_UNKNOWN;

  child_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Client::ProvidePictureBuffersWithVisibleRect, client_,
                     buffer_count, pixel_format, 1, egl_image_size_,
                     gfx::Rect(visible_size_), device_->GetTextureTarget()));

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

  input_queue_->DeallocateBuffers();
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

    if (output_record.egl_image != EGL_NO_IMAGE_KHR) {
      child_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(base::IgnoreResult(&V4L2Device::DestroyEGLImage),
                         device_, egl_display_, output_record.egl_image));
    }

    DVLOGF(3) << "dismissing PictureBuffer id=" << output_record.picture_id;
    child_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Client::DismissPictureBuffer, client_,
                                  output_record.picture_id));
  }

  while (!buffers_awaiting_fence_.empty())
    buffers_awaiting_fence_.pop();

  if (!output_queue_->DeallocateBuffers()) {
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
    scoped_refptr<VideoFrame> frame) {
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
  // TODO(hubbe): Insert correct color space. http://crbug.com/647725
  const Picture picture(output_record.picture_id, bitstream_buffer_id,
                        gfx::Rect(visible_size_), gfx::ColorSpace(), false);
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
    scoped_refptr<VideoFrame> frame) {
  DVLOGF(4) << "ip_buffer_index=" << ip_buffer_index
            << ", bitstream_buffer_id=" << bitstream_buffer_id;
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  // TODO(crbug.com/921825): Remove this workaround once reset callback is
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

  // If the picture has not been cleared yet, this means it is the first time
  // we are seeing this buffer from the image processor. Schedule a call to
  // CreateEGLImageFor before the picture is sent to the client. It is
  // guaranteed that CreateEGLImageFor will complete before the picture is sent
  // to the client as both events happen on the child thread due to the picture
  // uncleared status.
  if (ip_output_record.texture_id != 0 && !ip_output_record.cleared) {
    DCHECK(frame->HasDmaBufs());
    child_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&V4L2VideoDecodeAccelerator::CreateEGLImageFor,
                       weak_this_, ip_buffer_index, ip_output_record.picture_id,
                       media::DuplicateFDs(frame->DmabufFds()),
                       ip_output_record.texture_id, egl_image_size_,
                       egl_image_format_fourcc_));
  }

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
        V4L2Device::V4L2MemoryToString(input_queue_->GetMemoryType());
    if (output_queue_->GetMemoryType() == V4L2_MEMORY_MMAP)
      input_queue_memory_usage = input_queue_->GetMemoryUsage();
  }

  size_t output_queue_buffers_count = 0;
  size_t output_queue_memory_usage = 0;
  std::string output_queue_buffers_memory_type;
  if (output_queue_) {
    output_queue_buffers_count = output_queue_->AllocatedBuffersCount();
    output_queue_buffers_memory_type =
        V4L2Device::V4L2MemoryToString(output_queue_->GetMemoryType());
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
