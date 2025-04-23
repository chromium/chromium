// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/media_foundation_video_encode_accelerator_win.h"

#include <objbase.h>

#include <codecapi.h>
#include <d3d11_1.h>
#include <mfapi.h>
#include <mferror.h>
#include <mftransform.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "base/containers/heap_array.h"
#include "base/features.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/native_library.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_variant.h"
#include "base/win/win_util.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/ipc/common/dxgi_helpers.h"
#include "gpu/ipc/service/shared_image_stub.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/encoder_status.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/base/win/color_space_util_win.h"
#include "media/base/win/mf_helpers.h"
#include "media/base/win/mf_initializer.h"
#include "media/gpu/gpu_video_encode_accelerator_helpers.h"
#include "media/gpu/h264_rate_controller.h"
#include "media/gpu/h264_ratectrl_rtc.h"
#include "media/gpu/windows/av1_video_rate_control_wrapper.h"
#include "media/gpu/windows/format_utils.h"
#include "media/gpu/windows/h264_video_rate_control_wrapper.h"
#include "media/gpu/windows/mf_video_encoder_shared_state.h"
#include "media/gpu/windows/mf_video_encoder_switches.h"
#include "media/gpu/windows/vp9_video_rate_control_wrapper.h"
#include "media/parsers/temporal_scalability_id_extractor.h"
#include "third_party/libaom/source/libaom/av1/ratectrl_rtc.h"
#include "third_party/libvpx/source/libvpx/vp9/ratectrl_rtc.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/color_space_win.h"
#include "ui/gfx/gpu_memory_buffer.h"

using Microsoft::WRL::ComPtr;

namespace media {

namespace {
constexpr uint32_t kDefaultGOPLength = 3000;
constexpr uint32_t kDefaultTargetBitrate = 5000000u;
constexpr size_t kNumInputBuffers = 3;
// Media Foundation uses 100 nanosecond units for time, see
// https://msdn.microsoft.com/en-us/library/windows/desktop/ms697282(v=vs.85).aspx.
constexpr size_t kOneMicrosecondInMFSampleTimeUnits = 10;

// Get distance from current frame to next temporal base layer frame.
uint32_t GetDistanceToNextTemporalBaseLayer(uint32_t frame_number,
                                            uint32_t temporal_layer_count) {
  DCHECK(temporal_layer_count >= 1 && temporal_layer_count <= 3);
  uint32_t pattern_count = 1 << (temporal_layer_count - 1);
  return (frame_number % pattern_count == 0)
             ? 0
             : pattern_count - (frame_number % pattern_count);
}

// Per
// https://learn.microsoft.com/en-us/windows/win32/medfound/handling-stream-changes,
// encoders should only accept an input type that matches the currently
// configured output type. If we want to change the frame rate, a
// stream restart flow is needed, which in turn generates a key-frame on the
// stream restart. This is not friendly for WebRTC encoding, which adjusts the
// encoding frame rate frequently.
// To mitigate this, we only configure the frame rate during HMFT
// initialization. On subsequent frame rate update request, if new frame rate is
// larger than currently configured frame rate and bitrate is kept unchanged,
// this implies average encoded frame size should decrease proportionally. Since
// we don't actually configure the new frame rate into HMFT(to avoid stream
// restart), we emulate this average frame size decrease by proportionally
// decreasing the target/peak bitrate(which does not require stream restart).
// This is similar for frame rate update request that is lower than currently
// configured, by increasing bitrate to emulate average frame size increase.
// See https://crbug.com/1295815 for more details.
uint32_t AdjustBitrateToFrameRate(uint32_t bitrate,
                                  uint32_t configured_framerate,
                                  uint32_t requested_framerate) {
  if (requested_framerate == 0u) {
    return 0u;
  }

  return bitrate * configured_framerate / requested_framerate;
}

VideoRateControlWrapper::RateControlConfig CreateRateControllerConfig(
    const VideoBitrateAllocation& bitrate_allocation,
    gfx::Size size,
    uint32_t frame_rate,
    int num_temporal_layers,
    VideoCodec codec,
    VideoEncodeAccelerator::Config::ContentType content_type) {
  // Fill rate control config variables.
  VideoRateControlWrapper::RateControlConfig config;
  config.content_type = content_type;
  config.width = size.width();
  config.height = size.height();
  config.target_bandwidth = bitrate_allocation.GetSumBps() / 1000;
  config.framerate = frame_rate;
  config.ss_number_layers = 1;
  config.ts_number_layers = num_temporal_layers;
  switch (codec) {
    case VideoCodec::kVP9: {
      config.max_quantizer = kVP9MaxQuantizer;
      config.min_quantizer = kVP9MinQuantizer;
      break;
    }
    case VideoCodec::kAV1: {
      config.max_quantizer = kAV1MaxQuantizer;
      config.min_quantizer = kAV1MinQuantizer;
      break;
    }
    case VideoCodec::kH264: {
      config.max_quantizer = kH264MaxQuantizer;
      config.min_quantizer = kH264MinQuantizer;
      break;
    }
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    case VideoCodec::kHEVC: {
      config.max_quantizer = kH265MaxQuantizer;
      config.min_quantizer = kH265MinQuantizer;
      break;
    }
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
    default:
      NOTREACHED();
  }
  int bitrate_sum = 0;
  for (int tid = 0; tid < num_temporal_layers; ++tid) {
    bitrate_sum += bitrate_allocation.GetBitrateBps(0, tid);
    config.layer_target_bitrate[tid] = bitrate_sum / 1000;
    config.ts_rate_decimator[tid] = 1u << (num_temporal_layers - tid - 1);
    config.min_quantizers[tid] = config.min_quantizer;
    config.max_quantizers[tid] = config.max_quantizer;
  }
  if ((codec == VideoCodec::kH264 || codec == VideoCodec::kHEVC) &&
      num_temporal_layers > 1 &&
      base::FeatureList::IsEnabled(kMediaFoundationSWBRCUseFixedDeltaQP)) {
    // `fixed_delta_qp` is used by H.264 and H.265 SW BRCs only in temporal
    // scalability video mode.
    config.fixed_delta_qp = kMediaFoundationSWBRCFixedDeltaQPValue.Get();
  }
  return config;
}

bool IsMatchingDevice(CHROME_LUID desired_luid, ID3D11Device* device) {
  ComDXGIDevice dxgi_device;
  if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgi_device)))) {
    return false;
  }
  ComDXGIAdapter dxgi_adapter;
  if (FAILED(dxgi_device->GetAdapter(&dxgi_adapter))) {
    return false;
  }
  DXGI_ADAPTER_DESC adapter_desc{};
  if (FAILED(dxgi_adapter->GetDesc(&adapter_desc))) {
    return false;
  }

  if (adapter_desc.AdapterLuid.HighPart == desired_luid.HighPart &&
      adapter_desc.AdapterLuid.LowPart == desired_luid.LowPart) {
    return true;
  }

  return false;
}

bool IsOdd(int value) {
  return (value & 1) != 0;
}

}  // namespace

struct MediaFoundationVideoEncodeAccelerator::PendingInput {
  PendingInput() = default;
  ~PendingInput() = default;
  PendingInput(const PendingInput&) = default;
  // If true, output bits should be discarded and the rate control object
  // shouldn't be let known about the encode.
  bool discard_output = false;
  EncodeOptions options;
  base::TimeDelta timestamp;
  gfx::ColorSpace color_space;
  ComMFSample input_sample;
  bool resolving_shared_image = false;
  gpu::Mailbox shared_image_token;
};

class MediaFoundationVideoEncodeAccelerator::EncodeOutput {
 public:
  EncodeOutput(uint32_t size, const BitstreamBufferMetadata& md)
      : metadata(md), data_(base::HeapArray<uint8_t>::Uninit(size)) {}

  EncodeOutput(const EncodeOutput&) = delete;
  EncodeOutput& operator=(const EncodeOutput&) = delete;

  base::span<uint8_t> as_span() { return data_.as_span(); }

  BitstreamBufferMetadata metadata;

 private:
  base::HeapArray<uint8_t> data_;
};

struct MediaFoundationVideoEncodeAccelerator::BitstreamBufferRef {
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

MediaFoundationVideoEncodeAccelerator::MediaFoundationVideoEncodeAccelerator(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    CHROME_LUID luid)
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      luid_(luid),
      gpu_preferences_(gpu_preferences),
      workarounds_(gpu_workarounds) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
  bitrate_allocation_.SetBitrate(0, 0, kDefaultTargetBitrate);
}

MediaFoundationVideoEncodeAccelerator::
    ~MediaFoundationVideoEncodeAccelerator() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(async_callback_ref_.IsOne());
}

VideoEncodeAccelerator::SupportedProfiles
MediaFoundationVideoEncodeAccelerator::GetSupportedProfiles() {
  TRACE_EVENT0("gpu,startup",
               "MediaFoundationVideoEncodeAccelerator::GetSupportedProfiles");
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  MediaFoundationVideoEncoderSharedState* mf_shared_state =
      MediaFoundationVideoEncoderSharedState::GetInstance(workarounds_);

  return mf_shared_state->GetSupportedProfiles();
}

EncoderStatus MediaFoundationVideoEncodeAccelerator::Initialize(
    const Config& config,
    Client* client,
    std::unique_ptr<MediaLog> media_log) {
  DVLOG(3) << __func__ << ": " << config.AsHumanReadableString();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  media_log_ = std::move(media_log);

  bool is_supported_format = false;
  if (base::FeatureList::IsEnabled(kMediaFoundationD3DVideoProcessing)) {
    is_supported_format =
        std::ranges::find(kSupportedPixelFormatsD3DVideoProcessing,
                          config.input_format) !=
        kSupportedPixelFormatsD3DVideoProcessing.end();
  } else {
    is_supported_format =
        std::ranges::find(kSupportedPixelFormats, config.input_format) !=
        kSupportedPixelFormats.end();
  }

  if (!is_supported_format) {
    MEDIA_LOG(ERROR, media_log_)
        << "Input format not supported= "
        << VideoPixelFormatToString(config.input_format);
    return {EncoderStatus::Codes::kEncoderInitializationError};
  }

  if (config.output_profile >= H264PROFILE_MIN &&
      config.output_profile <= H264PROFILE_MAX) {
    if (GetH264VProfile(config.output_profile, config.is_constrained_h264) ==
        eAVEncH264VProfile_unknown) {
      MEDIA_LOG(ERROR, media_log_)
          << "Output profile not supported = " << config.output_profile;
      return {EncoderStatus::Codes::kEncoderInitializationError};
    }
    codec_ = VideoCodec::kH264;
  } else if (config.output_profile >= VP9PROFILE_MIN &&
             config.output_profile <= VP9PROFILE_MAX) {
    if (GetVP9VProfile(config.output_profile) == eAVEncVP9VProfile_unknown) {
      MEDIA_LOG(ERROR, media_log_)
          << "Output profile not supported = " << config.output_profile;
      return {EncoderStatus::Codes::kEncoderInitializationError};
    }
    codec_ = VideoCodec::kVP9;
  } else if (config.output_profile == AV1PROFILE_PROFILE_MAIN) {
    codec_ = VideoCodec::kAV1;
  } else if (config.output_profile == HEVCPROFILE_MAIN) {
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    if (base::FeatureList::IsEnabled(kPlatformHEVCEncoderSupport)) {
      codec_ = VideoCodec::kHEVC;
    }
#endif
  }
  profile_ = config.output_profile;
  content_type_ = config.content_type;

  if (codec_ == VideoCodec::kUnknown) {
    MEDIA_LOG(ERROR, media_log_)
        << "Output profile not supported = " << config.output_profile;
    return {EncoderStatus::Codes::kEncoderInitializationError};
  }

  if (config.HasSpatialLayer()) {
    MEDIA_LOG(ERROR, media_log_) << "MediaFoundation does not support "
                                    "spatial layer encoding.";
    return {EncoderStatus::Codes::kEncoderInitializationError};
  }
  client_ = client;

  input_format_ = config.input_format;
  input_visible_size_ = config.input_visible_size;
  if (config.framerate > 0) {
    frame_rate_ = config.framerate;
  } else {
    frame_rate_ = kDefaultFrameRateNumerator / kDefaultFrameRateDenominator;
  }
  bitrate_allocation_ = AllocateBitrateForDefaultEncoding(config);

  bitstream_buffer_size_ =
      EstimateBitstreamBufferSize(bitrate_allocation_.GetSumBitrate(),
                                  frame_rate_, config.input_visible_size);
  gop_length_ = config.gop_length.value_or(kDefaultGOPLength);
  low_latency_mode_ = config.require_low_delay;

  if (config.HasTemporalLayer())
    num_temporal_layers_ = config.spatial_layers.front().num_of_temporal_layers;

  input_since_keyframe_count_ = 0;
  zero_layer_counter_ = 0;
  // Init bitream parser in the case temporal scalability encoding.
  svc_parser_ = std::make_unique<TemporalScalabilityIdExtractor>(
      codec_, num_temporal_layers_);

  if (state_ == kAcquiringCommandBuffer) {
    SetState(kInitializingWithCommandBuffer);
  } else {
    SetState(kInitializing);
  }

  std::vector<ComPtr<IMFActivate>> activates =
      EnumerateHardwareEncoders(codec_);

  if (activates.empty()) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderInitializationError,
                       "Failed finding a hardware encoder MFT"});
    return {EncoderStatus::Codes::kEncoderInitializationError};
  }

  bool activated = ActivateAsyncEncoder(activates, config.is_constrained_h264);
  activates.clear();

  if (!activated) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderInitializationError,
                       "Failed activating an async hardware encoder MFT"});
    return {EncoderStatus::Codes::kEncoderInitializationError};
  }

  // Set the SW implementation of the rate controller. Do nothing if SW RC is
  // not supported.
  SetSWRateControl();

  if (!SetEncoderModes()) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderInitializationError,
                       "Failed to set encoder modes"});
    return {EncoderStatus::Codes::kEncoderInitializationError};
  }

  if (!InitializeInputOutputParameters(config.output_profile,
                                       config.is_constrained_h264)) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderInitializationError,
                       "Failed to set input/output param."});
    return {EncoderStatus::Codes::kEncoderInitializationError};
  }

  // Get the max framerate and max/min resolutions of the given codec.
  //
  // NOTE:
  // We first attempt to retrieve max framerate and max/min resolutions from
  // cached data. If there is a cache miss, that implies the current encoder is
  // not on the same adapter as that used by "GetSupportedProfiles". If so we
  // use the actual encoder to retrieve max framerate and max/min resolutions.
  // The property `MF_VIDEO_MAX_MB_PER_SEC`, which we use to calculate the
  // resolution, is a static value stored in the encoder provided by the GPU
  // driver. It doesn't change regardless of the parameters we set for
  // `SetOutputType()` except for `MF_MT_MAJOR_TYPE` and `MF_MT_SUBTYPE`. So, as
  // long as the actual encoder remains the same, the result should be
  // unchanged. If the actual encoder can't handle the incoming resolution, we
  // can simply reject it without hesitation.
  if (base::FeatureList::IsEnabled(kExpandMediaFoundationEncodingResolutions)) {
    size_t activate_hash = GetMFTGuidHash(activate_.Get());
    MediaFoundationVideoEncoderSharedState* shared_state =
        MediaFoundationVideoEncoderSharedState::GetInstance(workarounds_);
    DCHECK(shared_state);
    const auto& max_framerate_and_resolutions =
        shared_state->GetMaxFramerateAndResolutions(activate_hash);
    const auto& min_resolution = shared_state->GetMinResolution(activate_hash);
    max_framerate_and_resolutions_ =
        !max_framerate_and_resolutions.empty()
            ? max_framerate_and_resolutions
            : GetMaxFramerateAndResolutionsFromMFT(
                  codec_, encoder_.Get(), /*allow_set_output_type=*/false);
    min_resolution_ = !min_resolution.IsEmpty()
                          ? min_resolution
                          : GetMinResolution(codec_, vendor_);
  } else {
    max_framerate_and_resolutions_ = {kDefaultMaxFramerateAndResolution};
    min_resolution_ = kDefaultMinResolution;
  }
  // Ideally, we should check size before `InitializeInputOutputParameters()`
  // because it sets `MF_MT_FRAME_SIZE` when `SetOutputType()` is called.
  // However, since we can only retrieve `MF_VIDEO_MAX_MB_PER_SEC` after calling
  // `SetOutputType()`, we have to check the frame size at this point.
  if (!IsFrameSizeAllowed(config.input_visible_size)) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderUnsupportedConfig,
                       "Unsupported frame size"});
    return {EncoderStatus::Codes::kEncoderInitializationError};
  }

  for (auto& [framerate, resolution] : max_framerate_and_resolutions_) {
    // TODO(crbug.com/382015342): Add implementation for checking bitrate
    // limits.
    encoder_info_.resolution_rate_limits.emplace_back(
        resolution, /*min_start_bitrate_bps=*/0, /*min_bitrate_bps=*/0,
        /*max_bitrate_bps=*/0, /*max_framerate_numerator=*/framerate,
        /*max_framerate_denominator=*/1);

    resolution.Transpose();
    encoder_info_.resolution_rate_limits.emplace_back(
        resolution,
        /*min_start_bitrate_bps=*/0, /*min_bitrate_bps=*/0,
        /*max_bitrate_bps=*/0, /*max_framerate_numerator=*/framerate,
        /*max_framerate_denominator=*/1);
    resolution.Transpose();
  }

  encoder_info_.implementation_name = "MediaFoundationVideoEncodeAccelerator";
  // Currently, MFVEA does not support odd resolution well. The implementation
  // here reports alignment of 2 in the EncoderInfo, together with simulcast
  // layers applied.
  // See https://crbug.com/1275453 for more details.
  encoder_info_.requested_resolution_alignment = 2;
  encoder_info_.apply_alignment_to_all_simulcast_layers = true;
  encoder_info_.has_trusted_rate_controller = false;
  if (codec_ == VideoCodec::kHEVC && vendor_ == DriverVendor::kIntel) {
    // On Intel HEVC we trust the rate controller based on manual testing and
    // because trusting it produces better results than not trusting it when
    // track frame rate suddenly drops, this avoids encoder FPS dropping even
    // more than the track FPS dropped, see https://crbug.com/402910373. This
    // risks overshooting but that seems like less of a concern on Intel.
    encoder_info_.has_trusted_rate_controller = true;
  }
  DCHECK(encoder_info_.is_hardware_accelerated);
  DCHECK(encoder_info_.supports_native_handle);
  DCHECK(encoder_info_.reports_average_qp);
  DCHECK(!encoder_info_.supports_simulcast);
  if (config.HasSpatialLayer() || config.HasTemporalLayer()) {
    DCHECK(!config.spatial_layers.empty());
    for (size_t i = 0; i < config.spatial_layers.size(); ++i) {
      encoder_info_.fps_allocation[i] =
          GetFpsAllocation(config.spatial_layers[i].num_of_temporal_layers);
    }
  } else {
    constexpr uint8_t kFullFramerate = 255;
    encoder_info_.fps_allocation[0] = {kFullFramerate};
  }

  encoder_info_.supports_frame_size_change =
      !workarounds_.disable_media_foundation_frame_size_change;

  if (state_ == kInitializing) {
    if (!InitializeMFT(nullptr)) {
      return {EncoderStatus::Codes::kEncoderInitializationError};
    }
  }

  // Notify encoder info change to client after initialization succeeded.
  client_->NotifyEncoderInfoChange(encoder_info_);

  return {EncoderStatus::Codes::kOk};
}

