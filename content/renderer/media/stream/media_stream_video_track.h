// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_VIDEO_TRACK_H_
#define CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_VIDEO_TRACK_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "content/renderer/media/stream/media_stream_track.h"
#include "content/renderer/media/stream/media_stream_video_source.h"
#include "content/renderer/media/stream/secure_display_link_tracker.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"

namespace content {

class VideoTrackAdapterSettings;

// MediaStreamVideoTrack is a video specific representation of a
// blink::WebMediaStreamTrack in content. It is owned by the blink object
// and can be retrieved from a blink object using
// WebMediaStreamTrack::getExtraData() or MediaStreamVideoTrack::GetVideoTrack.
class CONTENT_EXPORT MediaStreamVideoTrack : public MediaStreamTrack {
 public:
  // Help method to create a blink::WebMediaStreamTrack and a
  // MediaStreamVideoTrack instance. The MediaStreamVideoTrack object is owned
  // by the blink object in its WebMediaStreamTrack::ExtraData member.
  // |callback| is triggered if the track is added to the source
  // successfully and will receive video frames that match the given settings
  // or if the source fails to provide video frames.
  // If |enabled| is true, sinks added to the track will
  // receive video frames when the source delivers frames to the track.
  static blink::WebMediaStreamTrack CreateVideoTrack(
      MediaStreamVideoSource* source,
      const MediaStreamVideoSource::ConstraintsCallback& callback,
      bool enabled);
  static blink::WebMediaStreamTrack CreateVideoTrack(
      const blink::WebString& id,
      MediaStreamVideoSource* source,
      const MediaStreamVideoSource::ConstraintsCallback& callback,
      bool enabled);
  static blink::WebMediaStreamTrack CreateVideoTrack(
      MediaStreamVideoSource* source,
      const VideoTrackAdapterSettings& adapter_settings,
      const base::Optional<bool>& noise_reduction,
      bool is_screencast,
      const base::Optional<double>& min_frame_rate,
      const MediaStreamVideoSource::ConstraintsCallback& callback,
      bool enabled);

  static MediaStreamVideoTrack* GetVideoTrack(
      const blink::WebMediaStreamTrack& track);

  // Constructors for video tracks.
  MediaStreamVideoTrack(
      MediaStreamVideoSource* source,
      const MediaStreamVideoSource::ConstraintsCallback& callback,
      bool enabled);
  MediaStreamVideoTrack(
      MediaStreamVideoSource* source,
      const VideoTrackAdapterSettings& adapter_settings,
      const base::Optional<bool>& noise_reduction,
      bool is_screen_cast,
      const base::Optional<double>& min_frame_rate,
      const MediaStreamVideoSource::ConstraintsCallback& callback,
      bool enabled);
  ~MediaStreamVideoTrack() override;

  // MediaStreamTrack overrides.
  void SetEnabled(bool enabled) override;
  void SetContentHint(
      blink::WebMediaStreamTrack::ContentHintType content_hint) override;
  void StopAndNotify(base::OnceClosure callback) override;
  void GetSettings(blink::WebMediaStreamTrack::Settings& settings) override;

  void OnReadyStateChanged(blink::WebMediaStreamSource::ReadyState state);

  const base::Optional<bool>& noise_reduction() const {
    return noise_reduction_;
  }
  bool is_screencast() const {
    return is_screencast_;
  }
  const base::Optional<double>& min_frame_rate() const {
    return min_frame_rate_;
  }
  const base::Optional<double>& max_frame_rate() const {
    return max_frame_rate_;
  }
  const VideoTrackAdapterSettings& adapter_settings() const {
    return *adapter_settings_;
  }

  // Setting information about the track size.
  // Called from MediaStreamVideoSource at track initialization.
  void SetTargetSizeAndFrameRate(int width, int height, double frame_rate) {
    width_ = width;
    height_ = height;
    frame_rate_ = frame_rate;
  }

  MediaStreamVideoSource* source() const { return source_.get(); }

 private:
  // MediaStreamVideoSink is a friend to allow it to call AddSink() and
  // RemoveSink().
  friend class MediaStreamVideoSink;
  FRIEND_TEST_ALL_PREFIXES(MediaStreamRemoteVideoSourceTest, StartTrack);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamRemoteVideoSourceTest, RemoteTrackStop);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamRemoteVideoSourceTest,
                           PreservesColorSpace);
  FRIEND_TEST_ALL_PREFIXES(PepperToVideoTrackAdapterTest, PutFrame);

  // Add |sink| to receive state changes on the main render thread and video
  // frames in the |callback| method on the IO-thread.
  // |callback| will be reset on the render thread.
  // These two methods are private such that no subclass can intercept and
  // store the callback. This is important to ensure that we can release
  // the callback on render thread without reference to it on the IO-thread.
  void AddSink(MediaStreamVideoSink* sink,
               const VideoCaptureDeliverFrameCB& callback,
               bool is_sink_secure);
  void RemoveSink(MediaStreamVideoSink* sink);

  std::vector<MediaStreamVideoSink*> sinks_;

  // |FrameDeliverer| is an internal helper object used for delivering video
  // frames on the IO-thread using callbacks to all registered tracks.
  class FrameDeliverer;
  const scoped_refptr<FrameDeliverer> frame_deliverer_;

  // TODO(guidou): Make this field a regular field instead of a unique_ptr.
  std::unique_ptr<VideoTrackAdapterSettings> adapter_settings_;
  base::Optional<bool> noise_reduction_;
  bool is_screencast_;
  base::Optional<double> min_frame_rate_;
  base::Optional<double> max_frame_rate_;

  // Weak ref to the source this tracks is connected to.
  base::WeakPtr<MediaStreamVideoSource> source_;

  // This is used for tracking if all connected video sinks are secure.
  SecureDisplayLinkTracker<MediaStreamVideoSink> secure_tracker_;

  // Remembering our desired video size and frame rate.
  int width_ = 0;
  int height_ = 0;
  double frame_rate_ = 0.0;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamVideoTrack);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_VIDEO_TRACK_H_
