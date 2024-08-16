// Copyright 2014 The Chromium Authors
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

#include <memory>
#include <numeric>
#include <optional>
#include <utility>

#include "base/bits.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/color_plane_layout.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_types.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/image_processor_factory.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/gpu_video_encode_accelerator_helpers.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_utils.h"
#include "media/parsers/h264_level_limits.h"
#include "media/parsers/h264_parser.h"

#ifndef V4L2_CID_MPEG_VIDEO_H264_HIER_CODING_L0_BR
#define V4L2_CID_MPEG_VIDEO_H264_HIER_CODING_L0_BR (V4L2_CID_CODEC_BASE + 391)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_H264_HIER_CODING_L1_BR
#define V4L2_CID_MPEG_VIDEO_H264_HIER_CODING_L1_BR (V4L2_CID_CODEC_BASE + 392)
#endif

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

namespace {
// Convert VideoFrameLayout to ImageProcessor::PortConfig.
std::optional<ImageProcessor::PortConfig> VideoFrameLayoutToPortConfig(
    const VideoFrameLayout& layout,
    const gfx::Rect& visible_rect,
    VideoFrame::StorageType storage_type) {
  auto fourcc =
      Fourcc::FromVideoPixelFormat(layout.format(), !layout.is_multi_planar());
  if (!fourcc) {
    DVLOGF(1) << "Failed to create Fourcc from video pixel format "
              << VideoPixelFormatToString(layout.format());
    return std::nullopt;
  }
  return ImageProcessor::PortConfig(*fourcc, layout.coded_size(),
                                    layout.planes(), visible_rect,
                                    storage_type);
}

// Create Layout from |layout| with is_multi_planar = true.
std::optional<VideoFrameLayout> AsMultiPlanarLayout(
    const VideoFrameLayout& layout) {
  if (layout.is_multi_planar())
    return std::make_optional<VideoFrameLayout>(layout);
  return VideoFrameLayout::CreateMultiPlanar(
      layout.format(), layout.coded_size(), layout.planes());
}

scoped_refptr<base::SequencedTaskRunner> CreateEncoderTaskRunner() {
  if (base::FeatureList::IsEnabled(kUSeSequencedTaskRunnerForVEA)) {
    return base::ThreadPool::CreateSequencedTaskRunner(
        {base::WithBaseSyncPrimitives(), base::TaskPriority::USER_VISIBLE,
         base::MayBlock()});
  } else {
    return base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::WithBaseSyncPrimitives(), base::MayBlock(),
         base::TaskPriority::USER_VISIBLE},
        base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  }
}
}  // namespace

struct V4L2VideoEncodeAccelerator::BitstreamBufferRef {
  BitstreamBufferRef(int32_t id, base::WritableSharedMemoryMapping shm_mapping)
      : id(id), shm_mapping(std::move(shm_mapping)) {}
  const int32_t id;
  base::WritableSharedMemoryMapping shm_mapping;
};

V4L2VideoEncodeAccelerator::InputRecord::InputRecord() = default;

V4L2VideoEncodeAccelerator::InputRecord::InputRecord(const InputRecord&) =
    default;

V4L2VideoEncodeAccelerator::InputRecord::~InputRecord() = default;

V4L2VideoEncodeAccelerator::InputFrameInfo::InputFrameInfo()
    : InputFrameInfo(nullptr, false) {}

V4L2VideoEncodeAccelerator::InputFrameInfo::InputFrameInfo(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe)
    : frame(frame), force_keyframe(force_keyframe) {}

V4L2VideoEncodeAccelerator::InputFrameInfo::InputFrameInfo(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe,
    size_t index)
    : frame(std::move(frame)),
      force_keyframe(force_keyframe),
      ip_output_buffer_index(index) {}

V4L2VideoEncodeAccelerator::InputFrameInfo::InputFrameInfo(
    const InputFrameInfo&) = default;

V4L2VideoEncodeAccelerator::InputFrameInfo::~InputFrameInfo() = default;

// static
base::AtomicRefCount V4L2VideoEncodeAccelerator::num_instances_(0);

V4L2VideoEncodeAccelerator::V4L2VideoEncodeAccelerator(
    scoped_refptr<V4L2Device> device)
    : can_use_encoder_(num_instances_.Increment() < kMaxNumOfInstances),
      child_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      native_input_mode_(false),
      output_buffer_byte_size_(0),
      output_format_fourcc_(0),
      current_framerate_(0),
      encoder_state_(kUninitialized),
      device_(std::move(device)),
      input_memory_type_(V4L2_MEMORY_USERPTR),
      is_flush_supported_(false),
      // TODO(akahuang): Remove WithBaseSyncPrimitives() after replacing poll
      // thread by V4L2DevicePoller.
      encoder_task_runner_(CreateEncoderTaskRunner()),
      device_poll_thread_("V4L2EncoderDevicePollThread") {
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);
  DETACH_FROM_SEQUENCE(encoder_sequence_checker_);

  weak_this_ = weak_this_factory_.GetWeakPtr();
}

V4L2VideoEncodeAccelerator::~V4L2VideoEncodeAccelerator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK(!device_poll_thread_.IsRunning());
  VLOGF(2);

  num_instances_.Decrement();
}

bool V4L2VideoEncodeAccelerator::Initialize(
    const Config& config,
    Client* client,
    std::unique_ptr<MediaLog> media_log) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);
  DCHECK_EQ(encoder_state_, kUninitialized);

  TRACE_EVENT0("media,gpu", "V4L2VEA::Initialize");
  VLOGF(2) << ": " << config.AsHumanReadableString();

  if (!can_use_encoder_) {
    MEDIA_LOG(ERROR, media_log.get()) << "Too many encoders are allocated";
    return false;
  }

  if (config.HasSpatialLayer()) {
    MEDIA_LOG(ERROR, media_log.get())
        << "Spatial layer encoding is not yet supported";
    return false;
  }

  // Currently only Qualcomm (SC7180) supports temporal layers, MTK drivers
  // do not. There is no check here to determine if the driver supports temporal
  // layering. It is expected that clients will first call
  // GetSupportedScalabilityModesForV4L2Codec() to get the capabilities.
  if (config.HasTemporalLayer()) {
    if (VideoCodecProfileToVideoCodec(config.output_profile) ==
        VideoCodec::kH264) {
      constexpr uint8_t kNumSupportedH264TemporalLayers = 2;
      const uint8_t num_temporal_layers =
          config.spatial_layers[0].num_of_temporal_layers;

      if (num_temporal_layers == kNumSupportedH264TemporalLayers) {
        h264_l1t2_enabled_ = true;
      } else {
        MEDIA_LOG(ERROR, media_log.get())
            << "Unsupported number of temporal layers: "
            << base::strict_cast<size_t>(num_temporal_layers);
        return false;
      }
    } else {
      MEDIA_LOG(WARNING, media_log.get())
          << GetProfileName(config.output_profile)
          << " does not support temporal scalability. L1T1 will be produced.";
    }
  }

  encoder_input_visible_rect_ = gfx::Rect(config.input_visible_size);

  client_ptr_factory_ = std::make_unique<base::WeakPtrFactory<Client>>(client);
  client_ = client_ptr_factory_->GetWeakPtr();

  output_format_fourcc_ =
      VideoCodecProfileToV4L2PixFmt(config.output_profile, false);
  if (output_format_fourcc_ == V4L2_PIX_FMT_INVALID) {
    MEDIA_LOG(ERROR, media_log.get())
        << "invalid output_profile=" << GetProfileName(config.output_profile);
    return false;
  }

  if (!device_->Open(V4L2Device::Type::kEncoder, output_format_fourcc_)) {
    MEDIA_LOG(ERROR, media_log.get())
        << "Failed to open device for profile="
        << GetProfileName(config.output_profile)
        << ", fourcc=" << FourccToString(output_format_fourcc_);
    return false;
  }

  gfx::Size min_resolution;
  gfx::Size max_resolution;
  GetSupportedResolution(base::BindRepeating(&V4L2Device::Ioctl, device_),
                         output_format_fourcc_, &min_resolution,
                         &max_resolution);
  if (config.input_visible_size.width() < min_resolution.width() ||
      config.input_visible_size.height() < min_resolution.height() ||
      config.input_visible_size.width() > max_resolution.width() ||
      config.input_visible_size.height() > max_resolution.height()) {
    MEDIA_LOG(ERROR, media_log.get())
        << "Unsupported resolution: " << config.input_visible_size.ToString()
        << ", min=" << min_resolution.ToString()
        << ", max=" << max_resolution.ToString();
    return false;
  }

  // Ask if V4L2_ENC_CMD_STOP (Flush) is supported.
  struct v4l2_encoder_cmd cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.cmd = V4L2_ENC_CMD_STOP;
  is_flush_supported_ = (device_->Ioctl(VIDIOC_TRY_ENCODER_CMD, &cmd) == 0);
  if (!is_flush_supported_)
    VLOGF(2) << "V4L2_ENC_CMD_STOP is not supported.";

  struct v4l2_capability caps;
  memset(&caps, 0, sizeof(caps));
  const __u32 kCapsRequired = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
  if (device_->Ioctl(VIDIOC_QUERYCAP, &caps) != 0) {
    MEDIA_LOG(ERROR, media_log.get())
        << "ioctl() failed: VIDIOC_QUERYCAP, errno=" << errno;
    return false;
  }

  if ((caps.capabilities & kCapsRequired) != kCapsRequired) {
    MEDIA_LOG(ERROR, media_log.get())
        << "caps check failed: 0x" << std::hex << caps.capabilities;
    return false;
  }

  driver_name_ = device_->GetDriverName();

  encoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V4L2VideoEncodeAccelerator::InitializeTask,
                                weak_this_, config));
  return true;
}

