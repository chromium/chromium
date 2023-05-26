// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_VIDEO_DECODER_STREAM_ADAPTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_VIDEO_DECODER_STREAM_ADAPTER_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/single_sample_metrics.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "media/base/decoder_status.h"
#include "media/base/media_switches.h"
#include "media/base/overlay_info.h"
#include "media/base/status.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/decoder_stream.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/modules/video_coding/include/video_codec_interface.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SequencedThreadTaskRunner;
}  // namespace base

namespace media {
class DecoderBuffer;
class DecoderFactory;
class GpuVideoAcceleratorFactories;
class MediaLog;
}  // namespace media

namespace blink {

// This class decodes video for WebRTC using a media::VideoDecoderStream.
//
// VideoDecoderStream is used by almost all non-WebRTC video decoding in
// chromium.  It handles selecting the decoder implementation, including between
// hardware and software implementations.  It also provides fallback / fall-
// forward between implementations if needed for config changes.
//
// Lifecycle methods are called on the WebRTC worker thread. Decoding happens on
// a WebRTC DecodingThread, which is an rtc::PlatformThread owend by WebRTC; it
// does not have a TaskRunner.
//
// To stop decoding, WebRTC stops the DecodingThread and then calls Release() on
// the worker. Calling the DecodedImageCallback after the DecodingThread is
// stopped is illegal but, because we decode on the media thread, there is no
// way to synchronize this correctly.
class PLATFORM_EXPORT RTCVideoDecoderStreamAdapter
    : public webrtc::VideoDecoder {
 public:
  // Minimum resolution that we'll consider "not low resolution" for the purpose
  // of falling back to software.
#if BUILDFLAG(IS_CHROMEOS)
  // Effectively opt-out CrOS, since it may cause tests to fail (b/179724180).
  static constexpr gfx::Size kMinResolution{2, 2};
#else
  static constexpr gfx::Size kMinResolution{320, 240};
#endif

  // Maximum number of decoder instances we'll allow before fallback to software
  // if the resolution is too low.  We'll allow more than this for high
  // resolution streams, but they'll fall back if they adapt below the limit.
  static constexpr int32_t kMaxDecoderInstances = 8;

  // Creates and initializes an RTCVideoDecoderStreamAdapter. Returns nullptr if
  // |format| cannot be supported. The gpu_factories may be null, in which case
  // only SW decoders will be used.
  // Called on the worker thread.
  static std::unique_ptr<RTCVideoDecoderStreamAdapter> Create(
      media::GpuVideoAcceleratorFactories* gpu_factories,
      base::WeakPtr<media::DecoderFactory> decoder_factory,
      scoped_refptr<base::SequencedTaskRunner> media_task_runner,
      const gfx::ColorSpace& render_color_space,
      const webrtc::SdpVideoFormat& format);

  RTCVideoDecoderStreamAdapter(const RTCVideoDecoderStreamAdapter&) = delete;
  RTCVideoDecoderStreamAdapter& operator=(const RTCVideoDecoderStreamAdapter&) =
      delete;

  // Called on |media_task_runner_|.
  ~RTCVideoDecoderStreamAdapter() override;

  // webrtc::VideoDecoder implementation.
  // Called on the DecodingThread.
  bool Configure(const Settings& settings) override;
  // Called on the DecodingThread.
  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* callback) override;
  // Called on the DecodingThread.
  int32_t Decode(const webrtc::EncodedImage& input_image,
                 bool missing_frames,
                 int64_t render_time_ms) override;
  // Called on the worker thread and on the DecodingThread.
  int32_t Release() override;
  // Called on the worker thread and on the DecodingThread.
  DecoderInfo GetDecoderInfo() const override;

 private:
  class InternalDemuxerStream;

  using InitCB = CrossThreadOnceFunction<void(bool)>;
  using FlushDoneCB = CrossThreadOnceFunction<void()>;

  struct PendingBuffer {
    scoped_refptr<media::DecoderBuffer> buffer;
    absl::optional<media::VideoDecoderConfig> new_config;
  };

  // Called on the worker thread.
  RTCVideoDecoderStreamAdapter(
      media::GpuVideoAcceleratorFactories* gpu_factories,
      base::WeakPtr<media::DecoderFactory> decoder_factory,
      scoped_refptr<base::SequencedTaskRunner> media_task_runner,
      const gfx::ColorSpace& render_color_space,
      const media::VideoDecoderConfig& config,
      const webrtc::SdpVideoFormat& format);

  // May be called on the decoder / worker thread at any time to replace the
  // decoder stream / demuxer / etc.
  void InitializeOrReinitializeSync();

  void InitializeOnMediaThread(const media::VideoDecoderConfig& config,
                               InitCB init_cb);
  void OnInitializeDone(base::TimeTicks start_time, bool success);
  void DecodeOnMediaThread(std::unique_ptr<PendingBuffer>);
  void OnFrameReady(media::VideoDecoderStream::ReadResult result);

  bool ShouldReinitializeForSettingHDRColorSpace(
      const webrtc::EncodedImage& input_image) const;

  // If no read is in progress with `decoder_stream_`, and there is undecoded
  // input buffers, then start a read.  Otherwise, do nothing.
  void AttemptRead();

  // Start a `DecoderStream::Reset`.
  void ResetOnMediaThread();

  // Notification when `DecoderStream::Reset` completes.
  void OnResetCompleteOnMediaThread();

  // Adjust `allowable_decoder_queue_length_` if needed.
  void AdjustQueueLength_Locked();

  // Clear as much memory as we can, cancel outstanding callbacks, etc.
  void ShutdownOnMediaThread();

