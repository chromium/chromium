// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/legacy/v4l2_slice_video_decode_accelerator.h"

#include <errno.h>
#include <fcntl.h>
#include <libdrm/drm_fourcc.h>
#include <linux/media.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <memory>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/service/shared_image/gl_image_native_pixmap.h"
#include "media/base/media_switches.h"
#include "media/base/scopedfd_helper.h"
#include "media/base/video_types.h"
#include "media/base/video_util.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_decode_surface.h"
#include "media/gpu/v4l2/v4l2_image_processor_backend.h"
#include "media/gpu/v4l2/v4l2_utils.h"
#include "media/gpu/v4l2/v4l2_vda_helpers.h"
#include "media/gpu/v4l2/v4l2_video_decoder_delegate_h264.h"
#include "media/gpu/v4l2/v4l2_video_decoder_delegate_h265.h"
#include "media/gpu/v4l2/v4l2_video_decoder_delegate_vp8.h"
#include "media/gpu/v4l2/v4l2_video_decoder_delegate_vp9.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

#define NOTIFY_ERROR(x)                       \
  do {                                        \
    VLOGF(1) << "Setting error state: " << x; \
    SetErrorState(x);                         \
  } while (0)

#define IOCTL_OR_ERROR_RETURN_VALUE(type, arg, value, type_str) \
  do {                                                          \
    if (device_->Ioctl(type, arg) != 0) {                       \
      VPLOGF(1) << "ioctl() failed: " << type_str;              \
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

static const std::vector<uint32_t> kSupportedInputFourCCs = {
    V4L2_PIX_FMT_H264_SLICE,
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    V4L2_PIX_FMT_HEVC_SLICE,
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    V4L2_PIX_FMT_VP8_FRAME,
    V4L2_PIX_FMT_VP9_FRAME,
};

// static
base::AtomicRefCount V4L2SliceVideoDecodeAccelerator::num_instances_(0);

V4L2SliceVideoDecodeAccelerator::OutputRecord::OutputRecord()
    : picture_id(-1),
      texture_id(0),
      cleared(false),
      num_times_sent_to_client(0) {}

V4L2SliceVideoDecodeAccelerator::OutputRecord::OutputRecord(OutputRecord&&) =
    default;

V4L2SliceVideoDecodeAccelerator::OutputRecord::~OutputRecord() = default;

struct V4L2SliceVideoDecodeAccelerator::BitstreamBufferRef {
  BitstreamBufferRef(
      base::WeakPtr<VideoDecodeAccelerator::Client>& client,
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      scoped_refptr<DecoderBuffer> buffer,
      int32_t input_id);
  ~BitstreamBufferRef();

  const base::WeakPtr<VideoDecodeAccelerator::Client> client;
  const scoped_refptr<base::SequencedTaskRunner> client_task_runner;
  scoped_refptr<DecoderBuffer> buffer;
  off_t bytes_used;
  const int32_t input_id;
};

V4L2SliceVideoDecodeAccelerator::BitstreamBufferRef::BitstreamBufferRef(
    base::WeakPtr<VideoDecodeAccelerator::Client>& client,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    scoped_refptr<DecoderBuffer> buffer,
    int32_t input_id)
    : client(client),
      client_task_runner(std::move(client_task_runner)),
      buffer(std::move(buffer)),
      bytes_used(0),
      input_id(input_id) {}

V4L2SliceVideoDecodeAccelerator::BitstreamBufferRef::~BitstreamBufferRef() {
  if (input_id >= 0) {
    DVLOGF(5) << "returning input_id: " << input_id;
    client_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            &VideoDecodeAccelerator::Client::NotifyEndOfBitstreamBuffer, client,
            input_id));
  }
}

V4L2SliceVideoDecodeAccelerator::PictureRecord::PictureRecord(
    bool cleared,
    const Picture& picture)
    : cleared(cleared), picture(picture) {}

V4L2SliceVideoDecodeAccelerator::PictureRecord::~PictureRecord() {}

// static
scoped_refptr<gpu::GLImageNativePixmap>
V4L2SliceVideoDecodeAccelerator::CreateGLImage(const gfx::Size& size,
                                               const Fourcc fourcc,
                                               gfx::NativePixmapHandle handle,
                                               GLenum target,
                                               GLuint texture_id) {
  DVLOGF(3);

  size_t num_planes = handle.planes.size();
  DCHECK_LE(num_planes, 3u);

  gfx::BufferFormat buffer_format = gfx::BufferFormat::BGRA_8888;
  switch (fourcc.ToV4L2PixFmt()) {
    case DRM_FORMAT_ARGB8888:
      buffer_format = gfx::BufferFormat::BGRA_8888;
      break;
    case DRM_FORMAT_NV12:
      buffer_format = gfx::BufferFormat::YUV_420_BIPLANAR;
      break;
    case DRM_FORMAT_YVU420:
      buffer_format = gfx::BufferFormat::YVU_420;
      break;
    default:
      NOTREACHED();
  }

  scoped_refptr<gfx::NativePixmap> pixmap =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreateNativePixmapFromHandle(0, size, buffer_format,
                                         std::move(handle));

  DCHECK(pixmap);

  // TODO(b/220336463): plumb the right color space.
  auto image = gpu::GLImageNativePixmap::Create(
      size, buffer_format, std::move(pixmap), target, texture_id);
  DCHECK(image);
  return image;
}

V4L2SliceVideoDecodeAccelerator::V4L2SliceVideoDecodeAccelerator(
    scoped_refptr<V4L2Device> device,
    EGLDisplay egl_display,
    const BindGLImageCallback& bind_image_cb,
    const MakeGLContextCurrentCallback& make_context_current_cb)
    : can_use_decoder_(num_instances_.Increment() < kMaxNumOfInstances),
      output_planes_count_(0),
      child_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      device_(std::move(device)),
      decoder_thread_("V4L2SliceVideoDecodeAcceleratorThread"),
      video_profile_(VIDEO_CODEC_PROFILE_UNKNOWN),
      input_format_fourcc_(0),
      state_(kUninitialized),
      output_mode_(Config::OutputMode::ALLOCATE),
      decoder_flushing_(false),
      decoder_resetting_(false),
      surface_set_change_pending_(false),
      picture_clearing_count_(0),
      egl_display_(egl_display),
      bind_image_cb_(bind_image_cb),
      make_context_current_cb_(make_context_current_cb),
      gl_image_planes_count_(0),
      weak_this_factory_(this) {
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

V4L2SliceVideoDecodeAccelerator::~V4L2SliceVideoDecodeAccelerator() {
  DVLOGF(2);

  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DCHECK(!decoder_thread_.IsRunning());

  DCHECK(requests_.empty());
  DCHECK(output_buffer_map_.empty());

  num_instances_.Decrement();
}

void V4L2SliceVideoDecodeAccelerator::NotifyError(Error error) {
  // Notifying the client should only happen from the client's thread.
  if (!child_task_runner_->BelongsToCurrentThread()) {
    child_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&V4L2SliceVideoDecodeAccelerator::NotifyError,
                                  weak_this_, error));
    return;
  }

  // Notify the decoder's client an error has occurred.
  if (client_) {
    client_->NotifyError(error);
    client_ptr_factory_.reset();
  }
}

bool V4L2SliceVideoDecodeAccelerator::Initialize(const Config& config,
                                                 Client* client) {
  VLOGF(2) << "profile: " << config.profile;
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kUninitialized);

  if (!can_use_decoder_) {
    VLOGF(1) << "Reached the maximum number of decoder instances";
    return false;
  }

  if (config.is_encrypted()) {
    NOTREACHED() << "Encrypted streams are not supported for this VDA";
    return false;
  }

  if (config.output_mode != Config::OutputMode::ALLOCATE &&
      config.output_mode != Config::OutputMode::IMPORT) {
    NOTREACHED() << "Only ALLOCATE and IMPORT OutputModes are supported";
    return false;
  }

  client_ptr_factory_.reset(
      new base::WeakPtrFactory<VideoDecodeAccelerator::Client>(client));
  client_ = client_ptr_factory_->GetWeakPtr();
  // If we haven't been set up to decode on separate sequence via
  // TryToSetupDecodeOnSeparateSequence(), use the main thread/client for
  // decode tasks.
  if (!decode_task_runner_) {
    decode_task_runner_ = child_task_runner_;
    DCHECK(!decode_client_);
    decode_client_ = client_;
  }

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

    gl::GLDisplayEGL* display = gl::GLDisplayEGL::GetDisplayForCurrentContext();
    if (!display || !display->ext->b_EGL_KHR_fence_sync) {
      VLOGF(1) << "context does not have EGL_KHR_fence_sync";
      return false;
    }
  } else {
    DVLOGF(2) << "No GL callbacks provided, initializing without GL support";
  }

  video_profile_ = config.profile;

  input_format_fourcc_ =
      V4L2Device::VideoCodecProfileToV4L2PixFmt(video_profile_, true);

  if (input_format_fourcc_ == V4L2_PIX_FMT_INVALID ||
      !device_->Open(V4L2Device::Type::kDecoder, input_format_fourcc_)) {
    VLOGF(1) << "Failed to open device for profile: " << config.profile
             << " fourcc: " << FourccToString(input_format_fourcc_);
    return false;
  }

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_REQBUFS, &reqbufs);
  // Implicitly this is a way to differentiate between old kernels (pre 5.x)
  // where the request API was not present.
  CHECK(reqbufs.capabilities & V4L2_BUF_CAP_SUPPORTS_REQUESTS);

  // Check if |video_profile_| is supported by a decoder driver.
  if (!IsSupportedProfile(video_profile_)) {
    VLOGF(1) << "Unsupported profile " << GetProfileName(video_profile_);
    return false;
  }

  if (video_profile_ >= H264PROFILE_MIN && video_profile_ <= H264PROFILE_MAX) {
    decoder_ = std::make_unique<H264Decoder>(
        std::make_unique<V4L2VideoDecoderDelegateH264>(this, device_.get()),
        video_profile_, config.container_color_space);
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  } else if (video_profile_ >= HEVCPROFILE_MIN &&
             video_profile_ <= HEVCPROFILE_MAX) {
    decoder_ = std::make_unique<H265Decoder>(
        std::make_unique<V4L2VideoDecoderDelegateH265>(this, device_.get()),
        video_profile_, config.container_color_space);
#endif
  } else if (video_profile_ >= VP8PROFILE_MIN &&
             video_profile_ <= VP8PROFILE_MAX) {
    decoder_ = std::make_unique<VP8Decoder>(
        std::make_unique<V4L2VideoDecoderDelegateVP8>(this, device_.get()),
        config.container_color_space);
  } else if (video_profile_ >= VP9PROFILE_MIN &&
             video_profile_ <= VP9PROFILE_MAX) {
    decoder_ = std::make_unique<VP9Decoder>(
        std::make_unique<V4L2VideoDecoderDelegateVP9>(this, device_.get()),
        video_profile_, config.container_color_space);
  } else {
    NOTREACHED() << "Unsupported profile " << GetProfileName(video_profile_);
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

  if (!SetupFormats())
    return false;

  if (!decoder_thread_.Start()) {
    VLOGF(1) << "device thread failed to start";
    return false;
  }
  decoder_thread_task_runner_ = decoder_thread_.task_runner();
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "media::V4l2SliceVideoDecodeAccelerator",
      decoder_thread_task_runner_);

  state_ = kInitialized;
  output_mode_ = config.output_mode;

  // InitializeTask will NOTIFY_ERROR on failure.
  decoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2SliceVideoDecodeAccelerator::InitializeTask,
                     base::Unretained(this)));

  VLOGF(2) << "V4L2SliceVideoDecodeAccelerator initialized";
  return true;
}

