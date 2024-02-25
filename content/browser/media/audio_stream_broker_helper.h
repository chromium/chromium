// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_AUDIO_STREAM_BROKER_HELPER_H_
#define CONTENT_BROWSER_MEDIA_AUDIO_STREAM_BROKER_HELPER_H_

namespace content {

// Thread-safe utility that notifies the RenderFrameHost identified by
// `render_process_id` and `render_frame_id`` of a started stream to ensure that
// the renderer is not backgrounded. Must be paired with a later call to
// NotifyHostOfStoppedStream().
// `is_capturing` indicates if the audio stream is capturing a user input. For
// example, a stream capturing the microphone input.
void NotifyFrameHostOfAudioStreamStarted(int render_process_id,
                                         int render_frame_id,
                                         bool is_capturing);
void NotifyFrameHostOfAudioStreamStopped(int render_process_id,
                                         int render_frame_id,
                                         bool is_capturing);

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_AUDIO_STREAM_BROKER_HELPER_H_
