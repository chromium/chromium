// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/vt_video_encode_accelerator_mac.h"

#import <Foundation/Foundation.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <vector>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/osstatus_logging.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "media/base/bitrate.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/encoder_status.h"
#include "media/base/mac/color_space_util_mac.h"
#include "media/base/mac/video_frame_mac.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/gpu/command_buffer_helper.h"
#include "media/gpu/gpu_video_encode_accelerator_helpers.h"
#include "media/gpu/mac/vt_hdr_metadata.h"
#include "media/media_buildflags.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/hdr_metadata_mac.h"
#include "ui/gfx/mac/io_surface.h"

using base::apple::CFToNSPtrCast;
using base::apple::NSToCFOwnershipCast;
using base::apple::NSToCFPtrCast;

#define SOFTWARE_ENCODING_SUPPORTED BUILDFLAG(IS_MAC)

namespace media {

struct SharedImageEncodeAccess
    : public base::RefCountedDeleteOnSequence<SharedImageEncodeAccess> {
  SharedImageEncodeAccess()
      : base::RefCountedDeleteOnSequence<SharedImageEncodeAccess>(
            base::SequencedTaskRunner::GetCurrentDefault()) {}

  SharedImageEncodeAccess(const SharedImageEncodeAccess&) = delete;
  SharedImageEncodeAccess& operator=(const SharedImageEncodeAccess&) = delete;

  // Destroyed in reverse declaration order so scoped_access ends first and the
  // helper-provided memory tracker outlives the representation.
  scoped_refptr<CommandBufferHelper> command_buffer_helper;
  std::unique_ptr<gpu::OverlayImageRepresentation> representation;
  std::unique_ptr<gpu::OverlayImageRepresentation::ScopedReadAccess>
      scoped_access;

 private:
  friend class base::RefCountedDeleteOnSequence<SharedImageEncodeAccess>;
  friend class base::DeleteHelper<SharedImageEncodeAccess>;
  ~SharedImageEncodeAccess() = default;
};

using EncoderType = VideoEncodeAccelerator::Config::EncoderType;

namespace {

constexpr size_t kMaxFrameRateNumerator = 120;
constexpr size_t kMaxFrameRateDenominator = 1;
constexpr size_t kNumInputBuffers = 3;
constexpr gfx::Size kDefaultSupportedResolution = gfx::Size(640, 480);
constexpr int kH26xMaxQp = 51;

// Configures a PQ session for HDR metadata. `hdr_metadata` is recorded on the
// format description of every encoded sample, and asking for metadata insertion
// additionally gets it into the bitstream on encoders that implement it.
void ConfigureVtSessionForHdrMetadata(
    video_toolbox::SessionPropertySetter& session_property_setter,
    const std::optional<gfx::HDRMetadata>& hdr_metadata) {
  if (hdr_metadata && hdr_metadata->HasMDCV() &&
      session_property_setter.IsSupported(
          kVTCompressionPropertyKey_MasteringDisplayColorVolume)) {
    if (auto mdcv = gfx::GenerateMasteringDisplayColorVolume(*hdr_metadata)) {
      if (!session_property_setter.Set(
              kVTCompressionPropertyKey_MasteringDisplayColorVolume,
              mdcv.get())) {
        DLOG(ERROR) << "Failed to set MasteringDisplayColorVolume on "
                       "VTCompressionSession.";
      }
    }
  }

  if (hdr_metadata && hdr_metadata->HasCLLI() &&
      session_property_setter.IsSupported(
          kVTCompressionPropertyKey_ContentLightLevelInfo)) {
    if (auto clli = gfx::GenerateContentLightLevelInfo(*hdr_metadata)) {
      if (!session_property_setter.Set(
              kVTCompressionPropertyKey_ContentLightLevelInfo, clli.get())) {
        DLOG(ERROR) << "Failed to set ContentLightLevelInfo on "
                       "VTCompressionSession.";
      }
    }
  }

  if (session_property_setter.IsSupported(
          kVTCompressionPropertyKey_HDRMetadataInsertionMode) &&
      !session_property_setter.Set(
          kVTCompressionPropertyKey_HDRMetadataInsertionMode,
          kVTHDRMetadataInsertionMode_Auto)) {
    DLOG(ERROR) << "Failed to set HDRMetadataInsertionMode on "
                   "VTCompressionSession.";
  }
}

#if SOFTWARE_ENCODING_SUPPORTED
// The IDs of the encoders that may be selected when we enable low latency via
// `kVTVideoEncoderSpecification_EnableLowLatencyRateControl`. Low latency is
// in general only possible with a hardware encoder in VideoToolbox, so we
// assume these are in fact hardware encoders. For some reason, neither
// `VTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder` nor
// `kVTVideoEncoderList_IsHardwareAccelerated` is set for these encoders.
constexpr std::string_view kRealtimeHardwareEncoderIDs[] = {
    "com.apple.videotoolbox.videoencoder.h264.rtvc",
    "com.apple.videotoolbox.videoencoder.hevc.rtvc",
};
#endif  // SOFTWARE_ENCODING_SUPPORTED

base::span<const VideoCodecProfile> GetSupportedVideoCodecProfiles() {
  static const base::NoDestructor<std::vector<VideoCodecProfile>> kProfiles(
      []() {
        std::vector<VideoCodecProfile> profiles{
            H264PROFILE_BASELINE,
            H264PROFILE_MAIN,
            H264PROFILE_HIGH,
        };
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
        if (base::FeatureList::IsEnabled(kPlatformHEVCEncoderSupport)) {
          profiles.push_back(HEVCPROFILE_MAIN);
          if (base::FeatureList::IsEnabled(kPlatformHEVCMain10EncoderSupport)) {
            profiles.push_back(HEVCPROFILE_MAIN10);
          }
        }
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
        return profiles;
      }());
  return *kProfiles;
}

base::span<const gfx::Size> GetMinResolutions(VideoCodec codec) {
#if defined(ARCH_CPU_X86_FAMILY)
  // Below test result based on a 2019 Intel MacBook Pro, and a 2015
  // Intel MacBook Pro.
  static constexpr auto kMinH264Resolutions = std::to_array({
      gfx::Size(640, 2),
      gfx::Size(18, 480),
  });
  static constexpr auto kMinHEVCResolutions = std::to_array({
      gfx::Size(146, 50),
  });
#else
  // Below test result based on a 2021 M1 Pro MacBook Pro, and a 2024
  // M4 Mac Mini.
  static constexpr auto kMinH264Resolutions = std::to_array({
      gfx::Size(16, 16),
  });
  static constexpr auto kMinHEVCResolutions = std::to_array({
      gfx::Size(16, 16),
  });
#endif  // defined(ARCH_CPU_X86_FAMILY)
  switch (codec) {
    case VideoCodec::kH264:
      return kMinH264Resolutions;
    case VideoCodec::kHEVC:
      return kMinHEVCResolutions;
    default:
      NOTREACHED();
  }
}

gfx::Size GetMaxResolution(VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kH264:
      // Test result on a M1 Pro Mac shows that the max supported resolution of
      // H.264 is 4096 x 2304 if encode mode is real time, for none real time
      // mode, the max supported resolution is 4096 x 4096. On some Intel Macs,
      // this can go up to 8K, however, due to the excessive number of chips in
      // x64 Macs, use the conservative values of 4096 x 2304 here.
      return gfx::Size(4096, 2304);
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    case VideoCodec::kHEVC:
#if defined(ARCH_CPU_ARM_FAMILY)
      // Test result on a M1 Pro Mac shows that the max supported resolution of
      // HEVC is 8192 x 4352 if encode mode is real time, for none real time
      // mode, the max supported resolution is 16384 x 8192. Use the
      // conservative values of 8192 x 4352 here.
      return gfx::Size(8192, 4352);
#else
      // On some Intel Macs, this can go up to 8K, however, due to the excessive
      // number of chips in x64 Macs, use the conservative values of 4096 x 2304
      // here.
      return gfx::Size(4096, 2304);
#endif  // defined(ARCH_CPU_ARM_FAMILY)
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    default:
      NOTREACHED();
  }
}

bool IsSVCSupported(VideoCodecProfile profile) {
  const VideoCodec codec = VideoCodecProfileToVideoCodec(profile);
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER) && defined(ARCH_CPU_ARM_FAMILY)
  // macOS 14.0+ support SVC HEVC encoding for Apple Silicon chips only.
  if (profile == HEVCPROFILE_MAIN) {
    if (@available(macOS 14.0, iOS 17.0, *)) {
      return true;
    }
    return false;
  }
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER) &&
        // defined(ARCH_CPU_ARM_FAMILY)
  return codec == VideoCodec::kH264;
}

bool IsManualQpSupported(VideoCodecProfile profile) {
  // Querying `kVTCompressionPropertyKey_SupportsBaseFrameQP` is the
  // way Apple recommends to test whether per frame QP is supported by a
  // given encoder. Based on tests on Intel and Apple Silicon Macs,
  // the property `kVTCompressionPropertyKey_SupportsBaseFrameQP` will
  // only report `supported` for encoders created with
  // `kVTVideoEncoderSpecification_EnableLowLatencyRateControl` set to
  // `true`. Thus, we assume external mode is supported if SVC is
  // supported.
  return IsSVCSupported(profile);
}