void V4L2SliceVideoDecodeAccelerator::InitializeTask() {
  VLOGF(2);
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kInitialized);
  TRACE_EVENT0("media,gpu", "V4L2SVDA::InitializeTask");

  if (IsDestroyPending())
    return;

  input_queue_ = device_->GetQueue(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
  output_queue_ = device_->GetQueue(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
  if (!input_queue_ || !output_queue_) {
    LOG(ERROR) << "Failed creating V4L2Queues";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  if (!CreateInputBuffers()) {
    LOG(ERROR) << "Failed CreateInputBuffers()";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }
  // Output buffers will be created once decoder gives us information
  // about their size and required count.
  state_ = kDecoding;
}

void V4L2SliceVideoDecodeAccelerator::Destroy() {
  VLOGF(2);
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  // Signal any waiting/sleeping tasks to early exit as soon as possible to
  // avoid waiting too long for the decoder_thread_ to Stop().
  destroy_pending_.Signal();

  weak_this_factory_.InvalidateWeakPtrs();

  if (decoder_thread_.IsRunning()) {
    decoder_thread_task_runner_->PostTask(
        FROM_HERE,
        // The image processor's destructor may post new tasks to
        // |decoder_thread_task_runner_|. In order to make sure that
        // DestroyTask() runs last, we perform shutdown in two stages:
        // 1) Destroy image processor so that no new task it posted by it
        // 2) Post DestroyTask to |decoder_thread_task_runner_| so that it
        //    executes after all the tasks potentially posted by the IP.
        base::BindOnce(
            [](V4L2SliceVideoDecodeAccelerator* vda) {
              // The image processor's thread was the user of the image
              // processor device, so let it keep the last reference and destroy
              // it in its own thread.
              vda->image_processor_device_ = nullptr;
              vda->image_processor_ = nullptr;
              vda->surfaces_at_ip_ = {};
              vda->decoder_thread_task_runner_->PostTask(
                  FROM_HERE,
                  base::BindOnce(&V4L2SliceVideoDecodeAccelerator::DestroyTask,
                                 base::Unretained(vda)));
            },
            base::Unretained(this)));

    // Wait for tasks to finish/early-exit.
    decoder_thread_.Stop();
  }

  delete this;
  VLOGF(2) << "Destroyed";
}

void V4L2SliceVideoDecodeAccelerator::DestroyTask() {
  DVLOGF(2);
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media,gpu", "V4L2SVDA::DestroyTask");

  state_ = kDestroying;

  decoder_->Reset();

  decoder_current_bitstream_buffer_.reset();
  while (!decoder_input_queue_.empty())
    decoder_input_queue_.pop_front();

  // Stop streaming and the V4L2 device poller.
  StopDevicePoll();

  DestroyInputBuffers();
  DestroyOutputs(false);

  input_queue_ = nullptr;
  output_queue_ = nullptr;

  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);

  // Clear the V4L2 devices in the decoder thread so the V4L2Device's
  // destructor is called from the thread that used it.
  device_ = nullptr;

  DCHECK(surfaces_at_device_.empty());
  DCHECK(surfaces_at_display_.empty());
  DCHECK(decoder_display_queue_.empty());
}

bool V4L2SliceVideoDecodeAccelerator::SetupFormats() {
  DCHECK_EQ(state_, kUninitialized);

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
    DVLOGF(1) << "Input fourcc " << input_format_fourcc_
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
  output_format_fourcc_ = absl::nullopt;
  output_planes_count_ = 0;
  while (device_->Ioctl(VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
    auto fourcc = Fourcc::FromV4L2PixFmt(fmtdesc.pixelformat);
    if (fourcc && device_->CanCreateEGLImageFrom(*fourcc)) {
      output_format_fourcc_ = fourcc;
      output_planes_count_ = V4L2Device::GetNumPlanesOfV4L2PixFmt(
        output_format_fourcc_->ToV4L2PixFmt());
      break;
    }
    ++fmtdesc.index;
  }

  DCHECK(!image_processor_device_);
  if (!output_format_fourcc_) {
    VLOGF(2) << "Could not find a usable output format. Trying image processor";
    if (!V4L2ImageProcessorBackend::IsSupported()) {
      VLOGF(1) << "Image processor not available";
      return false;
    }
    image_processor_device_ = V4L2Device::Create();
    if (!image_processor_device_) {
      VLOGF(1) << "Could not create a V4L2Device for image processor";
      return false;
    }
    output_format_fourcc_ =
        v4l2_vda_helpers::FindImageProcessorInputFormat(device_.get());
    if (!output_format_fourcc_) {
      VLOGF(1) << "Can't find a usable input format from image processor";
      return false;
    }
    output_planes_count_ = V4L2Device::GetNumPlanesOfV4L2PixFmt(
        output_format_fourcc_->ToV4L2PixFmt());

    gl_image_format_fourcc_ = v4l2_vda_helpers::FindImageProcessorOutputFormat(
        image_processor_device_.get());
    if (!gl_image_format_fourcc_) {
      VLOGF(1) << "Can't find a usable output format from image processor";
      return false;
    }
    gl_image_planes_count_ = V4L2Device::GetNumPlanesOfV4L2PixFmt(
        gl_image_format_fourcc_->ToV4L2PixFmt());
  } else {
    gl_image_format_fourcc_ = output_format_fourcc_;
    gl_image_planes_count_ = output_planes_count_;
  }

  // Only set fourcc for output; resolution, etc., will come from the
  // driver once it extracts it from the stream.
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  format.fmt.pix_mp.pixelformat = output_format_fourcc_->ToV4L2PixFmt();
  format.fmt.pix_mp.num_planes = V4L2Device::GetNumPlanesOfV4L2PixFmt(
      output_format_fourcc_->ToV4L2PixFmt());
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_FMT, &format);
  DCHECK_EQ(format.fmt.pix_mp.pixelformat,
            output_format_fourcc_->ToV4L2PixFmt());

  DCHECK_EQ(static_cast<size_t>(format.fmt.pix_mp.num_planes),
            output_planes_count_);

  return true;
}

bool V4L2SliceVideoDecodeAccelerator::ResetImageProcessor() {
  VLOGF(2);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(image_processor_);

  if (!image_processor_->Reset())
    return false;

  surfaces_at_ip_ = {};

  return true;
}

bool V4L2SliceVideoDecodeAccelerator::CreateImageProcessor() {
  VLOGF(2);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(!image_processor_);
  const ImageProcessor::OutputMode image_processor_output_mode =
      (output_mode_ == Config::OutputMode::ALLOCATE
           ? ImageProcessor::OutputMode::ALLOCATE
           : ImageProcessor::OutputMode::IMPORT);

  // Start with a brand new image processor device, since the old one was
  // already opened and attempting to open it again is not supported.
  image_processor_device_ = V4L2Device::Create();
  if (!image_processor_device_) {
    VLOGF(1) << "Could not create a V4L2Device for image processor";
    return false;
  }

  image_processor_ = v4l2_vda_helpers::CreateImageProcessor(
      *output_format_fourcc_, *gl_image_format_fourcc_, coded_size_,
      coded_size_, visible_rect_, VideoFrame::StorageType::STORAGE_DMABUFS,
      output_buffer_map_.size(), image_processor_device_,
      image_processor_output_mode,
      // Unretained(this) is safe for ErrorCB because |decoder_thread_| is owned
      // by this V4L2VideoDecodeAccelerator and |this| must be valid when
      // ErrorCB is executed.
      decoder_thread_.task_runner(),
      base::BindRepeating(&V4L2SliceVideoDecodeAccelerator::ImageProcessorError,
                          base::Unretained(this)));

  if (!image_processor_) {
    LOG(ERROR) << "Error creating image processor";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  VLOGF(2) << "ImageProcessor is created: " << image_processor_->backend_type();

  DCHECK_EQ(gl_image_size_, image_processor_->output_config().size);
  return true;
}
bool V4L2SliceVideoDecodeAccelerator::CreateInputBuffers() {
  VLOGF(2);
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(!input_queue_->IsStreaming());

  if (input_queue_->AllocateBuffers(kNumInputBuffers, V4L2_MEMORY_MMAP,
                                    /*incoherent=*/false) < kNumInputBuffers) {
    LOG(ERROR) << "Failed AllocateBuffers";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  CHECK(input_queue_->SupportsRequests());
  requests_queue_ = device_->GetRequestsQueue();
  return !!requests_queue_;
}

bool V4L2SliceVideoDecodeAccelerator::CreateOutputBuffers() {
  VLOGF(2);
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(!output_queue_->IsStreaming());
  DCHECK(output_buffer_map_.empty());
  DCHECK(surfaces_at_display_.empty());
  DCHECK(surfaces_at_ip_.empty());
  DCHECK(surfaces_at_device_.empty());

  gfx::Size pic_size = decoder_->GetPicSize();
  size_t num_pictures = decoder_->GetRequiredNumOfPictures();

  DCHECK_GT(num_pictures, 0u);
  DCHECK(!pic_size.IsEmpty());

  // Since VdaVideoDecoder doesn't allocate PictureBuffer with size adjusted by
  // itself, we have to adjust here.
  auto ret = input_queue_->GetFormat().first;
  if (!ret) {
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }
  struct v4l2_format format = std::move(*ret);

  format.fmt.pix_mp.width = pic_size.width();
  format.fmt.pix_mp.height = pic_size.height();

  if (device_->Ioctl(VIDIOC_S_FMT, &format) != 0) {
    PLOG(ERROR) << "Failed setting OUTPUT format to: " << input_format_fourcc_;
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  // Get the coded size from the CAPTURE queue
  ret = output_queue_->GetFormat().first;
  if (!ret) {
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }
  format = std::move(*ret);

  coded_size_.SetSize(base::checked_cast<int>(format.fmt.pix_mp.width),
                      base::checked_cast<int>(format.fmt.pix_mp.height));
  DCHECK_EQ(coded_size_.width() % 16, 0);
  DCHECK_EQ(coded_size_.height() % 16, 0);

  if (!gfx::Rect(coded_size_).Contains(visible_rect_)) {
    VLOGF(1) << "The visible rectangle is not contained in the coded size";
    NOTIFY_ERROR(UNREADABLE_INPUT);
    return false;
  }

  // Now that we know the desired buffers resolution, ask the image processor
  // what it supports so we can request the correct picture buffers.
  gl_image_size_ = coded_size_;
  if (image_processor_device_) {
    size_t planes_count;
    auto output_size = coded_size_;
    if (!V4L2ImageProcessorBackend::TryOutputFormat(
            output_format_fourcc_->ToV4L2PixFmt(),
            gl_image_format_fourcc_->ToV4L2PixFmt(), coded_size_, &output_size,
            &planes_count)) {
      VLOGF(1) << "Failed to get output size and plane count of IP";
      return false;
    }
    // This is very restrictive because it assumes the IP has the same alignment
    // criteria as the video decoder that will produce the input video frames.
    // In practice, this applies to all Image Processors, i.e. Mediatek devices.
    DCHECK_EQ(coded_size_, output_size);
    if (gl_image_planes_count_ != planes_count) {
      VLOGF(1) << "IP buffers planes count returned by V4L2 (" << planes_count
               << ") doesn't match the computed number ("
               << gl_image_planes_count_ << ")";
      return false;
    }
  } else {
  }

  if (!gfx::Rect(coded_size_).Contains(gfx::Rect(pic_size))) {
    VLOGF(1) << "Got invalid adjusted coded size: " << coded_size_.ToString();
    return false;
  }

  DVLOGF(3) << "buffer_count=" << num_pictures
            << ", pic size=" << pic_size.ToString()
            << ", coded size=" << coded_size_.ToString();

  VideoPixelFormat pixel_format = gl_image_format_fourcc_->ToVideoPixelFormat();
  child_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VideoDecodeAccelerator::Client::ProvidePictureBuffersWithVisibleRect,
          client_, num_pictures, pixel_format, 1, gl_image_size_, visible_rect_,
          device_->GetTextureTarget()));

  // Go into kAwaitingPictureBuffers to prevent us from doing any more decoding
  // or event handling while we are waiting for AssignPictureBuffers(). Not
  // having Pictures available would not have prevented us from making decoding
  // progress entirely e.g. in the case of H.264 where we could further decode
  // non-slice NALUs and could even get another resolution change before we were
  // done with this one. After we get the buffers, we'll go back into kIdle and
  // kick off further event processing, and eventually go back into kDecoding
  // once no more events are pending (if any).
  state_ = kAwaitingPictureBuffers;
  return true;
}

void V4L2SliceVideoDecodeAccelerator::DestroyInputBuffers() {
  VLOGF(2);
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread() ||
         !decoder_thread_.IsRunning());

  if (!input_queue_)
    return;

  DCHECK(!input_queue_->IsStreaming());

  if (!input_queue_->DeallocateBuffers())
    VLOGF(1) << "Failed to deallocate V4L2 input buffers";
}

void V4L2SliceVideoDecodeAccelerator::DismissPictures(
    const std::vector<int32_t>& picture_buffer_ids,
    base::WaitableEvent* done) {
  DVLOGF(3);
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  for (auto picture_buffer_id : picture_buffer_ids) {
    DVLOGF(4) << "dismissing PictureBuffer id=" << picture_buffer_id;
    client_->DismissPictureBuffer(picture_buffer_id);
  }

  done->Signal();
}

void V4L2SliceVideoDecodeAccelerator::ServiceDeviceTask(bool event) {
  DVLOGF(4);
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());

  DVLOGF(3) << "buffer counts: "
            << "INPUT[" << decoder_input_queue_.size() << "]"
            << " => DEVICE[" << input_queue_->FreeBuffersCount() << "+"
            << input_queue_->QueuedBuffersCount() << "/"
            << input_queue_->AllocatedBuffersCount() << "]->["
            << output_queue_->FreeBuffersCount() << "+"
            << output_queue_->QueuedBuffersCount() << "/"
            << output_buffer_map_.size() << "]"
            << " => DISPLAYQ[" << decoder_display_queue_.size() << "]"
            << " => CLIENT[" << surfaces_at_display_.size() << "]";

  if (IsDestroyPending())
    return;

  Dequeue();
}

