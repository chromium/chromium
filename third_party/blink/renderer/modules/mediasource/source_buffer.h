/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_SOURCE_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_SOURCE_BUFFER_H_

#include <memory>
#include "third_party/blink/public/platform/web_source_buffer_client.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/mediasource/track_default_list.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class AudioTrackList;
class DOMArrayBuffer;
class DOMArrayBufferView;
class EventQueue;
class ExceptionState;
class MediaSource;
class TimeRanges;
class VideoTrackList;
class WebSourceBuffer;

class SourceBuffer final : public EventTargetWithInlineData,
                           public ActiveScriptWrappable<SourceBuffer>,
                           public ExecutionContextLifecycleObserver,
                           public WebSourceBufferClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(SourceBuffer, Dispose);

 public:
  static const AtomicString& SegmentsKeyword();
  static const AtomicString& SequenceKeyword();

  SourceBuffer(std::unique_ptr<WebSourceBuffer>, MediaSource*, EventQueue*);
  ~SourceBuffer() override;

  // SourceBuffer.idl methods
  const AtomicString& mode() const { return mode_; }
  void setMode(const AtomicString&, ExceptionState&);
  bool updating() const { return updating_; }
  TimeRanges* buffered(ExceptionState&) const;
  WebTimeRanges buffered() const;
  double timestampOffset() const;
  void setTimestampOffset(double, ExceptionState&);
  void appendBuffer(DOMArrayBuffer* data, ExceptionState&);
  void appendBuffer(NotShared<DOMArrayBufferView> data, ExceptionState&);
  void abort(ExceptionState&);
  void remove(double start, double end, ExceptionState&);
  void changeType(const String& type, ExceptionState&);
  double appendWindowStart() const;
  void setAppendWindowStart(double, ExceptionState&);
  double appendWindowEnd() const;
  void setAppendWindowEnd(double, ExceptionState&);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(updatestart, kUpdatestart)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(update, kUpdate)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(updateend, kUpdateend)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(error, kError)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(abort, kAbort)
  TrackDefaultList* trackDefaults() const { return track_defaults_.Get(); }
  void setTrackDefaults(TrackDefaultList*, ExceptionState&);

  AudioTrackList& audioTracks();
  VideoTrackList& videoTracks();

  void RemovedFromMediaSource();
  double HighestPresentationTimestamp();

  // ScriptWrappable
  bool HasPendingActivity() const final;

  // ExecutionContextLifecycleObserver
  void ContextDestroyed() override;

  // EventTarget interface
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  // WebSourceBufferClient interface
  bool InitializationSegmentReceived(const WebVector<MediaTrackInfo>&) override;
  void NotifyParseWarning(const ParseWarning) override;

  void Trace(Visitor*) const override;

 private:
  void Dispose();

  bool IsRemoved() const;
  void ScheduleEvent(const AtomicString& event_name);

  bool PrepareAppend(double media_time, size_t new_data_size, ExceptionState&);
  bool EvictCodedFrames(double media_time, size_t new_data_size);
  void AppendBufferInternal(double media_time,
                            const unsigned char*,
                            size_t,
                            ExceptionState&);
  void AppendBufferAsyncPart();
  void AppendError();

  void RemoveAsyncPart();

  void CancelRemove();
  void AbortIfUpdating();

  void RemoveMediaTracks();

  // Returns MediaElement playback position (i.e. MediaElement.currentTime() )
  // in seconds, or NaN if media element is not available.
  double GetMediaTime();

  const TrackDefault* GetTrackDefault(
      const AtomicString& track_type,
      const AtomicString& byte_stream_track_id) const;
  AtomicString DefaultTrackLabel(
      const AtomicString& track_type,
      const AtomicString& byte_stream_track_id) const;
  AtomicString DefaultTrackLanguage(
      const AtomicString& track_type,
      const AtomicString& byte_stream_track_id) const;

  std::unique_ptr<WebSourceBuffer> web_source_buffer_;

  // If any portion of an attached HTMLMediaElement (HTMLME) and the MediaSource
  // Extensions (MSE) API is alive (having pending activity or traceable from a
  // GC root), the whole group is not GC'ed. Here, using Member,
  // instead of Member, because |source_|'s and |track_defaults_|'s wrappers
  // need to remain alive at least to successfully dispatch any events enqueued
  // by the behavior of the HTMLME+MSE API. It makes those wrappers remain alive
  // as long as this SourceBuffer's wrapper is alive.
  Member<MediaSource> source_;
  Member<TrackDefaultList> track_defaults_;
  Member<EventQueue> async_event_queue_;

  AtomicString mode_;
  bool updating_;
  double timestamp_offset_;
  Member<AudioTrackList> audio_tracks_;
  Member<VideoTrackList> video_tracks_;
  double append_window_start_;
  double append_window_end_;
  bool first_initialization_segment_received_;

  Vector<unsigned char> pending_append_data_;
  wtf_size_t pending_append_data_offset_;
  TaskHandle append_buffer_async_task_handle_;

  double pending_remove_start_;
  double pending_remove_end_;
  TaskHandle remove_async_task_handle_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_SOURCE_BUFFER_H_
