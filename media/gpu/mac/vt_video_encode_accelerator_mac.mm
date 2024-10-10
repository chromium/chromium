// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/vt_video_encode_accelerator_mac.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/osstatus_logging.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "media/base/bitrate.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/mac/color_space_util_mac.h"
#include "media/base/mac/video_frame_mac.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/video/video_encode_accelerator.h"

using base::apple::CFToNSPtrCast;
using base::apple::NSToCFPtrCast;

// This is a min version of macOS where we want to support SVC encoding via
// EnableLowLatencyRateControl flag. The flag is actually supported since 11.3,
// but there we see frame drops even with ample bitrate budget. Excessive frame
// drops were fixed in 12.0.1.
#define LOW_LATENCY_AND_SVC_AVAILABLE_VER 12.0.1

namespace media {

using EncoderType = VideoEncodeAccelerator::Config::EncoderType;

namespace {

constexpr size_t kMaxFrameRateNumerator = 120;
constexpr size_t kMaxFrameRateDenominator = 1;
constexpr size_t kNumInputBuffers = 3;
constexpr gfx::Size kDefaultSupportedResolution = gfx::Size(640, 480);
// TODO(crbug.com/40876392): We should add a function like a
// `GetVideoEncodeAcceleratorProfileIsSupported`, to test the
// real support status with a give resolution, framerate etc,
// instead of query a "supportedProfile" list.
constexpr gfx::Size kMaxSupportedResolution = gfx::Size(4096, 2304);

#if !BUILDFLAG(IS_IOS)
// The ID of the encoder that may be selected when we enable low latency via
// `kVTVideoEncoderSpecification_EnableLowLatencyRateControl`. Low latency is
// in general only possible with a hardware encoder in VideoToolbox, so we
// assume this is in fact a hardware encoder. For some reason, neither
// `VTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder` nor
// `kVTVideoEncoderList_IsHardwareAccelerated` is set for this encoder.
constexpr std::string_view kRealtimeHardwareH264EncoderID =
    "com.apple.videotoolbox.videoencoder.h264.rtvc";
#endif

constexpr VideoCodecProfile kSupportedProfiles[] = {
    H264PROFILE_BASELINE,
    H264PROFILE_MAIN,
    H264PROFILE_HIGH,
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    // macOS actually start supporting HEVC since macOS 10.13+, but we only
    // support decoding HEVC on macOS 11.0+ due to the failure of create a
    // decompression session on some device, so limit this to macOS 11.0 as
    // well.
    HEVCPROFILE_MAIN,
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
};

bool IsSVCSupported(const VideoCodec& codec) {
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER) && defined(ARCH_CPU_ARM_FAMILY)
  // macOS 14.0+ support SVC HEVC encoding for Apple Silicon chips only.
  if (codec == VideoCodec::kHEVC) {
    if (@available(macOS 14.0, iOS 17.0, *)) {
      return true;
    }
    return false;
  }
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER) &&
        // defined(ARCH_CPU_ARM_FAMILY)
  if (@available(macOS LOW_LATENCY_AND_SVC_AVAILABLE_VER, *)) {
    return codec == VideoCodec::kH264;
  }
  return false;
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
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return kVTProfileLevel_H264_Baseline_AutoLevel;
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
      NOTREACHED_IN_MIGRATION();
  }
  return kCMVideoCodecType_H264;
}