void V4L2SliceVideoDecodeAccelerator::Dequeue() {
  DVLOGF(4);
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());

  while (input_queue_->QueuedBuffersCount() > 0) {
    DCHECK(input_queue_->IsStreaming());
    auto ret = input_queue_->DequeueBuffer();

    if (ret.first == false) {
      LOG(ERROR) << "Error in DequeueBuffer() on input queue";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    } else if (!ret.second) {
      // we're just out of buffers to dequeue.
      break;
    }

    DVLOGF(4) << "Dequeued input=" << ret.second->BufferId()
              << " count: " << input_queue_->QueuedBuffersCount();
  }

  while (output_queue_->QueuedBuffersCount() > 0) {
    DCHECK(output_queue_->IsStreaming());
    auto ret = output_queue_->DequeueBuffer();
    if (ret.first == false) {
      LOG(ERROR) << "Error in DequeueBuffer() on output queue";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    } else if (!ret.second) {
      // we're just out of buffers to dequeue.
      break;
    }

    const size_t buffer_id = ret.second->BufferId();

    DVLOGF(4) << "Dequeued output=" << buffer_id << " count "
              << output_queue_->QueuedBuffersCount();

    DCHECK(!surfaces_at_device_.empty());
    auto surface = std::move(surfaces_at_device_.front());
    surfaces_at_device_.pop();
    DCHECK_EQ(static_cast<size_t>(surface->output_record()), buffer_id);

    // If using an image processor, process the image before considering it
    // decoded.
    if (image_processor_) {
      if (!ProcessFrame(std::move(ret.second), std::move(surface))) {
        LOG(ERROR) << "Processing frame failed";
        NOTIFY_ERROR(PLATFORM_FAILURE);
      }
    } else {
      DCHECK_EQ(decoded_buffer_map_.count(buffer_id), 0u);
      decoded_buffer_map_.emplace(buffer_id, buffer_id);
      surface->SetDecoded();

      surface->SetReleaseCallback(
          base::BindOnce(&V4L2SliceVideoDecodeAccelerator::ReuseOutputBuffer,
                         base::Unretained(this), std::move(ret.second)));
    }
  }

  // A frame was decoded, see if we can output it.
  TryOutputSurfaces();

  ProcessPendingEventsIfNeeded();
  ScheduleDecodeBufferTaskIfNeeded();
}

void V4L2SliceVideoDecodeAccelerator::NewEventPending() {
  // Switch to event processing mode if we are decoding. Otherwise we are either
  // already in it, or we will potentially switch to it later, after finishing
  // other tasks.
  if (state_ == kDecoding)
    state_ = kIdle;

  ProcessPendingEventsIfNeeded();
}

bool V4L2SliceVideoDecodeAccelerator::FinishEventProcessing() {
  DCHECK_EQ(state_, kIdle);

  state_ = kDecoding;
  ScheduleDecodeBufferTaskIfNeeded();

  return true;
}

void V4L2SliceVideoDecodeAccelerator::ProcessPendingEventsIfNeeded() {
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  // Process pending events, if any, in the correct order.
  // We always first process the surface set change, as it is an internal
  // event from the decoder and interleaving it with external requests would
  // put the decoder in an undefined state.
  using ProcessFunc = bool (V4L2SliceVideoDecodeAccelerator::*)();
  const ProcessFunc process_functions[] = {
      &V4L2SliceVideoDecodeAccelerator::FinishSurfaceSetChange,
      &V4L2SliceVideoDecodeAccelerator::FinishFlush,
      &V4L2SliceVideoDecodeAccelerator::FinishReset,
      &V4L2SliceVideoDecodeAccelerator::FinishEventProcessing,
  };

  for (const auto& fn : process_functions) {
    if (state_ != kIdle)
      return;

    if (!(this->*fn)())
      return;
  }
}

void V4L2SliceVideoDecodeAccelerator::ReuseOutputBuffer(
    V4L2ReadableBufferRef buffer) {
  DVLOGF(4) << "Reusing output buffer, index=" << buffer->BufferId();
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());

  DCHECK_EQ(decoded_buffer_map_.count(buffer->BufferId()), 1u);
  decoded_buffer_map_.erase(buffer->BufferId());

  ScheduleDecodeBufferTaskIfNeeded();
}

bool V4L2SliceVideoDecodeAccelerator::StartDevicePoll() {
  DVLOGF(3) << "Starting device poll";
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());

  if (!input_queue_->Streamon())
    return false;

  if (!output_queue_->Streamon())
    return false;

  // We can use base::Unretained here because the client thread will flush
  // all tasks posted to the decoder thread before deleting the SVDA.
  return device_->StartPolling(
      base::BindRepeating(&V4L2SliceVideoDecodeAccelerator::ServiceDeviceTask,
                          base::Unretained(this)),
      base::BindRepeating(&V4L2SliceVideoDecodeAccelerator::OnPollError,
                          base::Unretained(this)));
}

