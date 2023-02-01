// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/vt_video_encode_accelerator_mac.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/bitrate.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/mac/video_frame_mac.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/video/video_encode_accelerator.h"

// This is a min version of macOS where we want to support SVC encoding via
// EnableLowLatencyRateControl flag. The flag is actually supported since 11.3,
// but there we see frame drops even with ample bitrate budget. Excessive frame
// drops were fixed in 12.0.1.
#define LOW_LATENCY_FLAG_AVAILABLE_VER 12.0.1

namespace media {

namespace {

constexpr size_t kBitsPerByte = 8;
constexpr size_t kDefaultFrameRateNumerator = 30;
constexpr size_t kDefaultFrameRateDenominator = 1;
constexpr size_t kMaxFrameRateNumerator = 120;
constexpr size_t kMaxFrameRateDenominator = 1;
constexpr size_t kNumInputBuffers = 3;
constexpr gfx::Size kDefaultSupportedResolution = gfx::Size(640, 480);
// TODO(crbug.com/1380682): We should add a function like a
// `GetVideoEncodeAcceleratorProfileIsSupported`, to test the
// real support status with a give resolution, framerate etc,
// instead of query a "supportedProfile" list.
constexpr gfx::Size kMaxSupportedResolution = gfx::Size(4096, 2304);

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
      NOTREACHED();
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
      NOTREACHED();
  }
  return kCMVideoCodecType_H264;
}

base::ScopedCFTypeRef<CFArrayRef> CreateRateLimitArray(const Bitrate& bitrate) {
  std::vector<CFNumberRef> limits;
  switch (bitrate.mode()) {
    case Bitrate::Mode::kConstant: {
      // CBR should be enforces with granularity of a second.
      float target_interval = 1.0;
      int32_t target_bitrate = bitrate.target_bps() / kBitsPerByte;

      limits.push_back(
          CFNumberCreate(nullptr, kCFNumberSInt32Type, &target_bitrate));
      limits.push_back(
          CFNumberCreate(nullptr, kCFNumberFloat32Type, &target_interval));
      break;
    }
    case Bitrate::Mode::kVariable: {
      // 5 seconds should be an okay interval for VBR to enforce the long-term
      // limit.
      float avg_interval = 5.0;
      int32_t avg_bitrate = base::saturated_cast<int32_t>(
          bitrate.target_bps() / kBitsPerByte * avg_interval);

      // And the peak bitrate is measured per-second in a way similar to CBR.
      float peak_interval = 1.0;
      int32_t peak_bitrate = bitrate.peak_bps() / kBitsPerByte;
      limits.push_back(
          CFNumberCreate(nullptr, kCFNumberSInt32Type, &peak_bitrate));
      limits.push_back(
          CFNumberCreate(nullptr, kCFNumberFloat32Type, &peak_interval));
      limits.push_back(
          CFNumberCreate(nullptr, kCFNumberSInt32Type, &avg_bitrate));
      limits.push_back(
          CFNumberCreate(nullptr, kCFNumberFloat32Type, &avg_interval));
      break;
    }

    default:
      NOTREACHED();
  }

  base::ScopedCFTypeRef<CFArrayRef> result(CFArrayCreate(
      kCFAllocatorDefault, reinterpret_cast<const void**>(limits.data()),
      limits.size(), &kCFTypeArrayCallBacks));
  for (auto* number : limits)
    CFRelease(number);
  return result;
}

