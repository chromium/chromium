// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_VIDEO_TRACK_RECORDER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_VIDEO_TRACK_RECORDER_H_

#include <atomic>
#include <memory>
#include <optional>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_encoder.h"
#include "media/base/video_frame_converter.h"
#include "media/base/video_frame_pool.h"
#include "media/capture/video/video_capture_buffer_pool_util.h"
#include "media/media_buildflags.h"
#include "media/muxers/webm_muxer.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "media/video/video_encode_accelerator.h"
#include "third_party/blink/public/platform/media/video_capture.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/public/web/modules/mediastream/encoded_video_frame.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_sink.h"
#include "third_party/blink/renderer/modules/mediarecorder/key_frame_request_processor.h"
#include "third_party/blink/renderer/modules/mediarecorder/track_recorder.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/allow_discouraged_type.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/weak_cell.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/sequence_bound.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace media {
struct VideoEncoderInfo;
class VideoEncoderMetricsProvider;
class VideoFrame;
}

namespace video_track_recorder {
#if BUILDFLAG(IS_ANDROID)
const int kVEAEncoderMinResolutionWidth = 176;
const int kVEAEncoderMinResolutionHeight = 144;
#else
const int kVEAEncoderMinResolutionWidth = 640;
const int kVEAEncoderMinResolutionHeight = 480;
#endif
}  // namespace video_track_recorder

namespace blink {

class MediaStreamVideoTrack;

// Base class serving as interface for eventually saving encoded frames stemming
// from media from a source.
class VideoTrackRecorder : public TrackRecorder<MediaStreamVideoSink> {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // (kMaxValue being the only exception, as it does not map to a logged value,
  // and should be renumbered as new values are inserted.)
  //
  // LINT.IfChange(CodecHistogram)
  enum class CodecHistogram : uint8_t {
    kUnknown = 0,
    kVp8 = 1,
    kVp9 = 2,
    kH264 = 3,
    kAv1 = 4,
    kHevc = 5,
    kMaxValue = kHevc,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/media/enums.xml:MediaRecorderVideoCodec)

  static CodecHistogram CodecHistogramFromCodec(media::VideoCodec);

  // Callback interface for VideoTrackRecorders. The methods here need to all be
  // called on the main thread.
  class CallbackInterface : public GarbageCollectedMixin {
   public:
    // Called to indicate there is encoded passthrough (incoming encoded video
    // which wasn't encoded by MediaRecorder) video data available.
    // |encoded_alpha| represents the encode output of alpha channel when
    // available, can be empty otherwise.
    virtual void OnPassthroughVideo(
        const media::Muxer::VideoParameters& params,
        scoped_refptr<media::DecoderBuffer> encoded_data,
        base::TimeTicks timestamp) = 0;

    // Called to indicate there is encoded video data available. |encoded_alpha|
    // represents the encode output of alpha channel when available, can be
    // empty otherwise.
    virtual void OnEncodedVideo(
        const media::Muxer::VideoParameters& params,
        scoped_refptr<media::DecoderBuffer> encoded_data,
        std::optional<media::VideoEncoder::CodecDescription> codec_description,
        base::TimeTicks timestamp) = 0;

    virtual std::unique_ptr<media::VideoEncoderMetricsProvider>
    CreateVideoEncoderMetricsProvider() = 0;

    // Called on encountering encoder errors.
    virtual void OnVideoEncodingError(media::EncoderStatus error_status) = 0;

    // Called when a track's ready state changes.
    virtual void OnSourceReadyStateChanged() = 0;
  };

  // Video codec and its encoding profile/level.
  struct MODULES_EXPORT CodecProfile {
    media::VideoCodec codec;
    std::optional<media::VideoCodecProfile> profile;
    std::optional<media::VideoCodecLevel> level;

    explicit CodecProfile(media::VideoCodec codec);
    CodecProfile(media::VideoCodec codec,
                 std::optional<media::VideoCodecProfile> opt_profile,
                 std::optional<media::VideoCodecLevel> opt_level);
    CodecProfile(media::VideoCodec codec,
                 media::VideoCodecProfile profile,
                 media::VideoCodecLevel level);

    bool operator==(const CodecProfile& others) const {
      return (codec == others.codec) && (profile == others.profile) &&
             (level == others.level);
    }
  };

  using OnEncodedVideoCB = CrossThreadFunction<void(
      const media::Muxer::VideoParameters& params,
      scoped_refptr<media::DecoderBuffer> encoded_data,
      std::optional<media::VideoEncoder::CodecDescription> codec_description,
      base::TimeTicks capture_timestamp)>;

