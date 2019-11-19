// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/render_frame_observer_natives.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

namespace {

// Deletes itself when done.
class LoadWatcher : public content::RenderFrameObserver {
 public:
  LoadWatcher(content::RenderFrame* frame,
              const base::Callback<void(bool)>& callback)
      : content::RenderFrameObserver(frame), callback_(callback) {}

  void DidCreateDocumentElement() override {
    // Defer the callback instead of running it now to avoid re-entrancy caused
    // by the JavaScript callback.
    ExtensionFrameHelper::Get(render_frame())
        ->ScheduleAtDocumentStart(base::Bind(callback_, true));
    delete this;
  }

  void DidFailProvisionalLoad() override {
    // Use PostTask to avoid running user scripts while handling this
    // DidFailProvisionalLoad notification.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(callback_, false));
    delete this;
  }

  void OnDestruct() override { delete this; }

 private:
  base::Callback<void(bool)> callback_;

  DISALLOW_COPY_AND_ASSIGN(LoadWatcher);
};

}  // namespace

RenderFrameObserverNatives::RenderFrameObserverNatives(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

RenderFrameObserverNatives::~RenderFrameObserverNatives() {}

void RenderFrameObserverNatives::AddRoutes() {
  RouteHandlerFunction(
      "OnDocumentElementCreated", "app.window",
      base::BindRepeating(&RenderFrameObserverNatives::OnDocumentElementCreated,
                          base::Unretained(this)));
}

void RenderFrameObserverNatives::Invalidate() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  ObjectBackedNativeHandler::Invalidate();
}

void RenderFrameObserverNatives::OnDocumentElementCreated(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 2);
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsFunction());

  int frame_id = args[0].As<v8::Int32>()->Value();

  content::RenderFrame* frame = content::RenderFrame::FromRoutingID(frame_id);
  if (!frame) {
    LOG(WARNING) << "No render frame found to register LoadWatcher.";
    return;
  }

  v8::Global<v8::Function> v8_callback(context()->isolate(),
                                       args[1].As<v8::Function>());
  base::Callback<void(bool)> callback(
      base::Bind(&RenderFrameObserverNatives::InvokeCallback,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&v8_callback)));
  if (ExtensionFrameHelper::Get(frame)->did_create_current_document_element()) {
    // If the document element is already created, then we can call the callback
    // immediately (though use PostTask to ensure that the callback is called
    // asynchronously).
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(callback, true));
  } else {
    new LoadWatcher(frame, callback);
  }

  args.GetReturnValue().Set(true);
}

void RenderFrameObserverNatives::InvokeCallback(
    v8::Global<v8::Function> callback,
    bool succeeded) {
  v8::Isolate* isolate = context()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Value> args[] = {v8::Boolean::New(isolate, succeeded)};
  context()->SafeCallFunction(v8::Local<v8::Function>::New(isolate, callback),
                              base::size(args), args);
}

}  // namespace extensions
