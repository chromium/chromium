// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/mediarecorder/video_track_recorder.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/paint/skia_paint_canvas.h"
#include "media/base/async_destroy_video_encoder.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "media/base/video_codecs.h"
#include "media/base/video_encoder_metrics_provider.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/media_buildflags.h"
#include "media/muxers/webm_muxer.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/video_encode_accelerator_adapter.h"
#include "media/video/vpx_video_encoder.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/modules/mediarecorder/media_recorder_encoder_wrapper.h"
#include "third_party/blink/renderer/modules/mediarecorder/vea_encoder.h"
#include "third_party/blink/renderer/modules/mediarecorder/vpx_encoder.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_util.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/sequence_bound.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(ENABLE_OPENH264)
#include "media/video/openh264_video_encoder.h"
#include "third_party/blink/renderer/modules/mediarecorder/h264_encoder.h"
#endif  // #if BUILDFLAG(ENABLE_OPENH264)

#if BUILDFLAG(ENABLE_LIBAOM)
#include "media/video/av1_video_encoder.h"
#endif  // BUILDFLAG(ENABLE_LIBAOM)

using video_track_recorder::kVEAEncoderMinResolutionHeight;
using video_track_recorder::kVEAEncoderMinResolutionWidth;

namespace WTF {
template <>
struct CrossThreadCopier<std::vector<scoped_refptr<media::VideoFrame>>>
    : public CrossThreadCopierPassThrough<
          std::vector<scoped_refptr<media::VideoFrame>>> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<blink::KeyFrameRequestProcessor::Configuration>
    : public CrossThreadCopierPassThrough<
          blink::KeyFrameRequestProcessor::Configuration> {
  STATIC_ONLY(CrossThreadCopier);
};
}  // namespace WTF

namespace blink {

// Helper class used to bless annotation of our calls to
// CreateOffscreenGraphicsContext3DProvider using ScopedAllowBaseSyncPrimitives.
class VideoTrackRecorderImplContextProvider {
 public:
  static std::unique_ptr<WebGraphicsContext3DProvider>
  CreateOffscreenGraphicsContext(Platform::ContextAttributes context_attributes,
                                 Platform::GraphicsInfo* gl_info,
                                 const KURL& url) {
    base::ScopedAllowBaseSyncPrimitives allow;
    return CreateOffscreenGraphicsContext3DProvider(context_attributes, gl_info,
                                                    url);
  }
};

using CodecId = VideoTrackRecorder::CodecId;

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
  NOTREACHED_IN_MIGRATION() << rotation;
  return libyuv::kRotate0;
}

