// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/internal/headless_devtools_client_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "headless/public/headless_devtools_target.h"

namespace headless {

namespace {
int g_next_message_id = 0;
int g_next_raw_message_id = 1;
}  // namespace

// static
std::unique_ptr<HeadlessDevToolsClient> HeadlessDevToolsClient::Create() {
  auto result = std::make_unique<HeadlessDevToolsClientImpl>();
  result->InitBrowserMainThread();
  return result;
}

// static
std::unique_ptr<HeadlessDevToolsClient>
HeadlessDevToolsClient::CreateWithExternalHost(ExternalHost* external_host) {
  auto result = std::make_unique<HeadlessDevToolsClientImpl>();
  result->AttachToExternalHost(external_host);
  return result;
}

HeadlessDevToolsClientImpl::HeadlessDevToolsClientImpl()
    : accessibility_domain_(this),
      animation_domain_(this),
      browser_domain_(this),
      cache_storage_domain_(this),
      console_domain_(this),
      css_domain_(this),
      database_domain_(this),
      debugger_domain_(this),
      device_orientation_domain_(this),
      dom_domain_(this),
      dom_debugger_domain_(this),
      dom_snapshot_domain_(this),
      dom_storage_domain_(this),
      emulation_domain_(this),
      fetch_domain_(this),
      headless_experimental_domain_(this),
      heap_profiler_domain_(this),
      indexeddb_domain_(this),
      input_domain_(this),
      inspector_domain_(this),
      io_domain_(this),
      layer_tree_domain_(this),
      log_domain_(this),
      memory_domain_(this),
      network_domain_(this),
      page_domain_(this),
      performance_domain_(this),
      profiler_domain_(this),
      runtime_domain_(this),
      security_domain_(this),
      service_worker_domain_(this),
      target_domain_(this),
      tracing_domain_(this) {}

HeadlessDevToolsClientImpl::~HeadlessDevToolsClientImpl() {
  if (parent_client_)
    parent_client_->sessions_.erase(session_id_);
}

void HeadlessDevToolsClientImpl::AttachToExternalHost(
    ExternalHost* external_host) {
  DCHECK(!channel_ && !external_host_);
  external_host_ = external_host;
}

void HeadlessDevToolsClientImpl::InitBrowserMainThread() {
  browser_main_thread_ = content::GetUIThreadTaskRunner({});
}

void HeadlessDevToolsClientImpl::ChannelClosed() {
  pending_messages_.clear();
  channel_ = nullptr;
}

void HeadlessDevToolsClientImpl::AttachToChannel(
    std::unique_ptr<HeadlessDevToolsChannel> channel) {
  DCHECK(!channel_ && !external_host_);
  channel_ = std::move(channel);
  channel_->SetClient(this);
}

void HeadlessDevToolsClientImpl::DetachFromChannel() {
  pending_messages_.clear();
  channel_ = nullptr;
}

void HeadlessDevToolsClientImpl::SetRawProtocolListener(
    RawProtocolListener* raw_protocol_listener) {
  raw_protocol_listener_ = raw_protocol_listener;
}

std::unique_ptr<HeadlessDevToolsClient>
HeadlessDevToolsClientImpl::CreateSession(const std::string& session_id) {
  std::unique_ptr<HeadlessDevToolsClientImpl> client =
      std::make_unique<HeadlessDevToolsClientImpl>();
  client->parent_client_ = this;
  client->session_id_ = session_id;
  client->browser_main_thread_ = browser_main_thread_;
  sessions_[session_id] = client.get();
  return client;
}

int HeadlessDevToolsClientImpl::GetNextRawDevToolsMessageId() {
  int id = g_next_raw_message_id;
  g_next_raw_message_id += 2;
  return id;
}

void HeadlessDevToolsClientImpl::SendRawDevToolsMessage(
    const std::string& json_message) {
  std::unique_ptr<base::Value> message =
      base::JSONReader::ReadDeprecated(json_message);
  if (!message || !message->is_dict()) {
    LOG(ERROR) << "Malformed raw message";
    return;
  }
  if (!session_id_.empty())
    message->GetDict().Set("sessionId", session_id_);
  SendProtocolMessage(message->GetDict());
}

void HeadlessDevToolsClientImpl::DispatchMessageFromExternalHost(
    base::span<const uint8_t> json_message) {
  DCHECK(external_host_);
  ReceiveProtocolMessage(json_message);
}

void HeadlessDevToolsClientImpl::ReceiveProtocolMessage(
    base::span<const uint8_t> json_message) {
  base::StringPiece message_str(
      reinterpret_cast<const char*>(json_message.data()), json_message.size());
  // LOG(ERROR) << "[RECV] " << message_str;
  absl::optional<base::Value> message =
      base::JSONReader::Read(message_str, base::JSON_PARSE_RFC);
  if (!message || !message->is_dict()) {
    NOTREACHED() << "Badly formed reply " << message_str;
    return;
  }

  base::Value::Dict message_dict = std::move(*message).TakeDict();
  const std::string* session_id = message_dict.FindString("sessionId");
  if (session_id) {
    auto it = sessions_.find(*session_id);
    if (it != sessions_.end()) {
      it->second->ReceiveProtocolMessage(json_message, std::move(message_dict));
      return;
    }
  }
  ReceiveProtocolMessage(json_message, std::move(message_dict));
}

void HeadlessDevToolsClientImpl::ReceiveProtocolMessage(
    base::span<const uint8_t> json_message,
    base::Value::Dict message) {
  // Use of the legacy Value API below is needed since `HeadlessDevToolsClient`
  // is a public interface which we cannot change.
  if (raw_protocol_listener_ &&
      raw_protocol_listener_->OnProtocolMessage(
          json_message,
          base::Value::AsDictionaryValue(base::Value(message.Clone())))) {
    return;
  }

  bool success = false;
  if (message.contains("id")) {
    success = DispatchMessageReply(std::move(message));
  } else {
    success = DispatchEvent(std::move(message));
  }

  if (!success) {
    DLOG(ERROR) << "Unhandled protocol message: "
                << base::StringPiece(
                       reinterpret_cast<const char*>(json_message.data()),
                       json_message.size());
  }
}

bool HeadlessDevToolsClientImpl::DispatchMessageReply(
    base::Value::Dict message_dict) {
  auto maybe_id = message_dict.FindInt("id");
  if (!maybe_id.has_value()) {
    NOTREACHED() << "ID must be specified.";
    return false;
  }
  auto it = pending_messages_.find(maybe_id.value());
  if (it == pending_messages_.end()) {
    NOTREACHED() << "Unexpected reply";
    return false;
  }
  Callback callback = std::move(it->second);
  pending_messages_.erase(it);
  if (!callback.callback_with_result.is_null()) {
    const base::Value::Dict* maybe_result_dict =
        message_dict.FindDict("result");
    if (maybe_result_dict) {
      if (browser_main_thread_) {
        browser_main_thread_->PostTask(
            FROM_HERE,
            base::BindOnce(
                &HeadlessDevToolsClientImpl::DispatchMessageReplyWithResultTask,
                weak_ptr_factory_.GetWeakPtr(),
                std::move(callback.callback_with_result),
                base::Value(maybe_result_dict->Clone())));
      } else {
        std::move(callback.callback_with_result)
            .Run(base::Value(maybe_result_dict->Clone()));
      }

      return true;
    }

    maybe_result_dict = message_dict.FindDict("error");
    if (maybe_result_dict) {
      DLOG(ERROR) << "Error in method call result: " << *maybe_result_dict;
      if (browser_main_thread_) {
        browser_main_thread_->PostTask(
            FROM_HERE,
            base::BindOnce(
                &HeadlessDevToolsClientImpl::DispatchMessageReplyWithResultTask,
                weak_ptr_factory_.GetWeakPtr(),
                std::move(callback.callback_with_result), base::Value()));
      } else {
        std::move(callback.callback_with_result).Run(base::Value());
      }

      return true;
    }

    NOTREACHED() << "Reply has neither result nor error";
    return false;
  }

  if (!callback.callback.is_null()) {
    if (browser_main_thread_) {
      browser_main_thread_->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](base::WeakPtr<HeadlessDevToolsClientImpl> self,
                 base::OnceClosure callback) {
                if (self)
                  std::move(callback).Run();
              },
              weak_ptr_factory_.GetWeakPtr(), std::move(callback.callback)));
    } else {
      std::move(callback.callback).Run();
    }
  }
  return true;
}

