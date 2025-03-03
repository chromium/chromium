// Copyright 2021 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_RECORD_REPLAY_INTERFACE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_RECORD_REPLAY_INTERFACE_H_

#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "base/values.h"
#include "v8/include/v8.h"

namespace blink {
// Initialize command state after the first context is created, but before the
// first checkpoint in the recording is created.
void InitializeRecordReplay(
  const char* processType,
  v8::Isolate* isolate, LocalFrame* localFrame, v8::Local<v8::Context> context);

// Do any remaining initialization after the first checkpoint is created.
void InitializeRecordReplayAfterCheckpoint();

// Initialize everything that needs to be initialized with every root frame,
// before the first Checkpoint.
void OnRootFrameInit(v8::Isolate* isolate, LocalFrame* localFrame, v8::Local<v8::Context> context);

// Initialize everything that needs to be initialized with every root frame,
// after the first Checkpoint.
void OnRootFrameInitAfterCheckpoint(v8::Isolate* isolate, LocalFrame* localFrame, v8::Local<v8::Context> context);

// Initialize everything that depends on other initialization steps but
// for all windows.
// This is the last Replay code that we run for a new Window object.
void OnNewWindowAfterCheckpoint(v8::Isolate* isolate, LocalFrame* localFrame, v8::Local<v8::Context> context);

// Notify the driver that we're adding an error to the console.
void RecordReplayOnErrorEvent(ErrorEvent* error_event);

// Notify our blink bindings that the page that was running our script(s) is
//   about to reset, and its ExecutionContext and our V8 debugger session with it.
// From this point forward, command handling is not possible anymore
//   until a new page is spawned.
// Note: JS can still execute, even after this happened.
void RecordReplayClearContexts(const char* reason, LocalFrame* frame);

// Notify record/replay about new inspectors that have been created.
void RecordReplayRegisterV8Inspector(v8_inspector::V8Inspector* inspector,
                                     v8::Isolate* isolate);

class RecordReplayEventListener : public NativeEventListener {
 public:
  RecordReplayEventListener(v8::Isolate* isolate, LocalFrame* localFrame)
      : local_frame_(localFrame) {}

  void Invoke(ExecutionContext*, Event*) override;

  void Trace(Visitor*) const override;

  static RecordReplayEventListener* Create(v8::Isolate* isolate,
                                      LocalFrame* localFrame) {
    return MakeGarbageCollected<RecordReplayEventListener>(isolate, localFrame);
  }

 private:
  Member<LocalFrame> local_frame_;

  void HandleRecordReplayTokenMessage(v8::Local<v8::Context> context, v8::Local<v8::Object> message);
  void HandleRecordReplayMessage(v8::Local<v8::Context> context, v8::Local<v8::Object> message);
};

int RecordReplayOnDOMMutation(Node& target, const char* type);

} // namespace blink

#endif // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_RECORD_REPLAY_INTERFACE_H_