namespace {

static const struct {
  CodecId codec_id;
  media::VideoCodecProfile min_profile;
  media::VideoCodecProfile max_profile;
} kPreferredCodecIdAndVEAProfiles[] = {
    {CodecId::kVp8, media::VP8PROFILE_MIN, media::VP8PROFILE_MAX},
    {CodecId::kVp9, media::VP9PROFILE_MIN, media::VP9PROFILE_MAX},
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    {CodecId::kH264, media::H264PROFILE_MIN, media::H264PROFILE_MAX},
#endif
    {CodecId::kAv1, media::AV1PROFILE_MIN, media::AV1PROFILE_MAX},
};

static_assert(std::size(kPreferredCodecIdAndVEAProfiles) ==
                  static_cast<int>(CodecId::kLast),
              "|kPreferredCodecIdAndVEAProfiles| should consider all CodecIds");

// The maximum number of frames which we'll keep frame references alive for
// encode. The number of frames in flight is further restricted by the device
// video capture max buffer pool size if it is smaller. This guarantees that
// there is limit on the number of frames in a FIFO queue that are being encoded
// and frames coming after this limit is reached are dropped.
// TODO(emircan): Make this a LIFO queue that has different sizes for each
// encoder implementation.
const size_t kMaxNumberOfFramesInEncode = 10;

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

VideoTrackRecorderImpl::CodecEnumerator* GetCodecEnumerator() {
  static VideoTrackRecorderImpl::CodecEnumerator* enumerator =
      new VideoTrackRecorderImpl::CodecEnumerator(GetVEASupportedProfiles());
  return enumerator;
}

void UmaHistogramForCodec(bool uses_acceleration, CodecId codec_id) {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // (kMaxValue being the only exception, as it does not map to a logged value,
  // and should be renumbered as new values are inserted.)
  enum class VideoTrackRecorderCodecHistogram : uint8_t {
    kUnknown = 0,
    kVp8Sw = 1,
    kVp8Hw = 2,
    kVp9Sw = 3,
    kVp9Hw = 4,
    kH264Sw = 5,
    kH264Hw = 6,
    kAv1Sw = 7,
    kAv1Hw = 8,
    kMaxValue = kAv1Hw,
  };
  auto histogram = VideoTrackRecorderCodecHistogram::kUnknown;
  if (uses_acceleration) {
    switch (codec_id) {
      case CodecId::kVp8:
        histogram = VideoTrackRecorderCodecHistogram::kVp8Hw;
        break;
      case CodecId::kVp9:
        histogram = VideoTrackRecorderCodecHistogram::kVp9Hw;
        break;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
      case CodecId::kH264:
        histogram = VideoTrackRecorderCodecHistogram::kH264Hw;
        break;
#endif
      case CodecId::kAv1:
        histogram = VideoTrackRecorderCodecHistogram::kAv1Hw;
        break;
      case CodecId::kLast:
        break;
    }
  } else {
    switch (codec_id) {
      case CodecId::kVp8:
        histogram = VideoTrackRecorderCodecHistogram::kVp8Sw;
        break;
      case CodecId::kVp9:
        histogram = VideoTrackRecorderCodecHistogram::kVp9Sw;
        break;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
      case CodecId::kH264:
        histogram = VideoTrackRecorderCodecHistogram::kH264Sw;
        break;
#endif
      case CodecId::kAv1:
        histogram = VideoTrackRecorderCodecHistogram::kAv1Sw;
        break;
      case CodecId::kLast:
        break;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("Media.MediaRecorder.Codec", histogram);
}

// Returns the default codec profile for |codec_id|.
std::optional<media::VideoCodecProfile> GetMediaVideoCodecProfileForSwEncoder(
    VideoTrackRecorder::CodecId codec_id) {
  switch (codec_id) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS) && BUILDFLAG(ENABLE_OPENH264)
    case CodecId::kH264:
      return media::H264PROFILE_BASELINE;
#endif  // BUILDFLAG(ENABLE_OPENH264)
    case CodecId::kVp8:
      return media::VP8PROFILE_ANY;
    case CodecId::kVp9:
      return media::VP9PROFILE_MIN;
#if BUILDFLAG(ENABLE_LIBAOM)
    case CodecId::kAv1:
      return media::AV1PROFILE_MIN;
#endif  // BUILDFLAG(ENABLE_LIBAOM)
    default:
      return std::nullopt;
  }
}

bool IsSoftwareEncoderAvailable(CodecId codec_id) {
  return GetMediaVideoCodecProfileForSwEncoder(codec_id).has_value();
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
  } else if (!IsSoftwareEncoderAvailable(codec_profile.codec_id)) {
    LOG(ERROR) << "Can't use VEA, but must be able to use VEA, codec_id="
               << static_cast<int>(codec_profile.codec_id);
    return std::nullopt;
  }
  // Software encoder will be used.
  return codec_profile.profile.value_or(
      GetMediaVideoCodecProfileForSwEncoder(codec_profile.codec_id).value());
}

