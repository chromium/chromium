// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/web_request_event_handling_tracker.h"

#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "extensions/common/api/web_request/web_request_constants.h"
#include "extensions/common/extension_features.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/ipc_message_sender.h"
#include "extensions/renderer/script_context.h"
#include "v8/include/v8-local-handle.h"

namespace extensions {

WebRequestEventHandlingTracker::WebRequestEventHandlingTracker(
    IPCMessageSender* ipc_message_sender)
    : ipc_message_sender_(ipc_message_sender) {}

WebRequestEventHandlingTracker::~WebRequestEventHandlingTracker() = default;

// static
std::optional<WebRequestEventHandlingTracker::DispatchInfo>
WebRequestEventHandlingTracker::GetBlockingDispatchInfo(
    std::optional<ExtensionId> extension_id,
    const std::string& event_name,
    const base::ListValue& event_args) {
  if (!base::FeatureList::IsEnabled(
          extensions_features::kWebRequestPerContextEventDispatch)) {
    return std::nullopt;
  }
  if (!base::StartsWith(event_name, "webRequest.") &&
      !base::StartsWith(event_name, "webViewInternal.")) {
    return std::nullopt;
  }
  CHECK_EQ(event_name.find('/'), std::string::npos) << event_name;
  if (event_args.size() != 2u) {
    return std::nullopt;
  }

  const base::DictValue* details = event_args[0].GetIfDict();
  const base::DictValue* payload = event_args[1].GetIfDict();
  if (!details || !payload ||
      !payload->FindBool(kContextDispatchAwaitResponseKey).value_or(false)) {
    return std::nullopt;
  }

  const std::string* request_id_string = details->FindString("requestId");
  uint64_t request_id = 0;
  if (!request_id_string ||
      !base::StringToUint64(*request_id_string, &request_id)) {
    return std::nullopt;
  }

  return DispatchInfo{
      std::move(extension_id), event_name, request_id,
      payload->FindInt(kContextDispatchInstanceIdKey).value_or(0)};
}

void WebRequestEventHandlingTracker::ExpectReportFrom(
    ScriptContext& context,
    const DispatchInfo& info) {
  PendingDispatch& dispatch = pending_dispatches_[info];
  DCHECK(!dispatch.all_listeners_notified);
  dispatch.pending_contexts.insert(&context);

  // Watch `context` for invalidation, once per context: a context that goes
  // away before it reports must not hold back the completion signal.
  std::unique_ptr<binding::ContextInvalidationListener>& listener =
      invalidation_listeners_[&context];
  if (!listener) {
    v8::HandleScope handle_scope(context.isolate());
    listener = std::make_unique<binding::ContextInvalidationListener>(
        context.v8_context(),
        base::BindOnce(&WebRequestEventHandlingTracker::OnContextInvalidated,
                       base::Unretained(this), &context));
  }
}

void WebRequestEventHandlingTracker::OnAllListenersNotified(
    const DispatchInfo& info) {
  auto it = pending_dispatches_.try_emplace(info).first;
  it->second.all_listeners_notified = true;
  MaybeFinalize(it);
}

void WebRequestEventHandlingTracker::OnContextReported(
    ScriptContext& context,
    const DispatchInfo& info) {
  auto it = pending_dispatches_.find(info);
  if (it == pending_dispatches_.end() ||
      it->second.pending_contexts.erase(&context) == 0) {
    return;
  }
  MaybeFinalize(it);
}

void WebRequestEventHandlingTracker::OnContextInvalidated(
    ScriptContext* context) {
  invalidation_listeners_.erase(context);
  for (auto it = pending_dispatches_.begin();
       it != pending_dispatches_.end();) {
    it->second.pending_contexts.erase(context);
    // MaybeFinalize() can erase the entry, so advance the iterator first.
    auto current = it++;
    MaybeFinalize(current);
  }
}

void WebRequestEventHandlingTracker::MaybeFinalize(
    PendingDispatchMap::iterator it) {
  PendingDispatch& dispatch = it->second;
  if (!dispatch.all_listeners_notified || !dispatch.pending_contexts.empty()) {
    return;
  }
  const DispatchInfo& info = it->first;
  ipc_message_sender_->SendWebRequestEventHandlingDoneIPC(
      info.extension_id, info.event_name, info.request_id,
      info.web_view_instance_id);
  pending_dispatches_.erase(it);
}

}  // namespace extensions
