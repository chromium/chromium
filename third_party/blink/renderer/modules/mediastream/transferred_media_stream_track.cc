// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/transferred_media_stream_track.h"

#include <memory>

#include "base/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_track.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_capture_handle_change_event.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_capture_handle_change_event_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_double_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_long_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_capabilities.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_settings.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_point_2d.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/imagecapture/image_capture.h"
#include "third_party/blink/renderer/modules/mediastream/apply_constraints_request.h"
#include "third_party/blink/renderer/modules/mediastream/browser_capture_media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/capture_handle_change_event.h"
#include "third_party/blink/renderer/modules/mediastream/media_constraints_impl.h"
#include "third_party/blink/renderer/modules/mediastream/media_error_state.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_utils.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/overconstrained_error.h"
#include "third_party/blink/renderer/modules/mediastream/processed_local_audio_source.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_controller.h"
#include "third_party/blink/renderer/modules/mediastream/webaudio_media_stream_audio_sink.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_web_audio_source.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

TransferredMediaStreamTrack::TransferredMediaStreamTrack(
    ExecutionContext* context)
    : MediaStreamTrack(context,
                       MakeGarbageCollected<MediaStreamComponent>(
                           MakeGarbageCollected<MediaStreamSource>(
                               "dummy",
                               MediaStreamSource::StreamType::kTypeVideo,
                               "dummy",
                               false /* remote */))) {}

String TransferredMediaStreamTrack::kind() const {
  if (track_) {
    return track_->kind();
  }
  // TODO(https://crbug.com/1288839): return the transferred value.
  return "video";
}

String TransferredMediaStreamTrack::id() const {
  if (track_) {
    return track_->id();
  }
  // TODO(https://crbug.com/1288839): return the transferred value.
  return "";
}

String TransferredMediaStreamTrack::label() const {
  if (track_) {
    return track_->label();
  }
  // TODO(https://crbug.com/1288839): return the transferred value.
  return "dummy";
}

bool TransferredMediaStreamTrack::enabled() const {
  if (track_) {
    return track_->enabled();
  }
  // TODO(https://crbug.com/1288839): return the transferred value.
  return true;
}

void TransferredMediaStreamTrack::setEnabled(bool enabled) {
  if (track_) {
    track_->setEnabled(enabled);
  }
  // TODO(https://crbug.com/1288839): Save and forward to track_ once it's
  // initialized.
}

bool TransferredMediaStreamTrack::muted() const {
  if (track_) {
    return track_->muted();
  }
  // TODO(https://crbug.com/1288839): return the transferred value.
  return false;
}

String TransferredMediaStreamTrack::ContentHint() const {
  if (track_) {
    return track_->ContentHint();
  }
  // TODO(https://crbug.com/1288839): return the transferred value.
  return "";
}

void TransferredMediaStreamTrack::SetContentHint(const String& content_hint) {
  if (track_) {
    track_->SetContentHint(content_hint);
  }
  // TODO(https://crbug.com/1288839): Save and forward to track_ once it's
  // initialized.
}

String TransferredMediaStreamTrack::readyState() const {
  if (track_) {
    return track_->readyState();
  }
  // TODO(https://crbug.com/1288839): return the transferred value.
  return "live";
}

MediaStreamTrack* TransferredMediaStreamTrack::clone(
    ScriptState* script_state) {
  if (track_) {
    return track_->clone(script_state);
  }
  // TODO(https://crbug.com/1288839): Create another TransferredMediaStreamTrack
  // and call track_->clone() once track_ is initialized.
  return nullptr;
}

void TransferredMediaStreamTrack::stopTrack(
    ExecutionContext* execution_context) {
  if (track_) {
    track_->stopTrack(execution_context);
  }
  // TODO(https://crbug.com/1288839): Save and forward to track_ once it's
  // initialized.
}

MediaTrackCapabilities* TransferredMediaStreamTrack::getCapabilities() const {
  if (track_) {
    return track_->getCapabilities();
  }
  // TODO(https://crbug.com/1288839): return the transferred value.
  return MediaTrackCapabilities::Create();
}

MediaTrackConstraints* TransferredMediaStreamTrack::getConstraints() const {
  if (track_) {
    return track_->getConstraints();
  }
  // TODO(https://crbug.com/1288839): return the transferred value.
  return MediaTrackConstraints::Create();
}

MediaTrackSettings* TransferredMediaStreamTrack::getSettings() const {
  if (track_) {
    return track_->getSettings();
  }
  // TODO(https://crbug.com/1288839): return the transferred value.
  return MediaTrackSettings::Create();
}

CaptureHandle* TransferredMediaStreamTrack::getCaptureHandle() const {
  if (track_) {
    return track_->getCaptureHandle();
  }
  // TODO(https://crbug.com/1288839): return the transferred value.
  return CaptureHandle::Create();
}

ScriptPromise TransferredMediaStreamTrack::applyConstraints(
    ScriptState* script_state,
    const MediaTrackConstraints* constrints) {
  if (track_) {
    return track_->applyConstraints(script_state, constrints);
  }
  // TODO(https://crbug.com/1288839): return a promise which resolves once
  // track_ is set.
  return ScriptPromise();
}

void TransferredMediaStreamTrack::setImplementation(MediaStreamTrack* track) {
  track_ = track;
  // TODO(https://crbug.com/1288839): Replay mutations which have happened
  // before this point. Also set up plumbing so that events fired by the
  // implementation track are propagated to anything listening to events on this
  // object.
}

void TransferredMediaStreamTrack::Trace(Visitor* visitor) const {
  MediaStreamTrack::Trace(visitor);
  visitor->Trace(track_);
}

}  // namespace blink
