// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/web_ui_extension.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "content/common/frame_messages.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"
#include "content/renderer/web_ui_extension_data.h"
#include "gin/arguments.h"
#include "gin/function_template.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

namespace content {

namespace {

bool ShouldRespondToRequest(blink::WebLocalFrame** frame_ptr,
                            RenderFrame** render_frame_ptr) {
  blink::WebLocalFrame* frame = blink::WebLocalFrame::FrameForCurrentContext();
  if (!frame || !frame->View())
    return false;

  GURL frame_url = frame->GetDocument().Url();

  RenderFrame* render_frame = RenderFrame::FromWebFrame(frame);
  if (!render_frame)
    return false;

  bool webui_enabled =
      (render_frame->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI) &&
      (frame_url.SchemeIs(kChromeUIScheme) ||
       frame_url.SchemeIs(url::kDataScheme));

  if (!webui_enabled)
    return false;

  *frame_ptr = frame;
  *render_frame_ptr = render_frame;
  return true;
}

}  // namespace

// Exposes two methods:
//  - chrome.send: Used to send messages to the browser. Requires the message
//      name as the first argument and can have an optional second argument that
//      should be an array.
//  - chrome.getVariableValue: Returns value for the input variable name if such
//      a value was set by the browser. Else will return an empty string.
void WebUIExtension::Install(blink::WebLocalFrame* frame) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> chrome = GetOrCreateChromeObject(isolate, context);
  chrome
      ->Set(context, gin::StringToSymbol(isolate, "send"),
            gin::CreateFunctionTemplate(
                isolate, base::BindRepeating(&WebUIExtension::Send))
                ->GetFunction(context)
                .ToLocalChecked())
      .Check();
  chrome
      ->Set(context, gin::StringToSymbol(isolate, "getVariableValue"),
            gin::CreateFunctionTemplate(
                isolate, base::BindRepeating(&WebUIExtension::GetVariableValue))
                ->GetFunction(context)
                .ToLocalChecked())
      .Check();
}

// static
void WebUIExtension::Send(gin::Arguments* args) {
  blink::WebLocalFrame* frame;
  RenderFrame* render_frame;
  if (!ShouldRespondToRequest(&frame, &render_frame))
    return;

  std::string message;
  if (!args->GetNext(&message)) {
    args->ThrowError();
    return;
  }

  // If they've provided an optional message parameter, convert that into a
  // Value to send to the browser process.
  std::unique_ptr<base::ListValue> content;
  if (args->PeekNext().IsEmpty() || args->PeekNext()->IsUndefined()) {
    content.reset(new base::ListValue());
  } else {
    v8::Local<v8::Object> obj;
    if (!args->GetNext(&obj)) {
      args->ThrowError();
      return;
    }

    content = base::ListValue::From(V8ValueConverter::Create()->FromV8Value(
        obj, frame->MainWorldScriptContext()));
    DCHECK(content);
    // The conversion of |obj| could have triggered arbitrary JavaScript code,
    // so check that the frame is still valid to avoid dereferencing a stale
    // pointer.
    if (frame != blink::WebLocalFrame::FrameForCurrentContext()) {
      NOTREACHED();
      return;
    }
  }

  // Send the message up to the browser.
  render_frame->Send(new FrameHostMsg_WebUISend(render_frame->GetRoutingID(),
                                                message, *content));
}

// static
std::string WebUIExtension::GetVariableValue(const std::string& name) {
  blink::WebLocalFrame* frame;
  RenderFrame* render_frame;
  if (!ShouldRespondToRequest(&frame, &render_frame))
    return std::string();

  return WebUIExtensionData::Get(render_frame)->GetValue(name);
}

}  // namespace content
