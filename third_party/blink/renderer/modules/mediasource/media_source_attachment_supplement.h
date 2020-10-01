// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_ATTACHMENT_SUPPLEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_ATTACHMENT_SUPPLEMENT_H_

#include "third_party/blink/renderer/core/html/media/media_source_attachment.h"
#include "third_party/blink/renderer/core/html/media/media_source_tracer.h"
#include "third_party/blink/renderer/core/html/track/audio_track.h"
#include "third_party/blink/renderer/core/html/track/video_track.h"
#include "third_party/blink/renderer/modules/mediasource/media_source.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class MediaSource;

// Modules-specific common extension of the core MediaSourceAttachment
// interface. Includes extra interface methods used by concrete attachments to
// communicate with the media element, as well as method implementations and
// members common to all concrete attachments.
class MediaSourceAttachmentSupplement : public MediaSourceAttachment {
 public:
  // Communicates a change in the media resource duration to the attached media
  // element. In a same-thread attachment, communicates this information
  // synchronously. In a cross-thread attachment, communicates asynchronously to
  // the media element. Same-thread synchronous notification here is primarily
  // to preserve compliance of API behavior when not using MSE-in-Worker
  // (setting MediaSource.duration should be synchronously in agreement with
  // subsequent retrieval of MediaElement.duration, all on the main thread).
  virtual void NotifyDurationChanged(MediaSourceTracer* tracer,
                                     double duration) = 0;

  // Retrieves the current (or a recent) media element time. Implementations may
  // choose to either directly, synchronously consult the attached media element
  // (via |tracer| in a same thread implementation) or rely on a "recent"
  // currentTime pumped by the attached element via the MediaSourceAttachment
  // interface (in a cross-thread implementation).
  virtual double GetRecentMediaTime(MediaSourceTracer* tracer) = 0;

  // Retrieves whether or not the media element currently has an error.
  // Implementations may choose to either directly, synchronously consult the
  // attached media element (via |tracer| in a same thread implementation) or
  // rely on the element to correctly pump when it has an error to this
  // attachment (in a cross-thread implementation).
  virtual bool GetElementError(MediaSourceTracer* tracer) = 0;

  // Add/Remove tracks to/from the media Element's audioTracks() or
  // videoTracks() list. Note that this is synchronous in
  // SameThreadMediaSourceAttachment, but the CrossThreadMediaSourceAttachment
  // does a cross-thread task post and performs the operations on the main
  // thread to enable correct context ownership of created tracks and correct
  // context for track list operations.
  virtual void AddAudioTrackToMediaElement(MediaSourceTracer* tracer,
                                           AudioTrack* track) = 0;
  virtual void AddVideoTrackToMediaElement(MediaSourceTracer* tracer,
                                           VideoTrack* track) = 0;
  virtual void RemoveAudioTracksFromMediaElement(MediaSourceTracer* tracer,
                                                 Vector<String> audio_ids,
                                                 bool enqueue_change_event) = 0;
  virtual void RemoveVideoTracksFromMediaElement(MediaSourceTracer* tracer,
                                                 Vector<String> video_ids,
                                                 bool enqueue_change_event) = 0;
  // TODO(https://crbug.com/878133): Update the implementations to remove these
  // short-term cross-thread helpers and instead use the methods, above, once
  // track creation outside of main thread is supported. These helpers create
  // the tracks on the main thread from parameters (not from a currently-
  // uncreatable worker thread track).
  virtual void AddMainThreadAudioTrackToMediaElement(String id,
                                                     String kind,
                                                     String label,
                                                     String language,
                                                     bool enabled);
  virtual void AddMainThreadVideoTrackToMediaElement(String id,
                                                     String kind,
                                                     String label,
                                                     String language,
                                                     bool selected);

  virtual void OnMediaSourceContextDestroyed() = 0;

 protected:
  MediaSourceAttachmentSupplement();
  ~MediaSourceAttachmentSupplement() override;

  DISALLOW_COPY_AND_ASSIGN(MediaSourceAttachmentSupplement);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_ATTACHMENT_SUPPLEMENT_H_