bool MediaFoundationVideoEncodeAccelerator::InitializeMFT(
    ID3D11Device* shared_device) {
  HRESULT hr;
  if (IsMediaFoundationD3D11VideoCaptureEnabled()) {
    MEDIA_LOG(INFO, media_log_)
        << "Preferred DXGI device " << luid_.HighPart << ":" << luid_.LowPart;
    if (shared_device) {
      MEDIA_LOG(INFO, media_log_) << "Using shared d3d device";
      dxgi_device_manager_ = DXGIDeviceManager::Create(luid_, shared_device);
    } else {
      dxgi_device_manager_ = DXGIDeviceManager::Create(luid_);
    }

    if (!dxgi_device_manager_) {
      NotifyErrorStatus({EncoderStatus::Codes::kEncoderInitializationError,
                         "Failed to create DXGIDeviceManager"});
      return false;
    }

    LUID mft_luid{0, 0};
    UINT32 out_size = 0;
    activate_->GetBlob(MFT_ENUM_ADAPTER_LUID,
                       reinterpret_cast<BYTE*>(&mft_luid), sizeof(LUID),
                       &out_size);

    hr = E_FAIL;
    if (out_size == sizeof(LUID) && mft_luid.HighPart == luid_.HighPart &&
        mft_luid.LowPart == luid_.LowPart) {
      // Only try to set the device manager for MFTs on the correct adapter.
      // Don't rely on MFT rejecting the device manager.
      auto mf_dxgi_device_manager =
          dxgi_device_manager_->GetMFDXGIDeviceManager();
      hr = encoder_->ProcessMessage(
          MFT_MESSAGE_SET_D3D_MANAGER,
          reinterpret_cast<ULONG_PTR>(mf_dxgi_device_manager.Get()));
    }
    // Can't use D3D11 decoding if HMFT is on a wrong LUID or rejects
    // setting a DXGI device manager.
    if (FAILED(hr)) {
      dxgi_resource_mapping_required_ = true;
      MEDIA_LOG(INFO, media_log_)
          << "Couldn't set DXGIDeviceManager, fallback to non-D3D11 encoding";
    }
  }

  hr = encoder_->QueryInterface(IID_PPV_ARGS(&event_generator_));
  if (FAILED(hr)) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderInitializationError,
                       "Couldn't get event generator: " + PrintHr(hr)});
    return false;
  }

  event_generator_->BeginGetEvent(this, nullptr);

  // Start the asynchronous processing model
  hr = encoder_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
  if (FAILED(hr)) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kEncoderInitializationError,
         "Couldn't set ProcessMessage MFT_MESSAGE_COMMAND_FLUSH: " +
             PrintHr(hr)});
    return false;
  }
  hr = encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
  if (FAILED(hr)) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kEncoderInitializationError,
         "Couldn't set ProcessMessage MFT_MESSAGE_NOTIFY_BEGIN_STREAMING: " +
             PrintHr(hr)});
    return false;
  }
  hr = encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
  if (FAILED(hr)) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kEncoderInitializationError,
         "Couldn't set ProcessMessage MFT_MESSAGE_NOTIFY_START_OF_STREAM: " +
             PrintHr(hr)});
    return false;
  }
  encoder_needs_input_counter_ = 0;

  if (!base::FeatureList::IsEnabled(kMediaFoundationD3DVideoProcessing) ||
      input_format_ == PIXEL_FORMAT_NV12 ||
      input_format_ == PIXEL_FORMAT_I420) {
    return true;
  }

  if (!InitMFVideoProcessor()) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderInitializationError,
                       "Couldn't initialize MF video processor for color "
                       "format conversion"});
    return false;
  }

  MEDIA_LOG(INFO, media_log_)
      << "Using video processor to convert from " << input_format_
      << " to encoder accepted PIXEL_FORMAT_NV12";

  return true;
}

void MediaFoundationVideoEncodeAccelerator::Encode(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe) {
  Encode(std::move(frame), EncodeOptions(force_keyframe));
}

void MediaFoundationVideoEncodeAccelerator::Encode(
    scoped_refptr<VideoFrame> frame,
    const EncodeOptions& options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Avoid corruption triggered by consecutive key frames on Intel drivers.
  // See also https://crbug.com/40069818 and https://crbug.com/378681055.
  if (codec_ == VideoCodec::kVP9 &&
      last_frame_was_keyframe_request_ && options.key_frame) {
    // Force a fake frame in between two key frames that come in a row. The
    // MFVEA will discard the output of this frame, and the client will never
    // see any side effects, but it helps working around crbug.com/1473665.
    EncodeOptions discard_options(/*key_frame=*/false);
    EncodeInternal(frame, discard_options, /*discard_output=*/true);
  }

  bool force_key_frame =
      (input_since_keyframe_count_ + pending_input_queue_.size()) %
          gop_length_ ==
      0;

  bool discard_high_layer_frames =
      (((codec_ == VideoCodec::kVP9 || codec_ == VideoCodec::kAV1) &&
        vendor_ == DriverVendor::kIntel) ||
       (codec_ == VideoCodec::kH264 && (vendor_ == DriverVendor::kIntel ||
                                        vendor_ == DriverVendor::kNvidia))) &&
      IsTemporalScalabilityCoding() && (options.key_frame || force_key_frame);

  if (discard_high_layer_frames) {
    // Currently, Intel and NVIDIA drivers only allow apps to request keyframe
    // on base layer(T0) when encoding at L1T2/L1T3, any keyframe requests on
    // T1/T2 layer will be ignored by driver and not return a keyframe. For
    // VP9, AV1 and H.264, we expect when keyframe is requested, encoder will
    // reset the temporal layer state and produce a keyframe, to work around
    // this issue, MFVEA will add input and internally discard output until
    // driver transition to T0 layer.

    uint32_t distance_to_base_layer = GetDistanceToNextTemporalBaseLayer(
        input_since_keyframe_count_ + pending_input_queue_.size(),
        num_temporal_layers_);
    for (uint32_t i = 0; i < distance_to_base_layer; ++i) {
      EncodeOptions discard_options(/*key_frame=*/false);
      EncodeInternal(frame, discard_options, /*discard_output=*/true);
    }
  }

  EncodeInternal(std::move(frame), options, /*discard_output=*/false);
  last_frame_was_keyframe_request_ = options.key_frame;
}

void MediaFoundationVideoEncodeAccelerator::QueueInput(
    scoped_refptr<media::VideoFrame> frame,
    const VideoEncoder::EncodeOptions& options,
    bool discard_output) {
  PendingInput result;
  auto hr = MFCreateSample(&result.input_sample);
  if (FAILED(hr)) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderInitializationError,
                       "Failed to create sample"});
    return;
  }
  result.timestamp = frame->timestamp();
  result.color_space = frame->ColorSpace();
  result.options = options;
  result.discard_output = discard_output;

  if (!frame->VideoFrame::HasMappableGpuBuffer() && frame->HasSharedImage() &&
      command_buffer_helper_) {
    result.resolving_shared_image = true;
    result.shared_image_token = frame->shared_image()->mailbox();
    pending_input_queue_.push_back(result);
    auto d3d_device = dxgi_device_manager_->GetDevice();
    if (!d3d_device) {
      NotifyErrorStatus({EncoderStatus::Codes::kSystemAPICallError,
                         "Failed to get D3D device manager"});
      return;
    }

    gpu_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &GenerateSampleFromSharedImageVideoFrame, frame,
            std::move(d3d_device), command_buffer_helper_,
            base::BindPostTask(
                task_runner_,
                base::BindOnce(&MediaFoundationVideoEncodeAccelerator::
                                   OnSharedImageSampleAvailable,
                               weak_ptr_))));
    return;
  }

  hr = PopulateInputSampleBuffer(result, std::move(frame));
  if (FAILED(hr)) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderFailedEncode,
                       "Failed to populate input sample buffer"});
    return;
  }

  pending_input_queue_.push_back(result);
}

void MediaFoundationVideoEncodeAccelerator::EncodeInternal(
    scoped_refptr<VideoFrame> frame,
    const EncodeOptions& options,
    bool discard_output) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (state_) {
    case kEncoding: {
      QueueInput(std::move(frame), options, discard_output);
      // Check the status of METransformNeedInput counter, only feed input when
      // MFT is ready.
      if (encoder_needs_input_counter_ > 0) {
        FeedInputs();
      }
      break;
    }
    case kInitializingWithCommandBuffer:
      [[fallthrough]];
    case kWaitingForCommandBuffer:
      [[fallthrough]];
    case kInitializing: {
      QueueInput(std::move(frame), options, discard_output);
      break;
    }
    default:
      NotifyErrorStatus({EncoderStatus::Codes::kEncoderFailedEncode,
                         "Unexpected encoder state"});
      DVLOG(3) << "Abandon input frame for video encoder."
               << " State: " << static_cast<int>(state_);
  }
}

void MediaFoundationVideoEncodeAccelerator::UseOutputBitstreamBuffer(
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

  // After mapping, |region| is no longer necessary and it can be destroyed.
  // |mapping| will keep the shared memory region open.
  auto region = buffer.TakeRegion();
  auto mapping = region.Map();
  if (!mapping.IsValid()) {
    NotifyErrorStatus({EncoderStatus::Codes::kSystemAPICallError,
                       "Failed mapping shared memory"});
    return;
  }
  auto buffer_ref = std::make_unique<BitstreamBufferRef>(
      buffer.id(), std::move(mapping), buffer.size());

  if (encoder_output_queue_.empty()) {
    bitstream_buffer_queue_.push_back(std::move(buffer_ref));
    return;
  }
  auto encode_output = std::move(encoder_output_queue_.front());
  encoder_output_queue_.pop_front();

  if (buffer.size() < encode_output->as_span().size()) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kInvalidOutputBuffer,
         "Encoder output is too large: " + base::NumberToString(buffer.size()) +
             " vs. " + base::NumberToString(encode_output->as_span().size())});
    return;
  }
  buffer_ref->mapping.GetMemoryAsSpan<uint8_t>().copy_prefix_from(
      encode_output->as_span());

  client_->BitstreamBufferReady(buffer_ref->id, encode_output->metadata);
  if (encoder_output_queue_.empty() && state_ == kPostFlushing) {
    // We were waiting for all the outputs to be consumed by the client.
    // Now once it's happened, we can signal the Flush() has finished
    // and continue encoding.
    SetState(kEncoding);
    std::move(flush_callback_).Run(true);
  }
}

void MediaFoundationVideoEncodeAccelerator::RequestEncodingParametersChange(
    const Bitrate& bitrate,
    uint32_t framerate,
    const std::optional<gfx::Size>& size) {
  DVLOG(3) << __func__ << ": bitrate=" << bitrate.ToString()
           << ": framerate=" << framerate
           << ": size=" << (size.has_value() ? size->ToString() : "nullopt");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VideoBitrateAllocation allocation(bitrate.mode());
  switch (bitrate.mode()) {
    case Bitrate::Mode::kVariable:
      allocation.SetBitrate(0, 0, bitrate.target_bps());
      allocation.SetPeakBps(bitrate.peak_bps());
      break;
    case Bitrate::Mode::kConstant:
      allocation.SetBitrate(0, 0, bitrate.target_bps());
      break;
    case Bitrate::Mode::kExternal:
      break;
  }

  RequestEncodingParametersChange(allocation, framerate, size);
}

