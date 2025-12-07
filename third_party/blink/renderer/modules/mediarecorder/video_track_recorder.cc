// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/video_track_recorder.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/bind_post_task.h"
#include "base/task/common/task_annotator.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/paint/skia_paint_canvas.h"
#include "media/base/async_destroy_video_encoder.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/supported_types.h"
#include "media/base/video_codecs.h"
#include "media/base/video_encoder_metrics_provider.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/media_buildflags.h"
#include "media/muxers/muxer.h"
#include "media/muxers/webm_muxer.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/video_encode_accelerator_adapter.h"
#include "media/video/video_encoder_info.h"
#include "media/video/vpx_video_encoder.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/modules/mediarecorder/media_recorder_encoder_wrapper.h"
#include "third_party/blink/renderer/modules/mediarecorder/media_recorder_handler.h"
#include "third_party/blink/renderer/modules/mediarecorder/track_recorder.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_util.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/bind_post_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_media.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/sequence_bound.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(ENABLE_OPENH264)
#include "media/video/openh264_video_encoder.h"
#endif  // #if BUILDFLAG(ENABLE_OPENH264)

#if BUILDFLAG(ENABLE_LIBAOM)
#include "media/video/av1_video_encoder.h"
#endif  // BUILDFLAG(ENABLE_LIBAOM)

using video_track_recorder::kVEAEncoderMinResolutionHeight;
using video_track_recorder::kVEAEncoderMinResolutionWidth;

