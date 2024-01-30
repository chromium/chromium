// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/wake_event_page.h"

#include <memory>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/memory/weak_ptr.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/worker_thread.h"
#include "extensions/common/mojom/renderer_host.mojom.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/service_worker_data.h"
#include "extensions/renderer/v8_helpers.h"
#include "extensions/renderer/worker_thread_dispatcher.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-persistent-handle.h"
#include "v8/include/v8-primitive.h"

namespace extensions {

namespace {

constexpr char kWakeEventPageFunctionName[] = "WakeEventPage";

}  // namespace

class WakeEventPage::WakeEventPageNativeHandler
    : public ObjectBackedNativeHandler {
 public:
  // Handles own lifetime.
  WakeEventPageNativeHandler(ScriptContext* context)
      : ObjectBackedNativeHandler(context) {
    // Delete self on invalidation. base::Unretained because by definition this
    // can't be deleted before it's deleted.
    context->AddInvalidationObserver(base::BindOnce(
        &WakeEventPageNativeHandler::DeleteSelf, base::Unretained(this)));
  }

  // ObjectBackedNativeHandler:
  void AddRoutes() override {
    // Use Unretained not a WeakPtr because RouteHandlerFunction is tied to the
    // lifetime of this, so there is no way for DoWakeEventPage to be called
    // after destruction.
    RouteHandlerFunction(
        kWakeEventPageFunctionName,
        base::BindRepeating(&WakeEventPageNativeHandler::DoWakeEventPage,
                            base::Unretained(this)));
  }

  WakeEventPageNativeHandler(const WakeEventPageNativeHandler&) = delete;
  WakeEventPageNativeHandler& operator=(const WakeEventPageNativeHandler&) =
      delete;

  ~WakeEventPageNativeHandler() override {}

 private:
  void DeleteSelf() {
    Invalidate();
    delete this;
  }

  // Called by JavaScript with a single argument, the function to call when the
  // event page has been woken.
  void DoWakeEventPage(const v8::FunctionCallbackInfo<v8::Value>& args) {
    CHECK_EQ(1, args.Length());
    CHECK(args[0]->IsFunction());
    v8::Global<v8::Function> callback(args.GetIsolate(),
                                      args[0].As<v8::Function>());

    const std::string& extension_id = context()->GetExtensionID();
    CHECK(!extension_id.empty());

    mojom::RendererHost* renderer_host = nullptr;
    if (context()->IsForServiceWorker()) {
      renderer_host =
          WorkerThreadDispatcher::GetServiceWorkerData()->GetRendererHost();
    } else {
      content::RenderFrame* frame = context()->GetRenderFrame();
      CHECK(frame);
      renderer_host = ExtensionFrameHelper::Get(frame)->GetRendererHost();
    }
    CHECK(renderer_host);
    renderer_host->WakeEventPage(
        extension_id,
        base::BindOnce(&WakeEventPageNativeHandler::OnEventPageIsAwake,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnEventPageIsAwake(v8::Global<v8::Function> callback, bool success) {
    v8::Isolate* isolate = context()->isolate();
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Value> args[] = {
        v8::Boolean::New(isolate, success),
    };
    context()->SafeCallFunction(v8::Local<v8::Function>::New(isolate, callback),
                                std::size(args), args);
  }

  base::WeakPtrFactory<WakeEventPageNativeHandler> weak_ptr_factory_{this};
};

// static
v8::Local<v8::Function> WakeEventPage::GetForContext(ScriptContext* context) {
  v8::Isolate* isolate = context->isolate();
  v8::EscapableHandleScope handle_scope(isolate);
  v8::Local<v8::Context> v8_context = context->v8_context();
  v8::Context::Scope context_scope(v8_context);

  // Cache the imported function as a hidden property on the global object of
  // |v8_context|. Creating it isn't free.
  v8::Local<v8::Private> kWakeEventPageKey = v8::Private::ForApi(
      isolate, v8_helpers::ToV8StringUnsafe(isolate, "WakeEventPage"));
  v8::Local<v8::Value> wake_event_page;
  if (!v8_context->Global()
           ->GetPrivate(v8_context, kWakeEventPageKey)
           .ToLocal(&wake_event_page) ||
      wake_event_page->IsUndefined()) {
    // Implement this using a NativeHandler, which requires a function name
    // (arbitrary in this case). Handles own lifetime.
    WakeEventPageNativeHandler* native_handler =
        new WakeEventPageNativeHandler(context);
    native_handler->Initialize();

    // Extract and cache the wake-event-page function from the native handler.
    wake_event_page = v8_helpers::GetPropertyUnsafe(
        v8_context, native_handler->NewInstance(), kWakeEventPageFunctionName);
    v8_context->Global()
        ->SetPrivate(v8_context, kWakeEventPageKey, wake_event_page)
        .FromJust();
  }

  CHECK(wake_event_page->IsFunction());
  return handle_scope.Escape(wake_event_page.As<v8::Function>());
}

}  //  namespace extensions