VideoEncoderInfo GetVideoEncoderInfo(VTSessionRef compression_session,
                                     VideoCodecProfile profile) {
  VideoEncoderInfo info;
  info.implementation_name = "VideoToolbox";
  info.is_hardware_accelerated = false;

  base::ScopedCFTypeRef<CFBooleanRef> cf_using_hardware;
  if (VTSessionCopyProperty(
          compression_session,
          kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder,
          kCFAllocatorDefault, cf_using_hardware.InitializeInto()) == 0) {
    info.is_hardware_accelerated = CFBooleanGetValue(cf_using_hardware);
  }

  absl::optional<int> max_frame_delay_property;
  base::ScopedCFTypeRef<CFNumberRef> max_frame_delay_count;
  if (VTSessionCopyProperty(
          compression_session, kVTCompressionPropertyKey_MaxFrameDelayCount,
          kCFAllocatorDefault, max_frame_delay_count.InitializeInto()) == 0) {
    int32_t frame_delay;
    if (CFNumberGetValue(max_frame_delay_count, kCFNumberSInt32Type,
                         &frame_delay) &&
        frame_delay != kVTUnlimitedFrameDelayCount) {
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
    info.frame_delay =
        profile == H264PROFILE_BASELINE || profile == HEVCPROFILE_MAIN ? 0 : 13;
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
  InProgressFrameEncode(base::TimeDelta rtp_timestamp)
      : timestamp(rtp_timestamp) {}

  const base::TimeDelta timestamp;
};

struct VTVideoEncodeAccelerator::EncodeOutput {
  EncodeOutput() = delete;

  EncodeOutput(VTEncodeInfoFlags info_flags,
               CMSampleBufferRef sbuf,
               base::TimeDelta timestamp)
      : info(info_flags),
        sample_buffer(sbuf, base::scoped_policy::RETAIN),
        capture_timestamp(timestamp) {}

  EncodeOutput(const EncodeOutput&) = delete;
  EncodeOutput& operator=(const EncodeOutput&) = delete;

  const VTEncodeInfoFlags info;
  const base::ScopedCFTypeRef<CMSampleBufferRef> sample_buffer;
  const base::TimeDelta capture_timestamp;
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

// .5 is set as a minimum to prevent overcompensating for large temporary
// overshoots. We don't want to degrade video quality too badly.
// .95 is set to prevent oscillations. When a lower bitrate is set on the
// encoder than previously set, its output seems to have a brief period of
// drastically reduced bitrate, so we want to avoid that. In steady state
// conditions, 0.95 seems to give us better overall bitrate over long periods
// of time.
VTVideoEncodeAccelerator::VTVideoEncodeAccelerator()
    : bitrate_adjuster_(.5, .95),
      client_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      encoder_thread_task_runner_(
          base::ThreadPool::CreateSingleThreadTaskRunner({})),
      encoder_task_weak_factory_(this) {
  encoder_weak_ptr_ = encoder_task_weak_factory_.GetWeakPtr();
}

VTVideoEncodeAccelerator::~VTVideoEncodeAccelerator() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(!encoder_task_weak_factory_.HasWeakPtrs());
}

VideoEncodeAccelerator::SupportedProfiles
VTVideoEncodeAccelerator::GetSupportedH264Profiles() {
  SupportedProfiles profiles;
  bool supported =
      CreateCompressionSession(VideoCodec::kH264, kDefaultSupportedResolution);
  DestroyCompressionSession();
  if (!supported) {
    DVLOG(1) << "Hardware H.264 encode acceleration is not available on this "
                "platform.";
    return profiles;
  }
  SupportedProfile profile;
  profile.max_resolution = kMaxSupportedResolution;
  profile.max_framerate_numerator = kMaxFrameRateNumerator;
  profile.max_framerate_denominator = kMaxFrameRateDenominator;
  profile.rate_control_modes = VideoEncodeAccelerator::kConstantMode |
                               VideoEncodeAccelerator::kVariableMode;
  profile.scalability_modes.push_back(SVCScalabilityMode::kL1T1);
  if (__builtin_available(macOS LOW_LATENCY_FLAG_AVAILABLE_VER, *))
    profile.scalability_modes.push_back(SVCScalabilityMode::kL1T2);

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
        profiles.push_back(profile);

        // macOS doesn't provide a way to enumerate codec details, so just
        // assume software codec support is the same as hardware, but with
        // the lowest possible minimum resolution.
        profile.min_resolution = gfx::Size(2, 2);
        profile.is_software_codec = true;
        profiles.push_back(profile);
      }
    }
  }
  return profiles;
}

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
VideoEncodeAccelerator::SupportedProfiles
VTVideoEncodeAccelerator::GetSupportedHEVCProfiles() {
  SupportedProfiles profiles;
  if (!base::FeatureList::IsEnabled(kPlatformHEVCEncoderSupport))
    return profiles;
  if (__builtin_available(macOS 11.0, *)) {
    bool supported = CreateCompressionSession(VideoCodec::kHEVC,
                                              kDefaultSupportedResolution);
    DestroyCompressionSession();
    if (!supported) {
      DVLOG(1) << "Hardware HEVC encode acceleration is not available on this "
                  "platform.";
      return profiles;
    }
    SupportedProfile profile;
    profile.max_resolution = kMaxSupportedResolution;
    profile.max_framerate_numerator = kMaxFrameRateNumerator;
    profile.max_framerate_denominator = kMaxFrameRateDenominator;
    profile.rate_control_modes = VideoEncodeAccelerator::kConstantMode |
                                 VideoEncodeAccelerator::kVariableMode;
    for (const auto& supported_profile : kSupportedProfiles) {
      if (VideoCodecProfileToVideoCodec(supported_profile) ==
          VideoCodec::kHEVC) {
        profile.is_software_codec = false;
        profile.profile = supported_profile;
        profiles.push_back(profile);

        // macOS doesn't provide a way to enumerate codec details, so just
        // assume software codec support is the same as hardware, but with
        // the lowest possible minimum resolution.
        profile.min_resolution = gfx::Size(2, 2);
        profile.is_software_codec = true;
        profiles.push_back(profile);
      }
    }
  }
  return profiles;
}
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