void MediaFoundationVideoEncodeAccelerator::RequestEncodingParametersChange(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate,
    const std::optional<gfx::Size>& size) {
  DVLOG(3) << __func__ << ": bitrate=" << bitrate_allocation.GetSumBps()
           << ": framerate=" << framerate
           << ": size=" << (size.has_value() ? size->ToString() : "nullopt");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(imf_output_media_type_);
  DCHECK(imf_input_media_type_);
  DCHECK(encoder_);

  // RTC will pass a very small bitrate(1 bps) to the encoder when its bitrate
  // allocator assigns zero bitrate as part of initial setup. Ignore the request
  // to avoid CHECK failures on those BRC creations.
  if (bitrate_allocation.GetSumBps() <= 1) {
    DLOG(WARNING) << "Ignoring bitrate allocation request with 1 bps or less.";
    return;
  }

  if (bitrate_allocation.GetMode() != bitrate_allocation_.GetMode()) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderUnsupportedConfig,
                       "Can't change bitrate mode after Initialize()"});
    return;
  }

  if (framerate == 0) {
    framerate = kDefaultFrameRateNumerator / kDefaultFrameRateDenominator;
  }

  if (framerate == frame_rate_ && bitrate_allocation == bitrate_allocation_ &&
      !size.has_value()) {
    return;
  }

  bitrate_allocation_ = bitrate_allocation;
  frame_rate_ = framerate;
  // For SW BRC we don't reconfigure the encoder.
  if (rate_ctrl_) {
    rate_ctrl_->UpdateRateControl(CreateRateControllerConfig(
        bitrate_allocation_, size.value_or(input_visible_size_), frame_rate_,
        num_temporal_layers_, codec_, content_type_));
  } else {
    VARIANT var;
    var.vt = VT_UI4;
    HRESULT hr;
    switch (bitrate_allocation_.GetMode()) {
      case Bitrate::Mode::kVariable:
        var.ulVal = AdjustBitrateToFrameRate(bitrate_allocation_.GetPeakBps(),
                                             configured_frame_rate_, framerate);
        DVLOG(3) << "bitrate_allocation_.GetPeakBps() is "
                 << bitrate_allocation_.GetPeakBps();
        DVLOG(3) << "configured_frame_rate_ is " << configured_frame_rate_;
        DVLOG(3) << "framerate is " << framerate;
        DVLOG(3) << "Setting AVEncCommonMaxBitRate to " << var.ulVal;
        hr = codec_api_->SetValue(&CODECAPI_AVEncCommonMaxBitRate, &var);
        if (FAILED(hr)) {
          NotifyErrorStatus({EncoderStatus::Codes::kSystemAPICallError,
                             "Couldn't set max bitrate" + PrintHr(hr)});
          return;
        }
        [[fallthrough]];
      case Bitrate::Mode::kConstant:
        var.ulVal = AdjustBitrateToFrameRate(bitrate_allocation_.GetSumBps(),
                                             configured_frame_rate_, framerate);
        DVLOG(3) << "bitrate_allocation_.GetSumBps() is "
                 << bitrate_allocation_.GetSumBps();
        DVLOG(3) << "configured_frame_rate_ is " << configured_frame_rate_;
        DVLOG(3) << "framerate is " << framerate;
        DVLOG(3) << "Setting CODECAPI_AVEncCommonMeanBitRate to " << var.ulVal;
        hr = codec_api_->SetValue(&CODECAPI_AVEncCommonMeanBitRate, &var);
        if (FAILED(hr)) {
          NotifyErrorStatus({EncoderStatus::Codes::kSystemAPICallError,
                             "Couldn't set mean bitrate" + PrintHr(hr)});
          return;
        }
        break;
      case Bitrate::Mode::kExternal:
        DVLOG(3)
            << "RequestEncodingParametersChange for Bitrate::Mode::kExternal";
        break;
    }
  }

  if (size.has_value()) {
    UpdateFrameSize(size.value());
  }
}

bool MediaFoundationVideoEncodeAccelerator::IsFrameSizeAllowed(gfx::Size size) {
  // It's possible `max_framerate_and_resolutions_` is empty when we
  // failed to retrieve `MF_VIDEO_MAX_MB_PER_SEC`.
  DCHECK(!min_resolution_.IsEmpty());

  if (IsOdd(size.width()) || IsOdd(size.height())) {
    MEDIA_LOG(ERROR, media_log_) << "MediaFoundation does not support "
                                    "encoding frame of odd width/height well.";
    return false;
  }

  for (auto& [frame_rate, resolution] : max_framerate_and_resolutions_) {
    // TODO(crbug.com/365813271): Add framerate check once we can make sure
    // WebRTC check framerate before calling `RequestEncodingParametersChange`.
    if (size.width() >= min_resolution_.width() &&
        size.height() >= min_resolution_.height() &&
        size.width() <= resolution.width() &&
        size.height() <= resolution.height()) {
      return true;
    }

    size.Transpose();
    // Portrait profile only transpose max resolution.
    if (size.height() >= min_resolution_.width() &&
        size.width() >= min_resolution_.height() &&
        size.width() <= resolution.width() &&
        size.height() <= resolution.height()) {
      return true;
    }

    size.Transpose();
  }

  return false;
}

void MediaFoundationVideoEncodeAccelerator::UpdateFrameSize(
    const gfx::Size& frame_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(imf_output_media_type_);
  DCHECK(imf_input_media_type_);
  DCHECK(activate_);
  DCHECK(encoder_);
  DCHECK_NE(input_visible_size_, frame_size);
  DCHECK(pending_input_queue_.empty());

  if (!IsFrameSizeAllowed(frame_size)) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderUnsupportedConfig,
                       "Unsupported frame size"});
    return;
  }
  input_visible_size_ = frame_size;

  HRESULT hr = S_OK;
  // As this method is expected to be called after Flush(), it's safe to send
  // MFT_MESSAGE_COMMAND_FLUSH here. Without MFT_MESSAGE_COMMAND_FLUSH, MFT may
  // either:
  // - report 0x80004005 (Unspecified error) when encode the first frame after
  //   resolution change on Intel platform.
  // - report issues with SPS/PPS in the NALU analyzer phase of the tests on
  //   Qualcomm platform.
  if (vendor_ == DriverVendor::kIntel || vendor_ == DriverVendor::kQualcomm) {
    hr = encoder_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
    if (FAILED(hr)) {
      NotifyErrorStatus(
          {EncoderStatus::Codes::kSystemAPICallError,
           "Couldn't set ProcessMessage MFT_MESSAGE_COMMAND_FLUSH: " +
               PrintHr(hr)});
      return;
    }
  }
  // Reset the need input counter since MFT was notified to end stream.
  encoder_needs_input_counter_ = 0;
  hr = encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
  if (FAILED(hr)) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kSystemAPICallError,
         "Couldn't set ProcessMessage MFT_MESSAGE_NOTIFY_END_OF_STREAM: " +
             PrintHr(hr)});
    return;
  }
  hr = encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_END_STREAMING, 0);
  if (FAILED(hr)) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kSystemAPICallError,
         "Couldn't set ProcessMessage MFT_MESSAGE_NOTIFY_END_STREAMING: " +
             PrintHr(hr)});
    return;
  }
  hr = encoder_->SetInputType(input_stream_id_, nullptr, 0);
  if (FAILED(hr)) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kSystemAPICallError,
         "Couldn't set input stream type to nullptr: " + PrintHr(hr)});
    return;
  }
  hr = encoder_->SetOutputType(output_stream_id_, nullptr, 0);
  if (FAILED(hr)) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kSystemAPICallError,
         "Couldn't set output stream type to nullptr: " + PrintHr(hr)});
    return;
  }
  hr = MFSetAttributeSize(imf_output_media_type_.Get(), MF_MT_FRAME_SIZE,
                          input_visible_size_.width(),
                          input_visible_size_.height());
  if (FAILED(hr)) {
    NotifyErrorStatus({EncoderStatus::Codes::kSystemAPICallError,
                       "Couldn't set output frame size: " + PrintHr(hr)});
    return;
  }
  hr = encoder_->SetOutputType(output_stream_id_, imf_output_media_type_.Get(),
                               0);
  if (FAILED(hr)) {
    NotifyErrorStatus({EncoderStatus::Codes::kSystemAPICallError,
                       "Couldn't set output media type: " + PrintHr(hr)});
    return;
  }
  hr = MFSetAttributeSize(imf_input_media_type_.Get(), MF_MT_FRAME_SIZE,
                          input_visible_size_.width(),
                          input_visible_size_.height());
  if (FAILED(hr)) {
    NotifyErrorStatus({EncoderStatus::Codes::kSystemAPICallError,
                       "Couldn't set input frame size: " + PrintHr(hr)});
    return;
  }
  hr = encoder_->SetInputType(input_stream_id_, imf_input_media_type_.Get(), 0);
  if (FAILED(hr)) {
    NotifyErrorStatus({EncoderStatus::Codes::kSystemAPICallError,
                       "Couldn't set input media type: " + PrintHr(hr)});
    return;
  }
  hr = encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
  if (FAILED(hr)) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kSystemAPICallError,
         "Couldn't set ProcessMessage MFT_MESSAGE_NOTIFY_BEGIN_STREAMING: " +
             PrintHr(hr)});
    return;
  }
  hr = encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
  if (FAILED(hr)) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kSystemAPICallError,
         "Couldn't set ProcessMessage MFT_MESSAGE_NOTIFY_START_OF_STREAM: " +
             PrintHr(hr)});
    return;
  }

  bitstream_buffer_size_ = EstimateBitstreamBufferSize(
      bitrate_allocation_.GetSumBitrate(), frame_rate_, input_visible_size_);
  bitstream_buffer_queue_.clear();
  // Reset the input frame counter since MFT was notified to end the streaming
  // and restart with new frame size.
  input_since_keyframe_count_ = 0;
  client_->RequireBitstreamBuffers(kNumInputBuffers, input_visible_size_,
                                   bitstream_buffer_size_);

  if (mf_video_processor_) {
    hr = mf_video_processor_->UpdateOutputSize(input_visible_size_);
    if (FAILED(hr)) {
      NotifyErrorStatus(
          {EncoderStatus::Codes::kSystemAPICallError,
           "Couldn't update Video processor output size: " + PrintHr(hr)});
    }
  }
}

void MediaFoundationVideoEncodeAccelerator::Destroy() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (activate_) {
    activate_->ShutdownObject();
    activate_->Release();
  }
  delete this;
}

void MediaFoundationVideoEncodeAccelerator::DrainEncoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto hr = encoder_->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
  if (FAILED(hr)) {
    std::move(flush_callback_).Run(/*success=*/false);
    return;
  }
  SetState(kFlushing);
}

void MediaFoundationVideoEncodeAccelerator::Flush(
    FlushCallback flush_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(flush_callback);

  if (state_ != kEncoding || !encoder_) {
    DCHECK(false) << "Called Flush() with unexpected state."
                  << " State: " << static_cast<int>(state_);
    std::move(flush_callback).Run(/*success=*/false);
    return;
  }

  flush_callback_ = std::move(flush_callback);
  if (pending_input_queue_.empty()) {
    // There are no pending inputs we can just ask MF encoder to drain without
    // having to wait for any more METransformNeedInput requests.
    DrainEncoder();
  } else {
    // Otherwise METransformNeedInput will call DrainEncoder() when all the
    // inputs from `pending_input_queue_` were fed to the MF encoder.
    SetState(kPreFlushing);
  }
}

bool MediaFoundationVideoEncodeAccelerator::IsFlushSupported() {
  return true;
}

bool MediaFoundationVideoEncodeAccelerator::IsGpuFrameResizeSupported() {
  return true;
}

MediaFoundationVideoEncodeAccelerator::GetCommandBufferHelperResult::
    GetCommandBufferHelperResult() = default;
MediaFoundationVideoEncodeAccelerator::GetCommandBufferHelperResult::
    GetCommandBufferHelperResult(const MediaFoundationVideoEncodeAccelerator::
                                     GetCommandBufferHelperResult& other) =
        default;
MediaFoundationVideoEncodeAccelerator::GetCommandBufferHelperResult::
    ~GetCommandBufferHelperResult() = default;

MediaFoundationVideoEncodeAccelerator::GetCommandBufferHelperResult
GetCommandBufferHelperOnGpuThread(
    base::RepeatingCallback<scoped_refptr<CommandBufferHelper>()>
        get_command_buffer_helper_cb,
    CHROME_LUID luid,
    bool use_shared_device) {
  MediaFoundationVideoEncodeAccelerator::GetCommandBufferHelperResult result;
  result.command_buffer_helper = get_command_buffer_helper_cb.Run();
  scoped_refptr<gpu::SharedContextState> shared_context_state =
      result.command_buffer_helper->GetSharedImageStub()
          ->shared_context_state();

  if (use_shared_device && shared_context_state->GetD3D11Device()) {
    DCHECK(
        IsMatchingDevice(luid, shared_context_state->GetD3D11Device().Get()));
    result.shared_d3d_device = shared_context_state->GetD3D11Device();
    Microsoft::WRL::ComPtr<ID3D11Multithread> multi_threaded;
    HRESULT hr =
        result.shared_d3d_device->QueryInterface(IID_PPV_ARGS(&multi_threaded));
    CHECK(SUCCEEDED(hr));
    multi_threaded->SetMultithreadProtected(TRUE);
  }

  return result;
}

void MediaFoundationVideoEncodeAccelerator::SetCommandBufferHelperCB(
    base::RepeatingCallback<scoped_refptr<CommandBufferHelper>()>
        get_command_buffer_helper_cb,
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner) {
  if (!base::FeatureList::IsEnabled(kMediaFoundationSharedImageEncode)) {
    return;
  }

  // With GL/Ganesh, shared images will be copied out of GL textures into
  // a shared D3D texture created on MFVEA's own D3D device.  (See
  // GLTextureImageBacking::ProduceVideo).  It may be possible to
  // optimize out this copy by extracting the backing ANGLE texture
  // and synchronize access, but the copy works without invasive
  // work in ANGLE and is much better than readback.
  // With GraphiteDawn, MFVEA will use a shared D3D device and directly
  // access textures, with D3DImageBacking handling synchronization.
  SetState(kAcquiringCommandBuffer);
  gpu_task_runner_ = gpu_task_runner;
  gpu_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetCommandBufferHelperOnGpuThread,
                     get_command_buffer_helper_cb, luid_,
                     gpu_preferences_.gr_context_type ==
                         gpu::GrContextType::kGraphiteDawn),
      base::BindOnce(&MediaFoundationVideoEncodeAccelerator::
                         OnCommandBufferHelperAvailable,
                     weak_ptr_));
}

