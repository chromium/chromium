// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_MEDIA_RECORDER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_MEDIA_RECORDER_H_

#include <memory>
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/mediarecorder/media_recorder_handler.h"
#include "third_party/blink/renderer/modules/mediarecorder/media_recorder_options.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class Blob;
class BlobData;
class ExceptionState;

class MODULES_EXPORT MediaRecorder
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<MediaRecorder>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(MediaRecorder);
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum class State { kInactive = 0, kRecording, kPaused };

  static MediaRecorder* Create(ExecutionContext* context,
                               MediaStream* stream,
                               ExceptionState& exception_state);
  static MediaRecorder* Create(ExecutionContext* context,
                               MediaStream* stream,
                               const MediaRecorderOptions* options,
                               ExceptionState& exception_state);

  MediaRecorder(ExecutionContext* context,
                MediaStream* stream,
                const MediaRecorderOptions* options,
                ExceptionState& exception_state);
  ~MediaRecorder() override;

  MediaStream* stream() const { return stream_.Get(); }
  const String& mimeType() const { return mime_type_; }
  String state() const;
  uint32_t videoBitsPerSecond() const { return video_bits_per_second_; }
  uint32_t audioBitsPerSecond() const { return audio_bits_per_second_; }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(start, kStart)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(stop, kStop)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(dataavailable, kDataavailable)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(pause, kPause)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(resume, kResume)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(error, kError)

  void start(ExceptionState& exception_state);
  void start(int time_slice, ExceptionState& exception_state);
  void stop(ExceptionState& exception_state);
  void pause(ExceptionState& exception_state);
  void resume(ExceptionState& exception_state);
  void requestData(ExceptionState& exception_state);

  static bool isTypeSupported(ExecutionContext* context, const String& type);

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext* context) override;

  // ScriptWrappable
  bool HasPendingActivity() const final { return !stopped_; }

  virtual void WriteData(const char* data,
                         size_t length,
                         bool last_in_slice,
                         double timecode);
  virtual void OnError(const String& message);

  void Trace(blink::Visitor* visitor) override;

 private:
  void CreateBlobEvent(Blob* blob, double timecode);

  void StopRecording();
  void ScheduleDispatchEvent(Event* event);
  void DispatchScheduledEvent();

  Member<MediaStream> stream_;
  String mime_type_;
  bool stopped_;
  int audio_bits_per_second_;
  int video_bits_per_second_;

  State state_;

  std::unique_ptr<BlobData> blob_data_;

  Member<MediaRecorderHandler> recorder_handler_;

  HeapVector<Member<Event>> scheduled_events_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_MEDIA_RECORDER_H_