void V4L2VideoEncodeAccelerator::InitializeTask(const Config& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  TRACE_EVENT0("media,gpu", "V4L2VEA::InitializeTask");

  // Set kInitialized here so that NotifyErrorStatus() is invoked from here.
  encoder_state_ = kInitialized;

  native_input_mode_ =
      config.storage_type == Config::StorageType::kGpuMemoryBuffer;

  input_queue_ = device_->GetQueue(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
  output_queue_ = device_->GetQueue(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
  if (!input_queue_ || !output_queue_) {
    SetErrorState({EncoderStatus::Codes::kEncoderInitializationError,
                   "Failed to get V4L2Queue"});
    return;
  }

  if (!SetFormats(config.input_format, config.output_profile)) {
    return;
  }

  if (config.input_format != device_input_layout_->format()) {
    VLOGF(2) << "Input format: " << config.input_format << " is not supported "
             << "by the HW. Will try to convert to "
             << device_input_layout_->format();
    auto input_layout = VideoFrameLayout::CreateMultiPlanar(
        config.input_format, encoder_input_visible_rect_.size(),
        std::vector<ColorPlaneLayout>(
            VideoFrame::NumPlanes(config.input_format)));
    if (!input_layout) {
      SetErrorState({EncoderStatus::Codes::kUnsupportedFrameFormat,
                     "Invalid image processor input layout"});
      return;
    }

    // ImageProcessor for a pixel format conversion.
    if (!CreateImageProcessor(*input_layout, device_input_layout_->format(),
                              device_input_layout_->coded_size(),
                              encoder_input_visible_rect_,
                              encoder_input_visible_rect_)) {
      SetErrorState(
          {EncoderStatus::Codes::kEncoderInitializationError,
           base::StrCat(
               {"Failed to create image processor ", "for format conversion",
                VideoPixelFormatToString(input_layout->format()), " -> ",
                VideoPixelFormatToString(device_input_layout_->format())})});
      return;
    }

    const gfx::Size ip_output_buffer_size(
        image_processor_->output_config().planes[0].stride,
        image_processor_->output_config().size.height());
    if (!NegotiateInputFormat(device_input_layout_->format(),
                              ip_output_buffer_size)) {
      SetErrorState(
          {EncoderStatus::Codes::kUnsupportedFrameFormat,
           base::StrCat({"Failed to reconfigure v4l2 encoder driver with the ",
                         "ImageProcessor output buffer: ",
                         ip_output_buffer_size.ToString()})});
      return;
    }
  }

  if (!InitInputMemoryType(config) || !InitControls(config) ||
      !CreateOutputBuffers()) {
    SetErrorState(EncoderStatus::Codes::kEncoderInitializationError);
    return;
  }

  if (config.bitrate.mode() != Bitrate::Mode::kConstant &&
      config.bitrate.mode() != Bitrate::Mode::kVariable) {
    SetErrorState({EncoderStatus::Codes::kEncoderUnsupportedConfig,
                   base::StrCat({"Invalid bitrate mode: ",
                                 base::NumberToString(base::strict_cast<int>(
                                     config.bitrate.mode()))})});
    return;
  }

  if (config.bitrate.mode() == Bitrate::Mode::kVariable &&
      !base::FeatureList::IsEnabled(kChromeOSHWVBREncoding)) {
    SetErrorState({EncoderStatus::Codes::kEncoderUnsupportedConfig,
                   "VBR encoding is disabled"});
    return;
  }

  const uint32_t bitrate_mode =
      config.bitrate.mode() == Bitrate::Mode::kConstant
          ? V4L2_MPEG_VIDEO_BITRATE_MODE_CBR
          : V4L2_MPEG_VIDEO_BITRATE_MODE_VBR;
  const VideoBitrateAllocation bitrate_allocation =
      AllocateBitrateForDefaultEncoding(config);

  if (!device_->SetExtCtrls(
          V4L2_CID_MPEG_CLASS,
          {V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_BITRATE_MODE, bitrate_mode)})) {
    SetErrorState({EncoderStatus::Codes::kEncoderHardwareDriverError,
                   base::StrCat({"Failed to configure bitrate mode: ",
                                 base::NumberToString(base::strict_cast<int>(
                                     bitrate_allocation.GetMode()))})});
    return;
  }

  current_bitrate_allocation_ = VideoBitrateAllocation(config.bitrate.mode());

  RequestEncodingParametersChangeTask(bitrate_allocation, config.framerate,
                                      std::nullopt);

  // input_frame_size_ is the size of input_config of |image_processor_|.
  // On native_input_mode_, since the passed size in RequireBitstreamBuffers()
  // is ignored by the client, we don't update the expected frame size.
  if (!native_input_mode_ && image_processor_.get())
    input_frame_size_ = image_processor_->input_config().size;

  child_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::RequireBitstreamBuffers, client_,
                                kInputBufferCount, input_frame_size_,
                                output_buffer_byte_size_));

  // Notify VideoEncoderInfo after initialization.
  VideoEncoderInfo encoder_info;
  encoder_info.implementation_name = "V4L2VideoEncodeAccelerator";
  DCHECK(!encoder_info.has_trusted_rate_controller);
  DCHECK(encoder_info.is_hardware_accelerated);
  DCHECK(encoder_info.supports_native_handle);
  DCHECK(!encoder_info.supports_simulcast);

  // V4L2VideoEncodeAccelerator only supports temporal-SVC.
  if (config.HasTemporalLayer()) {
    CHECK(!config.spatial_layers.empty());
    for (size_t i = 0; i < config.spatial_layers.size(); ++i) {
      encoder_info.fps_allocation[i] =
          GetFpsAllocation(config.spatial_layers[i].num_of_temporal_layers);
    }
  } else {
    constexpr uint8_t kFullFramerate = 255;
    encoder_info.fps_allocation[0] = {kFullFramerate};
  }

  child_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Client::NotifyEncoderInfoChange, client_, encoder_info));
}

bool V4L2VideoEncodeAccelerator::CreateImageProcessor(
    const VideoFrameLayout& input_layout,
    const VideoPixelFormat output_format,
    const gfx::Size& output_size,
    const gfx::Rect& input_visible_rect,
    const gfx::Rect& output_visible_rect) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  auto ip_input_layout = AsMultiPlanarLayout(input_layout);
  if (!ip_input_layout) {
    LOG(ERROR) << "Failed to get multi-planar input layout, input_layout="
               << input_layout;
    return false;
  }

  VideoFrame::StorageType input_storage_type =
      native_input_mode_ ? VideoFrame::STORAGE_GPU_MEMORY_BUFFER
                         : VideoFrame::STORAGE_SHMEM;
  auto input_config = VideoFrameLayoutToPortConfig(
      *ip_input_layout, input_visible_rect, input_storage_type);
  if (!input_config) {
    LOG(ERROR) << "Failed to create ImageProcessor input config";
    return false;
  }

  auto platform_layout = GetPlatformVideoFrameLayout(
      output_format, output_size,
      gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE);
  if (!platform_layout) {
    LOG(ERROR) << "Failed to get Platform VideoFrameLayout";
    return false;
  }
  auto output_layout = AsMultiPlanarLayout(platform_layout.value());
  if (!output_layout) {
    LOG(ERROR) << "Failed to get multi-planar platform layout, platform_layout="
               << *platform_layout;
    return false;
  }
  auto output_config =
      VideoFrameLayoutToPortConfig(*output_layout, output_visible_rect,
                                   VideoFrame::STORAGE_GPU_MEMORY_BUFFER);
  if (!output_config) {
    LOG(ERROR) << "Failed to create ImageProcessor output config";
    return false;
  }
  image_processor_ = ImageProcessorFactory::Create(
      *input_config, *output_config, kImageProcBufferCount,
      base::BindRepeating(&V4L2VideoEncodeAccelerator::ImageProcessorError,
                          weak_this_),
      encoder_task_runner_);

  if (!image_processor_) {
    LOG(ERROR) << "Failed initializing image processor";
    return false;
  }
  VLOGF(2) << "ImageProcessor is created: " << image_processor_->backend_type();
  num_frames_in_image_processor_ = 0;

  // The output of image processor is the input of encoder. Output coded
  // width of processor must be the same as input coded width of encoder.
  // Output coded height of processor can be larger but not smaller than the
  // input coded height of encoder. For example, suppose input size of encoder
  // is 320x193. It is OK if the output of processor is 320x208.
  const auto& ip_output_size = image_processor_->output_config().size;
  if (ip_output_size.width() != output_layout->coded_size().width() ||
      ip_output_size.height() < output_layout->coded_size().height()) {
    LOG(ERROR) << "Invalid image processor output coded size "
               << ip_output_size.ToString()
               << ", expected output coded size is "
               << output_layout->coded_size().ToString();
    return false;
  }

  // Initialize |free_image_processor_output_buffer_indices_|.
  free_image_processor_output_buffer_indices_.resize(kImageProcBufferCount);
  std::iota(free_image_processor_output_buffer_indices_.begin(),
            free_image_processor_output_buffer_indices_.end(), 0);
  return AllocateImageProcessorOutputBuffers(kImageProcBufferCount);
}

bool V4L2VideoEncodeAccelerator::AllocateImageProcessorOutputBuffers(
    size_t count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK(image_processor_);
  DCHECK_EQ(image_processor_->output_mode(),
            ImageProcessor::OutputMode::IMPORT);

  // The existing buffers in |image_processor_output_buffers_| may be alive
  // until they are actually consumed by the encoder driver, after they are
  // destroyed here.
  image_processor_output_buffers_.clear();
  image_processor_output_buffers_.resize(count);
  const ImageProcessor::PortConfig& output_config =
      image_processor_->output_config();
  for (size_t i = 0; i < count; i++) {
    switch (output_config.storage_type) {
      case VideoFrame::STORAGE_GPU_MEMORY_BUFFER:
        image_processor_output_buffers_[i] = CreateGpuMemoryBufferVideoFrame(
            output_config.fourcc.ToVideoPixelFormat(), output_config.size,
            output_config.visible_rect, output_config.visible_rect.size(),
            base::TimeDelta(),
            gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE);
        break;
      default:
        LOG(ERROR) << "Unsupported output storage type of image processor: "
                   << output_config.storage_type;
        return false;
    }
    if (!image_processor_output_buffers_[i]) {
      LOG(ERROR) << "Failed to create VideoFrame";
      return false;
    }
  }
  return true;
}

