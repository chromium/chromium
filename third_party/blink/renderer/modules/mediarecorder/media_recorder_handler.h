// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_MEDIA_RECORDER_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_MEDIA_RECORDER_HANDLER_H_

#include <memory>
#include <optional>
#include <string_view>

#include "base/feature_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_encoder.h"
#include "media/muxers/muxer_timestamp_adapter.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream.h"
#include "third_party/blink/public/web/modules/mediastream/encoded_video_frame.h"
#include "third_party/blink/renderer/modules/mediarecorder/audio_track_recorder.h"
#include "third_party/blink/renderer/modules/mediarecorder/video_track_recorder.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/weak_cell.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/formats/mp4/h264_annex_b_to_avc_bitstream_converter.h"
#endif

namespace media {
class AudioBus;
class AudioParameters;
class VideoFrame;
class VideoEncoderMetricsProvider;
class Muxer;
}  // namespace media

namespace blink {

class MediaRecorder;
class MediaStreamDescriptor;
struct WebMediaCapabilitiesInfo;
struct WebMediaConfiguration;

MODULES_EXPORT BASE_DECLARE_FEATURE(kMediaRecorderEnableMp4Muxer);

// MediaRecorderHandler orchestrates the creation, lifetime management and
// mapping between:
// - MediaStreamTrack(s) providing data,
// - {Audio,Video}TrackRecorders encoding that data,
// - a WebmMuxer class multiplexing encoded data into a WebM container, and
// - a single recorder client receiving this contained data.
// All methods are called on the same thread as construction and destruction,
// i.e. the Main Render thread.
class MODULES_EXPORT MediaRecorderHandler final
    : public GarbageCollected<MediaRecorderHandler>,
      public VideoTrackRecorder::CallbackInterface,
      public AudioTrackRecorder::CallbackInterface,
      public WebMediaStreamObserver {
 public:
  MediaRecorderHandler(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      KeyFrameRequestProcessor::Configuration key_frame_config);
  MediaRecorderHandler(const MediaRecorderHandler&) = delete;
  MediaRecorderHandler& operator=(const MediaRecorderHandler&) = delete;

  // MediaRecorder API isTypeSupported(), which boils down to
  // CanSupportMimeType() [1] "If true is returned from this method, it only
  // indicates that the MediaRecorder implementation is capable of recording
  // Blob objects for the specified MIME type. Recording may still fail if
  // sufficient resources are not available to support the concrete media
  // encoding."
  // [1] https://w3c.github.io/mediacapture-record/MediaRecorder.html#methods
  bool CanSupportMimeType(const String& type, const String& web_codecs);
  bool Initialize(MediaRecorder* client,
                  MediaStreamDescriptor* media_stream,
                  const String& type,
                  const String& codecs,
                  AudioTrackRecorder::BitrateMode audio_bitrate_mode);

  AudioTrackRecorder::BitrateMode AudioBitrateMode();

  bool Start(int timeslice,
             const String& type,
             uint32_t audio_bits_per_second,
             uint32_t video_bits_per_second);
  void Stop();
  void Pause();
  void Resume();

  // Implements WICG Media Capabilities encodingInfo() call for local encoding.
  // https://wicg.github.io/media-capabilities/#media-capabilities-interface
  using OnMediaCapabilitiesEncodingInfoCallback =
      base::OnceCallback<void(std::unique_ptr<WebMediaCapabilitiesInfo>)>;
  void EncodingInfo(const WebMediaConfiguration& configuration,
                    OnMediaCapabilitiesEncodingInfoCallback cb);
  String ActualMimeType();

  void Trace(Visitor*) const override;

 private:
  friend class MediaRecorderHandlerFixture;
  friend class MediaRecorderHandlerPassthroughTest;

  // WebMediaStreamObserver overrides.
  void TrackAdded(const WebString& track_id) override;
  void TrackRemoved(const WebString& track_id) override;

  // VideoTrackRecorder::CallbackInterface overrides.
  void OnEncodedVideo(
      const media::Muxer::VideoParameters& params,
      scoped_refptr<media::DecoderBuffer> encoded_data,
      std::optional<media::VideoEncoder::CodecDescription> codec_description,
      base::TimeTicks timestamp) override;
  void OnPassthroughVideo(const media::Muxer::VideoParameters& params,
                          scoped_refptr<media::DecoderBuffer> encoded_data,
                          base::TimeTicks timestamp) override;
  std::unique_ptr<media::VideoEncoderMetricsProvider>
  CreateVideoEncoderMetricsProvider() override;
  void OnVideoEncodingError() override;
  // AudioTrackRecorder::CallbackInterface overrides.
  void OnEncodedAudio(
      const media::AudioParameters& params,
      scoped_refptr<media::DecoderBuffer> encoded_data,
      std::optional<media::AudioEncoder::CodecDescription> codec_description,
      base::TimeTicks timestamp) override;
  void OnAudioEncodingError(media::EncoderStatus error_status) override;
  // [Audio/Video]TrackRecorder::CallbackInterface overrides.
  void OnSourceReadyStateChanged() override;

  void OnStreamChanged(const String& message);

  void HandleEncodedVideo(
      const media::Muxer::VideoParameters& params,
      scoped_refptr<media::DecoderBuffer> encoded_data,
      std::optional<media::VideoEncoder::CodecDescription> codec_description,
      base::TimeTicks timestamp);
  void WriteData(base::span<const uint8_t> data);

  // Updates recorded tracks live and enabled.
  void UpdateTracksLiveAndEnabled();

  void OnVideoFrameForTesting(scoped_refptr<media::VideoFrame> frame,
                              const base::TimeTicks& timestamp);
  void OnEncodedVideoFrameForTesting(scoped_refptr<EncodedVideoFrame> frame,
                                     const base::TimeTicks& timestamp);
  void OnAudioBusForTesting(const media::AudioBus& audio_bus,
                            const base::TimeTicks& timestamp);
  void SetAudioFormatForTesting(const media::AudioParameters& params);
  void UpdateTrackLiveAndEnabled(const MediaStreamComponent& track,
                                 bool is_video);

  // Variant holding configured keyframe intervals.
  const KeyFrameRequestProcessor::Configuration key_frame_config_;

  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  // Set to true if there is no MIME type configured upon Initialize()
  // and the video track's source supports encoded output, giving
  // this class the freedom to provide whatever it chooses to produce.
  bool passthrough_enabled_ = false;

  // Sanitized video and audio bitrate settings passed on initialize().
  uint32_t video_bits_per_second_{0};
  uint32_t audio_bits_per_second_{0};

  // Video Codec and profile, VP8 is used by default.
  VideoTrackRecorder::CodecProfile video_codec_profile_{
      VideoTrackRecorder::CodecId::kLast};

  // Audio Codec, OPUS is used by default.
  AudioTrackRecorder::CodecId audio_codec_id_{
      AudioTrackRecorder::CodecId::kLast};

  // Audio bitrate mode (constant, variable, etc.), VBR is used by default.
  AudioTrackRecorder::BitrateMode audio_bitrate_mode_;

  // |recorder_| has no notion of time, thus may configure us via
  // start(timeslice) to notify it after a certain |timeslice_| has passed. We
  // use a moving |slice_origin_timestamp_| to track those time chunks.
  base::TimeDelta timeslice_;
  base::TimeTicks slice_origin_timestamp_;

  // The last seen video codec of the last received encoded video frame.
  std::optional<media::VideoCodec> last_seen_codec_;

  bool recording_ = false;

  String type_;
  // True if we're observing track changes to `media_stream_`.
  bool is_media_stream_observer_ = false;
  // The MediaStream being recorded.
  Member<MediaStreamDescriptor> media_stream_;
  HeapVector<Member<MediaStreamComponent>> video_tracks_;
  HeapVector<Member<MediaStreamComponent>> audio_tracks_;

  Member<MediaRecorder> recorder_ = nullptr;

  Vector<std::unique_ptr<VideoTrackRecorder>> video_recorders_;
  Vector<std::unique_ptr<AudioTrackRecorder>> audio_recorders_;

  // Worker class doing the actual muxing work.
  std::unique_ptr<media::MuxerTimestampAdapter> muxer_adapter_;

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  std::unique_ptr<media::H264AnnexBToAvcBitstreamConverter> h264_converter_;
#endif

  // For invalidation of in-flight callbacks back to ourselves. Need to track
  // each callback interface specifically as there seem to be no automatic
  // coercion.
  WeakCellFactory<AudioTrackRecorder::CallbackInterface> weak_audio_factory_{
      this};
  WeakCellFactory<VideoTrackRecorder::CallbackInterface> weak_video_factory_{
      this};
  WeakCellFactory<MediaRecorderHandler> weak_factory_{this};
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_MEDIA_RECORDER_HANDLER_H_
