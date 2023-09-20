// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/vt_video_decode_accelerator_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreVideo/CoreVideo.h>
#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <memory>

#include "base/apple/osstatus_logging.h"
#include "base/atomic_sequence_num.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_policy.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/sys_byteorder.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/version.h"
#include "components/crash/core/common/crash_key.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/ipc/service/shared_image_stub.h"
#include "media/base/limits.h"
#include "media/base/mac/color_space_util_mac.h"
#include "media/base/mac/video_frame_mac.h"
#include "media/base/media_switches.h"
#include "media/base/supported_types.h"
#include "media/filters/vp9_parser.h"
#include "media/gpu/mac/vp9_super_frame_bitstream_filter.h"
#include "media/gpu/mac/vt_config_util.h"
#include "media/video/h264_level_limits.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/scoped_binders.h"

#define NOTIFY_STATUS(name, status, session_failure) \
  do {                                               \
    OSSTATUS_DLOG(ERROR, status) << name;            \
    NotifyError(PLATFORM_FAILURE, session_failure);  \
  } while (0)

namespace media {

namespace {

// Parameter sets vector contain all PPSs/SPSs(/VPSs)
using ParameterSets = std::vector<base::span<const uint8_t>>;

// A sequence of ids for memory tracing.
base::AtomicSequenceNumber g_memory_dump_ids;

// The video codec profiles that are supported.
constexpr VideoCodecProfile kSupportedProfiles[] = {
    H264PROFILE_BASELINE, H264PROFILE_EXTENDED, H264PROFILE_MAIN,
    H264PROFILE_HIGH,

    // These are only supported on macOS 11+.
    VP9PROFILE_PROFILE0, VP9PROFILE_PROFILE2,

    // These are only supported on macOS 11+.
    HEVCPROFILE_MAIN, HEVCPROFILE_MAIN10, HEVCPROFILE_MAIN_STILL_PICTURE,

    // This is partially supported on macOS 11+, Apple Silicon Mac supports
    // 8 ~ 10 bit 400, 420, 422, 444 HW decoding, Intel Mac supports 8 ~ 12
    // bit 400, 420, 422, 444 SW decoding.
    HEVCPROFILE_REXT,

    // TODO(sandersd): Hi10p fails during
    // CMVideoFormatDescriptionCreateFromH264ParameterSets with
    // kCMFormatDescriptionError_InvalidParameter.
    //
    // H264PROFILE_HIGH10PROFILE,

    // TODO(sandersd): Find and test media with these profiles before enabling.
    //
    // H264PROFILE_SCALABLEBASELINE,
    // H264PROFILE_SCALABLEHIGH,
    // H264PROFILE_STEREOHIGH,
    // H264PROFILE_MULTIVIEWHIGH,
};

// Size to use for NALU length headers in AVC format (can be 1, 2, or 4).
constexpr int kNALUHeaderLength = 4;

// We request 16 picture buffers from the client, each of which has a texture ID
// that we can bind decoded frames to. The resource requirements are low, as we
// don't need the textures to be backed by storage.
//
// The lower limit is |limits::kMaxVideoFrames + 1|, enough to have one
// composited frame plus |limits::kMaxVideoFrames| frames to satisfy preroll.
//
// However, there can be pathological behavior where VideoRendererImpl will
// continue to call Decode() as long as it is willing to queue more output
// frames, which is variable but starts at |limits::kMaxVideoFrames +
// GetMaxDecodeRequests()|. If we don't have enough picture buffers, it will
// continue to call Decode() until we stop calling NotifyEndOfBistreamBuffer(),
// which for VTVDA is when the reorder queue is full. In testing this results in
// ~20 extra frames held by VTVDA.
//
// Allocating more picture buffers than VideoRendererImpl is willing to queue
// counterintuitively reduces memory usage in this case.
constexpr int kNumPictureBuffers = limits::kMaxVideoFrames * 4;

// Maximum number of frames to queue for reordering. (Also controls the maximum
// number of in-flight frames, since NotifyEndOfBitstreamBuffer() is called when
// frames are moved into the reorder queue.)
//
// Since the maximum possible |reorder_window| is 16 for H.264, 17 is the
// minimum safe (static) size of the reorder queue.
constexpr int kMaxReorderQueueSize = 17;

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
// If videotoolbox total output picture count is lower than
// kMinOutputsBeforeRASL, then we should skip the RASL frames
// to avoid kVTVideoDecoderBadDataErr
constexpr int kMinOutputsBeforeRASL = 5;
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

// Build an |image_config| dictionary for VideoToolbox initialization.
base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> BuildImageConfig(
    CMVideoDimensions coded_dimensions,
    bool is_hbd,
    bool has_alpha) {
  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> image_config;

  // Note that 4:2:0 textures cannot be used directly as RGBA in OpenGL, but are
  // lower power than 4:2:2 when composited directly by CoreAnimation.
  int32_t pixel_format = is_hbd
                             ? kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange
                             : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
  // macOS support 8 bit (they actually only recommand main profile)
  // HEVC with alpha layer well.
  if (has_alpha)
    pixel_format = kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar;

#define CFINT(i) CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i)
  base::apple::ScopedCFTypeRef<CFNumberRef> cf_pixel_format(
      CFINT(pixel_format));
  base::apple::ScopedCFTypeRef<CFNumberRef> cf_width(
      CFINT(coded_dimensions.width));
  base::apple::ScopedCFTypeRef<CFNumberRef> cf_height(
      CFINT(coded_dimensions.height));
#undef CFINT
  if (!cf_pixel_format.get() || !cf_width.get() || !cf_height.get())
    return image_config;

  image_config.reset(CFDictionaryCreateMutable(
      kCFAllocatorDefault,
      3,  // capacity
      &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
  if (!image_config.get())
    return image_config;

  CFDictionarySetValue(image_config, kCVPixelBufferPixelFormatTypeKey,
                       cf_pixel_format);
  CFDictionarySetValue(image_config, kCVPixelBufferWidthKey, cf_width);
  CFDictionarySetValue(image_config, kCVPixelBufferHeightKey, cf_height);

  return image_config;
}

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
// Create a CMFormatDescription using the provided |param_sets|.
base::apple::ScopedCFTypeRef<CMFormatDescriptionRef> CreateVideoFormatHEVC(
    ParameterSets param_sets) {
  DCHECK(!param_sets.empty());

  // Build the configuration records.
  std::vector<const uint8_t*> nalu_data_ptrs;
  std::vector<size_t> nalu_data_sizes;
  nalu_data_ptrs.reserve(param_sets.size());
  nalu_data_sizes.reserve(param_sets.size());
  for (auto& param : param_sets) {
    nalu_data_ptrs.push_back(param.data());
    nalu_data_sizes.push_back(param.size());
  }

  // For some unknown reason, even if apple has claimed that this API is
  // available after macOS 10.13, however base on the result on macOS 10.15.7,
  // we could get an OSStatus=-12906 kVTCouldNotFindVideoDecoderErr after
  // calling VTDecompressionSessionCreate(), so macOS 11+ is necessary
  // (https://crbug.com/1300444#c9)
  base::apple::ScopedCFTypeRef<CMFormatDescriptionRef> format;
  if (__builtin_available(macOS 11.0, *)) {
    OSStatus status = CMVideoFormatDescriptionCreateFromHEVCParameterSets(
        kCFAllocatorDefault,
        nalu_data_ptrs.size(),   // parameter_set_count
        nalu_data_ptrs.data(),   // &parameter_set_pointers
        nalu_data_sizes.data(),  // &parameter_set_sizes
        kNALUHeaderLength,       // nal_unit_header_length
        NULL, format.InitializeInto());
    OSSTATUS_DLOG_IF(WARNING, status != noErr, status)
        << "CMVideoFormatDescriptionCreateFromHEVCParameterSets()";
  }
  return format;
}
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

// Create a CMFormatDescription using the provided |pps| and |sps|.
base::apple::ScopedCFTypeRef<CMFormatDescriptionRef> CreateVideoFormatH264(
    const std::vector<uint8_t>& sps,
    const std::vector<uint8_t>& spsext,
    const std::vector<uint8_t>& pps) {
  DCHECK(!sps.empty());
  DCHECK(!pps.empty());

  // Build the configuration records.
  std::vector<const uint8_t*> nalu_data_ptrs;
  std::vector<size_t> nalu_data_sizes;
  nalu_data_ptrs.reserve(3);
  nalu_data_sizes.reserve(3);
  nalu_data_ptrs.push_back(&sps.front());
  nalu_data_sizes.push_back(sps.size());
  if (!spsext.empty()) {
    nalu_data_ptrs.push_back(&spsext.front());
    nalu_data_sizes.push_back(spsext.size());
  }
  nalu_data_ptrs.push_back(&pps.front());
  nalu_data_sizes.push_back(pps.size());

  // Construct a new format description from the parameter sets.
  base::apple::ScopedCFTypeRef<CMFormatDescriptionRef> format;
  OSStatus status = CMVideoFormatDescriptionCreateFromH264ParameterSets(
      kCFAllocatorDefault,
      nalu_data_ptrs.size(),     // parameter_set_count
      &nalu_data_ptrs.front(),   // &parameter_set_pointers
      &nalu_data_sizes.front(),  // &parameter_set_sizes
      kNALUHeaderLength,         // nal_unit_header_length
      format.InitializeInto());
  OSSTATUS_DLOG_IF(WARNING, status != noErr, status)
      << "CMVideoFormatDescriptionCreateFromH264ParameterSets()";
  return format;
}

base::apple::ScopedCFTypeRef<CMFormatDescriptionRef> CreateVideoFormatVP9(
    media::VideoColorSpace color_space,
    media::VideoCodecProfile profile,
    absl::optional<gfx::HDRMetadata> hdr_metadata,
    const gfx::Size& coded_size) {
  base::apple::ScopedCFTypeRef<CFDictionaryRef> format_config =
      CreateFormatExtensions(kCMVideoCodecType_VP9, profile, color_space,
                             hdr_metadata);

  base::apple::ScopedCFTypeRef<CMFormatDescriptionRef> format;
  if (!format_config) {
    DLOG(ERROR) << "Failed to configure vp9 decoder.";
    return format;
  }

  OSStatus status = CMVideoFormatDescriptionCreate(
      kCFAllocatorDefault, kCMVideoCodecType_VP9, coded_size.width(),
      coded_size.height(), format_config, format.InitializeInto());
  OSSTATUS_DLOG_IF(WARNING, status != noErr, status)
      << "CMVideoFormatDescriptionCreate()";
  return format;
}

// Create a VTDecompressionSession using the provided |format|. If
// |require_hardware| is true, the session will only use the hardware decoder.
bool CreateVideoToolboxSession(
    const CMFormatDescriptionRef format,
    bool require_hardware,
    bool is_hbd,
    bool has_alpha,
    const VTDecompressionOutputCallbackRecord* callback,
    base::apple::ScopedCFTypeRef<VTDecompressionSessionRef>* session,
    gfx::Size* configured_size) {
  // Prepare VideoToolbox configuration dictionaries.
  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> decoder_config(
      CFDictionaryCreateMutable(kCFAllocatorDefault,
                                1,  // capacity
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  if (!decoder_config) {
    DLOG(ERROR) << "Failed to create CFMutableDictionary";
    return false;
  }

#if BUILDFLAG(IS_MAC)
  // iOS is always hardware-accelerate while on mac, decoder configuration
  // handling is necessary.
  CFDictionarySetValue(
      decoder_config,
      kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder,
      kCFBooleanTrue);
  CFDictionarySetValue(
      decoder_config,
      kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder,
      require_hardware ? kCFBooleanTrue : kCFBooleanFalse);
#endif

  // VideoToolbox scales the visible rect to the output size, so we set the
  // output size for a 1:1 ratio. (Note though that VideoToolbox does not handle
  // top or left crops correctly.) We expect the visible rect to be integral.
  CGRect visible_rect = CMVideoFormatDescriptionGetCleanAperture(format, true);
  CMVideoDimensions visible_dimensions = {
      base::ClampFloor(visible_rect.size.width),
      base::ClampFloor(visible_rect.size.height)};
  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> image_config(
      BuildImageConfig(visible_dimensions, is_hbd, has_alpha));
  if (!image_config) {
    DLOG(ERROR) << "Failed to create decoder image configuration";
    return false;
  }

  OSStatus status = VTDecompressionSessionCreate(
      kCFAllocatorDefault,
      format,          // video_format_description
      decoder_config,  // video_decoder_specification
      image_config,    // destination_image_buffer_attributes
      callback,        // output_callback
      session->InitializeInto());
  if (status != noErr) {
    OSSTATUS_DLOG(WARNING, status) << "VTDecompressionSessionCreate()";
    return false;
  }

  *configured_size =
      gfx::Size(visible_rect.size.width, visible_rect.size.height);

  return true;
}

// TODO(sandersd): Share this computation with the VAAPI decoder.
int32_t ComputeH264ReorderWindow(const H264SPS* sps) {
  // When |pic_order_cnt_type| == 2, decode order always matches presentation
  // order.
  // TODO(sandersd): For |pic_order_cnt_type| == 1, analyze the delta cycle to
  // find the minimum required reorder window.
  if (sps->pic_order_cnt_type == 2)
    return 0;

  int max_dpb_mbs = H264LevelToMaxDpbMbs(sps->GetIndicatedLevel());
  int max_dpb_frames =
      max_dpb_mbs / ((sps->pic_width_in_mbs_minus1 + 1) *
                     (sps->pic_height_in_map_units_minus1 + 1));
  max_dpb_frames = std::clamp(max_dpb_frames, 0, 16);

  // See AVC spec section E.2.1 definition of |max_num_reorder_frames|.
  if (sps->vui_parameters_present_flag && sps->bitstream_restriction_flag) {
    return std::min(sps->max_num_reorder_frames, max_dpb_frames);
  } else if (sps->constraint_set3_flag) {
    if (sps->profile_idc == 44 || sps->profile_idc == 86 ||
        sps->profile_idc == 100 || sps->profile_idc == 110 ||
        sps->profile_idc == 122 || sps->profile_idc == 244) {
      return 0;
    }
  }
  return max_dpb_frames;
}

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
int32_t ComputeHEVCReorderWindow(const H265VPS* vps) {
  int32_t vps_max_sub_layers_minus1 = vps->vps_max_sub_layers_minus1;
  return vps->vps_max_num_reorder_pics[vps_max_sub_layers_minus1];
}
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

// Route decoded frame callbacks back into the VTVideoDecodeAccelerator.
void OutputThunk(void* decompression_output_refcon,
                 void* source_frame_refcon,
                 OSStatus status,
                 VTDecodeInfoFlags info_flags,
                 CVImageBufferRef image_buffer,
                 CMTime presentation_time_stamp,
                 CMTime presentation_duration) {
  VTVideoDecodeAccelerator* vda =
      reinterpret_cast<VTVideoDecodeAccelerator*>(decompression_output_refcon);
  vda->Output(source_frame_refcon, status, image_buffer);
}

gfx::BufferFormat ToBufferFormat(viz::SharedImageFormat format) {
  DCHECK(format.is_multi_plane());
  if (format == viz::MultiPlaneFormat::kYV12) {
    return gfx::BufferFormat::YVU_420;
  }
  if (format == viz::MultiPlaneFormat::kNV12) {
    return gfx::BufferFormat::YUV_420_BIPLANAR;
  }
  if (format == viz::MultiPlaneFormat::kNV12A) {
    return gfx::BufferFormat::YUVA_420_TRIPLANAR;
  }
  if (format == viz::MultiPlaneFormat::kP010) {
    return gfx::BufferFormat::P010;
  }
  NOTREACHED() << "format=" << format.ToString();
  return gfx::BufferFormat::RGBA_8888;
}

}  // namespace

// Detects coded size and color space changes. Also indicates when a frame won't
// generate any output.
class VP9ConfigChangeDetector {
 public:
  VP9ConfigChangeDetector() : vp9_parser_(false) {}
  ~VP9ConfigChangeDetector() = default;

  void DetectConfig(const uint8_t* stream, unsigned int size) {
    vp9_parser_.SetStream(stream, size, nullptr);
    config_changed_ = false;

    Vp9FrameHeader fhdr;
    gfx::Size allocate_size;
    std::unique_ptr<DecryptConfig> null_config;
    while (vp9_parser_.ParseNextFrame(&fhdr, &allocate_size, &null_config) ==
           Vp9Parser::kOk) {
      color_space_ = fhdr.GetColorSpace();

      gfx::Size new_size(fhdr.frame_width, fhdr.frame_height);
      if (!size_.IsEmpty() && !pending_config_changed_ && !config_changed_ &&
          size_ != new_size) {
        pending_config_changed_ = true;
        DVLOG(1) << "Configuration changed from " << size_.ToString() << " to "
                 << new_size.ToString();
      }
      size_ = new_size;

      // Resolution changes can happen on any frame technically, so wait for a
      // keyframe before signaling the config change.
      if (fhdr.IsKeyframe() && pending_config_changed_) {
        config_changed_ = true;
        pending_config_changed_ = false;
      }
    }
    if (pending_config_changed_)
      DVLOG(1) << "Deferring config change until next keyframe...";
  }

  gfx::Size GetCodedSize(const gfx::Size& container_coded_size) const {
    return size_.IsEmpty() ? container_coded_size : size_;
  }

  VideoColorSpace GetColorSpace(const VideoColorSpace& container_cs) const {
    return container_cs.IsSpecified() ? container_cs : color_space_;
  }

  bool config_changed() const { return config_changed_; }

 private:
  gfx::Size size_;
  bool config_changed_ = false;
  bool pending_config_changed_ = false;
  VideoColorSpace color_space_;
  Vp9Parser vp9_parser_;
};

void InitializeVideoToolbox() {
  // InitializeVideoToolbox() is called only from the GPU process main thread:
  // once for sandbox warmup, and then once each time a VTVideoDecodeAccelerator
  // is initialized. This ensures that everything is loaded whether or not the
  // sandbox is enabled.
#if BUILDFLAG(IS_MAC)
  static const bool unused = []() {
    // TODO: Enable VP9 for a iOS platform(https://crbug.com/1449877)
    if (__builtin_available(macOS 11.0, *)) {
      VTRegisterSupplementalVideoDecoderIfAvailable(kCMVideoCodecType_VP9);
    }
    return true;
  }();
  std::ignore = unused;
#endif
}

VTVideoDecodeAccelerator::Task::Task(TaskType type) : type(type) {}

VTVideoDecodeAccelerator::Task::Task(Task&& other) = default;

VTVideoDecodeAccelerator::Task::~Task() {}

VTVideoDecodeAccelerator::Frame::Frame(int32_t bitstream_id)
    : bitstream_id(bitstream_id) {}

VTVideoDecodeAccelerator::Frame::~Frame() {}

VTVideoDecodeAccelerator::PictureInfo::PictureInfo() = default;

VTVideoDecodeAccelerator::PictureInfo::~PictureInfo() {}

bool VTVideoDecodeAccelerator::FrameOrder::operator()(
    const std::unique_ptr<Frame>& lhs,
    const std::unique_ptr<Frame>& rhs) const {
  // TODO(sandersd): When it is provided, use the bitstream timestamp.
  if (lhs->pic_order_cnt != rhs->pic_order_cnt)
    return lhs->pic_order_cnt > rhs->pic_order_cnt;

  // If |pic_order_cnt| is the same, fall back on using the bitstream order.
  return lhs->bitstream_id > rhs->bitstream_id;
}

VTVideoDecodeAccelerator::VTVideoDecodeAccelerator(
    const GpuVideoDecodeGLClient& gl_client,
    const gpu::GpuDriverBugWorkarounds& workarounds,
    MediaLog* media_log)
    : gl_client_(gl_client),
      workarounds_(workarounds),
      // Non media/ use cases like PPAPI may not provide a MediaLog.
      media_log_(media_log ? media_log->Clone() : nullptr),
      gpu_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      decoder_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_VISIBLE})),
      decoder_weak_this_factory_(this),
      weak_this_factory_(this) {
  callback_.decompressionOutputCallback = OutputThunk;
  callback_.decompressionOutputRefCon = this;
  decoder_weak_this_ = decoder_weak_this_factory_.GetWeakPtr();
  weak_this_ = weak_this_factory_.GetWeakPtr();

  memory_dump_id_ = g_memory_dump_ids.GetNext();
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "VTVideoDecodeAccelerator", gpu_task_runner_);
}

VTVideoDecodeAccelerator::~VTVideoDecodeAccelerator() {
  DVLOG(1) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

bool VTVideoDecodeAccelerator::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  // TODO(sandersd): Dump SharedImages for output pictures (decoded frames for
  // which PictureReady() has been called already).

  // Dump the output queue (decoded frames for which
  // NotifyEndOfBitstreamBuffer() has not been called yet).
  {
    uint64_t total_count = 0;
    uint64_t total_size = 0;
    for (const auto& it : base::GetUnderlyingContainer(task_queue_)) {
      if (it.frame.get() && it.frame->image) {
        IOSurfaceRef io_surface = CVPixelBufferGetIOSurface(it.frame->image);
        if (io_surface) {
          ++total_count;
          total_size += IOSurfaceGetAllocSize(io_surface);
        }
      }
    }
    base::trace_event::MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(
        base::StringPrintf("media/vt_video_decode_accelerator_%d/output_queue",
                           memory_dump_id_));
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameObjectCount,
                    base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                    total_count);
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    total_size);
  }

  // Dump the reorder queue (decoded frames for which
  // NotifyEndOfBitstreamBuffer() has been called already).
  {
    uint64_t total_count = 0;
    uint64_t total_size = 0;
    for (const auto& it : base::GetUnderlyingContainer(reorder_queue_)) {
      if (it.get() && it->image) {
        IOSurfaceRef io_surface = CVPixelBufferGetIOSurface(it->image);
        if (io_surface) {
          ++total_count;
          total_size += IOSurfaceGetAllocSize(io_surface);
        }
      }
    }
    base::trace_event::MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(
        base::StringPrintf("media/vt_video_decode_accelerator_%d/reorder_queue",
                           memory_dump_id_));
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameObjectCount,
                    base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                    total_count);
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    total_size);
  }

  return true;
}

