/*
 * Copyright (C) 2008, 2009, 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/bindings/core/v8/v8_xml_http_request.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_blob.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_form_data.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_document.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/xmlhttprequest/xml_http_request.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_set_return_value.h"
#include "v8/include/v8.h"

namespace blink {

void V8XMLHttpRequest::ResponseTextAttributeGetterCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  XMLHttpRequest* xml_http_request = V8XMLHttpRequest::ToImpl(info.Holder());
  ExceptionState exception_state(info.GetIsolate(),
                                 ExceptionState::kGetterContext,
                                 "XMLHttpRequest", "responseText");
  v8::Local<v8::String> text = xml_http_request->responseText(exception_state);
  if (text.IsEmpty()) {
    V8SetReturnValueString(info, g_empty_string, info.GetIsolate());
    return;
  }
  V8SetReturnValue(info, text);
}

void V8XMLHttpRequest::ResponseAttributeGetterCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  XMLHttpRequest* xml_http_request = V8XMLHttpRequest::ToImpl(info.Holder());
  ExceptionState exception_state(info.GetIsolate(),
                                 ExceptionState::kGetterContext,
                                 "XMLHttpRequest", "response");

  switch (xml_http_request->GetResponseTypeCode()) {
    case XMLHttpRequest::kResponseTypeDefault:
    case XMLHttpRequest::kResponseTypeText:
      ResponseTextAttributeGetterCustom(info);
      return;

    case XMLHttpRequest::kResponseTypeJSON: {
      v8::Isolate* isolate = info.GetIsolate();

      v8::Local<v8::String> json_source =
          xml_http_request->ResponseJSONSource();
      if (json_source.IsEmpty()) {
        V8SetReturnValueNull(info);
        return;
      }

      // Catch syntax error. Swallows an exception (when thrown) as the
      // spec says. https://xhr.spec.whatwg.org/#response-body
      v8::Local<v8::Value> json =
          FromJSONString(isolate, isolate->GetCurrentContext(),
                         ToCoreString(json_source), exception_state);
      if (exception_state.HadException()) {
        exception_state.ClearException();
        V8SetReturnValueNull(info);
      } else {
        V8SetReturnValue(info, json);
      }
      return;
    }

    case XMLHttpRequest::kResponseTypeDocument: {
      Document* document = xml_http_request->responseXML(exception_state);
      bindings::V8SetReturnValue(info, document, xml_http_request);
      return;
    }

    case XMLHttpRequest::kResponseTypeBlob: {
      Blob* blob = xml_http_request->ResponseBlob();
      bindings::V8SetReturnValue(info, blob, xml_http_request);
      return;
    }

    case XMLHttpRequest::kResponseTypeArrayBuffer: {
      DOMArrayBuffer* array_buffer = xml_http_request->ResponseArrayBuffer();
      bindings::V8SetReturnValue(info, array_buffer, xml_http_request);
      return;
    }
  }
}

}  // namespace blink