void V4L2SliceVideoDecodeAccelerator::OnPollError() {
  LOG(ERROR) << "Error on Polling";
  NOTIFY_ERROR(PLATFORM_FAILURE);
}

bool V4L2SliceVideoDecodeAccelerator::StopDevicePoll() {
  DVLOGF(3) << "Stopping device poll";
  if (decoder_thread_.IsRunning())
    DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());

  if (!device_->StopPolling())
    return false;

  // We may be called before the queue is acquired.
  if (input_queue_) {
    if (!input_queue_->Streamoff())
      return false;

    DCHECK_EQ(input_queue_->QueuedBuffersCount(), 0u);
  }

  // We may be called before the queue is acquired.
  if (output_queue_) {
    if (!output_queue_->Streamoff())
      return false;

    DCHECK_EQ(output_queue_->QueuedBuffersCount(), 0u);
  }

  // Mark as decoded to allow reuse.
  while (!surfaces_at_device_.empty())
    surfaces_at_device_.pop();

  // Drop all surfaces that were awaiting decode before being displayed,
  // since we've just cancelled all outstanding decodes.
  while (!decoder_display_queue_.empty())
    decoder_display_queue_.pop();

  DVLOGF(3) << "Device poll stopped";
  return true;
}

void V4L2SliceVideoDecodeAccelerator::Decode(BitstreamBuffer bitstream_buffer) {
  Decode(bitstream_buffer.ToDecoderBuffer(), bitstream_buffer.id());
}

void V4L2SliceVideoDecodeAccelerator::Decode(
    scoped_refptr<DecoderBuffer> buffer,
    int32_t bitstream_id) {
  DVLOGF(4) << "input_id=" << bitstream_id
            << ", size=" << (buffer ? buffer->data_size() : 0);
  DCHECK(decode_task_runner_->RunsTasksInCurrentSequence());

  if (bitstream_id < 0) {
    LOG(ERROR) << "Invalid bitstream buffer, id: " << bitstream_id;
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  decoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2SliceVideoDecodeAccelerator::DecodeTask,
                     base::Unretained(this), std::move(buffer), bitstream_id));
}

void V4L2SliceVideoDecodeAccelerator::DecodeTask(
    scoped_refptr<DecoderBuffer> buffer,
    int32_t bitstream_id) {
  DVLOGF(4) << "input_id=" << bitstream_id
            << " size=" << (buffer ? buffer->data_size() : 0);
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());

  if (IsDestroyPending())
    return;

  std::unique_ptr<BitstreamBufferRef> bitstream_record(new BitstreamBufferRef(
      decode_client_, decode_task_runner_, std::move(buffer), bitstream_id));

  // Skip empty buffer.
  if (!bitstream_record->buffer)
    return;

  decoder_input_queue_.push_back(std::move(bitstream_record));

  TRACE_COUNTER_ID1("media,gpu", "V4L2SVDA decoder input BitstreamBuffers",
                    this, decoder_input_queue_.size());

  ScheduleDecodeBufferTaskIfNeeded();
}

bool V4L2SliceVideoDecodeAccelerator::TrySetNewBistreamBuffer() {
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(!decoder_current_bitstream_buffer_);

  if (decoder_input_queue_.empty())
    return false;

  decoder_current_bitstream_buffer_ = std::move(decoder_input_queue_.front());
  decoder_input_queue_.pop_front();

  if (decoder_current_bitstream_buffer_->input_id == kFlushBufferId) {
    // This is a buffer we queued for ourselves to trigger flush at this time.
    InitiateFlush();
    return false;
  }

  decoder_->SetStream(decoder_current_bitstream_buffer_->input_id,
                      *decoder_current_bitstream_buffer_->buffer);
  return true;
}

void V4L2SliceVideoDecodeAccelerator::ScheduleDecodeBufferTaskIfNeeded() {
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  if (state_ == kDecoding) {
    decoder_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&V4L2SliceVideoDecodeAccelerator::DecodeBufferTask,
                       base::Unretained(this)));
  }
}

void V4L2SliceVideoDecodeAccelerator::DecodeBufferTask() {
  DVLOGF(4);
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media,gpu", "V4L2SVDA::DecodeBufferTask");

  if (IsDestroyPending())
    return;

  if (state_ != kDecoding) {
    DVLOGF(3) << "Early exit, not in kDecoding";
    return;
  }

  while (true) {
    TRACE_EVENT_BEGIN0("media,gpu", "V4L2SVDA::DecodeBufferTask AVD::Decode");
    const AcceleratedVideoDecoder::DecodeResult res = decoder_->Decode();
    TRACE_EVENT_END0("media,gpu", "V4L2SVDA::DecodeBufferTask AVD::Decode");
    switch (res) {
      case AcceleratedVideoDecoder::kConfigChange:
        if (decoder_->GetBitDepth() != 8u) {
          LOG(ERROR) << "Unsupported bit depth: "
                     << base::strict_cast<int>(decoder_->GetBitDepth());
          NOTIFY_ERROR(PLATFORM_FAILURE);
          return;
        }
        if (!IsSupportedProfile(decoder_->GetProfile())) {
          LOG(ERROR) << "Unsupported profile: " << decoder_->GetProfile();
          NOTIFY_ERROR(PLATFORM_FAILURE);
          return;
        }
        VLOGF(2) << "Decoder requesting a new set of surfaces";
        InitiateSurfaceSetChange();
        return;

      case AcceleratedVideoDecoder::kRanOutOfStreamData:
        decoder_current_bitstream_buffer_.reset();
        if (!TrySetNewBistreamBuffer())
          return;

        break;

      case AcceleratedVideoDecoder::kRanOutOfSurfaces:
        // No more surfaces for the decoder, we'll come back once we have more.
        DVLOGF(4) << "Ran out of surfaces";
        return;

      case AcceleratedVideoDecoder::kNeedContextUpdate:
        DVLOGF(4) << "Awaiting context update";
        return;

      case AcceleratedVideoDecoder::kDecodeError:
        LOG(ERROR) << "Error decoding stream";
        NOTIFY_ERROR(PLATFORM_FAILURE);
        return;

      case AcceleratedVideoDecoder::kTryAgain:
        NOTREACHED() << "Should not reach here unless this class accepts "
                        "encrypted streams.";
        LOG(ERROR) << "No key for decoding stream.";
        NOTIFY_ERROR(PLATFORM_FAILURE);
        return;
    }
  }
}

void V4L2SliceVideoDecodeAccelerator::InitiateSurfaceSetChange() {
  VLOGF(2);
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kDecoding);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("media,gpu", "V4L2SVDA Resolution Change",
                                    TRACE_ID_LOCAL(this));
  DCHECK(!surface_set_change_pending_);
  surface_set_change_pending_ = true;
  NewEventPending();
}

bool V4L2SliceVideoDecodeAccelerator::FinishSurfaceSetChange() {
  VLOGF(2);
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());

  if (!surface_set_change_pending_)
    return true;

  if (!surfaces_at_device_.empty())
    return false;

  // Wait until all pending frames in image processor are processed.
  if (image_processor_ && !surfaces_at_ip_.empty())
    return false;

  DCHECK_EQ(state_, kIdle);
  DCHECK(decoder_display_queue_.empty());

#if DCHECK_IS_ON()
  // All output buffers should've been returned from decoder and device by now.
  // The only remaining owner of surfaces may be display (client), and we will
  // dismiss them when destroying output buffers below.
  const size_t num_imported_buffers = base::ranges::count_if(
      output_buffer_map_, [](const OutputRecord& output_record) {
        return output_record.output_frame != nullptr;
      });
  DCHECK_EQ(output_queue_->FreeBuffersCount() + surfaces_at_display_.size(),
            num_imported_buffers);
#endif  // DCHECK_IS_ON()

  if (!StopDevicePoll()) {
    LOG(ERROR) << "Failed StopDevicePoll()";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  image_processor_ = nullptr;

  // Update the visible rect.
  visible_rect_ = decoder_->GetVisibleRect();

  // Dequeued decoded surfaces may be pended in pending_picture_ready_ if they
  // are waiting for some pictures to be cleared. We should post them right away
  // because they are about to be dismissed and destroyed for surface set
  // change.
  SendPictureReady();

  // This will return only once all buffers are dismissed and destroyed.
  // This does not wait until they are displayed however, as display retains
  // references to the buffers bound to textures and will release them
  // after displaying.
  if (!DestroyOutputs(true)) {
    LOG(ERROR) << "Failed DestroyOutputs()";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  if (!CreateOutputBuffers()) {
    LOG(ERROR) << "Failed CreateOutputBuffers()";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  surface_set_change_pending_ = false;
  VLOGF(2) << "Surface set change finished";
  TRACE_EVENT_NESTABLE_ASYNC_END0("media,gpu", "V4L2SVDA Resolution Change",
                                  TRACE_ID_LOCAL(this));
  return true;
}

bool V4L2SliceVideoDecodeAccelerator::DestroyOutputs(bool dismiss) {
  VLOGF(2);
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  std::vector<int32_t> picture_buffers_to_dismiss;

  if (output_buffer_map_.empty())
    return true;

  for (const auto& [output_frame, picture_id, client_texture_id, texture_id,
                    cleared, num_times_sent_to_client] : output_buffer_map_) {
    picture_buffers_to_dismiss.push_back(picture_id);
  }

  if (dismiss) {
    VLOGF(2) << "Scheduling picture dismissal";
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    child_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&V4L2SliceVideoDecodeAccelerator::DismissPictures,
                       weak_this_, picture_buffers_to_dismiss, &done));
    done.Wait();
  }

  // At this point client can't call ReusePictureBuffer on any of the pictures
  // anymore, so it's safe to destroy.
  return DestroyOutputBuffers();
}