bool VTVideoDecodeAccelerator::Initialize(const Config& config,
                                          Client* client) {
  DVLOG(1) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  // All of these checks should be handled by the caller inspecting
  // SupportedProfiles(). PPAPI does not do that, however.
  if (config.output_mode != Config::OutputMode::ALLOCATE) {
    DVLOG(2) << "Output mode must be ALLOCATE";
    return false;
  }

  if (config.is_encrypted()) {
    DVLOG(2) << "Encrypted streams are not supported";
    return false;
  }

  codec_ = VideoCodecProfileToVideoCodec(config.profile);

  // If we don't have support support for a given codec, try to initialize
  // anyways -- otherwise we're certain to fail playback.
  static const base::NoDestructor<VideoDecodeAccelerator::SupportedProfiles>
      kActualSupportedProfiles(GetSupportedProfiles(workarounds_));
  if (!base::Contains(*kActualSupportedProfiles, config.profile,
                      &VideoDecodeAccelerator::SupportedProfile::profile) &&
      IsBuiltInVideoCodec(codec_)) {
    DVLOG(2) << "Unsupported profile";
    return false;
  }

  InitializeVideoToolbox();

  client_ = client;
  config_ = config;

  // Count the session as successfully initialized.
  UMA_HISTOGRAM_ENUMERATION("Media.VTVDA.SessionFailureReason",
                            SFT_SUCCESSFULLY_INITIALIZED, SFT_MAX + 1);
  return true;
}

