// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/render_frame_observer_natives.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/script_context.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-primitive.h"

namespace extensions {

namespace {

// Deletes itself when done.
class LoadWatcher : public content::RenderFrameObserver {
 public:
  LoadWatcher(content::RenderFrame* frame,
              base::OnceCallback<void(bool)> callback)
      : content::RenderFrameObserver(frame), callback_(std::move(callback)) {}

  LoadWatcher(const LoadWatcher&) = delete;
  LoadWatcher& operator=(const LoadWatcher&) = delete;

  void DidCreateDocumentElement() override {
    // Defer the callback instead of running it now to avoid re-entrancy caused
    // by the JavaScript callback.
    ExtensionFrameHelper::Get(render_frame())
        ->ScheduleAtDocumentStart(base::BindOnce(std::move(callback_), true));
    delete this;
  }

  void DidFailProvisionalLoad() override {
    // Use PostTask to avoid running user scripts while handling this
    // DidFailProvisionalLoad notification.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), false));
    delete this;
  }

  void OnDestruct() override { delete this; }

 private:
  base::OnceCallback<void(bool)> callback_;
};

}  // namespace

RenderFrameObserverNatives::RenderFrameObserverNatives(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

RenderFrameObserverNatives::~RenderFrameObserverNatives() = default;

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
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsFunction());

  content::RenderFrame* frame =
      ExtensionFrameHelper::FindFrameFromFrameTokenString(context()->isolate(),
                                                          args[0]);
  if (!frame) {
    LOG(WARNING) << "No render frame found to register LoadWatcher.";
    return;
  }

  v8::Global<v8::Function> v8_callback(context()->isolate(),
                                       args[1].As<v8::Function>());
  auto callback(base::BindOnce(&RenderFrameObserverNatives::InvokeCallback,
                               weak_ptr_factory_.GetWeakPtr(),
                               std::move(v8_callback)));
  if (ExtensionFrameHelper::Get(frame)->did_create_current_document_element()) {
    // If the document element is already created, then we can call the callback
    // immediately (though use PostTask to ensure that the callback is called
    // asynchronously).
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
  } else {
    new LoadWatcher(frame, std::move(callback));
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
                              std::size(args), args);
}

}  // namespace extensions
