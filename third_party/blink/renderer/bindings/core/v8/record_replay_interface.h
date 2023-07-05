// Copyright 2021 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_RECORD_REPLAY_INTERFACE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_RECORD_REPLAY_INTERFACE_H_

#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "base/values.h"
#include "v8/include/v8.h"

namespace blink {

// Initialize everything that needs to be initialized with every new global window.
void OnNewWindow(v8::Isolate* isolate, LocalFrame* localFrame);

// Initialize command state after the first context is created, but before the
// first checkpoint in the recording is created.
void SetupRecordReplayCommands(v8::Isolate* isolate, LocalFrame* localFrame);

// Initialize everything that needs to be initialized with every root frame.
void OnNewRootFrame(v8::Isolate* isolate, LocalFrame* localFrame);

// Notify the driver that we're adding an error to the console.
void RecordReplayOnErrorEvent(ErrorEvent* error_event);

// Notify record/replay about new inspectors that have been created.
void RecordReplayRegisterV8Inspector(v8_inspector::V8Inspector* inspector,
                                     v8::Isolate* isolate);

} // namespace blink

#endif // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_RECORD_REPLAY_INTERFACE_H_