bool V4L2VideoEncodeAccelerator::InitInputMemoryType(const Config& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  if (image_processor_) {
    const auto storage_type = image_processor_->output_config().storage_type;
    if (storage_type == VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
      input_memory_type_ = V4L2_MEMORY_DMABUF;
    } else if (VideoFrame::IsStorageTypeMappable(storage_type)) {
      input_memory_type_ = V4L2_MEMORY_USERPTR;
    } else {
      LOG(ERROR) << "Unsupported image processor's output StorageType: "
                 << storage_type;
      return false;
    }
  } else {
    switch (config.storage_type) {
      case Config::StorageType::kShmem:
        input_memory_type_ = V4L2_MEMORY_USERPTR;
        break;
      case Config::StorageType::kGpuMemoryBuffer:
        input_memory_type_ = V4L2_MEMORY_DMABUF;
        break;
    }
  }
  return true;
}

void V4L2VideoEncodeAccelerator::ImageProcessorError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  LOG(ERROR) << "Image processor error";

  // TODO(b/276005687): Let image processor return a minute error status and
  // convert it to EncoderStatus.
  SetErrorState(EncoderStatus::Codes::kFormatConversionError);
}

void V4L2VideoEncodeAccelerator::Encode(scoped_refptr<VideoFrame> frame,
                                        bool force_keyframe) {
  DVLOGF(4) << "force_keyframe=" << force_keyframe;
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);

  encoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V4L2VideoEncodeAccelerator::EncodeTask,
                                weak_this_, std::move(frame), force_keyframe));
}

void V4L2VideoEncodeAccelerator::UseOutputBitstreamBuffer(
    BitstreamBuffer buffer) {
  DVLOGF(4) << "id=" << buffer.id();
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);

  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2VideoEncodeAccelerator::UseOutputBitstreamBufferTask,
                     weak_this_, std::move(buffer)));
}

void V4L2VideoEncodeAccelerator::RequestEncodingParametersChange(
    const Bitrate& bitrate,
    uint32_t framerate,
    const std::optional<gfx::Size>& size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);

  VideoBitrateAllocation allocation(bitrate.mode());
  allocation.SetBitrate(0u, 0u, bitrate.target_bps());
  allocation.SetPeakBps(bitrate.peak_bps());

  RequestEncodingParametersChange(allocation, framerate, size);
}

void V4L2VideoEncodeAccelerator::RequestEncodingParametersChange(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate,
    const std::optional<gfx::Size>& size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);

  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &V4L2VideoEncodeAccelerator::RequestEncodingParametersChangeTask,
          weak_this_, bitrate_allocation, framerate, size));
}

void V4L2VideoEncodeAccelerator::Destroy() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);

  // We're destroying; cancel all callbacks.
  client_ptr_factory_.reset();

  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2VideoEncodeAccelerator::DestroyTask, weak_this_));
}

void V4L2VideoEncodeAccelerator::Flush(FlushCallback flush_callback) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);

  encoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V4L2VideoEncodeAccelerator::FlushTask,
                                weak_this_, std::move(flush_callback)));
}

void V4L2VideoEncodeAccelerator::FlushTask(FlushCallback flush_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  if (encoder_state_ == kInitialized) {
    // Flush() is called before either Encode() or UseOutputBitstreamBuffer() is
    // called. Just return as successful.
    child_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(flush_callback), true));
    return;
  }

  if (flush_callback_ || encoder_state_ != kEncoding) {
    SetErrorState({EncoderStatus::Codes::kEncoderIllegalState,
                   "Flush failed: there is a pending flush, or "
                   "VideoEncodeAccelerator is not in kEncoding state"});
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
  auto device = base::MakeRefCounted<V4L2Device>();
  return device->GetSupportedEncodeProfiles();
}

void V4L2VideoEncodeAccelerator::FrameProcessed(
    bool force_keyframe,
    base::TimeDelta timestamp,
    size_t output_buffer_index,
    scoped_refptr<VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DVLOGF(4) << "force_keyframe=" << force_keyframe
            << ", output_buffer_index=" << output_buffer_index;
  DCHECK_GE(output_buffer_index, 0u);
  TRACE_EVENT_NESTABLE_ASYNC_END2(
      "media,gpu", "V4L2VEA::ImageProcessor::Process",
      timestamp.InMicroseconds(), "timestamp", timestamp.InMicroseconds(),
      "output_size", image_processor_->output_config().size.ToString());

  encoder_input_queue_.emplace(std::move(frame), force_keyframe,
                               output_buffer_index);
  CHECK_GT(num_frames_in_image_processor_, 0u);
  num_frames_in_image_processor_--;
  MaybeFlushImageProcessor();

  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2VideoEncodeAccelerator::Enqueue, weak_this_));
}

void V4L2VideoEncodeAccelerator::ReuseImageProcessorOutputBuffer(
    size_t output_buffer_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DVLOGF(4) << "output_buffer_index=" << output_buffer_index;

  free_image_processor_output_buffer_indices_.push_back(output_buffer_index);
  InputImageProcessorTask();
}

BitstreamBufferMetadata V4L2VideoEncodeAccelerator::GetMetadata(
    const uint8_t* data,
    size_t data_size_bytes,
    bool key_frame,
    base::TimeDelta timestamp) {
  auto buffer_metadata =
      BitstreamBufferMetadata(data_size_bytes, key_frame, timestamp);

  if (h264_l1t2_enabled_) {
    H264Metadata h264_metadata;
    h264_metadata.temporal_idx = 0;
    h264_metadata.layer_sync = false;
    H264Parser parser;

    parser.SetStream(data, data_size_bytes);
    H264NALU nalu;
    while (parser.AdvanceToNextNALU(&nalu) == H264Parser::kOk) {
      // L1T2 describes a bitstream where every other frame is not used as a
      // reference frame. This allows those non reference frames to be discarded
      // and never decoded. The V4L2 api does not provide a way to request that
      // frames are not used as reference frames. It does provide a set of
      // controls (V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_*) that allows
      // specifying that.
      // In order to determine if a frame can be dropped the frames can be
      // queried to see if they are marked as reference frames
      // (i.e. nal_ref_idc == 0).
      if (nalu.nal_unit_type == H264NALU::kNonIDRSlice &&
          nalu.nal_ref_idc == 0) {
        h264_metadata.temporal_idx = 1;
        h264_metadata.layer_sync = true;
        break;
      }
    }

    buffer_metadata.h264 = h264_metadata;
  }

  return buffer_metadata;
}

size_t V4L2VideoEncodeAccelerator::CopyIntoOutputBuffer(
    const uint8_t* bitstream_data,
    size_t bitstream_size,
    std::unique_ptr<BitstreamBufferRef> buffer_ref) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  uint8_t* dst_ptr = buffer_ref->shm_mapping.GetMemoryAs<uint8_t>();
  size_t remaining_dst_size = buffer_ref->shm_mapping.size();

  if (!inject_sps_and_pps_) {
    if (bitstream_size <= remaining_dst_size) {
      memcpy(dst_ptr, bitstream_data, bitstream_size);
      return bitstream_size;
    } else {
      SetErrorState({EncoderStatus::Codes::kEncoderFailedEncode,
                     "Output data did not fit in the BitstreamBuffer"});
      return 0;
    }
  }

  // Cache the newest SPS and PPS found in the stream, and inject them before
  // each IDR found.
  H264Parser parser;
  parser.SetStream(bitstream_data, bitstream_size);
  H264NALU nalu;

  bool inserted_sps = false;
  bool inserted_pps = false;
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
        inserted_sps = true;
        break;

      case H264NALU::kPPS:
        cached_pps_.resize(nalu.size);
        memcpy(cached_pps_.data(), nalu.data, nalu.size);
        cached_h264_header_size_ =
            cached_sps_.size() + cached_pps_.size() + 2 * kH264StartCodeSize;
        inserted_pps = true;
        break;

      case H264NALU::kIDRSlice:
        if (inserted_sps && inserted_pps) {
          // Already inserted SPS and PPS. No need to inject.
          break;
        }
        // Only inject if we have both headers cached, and enough space for both
        // the headers and the NALU itself.
        if (cached_sps_.empty() || cached_pps_.empty()) {
          VLOGF(1) << "Cannot inject IDR slice without SPS and PPS";
          break;
        }
        if (cached_h264_header_size_ + nalu.size + kH264StartCodeSize >
                remaining_dst_size) {
          VLOGF(1) << "Not enough space to inject a stream header before IDR";
          break;
        }

        if (!inserted_sps) {
          CopyNALUPrependingStartCode(cached_sps_.data(), cached_sps_.size(),
                                      &dst_ptr, &remaining_dst_size);
        }
        if (!inserted_pps) {
          CopyNALUPrependingStartCode(cached_pps_.data(), cached_pps_.size(),
                                      &dst_ptr, &remaining_dst_size);
        }
        VLOGF(2) << "Stream header injected before IDR";
        break;
    }

    CopyNALUPrependingStartCode(nalu.data, nalu.size, &dst_ptr,
                                &remaining_dst_size);
  }

  return buffer_ref->shm_mapping.size() - remaining_dst_size;
}

void V4L2VideoEncodeAccelerator::EncodeTask(scoped_refptr<VideoFrame> frame,
                                            bool force_keyframe) {
  DVLOGF(4) << "force_keyframe=" << force_keyframe;
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK_NE(encoder_state_, kUninitialized);

  if (encoder_state_ == kError) {
    DVLOGF(1) << "early out: kError state";
    return;
  }

  if (frame) {
    TRACE_EVENT1("media,gpu", "V4L2VEA::EncodeTask", "timestamp",
                 frame->timestamp().InMicroseconds());
    // |frame| can be nullptr to indicate a flush.
    const bool is_expected_storage_type =
        native_input_mode_
            ? frame->storage_type() == VideoFrame::STORAGE_GPU_MEMORY_BUFFER
            : frame->IsMappable();
    if (!is_expected_storage_type) {
      SetErrorState({EncoderStatus::Codes::kInvalidInputFrame,
                     base::StrCat({"Unexpected storage: ",
                                   VideoFrame::StorageTypeToString(
                                       frame->storage_type())})});
      return;
    }

    if (!ReconfigureFormatIfNeeded(*frame)) {
      SetErrorState({EncoderStatus::Codes::kUnsupportedFrameFormat,
                     base::StrCat({"Unsupported frame: ",
                                   frame->AsHumanReadableString()})});
      return;
    }

    // If a video frame to be encoded is fed, then call VIDIOC_REQBUFS if it has
    // not been called yet.
    if (input_buffer_map_.empty() && !CreateInputBuffers()) {
      CHECK_EQ(encoder_state_, kError);
      return;
    }

    if (encoder_state_ == kInitialized) {
      if (!StartDevicePoll())
        return;
      encoder_state_ = kEncoding;
    }
  }

  if (image_processor_) {
    image_processor_input_queue_.emplace(std::move(frame), force_keyframe);
    InputImageProcessorTask();
  } else {
    encoder_input_queue_.emplace(std::move(frame), force_keyframe);
    Enqueue();
  }
}