bool V4L2SliceVideoDecodeAccelerator::DestroyOutputBuffers() {
  VLOGF(2);
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread() ||
         !decoder_thread_.IsRunning());
  DCHECK(surfaces_at_device_.empty());
  DCHECK(decoder_display_queue_.empty());

  if (!output_queue_ || output_buffer_map_.empty())
    return true;

  DCHECK(!output_queue_->IsStreaming());
  DCHECK_EQ(output_queue_->QueuedBuffersCount(), 0u);

  // Release all buffers waiting for an import buffer event.
  output_wait_map_.clear();

  // Release all buffers awaiting a fence since we are about to destroy them.
  surfaces_awaiting_fence_ = {};

  // It's ok to do this, client will retain references to textures, but we are
  // not interested in reusing the surfaces anymore.
  // This will prevent us from reusing old surfaces in case we have some
  // ReusePictureBuffer() pending on ChildThread already. It's ok to ignore
  // them, because we have already dismissed them (in DestroyOutputs()).
  surfaces_at_display_.clear();

  output_buffer_map_.clear();

  if (!output_queue_->DeallocateBuffers())
    VLOGF(1) << "Failed to deallocate V4L2 output buffers";

  return true;
}

void V4L2SliceVideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<PictureBuffer>& buffers) {
  VLOGF(2);
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  decoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2SliceVideoDecodeAccelerator::AssignPictureBuffersTask,
                     base::Unretained(this), buffers));
}

void V4L2SliceVideoDecodeAccelerator::AssignPictureBuffersTask(
    const std::vector<PictureBuffer>& buffers) {
  VLOGF(2);
  DCHECK(!output_queue_->IsStreaming());
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kAwaitingPictureBuffers);
  TRACE_EVENT1("media,gpu", "V4L2SVDA::AssignPictureBuffersTask",
               "buffers_size", buffers.size());

  if (IsDestroyPending())
    return;

  const uint32_t req_buffer_count = decoder_->GetRequiredNumOfPictures();

  if (buffers.size() < req_buffer_count) {
    LOG(ERROR) << "Failed to provide requested picture buffers. "
               << "(Got " << buffers.size() << ", requested "
               << req_buffer_count << ")";
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  const gfx::Size pic_size_received_from_client = buffers[0].size();
  const gfx::Size pic_size_expected_from_client =
      output_mode_ == Config::OutputMode::ALLOCATE
          ? GetRectSizeFromOrigin(visible_rect_)
          : coded_size_;
  if (output_mode_ == Config::OutputMode::ALLOCATE &&
      pic_size_expected_from_client != pic_size_received_from_client) {
    // In ALLOCATE mode, we don't allow the client to adjust the size. That's
    // because the client is responsible only for creating the GL texture and
    // its dimensions should match the dimensions we use to create the GL image
    // here (eventually).
    LOG(ERROR)
        << "The client supplied a picture buffer with an unexpected size (Got "
        << pic_size_received_from_client.ToString() << ", expected "
        << pic_size_expected_from_client.ToString() << ")";
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  } else if (output_mode_ == Config::OutputMode::IMPORT &&
             !image_processor_device_ &&
             pic_size_expected_from_client != pic_size_received_from_client) {
    // If a client allocates a different frame size, S_FMT should be called with
    // the size.
    v4l2_format format;
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    format.fmt.pix_mp.width = pic_size_received_from_client.width();
    format.fmt.pix_mp.height = pic_size_received_from_client.height();
    format.fmt.pix_mp.pixelformat = output_format_fourcc_->ToV4L2PixFmt();
    format.fmt.pix_mp.num_planes = output_planes_count_;
    if (device_->Ioctl(VIDIOC_S_FMT, &format) != 0) {
      PLOG(ERROR) << "Failed with frame size adjusted by client"
                  << pic_size_received_from_client.ToString();
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }

    coded_size_.SetSize(format.fmt.pix_mp.width, format.fmt.pix_mp.height);
    // If size specified by ProvidePictureBuffers() is adjusted by the client,
    // the size must not be adjusted by a v4l2 driver again.
    if (coded_size_ != pic_size_received_from_client) {
      LOG(ERROR) << "The size of PictureBuffer is invalid."
                 << " size adjusted by the client = "
                 << pic_size_received_from_client.ToString()
                 << " size adjusted by a driver = " << coded_size_.ToString();
      NOTIFY_ERROR(INVALID_ARGUMENT);
      return;
    }

    if (!gfx::Rect(coded_size_).Contains(gfx::Rect(decoder_->GetPicSize()))) {
      LOG(ERROR) << "Got invalid adjusted coded size: "
                 << coded_size_.ToString();
      NOTIFY_ERROR(INVALID_ARGUMENT);
      return;
    }

    gl_image_size_ = coded_size_;
  }

  const v4l2_memory memory =
      (image_processor_device_ || output_mode_ == Config::OutputMode::ALLOCATE
           ? V4L2_MEMORY_MMAP
           : V4L2_MEMORY_DMABUF);
  if (output_queue_->AllocateBuffers(buffers.size(), memory,
                                     /*incoherent=*/false) != buffers.size()) {
    LOG(ERROR) << "Could not allocate enough output buffers";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  DCHECK(output_buffer_map_.empty());
  DCHECK(output_wait_map_.empty());
  output_buffer_map_.resize(buffers.size());

  // In import mode we will create the IP when importing the first buffer.
  if (image_processor_device_ && output_mode_ == Config::OutputMode::ALLOCATE) {
    if (!CreateImageProcessor()) {
      LOG(ERROR) << "Failed CreateImageProcessor()";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }
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
    DCHECK_EQ(output_record.cleared, false);

    output_record.picture_id = buffers[i].id();
    output_record.texture_id = buffers[i].service_texture_ids().empty()
                                   ? 0
                                   : buffers[i].service_texture_ids()[0];

    output_record.client_texture_id = buffers[i].client_texture_ids().empty()
                                          ? 0
                                          : buffers[i].client_texture_ids()[0];

    // We move the buffer into output_wait_map_, so get a reference to
    // its video frame if we need it to create the native pixmap for import.
    scoped_refptr<VideoFrame> video_frame;
    if (output_mode_ == Config::OutputMode::ALLOCATE &&
        !image_processor_device_) {
      video_frame = buffer.GetVideoFrame();
    }

    // The buffer will remain here until ImportBufferForPicture is called,
    // either by the client, or by ourselves, if we are allocating.
    DCHECK_EQ(output_wait_map_.count(buffers[i].id()), 0u);
    output_wait_map_.emplace(buffers[i].id(), std::move(buffer));

    // If we are in allocate mode, then we can already call
    // ImportBufferForPictureTask().
    if (output_mode_ == Config::OutputMode::ALLOCATE) {
      gfx::NativePixmapHandle native_pixmap;

      // If we are using an image processor, the DMABufs that we need to import
      // are those of the image processor's buffers, not the decoders. So
      // pass an empty native pixmap in that case.
      if (!image_processor_device_) {
        native_pixmap =
            CreateGpuMemoryBufferHandle(video_frame.get()).native_pixmap_handle;
      }

      ImportBufferForPictureTask(output_record.picture_id,
                                 std::move(native_pixmap));
    }  // else we'll get triggered via ImportBufferForPicture() from client.

    DVLOGF(3) << "buffer[" << i << "]: picture_id=" << output_record.picture_id;
  }

  if (!StartDevicePoll()) {
    LOG(ERROR) << "Failed StartDevicePoll()";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }
}

void V4L2SliceVideoDecodeAccelerator::CreateGLImageFor(
    scoped_refptr<V4L2Device> gl_device,
    size_t buffer_index,
    int32_t picture_buffer_id,
    gfx::NativePixmapHandle handle,
    GLuint client_texture_id,
    GLuint texture_id,
    const gfx::Size& visible_size,
    const Fourcc fourcc) {
  DVLOGF(3) << "index=" << buffer_index;
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(texture_id, 0u);
  DCHECK(gl_device->CanCreateEGLImageFrom(fourcc));
  TRACE_EVENT1("media,gpu", "V4L2SVDA::CreateGLImageFor", "picture_buffer_id",
               picture_buffer_id);

  if (!make_context_current_cb_) {
    LOG(ERROR) << "GL callbacks required for binding to GLImages";
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }
  if (!make_context_current_cb_.Run()) {
    LOG(ERROR) << "No GL context";
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  scoped_refptr<gpu::GLImageNativePixmap> gl_image =
      CreateGLImage(visible_size, fourcc, std::move(handle),
                    gl_device->GetTextureTarget(), texture_id);
  if (!gl_image) {
    LOG(ERROR) << "Could not create GLImage,"
               << " index=" << buffer_index << " texture_id=" << texture_id;
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }
  bool ret = bind_image_cb_.Run(client_texture_id,
                                gl_device->GetTextureTarget(), gl_image);
  if (!ret) {
    LOG(ERROR) << "Error while running bind image callback";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }
}

void V4L2SliceVideoDecodeAccelerator::ImportBufferForPicture(
    int32_t picture_buffer_id,
    VideoPixelFormat pixel_format,
    gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle) {
  DVLOGF(3) << "picture_buffer_id=" << picture_buffer_id;
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  if (output_mode_ != Config::OutputMode::IMPORT) {
    LOG(ERROR) << "Cannot import in non-import mode";
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  decoder_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &V4L2SliceVideoDecodeAccelerator::ImportBufferForPictureForImportTask,
          base::Unretained(this), picture_buffer_id, pixel_format,
          std::move(gpu_memory_buffer_handle.native_pixmap_handle)));
}

void V4L2SliceVideoDecodeAccelerator::ImportBufferForPictureForImportTask(
    int32_t picture_buffer_id,
    VideoPixelFormat pixel_format,
    gfx::NativePixmapHandle handle) {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  if (pixel_format != gl_image_format_fourcc_->ToVideoPixelFormat()) {
    LOG(ERROR) << "Unsupported import format: "
               << VideoPixelFormatToString(pixel_format) << ", expected "
               << VideoPixelFormatToString(
                      gl_image_format_fourcc_->ToVideoPixelFormat());
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  ImportBufferForPictureTask(picture_buffer_id, std::move(handle));
}

void V4L2SliceVideoDecodeAccelerator::ImportBufferForPictureTask(
    int32_t picture_buffer_id,
    gfx::NativePixmapHandle handle) {
  DVLOGF(3) << "picture_buffer_id=" << picture_buffer_id;
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());

  if (IsDestroyPending())
    return;

  if (surface_set_change_pending_)
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

  // TODO(crbug.com/982172): ARC++ may adjust the size of the buffer due to
  // allocator constraints, but the VDA API does not provide a way for it to
  // communicate the actual buffer size. If we are importing, make sure that the
  // actual buffer size is coherent with what we expect, and adjust our size if
  // needed.
  if (output_mode_ == Config::OutputMode::IMPORT) {
    DCHECK_GT(handle.planes.size(), 0u);
    const gfx::Size handle_size = v4l2_vda_helpers::NativePixmapSizeFromHandle(
        handle, *gl_image_format_fourcc_, gl_image_size_);

    // If this is the first picture, then adjust the EGL width.
    // Otherwise just check that it remains the same.
    if (state_ == kAwaitingPictureBuffers) {
      DCHECK_GE(handle_size.width(), gl_image_size_.width());
      DVLOGF(3) << "Original gl_image_size=" << gl_image_size_.ToString()
                << ", adjusted buffer size=" << handle_size.ToString();
      gl_image_size_ = handle_size;
    }
    DCHECK_EQ(gl_image_size_, handle_size);

    // For allocate mode, the IP will already have been created in
    // AssignPictureBuffersTask.
    if (image_processor_device_ && !image_processor_) {
      DCHECK_EQ(kAwaitingPictureBuffers, state_);
      // This is the first buffer import. Create the image processor and change
      // the decoder state. The client may adjust the coded width. We don't have
      // the final coded size in AssignPictureBuffers yet. Use the adjusted
      // coded width to create the image processor.
      if (!CreateImageProcessor())
        return;
    }
  }

  // Put us in kIdle to allow further event processing.
  // ProcessPendingEventsIfNeeded() will put us back into kDecoding after all
  // other pending events are processed successfully.
  if (state_ == kAwaitingPictureBuffers) {
    state_ = kIdle;
    decoder_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &V4L2SliceVideoDecodeAccelerator::ProcessPendingEventsIfNeeded,
            base::Unretained(this)));
  }

  // If we are importing, create the output VideoFrame that we will render
  // into.
  if (output_mode_ == Config::OutputMode::IMPORT) {
    DCHECK_GT(handle.planes.size(), 0u);
    DCHECK(!iter->output_frame);

    // Duplicate the buffer FDs for the VideoFrame instance.
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
        gl_image_format_fourcc_->ToVideoPixelFormat(), gl_image_size_,
        std::move(color_planes));
    if (!layout) {
      LOG(ERROR) << "Cannot create layout!";
      NOTIFY_ERROR(INVALID_ARGUMENT);
      return;
    }
    iter->output_frame = VideoFrame::WrapExternalDmabufs(
        *layout, visible_rect_, visible_rect_.size(), std::move(duped_fds),
        base::TimeDelta());
  }

  // We should only create the GL image if rendering is enabled
  // (texture_id !=0). Moreover, if an image processor is in use, we will
  // create the GL image when its buffer becomes visible in FrameProcessed().
  if (iter->texture_id != 0 && !image_processor_) {
    DCHECK_EQ(Config::OutputMode::ALLOCATE, output_mode_);
    DCHECK_GT(handle.planes.size(), 0u);
    size_t index = iter - output_buffer_map_.begin();

    child_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&V4L2SliceVideoDecodeAccelerator::CreateGLImageFor,
                       weak_this_, device_, index, picture_buffer_id,
                       std::move(handle), iter->client_texture_id,
                       iter->texture_id, GetRectSizeFromOrigin(visible_rect_),
                       *gl_image_format_fourcc_));
  }

  // Buffer is now ready to be used.
  DCHECK_EQ(output_wait_map_.count(picture_buffer_id), 1u);
  output_wait_map_.erase(picture_buffer_id);
  ScheduleDecodeBufferTaskIfNeeded();
}

