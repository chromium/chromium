// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/media_controls_media_event_listener.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/track/text_track_list.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/modules/remoteplayback/availability_callback_wrapper.h"
#include "third_party/blink/renderer/modules/remoteplayback/remote_playback.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

MediaControlsMediaEventListener::MediaControlsMediaEventListener(
    MediaControlsImpl* media_controls)
    : media_controls_(media_controls) {
  if (GetMediaElement().isConnected())
    Attach();
}

void MediaControlsMediaEventListener::Attach() {
  DCHECK(GetMediaElement().isConnected());

  GetMediaElement().addEventListener(event_type_names::kVolumechange, this,
                                     /*use_capture=*/false);
  GetMediaElement().addEventListener(event_type_names::kFocusin, this,
                                     /*use_capture=*/false);
  GetMediaElement().addEventListener(event_type_names::kTimeupdate, this,
                                     /*use_capture=*/false);
  GetMediaElement().addEventListener(event_type_names::kPlay, this,
                                     /*use_capture=*/false);
  GetMediaElement().addEventListener(event_type_names::kPlaying, this,
                                     /*use_capture=*/false);
  GetMediaElement().addEventListener(event_type_names::kPause, this,
                                     /*use_capture=*/false);
  GetMediaElement().addEventListener(event_type_names::kDurationchange, this,
                                     /*use_capture=*/false);
  GetMediaElement().addEventListener(event_type_names::kSeeking, this,
                                     /*use_capture=*/false);
  GetMediaElement().addEventListener(event_type_names::kSeeked, this,
                                     /*use_capture=*/false);
  GetMediaElement().addEventListener(event_type_names::kError, this,
                                     /*use_capture=*/false);
  GetMediaElement().addEventListener(event_type_names::kLoadedmetadata, this,
                                     /*use_capture=*/false);
  GetMediaElement().addEventListener(event_type_names::kKeypress, this,
                                     /*use_capture=*/false);
  GetMediaElement().addEventListener(event_type_names::kKeydown, this,
                                     /*use_capture=*/false);
  GetMediaElement().addEventListener(event_type_names::kKeyup, this,
                                     /*use_capture=*/false);
  GetMediaElement().addEventListener(event_type_names::kWaiting, this,
                                     /*use_capture=*/false);
  GetMediaElement().addEventListener(event_type_names::kProgress, this,
                                     /*use_capture=*/false);
  GetMediaElement().addEventListener(event_type_names::kLoadeddata, this,
                                     /*use_capture=*/false);
  GetMediaElement().addEventListener(event_type_names::kPointermove, this,
                                     /*use_capture=*/false);
  GetMediaElement().addEventListener(event_type_names::kPointerout, this,
                                     /*use_capture=*/false);
  GetMediaElement().addEventListener(event_type_names::kPointerenter, this,
                                     /*use_capture=*/false);

  // Listen to two different fullscreen events in order to make sure the new and
  // old APIs are handled.
  GetMediaElement().addEventListener(event_type_names::kWebkitfullscreenchange,
                                     this, /*use_capture=*/false);
  GetMediaElement().addEventListener(event_type_names::kFullscreenchange, this,
                                     /*use_capture=*/false);

  // Picture-in-Picture events.
  if (media_controls_->GetDocument().GetSettings() &&
      media_controls_->GetDocument()
          .GetSettings()
          ->GetPictureInPictureEnabled() &&
      IsA<HTMLVideoElement>(GetMediaElement())) {
    GetMediaElement().addEventListener(event_type_names::kEnterpictureinpicture,
                                       this, /*use_capture=*/false);
    GetMediaElement().addEventListener(event_type_names::kLeavepictureinpicture,
                                       this, /*use_capture=*/false);
  }

  // TextTracks events.
  TextTrackList* text_tracks = GetMediaElement().textTracks();
  text_tracks->addEventListener(event_type_names::kAddtrack, this,
                                /*use_capture=*/false);
  text_tracks->addEventListener(event_type_names::kChange, this,
                                /*use_capture=*/false);
  text_tracks->addEventListener(event_type_names::kRemovetrack, this,
                                /*use_capture=*/false);

  // Keypress events.
  if (media_controls_->ButtonPanelElement()) {
    media_controls_->ButtonPanelElement()->addEventListener(
        event_type_names::kKeypress, this, false);
  }

  RemotePlayback& remote = RemotePlayback::From(GetMediaElement());
  remote.addEventListener(event_type_names::kConnect, this,
                          /*use_capture=*/false);
  remote.addEventListener(event_type_names::kConnecting, this,
                          /*use_capture=*/false);
  remote.addEventListener(event_type_names::kDisconnect, this,
                          /*use_capture=*/false);

  // TODO(avayvod, mlamouri): Attach can be called twice. See
  // https://crbug.com/713275.
  if (!remote_playback_availability_callback_id_.has_value()) {
    remote_playback_availability_callback_id_ =
        std::make_optional(remote.WatchAvailabilityInternal(
            MakeGarbageCollected<AvailabilityCallbackWrapper>(
                WTF::BindRepeating(&MediaControlsMediaEventListener::
                                       OnRemotePlaybackAvailabilityChanged,
                                   WrapWeakPersistent(this)))));
  }
}

