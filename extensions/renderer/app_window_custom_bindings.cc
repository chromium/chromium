// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/app_window_custom_bindings.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/switches.h"
#include "extensions/grit/extensions_renderer_resources.h"
#include "extensions/renderer/script_context.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8.h"

namespace extensions {

AppWindowCustomBindings::AppWindowCustomBindings(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

void AppWindowCustomBindings::AddRoutes() {
  RouteHandlerFunction("GetFrame",
                       base::BindRepeating(&AppWindowCustomBindings::GetFrame,
                                           base::Unretained(this)));
  RouteHandlerFunction(
      "ResumeParser",
      base::BindRepeating(&AppWindowCustomBindings::ResumeParser,
                          base::Unretained(this)));
}

void AppWindowCustomBindings::GetFrame(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // TODO(jeremya): convert this to IDL nocompile to get validation, and turn
  // these argument checks into CHECK().
  if (args.Length() != 2)
    return;

  if (!args[0]->IsInt32() || !args[1]->IsBoolean())
    return;

  int frame_id = args[0].As<v8::Int32>()->Value();
  bool notify_browser = args[1].As<v8::Boolean>()->Value();

  if (frame_id == MSG_ROUTING_NONE)
    return;

  content::RenderFrame* app_frame =
      content::RenderFrame::FromRoutingID(frame_id);
  if (!app_frame)
    return;

  if (notify_browser) {
    app_frame->Send(
        new ExtensionHostMsg_AppWindowReady(app_frame->GetRoutingID()));
  }

  v8::Local<v8::Value> window =
      app_frame->GetWebFrame()->MainWorldScriptContext()->Global();

  // If the new window loads a sandboxed page and has started loading its
  // document, its security origin is unique and the background script is not
  // allowed accessing its window.
  v8::Local<v8::Context> caller_context =
      args.GetIsolate()->GetCurrentContext();
  if (!ContextCanAccessObject(caller_context,
                              v8::Local<v8::Object>::Cast(window), true)) {
    return;
  }

  args.GetReturnValue().Set(window);
}

void AppWindowCustomBindings::ResumeParser(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 1 || !args[0]->IsInt32()) {
    NOTREACHED();
    return;
  }

  int frame_id = args[0].As<v8::Int32>()->Value();
  content::RenderFrame* app_frame =
      content::RenderFrame::FromRoutingID(frame_id);
  if (!app_frame) {
    NOTREACHED();
    return;
  }

  blink::WebDocumentLoader* loader =
      app_frame->GetWebFrame()->GetDocumentLoader();
  if (!loader) {
    NOTREACHED();
    return;
  }

  loader->ResumeParser();
}

}  // namespace extensions
