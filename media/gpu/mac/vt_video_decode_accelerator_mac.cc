// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/vt_video_decode_accelerator_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreVideo/CoreVideo.h>
#include <OpenGL/CGLIOSurface.h>
#include <OpenGL/gl.h>
#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <memory>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/sys_byteorder.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/version.h"
#include "components/crash/core/common/crash_key.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image_backing_gl_image.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/ipc/service/shared_image_stub.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/filters/vp9_parser.h"
#include "media/gpu/mac/vp9_super_frame_bitstream_filter.h"
#include "media/gpu/mac/vt_beta_stubs.h"
#include "media/gpu/mac/vt_config_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_io_surface.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/scoped_binders.h"

#define NOTIFY_STATUS(name, status, session_failure) \
  do {                                               \
    OSSTATUS_DLOG(ERROR, status) << name;            \
    NotifyError(PLATFORM_FAILURE, session_failure);  \
  } while (0)

namespace media {

namespace {

// A sequence of ids for memory tracing.
base::AtomicSequenceNumber g_memory_dump_ids;

// A sequence of shared memory ids for CVPixelBufferRefs.
base::AtomicSequenceNumber g_cv_pixel_buffer_ids;

// Only H.264 with 4:2:0 chroma sampling is supported.
constexpr VideoCodecProfile kSupportedProfiles[] = {
    H264PROFILE_BASELINE, H264PROFILE_EXTENDED, H264PROFILE_MAIN,
    H264PROFILE_HIGH,

    // These are only supported on macOS 11+.
    VP9PROFILE_PROFILE0, VP9PROFILE_PROFILE2,

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

// Build an |image_config| dictionary for VideoToolbox initialization.
base::ScopedCFTypeRef<CFMutableDictionaryRef> BuildImageConfig(
    CMVideoDimensions coded_dimensions,
    bool is_hbd) {
  base::ScopedCFTypeRef<CFMutableDictionaryRef> image_config;

  // Note that 4:2:0 textures cannot be used directly as RGBA in OpenGL, but are
  // lower power than 4:2:2 when composited directly by CoreAnimation.
  int32_t pixel_format = is_hbd
                             ? kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange
                             : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
#define CFINT(i) CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i)
  base::ScopedCFTypeRef<CFNumberRef> cf_pixel_format(CFINT(pixel_format));
  base::ScopedCFTypeRef<CFNumberRef> cf_width(CFINT(coded_dimensions.width));
  base::ScopedCFTypeRef<CFNumberRef> cf_height(CFINT(coded_dimensions.height));
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

// Create a CMFormatDescription using the provided |pps| and |sps|.
base::ScopedCFTypeRef<CMFormatDescriptionRef> CreateVideoFormatH264(
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
  base::ScopedCFTypeRef<CMFormatDescriptionRef> format;
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

base::ScopedCFTypeRef<CMFormatDescriptionRef> CreateVideoFormatVP9(
    media::VideoColorSpace color_space,
    media::VideoCodecProfile profile,
    base::Optional<gl::HDRMetadata> hdr_metadata,
    const gfx::Size& coded_size) {
  base::ScopedCFTypeRef<CFMutableDictionaryRef> format_config(
      CreateFormatExtensions(kCMVideoCodecType_VP9, profile, color_space,
                             hdr_metadata));

  base::ScopedCFTypeRef<CMFormatDescriptionRef> format;
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
    const VTDecompressionOutputCallbackRecord* callback,
    base::ScopedCFTypeRef<VTDecompressionSessionRef>* session,
    gfx::Size* configured_size) {
  // Prepare VideoToolbox configuration dictionaries.
  base::ScopedCFTypeRef<CFMutableDictionaryRef> decoder_config(
      CFDictionaryCreateMutable(kCFAllocatorDefault,
                                1,  // capacity
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  if (!decoder_config) {
    DLOG(ERROR) << "Failed to create CFMutableDictionary";
    return false;
  }

  CFDictionarySetValue(
      decoder_config,
      kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder,
      kCFBooleanTrue);
  CFDictionarySetValue(
      decoder_config,
      kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder,
      require_hardware ? kCFBooleanTrue : kCFBooleanFalse);

  // VideoToolbox scales the visible rect to the output size, so we set the
  // output size for a 1:1 ratio. (Note though that VideoToolbox does not handle
  // top or left crops correctly.) We expect the visible rect to be integral.
  CGRect visible_rect = CMVideoFormatDescriptionGetCleanAperture(format, true);
  CMVideoDimensions visible_dimensions = {visible_rect.size.width,
                                          visible_rect.size.height};
  base::ScopedCFTypeRef<CFMutableDictionaryRef> image_config(
      BuildImageConfig(visible_dimensions, is_hbd));
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

// The purpose of this function is to preload the generic and hardware-specific
// libraries required by VideoToolbox before the GPU sandbox is enabled.
// VideoToolbox normally loads the hardware-specific libraries lazily, so we
// must actually create a decompression session. If creating a decompression
// session fails, hardware decoding will be disabled (Initialize() will always
// return false).
bool InitializeVideoToolboxInternal() {
  VTDecompressionOutputCallbackRecord callback = {0};
  base::ScopedCFTypeRef<VTDecompressionSessionRef> session;
  gfx::Size configured_size;

  // Create a hardware decoding session.
  // SPS and PPS data are taken from a 480p sample (buck2.mp4).
  const std::vector<uint8_t> sps_normal = {
      0x67, 0x64, 0x00, 0x1e, 0xac, 0xd9, 0x80, 0xd4, 0x3d, 0xa1, 0x00, 0x00,
      0x03, 0x00, 0x01, 0x00, 0x00, 0x03, 0x00, 0x30, 0x8f, 0x16, 0x2d, 0x9a};
  const std::vector<uint8_t> pps_normal = {0x68, 0xe9, 0x7b, 0xcb};
  if (!CreateVideoToolboxSession(
          CreateVideoFormatH264(sps_normal, std::vector<uint8_t>(), pps_normal),
          /*require_hardware=*/true, /*is_hbd=*/false, &callback, &session,
          &configured_size)) {
    DVLOG(1) << "Hardware H264 decoding with VideoToolbox is not supported";
    return false;
  }

  session.reset();

  // Create a software decoding session.
  // SPS and PPS data are taken from a 18p sample (small2.mp4).
  const std::vector<uint8_t> sps_small = {
      0x67, 0x64, 0x00, 0x0a, 0xac, 0xd9, 0x89, 0x7e, 0x22, 0x10, 0x00,
      0x00, 0x3e, 0x90, 0x00, 0x0e, 0xa6, 0x08, 0xf1, 0x22, 0x59, 0xa0};
  const std::vector<uint8_t> pps_small = {0x68, 0xe9, 0x79, 0x72, 0xc0};
  if (!CreateVideoToolboxSession(
          CreateVideoFormatH264(sps_small, std::vector<uint8_t>(), pps_small),
          /*require_hardware=*/false, /*is_hbd=*/false, &callback, &session,
          &configured_size)) {
    DVLOG(1) << "Software H264 decoding with VideoToolbox is not supported";
    return false;
  }

  session.reset();

  if (base::mac::IsAtLeastOS11()) {
    // Until our target sdk version is 11.0 we need to dynamically link the
    // VTRegisterSupplementalVideoDecoderIfAvailable() symbol in.
    media_gpu_mac::StubPathMap paths;
    paths[media_gpu_mac::kModuleVt_beta].push_back(FILE_PATH_LITERAL(
        "/System/Library/Frameworks/VideoToolbox.framework/VideoToolbox"));
    if (!media_gpu_mac::InitializeStubs(paths))
      return true;  // VP9 support is optional.

// __builtin_available doesn't work for 11.0 yet; https://crbug.com/1115294
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
    VTRegisterSupplementalVideoDecoderIfAvailable(kCMVideoCodecType_VP9);
#pragma clang diagnostic pop

    // Create a VP9 decoding session.
    if (!CreateVideoToolboxSession(
            CreateVideoFormatVP9(VideoColorSpace::REC709(), VP9PROFILE_PROFILE0,
                                 base::nullopt, gfx::Size(720, 480)),
            /*require_hardware=*/true, /*is_hbd=*/false, &callback, &session,
            &configured_size)) {
      DVLOG(1) << "Hardware VP9 decoding with VideoToolbox is not supported";

      // We don't return false here since VP9 support is optional.
    }
  }

  return true;
}

// TODO(sandersd): Share this computation with the VAAPI decoder.
int32_t ComputeReorderWindow(const H264SPS* sps) {
  // When |pic_order_cnt_type| == 2, decode order always matches presentation
  // order.
  // TODO(sandersd): For |pic_order_cnt_type| == 1, analyze the delta cycle to
  // find the minimum required reorder window.
  if (sps->pic_order_cnt_type == 2)
    return 0;

  // TODO(sandersd): Compute MaxDpbFrames.
  int32_t max_dpb_frames = 16;

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

}  // namespace

// Detects coded size and color space changes. Also indicates when a frame won't
// generate any output.
class VP9ConfigChangeDetector {
 public:
  VP9ConfigChangeDetector() : parser_(false) {}
  ~VP9ConfigChangeDetector() = default;

  void DetectConfig(const uint8_t* stream, unsigned int size) {
    parser_.SetStream(stream, size, nullptr);
    config_changed_ = false;

    Vp9FrameHeader fhdr;
    gfx::Size allocate_size;
    std::unique_ptr<DecryptConfig> null_config;
    while (parser_.ParseNextFrame(&fhdr, &allocate_size, &null_config) ==
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
  Vp9Parser parser_;
};

bool InitializeVideoToolbox() {
  // InitializeVideoToolbox() is called only from the GPU process main thread:
  // once for sandbox warmup, and then once each time a VTVideoDecodeAccelerator
  // is initialized. This ensures that everything is loaded whether or not the
  // sandbox is enabled.
  static const bool succeeded = InitializeVideoToolboxInternal();
  return succeeded;
}

VTVideoDecodeAccelerator::Task::Task(TaskType type) : type(type) {}

VTVideoDecodeAccelerator::Task::Task(Task&& other) = default;

VTVideoDecodeAccelerator::Task::~Task() {}

VTVideoDecodeAccelerator::Frame::Frame(int32_t bitstream_id)
    : bitstream_id(bitstream_id) {}

VTVideoDecodeAccelerator::Frame::~Frame() {}

VTVideoDecodeAccelerator::PictureInfo::PictureInfo()
    : uses_shared_images(true) {}

VTVideoDecodeAccelerator::PictureInfo::PictureInfo(uint32_t client_texture_id,
                                                   uint32_t service_texture_id)
    : uses_shared_images(false),
      client_texture_id(client_texture_id),
      service_texture_id(service_texture_id) {}

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
    MediaLog* media_log)
    : gl_client_(gl_client),
      media_log_(media_log),
      gpu_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      decoder_thread_("VTDecoderThread"),
      weak_this_factory_(this) {
  DCHECK(gl_client_.bind_image);

  callback_.decompressionOutputCallback = OutputThunk;
  callback_.decompressionOutputRefCon = this;
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

  // Dump output pictures (decoded frames for which PictureReady() has been
  // called already).
  for (const auto& it : picture_info_map_) {
    PictureInfo* picture_info = it.second.get();
    if (picture_info->gl_image) {
      std::string dump_name =
          base::StringPrintf("media/vt_video_decode_accelerator_%d/picture_%d",
                             memory_dump_id_, picture_info->bitstream_id);
      picture_info->gl_image->OnMemoryDump(pmd, 0, dump_name);
    }
  }

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

  static const base::NoDestructor<VideoDecodeAccelerator::SupportedProfiles>
      kActualSupportedProfiles(GetSupportedProfiles());
  if (std::find_if(kActualSupportedProfiles->begin(),
                   kActualSupportedProfiles->end(), [config](const auto& p) {
                     return p.profile == config.profile;
                   }) == kActualSupportedProfiles->end()) {
    DVLOG(2) << "Unsupported profile";
    return false;
  }

  if (!InitializeVideoToolbox()) {
    DVLOG(2) << "VideoToolbox is unavailable";
    return false;
  }

  client_ = client;
  config_ = config;

  switch (config.profile) {
    case H264PROFILE_BASELINE:
    case H264PROFILE_EXTENDED:
    case H264PROFILE_MAIN:
    case H264PROFILE_HIGH:
      codec_ = kCodecH264;
      break;
    case VP9PROFILE_PROFILE0:
    case VP9PROFILE_PROFILE2:
      codec_ = kCodecVP9;
      break;
    default:
      NOTREACHED() << "Unsupported profile.";
  };

  // Spawn a thread to handle parsing and calling VideoToolbox.
  // TODO(sandersd): This should probably use a base::ThreadPool thread instead.
  if (!decoder_thread_.Start()) {
    DLOG(ERROR) << "Failed to start decoder thread";
    return false;
  }

  // Count the session as successfully initialized.
  UMA_HISTOGRAM_ENUMERATION("Media.VTVDA.SessionFailureReason",
                            SFT_SUCCESSFULLY_INITIALIZED, SFT_MAX + 1);
  return true;
}

bool VTVideoDecodeAccelerator::FinishDelayedFrames() {
  DVLOG(3) << __func__;
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  if (session_) {
    OSStatus status = VTDecompressionSessionWaitForAsynchronousFrames(session_);
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
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  base::ScopedCFTypeRef<CMFormatDescriptionRef> format;
  switch (codec_) {
    case kCodecH264:
      format = CreateVideoFormatH264(active_sps_, active_spsext_, active_pps_);
      break;
    case kCodecVP9:
      format = CreateVideoFormatVP9(
          cc_detector_->GetColorSpace(config_.container_color_space),
          config_.profile, config_.hdr_metadata,
          cc_detector_->GetCodedSize(config_.initial_expected_coded_size));
      break;
    default:
      NOTREACHED() << "Unsupported codec.";
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
  const bool require_hardware = config_.profile == VP9PROFILE_PROFILE0 ||
                                config_.profile == VP9PROFILE_PROFILE2;
  const bool is_hbd = config_.profile == VP9PROFILE_PROFILE2;
  if (!CreateVideoToolboxSession(format_, require_hardware, is_hbd, &callback_,
                                 &session_, &configured_size_)) {
    NotifyError(PLATFORM_FAILURE, SFT_PLATFORM_ERROR);
    return false;
  }

  // Report whether hardware decode is being used.
  bool using_hardware = false;
  base::ScopedCFTypeRef<CFBooleanRef> cf_using_hardware;
  if (VTSessionCopyProperty(
          session_,
          // kVTDecompressionPropertyKey_UsingHardwareAcceleratedVideoDecoder
          CFSTR("UsingHardwareAcceleratedVideoDecoder"), kCFAllocatorDefault,
          cf_using_hardware.InitializeInto()) == 0) {
    using_hardware = CFBooleanGetValue(cf_using_hardware);
  }
  UMA_HISTOGRAM_BOOLEAN("Media.VTVDA.HardwareAccelerated", using_hardware);

  if (codec_ == kCodecVP9 && !vp9_bsf_)
    vp9_bsf_ = std::make_unique<VP9SuperFrameBitstreamFilter>();

  // Record that the configuration change is complete.
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
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

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
  base::ScopedCFTypeRef<CMSampleBufferRef> sample;
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

void VTVideoDecodeAccelerator::DecodeTask(scoped_refptr<DecoderBuffer> buffer,
                                          Frame* frame) {
  DVLOG(2) << __func__ << "(" << frame->bitstream_id << ")";
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  // NALUs are stored with Annex B format in the bitstream buffer (start codes),
  // but VideoToolbox expects AVC format (length headers), so we must rewrite
  // the data.
  //
  // Locate relevant NALUs and compute the size of the rewritten data. Also
  // record parameter sets for VideoToolbox initialization.
  size_t data_size = 0;
  std::vector<H264NALU> nalus;
  parser_.SetStream(buffer->data(), buffer->data_size());
  H264NALU nalu;
  while (true) {
    H264Parser::Result result = parser_.AdvanceToNextNALU(&nalu);
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
        result = parser_.ParseSPS(&sps_id);
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
        result = parser_.ParseSPSExt(&sps_id);
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
        result = parser_.ParsePPS(&pps_id);
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
          result = parser_.ParseSliceHeader(nalu, &slice_hdr);
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
          const H264PPS* pps = parser_.GetPPS(slice_hdr.pic_parameter_set_id);
          if (!pps) {
            WriteToMediaLog(MediaLogMessageLevel::kERROR,
                            "Missing PPS referenced by slice");
            NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
            return;
          }

          const H264SPS* sps = parser_.GetSPS(pps->seq_parameter_set_id);
          if (!sps) {
            WriteToMediaLog(MediaLogMessageLevel::kERROR,
                            "Missing SPS referenced by PPS");
            NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
            return;
          }

          // Record the configuration.
          DCHECK(seen_pps_.count(slice_hdr.pic_parameter_set_id));
          DCHECK(seen_sps_.count(pps->seq_parameter_set_id));
          active_sps_ = seen_sps_[pps->seq_parameter_set_id];
          // Note: SPS extension lookup may create an empty entry.
          active_spsext_ = seen_spsext_[pps->seq_parameter_set_id];
          active_pps_ = seen_pps_[slice_hdr.pic_parameter_set_id];

          // Compute and store frame properties. |image_size| gets filled in
          // later, since it comes from the decoder configuration.
          base::Optional<int32_t> pic_order_cnt =
              poc_.ComputePicOrderCnt(sps, slice_hdr);
          if (!pic_order_cnt.has_value()) {
            WriteToMediaLog(MediaLogMessageLevel::kERROR,
                            "Unable to compute POC");
            NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
            return;
          }

          frame->has_slice = true;
          frame->is_idr = nalu.nal_unit_type == media::H264NALU::kIDRSlice;
          frame->has_mmco5 = poc_.IsPendingMMCO5();
          frame->pic_order_cnt = *pic_order_cnt;
          frame->reorder_window = ComputeReorderWindow(sps);
        }
        FALLTHROUGH;

      default:
        nalus.push_back(nalu);
        data_size += kNALUHeaderLength + nalu.size;
        break;
    }
  }

  if (frame->is_idr)
    waiting_for_idr_ = false;

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

  // Apply any configuration change, but only at an IDR. If there is no IDR, we
  // just hope for the best from the decoder.
  if (frame->is_idr &&
      (configured_sps_ != active_sps_ || configured_spsext_ != active_spsext_ ||
       configured_pps_ != active_pps_)) {
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
  base::ScopedCFTypeRef<CMBlockBufferRef> data;
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
    H264NALU& nalu = nalus[i];
    uint32_t header = base::HostToNet32(static_cast<uint32_t>(nalu.size));
    status =
        CMBlockBufferReplaceDataBytes(&header, data, offset, kNALUHeaderLength);
    if (status) {
      NOTIFY_STATUS("CMBlockBufferReplaceDataBytes()", status,
                    SFT_PLATFORM_ERROR);
      return;
    }
    offset += kNALUHeaderLength;
    status = CMBlockBufferReplaceDataBytes(nalu.data, data, offset, nalu.size);
    if (status) {
      NOTIFY_STATUS("CMBlockBufferReplaceDataBytes()", status,
                    SFT_PLATFORM_ERROR);
      return;
    }
    offset += nalu.size;
  }

  // Package the data in a CMSampleBuffer.
  base::ScopedCFTypeRef<CMSampleBufferRef> sample;
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
  DCHECK_EQ(1u, pending_frames_.count(bitstream_id));

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

  Task task(TASK_FRAME);
  task.frame = std::move(pending_frames_[bitstream_id]);
  pending_frames_.erase(bitstream_id);
  task_queue_.push(std::move(task));
  ProcessWorkQueues();
}

void VTVideoDecodeAccelerator::FlushTask(TaskType type) {
  DVLOG(3) << __func__;
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  FinishDelayedFrames();

  // All the frames that are going to be sent must have been sent by now. So
  // clear any state in the bitstream filter.
  if (vp9_bsf_)
    vp9_bsf_->Flush();

  if (type == TASK_DESTROY && session_) {
    // Destroy the decoding session before returning from the decoder thread.
    VTDecompressionSessionInvalidate(session_);
    session_.reset();
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

  if (codec_ == kCodecVP9) {
    decoder_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&VTVideoDecodeAccelerator::DecodeTaskVp9,
                       base::Unretained(this), std::move(buffer), frame));
  } else {
    decoder_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&VTVideoDecodeAccelerator::DecodeTask,
                       base::Unretained(this), std::move(buffer), frame));
  }
}

void VTVideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<PictureBuffer>& pictures) {
  DVLOG(1) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  for (const PictureBuffer& picture : pictures) {
    DVLOG(3) << "AssignPictureBuffer(" << picture.id() << ")";
    DCHECK(!picture_info_map_.count(picture.id()));
    assigned_picture_ids_.insert(picture.id());
    available_picture_ids_.push_back(picture.id());
    if (picture.client_texture_ids().empty() &&
        picture.service_texture_ids().empty()) {
      picture_info_map_.insert(
          std::make_pair(picture.id(), std::make_unique<PictureInfo>()));
    } else {
      DCHECK_LE(1u, picture.client_texture_ids().size());
      DCHECK_LE(1u, picture.service_texture_ids().size());
      picture_info_map_.insert(std::make_pair(
          picture.id(),
          std::make_unique<PictureInfo>(picture.client_texture_ids()[0],
                                        picture.service_texture_ids()[0])));
    }
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
  if (picture_info->uses_shared_images) {
    picture_info->scoped_shared_image = nullptr;
  } else {
    gl_client_.bind_image.Run(picture_info->client_texture_id,
                              gpu::GetPlatformSpecificTextureTarget(), nullptr,
                              false);
  }
  picture_info->gl_image = nullptr;
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
      if (codec_ != kCodecH264) {
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
      if (codec_ == kCodecVP9) {
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
      if ((codec_ == kCodecH264 && reorder_queue_.size() == 0) ||
          (codec_ == kCodecVP9 && output_queue_.empty())) {
        DVLOG(1) << "Flush complete";
        pending_flush_tasks_.pop();
        client_->NotifyFlushDone();
        task_queue_.pop();
        return true;
      }
      return false;

    case TASK_RESET:
      DCHECK_EQ(task.type, pending_flush_tasks_.front());
      if ((codec_ == kCodecH264 && reorder_queue_.size() == 0) ||
          (codec_ == kCodecVP9 && output_queue_.empty())) {
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

  if (!resetting) {
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
      DVLOG(3) << "ProvidePictureBuffers(" << kNumPictureBuffers
               << frame.image_size.ToString() << ")";
      client_->ProvidePictureBuffers(kNumPictureBuffers, PIXEL_FORMAT_UNKNOWN,
                                     1, frame.image_size,
                                     gpu::GetPlatformSpecificTextureTarget());
      return false;
    }
    if (!SendFrame(frame))
      return false;
  }

  return true;
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
  DCHECK(!picture_info->gl_image);

  const gfx::BufferFormat buffer_format =
      config_.profile == VP9PROFILE_PROFILE2
          ? gfx::BufferFormat::P010
          : gfx::BufferFormat::YUV_420_BIPLANAR;
  // TODO(https://crbug.com/1108909): BGRA is not an appropriate value for
  // these parameters.
  const GLenum gl_format = GL_BGRA_EXT;
  const viz::ResourceFormat viz_resource_format =
      viz::ResourceFormat::BGRA_8888;

  scoped_refptr<gl::GLImageIOSurface> gl_image(
      gl::GLImageIOSurface::Create(frame.image_size, gl_format));
  if (!gl_image->InitializeWithCVPixelBuffer(
          frame.image.get(),
          gfx::GenericSharedMemoryId(g_cv_pixel_buffer_ids.GetNext()),
          buffer_format)) {
    NOTIFY_STATUS("Failed to initialize GLImageIOSurface", PLATFORM_FAILURE,
                  SFT_PLATFORM_ERROR);
  }
  gl_image->DisableInUseByWindowServer();
  gfx::ColorSpace color_space = GetImageBufferColorSpace(frame.image);
  gl_image->SetColorSpaceForYUVToRGBConversion(color_space);

  scoped_refptr<Picture::ScopedSharedImage> scoped_shared_image;
  if (picture_info->uses_shared_images) {
    gpu::SharedImageStub* shared_image_stub = client_->GetSharedImageStub();
    DCHECK(shared_image_stub);
    const uint32_t shared_image_usage =
        gpu::SHARED_IMAGE_USAGE_DISPLAY | gpu::SHARED_IMAGE_USAGE_SCANOUT;
    gpu::Mailbox mailbox = gpu::Mailbox::GenerateForSharedImage();

    gpu::SharedImageBackingGLCommon::InitializeGLTextureParams gl_params;
    // ANGLE-on-Metal exposes IOSurfaces via GL_TEXTURE_2D. Be robust to that.
    gl_params.target = gl_client_.supports_arb_texture_rectangle
                           ? GL_TEXTURE_RECTANGLE_ARB
                           : GL_TEXTURE_2D;
    gl_params.internal_format = gl_format;
    gl_params.format = gl_format;
    gl_params.type = GL_UNSIGNED_BYTE;
    gl_params.is_cleared = true;
    gpu::SharedImageBackingGLCommon::UnpackStateAttribs gl_attribs;

    auto shared_image = std::make_unique<gpu::SharedImageBackingGLImage>(
        gl_image, mailbox, viz_resource_format, frame.image_size, color_space,
        kTopLeft_GrSurfaceOrigin, kOpaque_SkAlphaType, shared_image_usage,
        gl_params, gl_attribs, gl_client_.is_passthrough);

    const bool success = shared_image_stub->factory()->RegisterBacking(
        std::move(shared_image), /* legacy_mailbox */ true);
    if (!success) {
      DLOG(ERROR) << "Failed to register shared image";
      NotifyError(PLATFORM_FAILURE, SFT_PLATFORM_ERROR);
      return false;
    }

    // Wrap the destroy callback in a lambda that ensures that it be called on
    // the appropriate thread.
    auto destroy_shared_image_lambda =
        [](gpu::SharedImageStub::SharedImageDestructionCallback callback,
           scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
          task_runner->PostTask(
              FROM_HERE, base::BindOnce(std::move(callback), gpu::SyncToken()));
        };
    auto destroy_shared_image_callback = base::BindOnce(
        destroy_shared_image_lambda,
        shared_image_stub->GetSharedImageDestructionCallback(mailbox),
        gpu_task_runner_);
    scoped_shared_image = scoped_refptr<Picture::ScopedSharedImage>(
        new Picture::ScopedSharedImage(
            mailbox, gl_params.target,
            std::move(destroy_shared_image_callback)));
  } else {
    if (!gl_client_.bind_image.Run(picture_info->client_texture_id,
                                   gpu::GetPlatformSpecificTextureTarget(),
                                   gl_image, false)) {
      DLOG(ERROR) << "Failed to bind image";
      NotifyError(PLATFORM_FAILURE, SFT_PLATFORM_ERROR);
      return false;
    }
  }
  picture_info->gl_image = gl_image;
  picture_info->bitstream_id = frame.bitstream_id;
  available_picture_ids_.pop_back();

  DVLOG(3) << "PictureReady(picture_id=" << picture_id << ", "
           << "bitstream_id=" << frame.bitstream_id << ")";
  Picture picture(picture_id, frame.bitstream_id, gfx::Rect(frame.image_size),
                  color_space, true);
  // The GLImageIOSurface keeps the IOSurface alive as long as it exists, but
  // bound textures do not, and they can outlive the GLImageIOSurface if they
  // are deleted in the command buffer before they are used by the platform GL
  // implementation. (https://crbug.com/930479#c69)
  //
  // A fence is required whenever a GLImage is bound, but we can't know in
  // advance whether that will happen.
  //
  // TODO(sandersd): Can GLImageIOSurface be responsible for fences, so that
  // we don't need to use them when the image is never bound? Bindings are
  // typically only created when WebGL is in use.
  picture.set_read_lock_fences_enabled(true);
  picture.set_scoped_shared_image(scoped_shared_image);
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
  decoder_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VTVideoDecodeAccelerator::FlushTask,
                                base::Unretained(this), type));

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

  // In a forceful shutdown, the decoder thread may be dead already.
  if (!decoder_thread_.IsRunning()) {
    delete this;
    return;
  }

  // For a graceful shutdown, return assigned buffers and flush before
  // destructing |this|.
  for (int32_t bitstream_id : assigned_bitstream_ids_)
    client_->NotifyEndOfBitstreamBuffer(bitstream_id);
  assigned_bitstream_ids_.clear();
  state_ = STATE_DESTROYING;
  QueueFlush(TASK_DESTROY);

  // Prevent calling into a deleted MediaLog.
  media_log_ = nullptr;
}

bool VTVideoDecodeAccelerator::TryToSetupDecodeOnSeparateThread(
    const base::WeakPtr<Client>& decode_client,
    const scoped_refptr<base::SingleThreadTaskRunner>& decode_task_runner) {
  return false;
}

bool VTVideoDecodeAccelerator::SupportsSharedImagePictureBuffers() const {
  // TODO(https://crbug.com/1108909): Enable shared image use on macOS.
  return false;
}

// static
VideoDecodeAccelerator::SupportedProfiles
VTVideoDecodeAccelerator::GetSupportedProfiles() {
  SupportedProfiles profiles;
  if (!InitializeVideoToolbox())
    return profiles;

  for (const auto& supported_profile : kSupportedProfiles) {
    if (supported_profile == VP9PROFILE_PROFILE0 ||
        supported_profile == VP9PROFILE_PROFILE2) {
      if (!base::mac::IsAtLeastOS11())
        continue;
      if (!base::FeatureList::IsEnabled(kVideoToolboxVp9Decoding))
        continue;
      if (__builtin_available(macOS 10.13, *)) {
        if ((supported_profile == VP9PROFILE_PROFILE0 ||
             supported_profile == VP9PROFILE_PROFILE2) &&
            !VTIsHardwareDecodeSupported(kCMVideoCodecType_VP9)) {
          continue;
        }

        // Success! We have VP9 hardware decoding support.
      } else {
        continue;
      }
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