bool MediaFoundationVideoEncodeAccelerator::ActivateAsyncEncoder(
    std::vector<ComPtr<IMFActivate>>& activates,
    bool is_constrained_h264) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Try to create the encoder with priority according to merit value.
  HRESULT hr = E_FAIL;
  for (auto& activate : activates) {
    auto vendor = GetDriverVendor(activate.Get());

    // Skip NVIDIA GPU due to https://crbug.com/1088650 for constrained
    // baseline profile H.264 encoding, and go to the next instance according
    // to merit value.
    if (codec_ == VideoCodec::kH264 && is_constrained_h264 &&
        vendor == DriverVendor::kNvidia) {
      DLOG(WARNING) << "Skipped NVIDIA GPU due to https://crbug.com/1088650";
      continue;
    }

    if (num_temporal_layers_ >
        GetMaxTemporalLayerVendorLimit(vendor, codec_, workarounds_)) {
      DLOG(WARNING) << "Skipped GPUs due to not supporting temporal layer";
      continue;
    }

    DCHECK(!encoder_);
    DCHECK(!activate_);
    hr = activate->ActivateObject(IID_PPV_ARGS(&encoder_));
    if (encoder_.Get() != nullptr) {
      DCHECK(SUCCEEDED(hr));
      activate_ = activate;
      vendor_ = vendor;

      // Print the friendly name.
      base::win::ScopedCoMem<WCHAR> friendly_name;
      UINT32 name_length;
      activate_->GetAllocatedString(MFT_FRIENDLY_NAME_Attribute, &friendly_name,
                                    &name_length);
      DVLOG(3) << "Selected asynchronous hardware encoder's friendly name: "
               << friendly_name;
      // Encoder is successfully activated.
      break;
    } else {
      DCHECK(FAILED(hr));

      // The component that calls ActivateObject is
      // responsible for calling ShutdownObject,
      // https://docs.microsoft.com/en-us/windows/win32/api/mfobjects/nf-mfobjects-imfactivate-shutdownobject.
      activate->ShutdownObject();
    }
  }

  RETURN_ON_HR_FAILURE(hr, "Couldn't activate asynchronous hardware encoder",
                       false);
  RETURN_ON_FAILURE((encoder_.Get() != nullptr),
                    "No asynchronous hardware encoder instance created", false);

  ComMFAttributes all_attributes;
  hr = encoder_->GetAttributes(&all_attributes);
  if (SUCCEEDED(hr)) {
    // An asynchronous MFT must support dynamic format changes,
    // https://docs.microsoft.com/en-us/windows/win32/medfound/asynchronous-mfts#format-changes.
    UINT32 dynamic = FALSE;
    hr = all_attributes->GetUINT32(MFT_SUPPORT_DYNAMIC_FORMAT_CHANGE, &dynamic);
    if (!dynamic) {
      DLOG(ERROR) << "Couldn't support dynamic format change.";
      return false;
    }

    // Unlock the selected asynchronous MFTs,
    // https://docs.microsoft.com/en-us/windows/win32/medfound/asynchronous-mfts#unlocking-asynchronous-mfts.
    UINT32 async = FALSE;
    hr = all_attributes->GetUINT32(MF_TRANSFORM_ASYNC, &async);
    if (!async) {
      DLOG(ERROR) << "MFT encoder is not asynchronous.";
      return false;
    }

    hr = all_attributes->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, TRUE);
    RETURN_ON_HR_FAILURE(hr, "Couldn't unlock transform async", false);
  }

  return true;
}

bool MediaFoundationVideoEncodeAccelerator::InitializeInputOutputParameters(
    VideoCodecProfile output_profile,
    bool is_constrained_h264) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(encoder_);

  DWORD input_count = 0;
  DWORD output_count = 0;
  HRESULT hr = encoder_->GetStreamCount(&input_count, &output_count);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get stream count", false);
  if (input_count < 1 || output_count < 1) {
    DLOG(ERROR) << "Stream count too few: input " << input_count << ", output "
                << output_count;
    return false;
  }

  std::vector<DWORD> input_ids(input_count, 0);
  std::vector<DWORD> output_ids(output_count, 0);
  hr = encoder_->GetStreamIDs(input_count, input_ids.data(), output_count,
                              output_ids.data());
  if (hr == S_OK) {
    input_stream_id_ = input_ids[0];
    output_stream_id_ = output_ids[0];
  } else if (hr == E_NOTIMPL) {
    input_stream_id_ = 0;
    output_stream_id_ = 0;
  } else {
    DLOG(ERROR) << "Couldn't find stream ids from hardware encoder.";
    return false;
  }

  // Initialize output parameters.
  hr = MFCreateMediaType(&imf_output_media_type_);
  RETURN_ON_HR_FAILURE(hr, "Couldn't create output media type", false);
  hr = imf_output_media_type_->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set media type", false);
  hr = imf_output_media_type_->SetGUID(MF_MT_SUBTYPE,
                                       VideoCodecToMFSubtype(codec_));
  RETURN_ON_HR_FAILURE(hr, "Couldn't set video format", false);

  if (!rate_ctrl_) {
    UINT32 bitrate = AdjustBitrateToFrameRate(bitrate_allocation_.GetSumBps(),
                                              frame_rate_, frame_rate_);
    DVLOG(3) << "MF_MT_AVG_BITRATE is " << bitrate;
    // Setting MF_MT_AVG_BITRATE to zero will make some encoders upset
    if (bitrate > 0) {
      hr = imf_output_media_type_->SetUINT32(MF_MT_AVG_BITRATE, bitrate);
      RETURN_ON_HR_FAILURE(hr, "Couldn't set bitrate", false);
    }
  }
  configured_frame_rate_ = frame_rate_;

  hr = MFSetAttributeRatio(imf_output_media_type_.Get(), MF_MT_FRAME_RATE,
                           configured_frame_rate_, 1);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame rate", false);
  hr = MFSetAttributeSize(imf_output_media_type_.Get(), MF_MT_FRAME_SIZE,
                          input_visible_size_.width(),
                          input_visible_size_.height());
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame size", false);
  hr = imf_output_media_type_->SetUINT32(MF_MT_INTERLACE_MODE,
                                         MFVideoInterlace_Progressive);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set interlace mode", false);
  if (codec_ == VideoCodec::kH264) {
    hr = imf_output_media_type_->SetUINT32(
        MF_MT_MPEG2_PROFILE,
        GetH264VProfile(output_profile, is_constrained_h264));
  } else if (codec_ == VideoCodec::kVP9) {
    hr = imf_output_media_type_->SetUINT32(MF_MT_MPEG2_PROFILE,
                                           GetVP9VProfile(output_profile));
  } else if (codec_ == VideoCodec::kHEVC) {
    hr = imf_output_media_type_->SetUINT32(MF_MT_MPEG2_PROFILE,
                                           GetHEVCProfile(output_profile));
  }
  RETURN_ON_HR_FAILURE(hr, "Couldn't set codec profile", false);
  hr = encoder_->SetOutputType(output_stream_id_, imf_output_media_type_.Get(),
                               0);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set output media type", false);

  // Initialize input parameters.
  hr = MFCreateMediaType(&imf_input_media_type_);
  RETURN_ON_HR_FAILURE(hr, "Couldn't create input media type", false);
  hr = imf_input_media_type_->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set media type", false);
  hr = imf_input_media_type_->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set video format", false);
  DVLOG(3) << "MF_MT_FRAME_RATE is " << configured_frame_rate_;
  hr = MFSetAttributeRatio(imf_input_media_type_.Get(), MF_MT_FRAME_RATE,
                           configured_frame_rate_, 1);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame rate", false);
  DVLOG(3) << "MF_MT_FRAME_SIZE is " << input_visible_size_.width() << "x"
           << input_visible_size_.height();
  hr = MFSetAttributeSize(imf_input_media_type_.Get(), MF_MT_FRAME_SIZE,
                          input_visible_size_.width(),
                          input_visible_size_.height());
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame size", false);
  hr = imf_input_media_type_->SetUINT32(MF_MT_INTERLACE_MODE,
                                        MFVideoInterlace_Progressive);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set interlace mode", false);
  hr = encoder_->SetInputType(input_stream_id_, imf_input_media_type_.Get(), 0);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set input media type", false);

  return true;
}

void MediaFoundationVideoEncodeAccelerator::SetSWRateControl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Use SW BRC only in the case CBR encoding with number of temporal layers no
  // more than 3.
  if (bitrate_allocation_.GetMode() != Bitrate::Mode::kConstant ||
      !base::FeatureList::IsEnabled(kMediaFoundationUseSoftwareRateCtrl) ||
      num_temporal_layers_ > 3) {
    return;
  }

  // The following codecs support SW BRC: VP9, H264, HEVC, and AV1.
  VideoCodec kCodecsHaveSWBRC[] = {
      VideoCodec::kVP9,
      VideoCodec::kH264,
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
      VideoCodec::kHEVC,
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
      VideoCodec::kAV1,
  };
  if (!base::Contains(kCodecsHaveSWBRC, codec_)) {
    return;
  }

  // Qualcomm (and possibly other vendor) AV1 HMFT does not work with SW BRC.
  // More info: https://crbug.com/343757696
  if (codec_ == VideoCodec::kAV1 && vendor_ == DriverVendor::kQualcomm) {
    return;  // SW BRC and QCOM AV1 HMFT not ok
  }

  if (codec_ == VideoCodec::kH264) {
    // H264 SW BRC supports up to two temporal layers.
    if (num_temporal_layers_ > 2) {
      return;
    }

    if (vendor_ == DriverVendor::kQualcomm) {
      // Qualcomm H264 HMFT does not work with SW BRC.
      // More info: https://crbug.com/390581539
      return;
    }

    // Check feature flag for the camera source.
    if (content_type_ == VideoEncodeAccelerator::Config::ContentType::kCamera &&
        !base::FeatureList::IsEnabled(kMediaFoundationUseSWBRCForH264Camera)) {
      return;
    }

    // Check feature flag for the desktop source.
    if (content_type_ ==
            VideoEncodeAccelerator::Config::ContentType::kDisplay &&
        !base::FeatureList::IsEnabled(kMediaFoundationUseSWBRCForH264Desktop)) {
      return;
    }
  }

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  if (codec_ == VideoCodec::kHEVC) {
    // H265 SW BRC supports up to two temporal layers.
    if (num_temporal_layers_ > 2) {
      return;
    }

    if (vendor_ == DriverVendor::kQualcomm) {
      // Qualcomm H265 HMFT does not work with SW BRC.
      // More info: https://crbug.com/390581539
      return;
    }

    // Check feature flag.
    if ((vendor_ != DriverVendor::kIntel ||
         !workarounds_.disable_hevc_hmft_cbr_encoding) &&
        !base::FeatureList::IsEnabled(kMediaFoundationUseSWBRCForH265)) {
      return;
    }
  }
#endif

  VideoRateControlWrapper::RateControlConfig rate_config =
      CreateRateControllerConfig(bitrate_allocation_, input_visible_size_,
                                 frame_rate_, num_temporal_layers_, codec_,
                                 content_type_);
  if (codec_ == VideoCodec::kVP9) {
    rate_ctrl_ = VP9RateControl::Create(rate_config);
  } else if (codec_ == VideoCodec::kAV1) {
    rate_ctrl_ = AV1RateControl::Create(rate_config);
  } else if (codec_ == VideoCodec::kH264) {
    rate_ctrl_ = H264RateControl::Create(rate_config);
  } else if (codec_ == VideoCodec::kHEVC) {
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    // Reuse the H.264 rate controller for HEVC.
    rate_ctrl_ = H264RateControl::Create(rate_config);
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
  }
}

bool MediaFoundationVideoEncodeAccelerator::SetEncoderModes() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(encoder_);

  HRESULT hr = encoder_.As(&codec_api_);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get ICodecAPI", false);

  VARIANT var;
  var.vt = VT_UI4;
  switch (bitrate_allocation_.GetMode()) {
    case Bitrate::Mode::kConstant:
      if (rate_ctrl_) {
        DVLOG(3) << "SetEncoderModes() with Bitrate::Mode::kConstant and "
                    "rate_ctrl_, using eAVEncCommonRateControlMode_Quality";
        var.ulVal = eAVEncCommonRateControlMode_Quality;
      } else {
        DVLOG(3) << "SetEncoderModes() with Bitrate::Mode::kConstant and no "
                    "rate_ctrl_, using eAVEncCommonRateControlMode_CBR";
        var.ulVal = eAVEncCommonRateControlMode_CBR;
      }
      break;
    case Bitrate::Mode::kVariable: {
      DCHECK(!rate_ctrl_);
      DVLOG(3) << "SetEncoderModes() with Bitrate::Mode::kVariable, using "
                  "eAVEncCommonRateControlMode_PeakConstrainedVBR";
      var.ulVal = eAVEncCommonRateControlMode_PeakConstrainedVBR;
      break;
    }
    case Bitrate::Mode::kExternal:
      // Unsupported.
      DVLOG(3) << "SetEncoderModes() with Bitrate::Mode::kExternal, using "
                  "eAVEncCommonRateControlMode_Quality";
      var.ulVal = eAVEncCommonRateControlMode_Quality;
      break;
  }
  hr = codec_api_->SetValue(&CODECAPI_AVEncCommonRateControlMode, &var);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set CommonRateControlMode", false);

  // Intel drivers want the layer count to be set explicitly for H.264/HEVC,
  // even if it's one.
  const bool set_svc_layer_count =
      (num_temporal_layers_ > 1) ||
      (vendor_ == DriverVendor::kIntel &&
       (codec_ == VideoCodec::kH264 || codec_ == VideoCodec::kHEVC));
  if (set_svc_layer_count) {
    var.ulVal = num_temporal_layers_;
    DVLOG(3) << "Setting CODECAPI_AVEncVideoTemporalLayerCount to "
             << var.ulVal;
    hr = codec_api_->SetValue(&CODECAPI_AVEncVideoTemporalLayerCount, &var);
    RETURN_ON_HR_FAILURE(hr, "Couldn't set temporal layer count", false);

    // On Intel platform at L1T2, for some codecs(AV1 & HEVC), recent drivers
    // allow configuring the number of reference frames to 1, which will produce
    // bitstream that follows WebRTC SVC spec for L1T2. For L1T3, however,
    // driver does not allow reducing the number of reference frames to 1.
    if (vendor_ == DriverVendor::kIntel && num_temporal_layers_ == 2) {
      if (S_OK ==
          codec_api_->IsModifiable(&CODECAPI_AVEncVideoMaxNumRefFrame)) {
        var.ulVal = 1;
        DVLOG(3) << "Setting CODECAPI_AVEncVideoMaxNumRefFrame to "
                 << var.ulVal;
        hr = codec_api_->SetValue(&CODECAPI_AVEncVideoMaxNumRefFrame, &var);
        if (SUCCEEDED(hr)) {
          encoder_produces_svc_spec_compliant_bitstream_ = true;
        } else {
          // Failing to set number of reference frames is not fatal.
          DVLOG(3) << "Couldn't set CODECAPI_AVEncVideoMaxNumRefFrame to 1";
        }
      }
    }
  }

  if (!rate_ctrl_ &&
      bitrate_allocation_.GetMode() != Bitrate::Mode::kExternal) {
    var.ulVal = AdjustBitrateToFrameRate(bitrate_allocation_.GetSumBps(),
                                         configured_frame_rate_, frame_rate_);
    DVLOG(3) << "bitrate_allocation_.GetSumBps() is "
             << bitrate_allocation_.GetSumBps();
    DVLOG(3) << "configured_frame_rate_ is " << configured_frame_rate_;
    DVLOG(3) << "framerate is " << frame_rate_;
    DVLOG(3) << "Setting CODECAPI_AVEncCommonMeanBitRate to " << var.ulVal;
    hr = codec_api_->SetValue(&CODECAPI_AVEncCommonMeanBitRate, &var);
    RETURN_ON_HR_FAILURE(hr, "Couldn't set bitrate", false);
  }

  if (bitrate_allocation_.GetMode() == Bitrate::Mode::kVariable) {
    var.ulVal = AdjustBitrateToFrameRate(bitrate_allocation_.GetPeakBps(),
                                         configured_frame_rate_, frame_rate_);
    DVLOG(3) << "bitrate_allocation_.GetPeakBps() is "
             << bitrate_allocation_.GetPeakBps();
    DVLOG(3) << "configured_frame_rate_ is " << configured_frame_rate_;
    DVLOG(3) << "framerate is " << frame_rate_;
    DVLOG(3) << "Setting CODECAPI_AVEncCommonMaxBitRate to " << var.ulVal;
    hr = codec_api_->SetValue(&CODECAPI_AVEncCommonMaxBitRate, &var);
    RETURN_ON_HR_FAILURE(hr, "Couldn't set bitrate", false);
  }

  if (S_OK == codec_api_->IsModifiable(&CODECAPI_AVEncAdaptiveMode)) {
    var.ulVal = eAVEncAdaptiveMode_Resolution;
    DVLOG(3) << "Setting CODECAPI_AVEncAdaptiveMode to " << var.ulVal;
    hr = codec_api_->SetValue(&CODECAPI_AVEncAdaptiveMode, &var);
    RETURN_ON_HR_FAILURE(hr, "Couldn't set adaptive mode", false);
  }

  var.ulVal = gop_length_;
  DVLOG(3) << "Setting CODECAPI_AVEncMPVGOPSize to " << var.ulVal;
  hr = codec_api_->SetValue(&CODECAPI_AVEncMPVGOPSize, &var);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set keyframe interval", false);

  if (S_OK == codec_api_->IsModifiable(&CODECAPI_AVLowLatencyMode)) {
    var.vt = VT_BOOL;
    var.boolVal = low_latency_mode_ ? VARIANT_TRUE : VARIANT_FALSE;
    DVLOG(3) << "Setting CODECAPI_AVLowLatencyMode to " << var.boolVal;
    hr = codec_api_->SetValue(&CODECAPI_AVLowLatencyMode, &var);
    RETURN_ON_HR_FAILURE(hr, "Couldn't set low latency mode", false);
  }

  // For AV1 screen content encoding, configure scenario to enable AV1
  // SCC tools(palette mode, intra block copy, etc.) This will also turn
  // off CDEF on I-frame, and enable long term reference for screen contents.
  // For other codecs this may impact some encoding parameters as well.
  // TODO(crbugs.com/336592435): Set scenario info if we confirm it
  // works on other vendors, and possibly set eAVScenarioInfo_VideoConference
  // for camera streams if all drivers support it.
  if (S_OK == codec_api_->IsModifiable(&CODECAPI_AVScenarioInfo) &&
      vendor_ == DriverVendor::kIntel &&
      content_type_ == Config::ContentType::kDisplay) {
    var.vt = VT_UI4;
    var.ulVal = eAVScenarioInfo_DisplayRemoting;
    hr = codec_api_->SetValue(&CODECAPI_AVScenarioInfo, &var);
    RETURN_ON_HR_FAILURE(hr, "Couldn't set scenario info", false);
  }

  // For QCOM there are DCHECK issues with frame-dropping and timestamps due
  // to the AVScenarioInfo and b-frames, respectively.  Disable these, see
  // mfenc.c for similar logic.
  if (vendor_ == DriverVendor::kQualcomm) {
    var.vt = VT_UI4;
    // More info: https://crbug.com/343757695
    var.ulVal = eAVScenarioInfo_CameraRecord;
    hr = codec_api_->SetValue(&CODECAPI_AVScenarioInfo, &var);
    RETURN_ON_HR_FAILURE(hr, "Couldn't set scenario info", false);

    // More info: https://crbug.com/343748806
    var.ulVal = 0;
    hr = codec_api_->SetValue(&CODECAPI_AVEncMPVDefaultBPictureCount, &var);
    RETURN_ON_HR_FAILURE(hr, "Couldn't set bframe count", false);
  }

  return true;
}