bool V4L2VideoEncodeAccelerator::ReconfigureFormatIfNeeded(
    const VideoFrame& frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  if (input_buffer_map_.empty()) {
    // Updates |input_natural_size_| on the first VideoFrame.
    // |input_natural_size_| is a dimension to be encoded (i.e.
    // |encoder_input_visible_rect_.size()|), but can be different from it
    // in simulcast case.
    input_natural_size_ = frame.natural_size();
  }

  if (!native_input_mode_) {
    // frame.coded_size() must be the size specified in
    // RequireBitstreamBuffers() in non native-input mode.
    return frame.coded_size() == input_frame_size_;
  }

  if (!input_buffer_map_.empty()) {
    // ReconfigureFormatIfNeeded() has been called with the first VideoFrame.
    // We checks here we need to (re)create ImageProcessor because the visible
    // rectangle of |frame| differs from the first VideoFrame.
    // |frame.natural_size()| must be unchanged during encoding in the same
    // VideoEncodeAccelerator  instance. When it is changed, a client has to
    // recreate VideoEncodeAccelerator.
    if (frame.natural_size() != input_natural_size_) {
      LOG(ERROR) << "Encoder resolution is changed during encoding"
                 << ", frame.natural_size()=" << frame.natural_size().ToString()
                 << ", input_natural_size_=" << input_natural_size_.ToString();
      return false;
    }
    if (frame.coded_size() == input_frame_size_) {
      return true;
    }

    // If a dimension of the underlying VideoFrame varies during video encoding
    // (i.e. frame.coded_size() != input_frame_size_), we (re)create
    // ImageProcessor to crop the VideoFrame, |frame.visible_rect()| ->
    // |encoder_input_visible_rect_|.
    // TODO(hiroh): if |frame.coded_size()| is the same as VideoFrame::
    // DetermineAlignedSize(input_format, encoder_input_visible_rect_.size())
    // and don't need a pixel format conversion, image processor is not
    // necessary but we should rather NegotiateInputFormat().
  } else if (frame.coded_size() == input_frame_size_) {
    // This path is for the first frame on Encode().
    // Height and width that V4L2VEA needs to configure.
    const gfx::Size buffer_size(frame.stride(0), frame.coded_size().height());

    // A buffer given by client is allocated with the same dimension using
    // minigbm. However, it is possible that stride and height are different
    // from ones adjusted by a driver.
    if (!image_processor_) {
      if (device_input_layout_->coded_size().width() == buffer_size.width() &&
          device_input_layout_->coded_size().height() == buffer_size.height()) {
        return true;
      }
      return NegotiateInputFormat(device_input_layout_->format(), buffer_size)
          .has_value();
    }

    if (image_processor_->input_config().size.height() ==
            buffer_size.height() &&
        image_processor_->input_config().planes[0].stride ==
            buffer_size.width()) {
      return true;
    }
  }

  // The |frame| dimension is different from the resolution configured to
  // V4L2VEA. This is the case that V4L2VEA needs to create ImageProcessor for
  // cropping and scaling. Update |input_frame_size_| to check if succeeding
  // frames' dimensions are not different from the current one.
  input_frame_size_ = frame.coded_size();
  if (!CreateImageProcessor(frame.layout(), device_input_layout_->format(),
                            device_input_layout_->coded_size(),
                            frame.visible_rect(),
                            encoder_input_visible_rect_)) {
    LOG(ERROR) << "Failed to create image processor";
    return false;
  }

  if (gfx::Size(image_processor_->output_config().planes[0].stride,
                image_processor_->output_config().size.height()) !=
      device_input_layout_->coded_size()) {
    LOG(ERROR) << "Image Processor's output buffer's size is different from "
               << "input buffer size configure to the encoder driver. "
               << "ip's output buffer size: "
               << gfx::Size(image_processor_->output_config().planes[0].stride,
                            image_processor_->output_config().size.height())
                      .ToString()
               << ", encoder's input buffer size: "
               << device_input_layout_->coded_size().ToString();
    return false;
  }
  return true;
}

void V4L2VideoEncodeAccelerator::MaybeFlushImageProcessor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK(image_processor_);
  if (image_processor_input_queue_.size() == 1 &&
      !image_processor_input_queue_.front().frame &&
      num_frames_in_image_processor_ == 0) {
    // Flush the encoder once the image processor is done with its own flush.
    DVLOGF(3) << "All frames to be flush have been processed by "
              << "|image_processor_|. Move the flush request to the encoder";
    image_processor_input_queue_.pop();
    encoder_input_queue_.emplace(nullptr, false);
    Enqueue();
  }
}

void V4L2VideoEncodeAccelerator::InputImageProcessorTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  MaybeFlushImageProcessor();

  if (free_image_processor_output_buffer_indices_.empty())
    return;
  if (image_processor_input_queue_.empty())
    return;
  // The flush request is at the top. Waiting until all frames are processed by
  // the image processor.
  if (!image_processor_input_queue_.front().frame)
    return;

  const size_t output_buffer_index =
      free_image_processor_output_buffer_indices_.back();
  free_image_processor_output_buffer_indices_.pop_back();

  InputFrameInfo frame_info = std::move(image_processor_input_queue_.front());
  image_processor_input_queue_.pop();
  auto frame = std::move(frame_info.frame);
  const bool force_keyframe = frame_info.force_keyframe;
  auto timestamp = frame->timestamp();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "media,gpu", "V4L2VEA::ImageProcessor::Process",
      timestamp.InMicroseconds(), "timestamp", timestamp.InMicroseconds());
  auto output_frame = image_processor_output_buffers_[output_buffer_index];

  if (!image_processor_->Process(
          std::move(frame), std::move(output_frame),
          base::BindOnce(&V4L2VideoEncodeAccelerator::FrameProcessed,
                         weak_this_, force_keyframe, timestamp,
                         output_buffer_index))) {
    SetErrorState({EncoderStatus::Codes::kFormatConversionError,
                   "Failed in ImageProcessor::Process"});
    return;
  }

  num_frames_in_image_processor_++;
}

void V4L2VideoEncodeAccelerator::UseOutputBitstreamBufferTask(
    BitstreamBuffer buffer) {
  DVLOGF(4) << "id=" << buffer.id();
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  if (buffer.size() < output_buffer_byte_size_) {
    SetErrorState({EncoderStatus::Codes::kInvalidOutputBuffer,
                   "Provided bitstream buffer too small"});
    return;
  }

  base::UnsafeSharedMemoryRegion shm_region = buffer.TakeRegion();
  base::WritableSharedMemoryMapping shm_mapping =
      shm_region.MapAt(buffer.offset(), buffer.size());
  if (!shm_mapping.IsValid()) {
    SetErrorState({EncoderStatus::Codes::kSystemAPICallError,
                   "Failed to map a shared memory buffer"});
    return;
  }

  bitstream_buffer_pool_.push_back(std::make_unique<BitstreamBufferRef>(
      buffer.id(), std::move(shm_mapping)));
  PumpBitstreamBuffers();

  if (encoder_state_ == kInitialized) {
    if (!StartDevicePoll())
      return;
    encoder_state_ = kEncoding;
  }
}

void V4L2VideoEncodeAccelerator::DestroyTask() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  weak_this_factory_.InvalidateWeakPtrs();

  // If a flush is pending, notify client that it did not finish.
  if (flush_callback_) {
    child_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(flush_callback_), false));
  }

  // Stop streaming and the device_poll_thread_.
  LOG_IF(ERROR, !StopDevicePoll()) << "Failure in termination";

  DestroyInputBuffers();
  DestroyOutputBuffers();

  delete this;
}

void V4L2VideoEncodeAccelerator::ServiceDeviceTask() {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
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
  bool poll_device = (input_queue_->QueuedBuffersCount() +
                          output_queue_->QueuedBuffersCount() >
                      0);

  // ServiceDeviceTask() should only ever be scheduled from DevicePollTask(),
  // so either:
  // * device_poll_thread_ is running normally
  // * device_poll_thread_ scheduled us, but then a DestroyTask() shut it down,
  //   in which case we're in kError state, and we should have early-outed
  //   already.
  DCHECK(device_poll_thread_.task_runner());
  // Queue the DevicePollTask() now.
  // base::Unretained(this) is safe, because device_poll_thread_ is owned by
  // *this and stops before *this destruction.
  device_poll_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2VideoEncodeAccelerator::DevicePollTask,
                                base::Unretained(this), poll_device));

  DVLOGF(3) << encoder_input_queue_.size() << "] => DEVICE["
            << input_queue_->FreeBuffersCount() << "+"
            << input_queue_->QueuedBuffersCount() << "/"
            << input_buffer_map_.size() << "->"
            << output_queue_->FreeBuffersCount() << "+"
            << output_queue_->QueuedBuffersCount() << "/"
            << output_queue_->AllocatedBuffersCount() << "] => OUT["
            << bitstream_buffer_pool_.size() << "]";
}