MediaRecorderEncoderWrapper::CreateEncoderCB
GetCreateHardwareVideoEncoderCallback() {
  return ConvertToBaseRepeatingCallback(WTF::CrossThreadBindRepeating(
      [](media::GpuVideoAcceleratorFactories* gpu_factories)
          -> std::unique_ptr<media::VideoEncoder> {
        return std::make_unique<media::AsyncDestroyVideoEncoder<
            media::VideoEncodeAcceleratorAdapter>>(
            std::make_unique<media::VideoEncodeAcceleratorAdapter>(
                gpu_factories, std::make_unique<media::NullMediaLog>(),
                base::SequencedTaskRunner::GetCurrentDefault()));
      }));
}

MediaRecorderEncoderWrapper::CreateEncoderCB
GetCreateSoftwareVideoEncoderCallback(CodecId codec_id) {
  switch (codec_id) {
#if BUILDFLAG(ENABLE_OPENH264)
    case CodecId::kH264:
      return ConvertToBaseRepeatingCallback(WTF::CrossThreadBindRepeating(
          [](media::GpuVideoAcceleratorFactories* /*gpu_factories*/)
              -> std::unique_ptr<media::VideoEncoder> {
            return std::make_unique<media::OpenH264VideoEncoder>();
          }));
#endif  // BUILDFLAG(ENABLE_OPENH264)
#if BUILDFLAG(ENABLE_LIBVPX)
    case CodecId::kVp8:
    case CodecId::kVp9:
      return ConvertToBaseRepeatingCallback(WTF::CrossThreadBindRepeating(
          [](media::GpuVideoAcceleratorFactories* /*gpu_factories*/)
              -> std::unique_ptr<media::VideoEncoder> {
            return std::make_unique<media::VpxVideoEncoder>();
          }));
#endif
#if BUILDFLAG(ENABLE_LIBAOM)
    case CodecId::kAv1:
      return ConvertToBaseRepeatingCallback(WTF::CrossThreadBindRepeating(
          [](media::GpuVideoAcceleratorFactories* /*gpu_factories*/)
              -> std::unique_ptr<media::VideoEncoder> {
            return std::make_unique<media::Av1VideoEncoder>();
          }));
#endif  // BUILDFLAG(ENABLE_LIBAOM)
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unsupported codec=" << static_cast<int>(codec_id);
      return base::NullCallback();
  }
}
}  // anonymous namespace

VideoTrackRecorder::VideoTrackRecorder(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    WeakCell<CallbackInterface>* callback_interface)
    : TrackRecorder(base::BindPostTask(
          main_thread_task_runner,
          WTF::BindOnce(&CallbackInterface::OnSourceReadyStateChanged,
                        WrapPersistent(callback_interface)))),
      main_thread_task_runner_(std::move(main_thread_task_runner)),
      callback_interface_(callback_interface) {
  CHECK(main_thread_task_runner_);
}

VideoTrackRecorderImpl::CodecProfile::CodecProfile(CodecId codec_id)
    : codec_id(codec_id) {}

VideoTrackRecorderImpl::CodecProfile::CodecProfile(
    CodecId codec_id,
    std::optional<media::VideoCodecProfile> opt_profile,
    std::optional<media::VideoCodecLevel> opt_level)
    : codec_id(codec_id), profile(opt_profile), level(opt_level) {}

VideoTrackRecorderImpl::CodecProfile::CodecProfile(
    CodecId codec_id,
    media::VideoCodecProfile profile,
    media::VideoCodecLevel level)
    : codec_id(codec_id), profile(profile), level(level) {}

