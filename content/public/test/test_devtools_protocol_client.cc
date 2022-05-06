// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_devtools_protocol_client.h"

#include <memory>

#include "base/auto_reset.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/mojom/loader/mixed_content.mojom.h"

namespace content {

namespace {

const char kIdParam[] = "id";
const char kSessionIdParam[] = "sessionId";
const char kMethodParam[] = "method";
const char kParamsParam[] = "params";

}  // namespace

TestDevToolsProtocolClient::TestDevToolsProtocolClient() = default;
TestDevToolsProtocolClient::~TestDevToolsProtocolClient() = default;

const base::Value::Dict* TestDevToolsProtocolClient::SendSessionCommand(
    const std::string& method,
    std::unique_ptr<base::Value> params,
    const std::string& session_id,
    bool wait) {
  base::AutoReset<bool> reset_in_dispatch(&in_dispatch_, true);
  base::DictionaryValue command;
  command.SetInteger(kIdParam, ++last_sent_id_);
  command.SetString(kMethodParam, method);
  if (params) {
    command.SetKey(kParamsParam,
                   base::Value::FromUniquePtrValue(std::move(params)));
  }
  if (!session_id.empty())
    command.SetString(kSessionIdParam, session_id);

  std::string json_command;
  base::JSONWriter::Write(command, &json_command);
  agent_host_->DispatchProtocolMessage(
      this, base::as_bytes(base::make_span(json_command)));
  // Some messages are dispatched synchronously.
  // Only run loop if we are not finished yet.
  if (in_dispatch_ && wait)
    WaitForResponse();
  return result();
}

void TestDevToolsProtocolClient::WaitForResponse() {
  waiting_for_command_result_id_ = last_sent_id_;
  RunLoopUpdatingQuitClosure();
}

void TestDevToolsProtocolClient::AttachToWebContents(WebContents* wc) {
  agent_host_ = DevToolsAgentHost::GetOrCreateFor(wc);
  agent_host_->AttachClient(this);
}

void TestDevToolsProtocolClient::AttachToBrowserTarget() {
  // Tethering domain is not used in tests.
  agent_host_ = DevToolsAgentHost::CreateForBrowser(
      nullptr, DevToolsAgentHost::CreateServerSocketCallback());
  agent_host_->AttachClient(this);
}

bool TestDevToolsProtocolClient::HasExistingNotification(
    const std::string& search) const {
  for (const std::string& notification : notifications_) {
    if (notification == search)
      return true;
  }
  return false;
}

std::unique_ptr<base::DictionaryValue>
TestDevToolsProtocolClient::WaitForNotification(const std::string& notification,
                                                bool allow_existing) {
  if (allow_existing) {
    for (size_t i = 0; i < notifications_.size(); i++) {
      if (notifications_[i] == notification) {
        std::unique_ptr<base::DictionaryValue> result =
            std::move(notification_params_[i]);
        notifications_.erase(notifications_.begin() + i);
        notification_params_.erase(notification_params_.begin() + i);
        return result;
      }
    }
  }

  waiting_for_notification_ = notification;
  RunLoopUpdatingQuitClosure();
  return std::move(waiting_for_notification_params_);
}

std::unique_ptr<base::DictionaryValue>
TestDevToolsProtocolClient::WaitForMatchingNotification(
    const std::string& notification,
    const NotificationMatcher& matcher) {
  for (size_t i = 0; i < notifications_.size(); i++) {
    if (notifications_[i] == notification &&
        matcher.Run(notification_params_[i].get())) {
      std::unique_ptr<base::DictionaryValue> result =
          std::move(notification_params_[i]);
      notifications_.erase(notifications_.begin() + i);
      notification_params_.erase(notification_params_.begin() + i);
      return result;
    }
  }

  waiting_for_notification_ = notification;
  waiting_for_notification_matcher_ = matcher;
  RunLoopUpdatingQuitClosure();
  return std::move(waiting_for_notification_params_);
}

const base::Value::Dict* TestDevToolsProtocolClient::result() const {
  return response_.FindDict("result");
}

const base::Value::Dict* TestDevToolsProtocolClient::error() const {
  return response_.FindDict("error");
}

void TestDevToolsProtocolClient::RunLoopUpdatingQuitClosure() {
  base::RunLoop run_loop;
  run_loop_quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

void TestDevToolsProtocolClient::DispatchProtocolMessage(
    DevToolsAgentHost* agent_host,
    base::span<const uint8_t> message) {
  base::StringPiece message_str(reinterpret_cast<const char*>(message.data()),
                                message.size());
  base::Value parsed = *base::JSONReader::Read(message_str);
  if (absl::optional<int> id = parsed.GetDict().FindInt("id")) {
    response_ = std::move(parsed.GetDict());
    result_ids_.push_back(*id);
    in_dispatch_ = false;
    if (*id && *id == waiting_for_command_result_id_) {
      waiting_for_command_result_id_ = 0;
      std::move(run_loop_quit_closure_).Run();
    }
  } else {
    std::string& notification = *parsed.GetDict().FindString("method");
    notifications_.push_back(notification);
    base::Value* params = parsed.GetDict().Find("params");
    if (params) {
      notification_params_.push_back(
          base::Value::AsDictionaryValue(*params).CreateDeepCopy());
    } else {
      notification_params_.push_back(
          base::WrapUnique(new base::DictionaryValue()));
    }
    if (waiting_for_notification_ == notification &&
        (waiting_for_notification_matcher_.is_null() ||
         waiting_for_notification_matcher_.Run(
             notification_params_[notification_params_.size() - 1].get()))) {
      waiting_for_notification_ = std::string();
      waiting_for_notification_matcher_ = NotificationMatcher();
      waiting_for_notification_params_ = base::WrapUnique(
          notification_params_[notification_params_.size() - 1]->DeepCopy());
      std::move(run_loop_quit_closure_).Run();
    }
  }
}

void TestDevToolsProtocolClient::AgentHostClosed(
    DevToolsAgentHost* agent_host) {
  if (!agent_host_can_close_)
    NOTREACHED();
}

bool TestDevToolsProtocolClient::AllowUnsafeOperations() {
  return allow_unsafe_operations_;
}

}  // namespace content