void V4L2VideoEncodeAccelerator::Enqueue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK(input_queue_ && output_queue_);
  TRACE_EVENT0("media,gpu", "V4L2VEA::Enqueue");
  DVLOGF(4) << "free_input_buffers: " << input_queue_->FreeBuffersCount()
            << ", input_queue: " << encoder_input_queue_.size();

  bool do_streamon = false;
  // Enqueue all the inputs we can.
  const size_t old_inputs_queued = input_queue_->QueuedBuffersCount();
  while (!encoder_input_queue_.empty() &&
         input_queue_->FreeBuffersCount() > 0) {
    // A null frame indicates a flush.
    if (encoder_input_queue_.front().frame == nullptr) {
      DVLOGF(3) << "All input frames needed to be flushed are enqueued.";
      encoder_input_queue_.pop();

      // If we are not streaming, the device is not running and there is no need
      // to call V4L2_ENC_CMD_STOP to request a flush. This also means there is
      // nothing left to process, so we can return flush success back to the
      // client.
      if (!input_queue_->IsStreaming()) {
        child_task_runner_->PostTask(
            FROM_HERE, base::BindOnce(std::move(flush_callback_), true));
        return;
      }
      struct v4l2_encoder_cmd cmd;
      memset(&cmd, 0, sizeof(cmd));
      cmd.cmd = V4L2_ENC_CMD_STOP;
      if (device_->Ioctl(VIDIOC_ENCODER_CMD, &cmd) != 0) {
        SetErrorState(
            {EncoderStatus::Codes::kEncoderFailedFlush,
             base::StrCat({"ioctl() failed: VIDIOC_ENCODER_CMD, errno=",
                           base::NumberToString(errno)})});
        child_task_runner_->PostTask(
            FROM_HERE, base::BindOnce(std::move(flush_callback_), false));
        return;
      }
      encoder_state_ = kFlushing;
      break;
    }

    std::optional<V4L2WritableBufferRef> input_buffer;
    switch (input_memory_type_) {
      case V4L2_MEMORY_DMABUF:
        input_buffer = input_queue_->GetFreeBufferForFrame(
            GetSharedMemoryId(*encoder_input_queue_.front().frame));
        // We may have failed to preserve buffer affinity, fallback to any
        // buffer in that case.
        if (!input_buffer)
          input_buffer = input_queue_->GetFreeBuffer();
        break;
      default:
        input_buffer = input_queue_->GetFreeBuffer();
        break;
    }
    // input_buffer cannot be std::nullopt since we checked for
    // input_queue_->FreeBuffersCount() > 0 before entering the loop.
    DCHECK(input_buffer);
    if (!EnqueueInputRecord(std::move(*input_buffer)))
      return;
  }
  if (old_inputs_queued == 0 && input_queue_->QueuedBuffersCount() != 0) {
    // We just started up a previously empty queue.
    // Queue state changed; signal interrupt.
    if (!device_->SetDevicePollInterrupt())
      return;
    // Shall call VIDIOC_STREAMON if we haven't yet.
    do_streamon = !input_queue_->IsStreaming();
  }

  if (!input_queue_->IsStreaming() && !do_streamon) {
    // We don't have to enqueue any buffers in the output queue until we enqueue
    // buffers in the input queue. This enables to call S_FMT in Encode() on
    // the first frame.
    return;
  }

  // Enqueue all the outputs we can.
  const size_t old_outputs_queued = output_queue_->QueuedBuffersCount();
  while (auto output_buffer = output_queue_->GetFreeBuffer()) {
    if (!EnqueueOutputRecord(std::move(*output_buffer)))
      return;
  }
  if (old_outputs_queued == 0 && output_queue_->QueuedBuffersCount() != 0) {
    // We just started up a previously empty queue.
    // Queue state changed; signal interrupt.
    if (!device_->SetDevicePollInterrupt())
      return;
  }

  // STREAMON in CAPTURE queue first and then OUTPUT queue.
  // This is a workaround of a tegra driver bug that STREAMON in CAPTURE queue
  // will never return (i.e. blocks |encoder_thread_| forever) if the STREAMON
  // in CAPTURE queue is called after STREAMON in OUTPUT queue.
  // Once nyan_kitty, which uses tegra driver, reaches EOL, crrev.com/c/1753982
  // should be reverted.
  if (do_streamon) {
    DCHECK(!output_queue_->IsStreaming() && !input_queue_->IsStreaming());
    // When VIDIOC_STREAMON can be executed in OUTPUT queue, it is fine to call
    // STREAMON in CAPTURE queue.
    if (!output_queue_->Streamon()) {
      SetErrorState({EncoderStatus::Codes::kEncoderHardwareDriverError,
                     "Failed to turn on streaming for CAPTURE queue"});
      return;
    }
    if (!input_queue_->Streamon()) {
      SetErrorState({EncoderStatus::Codes::kEncoderHardwareDriverError,
                     "Failed to turn on streaming for OUTPUT queue"});
      return;
    }
  }
}

void V4L2VideoEncodeAccelerator::Dequeue() {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  TRACE_EVENT0("media,gpu", "V4L2VEA::Dequeue");

  // Dequeue completed input (VIDEO_OUTPUT) buffers, and recycle to the free
  // list.
  while (input_queue_->QueuedBuffersCount() > 0) {
    DVLOGF(4) << "inputs queued: " << input_queue_->QueuedBuffersCount();
    DCHECK(input_queue_->IsStreaming());

    auto ret = input_queue_->DequeueBuffer();
    if (!ret.first) {
      SetErrorState(
          {EncoderStatus::Codes::kEncoderHardwareDriverError,
           base::StrCat({"Failed to dequeue buffer in OUTPUT queue, errno=",
                         base::NumberToString(errno)})});
      return;
    }
    if (!ret.second) {
      // We're just out of buffers to dequeue.
      break;
    }

    InputRecord& input_record = input_buffer_map_[ret.second->BufferId()];
    input_record.frame = nullptr;
    if (input_record.ip_output_buffer_index)
      ReuseImageProcessorOutputBuffer(*input_record.ip_output_buffer_index);
  }

  // Dequeue completed output (VIDEO_CAPTURE) buffers, and recycle to the
  // free list.  Notify the client that an output buffer is complete.
  bool buffer_dequeued = false;
  while (output_queue_->QueuedBuffersCount() > 0) {
    DCHECK(output_queue_->IsStreaming());

    auto ret = output_queue_->DequeueBuffer();
    if (!ret.first) {
      SetErrorState(
          {EncoderStatus::Codes::kEncoderHardwareDriverError,
           base::StrCat({"Failed to dequeue buffer in CAPTURE queue, errno=",
                         base::NumberToString(errno)})});
      return;
    }
    if (!ret.second) {
      // We're just out of buffers to dequeue.
      break;
    }

    const uint64_t timestamp_us =
        ret.second->GetTimeStamp().tv_usec +
        ret.second->GetTimeStamp().tv_sec * base::Time::kMicrosecondsPerSecond;
    TRACE_EVENT_NESTABLE_ASYNC_END2(
        "media,gpu", "PlatformEncoding.Encode", timestamp_us, "timestamp",
        timestamp_us, "size", encoder_input_visible_rect_.size().ToString());

    output_buffer_queue_.push_back(std::move(ret.second));
    buffer_dequeued = true;
  }

  if (buffer_dequeued)
    PumpBitstreamBuffers();
}

void V4L2VideoEncodeAccelerator::PumpBitstreamBuffers() {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  while (!output_buffer_queue_.empty()) {
    auto output_buf = std::move(output_buffer_queue_.front());
    output_buffer_queue_.pop_front();

    size_t bitstream_size = base::checked_cast<size_t>(
        output_buf->GetPlaneBytesUsed(0) - output_buf->GetPlaneDataOffset(0));
    if (bitstream_size > 0) {
      if (bitstream_buffer_pool_.empty()) {
        DVLOGF(4) << "No free bitstream buffer, skip.";
        output_buffer_queue_.push_front(std::move(output_buf));
        break;
      }

      auto buffer_ref = std::move(bitstream_buffer_pool_.back());
      auto buffer_id = buffer_ref->id;
      bitstream_buffer_pool_.pop_back();

      const uint8_t* output_buffer =
          static_cast<const uint8_t*>(output_buf->GetPlaneMapping(0)) +
          output_buf->GetPlaneDataOffset(0);

      size_t output_data_size = CopyIntoOutputBuffer(
          output_buffer, bitstream_size, std::move(buffer_ref));

      DVLOGF(4) << "returning buffer_id=" << buffer_id
                << ", size=" << output_data_size
                << ", key_frame=" << output_buf->IsKeyframe();
      const int64_t timestamp_us = output_buf->GetTimeStamp().tv_usec +
                                   output_buf->GetTimeStamp().tv_sec *
                                       base::Time::kMicrosecondsPerSecond;
      TRACE_EVENT2("media,gpu", "V4L2VEA::BitstreamBufferReady", "timestamp",
                   timestamp_us, "bitstream_buffer_id", buffer_id);
      child_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&Client::BitstreamBufferReady, client_, buffer_id,
                         GetMetadata(output_buffer, output_data_size,
                                     output_buf->IsKeyframe(),
                                     base::Microseconds(timestamp_us))));
    }

    if ((encoder_state_ == kFlushing) && output_buf->IsLast()) {
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
      if (device_->Ioctl(VIDIOC_ENCODER_CMD, &cmd) != 0) {
        SetErrorState(
            {EncoderStatus::Codes::kEncoderFailedFlush,
             base::StrCat({"ioctl() failed: VIDIOC_ENCODER_CMD, errno=",
                           base::NumberToString(errno)})});
        return;
      }
    }
  }

  // We may free some V4L2 output buffers above. Enqueue them if needed.
  if (output_queue_->FreeBuffersCount() > 0) {
    encoder_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&V4L2VideoEncodeAccelerator::Enqueue, weak_this_));
  }
}

