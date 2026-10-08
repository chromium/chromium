// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_output_stream_observer_impl.h"

#include "content/browser/media/audio_stream_monitor.h"
#include "content/public/browser/browser_thread.h"

namespace content {

AudioOutputStreamObserverImpl::AudioOutputStreamObserverImpl(
    int render_process_id,
    int render_frame_id,
    int stream_id)
    : render_frame_host_id_(render_process_id, render_frame_id),
      stream_id_(stream_id) {}

AudioOutputStreamObserverImpl::~AudioOutputStreamObserverImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (did_start_playing_) {
    DidStopPlaying();
  }
}

void AudioOutputStreamObserverImpl::DidStartPlaying() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  did_start_playing_ = true;
  AudioStreamMonitor::StartMonitoringStream(render_frame_host_id_, stream_id_);
}
void AudioOutputStreamObserverImpl::DidStopPlaying() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AudioStreamMonitor::StopMonitoringStream(render_frame_host_id_, stream_id_);
  did_start_playing_ = false;
  is_audible_ = false;
}

void AudioOutputStreamObserverImpl::DidChangeAudibleState(bool is_audible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_audible_ = is_audible;
  AudioStreamMonitor::UpdateStreamAudibleState(render_frame_host_id_,
                                               stream_id_, is_audible);
}

void AudioOutputStreamObserverImpl::Reparent(int new_render_frame_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const GlobalRenderFrameHostId new_rfh_id(render_frame_host_id_.child_id,
                                           new_render_frame_id);
  if (render_frame_host_id_ == new_rfh_id) {
    return;
  }
  if (did_start_playing_) {
    // Register the stream under the new frame before removing the old one.
    // Note: During RenderFrameDeleted(), AudioStreamMonitor::RenderFrameDeleted
    // runs synchronously on the UI thread and may have already cleared the old
    // frame's entry (causing a brief UI->IO->UI blip in IsCurrentlyAudible(),
    // though WasRecentlyAudible() remains true due to its 2s hold timer).
    AudioStreamMonitor::StartMonitoringStream(new_rfh_id, stream_id_);
    if (is_audible_) {
      AudioStreamMonitor::UpdateStreamAudibleState(new_rfh_id, stream_id_,
                                                   true);
    }
    AudioStreamMonitor::StopMonitoringStream(render_frame_host_id_, stream_id_);
  }
  render_frame_host_id_ = new_rfh_id;
}

}  // namespace content