void MediaFoundationVideoEncodeAccelerator::NotifyErrorStatus(
    EncoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!status.is_ok());
  CHECK(media_log_);
  SetState(kError);
  MEDIA_LOG(ERROR, media_log_) << status.message();
  DLOG(ERROR) << "Call NotifyErrorStatus(): code="
              << static_cast<int>(status.code())
              << ", message=" << status.message();
  CHECK(client_);
  client_->NotifyErrorStatus(std::move(status));
}

void MediaFoundationVideoEncodeAccelerator::FeedInputs() {
  if (pending_input_queue_.empty()) {
    return;
  }

  // There's no point in trying to feed more than one input here,
  // because MF encoder never accepts more than one input in a row.
  auto& next_input = pending_input_queue_.front();
  if (next_input.resolving_shared_image) {
    return;
  }

  HRESULT hr = ProcessInput(next_input);
  if (hr == MF_E_NOTACCEPTING) {
    return;
  }
  if (FAILED(hr)) {
    NotifyErrorStatus({EncoderStatus::Codes::kSystemAPICallError,
                       "Failed to encode pending frame: " + PrintHr(hr)});
    return;
  }
  encoder_needs_input_counter_--;
  pending_input_queue_.pop_front();
  input_since_keyframe_count_++;
}

HRESULT MediaFoundationVideoEncodeAccelerator::ProcessInput(
    const PendingInput& input) {
  DVLOG(3) << __func__;
  CHECK(input.input_sample);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(encoder_needs_input_counter_ > 0);
  TRACE_EVENT2("media", "MediaFoundationVideoEncodeAccelerator::ProcessInput",
               "timestamp", input.timestamp, "discard_output",
               input.discard_output);

  HRESULT hr = S_OK;
  if (has_not_accepted_sample_) {
    // Let's validate that prepared sample actually matches the metadata.
    const OutOfBandMetadata& metadata = sample_metadata_queue_.back();
    if (metadata.timestamp != input.timestamp) {
      LOG(ERROR) << "Prepared sample doesn't match metadata.";
      return E_FAIL;
    }
  } else {
    // Force key frame for the first frame in GOP.
    bool force_key_frame = input_since_keyframe_count_ % gop_length_ == 0;

    // Reset the frame count when keyframe is requested.
    if (input.options.key_frame || force_key_frame) {
      input_since_keyframe_count_ = 0;
    }

    int max_quantizer = AVEncQPtoQindex(codec_, GetMaxQuantizer(codec_));
    std::optional<uint8_t> quantizer;
    int temporal_id = 0;
    if (input.options.quantizer.has_value()) {
      DCHECK(codec_ == VideoCodec::kH264 || codec_ == VideoCodec::kHEVC);
      quantizer = std::clamp(static_cast<int>(input.options.quantizer.value()),
                             1, kH26xMaxQp);
    } else if (rate_ctrl_ && !input.discard_output) {
      VideoRateControlWrapper::FrameParams frame_params{};
      frame_params.frame_type =
          input.options.key_frame || force_key_frame
              ? VideoRateControlWrapper::FrameParams::FrameType::kKeyFrame
              : VideoRateControlWrapper::FrameParams::FrameType::kInterFrame;
      // H.264 and H.265 SW BRC need timestamp information.
      frame_params.timestamp = input.timestamp.InMilliseconds();
      temporal_id =
          svc_parser_->AssignTemporalIdBySvcSpec(input_since_keyframe_count_);
      frame_params.temporal_layer_id = temporal_id;
      // For now, MFVEA does not support spatial layer encoding.
      frame_params.spatial_layer_id = 0;
      // If there exists a rate_ctrl_, the qp computed by rate_ctrl_ should be
      // set on sample metadata and carried over from input to output.
      int computed_qp = rate_ctrl_->ComputeQP(frame_params);
      if (computed_qp < 0) {
        // Negative QP values mean that the frame should be dropped. We use
        // maximum QP in that case.
        // Drop frame functionality is not supported yet.
        // TODO(b/361250558): Support drop frame for H.264/HEVC Rate Controller
        computed_qp = max_quantizer;
      }
      quantizer = std::clamp(computed_qp, 1, max_quantizer);
    } else if (input.discard_output) {
      // Set up encoder for maximum speed if we're anyway going to discard the
      // output.
      quantizer = max_quantizer;
    }

    if (quantizer.has_value()) {
      VARIANT var;
      var.vt = VT_UI4;
      var.ulVal = temporal_id;
      DVLOG(3) << "Setting CODECAPI_AVEncVideoSelectLayer to " << var.ulVal;
      hr = codec_api_->SetValue(&CODECAPI_AVEncVideoSelectLayer, &var);
      RETURN_ON_HR_FAILURE(hr, "Couldn't set select temporal layer", hr);
      var.vt = VT_UI8;
      var.ullVal = QindextoAVEncQP(codec_, quantizer.value());
      DVLOG(3) << "Setting CODECAPI_AVEncVideoEncodeQP to " << var.ullVal;
      hr = codec_api_->SetValue(&CODECAPI_AVEncVideoEncodeQP, &var);
      RETURN_ON_HR_FAILURE(hr, "Couldn't set frame QP", hr);
      hr = input.input_sample->SetUINT64(MFSampleExtension_VideoEncodeQP,
                                         var.ullVal);
      RETURN_ON_HR_FAILURE(hr, "Couldn't set input sample attribute QP", hr);
    }
    if (input.options.key_frame || force_key_frame) {
      VARIANT var;
      var.vt = VT_UI4;
      var.ulVal = 1;
      DVLOG(3) << "Setting CODECAPI_AVEncVideoForceKeyFrame to " << var.ulVal;
      hr = codec_api_->SetValue(&CODECAPI_AVEncVideoForceKeyFrame, &var);
      RETURN_ON_HR_FAILURE(hr, "Set CODECAPI_AVEncVideoForceKeyFrame failed",
                           hr);
    }

    // We don't actually tell the MFT about the color space since all current
    // MFT implementations just write UNSPECIFIED in the bitstream, and setting
    // it can actually break some encoders; see https://crbug.com/1446081.
    sample_metadata_queue_.push_back(
        OutOfBandMetadata{.color_space = input.color_space,
                          .discard_output = input.discard_output,
                          .qp = quantizer,
                          .frame_id = input_since_keyframe_count_,
                          .timestamp = input.timestamp});
  }

  {
    TRACE_EVENT1("media", "IMFTransform::ProcessInput", "timestamp",
                 input.timestamp);
    hr = encoder_->ProcessInput(input_stream_id_, input.input_sample.Get(), 0);
  }

  // Check if ProcessInput() actually accepted the sample, if not, remember that
  // we don't need to prepare sample next time but just check the timestamp and
  // use it directly. This is a workaround for crbug.com/377749373 since HMFT
  // may reject to accept the new input which is corresponding to a
  // METransformNeedInput event.
  has_not_accepted_sample_ = (hr == MF_E_NOTACCEPTING);
  return hr;
}