VideoEncodeAccelerator::SupportedProfiles
VTVideoEncodeAccelerator::GetSupportedProfiles() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(client);

  // Clients are expected to call Flush() before reinitializing the encoder.
  DCHECK_EQ(pending_encodes_, 0);

  if (config.input_format != PIXEL_FORMAT_I420 &&
      config.input_format != PIXEL_FORMAT_NV12) {
    MEDIA_LOG(ERROR, media_log.get())
        << "Input format not supported= "
        << VideoPixelFormatToString(config.input_format);
    return false;
  }
  if (!base::Contains(kSupportedProfiles, config.output_profile)) {
    MEDIA_LOG(ERROR, media_log.get()) << "Output profile not supported= "
                                      << GetProfileName(config.output_profile);
    return false;
  }
  profile_ = config.output_profile;
  codec_ = VideoCodecProfileToVideoCodec(config.output_profile);
  client_ptr_factory_ = std::make_unique<base::WeakPtrFactory<Client>>(client);
  client_ = client_ptr_factory_->GetWeakPtr();
  input_visible_size_ = config.input_visible_size;
  if (config.initial_framerate.has_value())
    frame_rate_ = config.initial_framerate.value();
  else
    frame_rate_ = kDefaultFrameRateNumerator / kDefaultFrameRateDenominator;
  bitrate_ = config.bitrate;
  bitstream_buffer_size_ = config.input_visible_size.GetArea();
  require_low_delay_ = config.require_low_delay;

  if (codec_ == VideoCodec::kH264 || codec_ == VideoCodec::kHEVC) {
    required_encoder_type_ = config.required_encoder_type;
  } else {
    DLOG(ERROR) << "Software encoder selection is only allowed for H264/H265.";
  }

  if (config.HasTemporalLayer())
    num_temporal_layers_ = config.spatial_layers.front().num_of_temporal_layers;

  if (num_temporal_layers_ > 2) {
    MEDIA_LOG(ERROR, media_log.get())
        << "Unsupported number of SVC temporal layers.";
    return false;
  }

  if (!ResetCompressionSession(codec_)) {
    MEDIA_LOG(ERROR, media_log.get()) << "Failed creating compression session.";
    return false;
  }

  auto encoder_info = GetVideoEncoderInfo(compression_session_, profile_);

  // Report whether hardware encode is being used.
  if (!encoder_info.is_hardware_accelerated) {
    MEDIA_LOG(INFO, media_log.get())
        << "VideoToolbox selected a software encoder.";
  }

  client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Client::NotifyEncoderInfoChange, client_, encoder_info));

  client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::RequireBitstreamBuffers, client_,
                                kNumInputBuffers, input_visible_size_,
                                bitstream_buffer_size_));
  return true;
}

void VTVideoEncodeAccelerator::Encode(scoped_refptr<VideoFrame> frame,
                                      bool force_keyframe) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VTVideoEncodeAccelerator::EncodeTask, encoder_weak_ptr_,
                     std::move(frame), force_keyframe));
}

void VTVideoEncodeAccelerator::UseOutputBitstreamBuffer(
    BitstreamBuffer buffer) {
  DVLOG(3) << __func__ << ": buffer size=" << buffer.size();
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  if (buffer.size() < bitstream_buffer_size_) {
    DLOG(ERROR) << "Output BitstreamBuffer isn't big enough: " << buffer.size()
                << " vs. " << bitstream_buffer_size_;
    client_->NotifyError(kInvalidArgumentError);
    return;
  }

  auto mapping = buffer.TakeRegion().Map();
  if (!mapping.IsValid()) {
    DLOG(ERROR) << "Failed mapping shared memory.";
    client_->NotifyError(kPlatformFailureError);
    return;
  }

  std::unique_ptr<BitstreamBufferRef> buffer_ref(
      new BitstreamBufferRef(buffer.id(), std::move(mapping), buffer.size()));

  encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VTVideoEncodeAccelerator::UseOutputBitstreamBufferTask,
                     encoder_weak_ptr_, std::move(buffer_ref)));
}