VideoTrackRecorderImpl::CodecEnumerator::CodecEnumerator(
    const media::VideoEncodeAccelerator::SupportedProfiles&
        vea_supported_profiles) {
  for (const auto& supported_profile : vea_supported_profiles) {
    const media::VideoCodecProfile codec = supported_profile.profile;
    for (auto& codec_id_and_profile : kPreferredCodecIdAndVEAProfiles) {
      if (codec >= codec_id_and_profile.min_profile &&
          codec <= codec_id_and_profile.max_profile) {
        DVLOG(2) << "Accelerated codec found: " << media::GetProfileName(codec)
                 << ", min_resolution: "
                 << supported_profile.min_resolution.ToString()
                 << ", max_resolution: "
                 << supported_profile.max_resolution.ToString()
                 << ", max_framerate: "
                 << supported_profile.max_framerate_numerator << "/"
                 << supported_profile.max_framerate_denominator;
        auto iter = supported_profiles_.find(codec_id_and_profile.codec_id);
        if (iter == supported_profiles_.end()) {
          auto result = supported_profiles_.insert(
              codec_id_and_profile.codec_id,
              media::VideoEncodeAccelerator::SupportedProfiles());
          result.stored_value->value.push_back(supported_profile);
        } else {
          iter->value.push_back(supported_profile);
        }
        if (preferred_codec_id_ == CodecId::kLast) {
          preferred_codec_id_ = codec_id_and_profile.codec_id;
        }
      }
    }
  }
}

VideoTrackRecorderImpl::CodecEnumerator::~CodecEnumerator() = default;

std::pair<media::VideoCodecProfile, bool>
VideoTrackRecorderImpl::CodecEnumerator::FindSupportedVideoCodecProfile(
    CodecId codec,
    media::VideoCodecProfile profile) const {
  const auto profiles = supported_profiles_.find(codec);
  if (profiles == supported_profiles_.end()) {
    return {media::VIDEO_CODEC_PROFILE_UNKNOWN, false};
  }
  for (const auto& p : profiles->value) {
    if (p.profile == profile) {
      const bool vbr_support =
          p.rate_control_modes & media::VideoEncodeAccelerator::kVariableMode;
      return {profile, vbr_support};
    }
  }
  return {media::VIDEO_CODEC_PROFILE_UNKNOWN, false};
}

VideoTrackRecorderImpl::CodecId
VideoTrackRecorderImpl::CodecEnumerator::GetPreferredCodecId(
    MediaTrackContainerType type) const {
  if (preferred_codec_id_ == CodecId::kLast) {
    if (type == MediaTrackContainerType::kVideoMp4 ||
        type == MediaTrackContainerType::kAudioMp4) {
      return CodecId::kVp9;
    }
    return CodecId::kVp8;
  }

  return preferred_codec_id_;
}

std::pair<media::VideoCodecProfile, bool>
VideoTrackRecorderImpl::CodecEnumerator::GetFirstSupportedVideoCodecProfile(
    CodecId codec) const {
  const auto profile = supported_profiles_.find(codec);
  if (profile == supported_profiles_.end()) {
    return {media::VIDEO_CODEC_PROFILE_UNKNOWN, false};
  }

  const auto& supported_profile = profile->value.front();
  const bool vbr_support = supported_profile.rate_control_modes &
                           media::VideoEncodeAccelerator::kVariableMode;
  return {supported_profile.profile, vbr_support};
}