void HeadlessDevToolsClientImpl::DispatchMessageReplyWithResultTask(
    base::OnceCallback<void(const base::Value&)> callback,
    base::Value result) {
  DCHECK(result.is_dict() || result.is_none());

  std::move(callback).Run(result);
}

bool HeadlessDevToolsClientImpl::DispatchEvent(base::Value::Dict message_dict) {
  const std::string* method = message_dict.FindString("method");
  if (!method)
    return false;
  if (*method == "Inspector.targetCrashed")
    renderer_crashed_ = true;
  EventHandlerMap::const_iterator it = event_handlers_.find(*method);
  if (it == event_handlers_.end()) {
    if (*method != "Inspector.targetCrashed")
      NOTREACHED() << "Unknown event: " << method;
    return false;
  }
  if (!it->second.is_null()) {
    const base::Value::Dict* maybe_result_dict =
        message_dict.FindDict("params");
    if (!maybe_result_dict) {
      NOTREACHED() << "Badly formed event parameters";
      return false;
    }

    if (browser_main_thread_) {
      // DevTools assumes event handling is async so we must post a task here or
      // we risk breaking things.
      browser_main_thread_->PostTask(
          FROM_HERE,
          base::BindOnce(&HeadlessDevToolsClientImpl::DispatchEventTask,
                         weak_ptr_factory_.GetWeakPtr(), &it->second,
                         maybe_result_dict->Clone()));
    } else {
      DispatchEventTask(&it->second, maybe_result_dict->Clone());
    }
  }
  return true;
}

