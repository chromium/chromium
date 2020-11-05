// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_VIDEO_TRACK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_VIDEO_TRACK_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "third_party/blink/public/platform/modules/mediastream/secure_display_link_tracker.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_sink.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_track.h"
#include "third_party/blink/public/web/modules/mediastream/encoded_video_frame.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_track_platform.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class VideoTrackAdapterSettings;

// MediaStreamVideoTrack is a video-specific representation of a
// MediaStreamTrackPlatform. It is owned by a MediaStreamComponent
// and can be retrieved using MediaStreamComponent::GetPlatformTrack().
class MODULES_EXPORT MediaStreamVideoTrack : public MediaStreamTrackPlatform {
 public:
  // Help method to create a WebMediaStreamTrack and a
  // MediaStreamVideoTrack instance. The MediaStreamVideoTrack object is owned
  // by the blink object in its WebMediaStreamTrack::GetPlatformTrack() member.
  // |callback| is triggered if the track is added to the source
  // successfully and will receive video frames that match the given settings
  // or if the source fails to provide video frames.
  // If |enabled| is true, sinks added to the track will
  // receive video frames when the source delivers frames to the track.
  static WebMediaStreamTrack CreateVideoTrack(
      MediaStreamVideoSource* source,
      MediaStreamVideoSource::ConstraintsOnceCallback callback,
      bool enabled);
  static WebMediaStreamTrack CreateVideoTrack(
      MediaStreamVideoSource* source,
      const VideoTrackAdapterSettings& adapter_settings,
      const base::Optional<bool>& noise_reduction,
      bool is_screencast,
      const base::Optional<double>& min_frame_rate,
      const base::Optional<double>& pan,
      const base::Optional<double>& tilt,
      const base::Optional<double>& zoom,
      bool pan_tilt_zoom_allowed,
      MediaStreamVideoSource::ConstraintsOnceCallback callback,
      bool enabled);

  static MediaStreamVideoTrack* From(const MediaStreamComponent* track);

  // Constructors for video tracks.
  MediaStreamVideoTrack(
      MediaStreamVideoSource* source,
      MediaStreamVideoSource::ConstraintsOnceCallback callback,
      bool enabled);
  MediaStreamVideoTrack(
      MediaStreamVideoSource* source,
      const VideoTrackAdapterSettings& adapter_settings,
      const base::Optional<bool>& noise_reduction,
      bool is_screen_cast,
      const base::Optional<double>& min_frame_rate,
      const base::Optional<double>& pan,
      const base::Optional<double>& tilt,
      const base::Optional<double>& zoom,
      bool pan_tilt_zoom_allowed,
      MediaStreamVideoSource::ConstraintsOnceCallback callback,
      bool enabled);
  ~MediaStreamVideoTrack() override;

  // MediaStreamTrack overrides.
  void SetEnabled(bool enabled) override;
  void SetContentHint(
      WebMediaStreamTrack::ContentHintType content_hint) override;
  void StopAndNotify(base::OnceClosure callback) override;
  void GetSettings(MediaStreamTrackPlatform::Settings& settings) override;

  // Add |sink| to receive state changes on the main render thread and video
  // frames in the |callback| method on the IO-thread.
  // |callback| will be reset on the render thread.
  void AddSink(WebMediaStreamSink* sink,
               const VideoCaptureDeliverFrameCB& callback,
               bool is_sink_secure);
  void RemoveSink(WebMediaStreamSink* sink);

  // Returns the number of currently connected sinks.
  size_t CountSinks() const;

  // Adds |callback| for encoded frame output on the IO thread. The function
  // will cause generation of a keyframe from the source.
  // Encoded sinks are not secure.
  void AddEncodedSink(WebMediaStreamSink* sink, EncodedVideoFrameCB callback);

  // Removes encoded callbacks associated with |sink|.
  void RemoveEncodedSink(WebMediaStreamSink* sink);

  // Returns the number of currently present encoded sinks.
  size_t CountEncodedSinks() const;

  void OnReadyStateChanged(WebMediaStreamSource::ReadyState state);

