// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_H_

#include <memory>
#include <tuple>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/web_media_source.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/html/media/media_source_tracer.h"
#include "third_party/blink/renderer/core/html/time_ranges.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/mediasource/media_source_attachment_supplement.h"
#include "third_party/blink/renderer/modules/mediasource/source_buffer.h"
#include "third_party/blink/renderer/modules/mediasource/source_buffer_list.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class EventQueue;
class ExceptionState;
class HTMLMediaElement;
class MediaSourceAttachmentSupplement;
class TrackBase;
class WebSourceBuffer;

// Media Source Extensions (MSE) API's MediaSource object implementation (see
// also https://w3.org/TR/media-source/). Web apps can extend an
// HTMLMediaElement's instance to use the MSE API (also known as "attaching MSE
// to a media element") by using a Media Source object URL as the media
// element's src attribute or the src attribute of a <source> inside the media
// element. A MediaSourceAttachmentSupplement encapsulates the linkage of that
// object URL to a MediaSource instance, and allows communication between the
// media element and the MSE API.
class MediaSource final : public EventTargetWithInlineData,
                          public ActiveScriptWrappable<MediaSource>,
                          public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const AtomicString& OpenKeyword();
  static const AtomicString& ClosedKeyword();
  static const AtomicString& EndedKeyword();

  static MediaSource* Create(ExecutionContext*);

  explicit MediaSource(ExecutionContext*);
  ~MediaSource() override;

  static void LogAndThrowDOMException(ExceptionState&,
                                      DOMExceptionCode error,
                                      const String& message);
  static void LogAndThrowTypeError(ExceptionState&, const String&);

  // Web-exposed methods from media_source.idl
  SourceBufferList* sourceBuffers() { return source_buffers_.Get(); }
  SourceBufferList* activeSourceBuffers() {
    return active_source_buffers_.Get();
  }
  SourceBuffer* addSourceBuffer(const String& type, ExceptionState&);
  void removeSourceBuffer(SourceBuffer*, ExceptionState&);
  void setDuration(double, ExceptionState&);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(sourceopen, kSourceopen)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(sourceended, kSourceended)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(sourceclose, kSourceclose)

  const AtomicString& readyState() const { return ready_state_; }
  void endOfStream(const AtomicString& error, ExceptionState&);
  void endOfStream(ExceptionState&);
  void setLiveSeekableRange(double start, double end, ExceptionState&);
  void clearLiveSeekableRange(ExceptionState&);

  static bool isTypeSupported(ExecutionContext* context, const String& type);

  // Methods needed by a MediaSourceAttachmentSupplement to service operations
  // proxied from an HTMLMediaElement.
  MediaSourceTracer* StartAttachingToMediaElement(
      scoped_refptr<MediaSourceAttachmentSupplement> attachment,
      HTMLMediaElement* element);
  void CompleteAttachingToMediaElement(std::unique_ptr<WebMediaSource>);
  void Close();
  bool IsClosed() const;
  double duration() const;
  WebTimeRanges BufferedInternal() const;
  WebTimeRanges SeekableInternal() const;
  TimeRanges* Buffered() const;
  void OnTrackChanged(TrackBase*);

  // EventTarget interface
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ScriptWrappable
  bool HasPendingActivity() const final;

  // ExecutionContextLifecycleObserver interface
  void ContextDestroyed() override;

  // Used by SourceBuffer.
  void OpenIfInEndedState();
  bool IsOpen() const;
  void SetSourceBufferActive(SourceBuffer*, bool);
  HTMLMediaElement* MediaElement() const;
  std::pair<scoped_refptr<MediaSourceAttachmentSupplement>, MediaSourceTracer*>
  AttachmentAndTracer() const;
  void EndOfStreamAlgorithm(const WebMediaSource::EndOfStreamStatus);

  void Trace(Visitor*) const override;

 private:
  void SetReadyState(const AtomicString&);
  void OnReadyStateChange(const AtomicString&, const AtomicString&);

  bool IsUpdating() const;

  std::unique_ptr<WebSourceBuffer> CreateWebSourceBuffer(const String& type,
                                                         const String& codecs,
                                                         ExceptionState&);
  void ScheduleEvent(const AtomicString& event_name);
  static void RecordIdentifiabilityMetric(ExecutionContext* context,
                                          const String& type,
                                          bool result);

  // Implements the duration change algorithm.
  // http://w3c.github.io/media-source/#duration-change-algorithm
  void DurationChangeAlgorithm(double new_duration, ExceptionState&);

  std::unique_ptr<WebMediaSource> web_media_source_;
  AtomicString ready_state_;
  Member<EventQueue> async_event_queue_;

  // Keep the attached element (via attachment_tracer_), |source_buffers_|,
  // |active_source_buffers_|, and their wrappers from being collected if we are
  // alive or traceable from a GC root. Activity by this MediaSource or on
  // references to objects returned by exercising this MediaSource (such as an
  // app manipulating a SourceBuffer retrieved via activeSourceBuffers()) may
  // cause events to be dispatched by these other objects.
  // |media_source_attachment_| and |attachment_tracer_| must be carefully set
  // and reset: the actual derived type of the attachment (same-thread vs
  // cross-thread, for instance) must be the same semantic as the actual derived
  // type of the tracer. Further, if there is no attachment, then there must be
  // no tracer that's tracking an active attachment.
  // TODO(https://crbug.com/878133): Remove |attached_element_| once it is fully
  // replaced by usage of |media_source_attachment_| and |attachment_tracer_|.
  scoped_refptr<MediaSourceAttachmentSupplement> media_source_attachment_;
  Member<MediaSourceTracer> attachment_tracer_;
  Member<HTMLMediaElement> attached_element_;
  Member<SourceBufferList> source_buffers_;
  Member<SourceBufferList> active_source_buffers_;

  Member<TimeRanges> live_seekable_range_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_H_