void HeadlessDevToolsClientImpl::DispatchEventTask(
    const EventHandler* event_handler,
    base::Value::Dict result_dict) {
  event_handler->Run(base::Value(std::move(result_dict)));
}

accessibility::Domain* HeadlessDevToolsClientImpl::GetAccessibility() {
  return &accessibility_domain_;
}

animation::Domain* HeadlessDevToolsClientImpl::GetAnimation() {
  return &animation_domain_;
}

browser::Domain* HeadlessDevToolsClientImpl::GetBrowser() {
  return &browser_domain_;
}

cache_storage::Domain* HeadlessDevToolsClientImpl::GetCacheStorage() {
  return &cache_storage_domain_;
}

console::Domain* HeadlessDevToolsClientImpl::GetConsole() {
  return &console_domain_;
}

css::Domain* HeadlessDevToolsClientImpl::GetCSS() {
  return &css_domain_;
}

database::Domain* HeadlessDevToolsClientImpl::GetDatabase() {
  return &database_domain_;
}

debugger::Domain* HeadlessDevToolsClientImpl::GetDebugger() {
  return &debugger_domain_;
}

device_orientation::Domain* HeadlessDevToolsClientImpl::GetDeviceOrientation() {
  return &device_orientation_domain_;
}

dom::Domain* HeadlessDevToolsClientImpl::GetDOM() {
  return &dom_domain_;
}

dom_debugger::Domain* HeadlessDevToolsClientImpl::GetDOMDebugger() {
  return &dom_debugger_domain_;
}

dom_snapshot::Domain* HeadlessDevToolsClientImpl::GetDOMSnapshot() {
  return &dom_snapshot_domain_;
}

dom_storage::Domain* HeadlessDevToolsClientImpl::GetDOMStorage() {
  return &dom_storage_domain_;
}

emulation::Domain* HeadlessDevToolsClientImpl::GetEmulation() {
  return &emulation_domain_;
}

fetch::Domain* HeadlessDevToolsClientImpl::GetFetch() {
  return &fetch_domain_;
}

headless_experimental::Domain*
HeadlessDevToolsClientImpl::GetHeadlessExperimental() {
  return &headless_experimental_domain_;
}

heap_profiler::Domain* HeadlessDevToolsClientImpl::GetHeapProfiler() {
  return &heap_profiler_domain_;
}

indexeddb::Domain* HeadlessDevToolsClientImpl::GetIndexedDB() {
  return &indexeddb_domain_;
}

input::Domain* HeadlessDevToolsClientImpl::GetInput() {
  return &input_domain_;
}

inspector::Domain* HeadlessDevToolsClientImpl::GetInspector() {
  return &inspector_domain_;
}