bool V4L2VideoEncodeAccelerator::EnqueueInputRecord(
    V4L2WritableBufferRef input_buf) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK(!encoder_input_queue_.empty());
  TRACE_EVENT0("media,gpu", "V4L2VEA::EnqueueInputRecord");

  // Enqueue an input (VIDEO_OUTPUT) buffer.
  InputFrameInfo frame_info = encoder_input_queue_.front();
  if (frame_info.force_keyframe) {
    if (!device_->SetExtCtrls(
            V4L2_CTRL_CLASS_MPEG,
            {V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME)})) {
      SetErrorState({EncoderStatus::Codes::kEncoderFailedEncode,
                     "Failed requesting keyframe"});
      return false;
    }
  }

  scoped_refptr<VideoFrame> frame = frame_info.frame;

  size_t buffer_id = input_buf.BufferId();

  struct timeval timestamp;
  timestamp.tv_sec = static_cast<time_t>(frame->timestamp().InSeconds());
  timestamp.tv_usec =
      frame->timestamp().InMicroseconds() -
      frame->timestamp().InSeconds() * base::Time::kMicrosecondsPerSecond;
  input_buf.SetTimeStamp(timestamp);

  DCHECK_EQ(device_input_layout_->format(), frame->format());
  size_t num_planes = GetNumPlanesOfV4L2PixFmt(
      Fourcc::FromVideoPixelFormat(device_input_layout_->format(),
                                   !device_input_layout_->is_multi_planar())
          ->ToV4L2PixFmt());

  // Create GpuMemoryBufferHandle for native_input_mode.
  gfx::GpuMemoryBufferHandle gmb_handle;
  if (input_buf.Memory() == V4L2_MEMORY_DMABUF) {
    gmb_handle = CreateGpuMemoryBufferHandle(frame.get());
    if (gmb_handle.is_null() || gmb_handle.type != gfx::NATIVE_PIXMAP) {
      SetErrorState({EncoderStatus::Codes::kSystemAPICallError,
                     "Failed to create native GpuMemoryBufferHandle"});
      return false;
    }
  }

  for (size_t i = 0; i < num_planes; ++i) {
    // Single-buffer input format may have multiple color planes, so bytesused
    // of the single buffer should be sum of each color planes' size.
    size_t bytesused = 0;
    if (num_planes == 1) {
      bytesused = VideoFrame::AllocationSize(
          frame->format(), device_input_layout_->coded_size());
    } else {
      bytesused = base::checked_cast<size_t>(
          VideoFrame::PlaneSize(frame->format(), i,
                                device_input_layout_->coded_size())
              .GetArea());
    }

    switch (input_buf.Memory()) {
      case V4L2_MEMORY_USERPTR:
        // Use buffer_size VideoEncodeAccelerator HW requested by S_FMT.
        input_buf.SetPlaneSize(i, device_input_layout_->planes()[i].size);
        break;

      case V4L2_MEMORY_DMABUF: {
        const std::vector<gfx::NativePixmapPlane>& planes =
            gmb_handle.native_pixmap_handle.planes;
        // TODO(crbug.com/901264): The way to pass an offset within a DMA-buf is
        // not defined in V4L2 specification, so we abuse data_offset for now.
        // Fix it when we have the right interface, including any necessary
        // validation and potential alignment
        input_buf.SetPlaneDataOffset(i, planes[i].offset);
        bytesused += planes[i].offset;
        // Workaround: filling length should not be needed. This is a bug of
        // videobuf2 library.
        input_buf.SetPlaneSize(
            i, device_input_layout_->planes()[i].size + planes[i].offset);
        break;
      }
      default:
        NOTREACHED();
    }

    input_buf.SetPlaneBytesUsed(i, bytesused);
  }

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("media,gpu", "PlatformEncoding.Encode",
                                    frame->timestamp().InMicroseconds(),
                                    "timestamp",
                                    frame->timestamp().InMicroseconds());

  switch (input_buf.Memory()) {
    case V4L2_MEMORY_USERPTR: {
      if (frame->storage_type() != VideoFrame::STORAGE_SHMEM) {
        SetErrorState({EncoderStatus::Codes::kInvalidInputFrame,
                       "VideoFrame doesn't have shared memory"});
        return false;
      }

      // The frame data is readable only and the driver doesn't actually write
      // the buffer. But USRPTR buffer needs void*. So const_cast<> is required.
      std::vector<void*> user_ptrs(num_planes);
      for (size_t i = 0; i < num_planes; ++i) {
        user_ptrs[i] = const_cast<uint8_t*>(frame->data(i));
      }
      if (!std::move(input_buf).QueueUserPtr(std::move(user_ptrs))) {
        SetErrorState(
            {EncoderStatus::Codes::kEncoderHardwareDriverError,
             base::StrCat(
                 {"Failed queue a USRPTR buffer to input queue, errno=",
                  base::NumberToString(errno)})});
        return false;
      }
      break;
    }
    case V4L2_MEMORY_DMABUF: {
      if (!std::move(input_buf).QueueDMABuf(
              gmb_handle.native_pixmap_handle.planes)) {
        SetErrorState(
            {EncoderStatus::Codes::kEncoderHardwareDriverError,
             base::StrCat(
                 {"Failed queue a DMABUF buffer to input queue, errno=",
                  base::NumberToString(errno)})});
        return false;
      }

      // TODO(b/266443239): Remove this workaround once RK3399 boards reaches
      // EOL. v4lplugin holds v4l2_buffer in QBUF without duplicating the passed
      // fds and resumes the QBUF request later after VIDIOC_QBUF returns. It is
      // required to keep the passed fds valid until DQBUF is complete.
      if (driver_name_ == "hantro-vpu") {
        frame->AddDestructionObserver(base::BindOnce(
            [](gfx::GpuMemoryBufferHandle) {}, std::move(gmb_handle)));
      }
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
      SetErrorState({EncoderStatus::Codes::kEncoderIllegalState,
                     base::StrCat({"Unknown input memory type: ",
                                   base::NumberToString(static_cast<int>(
                                       input_buf.Memory()))})});
      return false;
  }

  // Keep |frame| in |input_record| so that a client doesn't use |frame| until
  // a driver finishes using it, that is, VIDIOC_DQBUF is called.
  InputRecord& input_record = input_buffer_map_[buffer_id];
  input_record.frame = frame;
  input_record.ip_output_buffer_index = frame_info.ip_output_buffer_index;
  encoder_input_queue_.pop();
  return true;
}

bool V4L2VideoEncodeAccelerator::EnqueueOutputRecord(
    V4L2WritableBufferRef output_buf) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  TRACE_EVENT0("media,gpu", "V4L2VEA::EnqueueOutputRecord");

  // Enqueue an output (VIDEO_CAPTURE) buffer.
  if (!std::move(output_buf).QueueMMap()) {
    SetErrorState({EncoderStatus::Codes::kEncoderHardwareDriverError,
                   base::StrCat({"Failed to QueueMMap, errno=",
                                 base::NumberToString(errno)})});
    return false;
  }
  return true;
}

bool V4L2VideoEncodeAccelerator::StartDevicePoll() {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK(!device_poll_thread_.IsRunning());

  // Start up the device poll thread and schedule its first DevicePollTask().
  if (!device_poll_thread_.Start()) {
    SetErrorState({EncoderStatus::Codes::kSystemAPICallError,
                   "StartDevicePoll(): Device thread failed to start"});
    return false;
  }
  // Enqueue a poll task with no devices to poll on -- it will wait only on the
  // interrupt fd.
  // base::Unretained(this) is safe, because device_poll_thread_ is owned by
  // *this and stops before *this destruction.
  device_poll_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2VideoEncodeAccelerator::DevicePollTask,
                                base::Unretained(this), false));

  return true;
}

bool V4L2VideoEncodeAccelerator::StopDevicePoll() {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  if (device_->IsValid()) {
    // Signal the DevicePollTask() to stop, and stop the device poll thread.
    if (!device_->SetDevicePollInterrupt()) {
      return false;
    }
    device_poll_thread_.Stop();
    // Clear the interrupt now, to be sure.
    if (!device_->ClearDevicePollInterrupt()) {
      return false;
    }
  }
  // Tegra driver cannot call Streamoff() when the stream is off, so we check
  // IsStreaming() first.
  if (input_queue_ && input_queue_->IsStreaming() && !input_queue_->Streamoff())
    return false;

  if (output_queue_ && output_queue_->IsStreaming() &&
      !output_queue_->Streamoff())
    return false;

  // Reset all our accounting info.
  while (!encoder_input_queue_.empty())
    encoder_input_queue_.pop();
  for (auto& [frame, ip_output_buffer_index] : input_buffer_map_) {
    frame = nullptr;
  }

  bitstream_buffer_pool_.clear();

  DVLOGF(3) << "device poll stopped";
  return true;
}

void V4L2VideoEncodeAccelerator::DevicePollTask(bool poll_device) {
  DVLOGF(4);
  DCHECK(device_poll_thread_.task_runner()->BelongsToCurrentThread());

  bool event_pending;
  if (!device_->Poll(poll_device, &event_pending)) {
    SetErrorState({EncoderStatus::Codes::kSystemAPICallError,
                   "Failed to start device polloing"});
    return;
  }

  // All processing should happen on ServiceDeviceTask(), since we shouldn't
  // touch encoder state from this thread.
  encoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V4L2VideoEncodeAccelerator::ServiceDeviceTask,
                                weak_this_));
}

void V4L2VideoEncodeAccelerator::SetErrorState(EncoderStatus status) {
  // We can touch encoder_state_ only if this is the encoder thread or the
  // encoder thread isn't running.
  if (!encoder_task_runner_->RunsTasksInCurrentSequence()) {
    encoder_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&V4L2VideoEncodeAccelerator::SetErrorState,
                                  weak_this_, status));
    return;
  }

  CHECK(!status.is_ok());
  LOG(ERROR) << "SetErrorState: code=" << static_cast<int>(status.code())
             << ", message=" << status.message();
  // Post NotifyErrorStatus() only if we are already initialized, as the API
  // does not allow doing so before that.
  if (encoder_state_ != kError && encoder_state_ != kUninitialized) {
    LOG(ERROR) << "Call NotifyErrorStatus(): code="
               << static_cast<int>(status.code())
               << ", message=" << status.message();
    CHECK(child_task_runner_);
    child_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&Client::NotifyErrorStatus, client_, std::move(status)));
  }

  encoder_state_ = kError;
}

