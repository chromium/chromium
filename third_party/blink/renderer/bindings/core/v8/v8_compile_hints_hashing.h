// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_COMPILE_HINTS_HASHING_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_COMPILE_HINTS_HASHING_H_

#include "v8/include/v8.h"

namespace blink::v8_compile_hints {

// Helper for computing hashes we query the Bloom filter with. Each hash is
// computed from script URL + function position. We speed up computing the
// hashes by hashing the script name only once, and using the hash as "script
// identifier", then hash "script identifier + function position" pairs. This
// way retrieving data from the Bloom filter is also fast; we first compute the
// script name hash, and retrieve data for its functions as we encounter them.
uint32_t ScriptNameHash(v8::Local<v8::Value> name_value,
                        v8::Local<v8::Context> context,
                        v8::Isolate* isolate);

uint32_t CombineHash(uint32_t script_name_hash, int position);

}  // namespace blink::v8_compile_hints

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_COMPILE_HINTS_HASHING_H_