void VTVideoEncodeAccelerator::RequestEncodingParametersChange(
    const Bitrate& bitrate,
    uint32_t framerate) {
  DVLOG(3) << __func__ << ": bitrate=" << bitrate.ToString()
           << ": framerate=" << framerate;
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VTVideoEncodeAccelerator::RequestEncodingParametersChangeTask,
          encoder_weak_ptr_, bitrate, framerate));
}

void VTVideoEncodeAccelerator::Destroy() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  // Cancel all callbacks.
  client_ptr_factory_.reset();

  // VT resources need to be cleaned up on |encoder_thread_task_runner_|,
  // but the object itself is supposed to be deleted on this runner, so when
  // DestroyTask() is done we schedule deletion of |this|
  auto delete_self = [](VTVideoEncodeAccelerator* self) { delete self; };
  encoder_thread_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&VTVideoEncodeAccelerator::DestroyTask, encoder_weak_ptr_),
      base::BindOnce(delete_self, base::Unretained(this)));
}

void VTVideoEncodeAccelerator::Flush(FlushCallback flush_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  encoder_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VTVideoEncodeAccelerator::FlushTask,
                                encoder_weak_ptr_, std::move(flush_callback)));
}

bool VTVideoEncodeAccelerator::IsFlushSupported() {
  return true;
}

void VTVideoEncodeAccelerator::EncodeTask(scoped_refptr<VideoFrame> frame,
                                          bool force_keyframe) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(compression_session_);
  DCHECK(frame);

  auto pixel_buffer = WrapVideoFrameInCVPixelBuffer(frame);
  if (!pixel_buffer) {
    DLOG(ERROR) << "WrapVideoFrameInCVPixelBuffer failed.";
    NotifyError(kPlatformFailureError);
    return;
  }
  base::ScopedCFTypeRef<CFDictionaryRef> frame_props =
      video_toolbox::DictionaryWithKeyValue(
          kVTEncodeFrameOptionKey_ForceKeyFrame,
          force_keyframe ? kCFBooleanTrue : kCFBooleanFalse);

  auto timestamp_cm =
      CMTimeMake(frame->timestamp().InMicroseconds(), USEC_PER_SEC);
  // Wrap information we'll need after the frame is encoded in a heap object.
  // We'll get the pointer back from the VideoToolbox completion callback.
  std::unique_ptr<InProgressFrameEncode> request(
      new InProgressFrameEncode(frame->timestamp()));

  if (bitrate_.mode() == Bitrate::Mode::kConstant) {
    // In CBR mode, we adjust bitrate before every encode based on past history
    // of bitrate adherence.
    SetAdjustedConstantBitrate(bitrate_adjuster_.GetAdjustedBitrateBps());
  }

  // We can pass the ownership of |request| to the encode callback if
  // successful. Otherwise let it fall out of scope.
  OSStatus status = VTCompressionSessionEncodeFrame(
      compression_session_, pixel_buffer, timestamp_cm, kCMTimeInvalid,
      frame_props, reinterpret_cast<void*>(request.get()), nullptr);
  if (status != noErr) {
    DLOG(ERROR) << " VTCompressionSessionEncodeFrame failed: " << status;
    NotifyError(kPlatformFailureError);
  } else {
    ++pending_encodes_;
    CHECK(request.release());
  }
}

void VTVideoEncodeAccelerator::UseOutputBitstreamBufferTask(
    std::unique_ptr<BitstreamBufferRef> buffer_ref) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  // If there is already EncodeOutput waiting, copy its output first.
  if (!encoder_output_queue_.empty()) {
    std::unique_ptr<VTVideoEncodeAccelerator::EncodeOutput> encode_output =
        std::move(encoder_output_queue_.front());
    encoder_output_queue_.pop_front();
    ReturnBitstreamBuffer(std::move(encode_output), std::move(buffer_ref));
    return;
  }

  bitstream_buffer_queue_.push_back(std::move(buffer_ref));
}