void V4L2VideoEncodeAccelerator::RequestEncodingParametersChangeTask(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate,
    const std::optional<gfx::Size>& size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  if (size.has_value()) {
    SetErrorState({EncoderStatus::Codes::kEncoderUnsupportedConfig,
                   "Update output frame size is not supported"});
    return;
  }
  if (current_bitrate_allocation_ == bitrate_allocation &&
      current_framerate_ == framerate) {
    return;
  }

  DVLOGF(2) << "bitrate=" << bitrate_allocation.ToString()
            << ", framerate=" << framerate;
  if (bitrate_allocation.GetMode() != current_bitrate_allocation_.GetMode()) {
    SetErrorState({EncoderStatus::Codes::kEncoderUnsupportedConfig,
                   "Bitrate mode changed during encoding"});
    return;
  }

  TRACE_EVENT2("media,gpu", "V4L2VEA::RequestEncodingParametersChangeTask",
               "bitrate", current_bitrate_allocation_.ToString(), "framerate",
               framerate);
  if (current_bitrate_allocation_ != bitrate_allocation) {
    switch (bitrate_allocation.GetMode()) {
      case Bitrate::Mode::kVariable:
        device_->SetExtCtrls(V4L2_CTRL_CLASS_MPEG,
                             {V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_BITRATE_PEAK,
                                          bitrate_allocation.GetPeakBps())});

        // Both the average and peak bitrate are to be set in VBR.
        // Only the average bitrate are to be set in CBR.
        [[fallthrough]];
      case Bitrate::Mode::kConstant:
        if (h264_l1t2_enabled_) {
          if (!device_->SetExtCtrls(
                  V4L2_CTRL_CLASS_MPEG,
                  {V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_H264_HIER_CODING_L0_BR,
                               bitrate_allocation.GetBitrateBps(0u, 0u)),
                   V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_H264_HIER_CODING_L1_BR,
                               bitrate_allocation.GetBitrateBps(0u, 1u))})) {
            SetErrorState({EncoderStatus::Codes::kEncoderHardwareDriverError,
                           "Failed to change average bitrate"});
            return;
          }
        } else if (!device_->SetExtCtrls(
                       V4L2_CTRL_CLASS_MPEG,
                       {V4L2ExtCtrl(
                           V4L2_CID_MPEG_VIDEO_BITRATE,
                           bitrate_allocation.GetBitrateBps(0u, 0u))})) {
          SetErrorState({EncoderStatus::Codes::kEncoderHardwareDriverError,
                         "Failed to change average bitrate"});
          return;
        }
        break;
      case Bitrate::Mode::kExternal:
        SetErrorState({EncoderStatus::Codes::kEncoderUnsupportedConfig,
                       "Unsupported rate control mode."});
        return;
    }
  }

  if (current_framerate_ != framerate) {
    struct v4l2_streamparm parms;
    memset(&parms, 0, sizeof(parms));
    parms.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    // Note that we are provided "frames per second" but V4L2 expects "time per
    // frame"; hence we provide the reciprocal of the framerate here.
    parms.parm.output.timeperframe.numerator = 1;
    parms.parm.output.timeperframe.denominator = framerate;
    if (device_->Ioctl(VIDIOC_S_PARM, &parms) != 0) {
      SetErrorState({EncoderStatus::Codes::kEncoderHardwareDriverError,
                     base::StrCat({"ioctl() failed: VIDIOC_S_PARM, errno=",
                                   base::NumberToString(errno)})});
      return;
    }
  }

  current_bitrate_allocation_ = bitrate_allocation;
  current_framerate_ = framerate;
}

bool V4L2VideoEncodeAccelerator::SetOutputFormat(
    VideoCodecProfile output_profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK(!input_queue_->IsStreaming());
  DCHECK(!output_queue_->IsStreaming());

  DCHECK(!encoder_input_visible_rect_.IsEmpty());
  output_buffer_byte_size_ =
      GetEncodeBitstreamBufferSize(encoder_input_visible_rect_.size());

  // Sets 0 to width and height in CAPTURE queue, which should be ignored by the
  // driver.
  std::optional<struct v4l2_format> format = output_queue_->SetFormat(
      output_format_fourcc_, gfx::Size(), output_buffer_byte_size_);
  if (!format) {
    return false;
  }

  // Device might have adjusted the required output size.
  size_t adjusted_output_buffer_size =
      base::checked_cast<size_t>(format->fmt.pix_mp.plane_fmt[0].sizeimage);
  output_buffer_byte_size_ = adjusted_output_buffer_size;

  return true;
}

std::optional<struct v4l2_format>
V4L2VideoEncodeAccelerator::NegotiateInputFormat(VideoPixelFormat input_format,
                                                 const gfx::Size& size) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK(!input_queue_->IsStreaming());
  DCHECK(!output_queue_->IsStreaming());

  // First see if the device can use the provided format directly.
  std::vector<uint32_t> pix_fmt_candidates;
  auto input_fourcc = Fourcc::FromVideoPixelFormat(input_format, false);
  if (!input_fourcc) {
    LOG(ERROR) << "Invalid input format "
               << VideoPixelFormatToString(input_format);
    return std::nullopt;
  }
  pix_fmt_candidates.push_back(input_fourcc->ToV4L2PixFmt());
  // Second try preferred input formats for both single-planar and
  // multi-planar.
  for (auto preferred_format :
       device_->PreferredInputFormat(V4L2Device::Type::kEncoder)) {
    pix_fmt_candidates.push_back(preferred_format);
  }

  for (const auto pix_fmt : pix_fmt_candidates) {
    DVLOGF(3) << "Trying S_FMT with " << FourccToString(pix_fmt);

    std::optional<struct v4l2_format> format =
        input_queue_->SetFormat(pix_fmt, size, 0);
    if (!format)
      continue;

    DVLOGF(3) << "Success: S_FMT with " << FourccToString(pix_fmt);
    device_input_layout_ = V4L2FormatToVideoFrameLayout(*format);
    if (!device_input_layout_) {
      LOG(ERROR) << "Invalid device_input_layout_";
      return std::nullopt;
    }
    DVLOG(3) << "Negotiated device_input_layout_: " << *device_input_layout_;
    if (!gfx::Rect(device_input_layout_->coded_size())
             .Contains(gfx::Rect(size))) {
      LOG(ERROR) << "Input size " << size.ToString()
                 << " exceeds encoder capability. Size encoder can handle: "
                 << device_input_layout_->coded_size().ToString();
      return std::nullopt;
    }
    // Make sure that the crop is preserved as we have changed the input
    // resolution.
    if (!ApplyCrop()) {
      return std::nullopt;
    }

    return format;
  }
  return std::nullopt;
}

bool V4L2VideoEncodeAccelerator::ApplyCrop() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  struct v4l2_rect visible_rect;
  visible_rect.left = encoder_input_visible_rect_.x();
  visible_rect.top = encoder_input_visible_rect_.y();
  visible_rect.width = encoder_input_visible_rect_.width();
  visible_rect.height = encoder_input_visible_rect_.height();

  struct v4l2_selection selection_arg;
  memset(&selection_arg, 0, sizeof(selection_arg));
  selection_arg.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  selection_arg.target = V4L2_SEL_TGT_CROP;
  selection_arg.r = visible_rect;

  // The width and height might be adjusted by driver.
  // Need to read it back and set to |encoder_input_visible_rect_|.
  if (device_->Ioctl(VIDIOC_S_SELECTION, &selection_arg) == 0) {
    DVLOGF(3) << "VIDIOC_S_SELECTION is supported";
    visible_rect = selection_arg.r;
  } else {
    DVLOGF(3) << "Fallback to VIDIOC_S/G_CROP";
    struct v4l2_crop crop;
    memset(&crop, 0, sizeof(crop));
    crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    crop.c = visible_rect;
    if (device_->Ioctl(VIDIOC_S_CROP, &crop) != 0) {
      SetErrorState({EncoderStatus::Codes::kEncoderHardwareDriverError,
                     base::StrCat({"ioctl() failed: VIDIOC_S_CROP, errno=",
                                   base::NumberToString(errno)})});
      return false;
    }
    if (device_->Ioctl(VIDIOC_G_CROP, &crop) != 0) {
      SetErrorState({EncoderStatus::Codes::kEncoderHardwareDriverError,
                     base::StrCat({"ioctl() failed: VIDIOC_G_CROP",
                                   base::NumberToString(errno)})});
      return false;
    }

    visible_rect = crop.c;
  }

  const gfx::Rect adjusted_visible_rect(visible_rect.left, visible_rect.top,
                                        visible_rect.width,
                                        visible_rect.height);
  if (encoder_input_visible_rect_ != adjusted_visible_rect) {
    LOG(ERROR) << "Unsupported visible rectangle: "
               << encoder_input_visible_rect_.ToString()
               << ", the rectangle adjusted by the driver: "
               << adjusted_visible_rect.ToString();
    return false;
  }
  return true;
}

bool V4L2VideoEncodeAccelerator::SetFormats(VideoPixelFormat input_format,
                                            VideoCodecProfile output_profile) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK(!input_queue_->IsStreaming());
  DCHECK(!output_queue_->IsStreaming());

  if (!SetOutputFormat(output_profile)) {
    SetErrorState({EncoderStatus::Codes::kEncoderUnsupportedProfile,
                   base::StrCat({"Unsupported codec profile: ",
                                 GetProfileName(output_profile)})});
    return false;
  }

  gfx::Size input_size = encoder_input_visible_rect_.size();
  if (native_input_mode_) {
    auto input_layout = GetPlatformVideoFrameLayout(
        input_format, encoder_input_visible_rect_.size(),
        gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE);
    if (!input_layout)
      return false;
    input_size = gfx::Size(input_layout->planes()[0].stride,
                           input_layout->coded_size().height());
  }

  DCHECK(input_frame_size_.IsEmpty());
  auto v4l2_format = NegotiateInputFormat(input_format, input_size);
  if (!v4l2_format) {
    SetErrorState({EncoderStatus::Codes::kUnsupportedFrameFormat,
                   base::StrCat({"Unsupported input format: ",
                                 VideoPixelFormatToString(input_format)})});
    return false;
  }

  if (native_input_mode_) {
    input_frame_size_ = VideoFrame::DetermineAlignedSize(
        input_format, encoder_input_visible_rect_.size());
  } else {
    input_frame_size_ = V4L2Device::AllocatedSizeFromV4L2Format(*v4l2_format);
  }
  return true;
}

bool V4L2VideoEncodeAccelerator::InitControls(const Config& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  // Don't expect other output formats.
  CHECK(output_format_fourcc_ == V4L2_PIX_FMT_H264 ||
        output_format_fourcc_ == V4L2_PIX_FMT_VP8);

  // Enable frame-level bitrate control. This is the only mandatory control.
  if (!device_->SetExtCtrls(
          V4L2_CTRL_CLASS_MPEG,
          {V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE, 1)})) {
    SetErrorState({EncoderStatus::Codes::kEncoderHardwareDriverError,
                   "Failed enabling bitrate control"});
    return false;
  }

  switch (output_format_fourcc_) {
    case V4L2_PIX_FMT_H264:
      if (!InitControlsH264(config)) {
        return false;
      }
      break;
    case V4L2_PIX_FMT_VP8:
      InitControlsVP8(config);
      break;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unsupported codec " << FourccToString(output_format_fourcc_);
      return false;
  }

  // Optional controls:
  // - Enable macroblock-level bitrate control.
  device_->SetExtCtrls(V4L2_CTRL_CLASS_MPEG,
                       {V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_MB_RC_ENABLE, 1)});

  // - Set GOP length, or default 0 to disable periodic key frames.
  device_->SetGOPLength(config.gop_length.value_or(0));

  return true;
}