  // MediaStreamVideoSink implementation
  double GetRequiredMinFramesPerSec() const override { return 1; }

  // Wraps a counter in a class in order to enable use of base::WeakPtr<>.
  // See https://crbug.com/859610 for why this was added.
  class Counter {
   public:
    Counter();
    ~Counter();
    uint32_t count() const { return count_; }
    void IncreaseCount();
    void DecreaseCount();
    base::WeakPtr<Counter> GetWeakPtr();

   private:
    uint32_t count_;
    base::WeakPtrFactory<Counter> weak_factory_{this};
  };

  // Base class to describe a generic Encoder, encapsulating all actual encoder
  // (re)configurations, encoding and delivery of received frames. The class is
  // fully operated on a codec-specific SequencedTaskRunner.
  class MODULES_EXPORT Encoder {
   public:
    Encoder(scoped_refptr<base::SequencedTaskRunner> encoding_task_runner,
            VideoTrackRecorder::OnEncodedVideoCB on_encoded_video_cb,
            uint32_t bits_per_second);
    virtual ~Encoder();

    Encoder(const Encoder&) = delete;
    Encoder& operator=(const Encoder&) = delete;

    // Should be called shortly after wrapping the Encoder in a SequenceBound,
    // on the codec-specific task runner.
    void InitializeEncoder(
        KeyFrameRequestProcessor::Configuration key_frame_config,
        std::unique_ptr<media::VideoEncoderMetricsProvider> metrics_provider,
        size_t frame_buffer_pool_limit);

    // Start encoding |frame|, returning via |on_encoded_video_cb_|. This
    // call will also trigger an encode configuration upon first frame arrival
    // or parameter change, and an Encode() to actually
    // encode the frame. If the |frame|'s data is not directly available (e.g.
    // it's a texture) then MaybeProvideEncodableFrame() is called, and if
    // even that fails, black frames are sent instead.
    void StartFrameEncode(
        scoped_refptr<media::VideoFrame> video_frame,
        base::TimeTicks capture_timestamp);

    using OnEncodedVideoInternalCB =
        CrossThreadFunction<void(const media::Muxer::VideoParameters& params,
                                 std::string encoded_data,
                                 std::string encoded_alpha,
                                 base::TimeTicks capture_timestamp,
                                 bool is_key_frame)>;

    void SetPaused(bool paused);
    virtual bool CanEncodeAlphaChannel() const;
    virtual bool IsScreenContentEncodingForTesting() const { return false; }
    void ForceKeyFrameForNextFrameForTesting() {
      request_key_frame_for_testing_ = true;
    }
   protected:
    friend class VideoTrackRecorderTest;

    // Subclasses ought to call this whenever they learn about the current
    // video encoder details.
    void OnVideoEncoderInfo(const media::VideoEncoderInfo& encoder_info);

    scoped_refptr<media::VideoFrame> MaybeProvideEncodableFrame(
        scoped_refptr<media::VideoFrame> video_frame);
    // Called shortly after wrapping the Encoder in a SequenceBound, on the
    // codec-specific task runner.
    virtual void Initialize();
    virtual void EncodeFrame(scoped_refptr<media::VideoFrame> frame,
                             base::TimeTicks capture_timestamp,
                             bool request_keyframe) = 0;

    // Called when the frame reference is released after encode.
    void FrameReleased(scoped_refptr<media::VideoFrame> frame);

    // A helper function to convert the given |frame| to an I420 video frame.
    // Used mainly by the software encoders since I420 is the only supported
    // pixel format.  The function is best-effort.  If for any reason the
    // conversion fails, the original |frame| will be returned.
    scoped_refptr<media::VideoFrame> ConvertToI420ForSoftwareEncoder(
        scoped_refptr<media::VideoFrame> frame);

    const scoped_refptr<base::SequencedTaskRunner> encoding_task_runner_;
    size_t frame_buffer_pool_limit_ = 0;

    // While |paused_|, frames are not encoded.
    bool paused_ = false;

    // Callback transferring encoded video data.
    const OnEncodedVideoCB on_encoded_video_cb_;

    // Target bitrate for video encoding. If 0, a standard bitrate is used.
    const uint32_t bits_per_second_;

    // Testing state - decides if we should request a key frame next round.
    bool request_key_frame_for_testing_ = false;

    // Number of frames that we keep the reference alive for encode.
    std::unique_ptr<Counter> num_frames_in_encode_;