void MediaControlsMediaEventListener::Detach() {
  DCHECK(!GetMediaElement().isConnected());

  media_controls_->GetDocument().removeEventListener(
      event_type_names::kFullscreenchange, this, /*use_capture=*/false);

  TextTrackList* text_tracks = GetMediaElement().textTracks();
  text_tracks->removeEventListener(event_type_names::kAddtrack, this,
                                   /*use_capture=*/false);
  text_tracks->removeEventListener(event_type_names::kChange, this,
                                   /*use_capture=*/false);
  text_tracks->removeEventListener(event_type_names::kRemovetrack, this,
                                   /*use_capture=*/false);

  if (media_controls_->ButtonPanelElement()) {
    media_controls_->ButtonPanelElement()->removeEventListener(
        event_type_names::kKeypress, this, /*use_capture=*/false);
  }

  RemotePlayback& remote = RemotePlayback::From(GetMediaElement());
  remote.removeEventListener(event_type_names::kConnect, this,
                             /*use_capture=*/false);
  remote.removeEventListener(event_type_names::kConnecting, this,
                             /*use_capture=*/false);
  remote.removeEventListener(event_type_names::kDisconnect, this,
                             /*use_capture=*/false);

  // TODO(avayvod): apparently Detach() can be called without a previous
  // Attach() call. See https://crbug.com/713275 for more details.
  if (remote_playback_availability_callback_id_.has_value() &&
      remote_playback_availability_callback_id_.value() !=
          RemotePlayback::kWatchAvailabilityNotSupported) {
    remote.CancelWatchAvailabilityInternal(
        remote_playback_availability_callback_id_.value());
    remote_playback_availability_callback_id_.reset();
  }
}

HTMLMediaElement& MediaControlsMediaEventListener::GetMediaElement() {
  return media_controls_->MediaElement();
}

void MediaControlsMediaEventListener::Invoke(
    ExecutionContext* execution_context,
    Event* event) {
  if (event->type() == event_type_names::kVolumechange) {
    media_controls_->OnVolumeChange();
    return;
  }
  if (event->type() == event_type_names::kFocusin) {
    media_controls_->OnFocusIn();
    return;
  }
  if (event->type() == event_type_names::kTimeupdate) {
    media_controls_->OnTimeUpdate();
    return;
  }
  if (event->type() == event_type_names::kDurationchange) {
    media_controls_->OnDurationChange();
    return;
  }
  if (event->type() == event_type_names::kPlay) {
    media_controls_->OnPlay();
    return;
  }
  if (event->type() == event_type_names::kPlaying) {
    media_controls_->OnPlaying();
    return;
  }
  if (event->type() == event_type_names::kPause) {
    media_controls_->OnPause();
    return;
  }
  if (event->type() == event_type_names::kSeeking) {
    media_controls_->OnSeeking();
    return;
  }
  if (event->type() == event_type_names::kSeeked) {
    media_controls_->OnSeeked();
    return;
  }
  if (event->type() == event_type_names::kError) {
    media_controls_->OnError();
    return;
  }
  if (event->type() == event_type_names::kLoadedmetadata) {
    media_controls_->OnLoadedMetadata();
    return;
  }
  if (event->type() == event_type_names::kWaiting) {
    media_controls_->OnWaiting();
    return;
  }
  if (event->type() == event_type_names::kProgress) {
    media_controls_->OnLoadingProgress();
    return;
  }
  if (event->type() == event_type_names::kLoadeddata) {
    media_controls_->OnLoadedData();
    return;
  }

  // Fullscreen handling.
  if (event->type() == event_type_names::kFullscreenchange ||
      event->type() == event_type_names::kWebkitfullscreenchange) {
    if (GetMediaElement().IsFullscreen())
      media_controls_->OnEnteredFullscreen();
    else
      media_controls_->OnExitedFullscreen();
    return;
  }

  // Picture-in-Picture events.
  if (event->type() == event_type_names::kEnterpictureinpicture ||
      event->type() == event_type_names::kLeavepictureinpicture) {
    media_controls_->OnPictureInPictureChanged();
    return;
  }

  // TextTracks events.
  if (event->type() == event_type_names::kAddtrack ||
      event->type() == event_type_names::kRemovetrack) {
    media_controls_->OnTextTracksAddedOrRemoved();
    return;
  }
  if (event->type() == event_type_names::kChange) {
    media_controls_->OnTextTracksChanged();
    return;
  }

  // Keypress events.
  if (event->type() == event_type_names::kKeypress) {
    if (event->currentTarget() == media_controls_->ButtonPanelElement()) {
      media_controls_->OnPanelKeypress();
      return;
    }
  }

  if (event->type() == event_type_names::kKeypress ||
      event->type() == event_type_names::kKeydown ||
      event->type() == event_type_names::kKeyup) {
    media_controls_->OnMediaKeyboardEvent(event);
    return;
  }

  // RemotePlayback state change events.
  if (event->type() == event_type_names::kConnect ||
      event->type() == event_type_names::kConnecting ||
      event->type() == event_type_names::kDisconnect) {
    media_controls_->RemotePlaybackStateChanged();
    return;
  }

  if (event->type() == event_type_names::kPointermove ||
      event->type() == event_type_names::kPointerout ||
      event->type() == event_type_names::kPointerenter) {
    media_controls_->DefaultEventHandler(*event);
    return;
  }

  NOTREACHED_IN_MIGRATION();
}

void MediaControlsMediaEventListener::OnRemotePlaybackAvailabilityChanged() {
  media_controls_->RefreshCastButtonVisibility();
}

void MediaControlsMediaEventListener::Trace(Visitor* visitor) const {
  NativeEventListener::Trace(visitor);
  visitor->Trace(media_controls_);
}

}  // namespace blink