static CFStringRef VideoCodecProfileToVTProfile(VideoCodecProfile profile) {
  switch (profile) {
    case H264PROFILE_BASELINE:
      return kVTProfileLevel_H264_Baseline_AutoLevel;
    case H264PROFILE_MAIN:
      return kVTProfileLevel_H264_Main_AutoLevel;
    case H264PROFILE_HIGH:
      return kVTProfileLevel_H264_High_AutoLevel;
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    case HEVCPROFILE_MAIN:
      return kVTProfileLevel_HEVC_Main_AutoLevel;
    case HEVCPROFILE_MAIN10:
      return kVTProfileLevel_HEVC_Main10_AutoLevel;
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    default:
      NOTREACHED();
  }
}

static CMVideoCodecType VideoCodecToCMVideoCodec(VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kH264:
      return kCMVideoCodecType_H264;
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    case VideoCodec::kHEVC:
      return kCMVideoCodecType_HEVC;
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    default:
      NOTREACHED();
  }
}

bool IsHardwareEncoder(VTSessionRef compression_session) {
#if SOFTWARE_ENCODING_SUPPORTED
  base::apple::ScopedCFTypeRef<CFBooleanRef> using_hardware;
  if (VTSessionCopyProperty(
          compression_session,
          kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder,
          kCFAllocatorDefault, using_hardware.InitializeInto()) == noErr &&
      // `using_hardware` might not get initialized even if `noErr` is
      // returned, see crbug.com/c/369540616.
      using_hardware) {
    return CFBooleanGetValue(using_hardware.get());
  }
  DVLOG(1) << "Couldn't read the UsingHardwareAcceleratedVideoEncoder property";

  base::apple::ScopedCFTypeRef<CFStringRef> encoder_id;
  if (VTSessionCopyProperty(
          compression_session, kVTCompressionPropertyKey_EncoderID,
          kCFAllocatorDefault, encoder_id.InitializeInto()) == noErr) {
    if (std::ranges::contains(kRealtimeHardwareEncoderIDs,
                              base::SysCFStringRefToUTF8(encoder_id.get()))) {
      DVLOG(1) << "But " << encoder_id.get() << " is a known hardware encoder";
      return true;
    }
    DVLOG(1) << "Assuming " << encoder_id.get() << " to be a software encoder";
  }

  return false;
#else
  return true;
#endif  // SOFTWARE_ENCODING_SUPPORTED
}

base::apple::ScopedCFTypeRef<CFDictionaryRef> CreateSourceImageBufferAttributes(
    VideoCodecProfile profile,
    const gfx::Size& size,
    gfx::ColorSpace::RangeID source_range = gfx::ColorSpace::RangeID::LIMITED) {
  if (profile != HEVCPROFILE_MAIN10) {
    return base::apple::ScopedCFTypeRef<CFDictionaryRef>();
  }
  // VideoToolbox assumes 8-bit source buffers unless the session is told
  // about the 10-bit source format and range.
  const OSType pixel_format =
      source_range == gfx::ColorSpace::RangeID::FULL
          ? kCVPixelFormatType_420YpCbCr10BiPlanarFullRange
          : kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange;
  NSDictionary* attrs = @{
    CFToNSPtrCast(kCVPixelBufferPixelFormatTypeKey) : @(pixel_format),
    CFToNSPtrCast(kCVPixelBufferWidthKey) : @(size.width()),
    CFToNSPtrCast(kCVPixelBufferHeightKey) : @(size.height()),
  };
  return base::apple::ScopedCFTypeRef<CFDictionaryRef>(
      NSToCFOwnershipCast(attrs));
}

base::expected<video_toolbox::ScopedVTCompressionSessionRef, OSStatus>
CreateCompressionSession(
    VideoCodecProfile profile,
    const gfx::Size& input_size,
    EncoderType required_encoder_type,
    bool require_low_delay,
    VTCompressionOutputCallback output_callback = nullptr,
    VTVideoEncodeAccelerator* accelerator = nullptr,
    CFDictionaryRef source_image_buffer_attributes = nullptr) {
  CHECK_EQ(!output_callback, !accelerator);

  NSMutableDictionary* encoder_spec = [NSMutableDictionary dictionary];

  // When we're always hardware-accelerated anyway, encoder configuration
  // handling is not necessary.
#if SOFTWARE_ENCODING_SUPPORTED
  if (required_encoder_type == EncoderType::kHardware) {
    encoder_spec[CFToNSPtrCast(
        kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder)] =
        @YES;
  } else {
    encoder_spec[CFToNSPtrCast(
        kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder)] =
        @NO;
  }

  if (required_encoder_type == EncoderType::kSoftware) {
    encoder_spec[CFToNSPtrCast(
        kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder)] =
        @NO;
  }
#endif  // SOFTWARE_ENCODING_SUPPORTED

  // Don't enable low-latency rate control in SW mode as it doesn't seem to
  // apply to the SW encoder. From
  // https://developer.apple.com/videos/play/wwdc2021/10158/, "[...] the
  // low-latency mode always uses a hardware-accelerated video encoder". In
  // fact, trying to use
  // `kVTVideoEncoderSpecification_EnableLowLatencyRateControl` with the SW
  // encoder leads to an initialization error.
  if (required_encoder_type != EncoderType::kSoftware && require_low_delay &&
      IsSVCSupported(profile)) {
    encoder_spec[CFToNSPtrCast(
        kVTVideoEncoderSpecification_EnableLowLatencyRateControl)] = @YES;
  }

  // Create the compression session.
  // Note that the encoder object is given to the compression session as the
  // callback context using a raw pointer. The C API does not allow us to use a
  // smart pointer, nor is this encoder ref counted. However, this is still
  // safe, because we 1) we own the compression session and 2) we tear it down
  // safely. When destructing the encoder, the compression session is flushed
  // and invalidated. Internally, VideoToolbox will join all of its threads
  // before returning to the client. Therefore, when control returns to us, we
  // are guaranteed that the output callback will not execute again.
  video_toolbox::ScopedVTCompressionSessionRef session;
  const OSStatus status = VTCompressionSessionCreate(
      kCFAllocatorDefault, input_size.width(), input_size.height(),
      VideoCodecToCMVideoCodec(VideoCodecProfileToVideoCodec(profile)),
      NSToCFPtrCast(encoder_spec), source_image_buffer_attributes,
      /*compressedDataAllocator=*/nullptr, output_callback,
      reinterpret_cast<void*>(accelerator), session.InitializeInto());
  if (status != noErr) {
    return base::unexpected(status);
  }
  DVLOG(3) << " VTCompressionSession created with input size="
           << input_size.ToString();
  return session;
}

bool CanCreateHardwareCompressionSession(VideoCodecProfile profile) {
  auto session = CreateCompressionSession(
      profile, kDefaultSupportedResolution, EncoderType::kHardware,
      /*require_low_delay=*/false, /*output_callback=*/nullptr,
      /*accelerator=*/nullptr,
      CreateSourceImageBufferAttributes(profile, kDefaultSupportedResolution)
          .get());
  const bool can_create_hardware_session =
      session.has_value() &&
      (profile != HEVCPROFILE_MAIN10 ||
       video_toolbox::SessionPropertySetter(session.value())
           .Set(kVTCompressionPropertyKey_ProfileLevel,
                kVTProfileLevel_HEVC_Main10_AutoLevel));
  DVLOG_IF(1, !can_create_hardware_session)
      << "Hardware " << GetProfileName(profile)
      << " encode acceleration is not available on this platform.";
  return can_create_hardware_session;
}

// Returns the profile to probe when checking hardware encode for `profile`.
// H.264 baseline/main/high share the same codec type and session parameters, so
// one probe covers all of them. HEVC Main10 is distinct because it needs
// 10-bit source-buffer attributes and its own profile level.
VideoCodecProfile HardwareEncodeProbeProfile(VideoCodecProfile profile) {
  switch (profile) {
    case H264PROFILE_BASELINE:
    case H264PROFILE_MAIN:
    case H264PROFILE_HIGH:
      return H264PROFILE_BASELINE;
    default:
      return profile;
  }
}

std::vector<VideoPixelFormat> GpuSupportedPixelFormatsForProfile(
    VideoCodecProfile profile) {
  if (profile == HEVCPROFILE_MAIN10) {
    return {PIXEL_FORMAT_P010LE};
  }
  return {PIXEL_FORMAT_NV12};
}