void VTVideoEncodeAccelerator::RequestEncodingParametersChangeTask(
    const Bitrate& bitrate,
    uint32_t framerate) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  if (!compression_session_) {
    NotifyError(kPlatformFailureError);
    return;
  }

  frame_rate_ = framerate;
  video_toolbox::SessionPropertySetter session_property_setter(
      compression_session_);
  session_property_setter.Set(kVTCompressionPropertyKey_ExpectedFrameRate,
                              frame_rate_);

  switch (bitrate.mode()) {
    case Bitrate::Mode::kConstant:
      if (bitrate.target_bps() != static_cast<uint32_t>(target_bitrate_)) {
        target_bitrate_ = bitrate.target_bps();
        bitrate_adjuster_.SetTargetBitrateBps(target_bitrate_);
        SetAdjustedConstantBitrate(bitrate_adjuster_.GetAdjustedBitrateBps());
      }
      break;
    case Bitrate::Mode::kVariable:
      SetVariableBitrate(bitrate);
      break;
    default:
      NOTREACHED();
  }
  bitrate_ = bitrate;
}

void VTVideoEncodeAccelerator::SetAdjustedConstantBitrate(uint32_t bitrate) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  if (bitrate == encoder_set_bitrate_)
    return;

  encoder_set_bitrate_ = bitrate;
  video_toolbox::SessionPropertySetter session_property_setter(
      compression_session_);
  [[maybe_unused]] bool rv = session_property_setter.Set(
      kVTCompressionPropertyKey_AverageBitRate,
      base::saturated_cast<int32_t>(encoder_set_bitrate_));
  rv &= session_property_setter.Set(
      kVTCompressionPropertyKey_DataRateLimits,
      CreateRateLimitArray(Bitrate::ConstantBitrate(bitrate)));
  DLOG_IF(ERROR, !rv)
      << "Couldn't change bitrate parameters of encode session.";
}

void VTVideoEncodeAccelerator::SetVariableBitrate(const Bitrate& bitrate) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(bitrate.mode() == Bitrate::Mode::kVariable);

  video_toolbox::SessionPropertySetter session_property_setter(
      compression_session_);
  [[maybe_unused]] bool rv =
      session_property_setter.Set(kVTCompressionPropertyKey_AverageBitRate,
                                  static_cast<int32_t>(bitrate.target_bps()));
  rv &= session_property_setter.Set(kVTCompressionPropertyKey_DataRateLimits,
                                    CreateRateLimitArray(bitrate));
  DLOG_IF(ERROR, !rv)
      << "Couldn't change bitrate parameters of encode session.";
}

void VTVideoEncodeAccelerator::DestroyTask() {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  // Cancel all encoder thread callbacks.
  encoder_task_weak_factory_.InvalidateWeakPtrs();
  DestroyCompressionSession();
}

void VTVideoEncodeAccelerator::NotifyError(
    VideoEncodeAccelerator::Error error) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::NotifyError, client_, error));
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
  std::unique_ptr<EncodeOutput> encode_output(
      new EncodeOutput(info, sbuf, frame_info->timestamp));

  // This method is NOT called on |encoder_thread_|, so we still need to
  // post a task back to it to do work.
  encoder->encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VTVideoEncodeAccelerator::CompressionCallbackTask,
                     encoder->encoder_weak_ptr_, status,
                     std::move(encode_output)));
}

void VTVideoEncodeAccelerator::CompressionCallbackTask(
    OSStatus status,
    std::unique_ptr<EncodeOutput> encode_output) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  --pending_encodes_;
  DCHECK_GE(pending_encodes_, 0);

  if (status != noErr) {
    DLOG(ERROR) << " encode failed: " << status;
    NotifyError(kPlatformFailureError);
    return;
  }

  // If there isn't any BitstreamBuffer to copy into, add it to a queue for
  // later use.
  if (bitstream_buffer_queue_.empty()) {
    encoder_output_queue_.push_back(std::move(encode_output));
    return;
  }

  std::unique_ptr<VTVideoEncodeAccelerator::BitstreamBufferRef> buffer_ref =
      std::move(bitstream_buffer_queue_.front());
  bitstream_buffer_queue_.pop_front();
  ReturnBitstreamBuffer(std::move(encode_output), std::move(buffer_ref));
}