namespace blink {

// Helper class used to bless annotation of our calls to
// CreateOffscreenGraphicsContext3DProvider using ScopedAllowBaseSyncPrimitives.
class VideoTrackRecorderImplContextProvider {
 public:
  static std::unique_ptr<WebGraphicsContext3DProvider>
  CreateOffscreenGraphicsContext(const KURL& url) {
    base::ScopedAllowBaseSyncPrimitives allow;
    return CreateRasterGraphicsContextProvider(
        url, Platform::RasterContextType::kVideoTrackRecorder);
  }
};


libyuv::RotationMode MediaVideoRotationToRotationMode(
    media::VideoRotation rotation) {
  switch (rotation) {
    case media::VIDEO_ROTATION_0:
      return libyuv::kRotate0;
    case media::VIDEO_ROTATION_90:
      return libyuv::kRotate90;
    case media::VIDEO_ROTATION_180:
      return libyuv::kRotate180;
    case media::VIDEO_ROTATION_270:
      return libyuv::kRotate270;
  }
  NOTREACHED() << rotation;
}

namespace {

constexpr MediaTrackContainerType kVp8Types[] = {
    MediaTrackContainerType::kVideoMatroska,
    MediaTrackContainerType::kVideoWebM};
constexpr MediaTrackContainerType kVp9Types[] = {
    MediaTrackContainerType::kVideoMatroska,
    MediaTrackContainerType::kVideoWebM, MediaTrackContainerType::kVideoMp4};
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
constexpr MediaTrackContainerType kH264Types[] = {
    MediaTrackContainerType::kVideoMatroska,
    MediaTrackContainerType::kVideoMp4};
#endif
constexpr MediaTrackContainerType kAv1Types[] = {
    MediaTrackContainerType::kVideoWebM,
    MediaTrackContainerType::kVideoMatroska,
    MediaTrackContainerType::kVideoMp4};
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
constexpr MediaTrackContainerType kH265Types[] = {
    MediaTrackContainerType::kVideoMatroska,
    MediaTrackContainerType::kVideoMp4};
#endif

constexpr struct {
  media::VideoCodec codec;
  media::VideoCodecProfile min_profile;
  media::VideoCodecProfile max_profile;
  base::raw_span<const MediaTrackContainerType> supported_container_types;
} kPreferredCodecAndVEAProfiles[] = {
    {media::VideoCodec::kVP8, media::VP8PROFILE_ANY, media::VP8PROFILE_ANY,
     base::span{kVp8Types}},
    {media::VideoCodec::kVP9, media::VP9PROFILE_PROFILE0,
     media::VP9PROFILE_PROFILE0, base::span{kVp9Types}},
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    {media::VideoCodec::kH264, media::H264PROFILE_BASELINE,
     media::H264PROFILE_HIGH, base::span{kH264Types}},
#endif
    {media::VideoCodec::kAV1, media::AV1PROFILE_PROFILE_MAIN,
     media::AV1PROFILE_PROFILE_MAIN, base::span{kAv1Types}},
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    {media::VideoCodec::kHEVC, media::HEVCPROFILE_MAIN, media::HEVCPROFILE_MAIN,
     base::span{kH265Types}},
#endif
};

void NotifyEncoderSupportKnown(base::OnceClosure callback) {
  if (!Platform::Current()) {
    DLOG(ERROR) << "Couldn't access the render thread";
    std::move(callback).Run();
    return;
  }

  media::GpuVideoAcceleratorFactories* const gpu_factories =
      Platform::Current()->GetGpuFactories();
  if (!gpu_factories || !gpu_factories->IsGpuVideoEncodeAcceleratorEnabled()) {
    DLOG(ERROR) << "Couldn't initialize GpuVideoAcceleratorFactories";
    std::move(callback).Run();
    return;
  }

  gpu_factories->NotifyEncoderSupportKnown(std::move(callback));
}

// Obtains video encode accelerator's supported profiles.
media::VideoEncodeAccelerator::SupportedProfiles GetVEASupportedProfiles() {
  if (!Platform::Current()) {
    DLOG(ERROR) << "Couldn't access the render thread";
    return media::VideoEncodeAccelerator::SupportedProfiles();
  }

  media::GpuVideoAcceleratorFactories* const gpu_factories =
      Platform::Current()->GetGpuFactories();
  if (!gpu_factories || !gpu_factories->IsGpuVideoEncodeAcceleratorEnabled()) {
    DLOG(ERROR) << "Couldn't initialize GpuVideoAcceleratorFactories";
    return media::VideoEncodeAccelerator::SupportedProfiles();
  }
  return gpu_factories->GetVideoEncodeAcceleratorSupportedProfiles().value_or(
      media::VideoEncodeAccelerator::SupportedProfiles());
}

void UmaHistogramForCodecImpl(bool uses_acceleration, media::VideoCodec codec) {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // (kMaxValue being the only exception, as it does not map to a logged value,
  // and should be renumbered as new values are inserted.)
  enum class VideoTrackRecorderCodecImplHistogram : uint8_t {
    kUnknown = 0,
    kVp8Sw = 1,
    kVp8Hw = 2,
    kVp9Sw = 3,
    kVp9Hw = 4,
    kH264Sw = 5,
    kH264Hw = 6,
    kAv1Sw = 7,
    kAv1Hw = 8,
    kHevcHw = 9,
    kMaxValue = kHevcHw,
  };
  auto histogram = VideoTrackRecorderCodecImplHistogram::kUnknown;
  if (uses_acceleration) {
    switch (codec) {
      case media::VideoCodec::kVP8:
        histogram = VideoTrackRecorderCodecImplHistogram::kVp8Hw;
        break;
      case media::VideoCodec::kVP9:
        histogram = VideoTrackRecorderCodecImplHistogram::kVp9Hw;
        break;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
      case media::VideoCodec::kH264:
        histogram = VideoTrackRecorderCodecImplHistogram::kH264Hw;
        break;
#endif
      case media::VideoCodec::kAV1:
        histogram = VideoTrackRecorderCodecImplHistogram::kAv1Hw;
        break;
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
      case media::VideoCodec::kHEVC:
        histogram = VideoTrackRecorderCodecImplHistogram::kHevcHw;
        break;
#endif
      default:
        break;
    }
  } else {
    switch (codec) {
      case media::VideoCodec::kVP8:
        histogram = VideoTrackRecorderCodecImplHistogram::kVp8Sw;
        break;
      case media::VideoCodec::kVP9:
        histogram = VideoTrackRecorderCodecImplHistogram::kVp9Sw;
        break;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
      case media::VideoCodec::kH264:
        histogram = VideoTrackRecorderCodecImplHistogram::kH264Sw;
        break;
#endif
      case media::VideoCodec::kAV1:
        histogram = VideoTrackRecorderCodecImplHistogram::kAv1Sw;
        break;
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
      case media::VideoCodec::kHEVC:
        break;
#endif
      default:
        break;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("Media.MediaRecorder.Codec", histogram);
}

// Returns the default codec profile for |codec_id|.
std::optional<media::VideoCodecProfile> GetMediaVideoCodecProfileForSwEncoder(
    media::VideoCodec codec) {
  switch (codec) {
    case media::VideoCodec::kH264:
      return media::IsOpenH264SoftwareEncoderEnabled()
                 ? std::optional(media::H264PROFILE_BASELINE)
                 : std::nullopt;
    case media::VideoCodec::kVP8:
      return media::VP8PROFILE_ANY;
    case media::VideoCodec::kVP9:
      return media::VP9PROFILE_MIN;
#if BUILDFLAG(ENABLE_LIBAOM)
    case media::VideoCodec::kAV1:
      return media::AV1PROFILE_MIN;
#endif  // BUILDFLAG(ENABLE_LIBAOM)
    default:
      return std::nullopt;
  }
}

bool IsSoftwareEncoderAvailable(media::VideoCodec codec) {
  return GetMediaVideoCodecProfileForSwEncoder(codec).has_value();
}

std::optional<media::VideoCodecProfile> GetMediaVideoCodecProfile(
    VideoTrackRecorder::CodecProfile codec_profile,
    const gfx::Size& input_size,
    bool allow_vea_encoder) {
  const bool can_use_vea = VideoTrackRecorderImpl::CanUseAcceleratedEncoder(
      codec_profile, input_size.width(), input_size.height());
  if (can_use_vea && allow_vea_encoder) {
    // Hardware encoder will be used.
    // If |codec_profile.profile| is specified by a client, then the returned
    // profile is the same as it.
    // Otherwise, CanUseAcceleratedEncoder() fills the codec profile available
    // with a hardware encoder.
    CHECK(codec_profile.profile.has_value());
    return codec_profile.profile;
  } else if (!IsSoftwareEncoderAvailable(codec_profile.codec)) {
    LOG(ERROR) << "Can't use VEA, but must be able to use VEA, codec="
               << static_cast<int>(codec_profile.codec);
    return std::nullopt;
  }
  // Software encoder will be used.
  return codec_profile.profile.value_or(
      GetMediaVideoCodecProfileForSwEncoder(codec_profile.codec).value());
}

MediaRecorderEncoderWrapper::CreateEncoderCB
GetCreateHardwareVideoEncoderCallback(
    media::VideoCodec codec,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  CHECK(gpu_factories);
  auto required_encoder_type =
      media::MayHaveAndAllowSelectOSSoftwareEncoder(codec)
          ? media::VideoEncodeAccelerator::Config::EncoderType::kNoPreference
          : media::VideoEncodeAccelerator::Config::EncoderType::kHardware;
  return CrossThreadBindRepeating(
      [](media::VideoEncodeAccelerator::Config::EncoderType
             required_encoder_type,
         media::GpuVideoAcceleratorFactories* gpu_factories)
          -> std::unique_ptr<media::VideoEncoder> {
        return std::make_unique<media::AsyncDestroyVideoEncoder<
            media::VideoEncodeAcceleratorAdapter>>(
            std::make_unique<media::VideoEncodeAcceleratorAdapter>(
                gpu_factories, std::make_unique<media::NullMediaLog>(),
                base::SequencedTaskRunner::GetCurrentDefault(),
                required_encoder_type));
      },
      required_encoder_type, CrossThreadUnretained(gpu_factories));
}

MediaRecorderEncoderWrapper::CreateEncoderCB
GetCreateSoftwareVideoEncoderCallback(media::VideoCodec codec) {
  switch (codec) {
#if BUILDFLAG(ENABLE_OPENH264)
    case media::VideoCodec::kH264:
      return CrossThreadBindRepeating(
          []() -> std::unique_ptr<media::VideoEncoder> {
            return std::make_unique<media::OpenH264VideoEncoder>();
          });
#endif  // BUILDFLAG(ENABLE_OPENH264)
#if BUILDFLAG(ENABLE_LIBVPX)
    case media::VideoCodec::kVP8:
    case media::VideoCodec::kVP9:
      return CrossThreadBindRepeating(
          []() -> std::unique_ptr<media::VideoEncoder> {
            return std::make_unique<media::VpxVideoEncoder>();
          });
#endif
#if BUILDFLAG(ENABLE_LIBAOM)
    case media::VideoCodec::kAV1:
      return CrossThreadBindRepeating(
          []() -> std::unique_ptr<media::VideoEncoder> {
            return std::make_unique<media::Av1VideoEncoder>();
          });
#endif  // BUILDFLAG(ENABLE_LIBAOM)
    default:
      NOTREACHED() << "Unsupported codec=" << static_cast<int>(codec);
  }
}

std::optional<media::VideoTransformation> GetFrameTransformation(
    scoped_refptr<media::VideoFrame> video_frame) {
  if (const auto& transformation = video_frame->metadata().transformation) {
    return *transformation;
  }
  return std::nullopt;
}
}  // anonymous namespace

// static
VideoTrackRecorder::CodecHistogram VideoTrackRecorder::CodecHistogramFromCodec(
    media::VideoCodec codec) {
  switch (codec) {
    case media::VideoCodec::kVP8:
      return CodecHistogram::kVp8;
    case media::VideoCodec::kVP9:
      return CodecHistogram::kVp9;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    case media::VideoCodec::kH264:
      return CodecHistogram::kH264;
#endif
    case media::VideoCodec::kAV1:
      return CodecHistogram::kAv1;
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    case media::VideoCodec::kHEVC:
      return CodecHistogram::kHevc;
#endif
    default:
      return CodecHistogram::kUnknown;
  }
}

VideoTrackRecorder::VideoTrackRecorder(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    WeakCell<CallbackInterface>* callback_interface)
    : TrackRecorder(base::BindPostTask(
          main_thread_task_runner,
          BindOnce(&CallbackInterface::OnSourceReadyStateChanged,
                   WrapPersistent(callback_interface)))),
      main_thread_task_runner_(std::move(main_thread_task_runner)),
      callback_interface_(callback_interface) {
  CHECK(main_thread_task_runner_);
}

VideoTrackRecorderImpl::CodecProfile::CodecProfile(media::VideoCodec codec)
    : codec(codec) {}

VideoTrackRecorderImpl::CodecProfile::CodecProfile(
    media::VideoCodec codec,
    std::optional<media::VideoCodecProfile> opt_profile,
    std::optional<media::VideoCodecLevel> opt_level)
    : codec(codec), profile(opt_profile), level(opt_level) {}

VideoTrackRecorderImpl::CodecProfile::CodecProfile(
    media::VideoCodec codec,
    media::VideoCodecProfile profile,
    media::VideoCodecLevel level)
    : codec(codec), profile(profile), level(level) {}

VideoTrackRecorderImpl::Counter::Counter() : count_(0u) {}

VideoTrackRecorderImpl::Counter::~Counter() = default;

void VideoTrackRecorderImpl::Counter::IncreaseCount() {
  count_++;
}

void VideoTrackRecorderImpl::Counter::DecreaseCount() {
  count_--;
}

base::WeakPtr<VideoTrackRecorderImpl::Counter>
VideoTrackRecorderImpl::Counter::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

VideoTrackRecorderImpl::Encoder::Encoder(
    scoped_refptr<base::SequencedTaskRunner> encoding_task_runner,
    OnEncodedVideoCB on_encoded_video_cb,
    uint32_t bits_per_second)
    : encoding_task_runner_(std::move(encoding_task_runner)),
      on_encoded_video_cb_(std::move(on_encoded_video_cb)),
      bits_per_second_(bits_per_second),
      num_frames_in_encode_(
          std::make_unique<VideoTrackRecorderImpl::Counter>()) {
  CHECK(encoding_task_runner_);
  DCHECK(on_encoded_video_cb_);
}

VideoTrackRecorderImpl::Encoder::~Encoder() = default;

void VideoTrackRecorderImpl::Encoder::InitializeEncoder(
    KeyFrameRequestProcessor::Configuration key_frame_config,
    std::unique_ptr<media::VideoEncoderMetricsProvider> metrics_provider,
    size_t frame_buffer_pool_limit) {
  key_frame_processor_.UpdateConfig(key_frame_config);
  metrics_provider_ = std::move(metrics_provider);
  frame_buffer_pool_limit_ = frame_buffer_pool_limit;
  Initialize();
}

void VideoTrackRecorderImpl::Encoder::Initialize() {}

void VideoTrackRecorderImpl::Encoder::StartFrameEncode(
    scoped_refptr<media::VideoFrame> video_frame,
    base::TimeTicks capture_timestamp) {
  TRACE_EVENT("media", "Encoder::StartFrameEncode");
  if (paused_) {
    return;
  }
  auto timestamp = video_frame->metadata().capture_begin_time.value_or(
      video_frame->metadata().reference_time.value_or(capture_timestamp));
  bool force_key_frame =
      awaiting_first_frame_ ||
      key_frame_processor_.OnFrameAndShouldRequestKeyFrame(timestamp);
  if (force_key_frame) {
    key_frame_processor_.OnKeyFrame(timestamp);
  }
  awaiting_first_frame_ = false;

  if (num_frames_in_encode_->count() > max_number_of_frames_in_encode_) {
    LOCAL_HISTOGRAM_BOOLEAN("Media.MediaRecorder.DroppingFrameTooManyInEncode",
                            true);
    DLOG(WARNING) << "Too many frames are queued up. Dropping this one.";
    return;
  }

  const bool is_format_supported =
      (video_frame->HasMappableSharedImage() &&
       video_frame->format() == media::PIXEL_FORMAT_NV12) ||
      (video_frame->IsMappable() &&
       (video_frame->format() == media::PIXEL_FORMAT_NV12 ||
        video_frame->format() == media::PIXEL_FORMAT_I420 ||
        video_frame->format() == media::PIXEL_FORMAT_I420A));
  scoped_refptr<media::VideoFrame> frame = std::move(video_frame);
  // First, pixel format is converted to NV12, I420 or I420A.
  if (!is_format_supported) {
    frame = MaybeProvideEncodableFrame(std::move(frame));
  }
  if (frame && frame->format() == media::PIXEL_FORMAT_I420A &&
      !CanEncodeAlphaChannel()) {
    CHECK(!frame->HasMappableSharedImage());
    // Drop alpha channel if the encoder does not support it yet.
    frame = media::WrapAsI420VideoFrame(std::move(frame));
  }

  if (!frame) {
    // Explicit reasons for the frame drop are already logged.
    return;
  }
  frame->AddDestructionObserver(base::BindPostTask(
      encoding_task_runner_,
      blink::BindOnce(&VideoTrackRecorderImpl::Counter::DecreaseCount,
                      num_frames_in_encode_->GetWeakPtr())));
  num_frames_in_encode_->IncreaseCount();
  TRACE_EVENT_INSTANT("media", "PreEncodeFrame", "converted",
                      !is_format_supported);
  EncodeFrame(std::move(frame), timestamp,
              request_key_frame_for_testing_ || force_key_frame);
  request_key_frame_for_testing_ = false;
}

void VideoTrackRecorderImpl::Encoder::OnVideoEncoderInfo(
    const media::VideoEncoderInfo& encoder_info) {
  if (!encoder_info.frame_delay.has_value()) {
    max_number_of_frames_in_encode_ = kMaxNumberOfFramesInEncoderMinValue;
    return;
  }

  // The maximum number of input frames above the encoder frame delay that we
  // want to be able to enqueue---to account for IPC, etc.
  constexpr int kDefaultEncoderExtraInputCapacity = 2;

  const int preferred_capacity =
      encoder_info.frame_delay.value() + kDefaultEncoderExtraInputCapacity;
  max_number_of_frames_in_encode_ =
      encoder_info.input_capacity.has_value()
          ? std::min(preferred_capacity, encoder_info.input_capacity.value())
          : preferred_capacity;
  CHECK_GE(frame_buffer_pool_limit_, max_number_of_frames_in_encode_)
      << "The video capture buffer pool is too small for this encoder: "
      << encoder_info.implementation_name;
}

scoped_refptr<media::VideoFrame>
VideoTrackRecorderImpl::Encoder::MaybeProvideEncodableFrame(
    scoped_refptr<media::VideoFrame> video_frame) {
  DVLOG(3) << __func__;
  scoped_refptr<media::VideoFrame> frame;
  const bool is_opaque = media::IsOpaque(video_frame->format());
  if (media::IsRGB(video_frame->format()) && video_frame->IsMappable()) {
    // It's a mapped RGB frame, no readback needed,
    // all we need is to convert RGB to I420
    auto visible_rect = video_frame->visible_rect();
    frame = frame_pool_.CreateFrame(
        is_opaque ? media::PIXEL_FORMAT_I420 : media::PIXEL_FORMAT_I420A,
        visible_rect.size(), visible_rect, visible_rect.size(),
        video_frame->timestamp());
    if (!frame ||
        !frame_converter_.ConvertAndScale(*video_frame, *frame).is_ok()) {
      // Send black frames (yuv = {0, 127, 127}).
      DLOG(ERROR) << "Can't convert RGB to I420 - producing black frame";
      frame = media::VideoFrame::CreateColorFrame(
          video_frame->visible_rect().size(), 0u, 0x80, 0x80,
          video_frame->timestamp());
    }
    return frame;
  }

  // |encoder_thread_context_| is null if the GPU process has crashed or isn't
  // there
  if (!encoder_thread_context_) {
    // PaintCanvasVideoRenderer requires these settings to work.
    encoder_thread_context_ =
        VideoTrackRecorderImplContextProvider::CreateOffscreenGraphicsContext(
            KURL("chrome://VideoTrackRecorderImpl"));

    if (encoder_thread_context_ &&
        !encoder_thread_context_->BindToCurrentSequence()) {
      encoder_thread_context_ = nullptr;
    }
  }

  if (!encoder_thread_context_) {
    DLOG(ERROR) << "Can't create offscreen graphics canvas context - producing "
                   "black frame";
    // Send black frames (yuv = {0, 127, 127}).
    frame = media::VideoFrame::CreateColorFrame(
        video_frame->visible_rect().size(), 0u, 0x80, 0x80,
        video_frame->timestamp());
  } else {
    // Accelerated decoders produce ARGB/ABGR texture-backed frames (see
    // https://crbug.com/585242), fetch them using a PaintCanvasVideoRenderer.
    // Additionally, macOS accelerated decoders can produce XRGB content
    // and are treated the same way.
    //
    // This path is also used for less common formats like I422, I444, and
    // high bit depth pixel formats.

    const gfx::Size& old_visible_size = video_frame->visible_rect().size();
    gfx::Size new_visible_size = old_visible_size;

    frame = frame_pool_.CreateFrame(
        is_opaque ? media::PIXEL_FORMAT_I420 : media::PIXEL_FORMAT_I420A,
        new_visible_size, gfx::Rect(new_visible_size), new_visible_size,
        video_frame->timestamp());

    frame->metadata().MergeMetadataFrom(video_frame->metadata());
    frame->metadata().ClearTextureFrameMetadata();

    const SkImageInfo info = SkImageInfo::MakeN32(
        frame->visible_rect().width(), frame->visible_rect().height(),
        is_opaque ? kOpaque_SkAlphaType : kPremul_SkAlphaType);

    // Create |surface_| if it doesn't exist or incoming resolution has
    // changed.
    if (!canvas_ || canvas_->imageInfo().width() != info.width() ||
        canvas_->imageInfo().height() != info.height()) {
      bitmap_.allocPixels(info);
      canvas_ = std::make_unique<cc::SkiaPaintCanvas>(bitmap_);
    }
    if (!video_renderer_) {
      video_renderer_ = std::make_unique<media::PaintCanvasVideoRenderer>();
    }

    video_renderer_->Copy(video_frame.get(), canvas_.get(),
                          encoder_thread_context_->RasterContextProvider());

    SkPixmap pixmap;
    if (!bitmap_.peekPixels(&pixmap)) {
      DLOG(ERROR) << "Error trying to map PaintSurface's pixels";
      return nullptr;
    }

#if SK_PMCOLOR_BYTE_ORDER(R, G, B, A)
    const uint32_t source_pixel_format = libyuv::FOURCC_ABGR;
#else
    const uint32_t source_pixel_format = libyuv::FOURCC_ARGB;
#endif
    if (libyuv::ConvertToI420(
            static_cast<uint8_t*>(pixmap.writable_addr()),
            pixmap.computeByteSize(),
            frame->GetWritableVisibleData(media::VideoFrame::Plane::kY),
            frame->stride(media::VideoFrame::Plane::kY),
            frame->GetWritableVisibleData(media::VideoFrame::Plane::kU),
            frame->stride(media::VideoFrame::Plane::kU),
            frame->GetWritableVisibleData(media::VideoFrame::Plane::kV),
            frame->stride(media::VideoFrame::Plane::kV), 0 /* crop_x */,
            0 /* crop_y */, pixmap.width(), pixmap.height(),
            old_visible_size.width(), old_visible_size.height(),
            libyuv::kRotate0, source_pixel_format) != 0) {
      DLOG(ERROR) << "Error converting frame to I420";
      return nullptr;
    }
    if (!is_opaque) {
      // Alpha has the same alignment for both ABGR and ARGB.
      libyuv::ARGBExtractAlpha(
          static_cast<uint8_t*>(pixmap.writable_addr()),
          static_cast<int>(pixmap.rowBytes()) /* stride */,
          frame->GetWritableVisibleData(media::VideoFrame::Plane::kA),
          frame->stride(media::VideoFrame::Plane::kA), pixmap.width(),
          pixmap.height());
    }
  }
  return frame;
}

void VideoTrackRecorderImpl::Encoder::SetPaused(bool paused) {
  paused_ = paused;
}

bool VideoTrackRecorderImpl::Encoder::CanEncodeAlphaChannel() const {
  return false;
}

scoped_refptr<media::VideoFrame>
VideoTrackRecorderImpl::Encoder::ConvertToI420ForSoftwareEncoder(
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK_EQ(frame->format(), media::VideoPixelFormat::PIXEL_FORMAT_NV12);

  if (frame->HasMappableSharedImage()) {
    frame = media::ConvertToMemoryMappedFrame(frame);
  }
  if (!frame) {
    return nullptr;
  }

  scoped_refptr<media::VideoFrame> i420_frame = frame_pool_.CreateFrame(
      media::VideoPixelFormat::PIXEL_FORMAT_I420, frame->coded_size(),
      frame->visible_rect(), frame->natural_size(), frame->timestamp());
  auto ret = libyuv::NV12ToI420(
      frame->data(0), frame->stride(0), frame->data(1), frame->stride(1),
      i420_frame->writable_data(media::VideoFrame::Plane::kY),
      i420_frame->stride(media::VideoFrame::Plane::kY),
      i420_frame->writable_data(media::VideoFrame::Plane::kU),
      i420_frame->stride(media::VideoFrame::Plane::kU),
      i420_frame->writable_data(media::VideoFrame::Plane::kV),
      i420_frame->stride(media::VideoFrame::Plane::kV),
      frame->coded_size().width(), frame->coded_size().height());
  if (ret) {
    return frame;
  }
  return i420_frame;
}

// static
media::VideoCodec VideoTrackRecorderImpl::GetPreferredCodec(
    MediaTrackContainerType type) {
  for (const auto& supported_profile : GetVEASupportedProfiles()) {
    const media::VideoCodecProfile codec_profile = supported_profile.profile;
    for (auto& entry : kPreferredCodecAndVEAProfiles) {
      if (codec_profile >= entry.min_profile &&
          codec_profile <= entry.max_profile &&
          std::find(entry.supported_container_types.begin(),
                    entry.supported_container_types.end(),
                    type) != entry.supported_container_types.end()) {
        DVLOG(2) << "Accelerated codec found: "
                 << media::GetProfileName(codec_profile) << ", min_resolution: "
                 << supported_profile.min_resolution.ToString()
                 << ", max_resolution: "
                 << supported_profile.max_resolution.ToString()
                 << ", max_framerate: "
                 << supported_profile.max_framerate_numerator << "/"
                 << supported_profile.max_framerate_denominator;
        return entry.codec;
      }
    }
  }

  if (type == MediaTrackContainerType::kVideoMp4 ||
      type == MediaTrackContainerType::kAudioMp4) {
    return media::VideoCodec::kVP9;
  }

  return media::VideoCodec::kVP8;
}

// static
bool VideoTrackRecorderImpl::CanUseAcceleratedEncoder(
    CodecProfile& codec_profile,
    size_t width,
    size_t height,
    double framerate) {
  if (IsSoftwareEncoderAvailable(codec_profile.codec)) {
    if (width < kVEAEncoderMinResolutionWidth) {
      return false;
    }
    if (height < kVEAEncoderMinResolutionHeight) {
      return false;
    }
  }

  for (const auto& profile : GetVEASupportedProfiles()) {
    DCHECK_NE(profile.profile, media::VIDEO_CODEC_PROFILE_UNKNOWN);

    // Skip other profiles if the profile is specified or skip on codec.
    if ((codec_profile.profile && *codec_profile.profile != profile.profile) ||
        codec_profile.codec !=
            media::VideoCodecProfileToVideoCodec(profile.profile)) {
      continue;
    }

    // Skip if profile is OS software encoder profile and we don't allow use
    // OS software encoder.
    if (profile.is_software_codec &&
        !media::MayHaveAndAllowSelectOSSoftwareEncoder(
            media::VideoCodecProfileToVideoCodec(profile.profile))) {
      continue;
    }

    const gfx::Size& min_resolution = profile.min_resolution;
    DCHECK_GE(min_resolution.width(), 0);
    const size_t min_width = static_cast<size_t>(min_resolution.width());
    DCHECK_GE(min_resolution.height(), 0);
    const size_t min_height = static_cast<size_t>(min_resolution.height());

    const gfx::Size& max_resolution = profile.max_resolution;
    DCHECK_GE(max_resolution.width(), 0);
    const size_t max_width = static_cast<size_t>(max_resolution.width());
    DCHECK_GE(max_resolution.height(), 0);
    const size_t max_height = static_cast<size_t>(max_resolution.height());

    const bool width_within_range = max_width >= width && width >= min_width;
    const bool height_within_range =
        max_height >= height && height >= min_height;

    const bool valid_framerate =
        framerate * profile.max_framerate_denominator <=
        profile.max_framerate_numerator;

    if (width_within_range && height_within_range && valid_framerate) {
      // Record with the first found profile that satisfies the condition.
      codec_profile.profile = profile.profile;
      return true;
    }
  }
  return false;
}

VideoTrackRecorderImpl::VideoTrackRecorderImpl(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    CodecProfile codec_profile,
    MediaStreamComponent* track,
    WeakCell<CallbackInterface>* callback_interface,
    uint32_t bits_per_second,
    KeyFrameRequestProcessor::Configuration key_frame_config,
    size_t frame_buffer_pool_limit)
    : VideoTrackRecorder(std::move(main_thread_task_runner),
                         callback_interface),
      track_(track),
      key_frame_config_(key_frame_config),
      codec_profile_(codec_profile),
      bits_per_second_(bits_per_second),
      frame_buffer_pool_limit_(frame_buffer_pool_limit) {
  TRACE_EVENT("media", "VideoTrackRecorderImpl::VideoTrackRecorderImpl");
  CHECK(main_thread_task_runner_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(track_);
  DCHECK(track_->GetSourceType() == MediaStreamSource::kTypeVideo);

  // Start querying for encoder support known.
  NotifyEncoderSupportKnown(
      blink::BindOnce(&VideoTrackRecorderImpl::OnEncoderSupportKnown,
                      weak_factory_.GetWeakPtr()));

  // OnVideoFrame() will be called on Render Main thread.
  ConnectToTrack(base::BindPostTask(
      main_thread_task_runner_,
      blink::BindRepeating(&VideoTrackRecorderImpl::OnVideoFrame,
                           weak_factory_.GetWeakPtr(),
                           /*allow_vea_encoder=*/true)));
}

VideoTrackRecorderImpl::~VideoTrackRecorderImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DisconnectFromTrack();

  UMA_HISTOGRAM_COUNTS_100("Media.MediaRecorder.TrackTransformationChangeCount",
                           num_video_transformation_changes_);
}

void VideoTrackRecorderImpl::OnEncoderSupportKnown() {
  TRACE_EVENT("media", "VideoTrackRecorderImpl::OnEncoderSupportKnown");
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  encoder_support_known_ = true;

  // Flush out stored frames.
  for (auto& frame_reference : incoming_frame_queue_) {
    auto media_stream_frame = std::move(frame_reference);
    // As we ask for support only initially when we try to use VEA, no frames
    // have been encoded (hence no fallback attempt has been made). Hence it's
    // safe to pass true in `allow_vea_encoder`.
    ProcessOneVideoFrame(/*allow_vea_encoder=*/true,
                         std::move(media_stream_frame.video_frame),
                         media_stream_frame.estimated_capture_time);
  }
  incoming_frame_queue_.clear();
}

void VideoTrackRecorderImpl::OnVideoFrame(
    bool allow_vea_encoder,
    scoped_refptr<media::VideoFrame> video_frame,
    base::TimeTicks capture_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  TRACE_EVENT("media", "VideoTrackRecorderImpl::OnVideoFrame");
  CHECK(video_frame);

  // Measure how common video transformation changes are, crbug.com/391786486.
  auto frame_transformation = GetFrameTransformation(video_frame);
  if (num_video_transformation_changes_ == 0) {
    num_video_transformation_changes_ = 1;
  } else if (last_transformation_ != frame_transformation) {
    ++num_video_transformation_changes_;
  }
  last_transformation_ = frame_transformation;

  if (encoder_support_known_) {
    ProcessOneVideoFrame(allow_vea_encoder, std::move(video_frame),
                         capture_time);
  } else {
    // Return if encoder support isn't yet known. There's no limit of queued
    // frames implemented. In case it takes time for NotifyEncoderSupportKnown
    // to complete, the number of outstanding capture buffers is limited for
    // video capture and will eventually lead to the capturer stopping emitting
    // buffers. See
    // https://source.chromium.org/chromium/chromium/src/+/main:media/capture/video/video_capture_buffer_pool_util.cc.
    incoming_frame_queue_.push_back(
        MediaStreamFrame{.video_frame = std::move(video_frame),
                         .estimated_capture_time = capture_time});
  }
}

void VideoTrackRecorderImpl::ProcessOneVideoFrame(
    bool allow_vea_encoder,
    scoped_refptr<media::VideoFrame> video_frame,
    base::TimeTicks capture_time) {
  TRACE_EVENT("media", "VideoTrackRecorderImpl::ProcessOneVideoFrame");
  CHECK(video_frame);
  if (!encoder_) {
    InitializeEncoder(bits_per_second_, allow_vea_encoder,
                      video_frame->storage_type(),
                      video_frame->visible_rect().size());
  }
  if (encoder_) {
    encoder_.AsyncCall(&Encoder::StartFrameEncode)
        .WithArgs(video_frame, capture_time);
  }
}

void VideoTrackRecorderImpl::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  if (encoder_) {
    encoder_.AsyncCall(&Encoder::SetPaused).WithArgs(true);
  } else {
    should_pause_encoder_on_initialization_ = true;
  }
}

void VideoTrackRecorderImpl::Resume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  if (encoder_) {
    encoder_.AsyncCall(&Encoder::SetPaused).WithArgs(false);
  } else {
    should_pause_encoder_on_initialization_ = false;
  }
}

void VideoTrackRecorderImpl::OnVideoFrameForTesting(
    scoped_refptr<media::VideoFrame> frame,
    base::TimeTicks timestamp,
    bool allow_vea_encoder) {
  DVLOG(3) << __func__;
  OnVideoFrame(allow_vea_encoder, std::move(frame), timestamp);
}

void VideoTrackRecorderImpl::ForceKeyFrameForNextFrameForTesting() {
  encoder_.AsyncCall(&Encoder::ForceKeyFrameForNextFrameForTesting);
}

void VideoTrackRecorderImpl::CreateMediaVideoEncoder(
    scoped_refptr<base::SequencedTaskRunner> encoding_task_runner,
    CodecProfile codec_profile,
    bool is_screencast,
    bool create_vea_encoder) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  CHECK(codec_profile.profile.has_value());