bool IsHardwareEncoder(VTSessionRef compression_session,
                       VideoCodecProfile profile) {
  // iOS and HEVC are always hardware-accelerated.
#if BUILDFLAG(IS_IOS)
  return true;
#else
  if (VideoCodecProfileToVideoCodec(profile) == VideoCodec::kHEVC) {
    return true;
  }

  base::apple::ScopedCFTypeRef<CFBooleanRef> using_hardware;
  if (VTSessionCopyProperty(
          compression_session,
          kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder,
          kCFAllocatorDefault, using_hardware.InitializeInto()) == noErr) {
    return CFBooleanGetValue(using_hardware.get());
  }
  DVLOG(1) << "Couldn't read the UsingHardwareAcceleratedVideoEncoder property";

  base::apple::ScopedCFTypeRef<CFStringRef> encoder_id;
  if (VTSessionCopyProperty(
          compression_session, kVTCompressionPropertyKey_EncoderID,
          kCFAllocatorDefault, encoder_id.InitializeInto()) == noErr &&
      base::SysCFStringRefToUTF8(encoder_id.get()) ==
          kRealtimeHardwareH264EncoderID) {
    DVLOG(1) << "But " << encoder_id.get() << " is a known hardware encoder";
    return true;
  }

  return false;
#endif  // BUILDFLAG(IS_IOS)
}

base::expected<video_toolbox::ScopedVTCompressionSessionRef, OSStatus>
CreateCompressionSession(VideoCodec codec,
                         const gfx::Size& input_size,
                         EncoderType required_encoder_type,
                         bool require_low_delay,
                         VTCompressionOutputCallback output_callback = nullptr,
                         VTVideoEncodeAccelerator* accelerator = nullptr) {
  CHECK_EQ(!output_callback, !accelerator);

  NSMutableDictionary* encoder_spec = [NSMutableDictionary dictionary];

  // iOS is always hardware-accelerated while on mac, encoder configuration
  // handling is necessary.
#if BUILDFLAG(IS_MAC)
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
#endif

  if (@available(macOS LOW_LATENCY_AND_SVC_AVAILABLE_VER, *)) {
    // Don't enable low-latency rate control in SW mode as it doesn't seem to
    // apply to the SW encoder. From
    // https://developer.apple.com/videos/play/wwdc2021/10158/, "[...] the
    // low-latency mode always uses a hardware-accelerated video encoder". In
    // fact, trying to use
    // `kVTVideoEncoderSpecification_EnableLowLatencyRateControl` with the SW
    // encoder leads to an initialization error.
    if (required_encoder_type != EncoderType::kSoftware && require_low_delay &&
        IsSVCSupported(codec)) {
      encoder_spec[CFToNSPtrCast(
          kVTVideoEncoderSpecification_EnableLowLatencyRateControl)] = @YES;
    }
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
      VideoCodecToCMVideoCodec(codec), NSToCFPtrCast(encoder_spec),
      /*sourceImageBufferAttributes=*/nullptr,
      /*compressedDataAllocator=*/nullptr, output_callback,
      reinterpret_cast<void*>(accelerator), session.InitializeInto());
  if (status != noErr) {
    // IMPORTANT: ScopedCFTypeRef::release() doesn't call CFRelease(). In case
    // of an error VTCompressionSessionCreate() is not supposed to write a
    // non-null value into compression_session_, but just in case, we'll clear
    // it without calling CFRelease() because it can be unsafe to call
    // VTCompressionSessionInvalidate() on a not fully created session.
    std::ignore = session.release();
    return base::unexpected(status);
  }
  DVLOG(3) << " VTCompressionSession created with input size="
           << input_size.ToString();
  return session;
}

VideoEncoderInfo GetVideoEncoderInfo(VTSessionRef compression_session,
                                     VideoCodecProfile profile) {
  VideoEncoderInfo info;
  info.implementation_name = "VideoToolbox";
  info.is_hardware_accelerated =
      IsHardwareEncoder(compression_session, profile);

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
    info.frame_delay = profile == H264PROFILE_BASELINE ? 0 : 13;
    info.input_capacity = info.frame_delay.value() + 4;
  }
  if (max_frame_delay_property.has_value()) {
    info.frame_delay =
        std::min(info.frame_delay.value(), max_frame_delay_property.value());
    info.input_capacity =
        std::min(info.input_capacity.value(), max_frame_delay_property.value());
  }

  return info;
}

}  // namespace