void VTVideoEncodeAccelerator::ReturnBitstreamBuffer(
    std::unique_ptr<EncodeOutput> encode_output,
    std::unique_ptr<VTVideoEncodeAccelerator::BitstreamBufferRef> buffer_ref) {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  if (encode_output->info & kVTEncodeInfo_FrameDropped) {
    DVLOG(2) << " frame dropped";
    client_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&Client::BitstreamBufferReady, client_, buffer_ref->id,
                       BitstreamBufferMetadata(
                           0, false, encode_output->capture_timestamp)));
    MaybeRunFlushCallback();
    return;
  }

  auto* sample_attachments = static_cast<CFDictionaryRef>(
      CFArrayGetValueAtIndex(CMSampleBufferGetSampleAttachmentsArray(
                                 encode_output->sample_buffer.get(), true),
                             0));
  const bool keyframe = !CFDictionaryContainsKey(
      sample_attachments, kCMSampleAttachmentKey_NotSync);
  bool belongs_to_base_layer = true;
  if (CFBooleanRef value_ptr = base::mac::GetValueFromDictionary<CFBooleanRef>(
          sample_attachments, kCMSampleAttachmentKey_IsDependedOnByOthers)) {
    belongs_to_base_layer = static_cast<bool>(CFBooleanGetValue(value_ptr));
  }

  size_t used_buffer_size = 0;
  const bool copy_rv = video_toolbox::CopySampleBufferToAnnexBBuffer(
      codec_, encode_output->sample_buffer.get(), keyframe, buffer_ref->size,
      static_cast<char*>(buffer_ref->mapping.memory()), &used_buffer_size);
  if (!copy_rv) {
    DLOG(ERROR) << "Cannot copy output from SampleBuffer to AnnexBBuffer.";
    used_buffer_size = 0;
  }

  if (bitrate_.mode() == Bitrate::Mode::kConstant) {
    // In CBR mode, we let bitrate adjuster know how much encoded data was
    // produced to better control bitrate adherence.
    bitrate_adjuster_.Update(used_buffer_size);
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
      NOTREACHED();
      break;
  }

  client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::BitstreamBufferReady, client_,
                                buffer_ref->id, std::move(md)));
  MaybeRunFlushCallback();
}

bool VTVideoEncodeAccelerator::ResetCompressionSession(VideoCodec codec) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  DestroyCompressionSession();

  bool session_rv = CreateCompressionSession(codec, input_visible_size_);
  if (!session_rv)
    return false;

  const bool configure_rv = ConfigureCompressionSession(codec);
  if (configure_rv)
    RequestEncodingParametersChange(bitrate_, frame_rate_);
  return configure_rv;
}

bool VTVideoEncodeAccelerator::CreateCompressionSession(
    VideoCodec codec,
    const gfx::Size& input_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  std::vector<CFTypeRef> encoder_keys{
      kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder};
  std::vector<CFTypeRef> encoder_values{required_encoder_type_ ==
                                                Config::EncoderType::kHardware
                                            ? kCFBooleanTrue
                                            : kCFBooleanFalse};
  if (required_encoder_type_ == Config::EncoderType::kSoftware) {
    encoder_keys.push_back(
        kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder);
    encoder_values.push_back(kCFBooleanFalse);
  }

  if (__builtin_available(macOS LOW_LATENCY_FLAG_AVAILABLE_VER, *)) {
    // Remove the validation once HEVC SVC mode is supported on macOS.
    if (require_low_delay_ && codec == VideoCodec::kH264) {
      encoder_keys.push_back(
          kVTVideoEncoderSpecification_EnableLowLatencyRateControl);
      encoder_values.push_back(kCFBooleanTrue);
    }
  }
  base::ScopedCFTypeRef<CFDictionaryRef> encoder_spec =
      video_toolbox::DictionaryWithKeysAndValues(
          encoder_keys.data(), encoder_values.data(), encoder_keys.size());

  // Create the compression session.
  // Note that the encoder object is given to the compression session as the
  // callback context using a raw pointer. The C API does not allow us to use a
  // smart pointer, nor is this encoder ref counted. However, this is still
  // safe, because we 1) we own the compression session and 2) we tear it down
  // safely. When destructing the encoder, the compression session is flushed
  // and invalidated. Internally, VideoToolbox will join all of its threads
  // before returning to the client. Therefore, when control returns to us, we
  // are guaranteed that the output callback will not execute again.
  OSStatus status = VTCompressionSessionCreate(
      kCFAllocatorDefault, input_size.width(), input_size.height(),
      VideoCodecToCMVideoCodec(codec), encoder_spec,
      nullptr /* sourceImageBufferAttributes */,
      nullptr /* compressedDataAllocator */,
      &VTVideoEncodeAccelerator::CompressionCallback,
      reinterpret_cast<void*>(this), compression_session_.InitializeInto());
  if (status != noErr) {
    // IMPORTANT: ScopedCFTypeRef::release() doesn't call CFRelease().
    // In case of an error VTCompressionSessionCreate() is not supposed to
    // write a non-null value into compression_session_, but just in case,
    // we'll clear it without calling CFRelease() because it can be unsafe
    // to call on a not fully created session.
    (void)compression_session_.release();
    OSSTATUS_DLOG(ERROR, status) << " VTCompressionSessionCreate failed: ";
    return false;
  }
  DVLOG(3) << " VTCompressionSession created with input size="
           << input_size.ToString();
  return true;
}

