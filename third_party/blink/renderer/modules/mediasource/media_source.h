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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_H_

#include <memory>

#include "third_party/blink/public/platform/web_media_source.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/fileapi/url_registry.h"
#include "third_party/blink/renderer/core/html/media/html_media_source.h"
#include "third_party/blink/renderer/core/html/time_ranges.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/mediasource/source_buffer.h"
#include "third_party/blink/renderer/modules/mediasource/source_buffer_list.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"

namespace blink {

class EventQueue;
class ExceptionState;
class WebSourceBuffer;

class MediaSource final : public EventTargetWithInlineData,
                          public HTMLMediaSource,
                          public ActiveScriptWrappable<MediaSource>,
                          public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(MediaSource);

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

  // MediaSource.idl methods
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

  static bool isTypeSupported(const String& type);

  // HTMLMediaSource
  bool AttachToElement(HTMLMediaElement*) override;
  void SetWebMediaSourceAndOpen(std::unique_ptr<WebMediaSource>) override;
  void Close() override;
  bool IsClosed() const override;
  double duration() const override;
  WebTimeRanges BufferedInternal() const override;
  WebTimeRanges SeekableInternal() const override;
  TimeRanges* Buffered() const override;
  void OnTrackChanged(TrackBase*) override;

  // EventTarget interface
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ScriptWrappable
  bool HasPendingActivity() const final;

  // ContextLifecycleObserver interface
  void ContextDestroyed(ExecutionContext*) override;

  // URLRegistrable interface
  URLRegistry& Registry() const override;

  // Used by SourceBuffer.
  void OpenIfInEndedState();
  bool IsOpen() const;
  void SetSourceBufferActive(SourceBuffer*, bool);
  HTMLMediaElement* MediaElement() const;
  void EndOfStreamAlgorithm(const WebMediaSource::EndOfStreamStatus);

  // Used by MediaSourceRegistry.
  void AddedToRegistry();
  void RemovedFromRegistry();

  void Trace(blink::Visitor*) override;

 private:
  void SetReadyState(const AtomicString&);
  void OnReadyStateChange(const AtomicString&, const AtomicString&);

  bool IsUpdating() const;

  std::unique_ptr<WebSourceBuffer> CreateWebSourceBuffer(const String& type,
                                                         const String& codecs,
                                                         ExceptionState&);
  void ScheduleEvent(const AtomicString& event_name);

  // Implements the duration change algorithm.
  // http://w3c.github.io/media-source/#duration-change-algorithm
  void DurationChangeAlgorithm(double new_duration, ExceptionState&);

  std::unique_ptr<WebMediaSource> web_media_source_;
  AtomicString ready_state_;
  Member<EventQueue> async_event_queue_;

  // Here, using Member, instead of Member, to keep
  // |attached_element_|, |source_buffers_|, |active_source_buffers_|, and their
  // wrappers from being collected if we are alive or traceable from a GC root.
  // Activity by this MediaSource or on references to objects returned by
  // exercising this MediaSource (such as an app manipulating a SourceBuffer
  // retrieved via activeSourceBuffers()) may cause events to be dispatched by
  // these other objects.
  Member<HTMLMediaElement> attached_element_;
  Member<SourceBufferList> source_buffers_;
  Member<SourceBufferList> active_source_buffers_;

  Member<TimeRanges> live_seekable_range_;

  int added_to_registry_counter_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_H_