bool VTVideoDecodeAccelerator::FinishDelayedFrames() {
  DVLOG(3) << __func__;
  DCHECK(decoder_task_runner_->RunsTasksInCurrentSequence());
  if (session_) {
    OSStatus status = VTDecompressionSessionWaitForAsynchronousFrames(session_);
    output_count_for_cra_rasl_workaround_ = 0;
    if (status) {
      NOTIFY_STATUS("VTDecompressionSessionWaitForAsynchronousFrames()", status,
                    SFT_PLATFORM_ERROR);
      return false;
    }
  }
  return true;
}

bool VTVideoDecodeAccelerator::ConfigureDecoder() {
  DVLOG(2) << __func__;
  DCHECK(decoder_task_runner_->RunsTasksInCurrentSequence());

  base::apple::ScopedCFTypeRef<CMFormatDescriptionRef> format;
  switch (codec_) {
    case VideoCodec::kH264:
      format = CreateVideoFormatH264(active_sps_, active_spsext_, active_pps_);
      break;
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    case VideoCodec::kHEVC: {
      ParameterSets param_sets;
      for (auto& it : seen_vps_)
        param_sets.push_back(it.second);
      for (auto& it : seen_sps_)
        param_sets.push_back(it.second);
      for (auto& it : seen_pps_)
        param_sets.push_back(it.second);
      format = CreateVideoFormatHEVC(param_sets);
      break;
    }
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    case VideoCodec::kVP9:
      format = CreateVideoFormatVP9(
          cc_detector_->GetColorSpace(config_.container_color_space),
          config_.profile, config_.hdr_metadata,
          cc_detector_->GetCodedSize(config_.initial_expected_coded_size));
      break;
    default:
      // We can reach this case for non-built-in codecs.
      break;
  }

  if (!format) {
    NotifyError(PLATFORM_FAILURE, SFT_PLATFORM_ERROR);
    return false;
  }

  // TODO(crbug.com/1103432): We should use
  // VTDecompressionSessionCanAcceptFormatDescription() on |format| here to
  // avoid the configuration change if possible.

  // Ensure that the old decoder emits all frames before the new decoder can
  // emit any.
  if (!FinishDelayedFrames())
    return false;

  format_ = format;
  session_.reset();

  // Note: We can always require hardware once Flash and PPAPI are gone.
  // however for HEVC, do not use require hardware to avoid
  // kVTVideoDecoderUnsupportedDataFormatErr(-12910) failure,
  // hardware acceleration may be unavailable for a number of reasons,
  // so just enable hardware and let vt choose whether to use
  // hardware of software decode
  const bool require_hardware = config_.profile == VP9PROFILE_PROFILE0 ||
                                config_.profile == VP9PROFILE_PROFILE2;
  const bool is_hbd = config_.profile == VP9PROFILE_PROFILE2 ||
                      config_.profile == HEVCPROFILE_MAIN10 ||
                      config_.profile == HEVCPROFILE_REXT;
  if (!CreateVideoToolboxSession(format_, require_hardware, is_hbd, has_alpha_,
                                 &callback_, &session_, &configured_size_)) {
    NotifyError(PLATFORM_FAILURE, SFT_PLATFORM_ERROR);
    return false;
  }

  // Report whether hardware decode is being used.
  bool using_hardware = false;
  base::apple::ScopedCFTypeRef<CFBooleanRef> cf_using_hardware;
  if (VTSessionCopyProperty(
          session_,
          // kVTDecompressionPropertyKey_UsingHardwareAcceleratedVideoDecoder
          CFSTR("UsingHardwareAcceleratedVideoDecoder"), kCFAllocatorDefault,
          cf_using_hardware.InitializeInto()) == 0) {
    using_hardware = CFBooleanGetValue(cf_using_hardware);
  }
  UMA_HISTOGRAM_BOOLEAN("Media.VTVDA.HardwareAccelerated", using_hardware);

  if (codec_ == VideoCodec::kVP9 && !vp9_bsf_)
    vp9_bsf_ = std::make_unique<VP9SuperFrameBitstreamFilter>();

  // Record that the configuration change is complete.
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  // Actually seen vps/sps/pps may contain outdated parameter
  // sets, VideoToolbox perhaps can handle this well since those
  // outdated ones are not referenced by current pictures.
  // Let's see what will happens in this way.
  configured_vpss_ = seen_vps_;
  configured_spss_ = seen_sps_;
  configured_ppss_ = seen_pps_;
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  configured_sps_ = active_sps_;
  configured_spsext_ = active_spsext_;
  configured_pps_ = active_pps_;
  return true;
}

void VTVideoDecodeAccelerator::DecodeTaskVp9(
    scoped_refptr<DecoderBuffer> buffer,
    Frame* frame) {
  DVLOG(2) << __func__ << ": bit_stream=" << frame->bitstream_id
           << ", buffer=" << buffer->AsHumanReadableString();
  DCHECK(decoder_task_runner_->RunsTasksInCurrentSequence());

  if (!cc_detector_)
    cc_detector_ = std::make_unique<VP9ConfigChangeDetector>();
  cc_detector_->DetectConfig(buffer->data(), buffer->data_size());

  if (!session_ || cc_detector_->config_changed()) {
    // ConfigureDecoder() calls NotifyError() on failure.
    if (!ConfigureDecoder())
      return;
  }

  // Now that the configuration is up to date, copy it into the frame.
  frame->image_size = configured_size_;

  if (!vp9_bsf_->EnqueueBuffer(std::move(buffer))) {
    WriteToMediaLog(MediaLogMessageLevel::kERROR, "Unsupported VP9 stream");
    NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
    return;
  }

  // If we have no buffer this bitstream buffer is part of a super frame that we
  // need to assemble before giving to VideoToolbox.
  auto data = vp9_bsf_->take_buffer();
  if (!data) {
    gpu_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VTVideoDecodeAccelerator::DecodeDone,
                                  weak_this_, frame));
    return;
  }

  // Package the data in a CMSampleBuffer.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample;
  OSStatus status = CMSampleBufferCreateReady(kCFAllocatorDefault,
                                              data,     // data_buffer
                                              format_,  // format_description
                                              1,        // num_samples
                                              0,  // num_sample_timing_entries
                                              nullptr,  // &sample_timing_array
                                              0,  // num_sample_size_entries
                                              nullptr,  // &sample_size_array
                                              sample.InitializeInto());
  if (status) {
    NOTIFY_STATUS("CMSampleBufferCreate()", status, SFT_PLATFORM_ERROR);
    return;
  }

  // Send the frame for decoding.
  // Asynchronous Decompression allows for parallel submission of frames
  // (without it, DecodeFrame() does not return until the frame has been
  // decoded). We don't enable Temporal Processing because we are not passing
  // timestamps anyway.
  VTDecodeFrameFlags decode_flags =
      kVTDecodeFrame_EnableAsynchronousDecompression;
  status = VTDecompressionSessionDecodeFrame(
      session_,
      sample,                          // sample_buffer
      decode_flags,                    // decode_flags
      reinterpret_cast<void*>(frame),  // source_frame_refcon
      nullptr);                        // &info_flags_out
  if (status) {
    NOTIFY_STATUS("VTDecompressionSessionDecodeFrame()", status,
                  SFT_DECODE_ERROR);
    return;
  }
}