HRESULT MediaFoundationVideoEncodeAccelerator::PopulateInputSampleBuffer(
    const PendingInput& input,
    scoped_refptr<VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto input_sample = input.input_sample;
  bool supported_shared_image =
      frame->HasSharedImage() && command_buffer_helper_;
  if (frame->storage_type() !=
          VideoFrame::StorageType::STORAGE_GPU_MEMORY_BUFFER &&
      !frame->IsMappable() && !supported_shared_image) {
    LOG(ERROR) << "Unsupported video frame storage type";
    return MF_E_INVALID_STREAM_DATA;
  }

  TRACE_EVENT1(
      "media",
      "MediaFoundationVideoEncodeAccelerator::PopulateInputSampleBuffer",
      "timestamp", frame->timestamp());

  bool is_supported_format;
  if (base::FeatureList::IsEnabled(kMediaFoundationD3DVideoProcessing)) {
    is_supported_format =
        std::ranges::find(kSupportedPixelFormatsD3DVideoProcessing,
                          frame->format()) !=
        kSupportedPixelFormatsD3DVideoProcessing.end();
  } else {
    is_supported_format =
        std::ranges::find(kSupportedPixelFormats, frame->format()) !=
        kSupportedPixelFormats.end();
  }
  if (!is_supported_format) {
    LOG(ERROR) << "Unsupported video frame format";
    return MF_E_INVALID_STREAM_DATA;
  }

  if (base::FeatureList::IsEnabled(kMediaFoundationD3DVideoProcessing) &&
      frame->format() != input_format_) {
    input_format_ = frame->format();
    if (frame->format() == PIXEL_FORMAT_NV12 ||
        frame->format() == PIXEL_FORMAT_I420) {
      mf_video_processor_ = nullptr;
    } else if (!mf_video_processor_) {
      if (!InitMFVideoProcessor()) {
        LOG(ERROR)
            << "Failed to initialize video processor for color conversion";
        return MF_E_INVALID_STREAM_DATA;
      }
    }
  }

  auto hr = input_sample->SetSampleTime(frame->timestamp().InMicroseconds() *
                                        kOneMicrosecondInMFSampleTimeUnits);
  RETURN_ON_HR_FAILURE(hr, "SetSampleTime() failed", hr);

  UINT64 sample_duration = 0;
  hr = MFFrameRateToAverageTimePerFrame(frame_rate_, 1, &sample_duration);
  RETURN_ON_HR_FAILURE(hr, "Couldn't calculate sample duration", hr);

  hr = input_sample->SetSampleDuration(sample_duration);
  RETURN_ON_HR_FAILURE(hr, "SetSampleDuration() failed", hr);

  if (frame->HasMappableGpuBuffer() || supported_shared_image) {
    if ((frame->HasNativeGpuMemoryBuffer() ||
         (!frame->HasMappableGpuBuffer() && supported_shared_image)) &&
        dxgi_device_manager_ != nullptr) {
      if (!dxgi_resource_mapping_required_) {
        return PopulateInputSampleBufferGpu(std::move(frame), input_sample);
      } else {
        return CopyInputSampleBufferFromGpu(std::move(frame), input_sample);
      }
    }

    // ConvertToMemoryMappedFrame() doesn't copy pixel data,
    // it just maps GPU buffer owned by |frame| and presents it as mapped
    // view in CPU memory. |frame| will unmap the buffer when destructed.
    frame = ConvertToMemoryMappedFrame(std::move(frame));
    if (!frame) {
      LOG(ERROR) << "Failed to map shared memory GMB";
      return E_FAIL;
    }
  }

  const auto kTargetPixelFormat = PIXEL_FORMAT_NV12;
  ComMFMediaBuffer input_buffer;
  hr = input_sample->GetBufferByIndex(0, &input_buffer);
  if (FAILED(hr)) {
    // Allocate a new buffer.
    MFT_INPUT_STREAM_INFO input_stream_info;
    hr = encoder_->GetInputStreamInfo(input_stream_id_, &input_stream_info);
    RETURN_ON_HR_FAILURE(hr, "Couldn't get input stream info", hr);
    hr = MFCreateAlignedMemoryBuffer(
        input_stream_info.cbSize ? input_stream_info.cbSize
                                 : VideoFrame::AllocationSize(
                                       kTargetPixelFormat, input_visible_size_),
        input_stream_info.cbAlignment == 0 ? input_stream_info.cbAlignment
                                           : input_stream_info.cbAlignment - 1,
        &input_buffer);
    RETURN_ON_HR_FAILURE(hr, "Failed to create memory buffer", hr);
    hr = input_buffer->SetCurrentLength(
        input_stream_info.cbSize
            ? input_stream_info.cbSize
            : VideoFrame::AllocationSize(kTargetPixelFormat,
                                         input_visible_size_));
    RETURN_ON_HR_FAILURE(hr, "Failed to set length on buffer", hr);
    hr = input_sample->AddBuffer(input_buffer.Get());
    RETURN_ON_HR_FAILURE(hr, "Failed to add buffer to sample", hr);
  }

  // Establish plain pointers into the input buffer, where we will copy pixel
  // data to.
  MediaBufferScopedPointer scoped_buffer(input_buffer.Get());
  size_t dst_y_stride = VideoFrame::RowBytes(
      VideoFrame::Plane::kY, kTargetPixelFormat, input_visible_size_.width());
  size_t dst_y_size =
      dst_y_stride * VideoFrame::Rows(VideoFrame::Plane::kY, kTargetPixelFormat,
                                      input_visible_size_.height());
  auto dst_y = scoped_buffer.as_span().first(dst_y_size);

  size_t dst_uv_stride = VideoFrame::RowBytes(
      VideoFrame::Plane::kUV, kTargetPixelFormat, input_visible_size_.width());
  size_t dst_uv_size =
      dst_uv_stride * VideoFrame::Rows(VideoFrame::Plane::kUV,
                                       kTargetPixelFormat,
                                       input_visible_size_.height());
  auto dst_uv = scoped_buffer.as_span().subspan(dst_y_size, dst_uv_size);

  // Set up a VideoFrame with the data pointing into the input buffer.
  // We need it to ease copying and scaling by reusing ConvertAndScale()
  auto frame_in_buffer = VideoFrame::WrapExternalYuvData(
      kTargetPixelFormat, input_visible_size_, gfx::Rect(input_visible_size_),
      input_visible_size_, dst_y_stride, dst_uv_stride, dst_y, dst_uv,
      frame->timestamp());

  auto status = frame_converter_.ConvertAndScale(*frame, *frame_in_buffer);
  if (!status.is_ok()) {
    LOG(ERROR) << "ConvertAndScale failed with error code: "
               << static_cast<uint32_t>(status.code());
    return E_FAIL;
  }

  if (!base::FeatureList::IsEnabled(kMediaFoundationSharedImageEncode)) {
    return S_OK;
  }

  // Input may be mixed shared image textures and CPU frames.  Encoder
  // MFTs generally expect consistency, either all CPU frames or
  // all textures.  Create a texture for CPU frames so that if
  // textures are supplied as input later, the encoder does not fail.
  // The MFT will do this internally anyway for CPU frames, so this
  // does not add any additional work.
  if (dxgi_device_manager_ && !dxgi_resource_mapping_required_) {
    D3D11_TEXTURE2D_DESC input_desc = {
        .Width = static_cast<UINT>(input_visible_size_.width()),
        .Height = static_cast<UINT>(input_visible_size_.height()),
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = VideoPixelFormatToDxgiFormat(kTargetPixelFormat),
        .SampleDesc = {1, 0},
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_VIDEO_ENCODER,
        .CPUAccessFlags = 0,
        .MiscFlags = 0};
    D3D11_SUBRESOURCE_DATA init_data = {
        .pSysMem = scoped_buffer.as_span().data(),
        .SysMemPitch = static_cast<UINT>(dst_y_stride),
        .SysMemSlicePitch = 0};
    ComD3D11Texture2D input_texture;
    hr = dxgi_device_manager_->GetDevice()->CreateTexture2D(
        &input_desc, &init_data, &input_texture);
    RETURN_ON_HR_FAILURE(hr, "Failed to create input texture for frame", hr);
    if (vendor_ == DriverVendor::kNvidia) {
      // When passing an initialized texture to the Nvidia MFT, it may not
      // "see" the data unless the device is flushed.
      ComD3D11DeviceContext device_context;
      dxgi_device_manager_->GetDevice()->GetImmediateContext(&device_context);
      device_context->Flush();
    }
    ComMFMediaBuffer input_texture_buffer;
    hr = MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D),
                                   input_texture.Get(), 0, FALSE,
                                   &input_texture_buffer);
    RETURN_ON_HR_FAILURE(hr, "Failed to create DXGI surface buffer for frame",
                         hr);
    DWORD buffer_length = 0;
    hr = input_texture_buffer->GetMaxLength(&buffer_length);
    RETURN_ON_HR_FAILURE(hr, "Failed to get max buffer length", hr);
    hr = input_texture_buffer->SetCurrentLength(buffer_length);
    RETURN_ON_HR_FAILURE(hr, "Failed to set current buffer length", hr);
    hr = input_sample->RemoveAllBuffers();
    RETURN_ON_HR_FAILURE(hr, "Failed to remove buffers from sample", hr);
    hr = input_sample->AddBuffer(input_texture_buffer.Get());
    RETURN_ON_HR_FAILURE(hr, "Failed to add buffer to sample", hr);
  }

  return S_OK;
}

// Handle case where video frame is backed by a GPU texture, but needs to be
// copied to CPU memory, if HMFT does not accept texture from adapter
// different from that is currently used for encoding.
HRESULT MediaFoundationVideoEncodeAccelerator::CopyInputSampleBufferFromGpu(
    scoped_refptr<VideoFrame> frame,
    ComMFSample& input_sample) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(frame->storage_type() ==
             VideoFrame::StorageType::STORAGE_GPU_MEMORY_BUFFER ||
         frame->HasSharedImage());
  DCHECK(dxgi_device_manager_);

  auto d3d_device = dxgi_device_manager_->GetDevice();
  if (!d3d_device) {
    LOG(ERROR) << "Failed to get device from MF DXGI device manager";
    return E_HANDLE;
  }

  HRESULT hr;
  ComD3D11Texture2D input_texture;
  if (frame->storage_type() ==
      VideoFrame::StorageType::STORAGE_GPU_MEMORY_BUFFER) {
    gfx::GpuMemoryBufferHandle buffer_handle =
        frame->GetGpuMemoryBufferHandle();

    ComD3D11Device1 device1;
    hr = d3d_device.As(&device1);
    RETURN_ON_HR_FAILURE(hr, "Failed to query ID3D11Device1", hr);
    hr = device1->OpenSharedResource1(
        buffer_handle.dxgi_handle().buffer_handle(),
        IID_PPV_ARGS(&input_texture));
    RETURN_ON_HR_FAILURE(hr, "Failed to open shared GMB D3D texture", hr);
  } else if (frame->HasSharedImage()) {
    DCHECK(input_sample);
    ComMFMediaBuffer texture_buffer;
    hr = input_sample->GetBufferByIndex(0, &texture_buffer);
    RETURN_ON_HR_FAILURE(hr, "Failed to get sample's buffer", hr);
    Microsoft::WRL::ComPtr<IMFDXGIBuffer> dxgi_buffer;
    hr = texture_buffer.As(&dxgi_buffer);
    RETURN_ON_HR_FAILURE(hr, "Failed to get dxgi buffer", hr);
    hr = dxgi_buffer->GetResource(IID_PPV_ARGS(&input_texture));
  }

  // Check if we need to scale the input texture
  D3D11_TEXTURE2D_DESC input_desc = {};
  input_texture->GetDesc(&input_desc);
  gfx::Size texture_size(input_desc.Width, input_desc.Height);
  ComD3D11Texture2D sample_texture;
  if (texture_size != input_visible_size_ ||
      frame->visible_rect().size() != input_visible_size_ ||
      !frame->visible_rect().origin().IsOrigin()) {
    hr = PerformD3DScaling(input_texture.Get(), frame->visible_rect());
    RETURN_ON_HR_FAILURE(hr, "Failed to perform D3D video processing", hr);
    sample_texture = scaled_d3d11_texture_;
  } else {
    sample_texture = input_texture;
  }

  ComMFMediaBuffer input_buffer;

  // Allocate a new buffer.
  MFT_INPUT_STREAM_INFO input_stream_info;
  hr = encoder_->GetInputStreamInfo(input_stream_id_, &input_stream_info);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get input stream info", hr);
  hr = MFCreateAlignedMemoryBuffer(
      input_stream_info.cbSize
          ? input_stream_info.cbSize
          : VideoFrame::AllocationSize(frame->format(), input_visible_size_),
      input_stream_info.cbAlignment == 0 ? input_stream_info.cbAlignment
                                         : input_stream_info.cbAlignment - 1,
      &input_buffer);
  RETURN_ON_HR_FAILURE(hr, "Failed to create memory buffer for input sample",
                       hr);

  MediaBufferScopedPointer scoped_buffer(input_buffer.Get());
  bool copy_succeeded =
      gpu::CopyD3D11TexToMem(sample_texture.Get(), scoped_buffer.as_span(),
                             d3d_device.Get(), &staging_texture_);
  if (!copy_succeeded) {
    LOG(ERROR) << "Failed to copy sample to memory.";
    return E_FAIL;
  }
  size_t copied_bytes =
      input_visible_size_.width() * input_visible_size_.height() * 3 / 2;
  hr = input_buffer->SetCurrentLength(copied_bytes);
  RETURN_ON_HR_FAILURE(hr, "Failed to set current buffer length", hr);
  hr = input_sample->RemoveAllBuffers();
  RETURN_ON_HR_FAILURE(hr, "Failed to remove buffers from sample", hr);
  hr = input_sample->AddBuffer(input_buffer.Get());
  RETURN_ON_HR_FAILURE(hr, "Failed to add buffer to sample", hr);

  if (mf_video_processor_) {
    // This sample needs color space conversion
    ComMFSample vp_output_sample;
    hr = mf_video_processor_->Convert(input_sample.Get(), frame->format(),
                                      &vp_output_sample);
    // input_sample is the sample that will be fed to the encoder, but
    // its buffers are from the original color format.  Remove those
    // buffers and replace them with the buffer that has been
    // converted to the target color format.
    RETURN_ON_HR_FAILURE(hr, "Failed to convert input frame", hr);
    hr = input_sample->RemoveAllBuffers();
    RETURN_ON_HR_FAILURE(hr, "Failed to remove buffers from sample", hr);
    ComMFMediaBuffer vp_output_buffer;
    hr = vp_output_sample->GetBufferByIndex(0, &vp_output_buffer);
    RETURN_ON_HR_FAILURE(hr, "Failed to get output buffer from sample", hr);
    hr = input_sample->AddBuffer(vp_output_buffer.Get());
  }

  return S_OK;
}

// Handle case where video frame is backed by a GPU texture
HRESULT MediaFoundationVideoEncodeAccelerator::PopulateInputSampleBufferGpu(
    scoped_refptr<VideoFrame> frame,
    ComMFSample& input_sample) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(dxgi_device_manager_);

  if (mf_video_processor_) {
    // Using the MF video processor mitigates many of the issues handled below.
    // - MFVP will resize if needed
    // - MFVP acquires the texture's keyed mutex when available and
    //    holds it only for the duration needed.
    // - MFVP will call SetCurrentLength on the output buffer
    // - MFVP will output a different texture that can be used
    //    as encoder input with no synchronization issues.
    HRESULT hr;
    ComMFSample vp_output_sample;
    if (frame->HasSharedImage()) {
      hr = mf_video_processor_->Convert(input_sample.Get(), frame->format(),
                                        &vp_output_sample);
    } else {
      hr = mf_video_processor_->Convert(frame, &vp_output_sample);
    }
    RETURN_ON_HR_FAILURE(hr, "Failed to convert input frame", hr);
    hr = input_sample->RemoveAllBuffers();
    RETURN_ON_HR_FAILURE(hr, "Failed to remove buffers from sample", hr);
    ComMFMediaBuffer vp_output_buffer;
    hr = vp_output_sample->GetBufferByIndex(0, &vp_output_buffer);
    RETURN_ON_HR_FAILURE(hr, "Failed to get output buffer from sample", hr);
    hr = input_sample->AddBuffer(vp_output_buffer.Get());
    return S_OK;
  }

  // Check if we need to scale the input texture
  HRESULT hr;
  ComD3D11Texture2D input_texture;
  ComD3D11Texture2D sample_texture;
  if (frame->storage_type() ==
      VideoFrame::StorageType::STORAGE_GPU_MEMORY_BUFFER) {
    gfx::GpuMemoryBufferHandle buffer_handle =
        frame->GetGpuMemoryBufferHandle();

    auto d3d_device = dxgi_device_manager_->GetDevice();
    if (!d3d_device) {
      LOG(ERROR) << "Failed to get device from MF DXGI device manager";
      return E_HANDLE;
    }

    ComD3D11Device1 device1;
    hr = d3d_device.As(&device1);
    RETURN_ON_HR_FAILURE(hr, "Failed to query ID3D11Device1", hr);

    hr = device1->OpenSharedResource1(
        buffer_handle.dxgi_handle().buffer_handle(),
        IID_PPV_ARGS(&input_texture));
    RETURN_ON_HR_FAILURE(hr, "Failed to open shared GMB D3D texture", hr);
  } else if (frame->HasSharedImage()) {
    ComMFMediaBuffer texture_buffer;
    hr = input_sample->GetBufferByIndex(0, &texture_buffer);
    RETURN_ON_HR_FAILURE(hr, "Failed to get sample's buffer", hr);
    Microsoft::WRL::ComPtr<IMFDXGIBuffer> dxgi_buffer;
    hr = texture_buffer.As(&dxgi_buffer);
    RETURN_ON_HR_FAILURE(hr, "Failed to get dxgi buffer", hr);
    hr = dxgi_buffer->GetResource(IID_PPV_ARGS(&input_texture));
  }

  if (frame->visible_rect().size() != input_visible_size_) {
    hr = PerformD3DScaling(input_texture.Get(), frame->visible_rect());
    RETURN_ON_HR_FAILURE(hr, "Failed to perform D3D video processing", hr);
    sample_texture = scaled_d3d11_texture_;
  } else if (!frame->VideoFrame::HasMappableGpuBuffer() &&
             frame->HasSharedImage()) {
    // Shared images that are not GpuMemoryBuffers have already
    // been copied.
    sample_texture = input_texture;
  } else {
    // Even though no scaling is needed we still need to copy the texture to
    // avoid concurrent usage causing glitches (https://crbug.com/1462315). This
    // is preferred over holding a keyed mutex for the duration of the encode
    // operation since that can take a significant amount of time and mutex
    // acquisitions (necessary even for read-only operations) are blocking.
    hr = PerformD3DCopy(input_texture.Get(), frame->visible_rect());
    RETURN_ON_HR_FAILURE(hr, "Failed to perform D3D texture copy", hr);
    sample_texture = copied_d3d11_texture_;
  }

  ComMFMediaBuffer input_buffer;
  hr = MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D),
                                 sample_texture.Get(), 0, FALSE, &input_buffer);
  RETURN_ON_HR_FAILURE(hr, "Failed to create MF DXGI surface buffer", hr);

  // Some encoder MFTs (e.g. Qualcomm) depend on the sample buffer having a
  // valid current length. Call GetMaxLength() to compute the plane size.
  DWORD buffer_length = 0;
  hr = input_buffer->GetMaxLength(&buffer_length);
  RETURN_ON_HR_FAILURE(hr, "Failed to get max buffer length", hr);
  hr = input_buffer->SetCurrentLength(buffer_length);
  RETURN_ON_HR_FAILURE(hr, "Failed to set current buffer length", hr);

  hr = input_sample->RemoveAllBuffers();
  RETURN_ON_HR_FAILURE(hr, "Failed to remove buffers from sample", hr);
  hr = input_sample->AddBuffer(input_buffer.Get());
  RETURN_ON_HR_FAILURE(hr, "Failed to add buffer to sample", hr);
  return S_OK;
}