  MediaRecorderEncoderWrapper::OnErrorCB on_error_cb;
  if (create_vea_encoder) {
    // If |on_error_cb| is called, then MediaRecorderEncoderWrapper with a
    // software encoder will be created.
    // TODO(crbug.com/1441395): This should be handled by using
    // media::VideoEncoderFallback. This should be achieved after refactoring
    // VideoTrackRecorder to call media::VideoEncoder directly.
    on_error_cb = BindPostTask(
        main_thread_task_runner_,
        CrossThreadBindOnce(&VideoTrackRecorderImpl::OnHardwareEncoderError,
                            weak_factory_.GetWeakPtr()));
  } else {
    on_error_cb = BindPostTask(
        main_thread_task_runner_,
        CrossThreadBindOnce(
            &CallbackInterface::OnVideoEncodingError,
            MakeUnwrappingCrossThreadHandle(callback_interface())));
  }

  encoder_ = SequenceBound<MediaRecorderEncoderWrapper>(
      encoding_task_runner, encoding_task_runner, *codec_profile.profile,
      bits_per_second_, is_screencast, create_vea_encoder,
      create_vea_encoder
          ? GetCreateHardwareVideoEncoderCallback(
                codec_profile.codec, Platform::Current()->GetGpuFactories())
          : GetCreateSoftwareVideoEncoderCallback(codec_profile.codec),
      BindPostTask(main_thread_task_runner_,
                   CrossThreadBindRepeating(
                       &CallbackInterface::OnEncodedVideo,
                       MakeUnwrappingCrossThreadHandle(callback_interface()))),
      std::move(on_error_cb));
}