void V4L2SliceVideoDecodeAccelerator::ReusePictureBuffer(
    int32_t picture_buffer_id) {
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DVLOGF(4) << "picture_buffer_id=" << picture_buffer_id;

  std::unique_ptr<gl::GLFenceEGL> egl_fence;

  if (make_context_current_cb_) {
    if (!make_context_current_cb_.Run()) {
      LOG(ERROR) << "could not make context current";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }

    egl_fence = gl::GLFenceEGL::Create();
    if (!egl_fence) {
      LOG(ERROR) << "gl::GLFenceEGL::Create() failed";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }
  }

  decoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2SliceVideoDecodeAccelerator::ReusePictureBufferTask,
                     base::Unretained(this), picture_buffer_id,
                     std::move(egl_fence)));
}

void V4L2SliceVideoDecodeAccelerator::ReusePictureBufferTask(
    int32_t picture_buffer_id,
    std::unique_ptr<gl::GLFenceEGL> egl_fence) {
  DVLOGF(4) << "picture_buffer_id=" << picture_buffer_id;
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());

  if (IsDestroyPending())
    return;

  V4L2DecodeSurfaceByPictureBufferId::iterator it =
      surfaces_at_display_.find(picture_buffer_id);
  if (it == surfaces_at_display_.end()) {
    // It's possible that we've already posted a DismissPictureBuffer for this
    // picture, but it has not yet executed when this ReusePictureBuffer was
    // posted to us by the client. In that case just ignore this (we've already
    // dismissed it and accounted for that) and let the fence object get
    // destroyed.
    DVLOGF(3) << "got picture id=" << picture_buffer_id
              << " not in use (anymore?).";
    return;
  }

  DCHECK_EQ(decoded_buffer_map_.count(it->second->output_record()), 1u);
  const size_t output_map_index =
      decoded_buffer_map_[it->second->output_record()];
  DCHECK_LT(output_map_index, output_buffer_map_.size());
  OutputRecord& output_record = output_buffer_map_[output_map_index];
  if (!output_record.at_client()) {
    LOG(ERROR) << "picture_buffer_id not reusable";
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  --output_record.num_times_sent_to_client;
  // A output buffer might be sent multiple times. We only use the last fence.
  // When the last fence is signaled, all the previous fences must be executed.
  if (!output_record.at_client()) {
    // Take ownership of the EGL fence.
    if (egl_fence)
      surfaces_awaiting_fence_.push(
          std::make_pair(std::move(egl_fence), std::move(it->second)));

    surfaces_at_display_.erase(it);
  }
}

void V4L2SliceVideoDecodeAccelerator::Flush() {
  VLOGF(2);
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  decoder_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V4L2SliceVideoDecodeAccelerator::FlushTask,
                                base::Unretained(this)));
}

void V4L2SliceVideoDecodeAccelerator::FlushTask() {
  VLOGF(2);
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());

  if (IsDestroyPending())
    return;

  // Queue an empty buffer which - when reached - will trigger flush sequence.
  decoder_input_queue_.push_back(std::make_unique<BitstreamBufferRef>(
      decode_client_, decode_task_runner_, nullptr, kFlushBufferId));

  ScheduleDecodeBufferTaskIfNeeded();
}

void V4L2SliceVideoDecodeAccelerator::InitiateFlush() {
  VLOGF(2);
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("media,gpu", "V4L2SVDA Flush",
                                    TRACE_ID_LOCAL(this));

  // This will trigger output for all remaining surfaces in the decoder.
  // However, not all of them may be decoded yet (they would be queued
  // in hardware then).
  if (!decoder_->Flush()) {
    LOG(ERROR) << "Failed flushing the decoder.";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  // Put the decoder in an idle state, ready to resume.
  decoder_->Reset();

  DCHECK(!decoder_flushing_);
  decoder_flushing_ = true;
  NewEventPending();
}

bool V4L2SliceVideoDecodeAccelerator::FinishFlush() {
  VLOGF(4);
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());

  if (!decoder_flushing_)
    return true;

  if (!surfaces_at_device_.empty())
    return false;

  // Even if all output buffers have been returned, the decoder may still
  // be holding on an input device. Wait until the queue is actually drained.
  if (input_queue_->QueuedBuffersCount() != 0)
    return false;

  // Wait until all pending image processor tasks are completed.
  if (image_processor_ && !surfaces_at_ip_.empty())
    return false;

  DCHECK_EQ(state_, kIdle);

  // At this point, all remaining surfaces are decoded and dequeued, and since
  // we have already scheduled output for them in InitiateFlush(), their
  // respective PictureReady calls have been posted (or they have been queued on
  // pending_picture_ready_). So at this time, once we SendPictureReady(),
  // we will have all remaining PictureReady() posted to the client and we
  // can post NotifyFlushDone().
  DCHECK(decoder_display_queue_.empty());

  // Decoder should have already returned all surfaces and all surfaces are
  // out of hardware. There can be no other owners of input buffers.
  DCHECK_EQ(input_queue_->FreeBuffersCount(),
            input_queue_->AllocatedBuffersCount());

  SendPictureReady();

  decoder_flushing_ = false;
  VLOGF(2) << "Flush finished";

  child_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::NotifyFlushDone, client_));

  TRACE_EVENT_NESTABLE_ASYNC_END0("media,gpu", "V4L2SVDA Flush",
                                  TRACE_ID_LOCAL(this));
  return true;
}

void V4L2SliceVideoDecodeAccelerator::Reset() {
  VLOGF(2);
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  decoder_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V4L2SliceVideoDecodeAccelerator::ResetTask,
                                base::Unretained(this)));
}

void V4L2SliceVideoDecodeAccelerator::ResetTask() {
  VLOGF(2);
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("media,gpu", "V4L2SVDA Reset",
                                    TRACE_ID_LOCAL(this));

  if (IsDestroyPending())
    return;

  if (decoder_resetting_) {
    // This is a bug in the client, multiple Reset()s before NotifyResetDone()
    // are not allowed.
    NOTREACHED() << "Client should not be requesting multiple Reset()s";
    return;
  }

  // Put the decoder in an idle state, ready to resume.
  decoder_->Reset();

  // Drop all remaining inputs.
  decoder_current_bitstream_buffer_.reset();
  while (!decoder_input_queue_.empty())
    decoder_input_queue_.pop_front();

  decoder_resetting_ = true;
  NewEventPending();
}