void VTVideoDecodeAccelerator::DecodeTaskH264(
    scoped_refptr<DecoderBuffer> buffer,
    Frame* frame) {
  DVLOG(2) << __func__ << "(" << frame->bitstream_id << ")";
  DCHECK(decoder_task_runner_->RunsTasksInCurrentSequence());

  // NALUs are stored with Annex B format in the bitstream buffer (start codes),
  // but VideoToolbox expects AVC format (length headers), so we must rewrite
  // the data.
  //
  // Locate relevant NALUs and compute the size of the rewritten data. Also
  // record parameter sets for VideoToolbox initialization.
  size_t data_size = 0;
  std::vector<H264NALU> nalus;
  size_t first_slice_index = 0;
  h264_parser_.SetStream(buffer->data(), buffer->data_size());
  H264NALU nalu;
  while (true) {
    H264Parser::Result result = h264_parser_.AdvanceToNextNALU(&nalu);
    if (result == H264Parser::kEOStream)
      break;
    if (result == H264Parser::kUnsupportedStream) {
      WriteToMediaLog(MediaLogMessageLevel::kERROR, "Unsupported H.264 stream");
      NotifyError(PLATFORM_FAILURE, SFT_UNSUPPORTED_STREAM);
      return;
    }
    if (result != H264Parser::kOk) {
      WriteToMediaLog(MediaLogMessageLevel::kERROR,
                      "Failed to parse H.264 stream");
      NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
      return;
    }
    switch (nalu.nal_unit_type) {
      case H264NALU::kSPS: {
        int sps_id = -1;
        result = h264_parser_.ParseSPS(&sps_id);
        if (result == H264Parser::kUnsupportedStream) {
          WriteToMediaLog(MediaLogMessageLevel::kERROR, "Unsupported SPS");
          NotifyError(PLATFORM_FAILURE, SFT_UNSUPPORTED_STREAM);
          return;
        }
        if (result != H264Parser::kOk) {
          WriteToMediaLog(MediaLogMessageLevel::kERROR, "Could not parse SPS");
          NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
          return;
        }
        seen_sps_[sps_id].assign(nalu.data, nalu.data + nalu.size);
        seen_spsext_.erase(sps_id);
        break;
      }

      case H264NALU::kSPSExt: {
        int sps_id = -1;
        result = h264_parser_.ParseSPSExt(&sps_id);
        if (result != H264Parser::kOk) {
          WriteToMediaLog(MediaLogMessageLevel::kERROR,
                          "Could not parse SPS extension");
          NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
          return;
        }
        seen_spsext_[sps_id].assign(nalu.data, nalu.data + nalu.size);
        break;
      }

      case H264NALU::kPPS: {
        int pps_id = -1;
        result = h264_parser_.ParsePPS(&pps_id);
        if (result == H264Parser::kUnsupportedStream) {
          WriteToMediaLog(MediaLogMessageLevel::kERROR, "Unsupported PPS");
          NotifyError(PLATFORM_FAILURE, SFT_UNSUPPORTED_STREAM);
          return;
        }
        if (result != H264Parser::kOk) {
          WriteToMediaLog(MediaLogMessageLevel::kERROR, "Could not parse PPS");
          NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
          return;
        }
        seen_pps_[pps_id].assign(nalu.data, nalu.data + nalu.size);
        break;
      }

      case H264NALU::kSEIMessage: {
        nalus.push_back(nalu);
        data_size += kNALUHeaderLength + nalu.size;
        H264SEI sei;
        result = h264_parser_.ParseSEI(&sei);
        if (result != H264Parser::kOk)
          break;
        for (auto& sei_msg : sei.msgs) {
          switch (sei_msg.type) {
            case H264SEIMessage::kSEIRecoveryPoint:
              if (sei_msg.recovery_point.recovery_frame_cnt == 0) {
                // We only support immediate recovery points. Supporting
                // future points would require dropping |recovery_frame_cnt|
                // frames when needed.
                frame->has_recovery_point = true;
              }
              break;
            case H264SEIMessage::kSEIMasteringDisplayInfo:
              if (!config_.hdr_metadata) {
                config_.hdr_metadata = gfx::HDRMetadata();
              }
              config_.hdr_metadata->smpte_st_2086 =
                  sei_msg.mastering_display_info.ToGfx();
              break;
            case H264SEIMessage::kSEIContentLightLevelInfo:
              if (!config_.hdr_metadata) {
                config_.hdr_metadata = gfx::HDRMetadata();
              }
              config_.hdr_metadata->cta_861_3 =
                  sei_msg.content_light_level_info.ToGfx();
              break;
            default:
              break;
          }
        }
        break;
      }

      case H264NALU::kSliceDataA:
      case H264NALU::kSliceDataB:
      case H264NALU::kSliceDataC:
      case H264NALU::kNonIDRSlice:
      case H264NALU::kIDRSlice:
        // Only the first slice is examined. Other slices are at least one of:
        // the same frame, not decoded, invalid.
        if (!frame->has_slice) {
          // Parse slice header.
          H264SliceHeader slice_hdr;
          result = h264_parser_.ParseSliceHeader(nalu, &slice_hdr);
          if (result == H264Parser::kUnsupportedStream) {
            WriteToMediaLog(MediaLogMessageLevel::kERROR,
                            "Unsupported slice header");
            NotifyError(PLATFORM_FAILURE, SFT_UNSUPPORTED_STREAM);
            return;
          }
          if (result != H264Parser::kOk) {
            WriteToMediaLog(MediaLogMessageLevel::kERROR,
                            "Could not parse slice header");
            NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
            return;
          }

          // Lookup SPS and PPS.
          const H264PPS* pps =
              h264_parser_.GetPPS(slice_hdr.pic_parameter_set_id);
          if (!pps) {
            WriteToMediaLog(MediaLogMessageLevel::kERROR,
                            "Missing PPS referenced by slice");
            NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
            return;
          }

          const H264SPS* sps = h264_parser_.GetSPS(pps->seq_parameter_set_id);
          if (!sps) {
            WriteToMediaLog(MediaLogMessageLevel::kERROR,
                            "Missing SPS referenced by PPS");
            NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
            return;
          }

          // Record the configuration.
          DCHECK(seen_pps_.contains(slice_hdr.pic_parameter_set_id));
          DCHECK(seen_sps_.contains(pps->seq_parameter_set_id));
          active_sps_ = seen_sps_[pps->seq_parameter_set_id];
          // Note: SPS extension lookup may create an empty entry.
          active_spsext_ = seen_spsext_[pps->seq_parameter_set_id];
          active_pps_ = seen_pps_[slice_hdr.pic_parameter_set_id];

          // Compute and store frame properties. |image_size| gets filled in
          // later, since it comes from the decoder configuration.
          absl::optional<int32_t> pic_order_cnt =
              h264_poc_.ComputePicOrderCnt(sps, slice_hdr);
          if (!pic_order_cnt.has_value()) {
            WriteToMediaLog(MediaLogMessageLevel::kERROR,
                            "Unable to compute POC");
            NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
            return;
          }

          frame->has_slice = true;
          frame->is_idr = nalu.nal_unit_type == media::H264NALU::kIDRSlice;
          frame->has_mmco5 = h264_poc_.IsPendingMMCO5();
          frame->pic_order_cnt = *pic_order_cnt;
          frame->reorder_window = ComputeH264ReorderWindow(sps);

          first_slice_index = nalus.size();
        }
        nalus.push_back(nalu);
        data_size += kNALUHeaderLength + nalu.size;
        break;

      default:
        nalus.push_back(nalu);
        data_size += kNALUHeaderLength + nalu.size;
        break;
    }
  }

  if (frame->is_idr || frame->has_recovery_point)
    waiting_for_idr_ = false;
  frame->hdr_metadata = config_.hdr_metadata;

  // If no IDR has been seen yet, skip decoding. Note that Flash sends
  // configuration changes as a bitstream with only SPS/PPS; we don't print
  // error messages for those.
  if (frame->has_slice && waiting_for_idr_) {
    if (!missing_idr_logged_) {
      WriteToMediaLog(MediaLogMessageLevel::kERROR,
                      ("Illegal attempt to decode without IDR. "
                       "Discarding decode requests until the next IDR."));
      missing_idr_logged_ = true;
    }
    frame->has_slice = false;
  }

  // If there is nothing to decode, drop the request by returning a frame with
  // no image.
  if (!frame->has_slice) {
    gpu_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VTVideoDecodeAccelerator::DecodeDone,
                                  weak_this_, frame));
    return;
  }

  // If the configuration has changed, and we're at an IDR, reconfigure the
  // decoding session. Otherwise insert the parameter sets and hope for the
  // best.
  if (configured_sps_ != active_sps_ || configured_spsext_ != active_spsext_ ||
      configured_pps_ != active_pps_) {
    if (frame->is_idr) {
      if (active_sps_.empty()) {
        WriteToMediaLog(MediaLogMessageLevel::kERROR,
                        "Invalid configuration (no SPS)");
        NotifyError(INVALID_ARGUMENT, SFT_INVALID_STREAM);
        return;
      }
      if (active_pps_.empty()) {
        WriteToMediaLog(MediaLogMessageLevel::kERROR,
                        "Invalid configuration (no PPS)");
        NotifyError(INVALID_ARGUMENT, SFT_INVALID_STREAM);
        return;
      }

      // ConfigureDecoder() calls NotifyError() on failure.
      if (!ConfigureDecoder())
        return;
    } else {
      // Only |data| and |size| are read later, other fields are left empty.
      // In case that their are new PPS/SPS/SPSext appears after an IDR, or
      // videos that have multiple PPSs and we are referring to the one that
      // is not used to create video format.
      media::H264NALU sps_nalu;
      sps_nalu.data = active_sps_.data();
      sps_nalu.size = active_sps_.size();
      nalus.insert(nalus.begin() + first_slice_index, sps_nalu);
      data_size += kNALUHeaderLength + sps_nalu.size;
      first_slice_index += 1;

      if (active_spsext_.size()) {
        media::H264NALU spsext_nalu;
        spsext_nalu.data = active_spsext_.data();
        spsext_nalu.size = active_spsext_.size();
        nalus.insert(nalus.begin() + first_slice_index, spsext_nalu);
        data_size += kNALUHeaderLength + spsext_nalu.size;
        first_slice_index += 1;
      }

      media::H264NALU pps_nalu;
      pps_nalu.data = active_pps_.data();
      pps_nalu.size = active_pps_.size();
      nalus.insert(nalus.begin() + first_slice_index, pps_nalu);
      data_size += kNALUHeaderLength + pps_nalu.size;
      first_slice_index += 1;

      // Update the configured SPS/SPSext/PPS in case VT referrence to the wrong
      // parameter sets.
      configured_sps_ = active_sps_;
      configured_spsext_ = active_spsext_;
      configured_pps_ = active_pps_;
    }
  }

  // If the session is not configured by this point, fail.
  if (!session_) {
    WriteToMediaLog(MediaLogMessageLevel::kERROR,
                    "Cannot decode without configuration");
    NotifyError(INVALID_ARGUMENT, SFT_INVALID_STREAM);
    return;
  }

  // Now that the configuration is up to date, copy it into the frame.
  frame->image_size = configured_size_;

  // Create a memory-backed CMBlockBuffer for the translated data.
  // TODO(sandersd): Pool of memory blocks.
  base::apple::ScopedCFTypeRef<CMBlockBufferRef> data;
  OSStatus status = CMBlockBufferCreateWithMemoryBlock(
      kCFAllocatorDefault,
      nullptr,              // &memory_block
      data_size,            // block_length
      kCFAllocatorDefault,  // block_allocator
      nullptr,              // &custom_block_source
      0,                    // offset_to_data
      data_size,            // data_length
      0,                    // flags
      data.InitializeInto());
  if (status) {
    NOTIFY_STATUS("CMBlockBufferCreateWithMemoryBlock()", status,
                  SFT_PLATFORM_ERROR);
    return;
  }

  // Make sure that the memory is actually allocated.
  // CMBlockBufferReplaceDataBytes() is documented to do this, but prints a
  // message each time starting in Mac OS X 10.10.
  status = CMBlockBufferAssureBlockMemory(data);
  if (status) {
    NOTIFY_STATUS("CMBlockBufferAssureBlockMemory()", status,
                  SFT_PLATFORM_ERROR);
    return;
  }

  // Copy NALU data into the CMBlockBuffer, inserting length headers.
  size_t offset = 0;
  for (size_t i = 0; i < nalus.size(); i++) {
    H264NALU& nalu_ref = nalus[i];
    uint32_t header = base::HostToNet32(static_cast<uint32_t>(nalu_ref.size));
    status =
        CMBlockBufferReplaceDataBytes(&header, data, offset, kNALUHeaderLength);
    if (status) {
      NOTIFY_STATUS("CMBlockBufferReplaceDataBytes()", status,
                    SFT_PLATFORM_ERROR);
      return;
    }
    offset += kNALUHeaderLength;
    status = CMBlockBufferReplaceDataBytes(nalu_ref.data, data, offset,
                                           nalu_ref.size);
    if (status) {
      NOTIFY_STATUS("CMBlockBufferReplaceDataBytes()", status,
                    SFT_PLATFORM_ERROR);
      return;
    }
    offset += nalu_ref.size;
  }

  // Package the data in a CMSampleBuffer.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample;
  status = CMSampleBufferCreate(kCFAllocatorDefault,
                                data,        // data_buffer
                                true,        // data_ready
                                nullptr,     // make_data_ready_callback
                                nullptr,     // make_data_ready_refcon
                                format_,     // format_description
                                1,           // num_samples
                                0,           // num_sample_timing_entries
                                nullptr,     // &sample_timing_array
                                1,           // num_sample_size_entries
                                &data_size,  // &sample_size_array
                                sample.InitializeInto());
  if (status) {
    NOTIFY_STATUS("CMSampleBufferCreate()", status, SFT_PLATFORM_ERROR);
    return;
  }

  // Send the frame for decoding.
  // Asynchronous Decompression allows for parallel submission of frames
  // (without it, DecodeFrame() does not return until the frame has been
  // decoded). We don't enable Temporal Processing because we are not passing
  // timestamps anyway.
  VTDecodeFrameFlags decode_flags =
      kVTDecodeFrame_EnableAsynchronousDecompression;
  status = VTDecompressionSessionDecodeFrame(
      session_,
      sample,                          // sample_buffer
      decode_flags,                    // decode_flags
      reinterpret_cast<void*>(frame),  // source_frame_refcon
      nullptr);                        // &info_flags_out
  if (status) {
    NOTIFY_STATUS("VTDecompressionSessionDecodeFrame()", status,
                  SFT_DECODE_ERROR);
    return;
  }
}

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
void VTVideoDecodeAccelerator::DecodeTaskHEVC(
    scoped_refptr<DecoderBuffer> buffer,
    Frame* frame) {
  DCHECK(decoder_task_runner_->RunsTasksInCurrentSequence());

  size_t data_size = 0;
  std::vector<H265NALU> nalus;
  // store the previous slice header to allow us recover the current
  // from the previous one if dependent_slice_segment_flag exists
  std::unique_ptr<H265SliceHeader> curr_slice_hdr;
  std::unique_ptr<H265SliceHeader> last_slice_hdr;
  size_t first_slice_index = 0;
  // ID of the VPS/SPS/PPS that most recently activated by an IDR.
  int active_vps_id = 0;
  int active_sps_id = 0;
  int active_pps_id = 0;
  hevc_parser_.SetStream(buffer->data(), buffer->data_size());
  H265NALU nalu;
  while (true) {
    H265Parser::Result result = hevc_parser_.AdvanceToNextNALU(&nalu);
    if (result == H265Parser::kEOStream) {
      last_slice_hdr.reset();
      break;
    }

    if (result == H265Parser::kUnsupportedStream) {
      WriteToMediaLog(MediaLogMessageLevel::kERROR, "Unsupported H.265 stream");
      NotifyError(PLATFORM_FAILURE, SFT_UNSUPPORTED_STREAM);
      return;
    }

    if (result != H265Parser::kOk) {
      WriteToMediaLog(MediaLogMessageLevel::kERROR,
                      "Failed to parse H.265 stream");
      NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
      return;
    }

    switch (nalu.nal_unit_type) {
      case H265NALU::SPS_NUT: {
        int sps_id = -1;
        result = hevc_parser_.ParseSPS(&sps_id);
        if (result == H265Parser::kUnsupportedStream) {
          WriteToMediaLog(MediaLogMessageLevel::kERROR, "Unsupported SPS");
          NotifyError(PLATFORM_FAILURE, SFT_UNSUPPORTED_STREAM);
          return;
        }
        if (result != H265Parser::kOk) {
          WriteToMediaLog(MediaLogMessageLevel::kERROR, "Could not parse SPS");
          NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
          return;
        }
        seen_sps_[sps_id].assign(nalu.data, nalu.data + nalu.size);
        break;
      }

      case H265NALU::PPS_NUT: {
        int pps_id = -1;
        result = hevc_parser_.ParsePPS(nalu, &pps_id);
        if (result == H265Parser::kUnsupportedStream) {
          WriteToMediaLog(MediaLogMessageLevel::kERROR, "Unsupported PPS");
          NotifyError(PLATFORM_FAILURE, SFT_UNSUPPORTED_STREAM);
          return;
        }
        if (result == H265Parser::kMissingParameterSet) {
          WriteToMediaLog(MediaLogMessageLevel::kERROR,
                          "Missing SPS from what was parsed");
          NotifyError(PLATFORM_FAILURE, SFT_INVALID_STREAM);
          return;
        }
        if (result != H265Parser::kOk) {
          WriteToMediaLog(MediaLogMessageLevel::kERROR, "Could not parse PPS");
          NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
          return;
        }
        seen_pps_[pps_id].assign(nalu.data, nalu.data + nalu.size);
        break;
      }

      case H265NALU::VPS_NUT: {
        int vps_id = -1;
        result = hevc_parser_.ParseVPS(&vps_id);
        if (result == H265Parser::kUnsupportedStream) {
          WriteToMediaLog(MediaLogMessageLevel::kERROR, "Unsupported VPS");
          NotifyError(PLATFORM_FAILURE, SFT_UNSUPPORTED_STREAM);
          return;
        }
        if (result != H265Parser::kOk) {
          WriteToMediaLog(MediaLogMessageLevel::kERROR, "Could not parse VPS");
          NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
          return;
        }
        seen_vps_[vps_id].assign(nalu.data, nalu.data + nalu.size);
        break;
      }

      case H265NALU::PREFIX_SEI_NUT: {
        nalus.push_back(nalu);
        data_size += kNALUHeaderLength + nalu.size;
        H265SEI sei;
        result = hevc_parser_.ParseSEI(&sei);
        if (result != H265Parser::kOk)
          break;
        for (auto& sei_msg : sei.msgs) {
          switch (sei_msg.type) {
            case H265SEIMessage::kSEIAlphaChannelInfo:
              has_alpha_ =
                  sei_msg.alpha_channel_info.alpha_channel_cancel_flag == 0;
              break;
            case H265SEIMessage::kSEIMasteringDisplayInfo:
              if (!config_.hdr_metadata.has_value()) {
                config_.hdr_metadata.emplace();
              }
              config_.hdr_metadata->smpte_st_2086 =
                  sei_msg.mastering_display_info.ToGfx();
              break;
            case H265SEIMessage::kSEIContentLightLevelInfo:
              if (!config_.hdr_metadata.has_value()) {
                config_.hdr_metadata.emplace();
              }
              config_.hdr_metadata->cta_861_3 =
                  sei_msg.content_light_level_info.ToGfx();
              break;
            default:
              break;
          }
        }
        break;
      }

      case H265NALU::EOS_NUT:
        hevc_poc_.Reset();
        nalus.push_back(nalu);
        data_size += kNALUHeaderLength + nalu.size;
        break;
      case H265NALU::BLA_W_LP:
      case H265NALU::BLA_W_RADL:
      case H265NALU::BLA_N_LP:
      case H265NALU::IDR_W_RADL:
      case H265NALU::IDR_N_LP:
      case H265NALU::TRAIL_N:
      case H265NALU::TRAIL_R:
      case H265NALU::TSA_N:
      case H265NALU::TSA_R:
      case H265NALU::STSA_N:
      case H265NALU::STSA_R:
      case H265NALU::RADL_N:
      case H265NALU::RADL_R:
      case H265NALU::RASL_N:
      case H265NALU::RASL_R:
      case H265NALU::CRA_NUT: {
        // The VT session will report a OsStatus=12909 kVTVideoDecoderBadDataErr
        // if you send a RASL frame just after a CRA frame, so we wait until the
        // total output count is enough
        if (output_count_for_cra_rasl_workaround_ < kMinOutputsBeforeRASL &&
            (nalu.nal_unit_type == H265NALU::RASL_N ||
             nalu.nal_unit_type == H265NALU::RASL_R)) {
          continue;
        }
        // Just like H264, only the first slice is examined. Other slices are at
        // least one of: the same frame, not decoded, invalid so no need to
        // parse again.
        if (frame->has_slice) {
          nalus.push_back(nalu);
          data_size += kNALUHeaderLength + nalu.size;
          break;
        }
        curr_slice_hdr.reset(new H265SliceHeader());
        result = hevc_parser_.ParseSliceHeader(nalu, curr_slice_hdr.get(),
                                               last_slice_hdr.get());

        if (result == H265Parser::kMissingParameterSet) {
          curr_slice_hdr.reset();
          last_slice_hdr.reset();
          WriteToMediaLog(MediaLogMessageLevel::kERROR,
                          "Missing PPS when parsing slice header");
          continue;
        }

        if (result != H265Parser::kOk) {
          curr_slice_hdr.reset();
          last_slice_hdr.reset();
          WriteToMediaLog(MediaLogMessageLevel::kERROR,
                          "Could not parse slice header");
          NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
          return;
        }

        const H265PPS* pps =
            hevc_parser_.GetPPS(curr_slice_hdr->slice_pic_parameter_set_id);
        if (!pps) {
          WriteToMediaLog(MediaLogMessageLevel::kERROR,
                          "Missing PPS referenced by slice");
          NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
          return;
        }

        const H265SPS* sps = hevc_parser_.GetSPS(pps->pps_seq_parameter_set_id);
        if (!sps) {
          WriteToMediaLog(MediaLogMessageLevel::kERROR,
                          "Missing SPS referenced by PPS");
          NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
          return;
        }

        const H265VPS* vps =
            hevc_parser_.GetVPS(sps->sps_video_parameter_set_id);
        if (!vps) {
          WriteToMediaLog(MediaLogMessageLevel::kERROR,
                          "Missing VPS referenced by SPS");
          NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
          return;
        }

        // Record the configuration.
        active_vps_id = sps->sps_video_parameter_set_id;
        active_sps_id = pps->pps_seq_parameter_set_id;
        active_pps_id = curr_slice_hdr->slice_pic_parameter_set_id;
        DCHECK(seen_vps_.contains(active_vps_id));
        DCHECK(seen_sps_.contains(active_sps_id));
        DCHECK(seen_pps_.contains(active_pps_id));
        active_vps_ = seen_vps_[active_vps_id];
        active_sps_ = seen_sps_[active_sps_id];
        active_pps_ = seen_pps_[active_pps_id];

        // Compute and store frame properties. |image_size| gets filled in
        // later, since it comes from the decoder configuration.
        int32_t pic_order_cnt =
            hevc_poc_.ComputePicOrderCnt(sps, pps, *curr_slice_hdr.get());

        frame->has_slice = true;
        frame->is_idr = nalu.nal_unit_type >= H265NALU::BLA_W_LP &&
                        nalu.nal_unit_type <= H265NALU::RSV_IRAP_VCL23;
        frame->pic_order_cnt = pic_order_cnt;
        frame->reorder_window = ComputeHEVCReorderWindow(vps);

        first_slice_index = nalus.size();

        last_slice_hdr.swap(curr_slice_hdr);
        curr_slice_hdr.reset();
        [[fallthrough]];
      }
      default:
        nalus.push_back(nalu);
        data_size += kNALUHeaderLength + nalu.size;
        break;
    }
  }

  if (frame->is_idr)
    waiting_for_idr_ = false;
  frame->hdr_metadata = config_.hdr_metadata;

  // If no IDR has been seen yet, skip decoding. Note that Flash sends
  // configuration changes as a bitstream with only SPS/PPS/VPS; we don't print
  // error messages for those.
  if (frame->has_slice && waiting_for_idr_) {
    if (!missing_idr_logged_) {
      WriteToMediaLog(MediaLogMessageLevel::kERROR,
                      ("Illegal attempt to decode without IDR. "
                       "Discarding decode requests until the next IDR."));
      missing_idr_logged_ = true;
    }
    frame->has_slice = false;
  }

  // If there is nothing to decode, drop the request by returning a frame with
  // no image.
  if (!frame->has_slice) {
    gpu_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VTVideoDecodeAccelerator::DecodeDone,
                                  weak_this_, frame));
    return;
  }

  // Apply any configuration change, but only at an IDR. If there is no IDR, we
  // just hope for the best from the decoder.
  if (seen_vps_ != configured_vpss_ || seen_sps_ != configured_spss_ ||
      seen_pps_ != configured_ppss_) {
    if (frame->is_idr) {
      if (seen_vps_.empty()) {
        WriteToMediaLog(MediaLogMessageLevel::kERROR,
                        "Invalid configuration (no VPS)");
        NotifyError(INVALID_ARGUMENT, SFT_INVALID_STREAM);
        return;
      }
      if (seen_sps_.empty()) {
        WriteToMediaLog(MediaLogMessageLevel::kERROR,
                        "Invalid configuration (no SPS)");
        NotifyError(INVALID_ARGUMENT, SFT_INVALID_STREAM);
        return;
      }
      if (seen_pps_.empty()) {
        WriteToMediaLog(MediaLogMessageLevel::kERROR,
                        "Invalid configuration (no PPS)");
        NotifyError(INVALID_ARGUMENT, SFT_INVALID_STREAM);
        return;
      }

      // ConfigureDecoder() calls NotifyError() on failure.
      if (!ConfigureDecoder()) {
        return;
      }
    } else {
      // Only |data| and |size| are read later, other fields are left empty.
      // In case that their are new VPS/SPS/PPS appears after an IDR.
      media::H265NALU vps_nalu;
      vps_nalu.data = active_vps_.data();
      vps_nalu.size = active_vps_.size();
      nalus.insert(nalus.begin() + first_slice_index, vps_nalu);
      data_size += kNALUHeaderLength + vps_nalu.size;
      first_slice_index += 1;

      media::H265NALU sps_nalu;
      sps_nalu.data = active_sps_.data();
      sps_nalu.size = active_sps_.size();
      nalus.insert(nalus.begin() + first_slice_index, sps_nalu);
      data_size += kNALUHeaderLength + sps_nalu.size;
      first_slice_index += 1;

      media::H265NALU pps_nalu;
      pps_nalu.data = active_pps_.data();
      pps_nalu.size = active_pps_.size();
      nalus.insert(nalus.begin() + first_slice_index, pps_nalu);
      data_size += kNALUHeaderLength + pps_nalu.size;
      first_slice_index += 1;

      // Update the configured VPSs/SPSs/PPSs in case VT referrence to the wrong
      // parameter sets.
      configured_vpss_[active_vps_id].assign(
          active_vps_.data(), active_vps_.data() + active_vps_.size());
      configured_spss_[active_sps_id].assign(
          active_sps_.data(), active_sps_.data() + active_sps_.size());
      configured_ppss_[active_pps_id].assign(
          active_pps_.data(), active_pps_.data() + active_pps_.size());
    }
  }

  // Now that the configuration is up to date, copy it into the frame.
  frame->image_size = configured_size_;

  // Create a memory-backed CMBlockBuffer for the translated data.
  // TODO(sandersd): Pool of memory blocks.
  base::apple::ScopedCFTypeRef<CMBlockBufferRef> data;
  OSStatus status = CMBlockBufferCreateWithMemoryBlock(
      kCFAllocatorDefault,
      nullptr,              // &memory_block
      data_size,            // block_length
      kCFAllocatorDefault,  // block_allocator
      nullptr,              // &custom_block_source
      0,                    // offset_to_data
      data_size,            // data_length
      0,                    // flags
      data.InitializeInto());
  if (status) {
    NOTIFY_STATUS("CMBlockBufferCreateWithMemoryBlock()", status,
                  SFT_PLATFORM_ERROR);
    return;
  }

  // Make sure that the memory is actually allocated.
  // CMBlockBufferReplaceDataBytes() is documented to do this, but prints a
  // message each time starting in Mac OS X 10.10.
  status = CMBlockBufferAssureBlockMemory(data);
  if (status) {
    NOTIFY_STATUS("CMBlockBufferAssureBlockMemory()", status,
                  SFT_PLATFORM_ERROR);
    return;
  }

  // Copy NALU data into the CMBlockBuffer, inserting length headers.
  size_t offset = 0;
  for (size_t i = 0; i < nalus.size(); i++) {
    H265NALU& nalu_ref = nalus[i];
    uint32_t header = base::HostToNet32(static_cast<uint32_t>(nalu_ref.size));
    status =
        CMBlockBufferReplaceDataBytes(&header, data, offset, kNALUHeaderLength);
    if (status) {
      NOTIFY_STATUS("CMBlockBufferReplaceDataBytes()", status,
                    SFT_PLATFORM_ERROR);
      return;
    }
    offset += kNALUHeaderLength;
    status = CMBlockBufferReplaceDataBytes(nalu_ref.data, data, offset,
                                           nalu_ref.size);
    if (status) {
      NOTIFY_STATUS("CMBlockBufferReplaceDataBytes()", status,
                    SFT_PLATFORM_ERROR);
      return;
    }
    offset += nalu_ref.size;
  }

  // Package the data in a CMSampleBuffer.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample;
  status = CMSampleBufferCreate(kCFAllocatorDefault,
                                data,        // data_buffer
                                true,        // data_ready
                                nullptr,     // make_data_ready_callback
                                nullptr,     // make_data_ready_refcon
                                format_,     // format_description
                                1,           // num_samples
                                0,           // num_sample_timing_entries
                                nullptr,     // &sample_timing_array
                                1,           // num_sample_size_entries
                                &data_size,  // &sample_size_array
                                sample.InitializeInto());
  if (status) {
    NOTIFY_STATUS("CMSampleBufferCreate()", status, SFT_PLATFORM_ERROR);
    return;
  }

  // Send the frame for decoding.
  // Asynchronous Decompression allows for parallel submission of frames
  // (without it, DecodeFrame() does not return until the frame has been
  // decoded). We don't enable Temporal Processing because we are not passing
  // timestamps anyway.
  VTDecodeFrameFlags decode_flags =
      kVTDecodeFrame_EnableAsynchronousDecompression;
  status = VTDecompressionSessionDecodeFrame(
      session_,
      sample,                          // sample_buffer
      decode_flags,                    // decode_flags
      reinterpret_cast<void*>(frame),  // source_frame_refcon
      nullptr);                        // &info_flags_out
  if (status) {
    NOTIFY_STATUS("VTDecompressionSessionDecodeFrame()", status,
                  SFT_DECODE_ERROR);
    return;
  }
}
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

