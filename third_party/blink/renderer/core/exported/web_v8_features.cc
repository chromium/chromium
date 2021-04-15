// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_v8_features.h"

#include "third_party/blink/renderer/core/context_features/context_feature_settings.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "v8/include/v8.h"

namespace blink {

// static
void WebV8Features::EnableMojoJS(v8::Local<v8::Context> context, bool enable) {
  ScriptState* script_state = ScriptState::From(context);
  DCHECK(script_state->World().IsMainWorld());
  ContextFeatureSettings::From(
      ExecutionContext::From(script_state),
      ContextFeatureSettings::CreationMode::kCreateIfNotExists)
      ->enableMojoJS(enable);
}

// static
void WebV8Features::EnableSharedArrayBuffer() {
  static bool shared_array_buffer_enabled = false;
  if (shared_array_buffer_enabled)
    return;

  shared_array_buffer_enabled = true;
  constexpr char kSABFlag[] = "--harmony-sharedarraybuffer";
  v8::V8::SetFlagsFromString(kSABFlag, sizeof(kSABFlag));
}

void WebV8Features::EnableWasmThreads() {
  static bool wasm_threads_enabled = false;
  if (wasm_threads_enabled)
    return;

  wasm_threads_enabled = true;
  constexpr char kWasmThreadsFlag[] = "--experimental-wasm-threads";
  v8::V8::SetFlagsFromString(kWasmThreadsFlag, sizeof(kWasmThreadsFlag));
}

}  // namespace blink