bool V4L2SliceVideoDecodeAccelerator::FinishReset() {
  VLOGF(4);
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());

  if (!decoder_resetting_)
    return true;

  if (!surfaces_at_device_.empty())
    return false;

  // Drop all buffers in image processor.
  if (image_processor_ && !ResetImageProcessor()) {
    LOG(ERROR) << "Fail to reset image processor";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  DCHECK_EQ(state_, kIdle);
  DCHECK(!decoder_flushing_);
  SendPictureReady();

  // Drop any pending outputs.
  while (!decoder_display_queue_.empty())
    decoder_display_queue_.pop();

  // At this point we can have no input buffers in the decoder, because we
  // Reset()ed it in ResetTask(), and have not scheduled any new Decode()s
  // having been in kIdle since. We don't have any surfaces in the HW either -
  // we just checked that surfaces_at_device_.empty(), and inputs are tied
  // to surfaces. Since there can be no other owners of input buffers, we can
  // simply mark them all as available.
  DCHECK_EQ(input_queue_->QueuedBuffersCount(), 0u);

  decoder_resetting_ = false;
  VLOGF(2) << "Reset finished";

  child_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::NotifyResetDone, client_));

  TRACE_EVENT_NESTABLE_ASYNC_END0("media,gpu", "V4L2SVDA Reset",
                                  TRACE_ID_LOCAL(this));
  return true;
}

bool V4L2SliceVideoDecodeAccelerator::IsDestroyPending() {
  return destroy_pending_.IsSignaled();
}

void V4L2SliceVideoDecodeAccelerator::SetErrorState(Error error) {
  // We can touch decoder_state_ only if this is the decoder thread or the
  // decoder thread isn't running.
  if (decoder_thread_.IsRunning() &&
      !decoder_thread_task_runner_->BelongsToCurrentThread()) {
    decoder_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&V4L2SliceVideoDecodeAccelerator::SetErrorState,
                       base::Unretained(this), error));
    return;
  }

  // Notifying the client of an error will only happen if we are already
  // initialized, as the API does not allow doing so before that. Subsequent
  // errors and errors while destroying will be suppressed.
  if (state_ != kError && state_ != kUninitialized && state_ != kDestroying)
    NotifyError(error);

  state_ = kError;
}

bool V4L2SliceVideoDecodeAccelerator::SubmitSlice(
    V4L2DecodeSurface* dec_surface,
    const uint8_t* data,
    size_t size) {
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());

  V4L2WritableBufferRef& input_buffer = dec_surface->input_buffer();

  const size_t plane_size = input_buffer.GetPlaneSize(0);
  const size_t bytes_used = input_buffer.GetPlaneBytesUsed(0);

  if (bytes_used + size > plane_size) {
    VLOGF(1) << "Input buffer too small";
    return false;
  }

  uint8_t* mapping = static_cast<uint8_t*>(input_buffer.GetPlaneMapping(0));
  DCHECK_NE(mapping, nullptr);
  memcpy(mapping + bytes_used, data, size);
  input_buffer.SetPlaneBytesUsed(0, bytes_used + size);

  return true;
}

void V4L2SliceVideoDecodeAccelerator::DecodeSurface(
    scoped_refptr<V4L2DecodeSurface> dec_surface) {
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());

  DVLOGF(3) << "Submitting decode for surface: " << dec_surface->ToString();
  if (!dec_surface->Submit()) {
    LOG(ERROR) << "Error while submitting frame for decoding!";
    NOTIFY_ERROR(PLATFORM_FAILURE);
  }

  surfaces_at_device_.push(dec_surface);
}

void V4L2SliceVideoDecodeAccelerator::SurfaceReady(
    scoped_refptr<V4L2DecodeSurface> dec_surface,
    int32_t bitstream_id,
    const gfx::Rect& visible_rect,
    const VideoColorSpace& color_space) {
  DVLOGF(4);
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());

  dec_surface->SetVisibleRect(visible_rect);
  dec_surface->SetColorSpace(color_space);
  decoder_display_queue_.push(std::make_pair(bitstream_id, dec_surface));
  TryOutputSurfaces();
}

void V4L2SliceVideoDecodeAccelerator::TryOutputSurfaces() {
  while (!decoder_display_queue_.empty()) {
    scoped_refptr<V4L2DecodeSurface> dec_surface =
        decoder_display_queue_.front().second;

    if (!dec_surface->decoded())
      break;

    int32_t bitstream_id = decoder_display_queue_.front().first;
    decoder_display_queue_.pop();
    OutputSurface(bitstream_id, dec_surface);
  }
}

void V4L2SliceVideoDecodeAccelerator::OutputSurface(
    int32_t bitstream_id,
    scoped_refptr<V4L2DecodeSurface> dec_surface) {
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());

  DCHECK_EQ(decoded_buffer_map_.count(dec_surface->output_record()), 1u);
  const size_t output_map_index =
      decoded_buffer_map_[dec_surface->output_record()];
  DCHECK_LT(output_map_index, output_buffer_map_.size());
  OutputRecord& output_record = output_buffer_map_[output_map_index];

  if (!output_record.at_client()) {
    bool inserted =
        surfaces_at_display_
            .insert(std::make_pair(output_record.picture_id, dec_surface))
            .second;
    DCHECK(inserted);
  } else {
    // The surface is already sent to client, and not returned back yet.
    DCHECK(surfaces_at_display_.find(output_record.picture_id) !=
           surfaces_at_display_.end());
    CHECK(surfaces_at_display_[output_record.picture_id].get() ==
          dec_surface.get());
  }

  DCHECK_NE(output_record.picture_id, -1);
  ++output_record.num_times_sent_to_client;

  Picture picture(
      output_record.picture_id, bitstream_id, dec_surface->visible_rect(),
      dec_surface->color_space().ToGfxColorSpace(), true /* allow_overlay */);
  DVLOGF(4) << dec_surface->ToString()
            << ", bitstream_id: " << picture.bitstream_buffer_id()
            << ", picture_id: " << picture.picture_buffer_id()
            << ", visible_rect: " << picture.visible_rect().ToString();
  pending_picture_ready_.push(PictureRecord(output_record.cleared, picture));
  SendPictureReady();
  output_record.cleared = true;
}

void V4L2SliceVideoDecodeAccelerator::CheckGLFences() {
  DVLOGF(4);
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());

  while (!surfaces_awaiting_fence_.empty() &&
         surfaces_awaiting_fence_.front().first->HasCompleted()) {
    // Buffer at the front of the queue goes back to V4L2Queue's free list
    // and can be reused.
    surfaces_awaiting_fence_.pop();
  }

  // If we have no free buffers available, then preemptively schedule a
  // call to DecodeBufferTask() in a short time, otherwise we may starve out
  // of buffers because fences will not call back into us once they are
  // signaled. The delay chosen roughly corresponds to the time a frame is
  // displayed, which should be optimal in most cases.
  if (output_queue_->FreeBuffersCount() == 0) {
    constexpr int64_t kRescheduleDelayMs = 17;

    decoder_thread_.task_runner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&V4L2SliceVideoDecodeAccelerator::DecodeBufferTask,
                       base::Unretained(this)),
        base::Milliseconds(kRescheduleDelayMs));
  }
}

scoped_refptr<V4L2DecodeSurface>
V4L2SliceVideoDecodeAccelerator::CreateSurface() {
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kDecoding);
  TRACE_COUNTER_ID2(
      "media,gpu", "V4L2 input buffers", this, "free",
      input_queue_->FreeBuffersCount(), "in use",
      input_queue_->AllocatedBuffersCount() - input_queue_->FreeBuffersCount());
  TRACE_COUNTER_ID2("media,gpu", "V4L2 output buffers", this, "free",
                    output_queue_->FreeBuffersCount(), "in use",
                    output_queue_->AllocatedBuffersCount() -
                        output_queue_->AllocatedBuffersCount());
  TRACE_COUNTER_ID2("media,gpu", "V4L2 output buffers", this, "at client",
                    GetNumOfOutputRecordsAtClient(), "at device",
                    GetNumOfOutputRecordsAtDevice());

  // Release some output buffers if their fence has been signaled.
  CheckGLFences();

  auto input_buffer = input_queue_->GetFreeBuffer();
  // All buffers that are returned to the output free queue have their GL
  // fence signaled, so we can use them directly.
  auto output_buffer = output_queue_->GetFreeBuffer();
  if (!input_buffer || !output_buffer)
    return nullptr;

  const int input = input_buffer->BufferId();
  const int output = output_buffer->BufferId();

  const size_t index = output_buffer->BufferId();
  OutputRecord& output_record = output_buffer_map_[index];
  DCHECK_NE(output_record.picture_id, -1);

  // Get a free request from the queue for a new surface.
  absl::optional<V4L2RequestRef> request_ref =
      requests_queue_->GetFreeRequest();
  if (!request_ref) {
    LOG(ERROR) << "Failed getting a request";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return nullptr;
  }

  DVLOGF(4) << __func__ << " " << input << " -> " << output;
  return new V4L2RequestDecodeSurface(
      std::move(*input_buffer), std::move(*output_buffer),
      output_record.output_frame, std::move(*request_ref));
}