void MediaFoundationVideoEncodeAccelerator::ProcessOutput() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("media", "MediaFoundationVideoEncodeAccelerator::ProcessOutput");

  MFT_OUTPUT_DATA_BUFFER output_data_buffer = {0};
  output_data_buffer.dwStreamID = output_stream_id_;
  output_data_buffer.dwStatus = 0;
  output_data_buffer.pEvents = nullptr;
  output_data_buffer.pSample = nullptr;
  DWORD status = 0;
  HRESULT hr = encoder_->ProcessOutput(0, 1, &output_data_buffer, &status);
  // If there is an IMFCollection of events, release it
  if (output_data_buffer.pEvents != nullptr) {
    DVLOG(3) << "Got events from ProcessOutput, but discarding.";
    output_data_buffer.pEvents->Release();
  }
  if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
    hr = S_OK;
    ComMFMediaType media_type;
    for (DWORD type_index = 0; SUCCEEDED(hr); ++type_index) {
      hr = encoder_->GetOutputAvailableType(output_stream_id_, type_index,
                                            &media_type);
      if (SUCCEEDED(hr)) {
        break;
      }
    }
    hr = encoder_->SetOutputType(output_stream_id_, media_type.Get(), 0);
    return;
  }

  RETURN_ON_HR_FAILURE(hr, "Couldn't get encoded data", );
  DVLOG(3) << "Got encoded data " << PrintHr(hr);

  ComMFSample output_sample;
  ComMFMediaBuffer output_buffer;
  output_sample.Attach(output_data_buffer.pSample);
  hr = output_data_buffer.pSample->GetBufferByIndex(0, &output_buffer);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get buffer by index", );

  base::TimeDelta timestamp;
  LONGLONG sample_time;
  hr = output_data_buffer.pSample->GetSampleTime(&sample_time);
  if (SUCCEEDED(hr)) {
    timestamp =
        base::Microseconds(sample_time / kOneMicrosecondInMFSampleTimeUnits);
  }

  DCHECK(!sample_metadata_queue_.empty());
  const auto metadata = sample_metadata_queue_.front();
  sample_metadata_queue_.pop_front();
  if (metadata.discard_output) {
    return;
  }

  // If `frame_qp` is set here, it will be plumbed down to WebRTC.
  // If not set, the QP may be parsed by WebRTC from the bitstream but only if
  // the QP is trusted (`encoder_info_.reports_average_qp` is true, which it is
  // by default).
  std::optional<int32_t> frame_qp;
  bool should_notify_reports_average_qp_change = false;
  // If there exists a valid qp in sample metadata, do not query HMFT for
  // MFSampleExtension_VideoEncodeQP.
  if (metadata.qp.has_value()) {
    frame_qp = metadata.qp.value();
  } else {
    // For HMFT that continuously reports valid QP, update encoder info so that
    // WebRTC will not use bandwidth quality scaler for resolution adaptation.
    uint64_t frame_qp_from_sample = 0xfffful;
    hr = output_data_buffer.pSample->GetUINT64(MFSampleExtension_VideoEncodeQP,
                                               &frame_qp_from_sample);
    if (vendor_ == DriverVendor::kIntel) {
      if ((FAILED(hr) || !IsValidQp(codec_, frame_qp_from_sample)) &&
          encoder_info_.reports_average_qp) {
        should_notify_reports_average_qp_change = true;
        encoder_info_.reports_average_qp = false;
      }
    }
    // Bits 0-15: Default QP.
    if (SUCCEEDED(hr)) {
      frame_qp = AVEncQPtoQindex(codec_, frame_qp_from_sample & 0xfffful);
    }
  }
  if (should_notify_reports_average_qp_change) {
    client_->NotifyEncoderInfoChange(encoder_info_);
  }

  const bool keyframe = MFGetAttributeUINT32(
      output_data_buffer.pSample, MFSampleExtension_CleanPoint, false);
  DWORD output_buffer_size = 0;
  hr = output_buffer->GetCurrentLength(&output_buffer_size);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get buffer length", );
  DCHECK_NE(output_buffer_size, 0u);
  MediaBufferScopedPointer scoped_buffer(output_buffer.Get());
  auto output_buffer_span = scoped_buffer.as_span().first(output_buffer_size);

  BitstreamBufferMetadata md(output_buffer_span.size(), keyframe, timestamp);
  if (frame_qp.has_value() && IsValidQp(codec_, *frame_qp)) {
    md.qp = *frame_qp;
  }
  if (metadata.color_space.IsValid()) {
    md.encoded_color_space = metadata.color_space;
  }

  int temporal_id = 0;
  if (IsTemporalScalabilityCoding()) {
    DCHECK(svc_parser_);
    TemporalScalabilityIdExtractor::BitstreamMetadata bits_md;
    if (!svc_parser_->ParseChunk(output_buffer_span, metadata.frame_id,
                                 bits_md)) {
      NotifyErrorStatus({EncoderStatus::Codes::kEncoderHardwareDriverError,
                         "Parse bitstream failed"});
      return;
    }
    temporal_id = bits_md.temporal_id;
    if (codec_ == VideoCodec::kH264) {
      md.h264.emplace().temporal_idx = temporal_id;
    } else if (codec_ == VideoCodec::kHEVC) {
      SVCGenericMetadata& svc = md.svc_generic.emplace();
      svc.temporal_idx = temporal_id;
      svc.spatial_idx = 0;
      // We only get the temporal id from NALU header of output bitstream
      // without extracting the reference structure, so we are not able to
      // provide the reference flags and refresh flags for HEVC, if the
      // |follow_svc_spec| flag is false, and RTC will not send dependency
      // descriptor RTP extension.
      svc.follow_svc_spec = encoder_produces_svc_spec_compliant_bitstream_;
    } else if (codec_ == VideoCodec::kAV1) {
      SVCGenericMetadata& svc = md.svc_generic.emplace();
      svc.temporal_idx = temporal_id;
      svc.spatial_idx = 0;
      svc.follow_svc_spec = encoder_produces_svc_spec_compliant_bitstream_;
      if (!svc.follow_svc_spec) {
        svc.reference_flags = bits_md.reference_idx_flags;
        svc.refresh_flags = bits_md.refresh_frame_flags;
      }
    } else if (codec_ == VideoCodec::kVP9) {
      Vp9Metadata& vp9 = md.vp9.emplace();
      if (keyframe) {
        // |spatial_layer_resolutions| has to be filled if keyframe is
        // requested.
        vp9.spatial_layer_resolutions.emplace_back(input_visible_size_);
        vp9.begin_active_spatial_layer_index = 0;
        vp9.end_active_spatial_layer_index =
            1 /*vp9.spatial_layer_resolutions.size()*/;
      } else {
        // For VP9 L1T2/L1T3 encoding on Intel drivers, a T1 frame may ref the
        // previous T1 frame which leads to not all T0 frame can be a sync point
        // to go up for higher temporal layers. We need to pick out the T0 frame
        // based on deterministic pattern and mark it as up-switch.
        // See https://crbug.com/1358750 for more details.
        if (vendor_ == DriverVendor::kIntel) {
          DCHECK(num_temporal_layers_ >= 2 && num_temporal_layers_ <= 3);
          uint32_t multiplier = num_temporal_layers_ == 3 ? 2 : 4;
          bool is_single_ref = zero_layer_counter_ % multiplier == 0;
          vp9.temporal_up_switch = true;
          if (temporal_id == 0) {
            zero_layer_counter_++;
            if (!is_single_ref) {
              // If |is_single_ref| is false, the subsequent T1 frame will ref
              // the previous T1 frame, so the current frame can not mark as
              // up-switch.
              vp9.temporal_up_switch = false;
            }
          } else if (is_single_ref) {
            // If |is_single_ref| is true, the T1/T2 layer only allowed to ref
            // the frames with lower temporal layer id, add check to guarantee
            // the ref dependency follow the deterministic pattern on Intel
            // drivers.
            for (const auto ref : bits_md.ref_frame_list) {
              if (ref.temporal_id >= temporal_id) {
                NotifyErrorStatus(
                    {EncoderStatus::Codes::kEncoderHardwareDriverError,
                     "VP9 referenced frames check failed "});
                return;
              }
            }
          }
        }
        // Fill the encoding metadata for VP9 non key frames.
        vp9.inter_pic_predicted = true;
        vp9.temporal_idx = temporal_id;
        for (const auto ref : bits_md.ref_frame_list) {
          vp9.p_diffs.push_back(metadata.frame_id - ref.frame_id);
        }
      }
    }
  }

  if (rate_ctrl_) {
    VideoRateControlWrapper::FrameParams frame_params{};
    frame_params.frame_type =
        keyframe ? VideoRateControlWrapper::FrameParams::FrameType::kKeyFrame
                 : VideoRateControlWrapper::FrameParams::FrameType::kInterFrame;
    frame_params.temporal_layer_id = temporal_id;
    frame_params.timestamp = timestamp.InMilliseconds();
    // Notify SW BRC about recent encoded frame size.
    rate_ctrl_->PostEncodeUpdate(output_buffer_span.size(), frame_params);
  }
  DVLOG(3) << "Encoded data with size:" << output_buffer_span.size()
           << " keyframe " << keyframe;
  // If no bit stream buffer presents, queue the output first.
  if (bitstream_buffer_queue_.empty()) {
    DVLOG(3) << "No bitstream buffers.";

    // We need to copy the output so that encoding can continue.
    auto encode_output =
        std::make_unique<EncodeOutput>(output_buffer_span.size(), md);
    encode_output->as_span().copy_from(output_buffer_span);
    encoder_output_queue_.push_back(std::move(encode_output));
    return;
  }

  // If `bitstream_buffer_queue_` is not empty,
  // meaning we have output buffers to spare, `encoder_output_queue_` must
  // be empty, otherwise outputs should've already been returned using those
  // buffers.
  DCHECK(encoder_output_queue_.empty());

  // Immediately return encoded buffer with BitstreamBuffer to client.
  auto buffer_ref = std::move(bitstream_buffer_queue_.back());
  bitstream_buffer_queue_.pop_back();

  if (!buffer_ref->mapping.IsValid() ||
      buffer_ref->mapping.size() < output_buffer_span.size()) {
    NotifyErrorStatus({EncoderStatus::Codes::kInvalidOutputBuffer,
                       "Failed to copy bitstream media buffer."});
    return;
  }

  buffer_ref->mapping.GetMemoryAsSpan<uint8_t>().copy_prefix_from(
      output_buffer_span);
  client_->BitstreamBufferReady(buffer_ref->id, md);
}

void MediaFoundationVideoEncodeAccelerator::MediaEventHandler(
    MediaEventType event_type,
    HRESULT status) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(event_generator_);

  if (FAILED(status)) {
    NotifyErrorStatus({EncoderStatus::Codes::kSystemAPICallError,
                       "Media Foundation async error: " + PrintHr(status)});
    return;
  }

  switch (event_type) {
    case METransformNeedInput: {
      encoder_needs_input_counter_++;
      if (state_ == kInitializing || state_ == kInitializingWithCommandBuffer) {
        // HMFT is not ready for receiving inputs until the first
        // METransformNeedInput event is published.
        client_->RequireBitstreamBuffers(kNumInputBuffers, input_visible_size_,
                                         bitstream_buffer_size_);
        if (state_ == kInitializing) {
          SetState(kEncoding);
        } else {
          SetState(kWaitingForCommandBuffer);
        }
      } else if (state_ == kEncoding) {
        FeedInputs();
      } else if (state_ == kPreFlushing) {
        FeedInputs();
        if (pending_input_queue_.empty()) {
          // All pending inputs are sent to the MF encoder, it's time to tell it
          // to drain and produce all outputs.
          DrainEncoder();
        }
      }
      break;
    }
    case METransformHaveOutput: {
      ProcessOutput();
      break;
    }
    case METransformDrainComplete: {
      DCHECK(pending_input_queue_.empty());
      DCHECK(sample_metadata_queue_.empty());
      DCHECK_EQ(state_, kFlushing);
      // Reset the need input counter after drain complete.
      encoder_needs_input_counter_ = 0;
      auto hr = encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
      if (FAILED(hr)) {
        SetState(kError);
        std::move(flush_callback_).Run(false);
        return;
      }
      if (encoder_output_queue_.empty()) {
        // No pending outputs, let's signal that the Flush() is done and
        // continue encoding.
        SetState(kEncoding);
        std::move(flush_callback_).Run(true);
      } else {
        // There are pending outputs that are not returned yet,
        // let's wait for client to consume them, before signaling that
        // the Flush() has finished.
        SetState(kPostFlushing);
      }
      break;
    }
    case MEError: {
      NotifyErrorStatus({EncoderStatus::Codes::kEncoderHardwareDriverError,
                         "Media Foundation encountered a critical failure."});
      break;
    }
    default:
      break;
  }
  event_generator_->BeginGetEvent(this, nullptr);
}

void MediaFoundationVideoEncodeAccelerator::SetState(State state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(3) << "Setting state to: " << state;
  state_ = state;
}

