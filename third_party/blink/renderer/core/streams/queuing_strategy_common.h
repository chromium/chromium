// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_QUEUING_STRATEGY_COMMON_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_QUEUING_STRATEGY_COMMON_H_

#include "v8/include/v8.h"

namespace blink {

class ScriptState;
class ScriptValue;

enum class QueuingStrategySizeFunctionType {
  kByteLength,
  kCount,
};

using SizeFunctionFactory = v8::Local<v8::Function> (*)(ScriptState*);

// Returns the size function cached on `script_state`'s per-context data for
// `type`, or creates and caches it via `factory`. If `script_state`'s context
// is detached (`PerContextData()` is null), returns a newly created function
// without caching it; this is a minor intentional deviation from the standard
// to avoid retaining V8 objects in detached contexts.
ScriptValue GetCachedSizeFunction(ScriptState* script_state,
                                  QueuingStrategySizeFunctionType type,
                                  SizeFunctionFactory factory);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_QUEUING_STRATEGY_COMMON_H_