    // The maximum number of frames which we'll keep alive at a time during
    // encoding. This guarantees that there is a limit on the number of frames
    // in a FIFO queue that are being encoded, i.e., once this limit is
    // reached, further incoming frames are dropped.
    // This value can be updated by OnVideoEncoderInfo() so that it matches the
    // encoder capabilities. Some encoders must accumulate a certain number of
    // frames before they start producing output. Thus, it's also crucial that
    // the maximum size of the device's video capture buffer pool can
    // accommodate at least this many frames.
    static constexpr size_t kMaxNumberOfFramesInEncoderMinValue = 10;
    size_t max_number_of_frames_in_encode_ =
        kMaxNumberOfFramesInEncoderMinValue;

    // Used to retrieve incoming opaque VideoFrames (i.e. VideoFrames backed by
    // textures).
    std::unique_ptr<media::PaintCanvasVideoRenderer> video_renderer_;
    SkBitmap bitmap_;
    std::unique_ptr<cc::PaintCanvas> canvas_;
    std::unique_ptr<WebGraphicsContext3DProvider> encoder_thread_context_;
    KeyFrameRequestProcessor key_frame_processor_;
    bool awaiting_first_frame_ = true;
    std::unique_ptr<media::VideoEncoderMetricsProvider> metrics_provider_;

    media::VideoFramePool frame_pool_;

    // Handle frame format conversions.
    media::VideoFrameConverter frame_converter_;
  };

  VideoTrackRecorder(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      WeakCell<CallbackInterface>* callback_interface);

  virtual void Pause() = 0;
  virtual void Resume() = 0;
  virtual void OnVideoFrameForTesting(scoped_refptr<media::VideoFrame> frame,
                                      base::TimeTicks capture_time,
                                      bool allow_vea_encoder) {}
  virtual void OnEncodedVideoFrameForTesting(
      base::TimeTicks now,
      scoped_refptr<EncodedVideoFrame> frame,
      base::TimeTicks capture_time) {}

  WeakCell<CallbackInterface>* callback_interface() const {
    return callback_interface_;
  }

 protected:
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

 private:
  Persistent<WeakCell<CallbackInterface>> const callback_interface_;
};

// VideoTrackRecorderImpl uses the inherited WebMediaStreamSink and encodes the
// video frames received from a Stream Video Track. This class is constructed
// and used on a single thread, namely the main Render thread. This mirrors the
// other MediaStreamVideo* classes that are constructed/configured on Main
// Render thread but that pass frames on Render IO thread. It has an internal
// Encoder with its own threading subtleties, see the implementation file.
class MODULES_EXPORT VideoTrackRecorderImpl : public VideoTrackRecorder {
 public:
  static media::VideoCodec GetPreferredCodec(MediaTrackContainerType type);

  // Returns true if the device has a hardware accelerated encoder which can
  // encode video of the given |width|x|height| and |framerate| to specific
  // |codec_profile.codec_id|. If |codec_profile.profile| is set, then this
  // returns false if and only if the hardware encoder can encode in the
  // profile. If |codec_profile.profile| is not set, this sets
  // |codec_profile.profile| to the first profile supported by the hardware
  // encoder. Note: default framerate value means no restriction.
  static bool CanUseAcceleratedEncoder(CodecProfile& codec_profile,
                                       size_t width,
                                       size_t height,
                                       double framerate = 0.0);

  VideoTrackRecorderImpl(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      CodecProfile codec,
      MediaStreamComponent* track,
      WeakCell<CallbackInterface>* callback_interface,
      uint32_t bits_per_second,
      KeyFrameRequestProcessor::Configuration key_frame_config,
      size_t frame_buffer_pool_limit =
          media::DeviceVideoCaptureMaxBufferPoolSize());

  VideoTrackRecorderImpl(const VideoTrackRecorderImpl&) = delete;
  VideoTrackRecorderImpl& operator=(const VideoTrackRecorderImpl&) = delete;

  ~VideoTrackRecorderImpl() override;

  void Pause() override;
  void Resume() override;
  void OnVideoFrameForTesting(scoped_refptr<media::VideoFrame> frame,
                              base::TimeTicks capture_time,
                              bool allow_vea_encoder) override;
  void ForceKeyFrameForNextFrameForTesting();

 private:
  friend class VideoTrackRecorderTest;

  // Structure to store incoming MediaStream sink calls.
  struct MediaStreamFrame {
    scoped_refptr<media::VideoFrame> video_frame;
    base::TimeTicks estimated_capture_time;
  };