// This method may be called on any VideoToolbox thread.
void VTVideoDecodeAccelerator::Output(void* source_frame_refcon,
                                      OSStatus status,
                                      CVImageBufferRef image_buffer) {
  if (status) {
    NOTIFY_STATUS("Decoding", status, SFT_DECODE_ERROR);
    return;
  }

  // The type of |image_buffer| is CVImageBuffer, but we only handle
  // CVPixelBuffers. This should be guaranteed as we set
  // kCVPixelBufferOpenGLCompatibilityKey in |image_config|.
  //
  // Sometimes, for unknown reasons (http://crbug.com/453050), |image_buffer| is
  // NULL, which causes CFGetTypeID() to crash. While the rest of the code would
  // smoothly handle NULL as a dropped frame, we choose to fail permanantly here
  // until the issue is better understood.
  if (!image_buffer || CFGetTypeID(image_buffer) != CVPixelBufferGetTypeID()) {
    DLOG(ERROR) << "Decoded frame is not a CVPixelBuffer";
    NotifyError(PLATFORM_FAILURE, SFT_DECODE_ERROR);
    return;
  }

  Frame* frame = reinterpret_cast<Frame*>(source_frame_refcon);
  frame->image.reset(image_buffer, base::scoped_policy::RETAIN);
  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VTVideoDecodeAccelerator::DecodeDone, weak_this_, frame));
}