struct VTVideoEncodeAccelerator::InProgressFrameEncode {
  InProgressFrameEncode(scoped_refptr<VideoFrame> frame,
                        const gfx::ColorSpace& frame_cs)
      : frame(frame), encoded_color_space(frame_cs) {}
  const scoped_refptr<VideoFrame> frame;
  const gfx::ColorSpace encoded_color_space;
};

struct VTVideoEncodeAccelerator::EncodeOutput {
  EncodeOutput() = delete;

  EncodeOutput(VTEncodeInfoFlags info_flags,
               CMSampleBufferRef sbuf,
               const InProgressFrameEncode& frame_info)
      : info(info_flags),
        sample_buffer(sbuf, base::scoped_policy::RETAIN),
        capture_timestamp(frame_info.frame->timestamp()),
        encoded_color_space(frame_info.encoded_color_space) {}

  EncodeOutput(const EncodeOutput&) = delete;
  EncodeOutput& operator=(const EncodeOutput&) = delete;

  const VTEncodeInfoFlags info;
  const base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample_buffer;
  const base::TimeDelta capture_timestamp;
  const gfx::ColorSpace encoded_color_space;
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
  const base::WritableSharedMemoryMapping mapping;
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
VTVideoEncodeAccelerator::GetSupportedH264Profiles() {
  SupportedProfiles profiles;

  const bool can_create_hardware_session =
      CreateCompressionSession(VideoCodec::kH264, kDefaultSupportedResolution,
                               EncoderType::kHardware,
                               /*require_low_delay=*/false)
          .has_value();
  if (!can_create_hardware_session) {
    DVLOG(1) << "Hardware H.264 encode acceleration is not available on this "
                "platform.";
    return profiles;
  }

  SupportedProfile profile;
  profile.max_resolution = kMaxSupportedResolution;
  profile.max_framerate_numerator = kMaxFrameRateNumerator;
  profile.max_framerate_denominator = kMaxFrameRateDenominator;
  // Advertise VBR here, even though the peak bitrate is never actually used.
  // See RequestEncodingParametersChange() for more details.
  profile.rate_control_modes = VideoEncodeAccelerator::kConstantMode |
                               VideoEncodeAccelerator::kVariableMode;
  // L1T1 = no additional spatial and temporal layer = always supported.
  const std::vector<SVCScalabilityMode> always_supported_scalability_modes{
      SVCScalabilityMode::kL1T1};

  for (const auto& supported_profile : kSupportedProfiles) {
    if (VideoCodecProfileToVideoCodec(supported_profile) == VideoCodec::kH264) {
#if defined(ARCH_CPU_X86_FAMILY)
      for (const auto& min_resolution : {gfx::Size(640, 1), gfx::Size(1, 480)})
#else
      const auto min_resolution = gfx::Size();
#endif
      {
        profile.min_resolution = min_resolution;
        profile.is_software_codec = false;
        profile.profile = supported_profile;
        profile.scalability_modes = always_supported_scalability_modes;
        if (IsSVCSupported(VideoCodec::kH264)) {
          profile.scalability_modes.push_back(SVCScalabilityMode::kL1T2);
        }
        profiles.push_back(profile);

#if BUILDFLAG(IS_MAC)
        // iOS is always hardware-accelerated.
        // macOS doesn't provide a way to enumerate codec details, so just
        // assume software codec support is the same as hardware, but with
        // the lowest possible minimum resolution.
        profile.min_resolution = gfx::Size(2, 2);
        profile.scalability_modes = always_supported_scalability_modes;
        profile.is_software_codec = true;
        profiles.push_back(profile);
#endif  // BUILDFLAG(IS_MAC)
      }
    }
  }
  return profiles;
}

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
VideoEncodeAccelerator::SupportedProfiles
VTVideoEncodeAccelerator::GetSupportedHEVCProfiles() {
  SupportedProfiles profiles;
  if (!base::FeatureList::IsEnabled(kPlatformHEVCEncoderSupport)) {
    return profiles;
  }

  const bool can_create_hardware_session =
      CreateCompressionSession(VideoCodec::kHEVC, kDefaultSupportedResolution,
                               EncoderType::kHardware,
                               /*require_low_delay=*/false)
          .has_value();
  if (!can_create_hardware_session) {
    DVLOG(1) << "Hardware HEVC encode acceleration is not available on this "
                "platform.";
    return profiles;
  }

  SupportedProfile profile;
  profile.max_resolution = kMaxSupportedResolution;
  profile.max_framerate_numerator = kMaxFrameRateNumerator;
  profile.max_framerate_denominator = kMaxFrameRateDenominator;
  // Advertise VBR here, even though the peak bitrate is never actually used.
  // See RequestEncodingParametersChange() for more details.
  profile.rate_control_modes = VideoEncodeAccelerator::kConstantMode |
                               VideoEncodeAccelerator::kVariableMode;
  // L1T1 = no additional spatial and temporal layer = always supported.
  profile.scalability_modes.push_back(SVCScalabilityMode::kL1T1);
  if (IsSVCSupported(VideoCodec::kHEVC)) {
    profile.scalability_modes.push_back(SVCScalabilityMode::kL1T2);
  }

  for (const auto& supported_profile : kSupportedProfiles) {
    if (VideoCodecProfileToVideoCodec(supported_profile) == VideoCodec::kHEVC) {
      // macOS doesn't support HEVC software encoding on both Intel and Apple
      // Silicon Macs.
      profile.is_software_codec = false;
      profile.profile = supported_profile;
      profiles.push_back(profile);
    }
  }

  return profiles;
}
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

VideoEncodeAccelerator::SupportedProfiles
VTVideoEncodeAccelerator::GetSupportedProfiles() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SupportedProfiles profiles;
  for (const auto& supported_profile : GetSupportedH264Profiles())
    profiles.push_back(supported_profile);
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  for (const auto& supported_profile : GetSupportedHEVCProfiles())
    profiles.push_back(supported_profile);
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  return profiles;
}