io::Domain* HeadlessDevToolsClientImpl::GetIO() {
  return &io_domain_;
}

layer_tree::Domain* HeadlessDevToolsClientImpl::GetLayerTree() {
  return &layer_tree_domain_;
}

log::Domain* HeadlessDevToolsClientImpl::GetLog() {
  return &log_domain_;
}

memory::Domain* HeadlessDevToolsClientImpl::GetMemory() {
  return &memory_domain_;
}

network::Domain* HeadlessDevToolsClientImpl::GetNetwork() {
  return &network_domain_;
}

page::Domain* HeadlessDevToolsClientImpl::GetPage() {
  return &page_domain_;
}

performance::Domain* HeadlessDevToolsClientImpl::GetPerformance() {
  return &performance_domain_;
}

profiler::Domain* HeadlessDevToolsClientImpl::GetProfiler() {
  return &profiler_domain_;
}

runtime::Domain* HeadlessDevToolsClientImpl::GetRuntime() {
  return &runtime_domain_;
}

security::Domain* HeadlessDevToolsClientImpl::GetSecurity() {
  return &security_domain_;
}

service_worker::Domain* HeadlessDevToolsClientImpl::GetServiceWorker() {
  return &service_worker_domain_;
}

target::Domain* HeadlessDevToolsClientImpl::GetTarget() {
  return &target_domain_;
}

tracing::Domain* HeadlessDevToolsClientImpl::GetTracing() {
  return &tracing_domain_;
}

template <typename CallbackType>
void HeadlessDevToolsClientImpl::FinalizeAndSendMessage(
    base::Value::Dict message,
    CallbackType callback) {
  if (renderer_crashed_)
    return;
  int id = g_next_message_id;
  g_next_message_id += 2;  // We only send even numbered messages.
  message.Set("id", id);
  if (!session_id_.empty())
    message.Set("sessionId", session_id_);
  pending_messages_[id] = Callback(std::move(callback));
  SendProtocolMessage(message);
}

void HeadlessDevToolsClientImpl::SendProtocolMessage(
    const base::Value::Dict& message) {
  if (parent_client_) {
    parent_client_->SendProtocolMessage(message);
    return;
  }

  std::string json_message;
  base::JSONWriter::Write(message, &json_message);
  // LOG(ERROR) << "[SEND] " << json_message;
  auto bytes_message = base::as_bytes(base::make_span(json_message));
  if (channel_)
    channel_->SendProtocolMessage(bytes_message);
  else
    external_host_->SendProtocolMessage(bytes_message);
}

template <typename CallbackType>
void HeadlessDevToolsClientImpl::SendMessageWithParams(const char* method,
                                                       base::Value params,
                                                       CallbackType callback) {
  base::Value::Dict message;
  message.Set("method", method);
  message.Set("params", std::move(params));
  FinalizeAndSendMessage(std::move(message), std::move(callback));
}

void HeadlessDevToolsClientImpl::SendMessage(
    const char* method,
    base::Value params,
    base::OnceCallback<void(const base::Value&)> callback) {
  SendMessageWithParams(method, std::move(params), std::move(callback));
}

void HeadlessDevToolsClientImpl::SendMessage(const char* method,
                                             base::Value params,
                                             base::OnceClosure callback) {
  SendMessageWithParams(method, std::move(params), std::move(callback));
}

void HeadlessDevToolsClientImpl::RegisterEventHandler(
    const char* method,
    base::RepeatingCallback<void(const base::Value&)> callback) {
  DCHECK(event_handlers_.find(method) == event_handlers_.end());
  event_handlers_[method] = std::move(callback);
}

HeadlessDevToolsClientImpl::Callback::Callback() = default;

HeadlessDevToolsClientImpl::Callback::Callback(Callback&& other) = default;

HeadlessDevToolsClientImpl::Callback::Callback(base::OnceClosure callback)
    : callback(std::move(callback)) {}

HeadlessDevToolsClientImpl::Callback::Callback(
    base::OnceCallback<void(const base::Value&)> callback)
    : callback_with_result(std::move(callback)) {}

HeadlessDevToolsClientImpl::Callback::~Callback() = default;

HeadlessDevToolsClientImpl::Callback&
HeadlessDevToolsClientImpl::Callback::operator=(Callback&& other) = default;

}  // namespace headless
