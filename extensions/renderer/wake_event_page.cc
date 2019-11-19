// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/wake_event_page.h"

#include <memory>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/worker_thread.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/v8_helpers.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"

namespace extensions {

namespace {

base::LazyInstance<WakeEventPage>::DestructorAtExit g_wake_event_page_instance =
    LAZY_INSTANCE_INITIALIZER;

constexpr char kWakeEventPageFunctionName[] = "WakeEventPage";

}  // namespace

class WakeEventPage::WakeEventPageNativeHandler
    : public ObjectBackedNativeHandler {
 public:
  // Handles own lifetime.
  WakeEventPageNativeHandler(ScriptContext* context,
                             const MakeRequestCallback& make_request)
      : ObjectBackedNativeHandler(context), make_request_(make_request) {
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

    make_request_.Run(
        extension_id,
        base::Bind(&WakeEventPageNativeHandler::OnEventPageIsAwake,
                   weak_ptr_factory_.GetWeakPtr(), base::Passed(&callback)));
  }

  void OnEventPageIsAwake(v8::Global<v8::Function> callback, bool success) {
    v8::Isolate* isolate = context()->isolate();
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Value> args[] = {
        v8::Boolean::New(isolate, success),
    };
    context()->SafeCallFunction(v8::Local<v8::Function>::New(isolate, callback),
                                base::size(args), args);
  }

  MakeRequestCallback make_request_;
  base::WeakPtrFactory<WakeEventPageNativeHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WakeEventPageNativeHandler);
};

// static
WakeEventPage* WakeEventPage::Get() {
  return g_wake_event_page_instance.Pointer();
}

void WakeEventPage::Init(content::RenderThread* render_thread) {
  DCHECK(render_thread);
  DCHECK_EQ(content::RenderThread::Get(), render_thread);
  DCHECK(!message_filter_);

  message_filter_ = render_thread->GetSyncMessageFilter();
  render_thread->AddObserver(this);
}

v8::Local<v8::Function> WakeEventPage::GetForContext(ScriptContext* context) {
  DCHECK(message_filter_);

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
    WakeEventPageNativeHandler* native_handler = new WakeEventPageNativeHandler(
        context, base::Bind(&WakeEventPage::MakeRequest,
                            // Safe, owned by a LazyInstance.
                            base::Unretained(this)));
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

WakeEventPage::RequestData::RequestData(int thread_id,
                                        const OnResponseCallback& on_response)
    : thread_id(thread_id), on_response(on_response) {}

WakeEventPage::RequestData::~RequestData() {}

WakeEventPage::WakeEventPage() {}

WakeEventPage::~WakeEventPage() {}

void WakeEventPage::MakeRequest(const std::string& extension_id,
                                const OnResponseCallback& on_response) {
  static base::AtomicSequenceNumber sequence_number;
  int request_id = sequence_number.GetNext();
  {
    base::AutoLock lock(requests_lock_);
    requests_[request_id] = std::make_unique<RequestData>(
        content::WorkerThread::GetCurrentId(), on_response);
  }
  message_filter_->Send(
      new ExtensionHostMsg_WakeEventPage(request_id, extension_id));
}

bool WakeEventPage::OnControlMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WakeEventPage, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_WakeEventPageResponse,
                        OnWakeEventPageResponse)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WakeEventPage::OnWakeEventPageResponse(int request_id, bool success) {
  std::unique_ptr<RequestData> request_data;
  {
    base::AutoLock lock(requests_lock_);
    auto it = requests_.find(request_id);
    CHECK(it != requests_.end()) << "No request with ID " << request_id;
    request_data = std::move(it->second);
    requests_.erase(it);
  }
  if (request_data->thread_id == 0) {
    // Thread ID of 0 means it wasn't called on a worker thread, so safe to
    // call immediately.
    request_data->on_response.Run(success);
  } else {
    content::WorkerThread::PostTask(
        request_data->thread_id,
        base::Bind(request_data->on_response, success));
  }
}

}  //  namespace extensions