bool VTVideoEncodeAccelerator::Initialize(const Config& config,
                                          Client* client,
                                          std::unique_ptr<MediaLog> media_log) {
  DVLOG(3) << __func__ << ": " << config.AsHumanReadableString();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client);

  // Clients are expected to call Flush() before reinitializing the encoder.
  DCHECK_EQ(pending_encodes_, 0);

  if (config.input_format != PIXEL_FORMAT_I420 &&
      config.input_format != PIXEL_FORMAT_NV12) {
    MEDIA_LOG(ERROR, media_log)
        << "Input format not supported= "
        << VideoPixelFormatToString(config.input_format);
    return false;
  }
  if (!base::Contains(kSupportedProfiles, config.output_profile)) {
    MEDIA_LOG(ERROR, media_log) << "Output profile not supported= "
                                << GetProfileName(config.output_profile);
    return false;
  }
  profile_ = config.output_profile;
  codec_ = VideoCodecProfileToVideoCodec(config.output_profile);
  client_ = client;
  input_visible_size_ = config.input_visible_size;
  frame_rate_ = config.framerate;
  bitrate_ = config.bitrate;
  bitstream_buffer_size_ = config.input_visible_size.GetArea();
  require_low_delay_ = config.require_low_delay;

  if (codec_ == VideoCodec::kH264) {
    required_encoder_type_ = config.required_encoder_type;
  } else if (config.required_encoder_type == EncoderType::kSoftware) {
    DLOG(ERROR) << "Software encoder selection is only allowed for H264.";
  }

  if (config.HasTemporalLayer())
    num_temporal_layers_ = config.spatial_layers.front().num_of_temporal_layers;

  if (num_temporal_layers_ > 2) {
    MEDIA_LOG(ERROR, media_log) << "Unsupported number of SVC temporal layers.";
    return false;
  }

  if (!ResetCompressionSession()) {
    MEDIA_LOG(ERROR, media_log) << "Failed creating compression session.";
    return false;
  }

  auto encoder_info = GetVideoEncoderInfo(compression_session_.get(), profile_);

  // Report whether hardware encode is being used.
  if (!encoder_info.is_hardware_accelerated) {
    MEDIA_LOG(INFO, media_log) << "VideoToolbox selected a software encoder.";
  }

  media_log_ = std::move(media_log);

  client_->NotifyEncoderInfoChange(encoder_info);
  client_->RequireBitstreamBuffers(kNumInputBuffers, input_visible_size_,
                                   bitstream_buffer_size_);
  return true;
}