  const base::Optional<bool>& noise_reduction() const {
    return noise_reduction_;
  }
  bool is_screencast() const { return is_screencast_; }
  const base::Optional<double>& min_frame_rate() const {
    return min_frame_rate_;
  }
  const base::Optional<double>& max_frame_rate() const {
    return max_frame_rate_;
  }
  const VideoTrackAdapterSettings& adapter_settings() const {
    return *adapter_settings_;
  }
  const base::Optional<double>& pan() const { return pan_; }
  const base::Optional<double>& tilt() const { return tilt_; }
  const base::Optional<double>& zoom() const { return zoom_; }
  bool pan_tilt_zoom_allowed() const { return pan_tilt_zoom_allowed_; }

  // Setting information about the track size.
  // Called from MediaStreamVideoSource at track initialization.
  void SetTargetSizeAndFrameRate(int width, int height, double frame_rate) {
    width_ = width;
    height_ = height;
    frame_rate_ = frame_rate;
  }

  // Setting information about the track size.
  // Passed as callback on MediaStreamVideoTrack::AddTrack, and run from
  // VideoFrameResolutionAdapter on frame delivery to update track settings.
  void SetSizeAndComputedFrameRate(gfx::Size frame_size, double frame_rate) {
    width_ = frame_size.width();
    height_ = frame_size.height();
    computed_frame_rate_ = frame_rate;
  }

  void SetMinimumFrameRate(double min_frame_rate);

  // Setting information about the source format. The format is computed based
  // on incoming frames and it's used for applying constraints for remote video
  // tracks. Passed as callback on MediaStreamVideoTrack::AddTrack, and run from
  // VideoFrameResolutionAdapter on frame delivery.
  void set_computed_source_format(const media::VideoCaptureFormat& format) {
    computed_source_format_ = format;
  }

  void SetTrackAdapterSettings(const VideoTrackAdapterSettings& settings);

  media::VideoCaptureFormat GetComputedSourceFormat();

  MediaStreamVideoSource* source() const { return source_.get(); }

  void OnFrameDropped(media::VideoCaptureFrameDropReason reason);

  bool IsRefreshFrameTimerRunningForTesting() {
    return refresh_timer_.IsRunning();
  }

  void SetIsScreencastForTesting(bool is_screencast) {
    is_screencast_ = is_screencast;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaStreamRemoteVideoSourceTest, StartTrack);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamRemoteVideoSourceTest, RemoteTrackStop);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamRemoteVideoSourceTest,
                           PreservesColorSpace);
  FRIEND_TEST_ALL_PREFIXES(PepperToVideoTrackAdapterTest, PutFrame);

  void UpdateSourceCapturingSecure();
  void UpdateSourceHasConsumers();

  void RequestRefreshFrame();
  void StartTimerForRequestingFrames();
  void ResetRefreshTimer();

  // In debug builds, check that all methods that could cause object graph
  // or data flow changes are being called on the main thread.
  THREAD_CHECKER(main_render_thread_checker_);

  Vector<WebMediaStreamSink*> sinks_;
  Vector<WebMediaStreamSink*> encoded_sinks_;

  // |FrameDeliverer| is an internal helper object used for delivering video
  // frames on the IO-thread using callbacks to all registered tracks.
  class FrameDeliverer;
  scoped_refptr<FrameDeliverer> frame_deliverer_;

  // TODO(guidou): Make this field a regular field instead of a unique_ptr.
  std::unique_ptr<VideoTrackAdapterSettings> adapter_settings_;
  base::Optional<bool> noise_reduction_;
  bool is_screencast_;
  base::Optional<double> min_frame_rate_;
  base::Optional<double> max_frame_rate_;
  base::Optional<double> pan_;
  base::Optional<double> tilt_;
  base::Optional<double> zoom_;
  bool pan_tilt_zoom_allowed_ = false;

  // Weak ref to the source this tracks is connected to.
  base::WeakPtr<MediaStreamVideoSource> source_;

  // This is used for tracking if all connected video sinks are secure.
  SecureDisplayLinkTracker<WebMediaStreamSink> secure_tracker_;

  // Remembering our desired video size and frame rate.
  int width_ = 0;
  int height_ = 0;
  double frame_rate_ = 0.0;
  base::Optional<double> computed_frame_rate_;
  media::VideoCaptureFormat computed_source_format_;
  base::RepeatingTimer refresh_timer_;

  base::WeakPtrFactory<MediaStreamVideoTrack> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MediaStreamVideoTrack);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_VIDEO_TRACK_H_
