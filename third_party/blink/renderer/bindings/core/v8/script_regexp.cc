/*
 * Copyright (C) 2004, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2011 Peter Varga (pvarga@webkit.org), University of Szeged
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/bindings/core/v8/script_regexp.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"

namespace blink {

namespace {
const uint32_t kBacktrackLimit = 1'000'000;
}  // namespace

ScriptRegexp::ScriptRegexp(const String& pattern,
                           TextCaseSensitivity case_sensitivity,
                           MultilineMode multiline_mode,
                           CharacterMode char_mode) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      V8PerIsolateData::From(isolate)->EnsureScriptRegexpContext();
  v8::Context::Scope context_scope(context);
  v8::TryCatch try_catch(isolate);

  unsigned flags = v8::RegExp::kNone;
  if (case_sensitivity != kTextCaseSensitive)
    flags |= v8::RegExp::kIgnoreCase;
  if (multiline_mode == kMultilineEnabled)
    flags |= v8::RegExp::kMultiline;
  if (char_mode == UTF16)
    flags |= v8::RegExp::kUnicode;

  v8::Local<v8::RegExp> regex;
  if (v8::RegExp::NewWithBacktrackLimit(context, V8String(isolate, pattern),
                                        static_cast<v8::RegExp::Flags>(flags),
                                        kBacktrackLimit)
          .ToLocal(&regex))
    regex_.Set(isolate, regex);
  if (try_catch.HasCaught() && !try_catch.Message().IsEmpty())
    exception_message_ =
        ToCoreStringWithUndefinedOrNullCheck(try_catch.Message()->Get());
}

int ScriptRegexp::Match(const String& string,
                        int start_from,
                        int* match_length) const {
  if (match_length)
    *match_length = 0;

  if (regex_.IsEmpty() || string.IsNull())
    return -1;

  // v8 strings are limited to int.
  if (string.length() > INT_MAX)
    return -1;

  ScriptForbiddenScope::AllowUserAgentScript allow_script;

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      V8PerIsolateData::From(isolate)->EnsureScriptRegexpContext();
  v8::Context::Scope context_scope(context);
  v8::TryCatch try_catch(isolate);

  v8::Local<v8::RegExp> regex = regex_.NewLocal(isolate);
  v8::Local<v8::String> subject =
      V8String(isolate, string.Substring(start_from));
  v8::Local<v8::Value> return_value;
  if (!regex->Exec(context, subject).ToLocal(&return_value))
    return -1;

  // RegExp#exec returns null if there's no match, otherwise it returns an
  // Array of strings with the first being the whole match string and others
  // being subgroups. The Array also has some random properties tacked on like
  // "index" which is the offset of the match.
  //
  // https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/RegExp/exec

  DCHECK(!return_value.IsEmpty());
  if (!return_value->IsArray())
    return -1;

  v8::Local<v8::Array> result = return_value.As<v8::Array>();
  v8::Local<v8::Value> match_offset;
  if (!result->Get(context, V8AtomicString(isolate, "index"))
           .ToLocal(&match_offset))
    return -1;
  if (match_length) {
    v8::Local<v8::Value> match;
    if (!result->Get(context, 0).ToLocal(&match))
      return -1;
    *match_length = match.As<v8::String>()->Length();
  }

  return match_offset.As<v8::Int32>()->Value() + start_from;
}

}  // namespace blink