  // Log the init state, if we know what it is.  Will do nothing if Initialize
  // has not completed, and also InitDecode() has not been called.
  void AttemptLogInitializationState_Locked();

  // Called on the media thread when `decoder_stream_` changes the decoder.
  void OnDecoderChanged(media::VideoDecoder* decoder);

  // Called on any thread with `lock_` held to record the max pending frames
  // that we've observed so far.  If we've not queued any frames, then this does
  // nothing.  If it does record some frames, then it resets the max to zero.
  // Must be called on the media thread.
  void RecordMaxInFlightDecodesLockedOnMedia();

  // Destroy any existing demuxer / decoder stream / etc., reset everything, and
  // create / start init on a new one.  This can be called on the media thread
  // at any time to replace the DecoderStream and friends, including dropping
  // any previously enqueued decoder buffers.
  void RestartDecoderStreamOnMedia();

  // Try to reconstruct `decoder_stream_` and friends, but prefer software
  // decoders over hardware ones.
  //
  // Returns WEBRTC_VIDEO_{ERROR, FALLBACK_TO_SOFTWARE} depending on whether we
  // should get a keyframe or give up.
  // Note that this should eventually return void, because we should never fall
  // back to software.  However, since we don't always support software codecs,
  // we allow it.
  //
  // Called on decoder thread, with `lock_` held.
  int32_t FallBackToSoftware_Locked();

  // Returns true if we believe that decoding is paused pending decoder
  // selection or reset.
  bool IsDecodingPaused_Locked() const;

  // Construction parameters.
  const scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
  media::GpuVideoAcceleratorFactories* const gpu_factories_;
  base::WeakPtr<media::DecoderFactory> const decoder_factory_;
  gfx::ColorSpace render_color_space_;
  const webrtc::SdpVideoFormat format_;
  media::VideoDecoderConfig config_;

  // Media thread members.
  // |media_log_| must outlive |video_decoder_| because it is passed as a raw
  // pointer.
  std::unique_ptr<media::MediaLog> media_log_;
  // Metric to record the max pending buffer count that we've observed.
  std::unique_ptr<base::SingleSampleMetric> max_buffer_count_metric_;
  // Size that we've reported to `max_buffer_count_metric_`, since each report
  // sends IPC.  Only report things when they change.
  size_t max_reported_buffer_count_ = 0;

  // Decoding thread members.
  bool key_frame_required_ = true;
  int buffers_since_last_keyframe_ = 0;
  webrtc::VideoCodecType video_codec_type_ = webrtc::kVideoCodecGeneric;

  // Shared members.
  mutable base::Lock lock_;
  bool has_error_ GUARDED_BY(lock_) = false;
  // Current maximum number of in-flight undecoded frames.
  size_t max_pending_buffer_count_ GUARDED_BY(lock_);
  // Current number of in-flight decodes.
  size_t pending_buffer_count_ GUARDED_BY(lock_) = 0;
  // Has DecoderStream initialization completed?  This does not imply that it
  // completed successfully.
  bool init_complete_ GUARDED_BY(lock_) = false;
  // Has InitDecode() been called?
  bool init_decode_complete_ GUARDED_BY(lock_) = false;
  // Have we logged init status yet?
  bool logged_init_status_ GUARDED_BY(lock_) = false;
  // Current decoder info, as reported by GetDecoderInfo().
  webrtc::VideoDecoder::DecoderInfo decoder_info_ GUARDED_BY(lock_);
  // Current decoder type.
  media::VideoDecoderType video_decoder_type_ GUARDED_BY(lock_) =
      media::VideoDecoderType::kUnknown;
  // If it's true, it indicates the decoder has been initialized successfully.
  bool decoder_configured_ GUARDED_BY(lock_) = false;
  // Current decode callback, if any.
  webrtc::DecodedImageCallback* decode_complete_callback_ GUARDED_BY(lock_) =
      nullptr;
  // Time since construction.  Cleared when we record that a frame has been
  // successfully decoded.
  absl::optional<base::TimeTicks> start_time_ GUARDED_BY(lock_);
  // Resolution of most recently decoded frame, or the initial resolution if we
  // haven't decoded anything yet.  Since this is updated asynchronously, it's
  // only an approximation of "most recently".
  gfx::Size current_resolution_ GUARDED_BY(lock_);
  // If true, then we'll try to use software decoders instead of hardware
  // decoders.  Useful to fall back to software if hw isn't working.  Also
  // remember that this does not guarantee a sw decoder; if there is no chrome
  // sw decoder, then DecoderStream will use a hw one anyway.
  bool prefer_software_decoders_ GUARDED_BY(lock_) = false;
  // Have we incremented the decoder count?
  bool contributes_to_decoder_count_ GUARDED_BY(lock_) = false;
  // Do we have an in-flight `DecoderStream::Reset()`, or is a call to start
  // one pending via a call to the media thread?
  bool pending_reset_ GUARDED_BY(lock_) = false;

  // Do we have an outstanding `DecoderStream::Read()`?
  // Media thread only.
  bool pending_read_ = false;

  // Media thread only.
  std::unique_ptr<InternalDemuxerStream> demuxer_stream_;
  std::unique_ptr<media::VideoDecoderStream> decoder_stream_;

  // Thread management.
  SEQUENCE_CHECKER(decoding_sequence_checker_);

  base::WeakPtr<RTCVideoDecoderStreamAdapter> weak_this_;
  base::WeakPtrFactory<RTCVideoDecoderStreamAdapter> weak_this_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_VIDEO_DECODER_STREAM_ADAPTER_H_
