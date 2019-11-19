// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_VIDEO_TRACK_ADAPTER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_VIDEO_TRACK_ADAPTER_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "media/base/video_frame.h"
#include "third_party/blink/public/platform/modules/mediastream/media_stream_types.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class VideoTrackAdapterSettings;

// VideoTrackAdapter is a helper class used by MediaStreamVideoSource used for
// adapting the video resolution from a source implementation to the resolution
// a track requires. Different tracks can have different resolution constraints.
// The constraints can be set as max width and height as well as max and min
// aspect ratio.
// Video frames are delivered to a track using a VideoCaptureDeliverFrameCB on
// the IO-thread.
// Adaptations is done by wrapping the original media::VideoFrame in a new
// media::VideoFrame with a new visible_rect and natural_size.
class MODULES_EXPORT VideoTrackAdapter
    : public WTF::ThreadSafeRefCounted<VideoTrackAdapter> {
 public:
  using OnMutedCallback = base::RepeatingCallback<void(bool mute_state)>;

  VideoTrackAdapter(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      base::WeakPtr<MediaStreamVideoSource> media_stream_video_source);

  // Register |track| to receive video frames in |frame_callback| with
  // a resolution within the boundaries of the arguments, and settings
  // updates in |settings_callback|.
  // Must be called on the main render thread.
  // |source_frame_rate| is used to calculate a prudent interval to check for
  // passing frames and inform of the result via |on_muted_state_callback|.
  void AddTrack(const MediaStreamVideoTrack* track,
                VideoCaptureDeliverFrameCB frame_callback,
                VideoTrackSettingsCallback settings_callback,
                VideoTrackFormatCallback track_callback,
                const VideoTrackAdapterSettings& settings);
  void RemoveTrack(const MediaStreamVideoTrack* track);
  void ReconfigureTrack(const MediaStreamVideoTrack* track,
                        const VideoTrackAdapterSettings& settings);

  // Delivers |frame| to all tracks that have registered a callback.
  // Must be called on the IO-thread.
  void DeliverFrameOnIO(scoped_refptr<media::VideoFrame> frame,
                        base::TimeTicks estimated_capture_time);

  base::SingleThreadTaskRunner* io_task_runner() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return io_task_runner_.get();
  }

  // Start monitor that frames are delivered to this object. I.E, that
  // |DeliverFrameOnIO| is called with a frame rate of |source_frame_rate|.
  // |on_muted_callback| is triggered on the main render thread.
  void StartFrameMonitoring(double source_frame_rate,
                            const OnMutedCallback& on_muted_callback);
  void StopFrameMonitoring();

  void SetSourceFrameSize(const gfx::Size& source_frame_size);

  // Exported for testing.
  //
  // Calculates the desired size of a VideoTrack instance, and returns true if
  // |desired_size| is updated successfully, false otherwise.
  // |desired_size| is not updated if |settings| has rescaling disabled and
  // |input_size| is invalid.
  static bool CalculateDesiredSize(bool is_rotated,
                                   const gfx::Size& input_size,
                                   const VideoTrackAdapterSettings& settings,
                                   gfx::Size* desired_size);

 private:
  virtual ~VideoTrackAdapter();
  friend class WTF::ThreadSafeRefCounted<VideoTrackAdapter>;

  // These aliases mimic the definition of VideoCaptureDeliverFrameCB,
  // VideoTrackSettingsCallback and VideoTrackFormatCallback respectively.
  using VideoCaptureDeliverFrameInternalCallback =
      WTF::CrossThreadFunction<void(
          scoped_refptr<media::VideoFrame> video_frame,
          base::TimeTicks estimated_capture_time)>;
  using VideoTrackSettingsInternalCallback =
      WTF::CrossThreadFunction<void(gfx::Size frame_size, double frame_rate)>;
  using VideoTrackFormatInternalCallback =
      WTF::CrossThreadFunction<void(const media::VideoCaptureFormat&)>;
  void AddTrackOnIO(const MediaStreamVideoTrack* track,
                    VideoCaptureDeliverFrameInternalCallback frame_callback,
                    VideoTrackSettingsInternalCallback settings_callback,
                    VideoTrackFormatInternalCallback track_callback,
                    const VideoTrackAdapterSettings& settings);

  void RemoveTrackOnIO(const MediaStreamVideoTrack* track);
  void ReconfigureTrackOnIO(const MediaStreamVideoTrack* track,
                            const VideoTrackAdapterSettings& settings);

  using OnMutedInternalCallback =
      WTF::CrossThreadFunction<void(bool mute_state)>;
  void StartFrameMonitoringOnIO(OnMutedInternalCallback on_muted_state_callback,
                                double source_frame_rate);
  void StopFrameMonitoringOnIO();
  void SetSourceFrameSizeOnIO(const gfx::Size& frame_size);

  // Compare |frame_counter_snapshot| with the current |frame_counter_|, and
  // inform of the situation (muted, not muted) via |set_muted_state_callback|.
  void CheckFramesReceivedOnIO(OnMutedInternalCallback set_muted_state_callback,
                               uint64_t old_frame_counter_snapshot);

  // |thread_checker_| is bound to the main render thread.
  THREAD_CHECKER(thread_checker_);

  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  base::WeakPtr<MediaStreamVideoSource> media_stream_video_source_;

  // |renderer_task_runner_| is used to ensure that
  // VideoCaptureDeliverFrameCB is released on the main render thread.
  const scoped_refptr<base::SingleThreadTaskRunner> renderer_task_runner_;

  // VideoFrameResolutionAdapter is an inner class that lives on the IO-thread.
  // It does the resolution adaptation and delivers frames to all registered
  // tracks.
  class VideoFrameResolutionAdapter;
  using FrameAdapters = WTF::Vector<scoped_refptr<VideoFrameResolutionAdapter>>;
  FrameAdapters adapters_;

  // Set to true if frame monitoring has been started. It is only accessed on
  // the IO-thread.
  bool monitoring_frame_rate_;

  // Keeps track of it frames have been received. It is only accessed on the
  // IO-thread.
  bool muted_state_;

  // Running frame counter, accessed on the IO-thread.
  uint64_t frame_counter_;

  // Frame rate configured on the video source, accessed on the IO-thread.
  float source_frame_rate_;

  // Resolution configured on the video source, accessed on the IO-thread.
  base::Optional<gfx::Size> source_frame_size_;

  DISALLOW_COPY_AND_ASSIGN(VideoTrackAdapter);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_VIDEO_TRACK_ADAPTER_H_