void VideoTrackRecorderImpl::InitializeEncoder(
    uint32_t bits_per_second,
    bool allow_vea_encoder,
    media::VideoFrame::StorageType frame_storage_type,
    gfx::Size input_size) {
  TRACE_EVENT("media", "VideoTrackRecorderImpl::InitializeEncoder");
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  auto codec_profile = codec_profile_;
  const bool can_use_vea =
      CanUseAcceleratedEncoder(codec_profile, input_size.width(),
                               input_size.height()) &&
      Platform::Current()->GetGpuFactories();
  CHECK(callback_interface());

  std::optional<media::VideoCodecProfile> profile =
      GetMediaVideoCodecProfile(codec_profile, input_size, allow_vea_encoder);
  if (!profile) {
    if (auto* callback = callback_interface()->Get()) {
      callback->OnVideoEncodingError(
          media::EncoderStatus::Codes::kEncoderUnsupportedConfig);
    }
    return;
  }

  codec_profile.profile = *profile;

  const bool is_screencast =
      static_cast<const MediaStreamVideoTrack*>(track_->GetPlatformTrack())
          ->is_screencast();
  const bool create_vea_encoder = allow_vea_encoder && can_use_vea;
  auto encoding_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  CHECK(encoding_task_runner);
  UmaHistogramForCodecImpl(create_vea_encoder, codec_profile.codec);

  CreateMediaVideoEncoder(encoding_task_runner, codec_profile, is_screencast,
                          create_vea_encoder);
  CHECK(encoder_);

  auto metrics_provider =
      callback_interface()->Get()
          ? callback_interface()->Get()->CreateVideoEncoderMetricsProvider()
          : nullptr;

  encoder_.AsyncCall(&Encoder::InitializeEncoder)
      .WithArgs(key_frame_config_, std::move(metrics_provider),
                frame_buffer_pool_limit_);
  if (should_pause_encoder_on_initialization_) {
    encoder_.AsyncCall(&Encoder::SetPaused).WithArgs(true);
  }
}