VideoEncoderInfo GetVideoEncoderInfo(
    VTSessionRef compression_session,
    const VTVideoEncodeAccelerator::Config& config) {
  VideoEncoderInfo info;
  info.implementation_name = "VideoToolbox";
  info.is_hardware_accelerated = IsHardwareEncoder(compression_session);

  // TODO(crbug.com/382015342): Query bitrate limits, and report them through
  // VideoEncoderInfo's |resolution_bitrate_limits|.
  const VideoCodec codec = VideoCodecProfileToVideoCodec(config.output_profile);
  gfx::Size resolution = GetMaxResolution(codec);
  info.resolution_rate_limits.emplace_back(
      resolution, /*min_start_bitrate_bps=*/0,
      /*min_bitrate_bps=*/0, /*max_bitrate_bps=*/0, kMaxFrameRateNumerator,
      kMaxFrameRateDenominator);
  if (resolution.width() != resolution.height()) {
    resolution.Transpose();
    info.resolution_rate_limits.emplace_back(
        resolution,
        /*min_start_bitrate_bps=*/0, /*min_bitrate_bps=*/0,
        /*max_bitrate_bps=*/0, kMaxFrameRateNumerator,
        kMaxFrameRateDenominator);
  }

  std::optional<int> max_frame_delay_property;
  base::apple::ScopedCFTypeRef<CFNumberRef> max_frame_delay_count;
  if (VTSessionCopyProperty(
          compression_session, kVTCompressionPropertyKey_MaxFrameDelayCount,
          kCFAllocatorDefault, max_frame_delay_count.InitializeInto()) == 0) {
    int32_t frame_delay;
    if (CFNumberGetValue(max_frame_delay_count.get(), kCFNumberSInt32Type,
                         &frame_delay) &&
        frame_delay != kVTUnlimitedFrameDelayCount &&
        // For Apple Silicon Macs using macOS 15.0, it seems we can't
        // set `kVTCompressionPropertyKey_MaxFrameDelayCount` property
        // successfully, and its value is always equal to 0 instead of
        // `kVTUnlimitedFrameDelayCount`, we should use the default
        // value of `VideoEncoderInfo` instead.
        frame_delay != 0) {
      max_frame_delay_property = frame_delay;
    }
  }
  // Not all VideoToolbox encoders are created equal. The numbers below match
  // the characteristics of an Apple Silicon M1 laptop. It has been noted that,
  // for example, the HW encoder in a 2014 (Intel) machine has a smaller
  // capacity. And while overestimating the capacity is not a problem,
  // underestimating the frame delay is, so these numbers might need tweaking
  // in the face of new evidence.
  if (info.is_hardware_accelerated) {
    info.frame_delay = 0;
    info.input_capacity = 10;
  } else {
    info.frame_delay = config.output_profile == H264PROFILE_BASELINE ? 0 : 13;
    info.input_capacity = info.frame_delay.value() + 4;
  }
  if (max_frame_delay_property.has_value()) {
    info.frame_delay =
        std::min(info.frame_delay.value(), max_frame_delay_property.value());
    info.input_capacity =
        std::min(info.input_capacity.value(), max_frame_delay_property.value());
  }
  if (config.HasSpatialLayer() || config.HasTemporalLayer()) {
    CHECK(!config.spatial_layers.empty());
    for (size_t i = 0; i < config.spatial_layers.size(); ++i) {
      // Only L1T1, L1T2 are supported.
      CHECK_LE(config.spatial_layers[i].num_of_temporal_layers, 2);
      info.fps_allocation[i] =
          GetFpsAllocation(config.spatial_layers[i].num_of_temporal_layers);
    }
  } else {
    constexpr uint8_t kFullFramerate = 255;
    info.fps_allocation[0] = {kFullFramerate};
  }
  CHECK(info.reports_average_qp);

  if (base::FeatureList::IsEnabled(
          kVTVideoEncodeAcceleratorOpaqueSharedImageEncode)) {
    info.gpu_supported_pixel_formats =
        GpuSupportedPixelFormatsForProfile(config.output_profile);
    info.supports_gpu_shared_images = true;
  }

  return info;
}

using PixelBufferResolvedCB =
    base::OnceCallback<void(base::apple::ScopedCFTypeRef<CVPixelBufferRef>,
                            scoped_refptr<SharedImageEncodeAccess>,
                            EncoderStatus)>;

using CommandBufferHelperResolvedCB =
    base::OnceCallback<void(scoped_refptr<CommandBufferHelper>)>;

// Called after the acquire sync token is released.
void CreatePixelBufferFromSharedImage(scoped_refptr<CommandBufferHelper> helper,
                                      scoped_refptr<VideoFrame> frame,
                                      PixelBufferResolvedCB done_cb) {
  TRACE_EVENT0("media",
               "VTVideoEncodeAccelerator::CreatePixelBufferFromSharedImage");
  DCHECK(helper);
  DCHECK(frame);

  gpu::SharedImageManager* shared_image_manager =
      helper->GetSharedImageManager();
  if (!shared_image_manager) {
    std::move(done_cb).Run(base::apple::ScopedCFTypeRef<CVPixelBufferRef>(),
                           nullptr,
                           {EncoderStatus::Codes::kEncoderFailedEncode,
                            "SharedImageManager is not available"});
    return;
  }

  auto access = base::MakeRefCounted<SharedImageEncodeAccess>();
  access->command_buffer_helper = helper;
  access->representation = shared_image_manager->ProduceOverlay(
      frame->shared_image()->mailbox(), helper->GetMemoryTypeTracker());
  if (!access->representation) {
    std::move(done_cb).Run(base::apple::ScopedCFTypeRef<CVPixelBufferRef>(),
                           nullptr,
                           {EncoderStatus::Codes::kEncoderFailedEncode,
                            "ProduceOverlay failed for SharedImage"});
    return;
  }

  if (access->representation->size() != frame->coded_size()) {
    std::move(done_cb).Run(base::apple::ScopedCFTypeRef<CVPixelBufferRef>(),
                           nullptr,
                           {EncoderStatus::Codes::kEncoderFailedEncode,
                            "SharedImage size mismatch"});
    return;
  }

  access->scoped_access = access->representation->BeginScopedReadAccess();
  if (!access->scoped_access) {
    std::move(done_cb).Run(base::apple::ScopedCFTypeRef<CVPixelBufferRef>(),
                           nullptr,
                           {EncoderStatus::Codes::kEncoderFailedEncode,
                            "BeginScopedReadAccess failed for SharedImage"});
    return;
  }

  gfx::ScopedIOSurface io_surface = access->scoped_access->GetIOSurface();
  if (!io_surface) {
    std::move(done_cb).Run(base::apple::ScopedCFTypeRef<CVPixelBufferRef>(),
                           nullptr,
                           {EncoderStatus::Codes::kEncoderFailedEncode,
                            "SharedImage is not IOSurface-backed"});
    return;
  }

  auto pixel_buffer = WrapIOSurfaceInCVPixelBuffer(*frame, io_surface.get());
  if (!pixel_buffer) {
    std::move(done_cb).Run(base::apple::ScopedCFTypeRef<CVPixelBufferRef>(),
                           nullptr,
                           {EncoderStatus::Codes::kEncoderFailedEncode,
                            "WrapIOSurfaceInCVPixelBuffer failed"});
    return;
  }

  std::move(done_cb).Run(std::move(pixel_buffer), std::move(access),
                         EncoderStatus::Codes::kOk);
}

// Waits for |frame|'s acquire sync token, then creates a CVPixelBuffer.
void ResolveSharedImageOnGpuThread(scoped_refptr<CommandBufferHelper> helper,
                                   scoped_refptr<VideoFrame> frame,
                                   PixelBufferResolvedCB done_cb,
                                   base::ScopedClosureRunner done_guard) {
  DCHECK(helper);
  DCHECK(frame);
  DCHECK(frame->HasSharedImage());

  auto sync_token = frame->acquire_sync_token();
  helper->WaitForSyncToken(
      sync_token,
      base::BindOnce(
          [](scoped_refptr<CommandBufferHelper> helper,
             scoped_refptr<VideoFrame> frame, PixelBufferResolvedCB callback,
             base::ScopedClosureRunner callback_guard) {
            callback_guard.ReplaceClosure(base::OnceClosure());
            CreatePixelBufferFromSharedImage(
                std::move(helper), std::move(frame), std::move(callback));
          },
          helper, std::move(frame), std::move(done_cb), std::move(done_guard)));
}

}  // namespace

struct VTVideoEncodeAccelerator::PendingEncode {
  PendingEncode(scoped_refptr<VideoFrame> frame,
                const VideoEncoder::EncodeOptions& options)
      : frame(std::move(frame)), options(options) {}
  PendingEncode(PendingEncode&&) = default;
  PendingEncode& operator=(PendingEncode&&) = default;
  PendingEncode(const PendingEncode&) = delete;
  PendingEncode& operator=(const PendingEncode&) = delete;
  ~PendingEncode() = default;

  scoped_refptr<VideoFrame> frame;
  VideoEncoder::EncodeOptions options;
  bool resolve_requested = false;
};

struct VTVideoEncodeAccelerator::InProgressFrameEncode {
  InProgressFrameEncode(
      scoped_refptr<VideoFrame> frame,
      const gfx::ColorSpace& frame_cs,
      std::optional<int> frame_qp,
      scoped_refptr<SharedImageEncodeAccess> shared_image_access = nullptr)
      : frame(std::move(frame)),
        encoded_color_space(frame_cs),
        qp(frame_qp),
        shared_image_access(std::move(shared_image_access)) {}
  const scoped_refptr<VideoFrame> frame;
  const gfx::ColorSpace encoded_color_space;
  const std::optional<int> qp;
  scoped_refptr<SharedImageEncodeAccess> shared_image_access;
};

struct VTVideoEncodeAccelerator::EncodeOutput {
  EncodeOutput() = delete;

