/*
 * Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/bindings/v8_binding.h"

#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

v8::Local<v8::Function> GetBoundFunction(v8::Local<v8::Function> function) {
  v8::Local<v8::Value> bound_function = function->GetBoundFunction();
  return bound_function->IsFunction()
             ? v8::Local<v8::Function>::Cast(bound_function)
             : function;
}

v8::Local<v8::Value> FreezeV8Object(v8::Local<v8::Value> value,
                                    v8::Isolate* isolate) {
  value.As<v8::Object>()
      ->SetIntegrityLevel(isolate->GetCurrentContext(),
                          v8::IntegrityLevel::kFrozen)
      .ToChecked();
  return value;
}

String GetCurrentScriptUrl(v8::Isolate* isolate) {
  DCHECK(isolate);
  if (!isolate->InContext())
    return String();

  v8::Local<v8::String> script_name =
      v8::StackTrace::CurrentScriptNameOrSourceURL(isolate);
  return ToCoreStringWithNullCheck(isolate, script_name);
}

Vector<String> GetScriptUrlsFromCurrentStack(v8::Isolate* isolate,
                                             wtf_size_t unique_url_count) {
  Vector<String> unique_urls;

  if (!isolate || !isolate->InContext()) {
    return unique_urls;
  }

  // CurrentStackTrace is 10x faster than CaptureStackTrace if all that you
  // need is the url of the script at the top of the stack. See
  // crbug.com/1057211 for more detail.
  // Get at most 10 frames, regardless of the requested url count, to minimize
  // the performance impact.
  v8::Local<v8::StackTrace> stack_trace =
      v8::StackTrace::CurrentStackTrace(isolate, /*frame_limit=*/10);

  int frame_count = stack_trace->GetFrameCount();
  for (int i = 0; i < frame_count; ++i) {
    v8::Local<v8::StackFrame> frame = stack_trace->GetFrame(isolate, i);
    v8::Local<v8::String> script_name = frame->GetScriptName();
    if (script_name.IsEmpty() || !script_name->Length())
      continue;
    String url = ToCoreString(isolate, script_name);
    if (!unique_urls.Contains(url)) {
      unique_urls.push_back(std::move(url));
    }
    if (unique_urls.size() == unique_url_count)
      break;
  }
  return unique_urls;
}

namespace bindings {

void V8ObjectToPropertyDescriptor(v8::Isolate* isolate,
                                  v8::Local<v8::Value> descriptor_object,
                                  V8PropertyDescriptorBag& descriptor_bag,
                                  ExceptionState& exception_state) {
  // TODO(crbug.com/1261485): This function is the same as
  // v8::internal::PropertyDescriptor::ToPropertyDescriptor.  Make the
  // function exposed public and re-use it rather than re-implementing
  // the same logic in Blink.

  auto& desc = descriptor_bag;
  desc = V8PropertyDescriptorBag();

  if (!descriptor_object->IsObject()) {
    exception_state.ThrowTypeError("Property description must be an object.");
    return;
  }

  v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
  v8::Local<v8::Object> v8_desc = descriptor_object.As<v8::Object>();
  TryRethrowScope rethrow_scope(isolate, exception_state);

  auto get_value = [&](const char* property, bool& has,
                       v8::Local<v8::Value>& value) -> bool {
    const auto& v8_property = V8AtomicString(isolate, property);
    if (!v8_desc->Has(current_context, v8_property).To(&has)) {
      return false;
    }
    if (has) {
      if (!v8_desc->Get(current_context, v8_property).ToLocal(&value)) {
        return false;
      }
    } else {
      value = v8::Undefined(isolate);
    }
    return true;
  };

  auto get_bool = [&](const char* property, bool& has, bool& value) -> bool {
    v8::Local<v8::Value> v8_value;
    if (!get_value(property, has, v8_value))
      return false;
    if (has) {
      value = v8_value->ToBoolean(isolate)->Value();
    }
    return true;
  };

  if (!get_bool("enumerable", desc.has_enumerable, desc.enumerable))
    return;

  if (!get_bool("configurable", desc.has_configurable, desc.configurable))
    return;

  if (!get_value("value", desc.has_value, desc.value))
    return;

  if (!get_bool("writable", desc.has_writable, desc.writable))
    return;

  if (!get_value("get", desc.has_get, desc.get))
    return;

  if (!get_value("set", desc.has_set, desc.set))
    return;

  if ((desc.has_get || desc.has_set) && (desc.has_value || desc.has_writable)) {
    exception_state.ThrowTypeError(
        "Invalid property descriptor. Cannot both specify accessors and "
        "a value or writable attribute");
    return;
  }
}

}  // namespace bindings

}  // namespace blink