void VTVideoDecodeAccelerator::DecodeDone(Frame* frame) {
  DVLOG(3) << __func__ << "(" << frame->bitstream_id << ")";
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  // pending_frames_.erase() will delete |frame|.
  int32_t bitstream_id = frame->bitstream_id;
  DCHECK(pending_frames_.contains(bitstream_id));

  if (state_ == STATE_ERROR || state_ == STATE_DESTROYING) {
    // Destroy() handles NotifyEndOfBitstreamBuffer().
    pending_frames_.erase(bitstream_id);
    return;
  }

  DCHECK_EQ(state_, STATE_DECODING);
  if (!frame->image.get()) {
    pending_frames_.erase(bitstream_id);
    assigned_bitstream_ids_.erase(bitstream_id);
    client_->NotifyEndOfBitstreamBuffer(bitstream_id);
    return;
  }

  output_count_for_cra_rasl_workaround_++;

  Task task(TASK_FRAME);
  task.frame = std::move(pending_frames_[bitstream_id]);
  pending_frames_.erase(bitstream_id);
  task_queue_.push(std::move(task));
  ProcessWorkQueues();
}

void VTVideoDecodeAccelerator::FlushTask(TaskType type) {
  DVLOG(3) << __func__;
  DCHECK(decoder_task_runner_->RunsTasksInCurrentSequence());

  FinishDelayedFrames();

  // All the frames that are going to be sent must have been sent by now. So
  // clear any state in the bitstream filter.
  if (vp9_bsf_)
    vp9_bsf_->Flush();

  if (type == TASK_DESTROY) {
    if (session_) {
      // Destroy the decoding session before returning from the decoder thread.
      VTDecompressionSessionInvalidate(session_);
      session_.reset();
    }

    // This must be done on |decoder_task_runner_|.
    decoder_weak_this_factory_.InvalidateWeakPtrs();
  }

  // Queue a task even if flushing fails, so that destruction always completes.
  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VTVideoDecodeAccelerator::FlushDone, weak_this_, type));
}