  EncodeOutput(VTEncodeInfoFlags info_flags,
               CMSampleBufferRef sbuf,
               const InProgressFrameEncode& frame_info)
      : info(info_flags),
        sample_buffer(sbuf, base::scoped_policy::RETAIN),
        capture_timestamp(frame_info.frame->timestamp()),
        encoded_color_space(frame_info.encoded_color_space),
        qp(frame_info.qp) {}

  EncodeOutput(const EncodeOutput&) = delete;
  EncodeOutput& operator=(const EncodeOutput&) = delete;

  const VTEncodeInfoFlags info;
  const base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample_buffer;
  const base::TimeDelta capture_timestamp;
  const gfx::ColorSpace encoded_color_space;
  const std::optional<int> qp;
};

struct VTVideoEncodeAccelerator::BitstreamBufferRef {
  BitstreamBufferRef() = delete;

  BitstreamBufferRef(int32_t id,
                     base::WritableSharedMemoryMapping mapping,
                     size_t size)
      : id(id), mapping(std::move(mapping)), size(size) {}

  BitstreamBufferRef(const BitstreamBufferRef&) = delete;
  BitstreamBufferRef& operator=(const BitstreamBufferRef&) = delete;

  const int32_t id;
  base::WritableSharedMemoryMapping mapping;
  const size_t size;
};

VTVideoEncodeAccelerator::VTVideoEncodeAccelerator()
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  encoder_weak_ptr_ = encoder_weak_factory_.GetWeakPtr();
}

VTVideoEncodeAccelerator::~VTVideoEncodeAccelerator() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Flush the compression session and make it join its internal threads. After
  // this, no callbacks will be issued by the session and we can proceed with
  // the destruction of VTVideoEncodeAccelerator.
  compression_session_.reset();
}

VideoEncodeAccelerator::SupportedProfiles
VTVideoEncodeAccelerator::GetSupportedProfiles() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SupportedProfiles supported_profiles;

  SupportedProfile supported_profile;
  supported_profile.max_framerate_numerator = kMaxFrameRateNumerator;
  supported_profile.max_framerate_denominator = kMaxFrameRateDenominator;
  // Advertise VBR here, even though the peak bitrate is never actually used.
  // See RequestEncodingParametersChange() for more details.
  const SupportedRateControlMode always_supported_rate_control_modes =
      VideoEncodeAccelerator::kConstantMode |
      VideoEncodeAccelerator::kVariableMode;
  // L1T1 = no additional spatial and temporal layer = always supported.
  const std::vector<SVCScalabilityMode> always_supported_scalability_modes{
      SVCScalabilityMode::kL1T1};

  // A cache for CanCreateHardwareCompressionSession() results, which can be
  // costly to compute. Keyed by probe profile so H.264 baseline/main/high share
  // one probe while HEVC Main10 stays distinct. The factory already caches the
  // full SupportedProfiles list per GPU process.
  base::flat_map<VideoCodecProfile, bool> can_create_hardware_session;

  for (const VideoCodecProfile profile : GetSupportedVideoCodecProfiles()) {
    const VideoCodec codec = VideoCodecProfileToVideoCodec(profile);
    const VideoCodecProfile probe_profile = HardwareEncodeProbeProfile(profile);
    if (can_create_hardware_session.find(probe_profile) ==
        can_create_hardware_session.end()) {
      can_create_hardware_session[probe_profile] =
          CanCreateHardwareCompressionSession(probe_profile);
    }

    supported_profile.profile = profile;
    supported_profile.max_resolution = GetMaxResolution(codec);

    for (const auto& min_resolution : GetMinResolutions(codec)) {
      supported_profile.min_resolution = min_resolution;
      supported_profile.is_software_codec = false;
      supported_profile.scalability_modes = always_supported_scalability_modes;
      supported_profile.rate_control_modes =
          always_supported_rate_control_modes;
      if (IsSVCSupported(profile)) {
        supported_profile.scalability_modes.push_back(
            SVCScalabilityMode::kL1T2);
      }
      if (IsManualQpSupported(profile)) {
        supported_profile.rate_control_modes |=
            VideoEncodeAccelerator::kExternalMode;
      }
      if (base::FeatureList::IsEnabled(
              kVTVideoEncodeAcceleratorOpaqueSharedImageEncode)) {
        supported_profile.gpu_supported_pixel_formats =
            GpuSupportedPixelFormatsForProfile(profile);
        supported_profile.supports_gpu_shared_images = true;
      }
      if (can_create_hardware_session[probe_profile]) {
        supported_profiles.push_back(supported_profile);

        SupportedProfile portrait_profile(supported_profile);
        portrait_profile.max_resolution.Transpose();
        supported_profiles.push_back(portrait_profile);
      }

#if SOFTWARE_ENCODING_SUPPORTED
      // macOS doesn't provide a way to enumerate codec details, so just
      // assume software codec support is the same as hardware.
      //
      // NOTE: Although SW encoder always has lower supported min resolutions
      // compared with HW encoder, but when both HW and SW encoder exist and if
      // the resolution is not supported by hardware but supported by software,
      // and if you set `no-preference`, VT will always emit an error. Thus,
      // we should just re-use min resolutions of HW encoder for SW encoder.
      supported_profile.scalability_modes = always_supported_scalability_modes;
      supported_profile.rate_control_modes =
          always_supported_rate_control_modes;
      supported_profile.is_software_codec = true;
      supported_profiles.push_back(supported_profile);

      SupportedProfile portrait_profile(supported_profile);
      portrait_profile.max_resolution.Transpose();
      supported_profiles.push_back(portrait_profile);
#endif  // SOFTWARE_ENCODING_SUPPORTED
    }
  }
  return supported_profiles;
}

EncoderStatus VTVideoEncodeAccelerator::Initialize(
    const Config& config,
    Client* client,
    std::unique_ptr<MediaLog> media_log) {
  DVLOG(3) << __func__ << ": " << config.AsHumanReadableString();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client);

  // Clients are expected to call Flush() before reinitializing the encoder.
  DCHECK_EQ(pending_encodes_, 0);

  const bool input_format_supported =
      config.output_profile == HEVCPROFILE_MAIN10
          ? config.input_format == PIXEL_FORMAT_P010LE
          : (config.input_format == PIXEL_FORMAT_I420 ||
             config.input_format == PIXEL_FORMAT_NV12);
  if (!input_format_supported) {
    MEDIA_LOG(ERROR, media_log)
        << "Input format " << VideoPixelFormatToString(config.input_format)
        << " is not supported for " << GetProfileName(config.output_profile);
    return {EncoderStatus::Codes::kEncoderInitializationError};
  }
  if (!std::ranges::contains(GetSupportedVideoCodecProfiles(),
                             config.output_profile)) {
    MEDIA_LOG(ERROR, media_log) << "Output profile not supported= "
                                << GetProfileName(config.output_profile);
    return {EncoderStatus::Codes::kEncoderInitializationError};
  }
  input_format_ = config.input_format;
  profile_ = config.output_profile;
  codec_ = VideoCodecProfileToVideoCodec(config.output_profile);
  client_ = client;
  input_visible_size_ = config.input_visible_size;
  frame_rate_ = config.framerate;
  bitrate_ = config.bitrate;
  bitstream_buffer_size_ = EstimateBitstreamBufferSize(
      bitrate_, frame_rate_, config.input_visible_size);
  require_low_delay_ = config.require_low_delay;
  required_encoder_type_ = config.required_encoder_type;

  if (config.HasTemporalLayer()) {
    num_temporal_layers_ = config.spatial_layers.front().num_of_temporal_layers;
  }

  if (num_temporal_layers_ > 2) {
    MEDIA_LOG(ERROR, media_log) << "Unsupported number of SVC temporal layers.";
    return {EncoderStatus::Codes::kEncoderInitializationError};
  }

  if (config.bitrate.mode() == Bitrate::Mode::kExternal) {
    if (!IsManualQpSupported(profile_)) {
      MEDIA_LOG(ERROR, media_log) << "External bitrate mode is not supported.";
      return {EncoderStatus::Codes::kEncoderInitializationError};
    }
    if (!require_low_delay_) {
      MEDIA_LOG(INFO, media_log)
          << "Force enable low delay encoding for external bitrate mode.";
      require_low_delay_ = true;
    }
  }

  // We don't know the range of the source buffer yet, so we use LIMITED
  // initially
  if (!ResetCompressionSession(gfx::ColorSpace::RangeID::LIMITED)) {
    MEDIA_LOG(ERROR, media_log) << "Failed creating compression session.";
    return {EncoderStatus::Codes::kEncoderInitializationError};
  }

  auto encoder_info = GetVideoEncoderInfo(compression_session_.get(), config);

  // Report whether hardware encode is being used.
  if (!encoder_info.is_hardware_accelerated) {
    MEDIA_LOG(INFO, media_log) << "VideoToolbox selected a software encoder.";
  }

  media_log_ = std::move(media_log);

  client_->NotifyEncoderInfoChange(encoder_info);
  client_->RequireBitstreamBuffers(kNumInputBuffers, input_visible_size_,
                                   bitstream_buffer_size_);
  return {EncoderStatus::Codes::kOk};
}

void VTVideoEncodeAccelerator::Encode(scoped_refptr<VideoFrame> frame,
                                      bool force_keyframe) {
  Encode(std::move(frame), VideoEncoder::EncodeOptions(force_keyframe));
}