void V4L2SliceVideoDecodeAccelerator::SendPictureReady() {
  DVLOGF(4);
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  bool send_now =
      (decoder_resetting_ || decoder_flushing_ || surface_set_change_pending_);
  while (!pending_picture_ready_.empty()) {
    bool cleared = pending_picture_ready_.front().cleared;
    const Picture& picture = pending_picture_ready_.front().picture;
    if (cleared && picture_clearing_count_ == 0) {
      DVLOGF(4) << "Posting picture ready to decode task runner for: "
                << picture.picture_buffer_id();
      // This picture is cleared. It can be posted to a thread different than
      // the main GPU thread to reduce latency. This should be the case after
      // all pictures are cleared at the beginning.
      decode_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&Client::PictureReady, decode_client_, picture));
      pending_picture_ready_.pop();
    } else if (!cleared || send_now) {
      DVLOGF(4) << "cleared=" << pending_picture_ready_.front().cleared
                << ", decoder_resetting_=" << decoder_resetting_
                << ", decoder_flushing_=" << decoder_flushing_
                << ", surface_set_change_pending_="
                << surface_set_change_pending_
                << ", picture_clearing_count_=" << picture_clearing_count_;
      DVLOGF(4) << "Posting picture ready to GPU for: "
                << picture.picture_buffer_id();
      // If the picture is not cleared, post it to the child thread because it
      // has to be cleared in the child thread. A picture only needs to be
      // cleared once. If the decoder is resetting or flushing or changing
      // resolution, send all pictures to ensure PictureReady arrive before
      // reset done, flush done, or picture dismissed.
      child_task_runner_->PostTaskAndReply(
          FROM_HERE, base::BindOnce(&Client::PictureReady, client_, picture),
          // Unretained is safe. If Client::PictureReady gets to run, |this| is
          // alive. Destroy() will wait the decode thread to finish.
          base::BindOnce(&V4L2SliceVideoDecodeAccelerator::PictureCleared,
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

void V4L2SliceVideoDecodeAccelerator::PictureCleared() {
  DVLOGF(4) << "clearing count=" << picture_clearing_count_;
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK_GT(picture_clearing_count_, 0);
  picture_clearing_count_--;
  SendPictureReady();
}

bool V4L2SliceVideoDecodeAccelerator::TryToSetupDecodeOnSeparateSequence(
    const base::WeakPtr<Client>& decode_client,
    const scoped_refptr<base::SequencedTaskRunner>& decode_task_runner) {
  decode_client_ = decode_client;
  decode_task_runner_ = decode_task_runner;
  return true;
}

// static
VideoDecodeAccelerator::SupportedProfiles
V4L2SliceVideoDecodeAccelerator::GetSupportedProfiles() {
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (!device)
    return SupportedProfiles();

  return device->GetSupportedDecodeProfiles(kSupportedInputFourCCs);
}

bool V4L2SliceVideoDecodeAccelerator::IsSupportedProfile(
    VideoCodecProfile profile) {
  DCHECK(device_);
  if (supported_profiles_.empty()) {
    SupportedProfiles profiles = GetSupportedProfiles();
    for (const SupportedProfile& entry : profiles)
      supported_profiles_.push_back(entry.profile);
  }
  return base::Contains(supported_profiles_, profile);
}

size_t V4L2SliceVideoDecodeAccelerator::GetNumOfOutputRecordsAtDevice() const {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  return output_queue_->QueuedBuffersCount();
}

size_t V4L2SliceVideoDecodeAccelerator::GetNumOfOutputRecordsAtClient() const {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  return base::ranges::count_if(output_buffer_map_, &OutputRecord::at_client);
}

void V4L2SliceVideoDecodeAccelerator::ImageProcessorError() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  LOG(ERROR) << "Image processor error";
  NOTIFY_ERROR(PLATFORM_FAILURE);
}

bool V4L2SliceVideoDecodeAccelerator::ProcessFrame(
    V4L2ReadableBufferRef buffer,
    scoped_refptr<V4L2DecodeSurface> surface) {
  DVLOGF(4);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  scoped_refptr<VideoFrame> input_frame = buffer->GetVideoFrame();
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
    VLOGF(1) << "The visible rectangle is invalid!";
    return false;
  }
  if (!gfx::Rect(input_frame->natural_size())
           .Contains(gfx::Rect(natural_size))) {
    VLOGF(1) << "The natural size is too large!";
    return false;
  }
  scoped_refptr<VideoFrame> cropped_input_frame = VideoFrame::WrapVideoFrame(
      input_frame, input_frame->format(), visible_rect, natural_size);
  if (!cropped_input_frame) {
    VLOGF(1) << "Could not wrap the input frame for the image processor!";
    return false;
  }

  if (image_processor_->output_mode() == ImageProcessor::OutputMode::IMPORT) {
    // In IMPORT mode we can decide ourselves which IP buffer to use, so choose
    // the one with the same index number as our decoded buffer.
    const OutputRecord& output_record = output_buffer_map_[buffer->BufferId()];
    scoped_refptr<VideoFrame> output_frame = output_record.output_frame;

    // We will set a destruction observer to the output frame, so wrap the
    // imported frame into another one that we can destruct.
    scoped_refptr<VideoFrame> wrapped_frame = VideoFrame::WrapVideoFrame(
        output_frame, output_frame->format(), output_frame->visible_rect(),
        output_frame->visible_rect().size());
    DCHECK(wrapped_frame);

    image_processor_->Process(
        std::move(cropped_input_frame), std::move(wrapped_frame),
        base::BindOnce(&V4L2SliceVideoDecodeAccelerator::FrameProcessed,
                       base::Unretained(this), surface, buffer->BufferId()));
  } else {
    // In ALLOCATE mode we cannot choose which IP buffer to use. We will get
    // the surprise when FrameProcessed() is invoked...
    if (!image_processor_->Process(
            std::move(cropped_input_frame),
            base::BindOnce(&V4L2SliceVideoDecodeAccelerator::FrameProcessed,
                           base::Unretained(this), surface)))
      return false;
  }

  surfaces_at_ip_.push(std::make_pair(std::move(surface), std::move(buffer)));

  return true;
}

void V4L2SliceVideoDecodeAccelerator::FrameProcessed(
    scoped_refptr<V4L2DecodeSurface> surface,
    size_t ip_buffer_index,
    scoped_refptr<VideoFrame> frame) {
  DVLOGF(4);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  if (IsDestroyPending())
    return;

  // TODO(crbug.com/921825): Remove this workaround once reset callback is
  // implemented.
  if (surfaces_at_ip_.empty() || surfaces_at_ip_.front().first != surface ||
      output_buffer_map_.empty()) {
    // This can happen if image processor is reset.
    // V4L2SliceVideoDecodeAccelerator::Reset() makes
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
    DVLOGF(4) << "Ignore processed frame after reset";
    return;
  }

  DCHECK_LT(ip_buffer_index, output_buffer_map_.size());
  OutputRecord& ip_output_record = output_buffer_map_[ip_buffer_index];

  // If the picture has not been cleared yet, this means it is the first time
  // we are seeing this buffer from the image processor. Schedule a call to
  // CreateGLImageFor before the picture is sent to the client. It is
  // guaranteed that CreateGLImageFor will complete before the picture is sent
  // to the client as both events happen on the child thread due to the picture
  // uncleared status.
  if (ip_output_record.texture_id != 0 && !ip_output_record.cleared) {
    DCHECK(frame->HasDmaBufs());
    child_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &V4L2SliceVideoDecodeAccelerator::CreateGLImageFor, weak_this_,
            image_processor_device_, ip_buffer_index,
            ip_output_record.picture_id,
            CreateGpuMemoryBufferHandle(frame.get()).native_pixmap_handle,
            ip_output_record.client_texture_id, ip_output_record.texture_id,
            GetRectSizeFromOrigin(visible_rect_), *gl_image_format_fourcc_));
  }

  DCHECK(!surfaces_at_ip_.empty());
  DCHECK_EQ(surfaces_at_ip_.front().first, surface);
  V4L2ReadableBufferRef decoded_buffer =
      std::move(surfaces_at_ip_.front().second);
  surfaces_at_ip_.pop();
  DCHECK_EQ(decoded_buffer->BufferId(),
            static_cast<size_t>(surface->output_record()));

  // Keep the decoder buffer until the IP frame is itself released.
  // We need to keep this V4L2 frame because the decode surface still references
  // its index and we will use its OutputRecord to reference the IP buffer.
  frame->AddDestructionObserver(
      base::BindOnce(&V4L2SliceVideoDecodeAccelerator::ReuseOutputBuffer,
                     base::Unretained(this), decoded_buffer));

  // This holds the IP video frame until everyone is done with it
  surface->SetReleaseCallback(base::DoNothingWithBoundArgs(frame));
  DCHECK_EQ(decoded_buffer_map_.count(decoded_buffer->BufferId()), 0u);
  decoded_buffer_map_.emplace(decoded_buffer->BufferId(), ip_buffer_index);
  surface->SetDecoded();

  TryOutputSurfaces();
  ProcessPendingEventsIfNeeded();
  ScheduleDecodeBufferTaskIfNeeded();
}

// base::trace_event::MemoryDumpProvider implementation.
bool V4L2SliceVideoDecodeAccelerator::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  // OnMemoryDump() must be performed on |decoder_thread_|.
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  // VIDEO_OUTPUT queue's memory usage.
  const size_t input_queue_buffers_count =
      input_queue_->AllocatedBuffersCount();
  size_t input_queue_memory_usage = 0;
  std::string input_queue_buffers_memory_type =
      V4L2MemoryToString(input_queue_->GetMemoryType());
  input_queue_memory_usage += input_queue_->GetMemoryUsage();

  // VIDEO_CAPTURE queue's memory usage.
  const size_t output_queue_buffers_count = output_buffer_map_.size();
  size_t output_queue_memory_usage = 0;
  std::string output_queue_buffers_memory_type =
      V4L2MemoryToString(output_queue_->GetMemoryType());
  if (output_mode_ == Config::OutputMode::ALLOCATE) {
    // Call QUERY_BUF here because the length of buffers on VIDIOC_CATURE queue
    // are not recorded nowhere in V4L2VideoDecodeAccelerator.
    for (uint32_t index = 0; index < output_buffer_map_.size(); ++index) {
      struct v4l2_buffer v4l2_buffer;
      memset(&v4l2_buffer, 0, sizeof(v4l2_buffer));
      struct v4l2_plane v4l2_planes[VIDEO_MAX_PLANES];
      memset(v4l2_planes, 0, sizeof(v4l2_planes));
      DCHECK_LT(output_planes_count_, std::size(v4l2_planes));
      v4l2_buffer.m.planes = v4l2_planes;
      v4l2_buffer.length =
          std::min(output_planes_count_, std::size(v4l2_planes));
      v4l2_buffer.index = index;
      v4l2_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
      v4l2_buffer.memory = V4L2_MEMORY_MMAP;
      IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QUERYBUF, &v4l2_buffer);
      for (size_t i = 0; i < output_planes_count_; ++i)
        output_queue_memory_usage += v4l2_buffer.m.planes[i].length;
    }
  }

  const size_t total_usage =
      input_queue_memory_usage + output_queue_memory_usage;

  using ::base::trace_event::MemoryAllocatorDump;

  auto dump_name = base::StringPrintf("gpu/v4l2/slice_decoder/0x%" PRIxPTR,
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