void VTVideoDecodeAccelerator::FlushDone(TaskType type) {
  DVLOG(3) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  task_queue_.push(Task(type));
  ProcessWorkQueues();
}

void VTVideoDecodeAccelerator::Decode(BitstreamBuffer bitstream) {
  Decode(bitstream.ToDecoderBuffer(), bitstream.id());
}

void VTVideoDecodeAccelerator::Decode(scoped_refptr<DecoderBuffer> buffer,
                                      int32_t bitstream_id) {
  DVLOG(2) << __func__ << "(" << bitstream_id << ")";
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  if (bitstream_id < 0) {
    DLOG(ERROR) << "Invalid bitstream, id: " << bitstream_id;
    NotifyError(INVALID_ARGUMENT, SFT_INVALID_STREAM);
    return;
  }

  if (!buffer) {
    client_->NotifyEndOfBitstreamBuffer(bitstream_id);
    return;
  }

  DCHECK_EQ(0u, assigned_bitstream_ids_.count(bitstream_id));
  assigned_bitstream_ids_.insert(bitstream_id);

  Frame* frame = new Frame(bitstream_id);
  pending_frames_[bitstream_id] = base::WrapUnique(frame);

  if (codec_ == VideoCodec::kVP9) {
    decoder_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VTVideoDecodeAccelerator::DecodeTaskVp9,
                       decoder_weak_this_, std::move(buffer), frame));
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  } else if (codec_ == VideoCodec::kHEVC) {
    decoder_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VTVideoDecodeAccelerator::DecodeTaskHEVC,
                       decoder_weak_this_, std::move(buffer), frame));
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  } else {
    decoder_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VTVideoDecodeAccelerator::DecodeTaskH264,
                       decoder_weak_this_, std::move(buffer), frame));
  }
}

void VTVideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<PictureBuffer>& pictures) {
  DVLOG(1) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  for (const PictureBuffer& picture : pictures) {
    DVLOG(3) << "AssignPictureBuffer(" << picture.id() << ")";
    DCHECK(!picture_info_map_.contains(picture.id()));
    assigned_picture_ids_.insert(picture.id());
    available_picture_ids_.push_back(picture.id());

    // PictureBufferManager::CreatePictureBuffers() never creates
    // PictureBuffer instances with texture IDs on Apple platforms: it does so
    // only when requested to allocate GL textures, which is neither supported
    // nor ever requested on these platforms.
    CHECK(picture.client_texture_ids().empty() &&
          picture.service_texture_ids().empty());
    picture_info_map_.insert(
        std::make_pair(picture.id(), std::make_unique<PictureInfo>()));
  }

  // Pictures are not marked as uncleared until after this method returns, and
  // they will be broken if they are used before that happens. So, schedule
  // future work after that happens.
  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VTVideoDecodeAccelerator::ProcessWorkQueues, weak_this_));
}

void VTVideoDecodeAccelerator::ReusePictureBuffer(int32_t picture_id) {
  DVLOG(2) << __func__ << "(" << picture_id << ")";
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  // It's possible there was a ReusePictureBuffer() request in flight when we
  // called DismissPictureBuffer(), in which case we won't find it. In that case
  // we should just drop the ReusePictureBuffer() request.
  auto it = picture_info_map_.find(picture_id);
  if (it == picture_info_map_.end())
    return;

  // Drop references to allow the underlying buffer to be released.
  PictureInfo* picture_info = it->second.get();
  picture_info->scoped_shared_images.clear();
  picture_info->bitstream_id = 0;

  // Mark the picture as available and try to complete pending output work.
  DCHECK(assigned_picture_ids_.count(picture_id));
  available_picture_ids_.push_back(picture_id);
  ProcessWorkQueues();
}

void VTVideoDecodeAccelerator::ProcessWorkQueues() {
  DVLOG(3) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  switch (state_) {
    case STATE_DECODING:
      if (codec_ == VideoCodec::kVP9) {
        while (state_ == STATE_DECODING) {
          if (!ProcessOutputQueue() && !ProcessTaskQueue())
            break;
        }
        return;
      }

      // TODO(sandersd): Batch where possible.
      while (state_ == STATE_DECODING) {
        if (!ProcessReorderQueue() && !ProcessTaskQueue())
          break;
      }
      return;

    case STATE_ERROR:
      // Do nothing until Destroy() is called.
      return;

    case STATE_DESTROYING:
      // Drop tasks until we are ready to destruct.
      while (!task_queue_.empty()) {
        if (task_queue_.front().type == TASK_DESTROY) {
          delete this;
          return;
        }
        task_queue_.pop();
      }
      return;
  }
}

bool VTVideoDecodeAccelerator::ProcessTaskQueue() {
  DVLOG(3) << __func__ << " size=" << task_queue_.size();
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_DECODING);

  if (task_queue_.empty())
    return false;

  Task& task = task_queue_.front();
  switch (task.type) {
    case TASK_FRAME: {
      if (codec_ == VideoCodec::kVP9) {
        // Once we've reached our maximum output queue size, defer end of
        // bitstream buffer signals to avoid piling up too many frames.
        if (output_queue_.size() >= limits::kMaxVideoFrames)
          return false;

        assigned_bitstream_ids_.erase(task.frame->bitstream_id);
        client_->NotifyEndOfBitstreamBuffer(task.frame->bitstream_id);
        output_queue_.push_back(std::move(task.frame));
        task_queue_.pop();
        return true;
      }

      bool reorder_queue_has_space =
          reorder_queue_.size() < kMaxReorderQueueSize;
      bool reorder_queue_flush_needed =
          task.frame->is_idr || task.frame->has_mmco5;
      bool reorder_queue_flush_done = reorder_queue_.empty();
      if (reorder_queue_has_space &&
          (!reorder_queue_flush_needed || reorder_queue_flush_done)) {
        DVLOG(2) << "Decode(" << task.frame->bitstream_id << ") complete";
        assigned_bitstream_ids_.erase(task.frame->bitstream_id);
        client_->NotifyEndOfBitstreamBuffer(task.frame->bitstream_id);
        reorder_queue_.push(std::move(task.frame));
        task_queue_.pop();
        return true;
      }
      return false;
    }

    case TASK_FLUSH:
      DCHECK_EQ(task.type, pending_flush_tasks_.front());
      if ((codec_ != VideoCodec::kVP9 && reorder_queue_.size() == 0) ||
          (codec_ == VideoCodec::kVP9 && output_queue_.empty())) {
        DVLOG(1) << "Flush complete";
        pending_flush_tasks_.pop();
        client_->NotifyFlushDone();
        task_queue_.pop();
        return true;
      }
      return false;

    case TASK_RESET:
      DCHECK_EQ(task.type, pending_flush_tasks_.front());
      if ((codec_ != VideoCodec::kVP9 && reorder_queue_.size() == 0) ||
          (codec_ == VideoCodec::kVP9 && output_queue_.empty())) {
        DVLOG(1) << "Reset complete";
        waiting_for_idr_ = true;
        pending_flush_tasks_.pop();
        client_->NotifyResetDone();
        task_queue_.pop();
        return true;
      }
      return false;

    case TASK_DESTROY:
      NOTREACHED() << "Can't destroy while in STATE_DECODING";
      NotifyError(ILLEGAL_STATE, SFT_PLATFORM_ERROR);
      return false;
  }
}

bool VTVideoDecodeAccelerator::ProcessReorderQueue() {
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_DECODING);

  if (reorder_queue_.empty())
    return false;

  // If the next task is a flush (because there is a pending flush or because
  // the next frame is an IDR), then we don't need a full reorder buffer to send
  // the next frame.
  bool flushing =
      !task_queue_.empty() && (task_queue_.front().type != TASK_FRAME ||
                               task_queue_.front().frame->is_idr ||
                               task_queue_.front().frame->has_mmco5);

  size_t reorder_window = std::max(0, reorder_queue_.top()->reorder_window);
  DVLOG(3) << __func__ << " size=" << reorder_queue_.size()
           << " window=" << reorder_window << " flushing=" << flushing;
  if (flushing || reorder_queue_.size() > reorder_window) {
    if (ProcessFrame(*reorder_queue_.top())) {
      reorder_queue_.pop();
      return true;
    }
  }

  return false;
}

bool VTVideoDecodeAccelerator::ProcessOutputQueue() {
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_DECODING);

  if (output_queue_.empty())
    return false;

  if (ProcessFrame(*output_queue_.front())) {
    output_queue_.pop_front();
    return true;
  }

  return false;
}

bool VTVideoDecodeAccelerator::ProcessFrame(const Frame& frame) {
  DVLOG(3) << __func__ << "(" << frame.bitstream_id << ")";
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_DECODING);

  // If the next pending flush is for a reset, then the frame will be dropped.
  bool resetting = !pending_flush_tasks_.empty() &&
                   pending_flush_tasks_.front() == TASK_RESET;
  if (resetting)
    return true;

  DCHECK(frame.image.get());
  // If the |image_size| has changed, request new picture buffers and then
  // wait for them.
  //
  // TODO(sandersd): When used by GpuVideoDecoder, we don't need to bother
  // with this. We can tell that is the case when we also have a timestamp.
  if (picture_size_ != frame.image_size) {
    // Dismiss current pictures.
    for (int32_t picture_id : assigned_picture_ids_) {
      DVLOG(3) << "DismissPictureBuffer(" << picture_id << ")";
      client_->DismissPictureBuffer(picture_id);
    }
    assigned_picture_ids_.clear();
    picture_info_map_.clear();
    available_picture_ids_.clear();

    // Request new pictures.
    picture_size_ = frame.image_size;

    if (has_alpha_) {
      si_format_ = viz::MultiPlaneFormat::kNV12A;
      picture_format_ = PIXEL_FORMAT_NV12A;
    } else if (config_.profile == VP9PROFILE_PROFILE2 ||
               config_.profile == HEVCPROFILE_MAIN10 ||
               config_.profile == HEVCPROFILE_REXT) {
      si_format_ = viz::MultiPlaneFormat::kP010;
      picture_format_ = PIXEL_FORMAT_P016LE;
    } else {
      si_format_ = viz::MultiPlaneFormat::kNV12;
      picture_format_ = PIXEL_FORMAT_NV12;
    }

    DVLOG(3) << "ProvidePictureBuffers(" << kNumPictureBuffers
             << frame.image_size.ToString() << ")";
    client_->ProvidePictureBuffers(kNumPictureBuffers, picture_format_, 1,
                                   frame.image_size,
                                   gpu::GetPlatformSpecificTextureTarget());
    return false;
  }
  return SendFrame(frame);
}