void VTVideoEncodeAccelerator::Encode(
    scoped_refptr<VideoFrame> frame,
    const VideoEncoder::EncodeOptions& options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(compression_session_);
  DCHECK(frame);

  if (frame->HasSharedImage() && !frame->HasMappableSharedImage() &&
      !CanEncodeOpaqueSharedImage(*frame)) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kEncoderFailedEncode,
         "Unsupported opaque SharedImage for VideoToolbox encode"});
    return;
  }

  pending_encode_queue_.push_back(
      std::make_unique<PendingEncode>(std::move(frame), options));
  ProcessPendingEncodes();
}

void VTVideoEncodeAccelerator::ProcessPendingEncodes() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  while (!pending_encode_queue_.empty()) {
    auto& pending = pending_encode_queue_.front();
    const bool needs_shared_image_resolve =
        pending->frame->HasSharedImage() &&
        !pending->frame->HasMappableSharedImage();
    if (needs_shared_image_resolve) {
      if (command_buffer_helper_failed_) {
        FailPendingEncodes(
            {EncoderStatus::Codes::kGPUCommandBufferNotAvailable,
             "CommandBufferHelper unavailable for opaque SharedImage encode"});
        return;
      }
      if (!command_buffer_helper_ || !gpu_task_runner_ ||
          pending->resolve_requested) {
        return;
      }

      pending->resolve_requested = true;
      auto resolve_cb = base::BindPostTaskToCurrentDefault(base::BindOnce(
          &VTVideoEncodeAccelerator::OnSharedImageResolved, encoder_weak_ptr_));
      auto [resolve_success_cb, resolve_cancelled_cb] =
          base::SplitOnceCallback(std::move(resolve_cb));
      base::ScopedClosureRunner resolve_guard(base::BindOnce(
          [](PixelBufferResolvedCB callback) {
            std::move(callback).Run(
                base::apple::ScopedCFTypeRef<CVPixelBufferRef>(),
                scoped_refptr<SharedImageEncodeAccess>(),
                {EncoderStatus::Codes::kSharedImageResolveFailed,
                 "SharedImage sync token wait was cancelled"});
          },
          std::move(resolve_cancelled_cb)));
      gpu_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&ResolveSharedImageOnGpuThread, command_buffer_helper_,
                         pending->frame, std::move(resolve_success_cb),
                         std::move(resolve_guard)));
      return;
    }

    auto encode = std::move(pending_encode_queue_.front());
    pending_encode_queue_.pop_front();
    auto pixel_buffer = WrapVideoFrameInCVPixelBuffer(encode->frame);
    if (!pixel_buffer) {
      FailPendingEncodes({EncoderStatus::Codes::kEncoderFailedEncode,
                          "WrapVideoFrameInCVPixelBuffer failed"});
      return;
    }
    // EncodeWithPixelBuffer() may synchronously notify the client of an error,
    // and the client may respond by calling Destroy() and deleting this object.
    auto weak_this = encoder_weak_ptr_;
    if (!EncodeWithPixelBuffer(std::move(encode->frame), encode->options,
                               std::move(pixel_buffer),
                               /*si_access=*/nullptr)) {
      if (!weak_this) {
        return;
      }
      pending_encode_queue_.clear();
      auto flush_cb = std::move(pending_flush_cb_);
      if (flush_cb) {
        std::move(flush_cb).Run(/*success=*/false);
      }
      return;
    }
  }

  MaybeFinishFlush();
}

void VTVideoEncodeAccelerator::FailPendingEncodes(EncoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!status.is_ok());

  pending_encode_queue_.clear();
  flush_complete_frames_issued_ = false;
  auto flush_cb = std::move(pending_flush_cb_);
  // The client may destroy this object from within the flush callback.
  auto weak_this = encoder_weak_ptr_;
  if (flush_cb) {
    std::move(flush_cb).Run(/*success=*/false);
    if (!weak_this) {
      return;
    }
  }
  NotifyErrorStatus(std::move(status));
}

void VTVideoEncodeAccelerator::OnSharedImageResolved(
    base::apple::ScopedCFTypeRef<CVPixelBufferRef> pixel_buffer,
    scoped_refptr<SharedImageEncodeAccess> si_access,
    EncoderStatus resolve_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (pending_encode_queue_.empty()) {
    return;
  }

  auto encode = std::move(pending_encode_queue_.front());
  pending_encode_queue_.pop_front();
  CHECK(encode->resolve_requested);

  if (!resolve_status.is_ok()) {
    FailPendingEncodes(std::move(resolve_status));
    return;
  }

  // EncodeWithPixelBuffer() may synchronously notify the client of an error,
  // and the client may respond by calling Destroy() and deleting this object.
  auto weak_this = encoder_weak_ptr_;
  if (!EncodeWithPixelBuffer(std::move(encode->frame), encode->options,
                             std::move(pixel_buffer), std::move(si_access))) {
    if (!weak_this) {
      return;
    }
    pending_encode_queue_.clear();
    auto flush_cb = std::move(pending_flush_cb_);
    if (flush_cb) {
      std::move(flush_cb).Run(/*success=*/false);
    }
    return;
  }
  ProcessPendingEncodes();
}

bool VTVideoEncodeAccelerator::EncodeWithPixelBuffer(
    scoped_refptr<VideoFrame> frame,
    const VideoEncoder::EncodeOptions& options,
    base::apple::ScopedCFTypeRef<CVPixelBufferRef> pixel_buffer,
    scoped_refptr<SharedImageEncodeAccess> si_access) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pixel_buffer);
  if (!compression_session_ || !frame) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderFailedEncode,
                       "Missing compression session or frame"});
    return false;
  }

  bool force_keyframe_after_reset = false;
  if (can_set_encoder_color_space_) {
    // WrapVideoFrameInCVPixelBuffer() / CreatePixelBufferFromSharedImage() will
    // do a few different things depending on the input buffer type:
    //   * If it's an IOSurface, the underlying attached color space will
    //     passthrough to the pixel buffer.
    //   * If we're uploading to a new pixel buffer and the provided frame color
    //     space is valid that'll be set on the pixel buffer.
    //   * If the frame color space is not valid, BT709 will be assumed.
    auto frame_cs = GetImageBufferColorSpace(pixel_buffer.get());
    std::optional<gfx::HDRMetadata> frame_hdr_metadata;
    if (frame->hdr_metadata().IsValid()) {
      frame_hdr_metadata = frame->hdr_metadata();
    }
    const bool first_hbd_full_range =
        !encoder_color_space_ && profile_ == HEVCPROFILE_MAIN10 &&
        frame_cs.GetRangeID() == gfx::ColorSpace::RangeID::FULL;
    const bool color_space_or_hdr_metadata_changed =
        encoder_color_space_ && (frame_cs != encoder_color_space_ ||
                                 frame_hdr_metadata != encoder_hdr_metadata_);
    if (first_hbd_full_range || color_space_or_hdr_metadata_changed) {
      if (pending_encodes_) {
        auto status = VTCompressionSessionCompleteFrames(
            compression_session_.get(), kCMTimeInvalid);
        if (status != noErr) {
          NotifyErrorStatus(
              {EncoderStatus::Codes::kEncoderFailedFlush,
               "flush failed: " + logging::DescriptionFromOSStatus(status)});
          return false;
        }
      }
      if (!ResetCompressionSession(frame_cs.GetRangeID())) {
        // ResetCompressionSession() invokes NotifyErrorStatus() on failure.
        return false;
      }
      encoder_color_space_.reset();
      encoder_hdr_metadata_.reset();
      force_keyframe_after_reset = true;
    }

    if (!encoder_color_space_) {
      encoder_color_space_ = frame_cs;
      encoder_hdr_metadata_ = frame_hdr_metadata;
      SetEncoderColorSpace();
    }
  }

  NSMutableDictionary* frame_props = [NSMutableDictionary dictionary];
  frame_props[CFToNSPtrCast(kVTEncodeFrameOptionKey_ForceKeyFrame)] =
      (options.key_frame || force_keyframe_after_reset) ? @YES : @NO;

  std::optional<int> frame_qp;
  if (IsManualQpSupported(profile_) &&
      bitrate_.mode() == Bitrate::Mode::kExternal &&
      options.quantizer.has_value()) {
    DCHECK(require_low_delay_);
    frame_qp = std::clamp(options.quantizer.value(), 1, kH26xMaxQp);
    frame_props[CFToNSPtrCast(kVTEncodeFrameOptionKey_BaseFrameQP)] =
        @(frame_qp.value());
  }

  // VideoToolbox uses timestamps for rate control purposes, but we can't rely
  // on real frame timestamps to be consistent with configured frame rate.
  // That's why we map real frame timestamps to generate ones that a
  // monotonically increase according to the configured frame rate.
  // Outputs will still be assigned real timestamps from frame objects.
  auto generate_timestamp = AssignMonotonicTimestamp();
  auto timestamp_cm =
      CMTimeMake(generate_timestamp.InMicroseconds(), USEC_PER_SEC);
  auto duration_cm = CMTimeMake(
      (base::Seconds(1) / frame_rate_).InMicroseconds(), USEC_PER_SEC);

  // Wrap information we'll need after the frame is encoded in a heap object.
  // We'll get the pointer back from the VideoToolbox completion callback.
  // |si_access| keeps the SharedImage overlay read lock alive until then.
  auto request = std::make_unique<InProgressFrameEncode>(
      std::move(frame), encoder_color_space_.value_or(gfx::ColorSpace()),
      frame_qp, std::move(si_access));

  // Pass the ownership of `request` to the encode callback, then release the
  // smart pointer.
  //
  // NOTE: When encoding fails, VT still holds the `sourceFrameRefcon`, and
  // either the `CompressionCallback` or VT itself may still use this resource
  // afterwards. Therefore, we always release the smart pointer here.
  OSStatus status = VTCompressionSessionEncodeFrame(
      compression_session_.get(), pixel_buffer.get(), timestamp_cm, duration_cm,
      NSToCFPtrCast(frame_props), reinterpret_cast<void*>(request.release()),
      nullptr);
  ++pending_encodes_;
  if (status == kVTVideoEncoderNotAvailableNowErr ||
      status == kVTCouldNotCreateInstanceErr) {
    NotifyErrorStatus({EncoderStatus::Codes::kOutOfPlatformEncoders,
                       "No more encoders available. " +
                           logging::DescriptionFromOSStatus(status)});
    return false;
  }
  if (status != noErr) {
    NotifyErrorStatus({EncoderStatus::Codes::kSystemAPICallError,
                       "VTCompressionSessionEncodeFrame failed: " +
                           logging::DescriptionFromOSStatus(status)});
    return false;
  }
  return true;
}