HRESULT MediaFoundationVideoEncodeAccelerator::InitializeD3DVideoProcessing(
    ID3D11Texture2D* input_texture) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  D3D11_TEXTURE2D_DESC input_desc = {};
  input_texture->GetDesc(&input_desc);
  if (vp_desc_.InputWidth == input_desc.Width &&
      vp_desc_.InputHeight == input_desc.Height &&
      scaled_d3d11_texture_desc_.Width ==
          static_cast<UINT>(input_visible_size_.width()) &&
      scaled_d3d11_texture_desc_.Height ==
          static_cast<UINT>(input_visible_size_.height())) {
    return S_OK;
  }

  // Input/output framerates are dummy values for passthrough.
  D3D11_VIDEO_PROCESSOR_CONTENT_DESC vp_desc = {
      .InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE,
      .InputFrameRate = {60, 1},
      .InputWidth = input_desc.Width,
      .InputHeight = input_desc.Height,
      .OutputFrameRate = {60, 1},
      .OutputWidth = static_cast<UINT>(input_visible_size_.width()),
      .OutputHeight = static_cast<UINT>(input_visible_size_.height()),
      .Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL};

  ComD3D11Device texture_device;
  input_texture->GetDevice(&texture_device);
  ComD3D11VideoDevice video_device;
  HRESULT hr = texture_device.As(&video_device);
  RETURN_ON_HR_FAILURE(hr, "Failed to query for ID3D11VideoDevice", hr);

  ComD3D11VideoProcessorEnumerator video_processor_enumerator;
  hr = video_device->CreateVideoProcessorEnumerator(
      &vp_desc, &video_processor_enumerator);
  RETURN_ON_HR_FAILURE(hr, "CreateVideoProcessorEnumerator failed", hr);

  ComD3D11VideoProcessor video_processor;
  hr = video_device->CreateVideoProcessor(video_processor_enumerator.Get(), 0,
                                          &video_processor);
  RETURN_ON_HR_FAILURE(hr, "CreateVideoProcessor failed", hr);

  ComD3D11DeviceContext device_context;
  texture_device->GetImmediateContext(&device_context);
  ComD3D11VideoContext video_context;
  hr = device_context.As(&video_context);
  RETURN_ON_HR_FAILURE(hr, "Failed to query for ID3D11VideoContext", hr);

  // Auto stream processing (the default) can hurt power consumption.
  video_context->VideoProcessorSetStreamAutoProcessingMode(
      video_processor.Get(), 0, FALSE);

  D3D11_TEXTURE2D_DESC scaled_desc = {
      .Width = static_cast<UINT>(input_visible_size_.width()),
      .Height = static_cast<UINT>(input_visible_size_.height()),
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = DXGI_FORMAT_NV12,
      .SampleDesc = {1, 0},
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
      .CPUAccessFlags = 0,
      .MiscFlags = 0};
  ComD3D11Texture2D scaled_d3d11_texture;
  hr = texture_device->CreateTexture2D(&scaled_desc, nullptr,
                                       &scaled_d3d11_texture);
  RETURN_ON_HR_FAILURE(hr, "Failed to create texture", hr);

  hr = SetDebugName(scaled_d3d11_texture.Get(),
                    "MFVideoEncodeAccelerator_ScaledTexture");
  RETURN_ON_HR_FAILURE(hr, "Failed to set debug name", hr);

  D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC output_desc = {};
  output_desc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
  output_desc.Texture2D.MipSlice = 0;
  ComD3D11VideoProcessorOutputView vp_output_view;
  hr = video_device->CreateVideoProcessorOutputView(
      scaled_d3d11_texture.Get(), video_processor_enumerator.Get(),
      &output_desc, &vp_output_view);
  RETURN_ON_HR_FAILURE(hr, "CreateVideoProcessorOutputView failed", hr);

  video_device_ = std::move(video_device);
  video_processor_enumerator_ = std::move(video_processor_enumerator);
  video_processor_ = std::move(video_processor);
  video_context_ = std::move(video_context);
  vp_desc_ = std::move(vp_desc);
  scaled_d3d11_texture_ = std::move(scaled_d3d11_texture);
  scaled_d3d11_texture_->GetDesc(&scaled_d3d11_texture_desc_);
  vp_output_view_ = std::move(vp_output_view);
  return S_OK;
}

HRESULT MediaFoundationVideoEncodeAccelerator::PerformD3DScaling(
    ID3D11Texture2D* input_texture,
    const gfx::Rect& visible_rect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HRESULT hr = InitializeD3DVideoProcessing(input_texture);
  RETURN_ON_HR_FAILURE(hr, "Couldn't initialize D3D video processing", hr);

  // Set the color space for passthrough.
  auto src_color_space = gfx::ColorSpace::CreateSRGB();
  auto output_color_space = gfx::ColorSpace::CreateSRGB();

  D3D11_VIDEO_PROCESSOR_COLOR_SPACE src_d3d11_color_space =
      gfx::ColorSpaceWin::GetD3D11ColorSpace(src_color_space);
  video_context_->VideoProcessorSetStreamColorSpace(video_processor_.Get(), 0,
                                                    &src_d3d11_color_space);
  D3D11_VIDEO_PROCESSOR_COLOR_SPACE output_d3d11_color_space =
      gfx::ColorSpaceWin::GetD3D11ColorSpace(output_color_space);
  video_context_->VideoProcessorSetOutputColorSpace(video_processor_.Get(),
                                                    &output_d3d11_color_space);

  {
    std::optional<gpu::DXGIScopedReleaseKeyedMutex> release_keyed_mutex;
    ComDXGIKeyedMutex keyed_mutex;
    hr = input_texture->QueryInterface(IID_PPV_ARGS(&keyed_mutex));
    if (SUCCEEDED(hr)) {
      // The producer may still be using this texture for a short period of
      // time, so wait long enough to hopefully avoid glitches. For example,
      // all levels of the texture share the same keyed mutex, so if the
      // hardware decoder acquired the mutex to decode into a different array
      // level then it still may block here temporarily.
      constexpr int kMaxSyncTimeMs = 100;
      hr = keyed_mutex->AcquireSync(0, kMaxSyncTimeMs);
      // Can't check for FAILED(hr) because AcquireSync may return e.g.
      // WAIT_ABANDONED.
      if (hr != S_OK && hr != WAIT_TIMEOUT) {
        LOG(ERROR) << "Failed to acquire mutex: " << PrintHr(hr);
        return E_FAIL;
      }
      release_keyed_mutex.emplace(std::move(keyed_mutex), 0);
    }

    // Setup |video_context_| for VPBlt operation.
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC input_desc = {};
    input_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    input_desc.Texture2D.ArraySlice = 0;
    ComD3D11VideoProcessorInputView input_view;
    hr = video_device_->CreateVideoProcessorInputView(
        input_texture, video_processor_enumerator_.Get(), &input_desc,
        &input_view);
    RETURN_ON_HR_FAILURE(hr, "CreateVideoProcessorInputView failed", hr);

    D3D11_VIDEO_PROCESSOR_STREAM stream = {.Enable = true,
                                           .OutputIndex = 0,
                                           .InputFrameOrField = 0,
                                           .PastFrames = 0,
                                           .FutureFrames = 0,
                                           .pInputSurface = input_view.Get()};

    D3D11_TEXTURE2D_DESC input_texture_desc = {};
    input_texture->GetDesc(&input_texture_desc);
    RECT source_rect = {static_cast<LONG>(visible_rect.x()),
                        static_cast<LONG>(visible_rect.y()),
                        static_cast<LONG>(visible_rect.right()),
                        static_cast<LONG>(visible_rect.bottom())};
    video_context_->VideoProcessorSetStreamSourceRect(video_processor_.Get(), 0,
                                                      TRUE, &source_rect);

    D3D11_TEXTURE2D_DESC output_texture_desc = {};
    scaled_d3d11_texture_->GetDesc(&output_texture_desc);
    RECT dest_rect = {0, 0, static_cast<LONG>(output_texture_desc.Width),
                      static_cast<LONG>(output_texture_desc.Height)};
    video_context_->VideoProcessorSetOutputTargetRect(video_processor_.Get(),
                                                      TRUE, &dest_rect);
    video_context_->VideoProcessorSetStreamDestRect(video_processor_.Get(), 0,
                                                    TRUE, &dest_rect);

    hr = video_context_->VideoProcessorBlt(
        video_processor_.Get(), vp_output_view_.Get(), 0, 1, &stream);
    RETURN_ON_HR_FAILURE(hr, "VideoProcessorBlt failed", hr);
  }

  return hr;
}

HRESULT MediaFoundationVideoEncodeAccelerator::InitializeD3DCopying(
    ID3D11Texture2D* input_texture) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  D3D11_TEXTURE2D_DESC input_desc = {};
  input_texture->GetDesc(&input_desc);
  // Return early if `copied_d3d11_texture_` is already the correct size,
  // avoiding the overhead of creating a new destination texture.
  if (copied_d3d11_texture_) {
    D3D11_TEXTURE2D_DESC copy_desc = {};
    copied_d3d11_texture_->GetDesc(&copy_desc);
    if (input_desc.Width == copy_desc.Width &&
        input_desc.Height == copy_desc.Height) {
      return S_OK;
    }
  }
  ComD3D11Device texture_device;
  input_texture->GetDevice(&texture_device);
  D3D11_TEXTURE2D_DESC copy_desc = {
      .Width = input_desc.Width,
      .Height = input_desc.Height,
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = DXGI_FORMAT_NV12,
      .SampleDesc = {1, 0},
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
      .CPUAccessFlags = 0,
      .MiscFlags = 0};
  ComD3D11Texture2D copied_d3d11_texture;
  HRESULT hr = texture_device->CreateTexture2D(&copy_desc, nullptr,
                                               &copied_d3d11_texture);
  RETURN_ON_HR_FAILURE(hr, "Failed to create texture", hr);
  hr = SetDebugName(copied_d3d11_texture.Get(),
                    "MFVideoEncodeAccelerator_CopiedTexture");
  RETURN_ON_HR_FAILURE(hr, "Failed to set debug name", hr);
  copied_d3d11_texture_ = std::move(copied_d3d11_texture);
  return S_OK;
}

HRESULT MediaFoundationVideoEncodeAccelerator::PerformD3DCopy(
    ID3D11Texture2D* input_texture,
    const gfx::Rect& visible_rect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HRESULT hr = InitializeD3DCopying(input_texture);
  RETURN_ON_HR_FAILURE(hr, "Couldn't initialize D3D copying", hr);

  ComD3D11Device d3d_device = dxgi_device_manager_->GetDevice();
  if (!d3d_device) {
    LOG(ERROR) << "Failed to get device from MF DXGI device manager";
    return E_HANDLE;
  }
  ComD3D11DeviceContext device_context;
  d3d_device->GetImmediateContext(&device_context);

  {
    // We need to hold a keyed mutex during the copy operation.
    std::optional<gpu::DXGIScopedReleaseKeyedMutex> release_keyed_mutex;
    ComDXGIKeyedMutex keyed_mutex;
    hr = input_texture->QueryInterface(IID_PPV_ARGS(&keyed_mutex));
    if (SUCCEEDED(hr)) {
      constexpr int kMaxSyncTimeMs = 100;
      hr = keyed_mutex->AcquireSync(0, kMaxSyncTimeMs);
      // Can't check for FAILED(hr) because AcquireSync may return e.g.
      // WAIT_ABANDONED.
      if (hr != S_OK && hr != WAIT_TIMEOUT) {
        LOG(ERROR) << "Failed to acquire mutex: " << PrintHr(hr);
        return E_FAIL;
      }
      release_keyed_mutex.emplace(std::move(keyed_mutex), 0);
    }

    D3D11_BOX src_box = {static_cast<UINT>(visible_rect.x()),
                         static_cast<UINT>(visible_rect.y()),
                         0,
                         static_cast<UINT>(visible_rect.right()),
                         static_cast<UINT>(visible_rect.bottom()),
                         1};
    device_context->CopySubresourceRegion(copied_d3d11_texture_.Get(), 0, 0, 0,
                                          0, input_texture, 0, &src_box);
  }
  return S_OK;
}

void MediaFoundationVideoEncodeAccelerator::OnCommandBufferHelperAvailable(
    const GetCommandBufferHelperResult& result) {
  command_buffer_helper_ = result.command_buffer_helper;
  if (!InitializeMFT(result.shared_d3d_device.Get())) {
    // InitializeMFT will call NotifyErrorStatus if an error occurs
    return;
  }

  if (state_ == kAcquiringCommandBuffer) {
    // CommandBufferHelper acquired before Initialize is called, go to
    // uninitialized state so full initialization can be done
    // in Initialize.
    SetState(kUninitialized);
  } else if (state_ == kWaitingForCommandBuffer) {
    SetState(kEncoding);
    FeedInputs();
  } else if (state_ == kInitializingWithCommandBuffer) {
    SetState(kInitializing);
  }
}

void MediaFoundationVideoEncodeAccelerator::OnSharedImageSampleAvailable(
    scoped_refptr<VideoFrame> frame,
    ComMFSample sample,
    HRESULT hrResult) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (FAILED(hrResult)) {
    LOG(ERROR) << "Failed to obtain shared image for encoding: "
               << PrintHr(hrResult);
    NotifyErrorStatus({EncoderStatus::Codes::kSystemAPICallError,
                       "Failed to obtain shared image for encoding"});
    return;
  }

  // If the encoding client quickly supplies multiple shared texture
  // frames, there could be multiple shared images being resolved at
  // the same time.  This sample needs to be linked with the correct
  // frame in the queue.  In some circumstances, the client may supply
  // multiple VideoFrames backed by the same mailbox, so ensure a
  // queued frame being resolved gets completed.
  auto it = pending_input_queue_.begin();
  for (; it != pending_input_queue_.end(); it++) {
    if (it->shared_image_token == frame->shared_image()->mailbox() &&
        it->resolving_shared_image) {
      it->input_sample = sample;
      it->resolving_shared_image = false;
      break;
    }
  }
  DCHECK(it != pending_input_queue_.end());

  HRESULT hr = PopulateInputSampleBuffer(*it, std::move(frame));
  if (FAILED(hr)) {
    NotifyErrorStatus({EncoderStatus::Codes::kEncoderFailedEncode,
                       "Failed to populate input sample buffer"});
    return;
  }

  if (encoder_needs_input_counter_ > 0) {
    FeedInputs();
  }
}

bool MediaFoundationVideoEncodeAccelerator::InitMFVideoProcessor() {
  mf_video_processor_ =
      std::make_unique<MediaFoundationVideoProcessorAccelerator>(
          gpu_preferences_, workarounds_);
  bool initialized = false;
  MediaFoundationVideoProcessorAccelerator::Config vp_config;
  vp_config.input_format = input_format_;
  vp_config.input_visible_size = input_visible_size_;
  // Primaries information is provided per frame and will be
  // attached to the corresponding IMFSample.  This color
  // space information now serves as a default if frame
  // primaries are unknown.
  vp_config.input_color_space = gfx::ColorSpace::CreateREC709();
  vp_config.output_format = VideoPixelFormat::PIXEL_FORMAT_NV12;
  vp_config.output_visible_size = input_visible_size_;
  vp_config.output_color_space = gfx::ColorSpace::CreateREC709();
  if (dxgi_resource_mapping_required_) {
    initialized = mf_video_processor_->Initialize(vp_config, nullptr,
                                                  media_log_->Clone());
  } else {
    initialized = mf_video_processor_->Initialize(
        vp_config, dxgi_device_manager_, media_log_->Clone());
  }
  return initialized;
}

HRESULT MediaFoundationVideoEncodeAccelerator::GetParameters(DWORD* pdwFlags,
                                                             DWORD* pdwQueue) {
  *pdwFlags = MFASYNC_FAST_IO_PROCESSING_CALLBACK;
  *pdwQueue = MFASYNC_CALLBACK_QUEUE_TIMER;
  return S_OK;
}

HRESULT MediaFoundationVideoEncodeAccelerator::Invoke(
    IMFAsyncResult* pAsyncResult) {
  ComMFMediaEvent media_event;
  RETURN_IF_FAILED(event_generator_->EndGetEvent(pAsyncResult, &media_event));

  MediaEventType event_type = MEUnknown;
  RETURN_IF_FAILED(media_event->GetType(&event_type));

  HRESULT status = S_OK;
  media_event->GetStatus(&status);

  // Invoke() is called on some random OS thread, so we must post to our event
  // handler since MediaFoundationVideoEncodeAccelerator is single threaded.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaFoundationVideoEncodeAccelerator::MediaEventHandler,
                     weak_ptr_, event_type, status));
  return status;
}

ULONG MediaFoundationVideoEncodeAccelerator::AddRef() {
  return async_callback_ref_.Increment();
}

ULONG MediaFoundationVideoEncodeAccelerator::Release() {
  DCHECK(!async_callback_ref_.IsOne());
  return async_callback_ref_.Decrement() ? 1 : 0;
}

HRESULT MediaFoundationVideoEncodeAccelerator::QueryInterface(REFIID riid,
                                                              void** ppv) {
  static const QITAB kQI[] = {
      QITABENT(MediaFoundationVideoEncodeAccelerator, IMFAsyncCallback), {0}};
  return QISearch(this, kQI, riid, ppv);
}

}  // namespace media
