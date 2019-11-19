// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_QUEUING_STRATEGY_COMMON_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_QUEUING_STRATEGY_COMMON_H_

#include "v8/include/v8.h"

namespace blink {

class ScriptState;
class QueuingStrategyInit;

v8::Local<v8::Value> HighWaterMarkOrUndefined(ScriptState* script_state,
                                              const QueuingStrategyInit* init);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_QUEUING_STRATEGY_COMMON_H_