bool V4L2VideoEncodeAccelerator::InitControlsH264(const Config& config) {
#ifndef V4L2_CID_MPEG_VIDEO_PREPEND_SPSPPS_TO_IDR
#define V4L2_CID_MPEG_VIDEO_PREPEND_SPSPPS_TO_IDR (V4L2_CID_MPEG_BASE + 644)
#endif
  // Request to inject SPS and PPS before each IDR, if the device supports
  // that feature. Otherwise we'll have to cache and inject ourselves.
  if (device_->IsCtrlExposed(V4L2_CID_MPEG_VIDEO_PREPEND_SPSPPS_TO_IDR)) {
    if (!device_->SetExtCtrls(
            V4L2_CTRL_CLASS_MPEG,
            {V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_PREPEND_SPSPPS_TO_IDR, 1)})) {
      SetErrorState(
          {EncoderStatus::Codes::kEncoderHardwareDriverError,
           "Failed to set V4L2_CID_MPEG_VIDEO_PREPEND_SPSPPS_TO_IDR to 1"});
      return false;
    }
    inject_sps_and_pps_ = false;
    DVLOGF(2) << "Device supports injecting SPS+PPS before each IDR";
  } else {
    inject_sps_and_pps_ = true;
    DVLOGF(2) << "Will inject SPS+PPS before each IDR, unsupported by device";
  }

  // No B-frames, for lowest decoding latency.
  device_->SetExtCtrls(V4L2_CTRL_CLASS_MPEG,
                       {V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_B_FRAMES, 0)});

  // Set H.264 profile.
  int32_t profile_value =
      V4L2Device::VideoCodecProfileToV4L2H264Profile(config.output_profile);
  if (profile_value < 0) {
    SetErrorState({EncoderStatus::Codes::kEncoderUnsupportedProfile,
                   base::StrCat({"Unexpected h264 profile: ",
                                 GetProfileName(config.output_profile)})});
    return false;
  }
  if (!device_->SetExtCtrls(
          V4L2_CTRL_CLASS_MPEG,
          {V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_H264_PROFILE, profile_value)})) {
    SetErrorState({EncoderStatus::Codes::kEncoderUnsupportedProfile,
                   base::StrCat({"Unsupported h264 profile: ",
                                 GetProfileName(config.output_profile)})});
    return false;
  }

  // Set H.264 output level from config. Use Level 4.0 as fallback default.
  uint8_t h264_level = config.h264_output_level.value_or(H264SPS::kLevelIDC4p0);
  constexpr int kH264MacroblockSizeInPixels = 16;
  const uint32_t framerate = config.framerate;
  const uint32_t mb_width =
      base::bits::AlignUpDeprecatedDoNotUse(config.input_visible_size.width(),
                                            kH264MacroblockSizeInPixels) /
      kH264MacroblockSizeInPixels;
  const uint32_t mb_height =
      base::bits::AlignUpDeprecatedDoNotUse(config.input_visible_size.height(),
                                            kH264MacroblockSizeInPixels) /
      kH264MacroblockSizeInPixels;
  const uint32_t framesize_in_mbs = mb_width * mb_height;

  // Check whether the h264 level is valid.
  if (!CheckH264LevelLimits(config.output_profile, h264_level,
                            config.bitrate.target_bps(), framerate,
                            framesize_in_mbs)) {
    std::optional<uint8_t> valid_level =
        FindValidH264Level(config.output_profile, config.bitrate.target_bps(),
                           framerate, framesize_in_mbs);
    if (!valid_level) {
      SetErrorState(
          {EncoderStatus::Codes::kEncoderInitializationError,
           base::StrCat({"Could not find a valid h264 level for"
                         " profile=",
                         GetProfileName(config.output_profile), " bitrate=",
                         base::NumberToString(config.bitrate.target_bps()),
                         " framerate=", base::NumberToString(framerate),
                         " size=", config.input_visible_size.ToString()})});
      return false;
    }

    h264_level = *valid_level;
  }

  int32_t level_value = V4L2Device::H264LevelIdcToV4L2H264Level(h264_level);
  device_->SetExtCtrls(
      V4L2_CTRL_CLASS_MPEG,
      {V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_H264_LEVEL, level_value)});

  // Ask not to put SPS and PPS into separate bitstream buffers.
  device_->SetExtCtrls(
      V4L2_CTRL_CLASS_MPEG,
      {V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_HEADER_MODE,
                   V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME)});

  // H264 coding tools parameter. Since a driver may not support some of them,
  // EXT_S_CTRLS is called to each of them to enable as many coding tools as
  // possible.
  // Don't produce Intra-only frame.
  device_->SetExtCtrls(V4L2_CTRL_CLASS_MPEG,
                       {V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_H264_I_PERIOD, 0)});
  // Enable deblocking filter.
  device_->SetExtCtrls(
      V4L2_CTRL_CLASS_MPEG,
      {V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE,
                   V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_ENABLED)});
  // Use CABAC in Main and High profiles.
  if (config.output_profile == H264PROFILE_MAIN ||
      config.output_profile == H264PROFILE_HIGH) {
    device_->SetExtCtrls(
        V4L2_CTRL_CLASS_MPEG,
        {V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE,
                     V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC)});
  }
  // Use 8x8 transform in High profile.
  if (config.output_profile == H264PROFILE_HIGH) {
    device_->SetExtCtrls(
        V4L2_CTRL_CLASS_MPEG,
        {V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_H264_8X8_TRANSFORM, true)});
  }

  // Quantization parameter. The h264 qp range is 0-51.
  // Note: Webrtc default values are 24 and 37 respectively, see
  // h264_encoder_impl.cc.
  // These values were previously copied from the VA-API encoder.
  // The MAX_QP parameter needed modification to 51 due to
  // b/274867782 and b/241549978.
  // Ignore return values as these controls are optional.
  device_->SetExtCtrls(V4L2_CTRL_CLASS_MPEG,
                       {V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_H264_MAX_QP, 51)});
  // Don't set MIN_QP with other controls since it is not supported by
  // some devices and may prevent other controls from being set.
  // The MIN_QP needed modification due to b/280853786
  // The value 18 was tuned experimentally to let the test pass but
  // to be close to the original one
  device_->SetExtCtrls(V4L2_CTRL_CLASS_MPEG,
                       {V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_H264_MIN_QP, 18)});

  if (h264_l1t2_enabled_) {
    if (!device_->SetExtCtrls(
            V4L2_CTRL_CLASS_MPEG,
            {V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING, 1),
             V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_TYPE,
                         V4L2_MPEG_VIDEO_H264_HIERARCHICAL_CODING_P),
             V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER,
                         2u)})) {
      SetErrorState(
          {EncoderStatus::Codes::kEncoderInitializationError,
           base::StrCat(
               {"h264 hierachical coding was requested, but configuration of "
                "the V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING* controls "
                "failed."})});
      return false;
    }
  }

  return true;
}

void V4L2VideoEncodeAccelerator::InitControlsVP8(const Config& config) {
  // Quantization parameter. They are vp8 ac/dc indices and their ranges are
  // 0-127. These values were copied from the VA-API encoder.
  // Ignore return values as these controls are optional.
  device_->SetExtCtrls(V4L2_CTRL_CLASS_MPEG,
                       {V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_VPX_MIN_QP, 4),
                        V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_VPX_MAX_QP, 117)});
}

bool V4L2VideoEncodeAccelerator::CreateInputBuffers() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK(!input_queue_->IsStreaming());

  // If using DMABUF input, we want to reuse the same V4L2 buffer index
  // for the same input buffer as much as possible. But we don't know in advance
  // how many different input buffers we will get. Therefore we allocate as
  // many V4L2 buffers as possible (VIDEO_MAX_FRAME == 32). Unused indexes
  // won't have a tangible cost since they don't have backing memory.
  size_t num_buffers;
  switch (input_memory_type_) {
    case V4L2_MEMORY_DMABUF:
      num_buffers = VIDEO_MAX_FRAME;
      break;
    default:
      num_buffers = kInputBufferCount;
      break;
  }

  if (input_queue_->AllocateBuffers(num_buffers, input_memory_type_,
                                    /*incoherent=*/false) < kInputBufferCount) {
    SetErrorState({EncoderStatus::Codes::kEncoderHardwareDriverError,
                   "Failed to allocate V4L2 input buffers."});
    return false;
  }

  DCHECK(input_buffer_map_.empty());
  input_buffer_map_.resize(input_queue_->AllocatedBuffersCount());
  return true;
}

bool V4L2VideoEncodeAccelerator::CreateOutputBuffers() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK(!output_queue_->IsStreaming());

  if (output_queue_->AllocateBuffers(kOutputBufferCount, V4L2_MEMORY_MMAP,
                                     /*incoherent=*/false) <
      kOutputBufferCount) {
    SetErrorState(
        {EncoderStatus::Codes::kEncoderInitializationError,
         base::StrCat({"Failed to allocate V4L2 output buffers, errno=",
                       base::NumberToString(errno)})});
    return false;
  }
  return true;
}

void V4L2VideoEncodeAccelerator::DestroyInputBuffers() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  if (!input_queue_ || input_queue_->AllocatedBuffersCount() == 0)
    return;

  DCHECK(!input_queue_->IsStreaming());
  if (!input_queue_->DeallocateBuffers())
    VLOGF(1) << "Failed to deallocate V4L2 input buffers";

  input_buffer_map_.clear();
}

void V4L2VideoEncodeAccelerator::DestroyOutputBuffers() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  if (!output_queue_ || output_queue_->AllocatedBuffersCount() == 0)
    return;

  DCHECK(!output_queue_->IsStreaming());
  if (!output_queue_->DeallocateBuffers())
    VLOGF(1) << "Failed to deallocate V4L2 output buffers";
}

}  // namespace media
