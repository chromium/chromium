// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_METRICS_H_
#define MEDIA_REMOTING_METRICS_H_

#include <optional>

#include "base/time/time.h"
#include "media/base/pipeline_metadata.h"
#include "media/mojo/mojom/remoting_common.mojom.h"
#include "media/remoting/triggers.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace remoting {

// The compatibility of a media content with remoting, and the reasons for
// incompatibilities.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class RemotingCompatibility {
  kCompatible = 0,
  kNoAudioNorVideo = 1,
  kEncryptedVideo = 2,
  kIncompatibleVideoCodec = 3,
  kEncryptedAudio = 4,
  kIncompatibleAudioCodec = 5,
  kDisabledByPage = 6,
  kDurationBelowThreshold = 7,
  // Add new values here. Don't re-number existing values.

  kMaxValue = kDurationBelowThreshold,
};

// The rate of pixels in a video and whether the receiver supports its playback.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PixelRateSupport {
  // Pixels per second is at most the equivalent of 1080p 30fps.
  k2kSupported = 0,
  // More than 1080p 30fps and at most 2160p 30fps.
  k4kSupported = 1,
  k4kNotSupported = 2,
  kOver4kNotSupported = 3,
  // Add new values here. Don't re-number existing values.

  kMaxValue = kOver4kNotSupported,
};

class SessionMetricsRecorder {
 public:
  SessionMetricsRecorder();

  SessionMetricsRecorder(const SessionMetricsRecorder&) = delete;
  SessionMetricsRecorder& operator=(const SessionMetricsRecorder&) = delete;

  ~SessionMetricsRecorder();

  // When attempting to start a remoting session, WillStartSession() is called,
  // followed by either OnSessionStartSucceeded() or OnSessionStop() to indicate
  // whether the start succeeded. Later, OnSessionStop() is called when the
  // session ends.
  void WillStartSession(StartTrigger trigger);
  void DidStartSession();
  void StartSessionFailed(mojom::RemotingStartFailReason reason);
  void WillStopSession(StopTrigger trigger);

  // These may be called before, during, or after a remoting session.
  void OnPipelineMetadataChanged(const PipelineMetadata& metadata);
  void OnRemotePlaybackDisabled(bool disabled);

  // Records the rate of pixels in a video (bucketed into FHD, 4K, etc.) and
  // whether the receiver supports its playback. Records only on the first call
  // for the recorder instance.
  void RecordVideoPixelRateSupport(PixelRateSupport support);

  // Records the compatibility of a media content with remoting. Records only on
  // the first call for the recorder instance.
  void RecordCompatibility(RemotingCompatibility compatibility);

 private:
  // Whether audio only, video only, or both were played during the session.
  //
  // NOTE: Never re-number or re-use numbers here. These are used in UMA
  // histograms, and must remain backwards-compatible for all time. However,
  // *do* change TRACK_CONFIGURATION_MAX to one after the greatest value when
  // adding new ones. Also, don't forget to update histograms.xml!
  enum TrackConfiguration {
    NEITHER_AUDIO_NOR_VIDEO = 0,
    AUDIO_ONLY = 1,
    VIDEO_ONLY = 2,
    AUDIO_AND_VIDEO = 3,

    TRACK_CONFIGURATION_MAX = 3,
  };

  // Helper methods to record media configuration at relevant times.
  void RecordAudioConfiguration();
  void RecordVideoConfiguration();
  void RecordTrackConfiguration();

  // |start_trigger_| is set while a remoting session is active.
  std::optional<StartTrigger> start_trigger_;

  // When the current (or last) remoting session started.
  base::TimeTicks start_time_;

  // Last known audio and video configuration. These can change before/after a
  // remoting session as well as during one.
  AudioCodec last_audio_codec_;
  ChannelLayout last_channel_layout_;
  int last_sample_rate_;
  VideoCodec last_video_codec_;
  VideoCodecProfile last_video_profile_;
  gfx::Size last_natural_size_;

  // Last known disabled playback state. This can change before/after a remoting
  // session as well as during one.
  std::optional<bool> remote_playback_is_disabled_;

  bool did_record_pixel_rate_support_ = false;
  bool did_record_compatibility_ = false;
};

class RendererMetricsRecorder {
 public:
  RendererMetricsRecorder();

  RendererMetricsRecorder(const RendererMetricsRecorder&) = delete;
  RendererMetricsRecorder& operator=(const RendererMetricsRecorder&) = delete;

  ~RendererMetricsRecorder();

  // Called when an "initialize success" message is received from the remote.
  void OnRendererInitialized();

  // Called whenever there is direct (or indirect, but close-in-time) evidence
  // that playout has occurred.
  void OnEvidenceOfPlayoutAtReceiver();

  // These are called at regular intervals throughout the session to provide
  // estimated data flow rates.
  void OnAudioRateEstimate(int kilobits_per_second);
  void OnVideoRateEstimate(int kilobits_per_second);

 private:
  const base::TimeTicks start_time_;
  bool did_record_first_playout_ = false;
};

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_METRICS_H_