bool VTVideoEncodeAccelerator::ConfigureCompressionSession(VideoCodec codec) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(compression_session_);

  video_toolbox::SessionPropertySetter session_property_setter(
      compression_session_);
  bool rv = true;
  rv &= session_property_setter.Set(kVTCompressionPropertyKey_ProfileLevel,
                                    VideoCodecProfileToVTProfile(profile_));
  // Remove the validation once HEVC SVC mode is supported on macOS.
  rv &= session_property_setter.Set(
      kVTCompressionPropertyKey_RealTime,
      require_low_delay_ && codec == VideoCodec::kH264);

  rv &= session_property_setter.Set(
      kVTCompressionPropertyKey_AllowFrameReordering, false);
  // Limit keyframe output to 4 minutes, see https://crbug.com/658429.
  rv &= session_property_setter.Set(
      kVTCompressionPropertyKey_MaxKeyFrameInterval, 7200);
  rv &= session_property_setter.Set(
      kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration, 240);
  DLOG_IF(ERROR, !rv) << " Setting session property failed.";

  if (session_property_setter.IsSupported(
          kVTCompressionPropertyKey_MaxFrameDelayCount)) {
    rv &= session_property_setter.Set(
        kVTCompressionPropertyKey_MaxFrameDelayCount,
        static_cast<int>(kNumInputBuffers));
  } else {
    DLOG(WARNING) << "MaxFrameDelayCount is not supported";
  }

  // Remove the validation once HEVC SVC mode is supported on macOS.
  if (num_temporal_layers_ == 2 && codec_ == VideoCodec::kH264) {
    if (__builtin_available(macOS LOW_LATENCY_FLAG_AVAILABLE_VER, *)) {
      if (!session_property_setter.IsSupported(
              kVTCompressionPropertyKey_BaseLayerFrameRateFraction)) {
        DLOG(ERROR) << "BaseLayerFrameRateFraction is not supported";
        return false;
      }
      rv &= session_property_setter.Set(
          kVTCompressionPropertyKey_BaseLayerFrameRateFraction, 0.5);
      DLOG_IF(ERROR, !rv) << " Setting BaseLayerFrameRate property failed.";
    } else {
      DLOG(ERROR) << "SVC encoding is not supported on this OS version.";
      rv = false;
    }
  }

  return rv;
}

void VTVideoEncodeAccelerator::DestroyCompressionSession() {
  if (compression_session_) {
    VTCompressionSessionInvalidate(compression_session_);
    compression_session_.reset();
  }
}

void VTVideoEncodeAccelerator::FlushTask(FlushCallback flush_callback) {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(flush_callback);

  if (!compression_session_) {
    client_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(flush_callback), false));
    return;
  }

  // Even though this will block until all frames are returned, the frames will
  // be posted to the current task runner, so we can't run the flush callback
  // at this time.
  OSStatus status =
      VTCompressionSessionCompleteFrames(compression_session_, kCMTimeInvalid);

  if (status != noErr) {
    OSSTATUS_DLOG(ERROR, status)
        << " VTCompressionSessionCompleteFrames failed: " << status;
    client_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(flush_callback), /*success=*/false));
    return;
  }

  pending_flush_cb_ = std::move(flush_callback);
  MaybeRunFlushCallback();
}

void VTVideoEncodeAccelerator::MaybeRunFlushCallback() {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  if (!pending_flush_cb_)
    return;

  if (pending_encodes_ || !encoder_output_queue_.empty())
    return;

  client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(pending_flush_cb_), /*success=*/true));
}

}  // namespace media
