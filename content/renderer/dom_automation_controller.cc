// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/dom_automation_controller.h"

#include "base/json/json_string_value_serializer.h"
#include "base/strings/string_util.h"
#include "content/common/frame_messages.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/v8_value_converter_impl.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace content {

gin::WrapperInfo DomAutomationController::kWrapperInfo = {
    gin::kEmbedderNativeGin};

// static
void DomAutomationController::Install(RenderFrame* render_frame,
                                      blink::WebLocalFrame* frame) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<DomAutomationController> controller =
      gin::CreateHandle(isolate, new DomAutomationController(render_frame));
  if (controller.IsEmpty())
    return;

  v8::Local<v8::Object> global = context->Global();
  global
      ->Set(context, gin::StringToV8(isolate, "domAutomationController"),
            controller.ToV8())
      .Check();
}

DomAutomationController::DomAutomationController(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {}

DomAutomationController::~DomAutomationController() {}

gin::ObjectTemplateBuilder DomAutomationController::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<DomAutomationController>::GetObjectTemplateBuilder(
             isolate)
      .SetMethod("send", &DomAutomationController::SendMsg);
}

void DomAutomationController::OnDestruct() {}

void DomAutomationController::DidCreateScriptContext(
    v8::Local<v8::Context> context,
    int32_t world_id) {
  // Add the domAutomationController to isolated worlds as well.
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  // Resuse this object instead of creating others.
  gin::Handle<DomAutomationController> controller =
      gin::CreateHandle(isolate, this);
  if (controller.IsEmpty())
    return;

  v8::Local<v8::Object> global = context->Global();
  global
      ->Set(context, gin::StringToV8(isolate, "domAutomationController"),
            controller.ToV8())
      .Check();
}

bool DomAutomationController::SendMsg(const gin::Arguments& args) {
  if (!render_frame())
    return false;

  std::string json;
  JSONStringValueSerializer serializer(&json);
  std::unique_ptr<base::Value> value;

  // Warning: note that JSON officially requires the root-level object to be
  // an object (e.g. {foo:3}) or an array, while here we're serializing
  // strings, bools, etc. to "JSON".  This only works because (a) the JSON
  // writer is lenient, and (b) on the receiving side we wrap the JSON string
  // in square brackets, converting it to an array, then parsing it and
  // grabbing the 0th element to get the value out.
  if (!args.PeekNext().IsEmpty()) {
    V8ValueConverterImpl conv;
    value =
        conv.FromV8Value(args.PeekNext(), args.isolate()->GetCurrentContext());
  } else {
    NOTREACHED() << "No arguments passed to domAutomationController.send";
    return false;
  }

  if (!value || !serializer.Serialize(*value))
    return false;

  return Send(new FrameHostMsg_DomOperationResponse(routing_id(), json));
}

}  // namespace content
