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
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
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

// This is a min version of macOS where we want to support SVC encoding via
// EnableLowLatencyRateControl flag. The flag is actually supported since 11.3,
// but there we see frame drops even with ample bitrate budget. Excessive frame
// drops were fixed in 12.0.1.
#define LOW_LATENCY_FLAG_AVAILABLE_VER 12.0.1

namespace media {

namespace {

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

VideoEncoderInfo GetVideoEncoderInfo(VTSessionRef compression_session,
                                     VideoCodecProfile profile) {
  VideoEncoderInfo info;
  info.implementation_name = "VideoToolbox";
#if BUILDFLAG(IS_MAC)
  info.is_hardware_accelerated = false;

  base::ScopedCFTypeRef<CFBooleanRef> cf_using_hardware;
  if (VTSessionCopyProperty(
          compression_session,
          kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder,
          kCFAllocatorDefault, cf_using_hardware.InitializeInto()) == 0) {
    info.is_hardware_accelerated = CFBooleanGetValue(cf_using_hardware);
  }
#else
  // iOS is always hardware-accelerated.
  info.is_hardware_accelerated = true;
#endif

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
  const base::ScopedCFTypeRef<CMSampleBufferRef> sample_buffer;
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
  // Advertise VBR here, even though the peak bitrate is never actually used.
  // See RequestEncodingParametersChange() for more details.
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
    MEDIA_LOG(ERROR, media_log) << "Unsupported number of SVC temporal layers.";
    return false;
  }

  if (!ResetCompressionSession(codec_)) {
    MEDIA_LOG(ERROR, media_log) << "Failed creating compression session.";
    return false;
  }

  auto encoder_info = GetVideoEncoderInfo(compression_session_, profile_);

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
    //     passthough to the pixel buffer.
    //   * If we're uploading to a new pixel buffer and the provided frame color
    //     space is valid that'll be set on the pixel buffer.
    //   * If the frame color space is not valid, BT709 will be assumed.
    auto frame_cs = GetImageBufferColorSpace(pixel_buffer);
    if (encoder_color_space_ && frame_cs != encoder_color_space_) {
      if (pending_encodes_) {
        auto status = VTCompressionSessionCompleteFrames(compression_session_,
                                                         kCMTimeInvalid);
        if (status != noErr) {
          NotifyErrorStatus(
              {EncoderStatus::Codes::kEncoderFailedFlush,
               "flush failed: " + logging::DescriptionFromOSStatus(status)});
          return;
        }
      }
      if (!ResetCompressionSession(codec_)) {
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

  base::ScopedCFTypeRef<CFDictionaryRef> frame_props =
      video_toolbox::DictionaryWithKeyValue(
          kVTEncodeFrameOptionKey_ForceKeyFrame,
          force_keyframe ? kCFBooleanTrue : kCFBooleanFalse);

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
      compression_session_, pixel_buffer, timestamp_cm, duration_cm,
      frame_props, reinterpret_cast<void*>(request.get()), nullptr);
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
    uint32_t framerate) {
  DVLOG(3) << __func__ << ": bitrate=" << bitrate.ToString()
           << ": framerate=" << framerate;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!compression_session_) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kEncoderIllegalState, "No compression session"});
    return;
  }

  frame_rate_ = framerate;
  video_toolbox::SessionPropertySetter session_property_setter(
      compression_session_);
  session_property_setter.Set(kVTCompressionPropertyKey_ExpectedFrameRate,
                              frame_rate_);
  session_property_setter.Set(kVTCompressionPropertyKey_AverageBitRate,
                              static_cast<int32_t>(bitrate.target_bps()));
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
  DestroyCompressionSession();
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
  OSStatus status =
      VTCompressionSessionCompleteFrames(compression_session_, kCMTimeInvalid);

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
    client_->BitstreamBufferReady(
        buffer_ref->id,
        BitstreamBufferMetadata(0, false, encode_output->capture_timestamp));
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

  md.encoded_color_space = encode_output->encoded_color_space;

  client_->BitstreamBufferReady(buffer_ref->id, std::move(md));
  MaybeRunFlushCallback();
}

bool VTVideoEncodeAccelerator::ResetCompressionSession(VideoCodec codec) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DestroyCompressionSession();

  if (!CreateCompressionSession(codec, input_visible_size_)) {
    return false;
  }

  if (!ConfigureCompressionSession(codec)) {
    return false;
  }

  RequestEncodingParametersChange(bitrate_, frame_rate_);
  return true;
}

bool VTVideoEncodeAccelerator::CreateCompressionSession(
    VideoCodec codec,
    const gfx::Size& input_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<CFTypeRef> encoder_keys;
  std::vector<CFTypeRef> encoder_values;
  // iOS is always hardware-accelerate while on mac, encoder configuration
  // handling is necessary.
#if BUILDFLAG(IS_MAC)
  encoder_keys.push_back(
      kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder);
  encoder_values.push_back(required_encoder_type_ ==
                                   Config::EncoderType::kHardware
                               ? kCFBooleanTrue
                               : kCFBooleanFalse);

  if (required_encoder_type_ == Config::EncoderType::kSoftware) {
    encoder_keys.push_back(
        kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder);
    encoder_values.push_back(kCFBooleanFalse);
  }
#endif

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
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderInitializationError,
                       "VTCompressionSessionCreate failed: " +
                           logging::DescriptionFromOSStatus(status)});

    return false;
  }
  DVLOG(3) << " VTCompressionSession created with input size="
           << input_size.ToString();
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
  // Remove the validation once HEVC SVC mode is supported on macOS.
  if (!session_property_setter.Set(
          kVTCompressionPropertyKey_RealTime,
          require_low_delay_ && codec == VideoCodec::kH264)) {
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
  if (!session_property_setter.Set(
          kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration, 240)) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kEncoderUnsupportedConfig,
         "Failed to set max keyframe interval duration to 240 seconds"});
    return false;
  }

  if (session_property_setter.IsSupported(
          kVTCompressionPropertyKey_MaxFrameDelayCount)) {
    if (!session_property_setter.Set(
            kVTCompressionPropertyKey_MaxFrameDelayCount,
            static_cast<int>(kNumInputBuffers))) {
      NotifyErrorStatus({EncoderStatus::Codes::kEncoderUnsupportedConfig,
                         "Failed to set max frame delay count to " +
                             base::NumberToString(kNumInputBuffers)});
      return false;
    }
  } else {
    DLOG(WARNING) << "MaxFrameDelayCount is not supported";
  }

  // Remove the validation once HEVC SVC mode is supported on macOS.
  if (num_temporal_layers_ == 2 && codec_ == VideoCodec::kH264) {
    if (__builtin_available(macOS LOW_LATENCY_FLAG_AVAILABLE_VER, *)) {
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
    } else {
      NotifyErrorStatus({EncoderStatus::Codes::kEncoderUnsupportedConfig,
                         "SVC encoding is not supported on this OS version"});
      return false;
    }
  }

  return true;
}

void VTVideoEncodeAccelerator::DestroyCompressionSession() {
  if (compression_session_) {
    VTCompressionSessionInvalidate(compression_session_);
    compression_session_.reset();
  }
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
