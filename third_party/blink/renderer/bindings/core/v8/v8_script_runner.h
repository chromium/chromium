/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_SCRIPT_RUNNER_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_SCRIPT_RUNNER_H_

#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "v8/include/v8.h"

namespace WTF {
class TextPosition;
}

namespace blink {

class ExecutionContext;
class ReferrerScriptInfo;
class ScriptSourceCode;
class ScriptState;
class SingleCachedMetadataHandler;

class CORE_EXPORT V8ScriptRunner final {
  STATIC_ONLY(V8ScriptRunner);

 public:
  // For the following methods, the caller sites have to hold
  // a HandleScope and a ContextScope.
  static v8::MaybeLocal<v8::Script> CompileScript(
      ScriptState*,
      const ScriptSourceCode&,
      SanitizeScriptErrors,
      v8::ScriptCompiler::CompileOptions,
      v8::ScriptCompiler::NoCacheReason,
      const ReferrerScriptInfo&);
  static v8::MaybeLocal<v8::Module> CompileModule(
      v8::Isolate*,
      const String& source,
      SingleCachedMetadataHandler*,
      const String& file_name,
      const WTF::TextPosition&,
      v8::ScriptCompiler::CompileOptions,
      v8::ScriptCompiler::NoCacheReason,
      const ReferrerScriptInfo&);
  static v8::MaybeLocal<v8::Value> RunCompiledScript(v8::Isolate*,
                                                     v8::Local<v8::Script>,
                                                     ExecutionContext*);
  static v8::MaybeLocal<v8::Value> CompileAndRunInternalScript(
      v8::Isolate*,
      ScriptState*,
      const ScriptSourceCode&);
  static v8::MaybeLocal<v8::Value> CallAsConstructor(
      v8::Isolate*,
      v8::Local<v8::Object>,
      ExecutionContext*,
      int argc = 0,
      v8::Local<v8::Value> argv[] = nullptr);
  static v8::MaybeLocal<v8::Value> CallFunction(v8::Local<v8::Function>,
                                                ExecutionContext*,
                                                v8::Local<v8::Value> receiver,
                                                int argc,
                                                v8::Local<v8::Value> info[],
                                                v8::Isolate*);
  static v8::MaybeLocal<v8::Value> EvaluateModule(v8::Isolate*,
                                                  ExecutionContext*,
                                                  v8::Local<v8::Module>,
                                                  v8::Local<v8::Context>);

  // Only to be used from ModuleRecord::ReportException().
  static void ReportExceptionForModule(v8::Isolate*,
                                       v8::Local<v8::Value> exception,
                                       const String& file_name,
                                       const WTF::TextPosition&);

  // Reports an exception to the message handler, as if it were an uncaught
  // exception. Can only be called on the main thread.
  //
  // TODO(adamk): This should live on V8ThrowException, but it depends on
  // V8Initializer and so can't trivially move to platform/bindings.
  static void ReportException(v8::Isolate*, v8::Local<v8::Value> exception);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_SCRIPT_RUNNER_H_
