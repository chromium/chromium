// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/queuing_strategy_common.h"

#include "third_party/blink/renderer/core/streams/queuing_strategy_init.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

v8::Local<v8::Value> HighWaterMarkOrUndefined(ScriptState* script_state,
                                              const QueuingStrategyInit* init) {
  v8::Local<v8::Value> high_water_mark;
  if (init->hasHighWaterMark()) {
    high_water_mark = init->highWaterMark().V8Value();
  } else {
    high_water_mark = v8::Undefined(script_state->GetIsolate());
  }
  return high_water_mark;
}

}  // namespace blink
