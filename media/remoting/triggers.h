// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_TRIGGERS_H_
#define MEDIA_REMOTING_TRIGGERS_H_

namespace media {
namespace remoting {

// Events and conditions that can trigger remoting to start.
//
// NOTE: Never re-number or re-use numbers for different triggers. These are
// used in UMA histograms, and must remain backwards-compatible for all time.
// However, *do* change START_TRIGGER_MAX to one after the greatest value when
// adding new ones. Also, don't forget to update histograms.xml!
enum StartTrigger {
  UNKNOWN_START_TRIGGER = 0,

  // Local presentation changes.
  ENTERED_FULLSCREEN = 1,  // The media element became the fullscreen element.
  BECAME_DOMINANT_CONTENT = 2,  // Element now occupies most of the viewport.
  ENABLED_BY_PAGE = 3,          // The page re-allowed remote playback.

  // User actions, such as connecting to a receiver or pressing play.
  SINK_AVAILABLE = 4,         // A receiver (sink) became available.
  PLAY_COMMAND = 5,           // The media element was issued a play command.
  REQUESTED_BY_BROWSER = 10,  // The browser requested to start Media Remoting
                              // without fullscreen-in-tab.

  // Met requirements for playback of the media.
  SUPPORTED_AUDIO_CODEC = 6,  // Stream began using a supported audio codec.
  SUPPORTED_VIDEO_CODEC = 7,  // Stream began using a supported video codec.
  SUPPORTED_AUDIO_AND_VIDEO_CODECS = 8,  // Both now using a supported codec.
  CDM_READY = 9,  // The CDM required for decrypting the content became ready.
  PIXEL_RATE_READY = 11,  // The pixel rate was calculated.

  // Change this to the highest value.
  START_TRIGGER_MAX = 11,
};

// Events and conditions that can result in a start failure, or trigger remoting
// to stop.
//
// NOTE: Never re-number or re-use numbers for different triggers. These are
// used in UMA histograms, and must remain backwards-compatible for all time.
//
// ADDITIONAL NOTE: The values are intentionally out-of-order to maintain a
// logical grouping. When adding a new value, add one to STOP_TRIGGER_MAX, then
// update STOP_TRIGGER_MAX. Also, don't forget to update enums.xml!
enum StopTrigger {
  UNKNOWN_STOP_TRIGGER = 0,

  // Normal shutdown triggers.
  ROUTE_TERMINATED = 1,  // The route to the sink was terminated (user action?).
  MEDIA_ELEMENT_DESTROYED = 2,   // The media element on the page was destroyed.
  EXITED_FULLSCREEN = 3,         // The media element is no longer fullscreened.
  BECAME_AUXILIARY_CONTENT = 4,  // Element no longer occupies the viewport.
  DISABLED_BY_PAGE = 5,  // The web page blocked remoting during a session.

  // Content playback related errors forcing shutdown (or failing start).
  START_RACE = 6,  // Multiple remoting sessions attempted to start.
  UNSUPPORTED_AUDIO_CODEC = 7,  // Stream now using an unsupported audio codec.
  UNSUPPORTED_VIDEO_CODEC = 8,  // Stream now using an unsupported video codec.
  UNSUPPORTED_AUDIO_AND_VIDEO_CODECS = 9,  // Neither codec is supported.
  DECRYPTION_ERROR = 10,  // Could not decrypt content or CDM was destroyed.
  RECEIVER_INITIALIZE_FAILED = 11,  // The receiver reported a failed init.
  RECEIVER_PIPELINE_ERROR = 12,  // The media pipeline on the receiver error'ed.

  // Environmental errors forcing shutdown.
  FRAME_DROP_RATE_HIGH = 13,  // The receiver was dropping too many frames.
  PACING_TOO_SLOWLY = 14,     // Play-out was too slow, indicating bottlenecks.

  // Communications errors forcing shutdown.
  PEERS_OUT_OF_SYNC = 15,  // The local state disagrees with the remote.
  RPC_INVALID = 16,        // An RPC field value is missing or has bad data.
  DATA_PIPE_CREATE_ERROR = 17,  // Mojo data pipe creation failed (OOM?).
  MOJO_DISCONNECTED = 18,       // Mojo message pipe was disconnected; e.g, the
                                // browser shut down.
  DATA_PIPE_WRITE_ERROR = 24,   // Failure to write the mojo data pipe.

  // Message/Data sending errors forcing shutdown.
  MESSAGE_SEND_FAILED = 19,  // Failed to send a RPC message to the sink.
  DATA_SEND_FAILED = 20,     // Failed to pull from pipe or send to the sink.
  UNEXPECTED_FAILURE = 21,   // Unexpected failure or inconsistent state.
  SERVICE_GONE = 22,         // Mirror service disconnected.

  // User changing setting forcing shutdown.
  USER_DISABLED = 23,  // Media Remoting was disabled by user.

  // Media element was frozen (e.g. page was navigated away).
  MEDIA_ELEMENT_FROZEN = 25,

  // Change this to the highest value.
  STOP_TRIGGER_MAX = MEDIA_ELEMENT_FROZEN,
};

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_TRIGGERS_H_
