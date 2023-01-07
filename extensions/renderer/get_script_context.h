// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GET_SCRIPT_CONTEXT_H_
#define EXTENSIONS_RENDERER_GET_SCRIPT_CONTEXT_H_

#include "v8/include/v8-forward.h"

namespace extensions {
class ScriptContext;

// TODO(devlin): This is a very random place for these. But there's not really
// a better one - ScriptContextSet is (currently) thread-agnostic, and it'd be
// nice to avoid changing that.

// Returns the ScriptContext associated with the given v8::Context. This is
// designed to work for both main thread and worker thread contexts.
ScriptContext* GetScriptContextFromV8Context(v8::Local<v8::Context> context);

// Same as above, but CHECK()s the result before returning.
ScriptContext* GetScriptContextFromV8ContextChecked(
    v8::Local<v8::Context> context);

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_GET_SCRIPT_CONTEXT_H_
