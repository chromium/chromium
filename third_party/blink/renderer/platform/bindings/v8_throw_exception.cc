/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"

#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "v8/include/v8-exception.h"

namespace blink {

#define DEFINE_CREATE_AND_THROW_ERROR_FUNC(blinkErrorType, v8ErrorType,  \
                                           defaultMessage)               \
  v8::Local<v8::Value> V8ThrowException::Create##blinkErrorType(         \
      v8::Isolate* isolate, const String& message) {                     \
    return v8::Exception::v8ErrorType(                                   \
        V8String(isolate, message.IsNull() ? defaultMessage : message)); \
  }                                                                      \
                                                                         \
  void V8ThrowException::Throw##blinkErrorType(v8::Isolate* isolate,     \
                                               const String& message) {  \
    ThrowException(isolate, Create##blinkErrorType(isolate, message));   \
  }

DEFINE_CREATE_AND_THROW_ERROR_FUNC(Error, Error, "Error")
DEFINE_CREATE_AND_THROW_ERROR_FUNC(RangeError, RangeError, "Range error")
DEFINE_CREATE_AND_THROW_ERROR_FUNC(ReferenceError,
                                   ReferenceError,
                                   "Reference error")
DEFINE_CREATE_AND_THROW_ERROR_FUNC(SyntaxError, SyntaxError, "Syntax error")
DEFINE_CREATE_AND_THROW_ERROR_FUNC(TypeError, TypeError, "Type error")
DEFINE_CREATE_AND_THROW_ERROR_FUNC(WasmCompileError,
                                   WasmCompileError,
                                   "Compile error")
DEFINE_CREATE_AND_THROW_ERROR_FUNC(WasmLinkError, WasmLinkError, "Link error")
DEFINE_CREATE_AND_THROW_ERROR_FUNC(WasmRuntimeError,
                                   WasmRuntimeError,
                                   "Runtime error")

#undef DEFINE_CREATE_AND_THROW_ERROR_FUNC

}  // namespace blink