void VTVideoEncodeAccelerator::UseOutputBitstreamBuffer(
    BitstreamBuffer buffer) {
  DVLOG(3) << __func__ << ": buffer size=" << buffer.size();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (buffer.size() < bitstream_buffer_size_) {
    NotifyErrorStatus({EncoderStatus::Codes::kInvalidOutputBuffer,
                       "Output BitstreamBuffer isn't big enough: " +
                           base::NumberToString(buffer.size()) + " vs. " +
                           base::NumberToString(bitstream_buffer_size_)});
    return;
  }

  auto mapping = buffer.TakeRegion().Map();
  if (!mapping.IsValid()) {
    NotifyErrorStatus({EncoderStatus::Codes::kSystemAPICallError,
                       "Failed mapping shared memory"});
    return;
  }

  auto buffer_ref = std::make_unique<BitstreamBufferRef>(
      buffer.id(), std::move(mapping), buffer.size());

  // If there is already EncodeOutput waiting, copy its output first.
  if (!encoder_output_queue_.empty()) {
    auto encode_output = std::move(encoder_output_queue_.front());
    encoder_output_queue_.pop_front();
    ReturnBitstreamBuffer(std::move(encode_output), std::move(buffer_ref));
    return;
  }

  bitstream_buffer_queue_.push_back(std::move(buffer_ref));
}

void VTVideoEncodeAccelerator::RequestEncodingParametersChange(
    const Bitrate& bitrate,
    uint32_t framerate,
    const std::optional<gfx::Size>& size) {
  std::ostringstream parameters_description;
  parameters_description << ": bitrate=" << bitrate.ToString()
                         << ": framerate=" << framerate;
  if (size.has_value()) {
    parameters_description << ": frame size=" << size->width() << "x"
                           << size->height();
  }
  DVLOG(3) << __func__ << parameters_description.str();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (size.has_value()) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderUnsupportedConfig,
                       "Update output frame size is not supported"});
    return;
  }

  if (!compression_session_) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kEncoderIllegalState, "No compression session"});
    return;
  }

  frame_rate_ = framerate;
  video_toolbox::SessionPropertySetter session_property_setter(
      compression_session_);
  if (!session_property_setter.Set(kVTCompressionPropertyKey_ExpectedFrameRate,
                                   frame_rate_)) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kSystemAPICallError, "Can't change frame rate"});
    return;
  }
  if (bitrate.mode() != bitrate_.mode()) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderUnsupportedConfig,
                       "Can't change bitrate mode after Initialize()"});
    return;
  }
  if (bitrate.mode() != Bitrate::Mode::kExternal &&
      !session_property_setter.Set(
          kVTCompressionPropertyKey_AverageBitRate,
          static_cast<int32_t>(bitrate.target_bps()))) {
    NotifyErrorStatus({EncoderStatus::Codes::kSystemAPICallError,
                       "Can't change average bitrate"});
    return;
  }
  // Here in case of VBR we'd like to set more relaxed bitrate constraints.
  // It looks like setting VTCompressionPropertyKey_DataRateLimits should be
  // appropriate her, but it is NOT compatible with
  // EnableLowLatencyRateControl even though this fact is not documented.
  // Even in non low latency mode VTCompressionPropertyKey_DataRateLimits tends
  // to make the encoder undershoot set bitrate.
  bitrate_ = bitrate;
}

void VTVideoEncodeAccelerator::Destroy() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_encode_queue_.clear();
  delete this;
}

void VTVideoEncodeAccelerator::Flush(FlushCallback flush_callback) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(flush_callback);

  if (!compression_session_) {
    std::move(flush_callback).Run(/*success=*/false);
    return;
  }

  pending_flush_cb_ = std::move(flush_callback);
  flush_complete_frames_issued_ = false;
  ProcessPendingEncodes();
}

void VTVideoEncodeAccelerator::MaybeFinishFlush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pending_flush_cb_ || !pending_encode_queue_.empty()) {
    return;
  }

  if (!flush_complete_frames_issued_) {
    // Even though this will block until all frames are returned, the frames
    // will be posted to the current task runner, so we can't run the flush
    // callback at this time.
    OSStatus status = VTCompressionSessionCompleteFrames(
        compression_session_.get(), kCMTimeInvalid);
    if (status != noErr) {
      OSSTATUS_DLOG(ERROR, status)
          << " VTCompressionSessionCompleteFrames failed: ";
      std::move(pending_flush_cb_).Run(/*success=*/false);
      return;
    }
    flush_complete_frames_issued_ = true;
  }

  MaybeRunFlushCallback();
}

bool VTVideoEncodeAccelerator::IsFlushSupported() {
  return true;
}

bool VTVideoEncodeAccelerator::IsGpuFrameResizeSupported() {
  return base::FeatureList::IsEnabled(
      kVTVideoEncodeAcceleratorOpaqueSharedImageEncode);
}

// static
void VTVideoEncodeAccelerator::CompressionCallback(void* encoder_opaque,
                                                   void* request_opaque,
                                                   OSStatus status,
                                                   VTEncodeInfoFlags info,
                                                   CMSampleBufferRef sbuf) {
  // This function may be called asynchronously, on a different thread from the
  // one that calls VTCompressionSessionEncodeFrame.
  DVLOG(3) << __func__;

  auto* encoder = reinterpret_cast<VTVideoEncodeAccelerator*>(encoder_opaque);
  DCHECK(encoder);

  // InProgressFrameEncode holds timestamp information of the encoded frame.
  std::unique_ptr<InProgressFrameEncode> frame_info(
      reinterpret_cast<InProgressFrameEncode*>(request_opaque));

  // EncodeOutput holds onto CMSampleBufferRef when posting task between
  // threads.
  auto encode_output = std::make_unique<EncodeOutput>(info, sbuf, *frame_info);

  // This method is NOT called on |task_runner_|, so we still need to
  // post a task back to it to do work.
  encoder->task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VTVideoEncodeAccelerator::CompressionCallbackTask,
                     encoder->encoder_weak_ptr_, status,
                     std::move(encode_output)));
}

void VTVideoEncodeAccelerator::CompressionCallbackTask(
    OSStatus status,
    std::unique_ptr<EncodeOutput> encode_output) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  --pending_encodes_;
  DCHECK_GE(pending_encodes_, 0);

  if (status == kVTVideoEncoderNotAvailableNowErr ||
      status == kVTCouldNotCreateInstanceErr) {
    NotifyErrorStatus({EncoderStatus::Codes::kOutOfPlatformEncoders,
                       "No more encoders available. " +
                           logging::DescriptionFromOSStatus(status)});
    return;
  } else if (status != noErr) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kSystemAPICallError,
         "Encode failed: " + logging::DescriptionFromOSStatus(status)});
    return;
  }

  // If there isn't any BitstreamBuffer to copy into, add it to a queue for
  // later use.
  if (bitstream_buffer_queue_.empty()) {
    encoder_output_queue_.push_back(std::move(encode_output));
    return;
  }

  auto buffer_ref = std::move(bitstream_buffer_queue_.front());
  bitstream_buffer_queue_.pop_front();
  ReturnBitstreamBuffer(std::move(encode_output), std::move(buffer_ref));
}