void VideoTrackRecorderImpl::OnHardwareEncoderError(
    media::EncoderStatus error_status) {
  std::move(error_status).DebugLog(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // Try without VEA.
  DisconnectFromTrack();
  encoder_.Reset();
  ConnectToTrack(base::BindPostTask(
      main_thread_task_runner_,
      blink::BindRepeating(&VideoTrackRecorderImpl::OnVideoFrame,
                           weak_factory_.GetWeakPtr(),
                           /*allow_vea_encoder=*/false)));
}

void VideoTrackRecorderImpl::ConnectToTrack(
    const VideoCaptureDeliverFrameCB& callback) {
  track_->AddSink(this, callback, MediaStreamVideoSink::IsSecure::kNo,
                  MediaStreamVideoSink::UsesAlpha::kDefault);
}

void VideoTrackRecorderImpl::DisconnectFromTrack() {
  auto* video_track =
      static_cast<MediaStreamVideoTrack*>(track_->GetPlatformTrack());
  video_track->RemoveSink(this);
}

VideoTrackRecorderPassthrough::VideoTrackRecorderPassthrough(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    MediaStreamComponent* track,
    WeakCell<CallbackInterface>* callback_interface,
    KeyFrameRequestProcessor::Configuration key_frame_config)
    : VideoTrackRecorder(std::move(main_thread_task_runner),
                         callback_interface),
      track_(track),
      key_frame_processor_(key_frame_config) {
  CHECK(main_thread_task_runner_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // HandleEncodedVideoFrame() will be called on Render Main thread.
  // Note: Adding an encoded sink internally generates a new key frame
  // request, no need to RequestKeyFrame().
  ConnectEncodedToTrack(
      WebMediaStreamTrack(track_),
      base::BindPostTask(
          main_thread_task_runner_,
          blink::BindRepeating(
              &VideoTrackRecorderPassthrough::HandleEncodedVideoFrame,
              weak_factory_.GetWeakPtr(),
              blink::BindRepeating(base::TimeTicks::Now))));
}

VideoTrackRecorderPassthrough::~VideoTrackRecorderPassthrough() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DisconnectFromTrack();
}

void VideoTrackRecorderPassthrough::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  state_ = KeyFrameState::kPaused;
}

