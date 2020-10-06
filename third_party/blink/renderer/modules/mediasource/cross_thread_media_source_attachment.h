// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_CROSS_THREAD_MEDIA_SOURCE_ATTACHMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_CROSS_THREAD_MEDIA_SOURCE_ATTACHMENT_H_

#include <memory>

#include "base/util/type_safety/pass_key.h"
#include "third_party/blink/public/platform/web_time_range.h"
#include "third_party/blink/renderer/core/html/track/audio_track.h"
#include "third_party/blink/renderer/core/html/track/audio_track_list.h"
#include "third_party/blink/renderer/core/html/track/video_track.h"
#include "third_party/blink/renderer/core/html/track/video_track_list.h"
#include "third_party/blink/renderer/modules/mediasource/media_source.h"
#include "third_party/blink/renderer/modules/mediasource/media_source_attachment_supplement.h"
#include "third_party/blink/renderer/modules/mediasource/url_media_source.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

// Concrete attachment that supports operation between a media element on the
// main thread and the MSE API on a dedicated worker thread.
// TODO(https://crbug.com/878133): Implement this more fully. Currently it is
// implementing only the constructor, necessary for cross-thread registry
// implementation and basic verification.
class CrossThreadMediaSourceAttachment final
    : public MediaSourceAttachmentSupplement {
 public:
  // The only intended caller of this constructor is
  // URLMediaSource::createObjectUrl (as shown by using the PassKey), executing
  // in the worker thread context.  The raw pointer is then adopted into a
  // scoped_refptr in MediaSourceRegistryImpl::RegisterURL.
  CrossThreadMediaSourceAttachment(MediaSource* media_source,
                                   util::PassKey<URLMediaSource>);

  // MediaSourceAttachmentSupplement, called by MSE API on worker thread.
  void NotifyDurationChanged(MediaSourceTracer* tracer, double duration) final;
  double GetRecentMediaTime(MediaSourceTracer* tracer) final;
  bool GetElementError(MediaSourceTracer* tracer) final;
  AudioTrackList* CreateAudioTrackList(MediaSourceTracer* tracer) final;
  VideoTrackList* CreateVideoTrackList(MediaSourceTracer* tracer) final;
  void AddAudioTrackToMediaElement(MediaSourceTracer* tracer,
                                   AudioTrack* track) final;
  void AddVideoTrackToMediaElement(MediaSourceTracer* tracer,
                                   VideoTrack* track) final;
  void RemoveAudioTracksFromMediaElement(MediaSourceTracer* tracer,
                                         Vector<String> audio_ids,
                                         bool enqueue_change_event) final;
  void RemoveVideoTracksFromMediaElement(MediaSourceTracer* tracer,
                                         Vector<String> video_ids,
                                         bool enqueue_change_event) final;
  void AddMainThreadAudioTrackToMediaElement(String id,
                                             String kind,
                                             String label,
                                             String language,
                                             bool enabled) final;
  void AddMainThreadVideoTrackToMediaElement(String id,
                                             String kind,
                                             String label,
                                             String language,
                                             bool selected) final;
  void OnMediaSourceContextDestroyed() final;

  // MediaSourceAttachment methods called on main thread by media element,
  // except Unregister is called on either main or dedicated worker thread by
  // MediaSourceRegistryImpl.
  void Unregister() final;
  MediaSourceTracer* StartAttachingToMediaElement(HTMLMediaElement*,
                                                  bool* success) final;
  void CompleteAttachingToMediaElement(MediaSourceTracer* tracer,
                                       std::unique_ptr<WebMediaSource>) final;

  void Close(MediaSourceTracer* tracer) final;
  WebTimeRanges BufferedInternal(MediaSourceTracer* tracer) const final;
  WebTimeRanges SeekableInternal(MediaSourceTracer* tracer) const final;
  void OnTrackChanged(MediaSourceTracer* tracer, TrackBase*) final;

  void OnElementTimeUpdate(double time) final;
  void OnElementError() final;
  void OnElementContextDestroyed() final;

 private:
  ~CrossThreadMediaSourceAttachment() override;

  // Cache of the registered worker-thread MediaSource. Retains strong reference
  // on all Oilpan heaps, from construction of this object until Unregister() is
  // called. This lets the main thread successfully attach (modulo normal
  // reasons why StartAttaching..() can fail) to the worker-thread MediaSource
  // even if there were no other strong references other than this one on the
  // worker-thread Oilpan heap to the MediaSource.
  CrossThreadPersistent<MediaSource> registered_media_source_;

  DISALLOW_COPY_AND_ASSIGN(CrossThreadMediaSourceAttachment);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_CROSS_THREAD_MEDIA_SOURCE_ATTACHMENT_H_