void VTVideoEncodeAccelerator::ReturnBitstreamBuffer(
    std::unique_ptr<EncodeOutput> encode_output,
    std::unique_ptr<VTVideoEncodeAccelerator::BitstreamBufferRef> buffer_ref) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (encode_output->info & kVTEncodeInfo_FrameDropped) {
    DVLOG(2) << " frame dropped";
    client_->BitstreamBufferReady(buffer_ref->id,
                                  BitstreamBufferMetadata::CreateForDropFrame(
                                      encode_output->capture_timestamp));
    MaybeRunFlushCallback();
    return;
  }

  NSArray* sample_attachments_array =
      CFToNSPtrCast(CMSampleBufferGetSampleAttachmentsArray(
          encode_output->sample_buffer.get(), true));
  NSDictionary* sample_attachments =
      [sample_attachments_array count] > 0
          ? [sample_attachments_array objectAtIndex:0]
          : nil;
  NSNumber* not_sync = [sample_attachments
      objectForKey:CFToNSPtrCast(kCMSampleAttachmentKey_NotSync)];
  const bool keyframe = !not_sync || ![not_sync boolValue];

  NSNumber* depended = [sample_attachments
      objectForKey:CFToNSPtrCast(kCMSampleAttachmentKey_IsDependedOnByOthers)];
  const bool belongs_to_base_layer = !depended || [depended boolValue];

  std::vector<uint8_t> hdr_metadata_sei_nalu;
  if (keyframe && encode_output->encoded_color_space.GetTransferID() ==
                      gfx::ColorSpace::TransferID::PQ) {
    hdr_metadata_sei_nalu =
        BuildHdrMetadataSeiNalu(codec_, encode_output->sample_buffer.get());
  }

  size_t used_buffer_size = 0;
  const bool copy_rv = video_toolbox::CopySampleBufferToAnnexBBuffer(
      codec_, encode_output->sample_buffer.get(), keyframe,
      hdr_metadata_sei_nalu, buffer_ref->size,
      static_cast<char*>(buffer_ref->mapping.memory()), &used_buffer_size);
  if (!copy_rv) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kBitstreamConversionError,
         "Cannot copy output from SampleBuffer to AnnexBBuffer."});
    return;
  }

  BitstreamBufferMetadata md(used_buffer_size, keyframe,
                             encode_output->capture_timestamp);

  switch (codec_) {
    case VideoCodec::kH264:
      md.h264.emplace().temporal_idx = belongs_to_base_layer ? 0 : 1;
      break;
    case VideoCodec::kHEVC: {
      SVCGenericMetadata& svc = md.svc_generic.emplace();
      svc.temporal_idx = belongs_to_base_layer ? 0 : 1;
      svc.spatial_idx = 0;
      // We get the temporal id based on `IsDependedOnByOthers` property,
      // so we are not able to provide the reference flags and refresh
      // flags for HEVC, if the |follow_svc_spec| flag is false, RTC
      // will not send dependency descriptor RTP extension.
      svc.follow_svc_spec = encoder_produces_svc_spec_compliant_bitstream_;
      break;
    }
    default:
      NOTREACHED();
  }

  md.encoded_color_space = encode_output->encoded_color_space;
  if (encode_output->qp.has_value()) {
    md.qp = encode_output->qp.value();
  }

  if (calculate_psnr_) {
    if (@available(macOS 14.4, *)) {
      NSDictionary* quality_metrics = [sample_attachments
          objectForKey:CFToNSPtrCast(kVTSampleAttachmentKey_QualityMetrics)];
      if (quality_metrics) {
        NSNumber* luma_mse = [quality_metrics
            objectForKey:
                CFToNSPtrCast(
                    kVTSampleAttachmentQualityMetricsKey_LumaMeanSquaredError)];
        NSNumber* chroma_blue_mse = [quality_metrics
            objectForKey:
                CFToNSPtrCast(
                    kVTSampleAttachmentQualityMetricsKey_ChromaBlueMeanSquaredError)];
        NSNumber* chroma_red_mse = [quality_metrics
            objectForKey:
                CFToNSPtrCast(
                    kVTSampleAttachmentQualityMetricsKey_ChromaRedMeanSquaredError)];
        if (luma_mse && chroma_blue_mse && chroma_red_mse) {
          // YUV isn't the same as YCbCr, but we don't have a good way to report
          // the latter and in practice the difference will be small (luma vs
          // chroma is still a valid comparison).
          double y_mse = [luma_mse doubleValue];
          double cb_mse = [chroma_blue_mse doubleValue];
          double cr_mse = [chroma_red_mse doubleValue];

          md.yuv_psnr = YuvPsnr{
              .y = CalculatePsnr(y_mse, input_format_),
              .u = CalculatePsnr(cb_mse, input_format_),  // Cb -> U
              .v = CalculatePsnr(cr_mse, input_format_),  // Cr -> V
          };
        }
      }
    }
  }

  client_->BitstreamBufferReady(buffer_ref->id, std::move(md));
  MaybeRunFlushCallback();
}

bool VTVideoEncodeAccelerator::ResetCompressionSession(
    gfx::ColorSpace::RangeID source_range) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  compression_session_.reset();

  auto source_attrs = CreateSourceImageBufferAttributes(
      profile_, input_visible_size_, source_range);
  if (auto created = CreateCompressionSession(
          profile_, input_visible_size_, required_encoder_type_,
          require_low_delay_, &VTVideoEncodeAccelerator::CompressionCallback,
          this, source_attrs.get());
      created.has_value()) {
    compression_session_ = std::move(created.value());
  } else if (created.error() == kVTVideoEncoderNotAvailableNowErr ||
             created.error() == kVTCouldNotCreateInstanceErr) {
    NotifyErrorStatus({EncoderStatus::Codes::kOutOfPlatformEncoders,
                       "VTCompressionSessionCreate failed", "system_error",
                       logging::DescriptionFromOSStatus(created.error())});
    return false;
  } else {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderInitializationError,
                       "VTCompressionSessionCreate failed", "system_error",
                       logging::DescriptionFromOSStatus(created.error())});
    return false;
  }

  if (!ConfigureCompressionSession(codec_)) {
    return false;
  }

  RequestEncodingParametersChange(bitrate_, frame_rate_, std::nullopt);
  return true;
}

bool VTVideoEncodeAccelerator::ConfigureCompressionSession(VideoCodec codec) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(compression_session_);

  video_toolbox::SessionPropertySetter session_property_setter(
      compression_session_);
  if (!session_property_setter.Set(kVTCompressionPropertyKey_ProfileLevel,
                                   VideoCodecProfileToVTProfile(profile_))) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderUnsupportedProfile,
                       "Unsupported profile: " + GetProfileName(profile_)});
    return false;
  }
  if (!session_property_setter.Set(kVTCompressionPropertyKey_RealTime,
                                   require_low_delay_)) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kEncoderUnsupportedConfig,
         "The video encoder doesn't support compression in real time"});
    return false;
  }
  if (!session_property_setter.Set(
          kVTCompressionPropertyKey_AllowFrameReordering, false)) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kEncoderUnsupportedConfig,
         "The video encoder doesn't support non frame reordering compression"});
    return false;
  }
  if (base::FeatureList::IsEnabled(
          kVTVideoEncodeAcceleratorOpaqueSharedImageEncode)) {
    if (session_property_setter.IsSupported(
            kVTCompressionPropertyKey_PixelTransferProperties)) {
      // Keep the crop/scale geometry aligned with VideoFrameConverter:
      // VideoFrameConverter scales the visible rect rather than the entire
      // coded buffer. VideoFrame::visible_rect() is propagated as the source
      // CVPixelBuffer's clean aperture. VT's default scaling mode stretches the
      // full source buffer, so explicitly crop to that aperture before scaling.
      NSDictionary* pixel_transfer_properties = @{
        CFToNSPtrCast(kVTPixelTransferPropertyKey_ScalingMode) :
            CFToNSPtrCast(kVTScalingMode_CropSourceToCleanAperture)
      };
      if (!session_property_setter.Set(
              kVTCompressionPropertyKey_PixelTransferProperties,
              NSToCFPtrCast(pixel_transfer_properties))) {
        NotifyErrorStatus({EncoderStatus::Codes::kEncoderUnsupportedConfig,
                           "The video encoder doesn't support cropping to the "
                           "clean aperture"});
        return false;
      }
    } else {
      DLOG(WARNING) << "ScalingMode property is not supported";
    }
  }
  // Limit keyframe output to 4 minutes, see https://crbug.com/658429.
  if (!session_property_setter.Set(
          kVTCompressionPropertyKey_MaxKeyFrameInterval, 7200)) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderUnsupportedConfig,
                       "Failed to set max keyframe interval to 7200 frames"});
    return false;
  }
  // This property may suddenly become unsupported when a second compression
  // session is created if the codec is H.265 and CPU arch is x64. Skip setting
  // this property for H.265.
  if (codec != VideoCodec::kHEVC) {
    if (session_property_setter.IsSupported(
            kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration)) {
      if (!session_property_setter.Set(
              kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration, 240)) {
        NotifyErrorStatus(
            {EncoderStatus::Codes::kEncoderUnsupportedConfig,
             "Failed to set max keyframe interval duration to 240 seconds"});
        return false;
      }
    } else {
      DLOG(WARNING) << "MaxKeyFrameIntervalDuration is not supported";
    }
  }

  if (session_property_setter.IsSupported(
          kVTCompressionPropertyKey_MaxFrameDelayCount)) {
    // macOS 15.0 will reject encode if we set max frame delay count to 3,
    // don't fail the whole encode session if this property can not be set
    // properly.
    if (!session_property_setter.Set(
            kVTCompressionPropertyKey_MaxFrameDelayCount,
            static_cast<int>(kNumInputBuffers))) {
      DLOG(ERROR) << "Failed to set max frame delay count to "
                  << base::NumberToString(kNumInputBuffers);
    }
  } else {
    DLOG(WARNING) << "MaxFrameDelayCount is not supported";
  }

  if (@available(macOS 14.4, *)) {
    if (session_property_setter.IsSupported(
            kVTCompressionPropertyKey_CalculateMeanSquaredError) &&
        base::FeatureList::IsEnabled(kVTVideoEncodeAcceleratorCalculatePSNR)) {
      if (session_property_setter.Set(
              kVTCompressionPropertyKey_CalculateMeanSquaredError, true)) {
        calculate_psnr_ = true;
      } else {
        DLOG(WARNING) << "Failed to set CalculateMeanSquaredError property";
      }
    } else {
      DVLOG(1) << "CalculateMeanSquaredError is not supported or not enabled";
    }
  }

  if (num_temporal_layers_ != 2) {
    return true;
  }

  if (!IsHardwareEncoder(compression_session_.get()) ||
      !IsSVCSupported(profile_)) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderUnsupportedConfig,
                       "SVC encoding is not supported on this OS version or "
                       "hardware, or SW encoding was selected"});
    return false;
  }

  if (!session_property_setter.IsSupported(
          kVTCompressionPropertyKey_BaseLayerFrameRateFraction)) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderUnsupportedConfig,
                       "BaseLayerFrameRateFraction is not supported"});
    return false;
  }
  if (!session_property_setter.Set(
          kVTCompressionPropertyKey_BaseLayerFrameRateFraction, 0.5)) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderUnsupportedConfig,
                       "Setting BaseLayerFrameRate property failed"});
    return false;
  }

  // Configuring the number of reference frames to 1, which will produce
  // bitstream that follows WebRTC SVC spec for L1T2.
  bool skip_set_reference_buffer_count = false;
  if (@available(macOS 26, *)) {
    // We see that setting kVTCompressionPropertyKey_ReferenceBufferCount=1
    // causes frame drops on Mac OS Tahoe when encoding H264,
    // that's why we skip it. More info: http://crbug.com/450596068
    // Using an extra flag here because @available checks can't be combined
    // with other conditions in the same if statement,
    // see the `unsupported-availability-guard` warning.
    skip_set_reference_buffer_count = (codec == VideoCodec::kH264);
  }
  if (!skip_set_reference_buffer_count &&
      session_property_setter.IsSupported(
          kVTCompressionPropertyKey_ReferenceBufferCount)) {
    if (!session_property_setter.Set(
            kVTCompressionPropertyKey_ReferenceBufferCount, 1)) {
      DLOG(WARNING) << "Setting ReferenceBufferCount property failed";
    } else {
      encoder_produces_svc_spec_compliant_bitstream_ = true;
    }
  } else {
    DLOG(WARNING) << "ReferenceBufferCount is not supported";
  }

  return true;
}