void VideoTrackRecorderPassthrough::Resume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  state_ = KeyFrameState::kWaitingForKeyFrame;
  RequestKeyFrame();
}

void VideoTrackRecorderPassthrough::OnEncodedVideoFrameForTesting(
    base::TimeTicks now,
    scoped_refptr<EncodedVideoFrame> frame,
    base::TimeTicks capture_time) {
  HandleEncodedVideoFrame(
      blink::BindRepeating([](base::TimeTicks now) { return now; }, now), frame,
      capture_time);
}

void VideoTrackRecorderPassthrough::RequestKeyFrame() {
  auto* video_track =
      static_cast<MediaStreamVideoTrack*>(track_->GetPlatformTrack());
  DCHECK(video_track->source());
  video_track->source()->RequestKeyFrame();
}

void VideoTrackRecorderPassthrough::DisconnectFromTrack() {
  // TODO(crbug.com/704136) : Remove this method when moving
  // MediaStreamVideoTrack to Oilpan's heap.
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DisconnectEncodedFromTrack();
}

void VideoTrackRecorderPassthrough::HandleEncodedVideoFrame(
    base::RepeatingCallback<base::TimeTicks()> time_now_callback,
    scoped_refptr<EncodedVideoFrame> encoded_frame,
    base::TimeTicks estimated_capture_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  if (state_ == KeyFrameState::kPaused) {
    return;
  }
  if (state_ == KeyFrameState::kWaitingForKeyFrame &&
      !encoded_frame->IsKeyFrame()) {
    // Don't RequestKeyFrame() here - we already did this implicitly when
    // Creating/Starting or explicitly when Resuming this object.
    return;
  }
  state_ = KeyFrameState::kKeyFrameReceivedOK;

  auto now = std::move(time_now_callback).Run();
  if (encoded_frame->IsKeyFrame()) {
    key_frame_processor_.OnKeyFrame(now);
  }
  if (key_frame_processor_.OnFrameAndShouldRequestKeyFrame(now)) {
    RequestKeyFrame();
  }

  auto buffer = media::DecoderBuffer::CopyFrom(encoded_frame->Data());
  buffer->set_is_key_frame(encoded_frame->IsKeyFrame());

  media::Muxer::VideoParameters params(
      encoded_frame->Resolution(),
      /*frame_rate=*/0.0f,
      /*codec=*/encoded_frame->Codec(),
      /*color_space=*/encoded_frame->ColorSpace(),
      /*transformation=*/encoded_frame->Transformation());
  if (auto* callback = callback_interface()->Get()) {
    callback->OnPassthroughVideo(params, std::move(buffer),
                                 estimated_capture_time);
  }
}

}  // namespace blink
