// Copyright 2021 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_RECORD_REPLAY_DEVTOOLS_EVENT_LISTENER_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_RECORD_REPLAY_DEVTOOLS_EVENT_LISTENER_H_

#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "v8/include/v8.h"

namespace blink {

class RecordReplayDevtoolsEventListener : public NativeEventListener {
 public:
  RecordReplayDevtoolsEventListener(v8::Isolate* isolate, LocalFrame* localFrame)
      : local_frame_(localFrame) {}

  void Invoke(ExecutionContext*, Event*) override;

  void Trace(Visitor*) const override;

  static RecordReplayDevtoolsEventListener* Create(v8::Isolate* isolate,
                                      LocalFrame* localFrame) {
    return MakeGarbageCollected<RecordReplayDevtoolsEventListener>(isolate, localFrame);
  }

 private:
  Member<LocalFrame> local_frame_;

  void HandleRecordReplayTokenMessage(v8::Local<v8::Context> context, v8::Local<v8::Object> message);
  void HandleRecordReplayMessage(v8::Local<v8::Context> context, v8::Local<v8::Object> message);
};

}  // namespace blink

#endif // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_RECORD_REPLAY_DEVTOOLS_EVENT_LISTENER_H_
