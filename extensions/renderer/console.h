// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_CONSOLE_H_
#define EXTENSIONS_RENDERER_CONSOLE_H_

#include <string>

#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "v8/include/v8-forward.h"

namespace extensions {
class ScriptContext;

// Utility for logging console messages.
namespace console {

// Adds |message| to the console of of the |script_context|. If |script_context|
// is null, LOG()s the message instead.
void AddMessage(ScriptContext* script_context,
                blink::mojom::ConsoleMessageLevel level,
                const std::string& message);

// Logs an Error then crashes the current process.
void Fatal(ScriptContext* context, const std::string& message);

// Returns a new v8::Object with each standard log method (Debug/Log/Warn/Error)
// bound to respective debug/log/warn/error methods. This is a direct drop-in
// replacement for the standard devtools console.* methods usually accessible
// from JS.
v8::Local<v8::Object> AsV8Object(v8::Isolate* isolate);

}  // namespace console

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_CONSOLE_H_