  void OnVideoFrame(bool allow_vea_encoder,
                    scoped_refptr<media::VideoFrame> video_frame,
                    base::TimeTicks capture_time);
  void ProcessOneVideoFrame(bool allow_vea_encoder,
                            scoped_refptr<media::VideoFrame> video_frame,
                            base::TimeTicks capture_time);
  void OnEncoderSupportKnown();
  void InitializeEncoder(uint32_t bits_per_second,
                         bool allow_vea_encoder,
                         media::VideoFrame::StorageType frame_storage_type,
                         gfx::Size input_size);
  void CreateMediaVideoEncoder(
      scoped_refptr<base::SequencedTaskRunner> encoding_task_runner,
      CodecProfile codec_profile,
      bool is_screencast,
      bool create_vea_encoder);
  void OnHardwareEncoderError(media::EncoderStatus error_status);

  void ConnectToTrack(const VideoCaptureDeliverFrameCB& callback);
  void DisconnectFromTrack();

  // Used to check that we are destroyed on the same sequence we were created.
  SEQUENCE_CHECKER(main_sequence_checker_);

  // We need to hold on to the Blink track to remove ourselves on dtor.
  Persistent<MediaStreamComponent> track_;

  // Holds inner class to encode using whichever codec is configured.
  SequenceBound<Encoder> encoder_;

  const KeyFrameRequestProcessor::Configuration key_frame_config_;
  const CodecProfile codec_profile_;
  const uint32_t bits_per_second_;
  const size_t frame_buffer_pool_limit_;
  bool should_pause_encoder_on_initialization_
      GUARDED_BY_CONTEXT(main_sequence_checker_) = false;
  Vector<MediaStreamFrame> incoming_frame_queue_
      GUARDED_BY_CONTEXT(main_sequence_checker_);
  bool encoder_support_known_ GUARDED_BY_CONTEXT(main_sequence_checker_) =
      false;
  size_t num_video_transformation_changes_ = 0;
  std::optional<media::VideoTransformation> last_transformation_ = std::nullopt;
  base::WeakPtrFactory<VideoTrackRecorderImpl> weak_factory_{this};
};

// VideoTrackRecorderPassthrough uses the inherited WebMediaStreamSink to
// dispatch EncodedVideoFrame content received from a MediaStreamVideoTrack.
class MODULES_EXPORT VideoTrackRecorderPassthrough : public VideoTrackRecorder {
 public:
  VideoTrackRecorderPassthrough(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      MediaStreamComponent* track,
      WeakCell<CallbackInterface>* callback_interface,
      KeyFrameRequestProcessor::Configuration key_frame_config);

  VideoTrackRecorderPassthrough(const VideoTrackRecorderPassthrough&) = delete;
  VideoTrackRecorderPassthrough& operator=(
      const VideoTrackRecorderPassthrough&) = delete;

  ~VideoTrackRecorderPassthrough() override;

  // VideoTrackRecorderBase
  void Pause() override;
  void Resume() override;
  void OnEncodedVideoFrameForTesting(base::TimeTicks now,
                                     scoped_refptr<EncodedVideoFrame> frame,
                                     base::TimeTicks capture_time) override;

 private:
  void RequestKeyFrame();
  void DisconnectFromTrack();
  void HandleEncodedVideoFrame(
      base::RepeatingCallback<base::TimeTicks()> time_now_callback,
      scoped_refptr<EncodedVideoFrame> encoded_frame,
      base::TimeTicks estimated_capture_time);

  // Used to check that we are destroyed on the same sequence we were created.
  SEQUENCE_CHECKER(main_sequence_checker_);

  // This enum class tracks encoded frame waiting and dispatching state. This
  // is needed to guarantee we're dispatching decodable content to
  // |on_encoded_video_cb|. Examples of times where this is needed is
  // startup and Pause/Resume.
  enum class KeyFrameState {
    kWaitingForKeyFrame,
    kKeyFrameReceivedOK,
    kPaused
  };

  // We need to hold on to the Blink track to remove ourselves on dtor.
  const Persistent<MediaStreamComponent> track_;
  KeyFrameState state_ = KeyFrameState::kWaitingForKeyFrame;
  KeyFrameRequestProcessor key_frame_processor_;
  base::WeakPtrFactory<VideoTrackRecorderPassthrough> weak_factory_{this};
};
}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_VIDEO_TRACK_RECORDER_H_