bool VTVideoDecodeAccelerator::SendFrame(const Frame& frame) {
  DVLOG(2) << __func__ << "(" << frame.bitstream_id << ")";
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_DECODING);
  DCHECK(frame.image.get());

  if (available_picture_ids_.empty())
    return false;

  int32_t picture_id = available_picture_ids_.back();
  auto it = picture_info_map_.find(picture_id);
  DCHECK(it != picture_info_map_.end());
  PictureInfo* picture_info = it->second.get();

  gfx::ColorSpace color_space;
  if (codec_ == VideoCodec::kVP9) {
    // Prefer the color space from the config if available. It generally comes
    // from the color tag which is more expressive than the VP9 bitstream.
    color_space = config_.container_color_space.ToGfxColorSpace();
    if (!color_space.IsValid()) {
      color_space = GetImageBufferColorSpace(frame.image);
    }
  } else {
    // Otherwise prefer the frame color space.
    color_space = GetImageBufferColorSpace(frame.image);
    if (!color_space.IsValid()) {
      color_space = config_.container_color_space.ToGfxColorSpace();
    }
  }

  std::vector<gfx::BufferPlane> planes;
  if (IsMultiPlaneFormatForHardwareVideoEnabled()) {
    planes.push_back(gfx::BufferPlane::DEFAULT);
  } else {
    switch (picture_format_) {
      case PIXEL_FORMAT_NV12:
      case PIXEL_FORMAT_P016LE:
        planes.push_back(gfx::BufferPlane::Y);
        planes.push_back(gfx::BufferPlane::UV);
        break;
      case PIXEL_FORMAT_NV12A:
        planes.push_back(gfx::BufferPlane::Y);
        planes.push_back(gfx::BufferPlane::UV);
        planes.push_back(gfx::BufferPlane::A);
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  for (size_t plane = 0; plane < planes.size(); ++plane) {
    gpu::SharedImageStub* shared_image_stub = client_->GetSharedImageStub();
    if (!shared_image_stub) {
      DLOG(ERROR) << "Failed to get SharedImageStub";
      NotifyError(PLATFORM_FAILURE, SFT_PLATFORM_ERROR);
      return false;
    }

    const gfx::Size frame_size(CVPixelBufferGetWidth(frame.image.get()),
                               CVPixelBufferGetHeight(frame.image.get()));
    const uint32_t shared_image_usage =
        gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT |
        gpu::SHARED_IMAGE_USAGE_MACOS_VIDEO_TOOLBOX |
        gpu::SHARED_IMAGE_USAGE_RASTER | gpu::SHARED_IMAGE_USAGE_GLES2;
    GLenum target = gl_client_.supports_arb_texture_rectangle
                        ? GL_TEXTURE_RECTANGLE_ARB
                        : GL_TEXTURE_2D;

    gfx::GpuMemoryBufferHandle handle;
    handle.id = gfx::GpuMemoryBufferHandle::kInvalidId;
    handle.type = gfx::GpuMemoryBufferType::IO_SURFACE_BUFFER;
    handle.io_surface.reset(CVPixelBufferGetIOSurface(frame.image),
                            base::scoped_policy::RETAIN);

    gpu::Mailbox mailbox = gpu::Mailbox::GenerateForSharedImage();
    bool success;
    constexpr char kDebugLabel[] = "VTVideoDecodeAccelerator";
    if (IsMultiPlaneFormatForHardwareVideoEnabled()) {
      success = shared_image_stub->CreateSharedImage(
          mailbox, std::move(handle), si_format_, frame_size, color_space,
          kTopLeft_GrSurfaceOrigin, kOpaque_SkAlphaType, shared_image_usage,
          kDebugLabel);
    } else {
      success = shared_image_stub->CreateSharedImage(
          mailbox, std::move(handle), ToBufferFormat(si_format_), planes[plane],
          frame_size, color_space, kTopLeft_GrSurfaceOrigin,
          kOpaque_SkAlphaType, shared_image_usage, kDebugLabel);
    }
    if (!success) {
      DLOG(ERROR) << "Failed to create shared image";
      NotifyError(PLATFORM_FAILURE, SFT_PLATFORM_ERROR);
      return false;
    }

    // Wrap the destroy callback in a lambda that ensures that it be called on
    // the appropriate thread. Retain the image buffer so that VideoToolbox
    // will not reuse the IOSurface as long as the SharedImage is alive.
    auto destroy_shared_image_lambda =
        [](gpu::SharedImageStub::SharedImageDestructionCallback callback,
           base::apple::ScopedCFTypeRef<CVImageBufferRef> image,
           scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
          task_runner->PostTask(
              FROM_HERE, base::BindOnce(std::move(callback), gpu::SyncToken()));
        };
    auto destroy_shared_image_callback = base::BindOnce(
        destroy_shared_image_lambda,
        shared_image_stub->GetSharedImageDestructionCallback(mailbox),
        frame.image, gpu_task_runner_);
    picture_info->scoped_shared_images.push_back(
        scoped_refptr<Picture::ScopedSharedImage>(
            new Picture::ScopedSharedImage(
                mailbox, target, std::move(destroy_shared_image_callback))));
  }
  picture_info->bitstream_id = frame.bitstream_id;
  available_picture_ids_.pop_back();

  DVLOG(3) << "PictureReady(picture_id=" << picture_id << ", "
           << "bitstream_id=" << frame.bitstream_id << ")";
  Picture picture(picture_id, frame.bitstream_id, gfx::Rect(frame.image_size),
                  color_space, true);
  // We release the CVImageBuffer when the VideoFrame is destroyed. This happens
  // after commands referencing the SharedImages have been submitted to the
  // platform, but can be before they are actually executed. When this release
  // happens, the SharedImage contents can change immediately. Therefore we must
  // wait for the commands to finish executing before releasing (cf.
  // https://crbug.com/930479#c69). We do this via a read lock fence.
  // TODO(sandersd): Can IOSurfaceImageBacking be responsible for fences, so
  // that we don't need to use them when the image is never bound? Bindings are
  // typically only created when WebGL is in use.
  picture.set_read_lock_fences_enabled(true);
  if (frame.hdr_metadata)
    picture.set_hdr_metadata(frame.hdr_metadata);
  if (IsMultiPlaneFormatForHardwareVideoEnabled()) {
    picture.set_shared_image_format_type(
        SharedImageFormatType::kSharedImageFormat);
  }
  // For multiplanar shared images, planes.size() is 1.
  for (size_t plane = 0; plane < planes.size(); ++plane) {
    picture.set_scoped_shared_image(picture_info->scoped_shared_images[plane],
                                    plane);
  }

  if (IOSurfaceIsWebGPUCompatible(CVPixelBufferGetIOSurface(frame.image))) {
    picture.set_is_webgpu_compatible(true);
  }

  client_->PictureReady(std::move(picture));
  return true;
}

void VTVideoDecodeAccelerator::NotifyError(
    Error vda_error_type,
    VTVDASessionFailureType session_failure_type) {
  DCHECK_LT(session_failure_type, SFT_MAX + 1);
  if (!gpu_task_runner_->BelongsToCurrentThread()) {
    gpu_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VTVideoDecodeAccelerator::NotifyError, weak_this_,
                       vda_error_type, session_failure_type));
  } else if (state_ == STATE_DECODING) {
    state_ = STATE_ERROR;
    UMA_HISTOGRAM_ENUMERATION("Media.VTVDA.SessionFailureReason",
                              session_failure_type, SFT_MAX + 1);
    client_->NotifyError(vda_error_type);
  }
}

void VTVideoDecodeAccelerator::WriteToMediaLog(MediaLogMessageLevel level,
                                               const std::string& message) {
  if (!gpu_task_runner_->BelongsToCurrentThread()) {
    gpu_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VTVideoDecodeAccelerator::WriteToMediaLog,
                                  weak_this_, level, message));
    return;
  }

  DVLOG(1) << __func__ << "(" << static_cast<int>(level) << ") " << message;

  if (media_log_)
    media_log_->AddMessage(level, message);
}

void VTVideoDecodeAccelerator::QueueFlush(TaskType type) {
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  pending_flush_tasks_.push(type);
  decoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VTVideoDecodeAccelerator::FlushTask,
                                decoder_weak_this_, type));

  // If this is a new flush request, see if we can make progress.
  if (pending_flush_tasks_.size() == 1)
    ProcessWorkQueues();
}

void VTVideoDecodeAccelerator::Flush() {
  DVLOG(1) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  QueueFlush(TASK_FLUSH);
}

void VTVideoDecodeAccelerator::Reset() {
  DVLOG(1) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  QueueFlush(TASK_RESET);
}

void VTVideoDecodeAccelerator::Destroy() {
  DVLOG(1) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  // For a graceful shutdown, return assigned buffers and flush before
  // destructing |this|.
  for (int32_t bitstream_id : assigned_bitstream_ids_)
    client_->NotifyEndOfBitstreamBuffer(bitstream_id);
  assigned_bitstream_ids_.clear();
  state_ = STATE_DESTROYING;
  QueueFlush(TASK_DESTROY);
}

bool VTVideoDecodeAccelerator::TryToSetupDecodeOnSeparateSequence(
    const base::WeakPtr<Client>& decode_client,
    const scoped_refptr<base::SequencedTaskRunner>& decode_task_runner) {
  return false;
}

bool VTVideoDecodeAccelerator::SupportsSharedImagePictureBuffers() const {
  return true;
}

// static
VideoDecodeAccelerator::SupportedProfiles
VTVideoDecodeAccelerator::GetSupportedProfiles(
    const gpu::GpuDriverBugWorkarounds& workarounds) {
  SupportedProfiles profiles;
  InitializeVideoToolbox();

  for (const auto& supported_profile : kSupportedProfiles) {
    if (supported_profile == VP9PROFILE_PROFILE0 ||
        supported_profile == VP9PROFILE_PROFILE2) {
      if (workarounds.disable_accelerated_vp9_decode)
        continue;
      if (!VTIsHardwareDecodeSupported(kCMVideoCodecType_VP9))
        continue;
      // Success! We have VP9 hardware decoding support.
    }

    if (supported_profile == HEVCPROFILE_MAIN ||
        supported_profile == HEVCPROFILE_MAIN10 ||
        supported_profile == HEVCPROFILE_MAIN_STILL_PICTURE ||
        supported_profile == HEVCPROFILE_REXT) {
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
      if (!workarounds.disable_accelerated_hevc_decode &&
          base::FeatureList::IsEnabled(kPlatformHEVCDecoderSupport)) {
        if (__builtin_available(macOS 11.0, *)) {
          // Success! We have HEVC hardware decoding (or software
          // decoding if the hardware is not good enough) support too.
          SupportedProfile profile;
          profile.profile = supported_profile;
          profile.min_resolution.SetSize(16, 16);
          // max supported resolution -> 8k 👍
          profile.max_resolution.SetSize(8192, 8192);
          profiles.push_back(profile);
        }
      }
#endif  //  BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
      continue;
    }

    SupportedProfile profile;
    profile.profile = supported_profile;
    profile.min_resolution.SetSize(16, 16);
    profile.max_resolution.SetSize(4096, 4096);
    profiles.push_back(profile);
  }
  return profiles;
}

}  // namespace media
