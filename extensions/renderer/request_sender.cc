// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/request_sender.h"

#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/ipc_message_sender.h"
#include "extensions/renderer/script_context.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_scoped_user_gesture.h"
#include "third_party/blink/public/web/web_user_gesture_indicator.h"
#include "third_party/blink/public/web/web_user_gesture_token.h"

namespace extensions {

// Contains info relevant to a pending API request.
struct PendingRequest {
 public:
  PendingRequest(const std::string& name,
                 RequestSender::Source* source,
                 blink::WebUserGestureToken token)
      : name(name), source(source), token(token) {}

  std::string name;
  RequestSender::Source* source;
  blink::WebUserGestureToken token;
};

RequestSender::RequestSender(IPCMessageSender* ipc_message_sender)
    : ipc_message_sender_(ipc_message_sender) {}

RequestSender::~RequestSender() {}

void RequestSender::InsertRequest(
    int request_id,
    std::unique_ptr<PendingRequest> pending_request) {
  DCHECK_EQ(0u, pending_requests_.count(request_id));
  pending_requests_[request_id] = std::move(pending_request);
}

std::unique_ptr<PendingRequest> RequestSender::RemoveRequest(int request_id) {
  auto i = pending_requests_.find(request_id);
  if (i == pending_requests_.end())
    return std::unique_ptr<PendingRequest>();
  std::unique_ptr<PendingRequest> result = std::move(i->second);
  pending_requests_.erase(i);
  return result;
}

int RequestSender::GetNextRequestId() const {
  static int next_request_id = 0;
  return next_request_id++;
}

bool RequestSender::StartRequest(Source* source,
                                 const std::string& name,
                                 int request_id,
                                 bool has_callback,
                                 bool for_io_thread,
                                 base::ListValue* value_args) {
  ScriptContext* context = source->GetContext();
  if (!context)
    return false;

  bool for_service_worker =
      context->context_type() == Feature::SERVICE_WORKER_CONTEXT;
  // Get the current RenderFrame so that we can send a routed IPC message from
  // the correct source.
  // Note that |render_frame| would be nullptr for Service Workers. Service
  // Workers use control IPC instead.
  content::RenderFrame* render_frame = context->GetRenderFrame();
  if (!for_service_worker && !render_frame) {
    // It is important to early exit here for non Service Worker contexts so
    // that we do not create orphaned PendingRequests below.
    return false;
  }

  // TODO(koz): See if we can make this a CHECK.
  if (!context->HasAccessOrThrowError(name))
    return false;

  GURL source_url;
  blink::WebLocalFrame* webframe = context->web_frame();
  if (webframe)
    source_url = webframe->GetDocument().Url();

  InsertRequest(request_id,
                std::make_unique<PendingRequest>(
                    name, source,
                    blink::WebUserGestureIndicator::CurrentUserGestureToken()));

  auto params = std::make_unique<ExtensionHostMsg_Request_Params>();
  params->name = name;
  params->arguments.Swap(value_args);
  params->extension_id = context->GetExtensionID();
  params->source_url = source_url;
  params->request_id = request_id;
  params->has_callback = has_callback;

  // TODO(mustaq): What to do with extension service workers without
  // RenderFrames? crbug/733679
  params->user_gesture =
      blink::WebUserGestureIndicator::IsProcessingUserGestureThreadSafe(
          webframe);

  // Set Service Worker specific params to default values.
  params->worker_thread_id = -1;
  params->service_worker_version_id =
      blink::mojom::kInvalidServiceWorkerVersionId;

  binding::RequestThread thread =
      for_io_thread ? binding::RequestThread::IO : binding::RequestThread::UI;
  ipc_message_sender_->SendRequestIPC(context, std::move(params), thread);
  return true;
}

void RequestSender::HandleResponse(int request_id,
                                   bool success,
                                   const base::ListValue& response,
                                   const std::string& error) {
  base::ElapsedTimer timer;
  std::unique_ptr<PendingRequest> request = RemoveRequest(request_id);

  if (!request.get()) {
    // This can happen if a context is destroyed while a request is in flight.
    return;
  }

  // TODO(devlin): Would it be useful to partition this data based on
  // extension function once we have a suitable baseline? crbug.com/608561.
  blink::WebScopedUserGesture gesture(request->token);
  request->source->OnResponseReceived(
      request->name, request_id, success, response, error);
  UMA_HISTOGRAM_TIMES("Extensions.Functions.HandleResponseElapsedTime",
                      timer.Elapsed());
}

void RequestSender::InvalidateSource(Source* source) {
  for (auto it = pending_requests_.begin(); it != pending_requests_.end();) {
    if (it->second->source == source)
      pending_requests_.erase(it++);
    else
      ++it;
  }
}

}  // namespace extensions