media::VideoEncodeAccelerator::SupportedProfiles
VideoTrackRecorderImpl::CodecEnumerator::GetSupportedProfiles(
    CodecId codec) const {
  const auto profile = supported_profiles_.find(codec);
  return profile == supported_profiles_.end()
             ? media::VideoEncodeAccelerator::SupportedProfiles()
             : profile->value;
}

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
    const OnEncodedVideoCB& on_encoded_video_cb,
    uint32_t bits_per_second)
    : encoding_task_runner_(std::move(encoding_task_runner)),
      on_encoded_video_cb_(on_encoded_video_cb),
      bits_per_second_(bits_per_second),
      num_frames_in_encode_(
          std::make_unique<VideoTrackRecorderImpl::Counter>()) {
  CHECK(encoding_task_runner_);
  DCHECK(!on_encoded_video_cb_.is_null());
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

  if (num_frames_in_encode_->count() >
      std::min(kMaxNumberOfFramesInEncode, frame_buffer_pool_limit_)) {
    LOCAL_HISTOGRAM_BOOLEAN("Media.MediaRecorder.DroppingFrameTooManyInEncode",
                            true);
    DLOG(WARNING) << "Too many frames are queued up. Dropping this one.";
    return;
  }

  const bool is_format_supported =
      (video_frame->format() == media::PIXEL_FORMAT_NV12 &&
       video_frame->HasMappableGpuBuffer()) ||
      (video_frame->IsMappable() &&
       (video_frame->format() == media::PIXEL_FORMAT_I420 ||
        video_frame->format() == media::PIXEL_FORMAT_I420A));
  scoped_refptr<media::VideoFrame> frame = std::move(video_frame);
  // First, pixel format is converted to NV12, I420 or I420A.
  if (!is_format_supported) {
    frame = MaybeProvideEncodableFrame(std::move(frame));
  }
  if (frame && frame->format() == media::PIXEL_FORMAT_I420A &&
      !CanEncodeAlphaChannel()) {
    CHECK(!frame->HasMappableGpuBuffer());
    // Drop alpha channel if the encoder does not support it yet.
    frame = media::WrapAsI420VideoFrame(std::move(frame));
  }

  if (!frame) {
    // Explicit reasons for the frame drop are already logged.
    return;
  }
  frame->AddDestructionObserver(base::BindPostTask(
      encoding_task_runner_,
      WTF::BindOnce(&VideoTrackRecorderImpl::Counter::DecreaseCount,
                    num_frames_in_encode_->GetWeakPtr())));
  num_frames_in_encode_->IncreaseCount();
  EncodeFrame(std::move(frame), timestamp,
              request_key_frame_for_testing_ || force_key_frame);
  request_key_frame_for_testing_ = false;
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
      DLOG(ERROR) << "Can't convert RGB to I420";
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
    Platform::ContextAttributes attributes;
    attributes.enable_raster_interface = true;
    attributes.prefer_low_power_gpu = true;

    // TODO(crbug.com/1240756): This line can be removed once OOPR-Canvas has
    // shipped on all platforms
    attributes.support_grcontext = true;

    Platform::GraphicsInfo info;
    encoder_thread_context_ =
        VideoTrackRecorderImplContextProvider::CreateOffscreenGraphicsContext(
            attributes, &info, KURL("chrome://VideoTrackRecorderImpl"));

    if (encoder_thread_context_ &&
        !encoder_thread_context_->BindToCurrentSequence()) {
      encoder_thread_context_ = nullptr;
    }
  }

  if (!encoder_thread_context_) {
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

    media::VideoRotation video_rotation = media::VIDEO_ROTATION_0;
    if (video_frame->metadata().transformation) {
      video_rotation = video_frame->metadata().transformation->rotation;
    }

    if (video_rotation == media::VIDEO_ROTATION_90 ||
        video_rotation == media::VIDEO_ROTATION_270) {
      new_visible_size.SetSize(old_visible_size.height(),
                               old_visible_size.width());
    }

    frame = frame_pool_.CreateFrame(
        is_opaque ? media::PIXEL_FORMAT_I420 : media::PIXEL_FORMAT_I420A,
        new_visible_size, gfx::Rect(new_visible_size), new_visible_size,
        video_frame->timestamp());

    const SkImageInfo info = SkImageInfo::MakeN32(
        frame->visible_rect().width(), frame->visible_rect().height(),
        is_opaque ? kOpaque_SkAlphaType : kPremul_SkAlphaType);

    // Create |surface_| if it doesn't exist or incoming resolution has changed.
    if (!canvas_ || canvas_->imageInfo().width() != info.width() ||
        canvas_->imageInfo().height() != info.height()) {
      bitmap_.allocPixels(info);
      canvas_ = std::make_unique<cc::SkiaPaintCanvas>(bitmap_);
    }
    if (!video_renderer_) {
      video_renderer_ = std::make_unique<media::PaintCanvasVideoRenderer>();
    }

    encoder_thread_context_->CopyVideoFrame(video_renderer_.get(),
                                            video_frame.get(), canvas_.get());

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
            MediaVideoRotationToRotationMode(video_rotation),
            source_pixel_format) != 0) {
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

  if (frame->HasMappableGpuBuffer()) {
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
VideoTrackRecorderImpl::CodecId VideoTrackRecorderImpl::GetPreferredCodecId(
    MediaTrackContainerType type) {
  return GetCodecEnumerator()->GetPreferredCodecId(type);
}

// static
bool VideoTrackRecorderImpl::CanUseAcceleratedEncoder(
    CodecProfile& codec_profile,
    size_t width,
    size_t height,
    double framerate) {
  if (IsSoftwareEncoderAvailable(codec_profile.codec_id)) {
    if (width < kVEAEncoderMinResolutionWidth) {
      return false;
    }
    if (height < kVEAEncoderMinResolutionHeight) {
      return false;
    }
  }

  const auto profiles =
      GetCodecEnumerator()->GetSupportedProfiles(codec_profile.codec_id);
  if (profiles.empty()) {
    return false;
  }

  for (const auto& profile : profiles) {
    if (profile.profile == media::VIDEO_CODEC_PROFILE_UNKNOWN) {
      return false;
    }
    // Skip other profiles if the profile is specified.
    if (codec_profile.profile && *codec_profile.profile != profile.profile) {
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
      on_encoded_video_cb_(base::BindPostTask(
          main_thread_task_runner_,
          WTF::BindRepeating(&CallbackInterface::OnEncodedVideo,
                             WrapPersistent(callback_interface)))),
      frame_buffer_pool_limit_(frame_buffer_pool_limit) {
  TRACE_EVENT("media", "VideoTrackRecorderImpl::VideoTrackRecorderImpl");
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(track_);
  DCHECK(track_->GetSourceType() == MediaStreamSource::kTypeVideo);

  // Start querying for encoder support known.
  NotifyEncoderSupportKnown(
      WTF::BindOnce(&VideoTrackRecorderImpl::OnEncoderSupportKnown,
                    weak_factory_.GetWeakPtr()));

  // OnVideoFrame() will be called on Render Main thread.
  ConnectToTrack(base::BindPostTask(
      main_thread_task_runner_,
      WTF::BindRepeating(&VideoTrackRecorderImpl::OnVideoFrame,
                         weak_factory_.GetWeakPtr(),
                         /*allow_vea_encoder=*/true)));
}

VideoTrackRecorderImpl::~VideoTrackRecorderImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DisconnectFromTrack();
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

std::unique_ptr<VideoTrackRecorder::Encoder>
VideoTrackRecorderImpl::CreateMediaVideoEncoder(
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
    on_error_cb = base::BindPostTask(
        main_thread_task_runner_,
        WTF::BindOnce(&VideoTrackRecorderImpl::OnHardwareEncoderError,
                      weak_factory_.GetWeakPtr()));
  } else {
    on_error_cb = base::BindPostTask(
        main_thread_task_runner_,
        WTF::BindOnce(&CallbackInterface::OnVideoEncodingError,
                      WrapPersistent(callback_interface())));
  }

  media::GpuVideoAcceleratorFactories* gpu_factories =
      Platform::Current()->GetGpuFactories();
  return std::make_unique<MediaRecorderEncoderWrapper>(
      std::move(encoding_task_runner), *codec_profile.profile, bits_per_second_,
      is_screencast, create_vea_encoder ? gpu_factories : nullptr,
      create_vea_encoder
          ? GetCreateHardwareVideoEncoderCallback()
          : GetCreateSoftwareVideoEncoderCallback(codec_profile.codec_id),
      on_encoded_video_cb_, std::move(on_error_cb));
}

std::unique_ptr<VideoTrackRecorder::Encoder>
VideoTrackRecorderImpl::CreateSoftwareVideoEncoder(
    scoped_refptr<base::SequencedTaskRunner> encoding_task_runner,
    CodecProfile codec_profile,
    bool is_screencast) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  CHECK(codec_profile.profile.has_value());

  switch (codec_profile.codec_id) {
#if BUILDFLAG(ENABLE_OPENH264)
    case CodecId::kH264:
      return std::make_unique<H264Encoder>(
          std::move(encoding_task_runner), on_encoded_video_cb_, codec_profile,
          bits_per_second_, is_screencast,
          base::BindPostTask(
              main_thread_task_runner_,
              WTF::BindRepeating(&CallbackInterface::OnVideoEncodingError,
                                 WrapPersistent(callback_interface()))));
#endif
    case CodecId::kVp8:
    case CodecId::kVp9:
      return std::make_unique<VpxEncoder>(
          std::move(encoding_task_runner),
          codec_profile.codec_id == CodecId::kVp9, on_encoded_video_cb_,
          bits_per_second_, is_screencast,
          base::BindPostTask(
              main_thread_task_runner_,
              WTF::BindRepeating(&CallbackInterface::OnVideoEncodingError,
                                 WrapPersistent(callback_interface()))));
#if BUILDFLAG(ENABLE_LIBAOM)
    case CodecId::kAv1: {
      auto on_error_cb = base::BindPostTask(
          main_thread_task_runner_,
          WTF::BindOnce(&CallbackInterface::OnVideoEncodingError,
                        WrapPersistent(callback_interface())));
      return std::make_unique<MediaRecorderEncoderWrapper>(
          std::move(encoding_task_runner), *codec_profile.profile,
          bits_per_second_, is_screencast,
          /*gpu_factories=*/nullptr,
          GetCreateSoftwareVideoEncoderCallback(CodecId::kAv1),
          on_encoded_video_cb_, std::move(on_error_cb));
    }
#endif  // BUILDFLAG(ENABLE_LIBAOM)
    default:
      NOTREACHED() << "Unsupported codec: "
                   << static_cast<int>(codec_profile.codec_id);
  }
}

std::unique_ptr<VideoTrackRecorder::Encoder>
VideoTrackRecorderImpl::CreateHardwareVideoEncoder(
    scoped_refptr<base::SequencedTaskRunner> encoding_task_runner,
    CodecProfile codec_profile,
    const gfx::Size& input_size,
    bool use_import_mode,
    bool is_screencast) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  CHECK(codec_profile.profile.has_value());
  const auto [vea_profile, vbr_supported] =
      GetCodecEnumerator()->FindSupportedVideoCodecProfile(
          codec_profile.codec_id, *codec_profile.profile);

  // VBR encoding is preferred.
  media::Bitrate::Mode bitrate_mode = vbr_supported
                                          ? media::Bitrate::Mode::kVariable
                                          : media::Bitrate::Mode::kConstant;
  return std::make_unique<VEAEncoder>(
      std::move(encoding_task_runner), on_encoded_video_cb_,
      base::BindPostTask(
          main_thread_task_runner_,
          WTF::BindRepeating(&VideoTrackRecorderImpl::OnHardwareEncoderError,
                             weak_factory_.GetWeakPtr())),
      bitrate_mode, bits_per_second_, vea_profile, codec_profile.level,
      input_size, use_import_mode, is_screencast);
}

void VideoTrackRecorderImpl::InitializeEncoder(
    uint32_t bits_per_second,
    bool allow_vea_encoder,
    media::VideoFrame::StorageType frame_storage_type,
    gfx::Size input_size) {
  TRACE_EVENT("media", "VideoTrackRecorderImpl::InitializeEncoder");
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  auto codec_profile = codec_profile_;
  const bool can_use_vea = CanUseAcceleratedEncoder(
      codec_profile, input_size.width(), input_size.height());
  CHECK(callback_interface());

  std::optional<media::VideoCodecProfile> profile =
      GetMediaVideoCodecProfile(codec_profile, input_size, allow_vea_encoder);
  if (!profile) {
    if (auto* callback = callback_interface()->Get()) {
      callback->OnVideoEncodingError();
    }
    return;
  }

  codec_profile.profile = *profile;

  const bool is_screencast =
      static_cast<const MediaStreamVideoTrack*>(track_->GetPlatformTrack())
          ->is_screencast();
  const bool use_import_mode =
      frame_storage_type == media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER;
  const bool create_vea_encoder = allow_vea_encoder && can_use_vea;

  scoped_refptr<base::SequencedTaskRunner> encoding_task_runner;
  std::unique_ptr<Encoder> encoder;
  if (RuntimeEnabledFeatures::MediaRecorderUseMediaVideoEncoderEnabled()) {
    encoding_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
    encoder = CreateMediaVideoEncoder(encoding_task_runner, codec_profile,
                                      is_screencast, create_vea_encoder);
  } else {
    if (create_vea_encoder) {
      encoding_task_runner =
          Platform::Current()->GetGpuFactories()->GetTaskRunner();
      encoder = CreateHardwareVideoEncoder(encoding_task_runner, codec_profile,
                                           input_size, use_import_mode,
                                           is_screencast);
    } else {
      encoding_task_runner =
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
      encoder = CreateSoftwareVideoEncoder(encoding_task_runner, codec_profile,
                                           is_screencast);
    }
  }

  UmaHistogramForCodec(create_vea_encoder, codec_profile.codec_id);
  CHECK(encoder);

  auto metrics_provider =
      callback_interface()->Get()
          ? callback_interface()->Get()->CreateVideoEncoderMetricsProvider()
          : nullptr;
  CHECK(metrics_provider);
  encoder_.emplace(encoding_task_runner, std::move(encoder));
  encoder_.AsyncCall(&Encoder::InitializeEncoder)
      .WithArgs(key_frame_config_, std::move(metrics_provider),
                frame_buffer_pool_limit_);
  if (should_pause_encoder_on_initialization_) {
    encoder_.AsyncCall(&Encoder::SetPaused).WithArgs(true);
  }
}

void VideoTrackRecorderImpl::OnHardwareEncoderError() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // Try without VEA.
  DisconnectFromTrack();
  encoder_.Reset();
  ConnectToTrack(base::BindPostTask(
      main_thread_task_runner_,
      WTF::BindRepeating(&VideoTrackRecorderImpl::OnVideoFrame,
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // HandleEncodedVideoFrame() will be called on Render Main thread.
  // Note: Adding an encoded sink internally generates a new key frame
  // request, no need to RequestKeyFrame().
  ConnectEncodedToTrack(
      WebMediaStreamTrack(track_),
      base::BindPostTask(
          main_thread_task_runner_,
          WTF::BindRepeating(
              &VideoTrackRecorderPassthrough::HandleEncodedVideoFrame,
              weak_factory_.GetWeakPtr(),
              WTF::BindRepeating(base::TimeTicks::Now))));
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
      WTF::BindRepeating([](base::TimeTicks now) { return now; }, now), frame,
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

  std::optional<gfx::ColorSpace> color_space;
  if (encoded_frame->ColorSpace()) {
    color_space = encoded_frame->ColorSpace();
  }

  auto buffer = media::DecoderBuffer::CopyFrom(encoded_frame->Data());
  buffer->set_is_key_frame(encoded_frame->IsKeyFrame());

  media::Muxer::VideoParameters params(encoded_frame->Resolution(),
                                       /*frame_rate=*/0.0f,
                                       /*codec=*/encoded_frame->Codec(),
                                       color_space);
  if (auto* callback = callback_interface()->Get()) {
    callback->OnPassthroughVideo(params, std::move(buffer),
                                 estimated_capture_time);
  }
}

}  // namespace blink