void VTVideoEncodeAccelerator::Encode(scoped_refptr<VideoFrame> frame,
                                      bool force_keyframe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(compression_session_);
  DCHECK(frame);

  auto pixel_buffer = WrapVideoFrameInCVPixelBuffer(frame);
  if (!pixel_buffer) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderFailedEncode,
                       "WrapVideoFrameInCVPixelBuffer failed"});
    return;
  }

  if (can_set_encoder_color_space_) {
    // WrapVideoFrameInCVPixelBuffer() will do a few different things depending
    // on the input buffer type:
    //   * If it's an IOSurface, the underlying attached color space will
    //     passthrough to the pixel buffer.
    //   * If we're uploading to a new pixel buffer and the provided frame color
    //     space is valid that'll be set on the pixel buffer.
    //   * If the frame color space is not valid, BT709 will be assumed.
    auto frame_cs = GetImageBufferColorSpace(pixel_buffer.get());
    if (encoder_color_space_ && frame_cs != encoder_color_space_) {
      if (pending_encodes_) {
        auto status = VTCompressionSessionCompleteFrames(
            compression_session_.get(), kCMTimeInvalid);
        if (status != noErr) {
          NotifyErrorStatus(
              {EncoderStatus::Codes::kEncoderFailedFlush,
               "flush failed: " + logging::DescriptionFromOSStatus(status)});
          return;
        }
      }
      if (!ResetCompressionSession()) {
        // ResetCompressionSession() invokes NotifyErrorStatus() on failure.
        return;
      }
      encoder_color_space_.reset();
    }

    if (!encoder_color_space_) {
      encoder_color_space_ = frame_cs;
      SetEncoderColorSpace();
    }
  }

  NSDictionary* frame_props = @{
    CFToNSPtrCast(kVTEncodeFrameOptionKey_ForceKeyFrame) : force_keyframe ? @YES
                                                                          : @NO
  };

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
  auto request = std::make_unique<InProgressFrameEncode>(
      std::move(frame), encoder_color_space_.value_or(gfx::ColorSpace()));

  // We can pass the ownership of |request| to the encode callback if
  // successful. Otherwise let it fall out of scope.
  OSStatus status = VTCompressionSessionEncodeFrame(
      compression_session_.get(), pixel_buffer.get(), timestamp_cm, duration_cm,
      NSToCFPtrCast(frame_props), reinterpret_cast<void*>(request.get()),
      nullptr);
  if (status != noErr) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderFailedEncode,
                       "VTCompressionSessionEncodeFrame failed: " +
                           logging::DescriptionFromOSStatus(status)});
  } else {
    ++pending_encodes_;
    CHECK(request.release());
  }
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
  if (!session_property_setter.Set(
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

  // Even though this will block until all frames are returned, the frames will
  // be posted to the current task runner, so we can't run the flush callback
  // at this time.
  OSStatus status = VTCompressionSessionCompleteFrames(
      compression_session_.get(), kCMTimeInvalid);

  if (status != noErr) {
    OSSTATUS_DLOG(ERROR, status)
        << " VTCompressionSessionCompleteFrames failed: ";
    std::move(flush_callback).Run(/*success=*/false);
    return;
  }

  pending_flush_cb_ = std::move(flush_callback);
  MaybeRunFlushCallback();
}

bool VTVideoEncodeAccelerator::IsFlushSupported() {
  return true;
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

  if (status != noErr) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kEncoderFailedEncode,
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

  size_t used_buffer_size = 0;
  const bool copy_rv = video_toolbox::CopySampleBufferToAnnexBBuffer(
      codec_, encode_output->sample_buffer.get(), keyframe, buffer_ref->size,
      static_cast<char*>(buffer_ref->mapping.memory()), &used_buffer_size);
  if (!copy_rv) {
    DLOG(ERROR) << "Cannot copy output from SampleBuffer to AnnexBBuffer.";
    used_buffer_size = 0;
  }

  BitstreamBufferMetadata md(used_buffer_size, keyframe,
                             encode_output->capture_timestamp);

  switch (codec_) {
    case VideoCodec::kH264:
      md.h264.emplace().temporal_idx = belongs_to_base_layer ? 0 : 1;
      break;
    case VideoCodec::kHEVC:
      md.h265.emplace().temporal_idx = belongs_to_base_layer ? 0 : 1;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  md.encoded_color_space = encode_output->encoded_color_space;

  client_->BitstreamBufferReady(buffer_ref->id, std::move(md));
  MaybeRunFlushCallback();
}

bool VTVideoEncodeAccelerator::ResetCompressionSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  compression_session_.reset();

  if (auto created = CreateCompressionSession(
          codec_, input_visible_size_, required_encoder_type_,
          require_low_delay_, &VTVideoEncodeAccelerator::CompressionCallback,
          this);
      created.has_value()) {
    compression_session_ = std::move(created.value());
  } else {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderInitializationError,
                       "VTCompressionSessionCreate failed: " +
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
  // Limit keyframe output to 4 minutes, see https://crbug.com/658429.
  if (!session_property_setter.Set(
          kVTCompressionPropertyKey_MaxKeyFrameInterval, 7200)) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderUnsupportedConfig,
                       "Failed to set max keyframe interval to 7200 frames"});
    return false;
  }
  // This property may suddenly become unsupported when a second compression
  // session is created if the codec is H.265 and CPU arch is x64, so we can
  // always check if the property is supported before setting it.
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

  if (num_temporal_layers_ != 2) {
    return true;
  }

  if (!IsHardwareEncoder(compression_session_.get(), profile_) ||
      !IsSVCSupported(codec)) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderUnsupportedConfig,
                       "SVC encoding is not supported on this OS version or "
                       "hardware, or SW encoding was selected"});
    return false;
  }

  if (@available(macOS LOW_LATENCY_AND_SVC_AVAILABLE_VER, *)) {
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
  }

  return true;
}

void VTVideoEncodeAccelerator::MaybeRunFlushCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!pending_flush_cb_)
    return;

  if (pending_encodes_ || !encoder_output_queue_.empty())
    return;

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
}

void VTVideoEncodeAccelerator::NotifyErrorStatus(EncoderStatus status) {
  CHECK(!status.is_ok());
  LOG(ERROR) << "Call NotifyErrorStatus(): code="
             << static_cast<int>(status.code())
             << ", message=" << status.message();
  if (media_log_) {
    MEDIA_LOG(ERROR, media_log_) << status.message();
  }
  // NotifyErrorStatus() can be called without calling Initialize() in the case
  // of GetSupportedProfiles().
  if (!client_) {
    return;
  }
  client_->NotifyErrorStatus(std::move(status));
}

base::TimeDelta VTVideoEncodeAccelerator::AssignMonotonicTimestamp() {
  const base::TimeDelta step = base::Seconds(1) / frame_rate_;
  auto result = next_timestamp_;
  next_timestamp_ += step;
  return result;
}

}  // namespace media