void VTVideoEncodeAccelerator::MaybeRunFlushCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!pending_flush_cb_) {
    return;
  }

  if (pending_encodes_ || !encoder_output_queue_.empty() ||
      !pending_encode_queue_.empty()) {
    return;
  }

  flush_complete_frames_issued_ = false;
  std::move(pending_flush_cb_).Run(/*success=*/true);
}

void VTVideoEncodeAccelerator::SetEncoderColorSpace() {
  if (!encoder_color_space_ || !encoder_color_space_->IsValid()) {
    return;
  }

  CFStringRef primary, transfer, matrix;
  if (!GetImageBufferColorValues(*encoder_color_space_, &primary, &transfer,
                                 &matrix)) {
    DLOG(ERROR) << "Failed to set bitstream color space: "
                << encoder_color_space_->ToString();
    return;
  }

  video_toolbox::SessionPropertySetter session_property_setter(
      compression_session_);
  if (!session_property_setter.IsSupported(
          kVTCompressionPropertyKey_ColorPrimaries) ||
      !session_property_setter.IsSupported(
          kVTCompressionPropertyKey_TransferFunction) ||
      !session_property_setter.IsSupported(
          kVTCompressionPropertyKey_YCbCrMatrix)) {
    DLOG(ERROR) << "VTCompressionSession doesn't support color space settings.";
    can_set_encoder_color_space_ = false;
    return;
  }

  if (!session_property_setter.Set(kVTCompressionPropertyKey_ColorPrimaries,
                                   primary) ||
      !session_property_setter.Set(kVTCompressionPropertyKey_TransferFunction,
                                   transfer) ||
      !session_property_setter.Set(kVTCompressionPropertyKey_YCbCrMatrix,
                                   matrix)) {
    DLOG(ERROR) << "Failed to set color space on VTCompressionSession.";
    can_set_encoder_color_space_ = false;
    return;
  }

  DVLOG(1) << "Set encoder color space to: "
           << encoder_color_space_->ToString();

  // HDR10 is a PQ format. HLG signals its transfer function through the VUI and
  // must not get PQ-style mastering metadata.
  if (encoder_color_space_->GetTransferID() ==
      gfx::ColorSpace::TransferID::PQ) {
    ConfigureVtSessionForHdrMetadata(session_property_setter,
                                     encoder_hdr_metadata_);
  }
}

void VTVideoEncodeAccelerator::NotifyErrorStatus(EncoderStatus status) {
  CHECK(!status.is_ok());
  if (media_log_) {
    media_log_->NotifyError(status);
  }
  // NotifyErrorStatus() can be called without calling Initialize() in the case
  // of GetSupportedProfiles().
  if (!client_) {
    return;
  }
  client_->NotifyErrorStatus(std::move(status));
}

void VTVideoEncodeAccelerator::SetCommandBufferHelperCB(
    base::RepeatingCallback<scoped_refptr<CommandBufferHelper>()>
        get_command_buffer_helper_cb,
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::FeatureList::IsEnabled(
          kVTVideoEncodeAcceleratorOpaqueSharedImageEncode)) {
    return;
  }
  gpu_task_runner_ = std::move(gpu_task_runner);
  auto [reply, cancelled_reply] = base::SplitOnceCallback(
      base::BindOnce(&VTVideoEncodeAccelerator::OnCommandBufferHelperAvailable,
                     encoder_weak_ptr_));
  base::ScopedClosureRunner reply_guard(
      base::BindOnce(std::move(cancelled_reply), nullptr));
  gpu_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, std::move(get_command_buffer_helper_cb),
      base::BindOnce(
          [](CommandBufferHelperResolvedCB reply,
             base::ScopedClosureRunner reply_guard,
             scoped_refptr<CommandBufferHelper> command_buffer_helper) {
            reply_guard.ReplaceClosure(base::OnceClosure());
            std::move(reply).Run(std::move(command_buffer_helper));
          },
          std::move(reply), std::move(reply_guard)));
}

void VTVideoEncodeAccelerator::OnCommandBufferHelperAvailable(
    scoped_refptr<CommandBufferHelper> command_buffer_helper) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  command_buffer_helper_ = std::move(command_buffer_helper);
  if (!command_buffer_helper_) {
    command_buffer_helper_failed_ = true;
    if (!pending_encode_queue_.empty()) {
      FailPendingEncodes(
          {EncoderStatus::Codes::kGPUCommandBufferNotAvailable,
           "CommandBufferHelper unavailable for opaque SharedImage encode"});
    }
    return;
  }
  ProcessPendingEncodes();
}

bool VTVideoEncodeAccelerator::CanEncodeOpaqueSharedImage(
    const VideoFrame& frame) const {
  DCHECK(frame.HasSharedImage());
  DCHECK(!frame.HasMappableSharedImage());
  if (!base::FeatureList::IsEnabled(
          kVTVideoEncodeAcceleratorOpaqueSharedImageEncode)) {
    return false;
  }
  // Opaque SharedImage encode is wired for NV12 and P010.
  if (frame.format() != input_format_ ||
      (input_format_ != PIXEL_FORMAT_NV12 &&
       input_format_ != PIXEL_FORMAT_P010LE)) {
    return false;
  }
  return frame.shared_image()->usage().Has(
      gpu::SHARED_IMAGE_USAGE_MACOS_VIDEO_TOOLBOX);
}

base::TimeDelta VTVideoEncodeAccelerator::AssignMonotonicTimestamp() {
  const base::TimeDelta step = base::Seconds(1) / frame_rate_;
  auto result = next_timestamp_;
  next_timestamp_ += step;
  return result;
}

// static
double VTVideoEncodeAccelerator::CalculatePsnr(double mse,
                                               VideoPixelFormat format) {
  DCHECK_GE(mse, 0.0);
  DCHECK(format == PIXEL_FORMAT_I420 || format == PIXEL_FORMAT_NV12 ||
         format == PIXEL_FORMAT_P010LE);
  const double max_value = (1 << BitDepth(format)) - 1;
  if (mse == 0.0) {
    return 128.0;
  }
  double psnr = 10.0 * std::log10((max_value * max_value) / mse);
  return std::min(psnr, 128.0);
}

// static
double VTVideoEncodeAccelerator::CalculatePsnrForTesting(
    double mse,
    VideoPixelFormat format) {
  return CalculatePsnr(mse, format);
}

}  // namespace media
