// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_LOG_EVENTS_H_
#define MEDIA_BASE_MEDIA_LOG_EVENTS_H_

#include <string>

#include "media/base/media_export.h"
#include "media/base/media_log_type_enforcement.h"
#include "media/base/media_track.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// Maximum length of urls that we can show.
constexpr size_t kMaxUrlLength = 1000;

// Events are changes in the state of a player, or a user interaction, or any
// other internal representation of a player at a given point in time.
// This list contains both events that are instant, such as play/pause, as
// well as events that span ranges of time, such as waiting for more data
// from the network, or decoding a video frame.
enum class MediaLogEvent {
  // The media player has started playing.
  kPlay,

  // The media player has entered a paused state.
  kPause,

  // The media player has _started_ a seek operation.
  kSeek,

  // The pipeline state has changed - see PipelineStatus in
  // media/base/pipeline_status.h
  kPipelineStateChange,

  // The media stack implementation of the blink media player has been created
  // but may not be fully initialized.
  kWebMediaPlayerCreated,

  // The media player has been destroyed and the log will soon die. No events
  // can come after receiving this one.
  kWebMediaPlayerDestroyed,

  // A web request has finished and the pipeline will start iminently.
  kLoad,

  // The video size has changed.
  // TODO(tmathmeyer) This is already a property, it might be useless to have it
  // be an event too. consider removing it.
  kVideoSizeChanged,

  // The runtime of the video was changed by the demuxer.
  kDurationChanged,

  // There is no more content to consume.
  kEnded,

  // There was a change to the buffering state of the video. This can be caused
  // by either network slowness or decoding slowness. See the comments in
  // media/base/buffering_state.h for more information.
  kBufferingStateChanged,

  // The player has been suspended to save resources.
  kSuspended,

  // An internal-only event that the media log sends when it is created, and
  // includes a wall-clock timestamp.
  kMediaLogCreated,

  // The media player video "occlusion state" has changed, where "occlusion
  // state" represents the `MediaVideoVisibilityTracker` state related to
  // occlusion computations. This is recorded at the time that the video
  // visibility is reported, not for every occlusion computation.
  //
  // "Visibility" in the context of the `MediaVideoVisibilityTracker` is defined
  // as having an `HTMLVideoElement` that is intersecting with the viewport and
  // is not occluded by other html elements within the page, with the exception
  // of MediaControls.
  //
  // These logs only apply to HTMLVideoElement. Other media elements will not
  // log these events.
  kVideoOcclusionState,

  // Triggered whenever WMPI handles a track change.
  kAudioTrackChange,
  kVideoTrackChange,
};

// Sometimes URLs can have encoded data that can be exteremly large.
MEDIA_EXPORT std::string TruncateUrlString(const std::string& url);

// These events can be triggered with no extra associated data.
MEDIA_LOG_EVENT_TYPELESS(kPlay);
MEDIA_LOG_EVENT_TYPELESS(kPause);
MEDIA_LOG_EVENT_TYPELESS(kWebMediaPlayerDestroyed);
MEDIA_LOG_EVENT_TYPELESS(kEnded);
MEDIA_LOG_EVENT_TYPELESS(kSuspended);
MEDIA_LOG_EVENT_TYPELESS(kWebMediaPlayerCreated);

// These events can be triggered with the extra data / names as defined here.
// Note that some events can be defined multiple times.
MEDIA_LOG_EVENT_NAMED_DATA(kSeek, double, "seek_target");
MEDIA_LOG_EVENT_NAMED_DATA(kVideoSizeChanged, gfx::Size, "dimensions");
MEDIA_LOG_EVENT_NAMED_DATA(kDurationChanged, base::TimeDelta, "duration");
MEDIA_LOG_EVENT_NAMED_DATA(kPipelineStateChange, std::string, "pipeline_state");
MEDIA_LOG_EVENT_NAMED_DATA_OP(kLoad, std::string, "url", TruncateUrlString);
MEDIA_LOG_EVENT_NAMED_DATA_OP(kWebMediaPlayerCreated,
                              std::string,
                              "origin_url",
                              TruncateUrlString);
MEDIA_LOG_EVENT_NAMED_DATA(kMediaLogCreated, base::Time, "created");
MEDIA_LOG_EVENT_NAMED_DATA(kVideoOcclusionState,
                           std::string,
                           "video_occlusion_state");

// Each type of buffering state gets a different name.
MEDIA_LOG_EVENT_NAMED_DATA(
    kBufferingStateChanged,
    SerializableBufferingState<SerializableBufferingStateType::kVideo>,
    "video_buffering_state");
MEDIA_LOG_EVENT_NAMED_DATA(
    kBufferingStateChanged,
    SerializableBufferingState<SerializableBufferingStateType::kAudio>,
    "audio_buffering_state");
MEDIA_LOG_EVENT_NAMED_DATA(
    kBufferingStateChanged,
    SerializableBufferingState<SerializableBufferingStateType::kPipeline>,
    "pipeline_buffering_state");

MEDIA_LOG_EVENT_NAMED_DATA(kAudioTrackChange,
                           std::vector<MediaTrack::Id>,
                           "audio_tracks_enabled");
MEDIA_LOG_EVENT_NAMED_DATA(kVideoTrackChange,
                           std::optional<MediaTrack::Id>,
                           "video_track_selected");

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_LOG_EVENTS_H_
