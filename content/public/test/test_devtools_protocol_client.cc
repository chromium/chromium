// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_devtools_protocol_client.h"

#include <memory>
#include <string_view>

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
    const std::string method,
    base::Value::Dict params,
    const std::string session_id,
    bool wait) {
  response_.clear();
  base::AutoReset<bool> reset_in_dispatch(&in_dispatch_, true);
  base::Value::Dict command;
  command.Set(kIdParam, ++last_sent_id_);
  command.Set(kMethodParam, std::move(method));
  if (params.size())
    command.Set(kParamsParam, std::move(params));
  if (!session_id.empty())
    command.Set(kSessionIdParam, std::move(session_id));

  std::string json_command;
  base::JSONWriter::Write(base::Value(std::move(command)), &json_command);
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

void TestDevToolsProtocolClient::AttachToTabTarget(WebContents* wc) {
  agent_host_ = DevToolsAgentHost::GetOrCreateForTab(wc);
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
  return HasExistingNotificationMatching(
      [&search](const base::Value::Dict& notification) {
        return *notification.FindString(kMethodParam) == search;
      });
}

bool TestDevToolsProtocolClient::HasExistingNotificationMatching(
    base::FunctionRef<bool(const base::Value::Dict&)> pred) const {
  for (const auto& notification : notifications_) {
    if (pred(notification)) {
      return true;
    }
  }
  return false;
}

base::Value::Dict TestDevToolsProtocolClient::WaitForNotification(
    const std::string& notification,
    bool allow_existing) {
  if (allow_existing) {
    for (auto it = notifications_.begin(); it != notifications_.end(); ++it) {
      if (*it->FindString(kMethodParam) != notification)
        continue;
      base::Value::Dict result;
      if (base::Value::Dict* params = it->FindDict(kParamsParam))
        result = std::move(*params);
      notifications_.erase(it);
      return result;
    }
  }

  waiting_for_notification_ = notification;
  RunLoopUpdatingQuitClosure();
  return std::move(received_notification_params_);
}

base::Value::Dict TestDevToolsProtocolClient::WaitForMatchingNotification(
    const std::string& notification,
    const NotificationMatcher& matcher) {
  for (auto it = notifications_.begin(); it != notifications_.end(); ++it) {
    if (*it->FindString(kMethodParam) != notification)
      continue;
    base::Value* params = it->Find(kParamsParam);
    if (!params || !matcher.Run(params->GetDict()))
      continue;
    base::Value::Dict result = std::move(*params).TakeDict();
    notifications_.erase(it);
    return result;
  }

  waiting_for_notification_ = notification;
  waiting_for_notification_matcher_ = matcher;
  RunLoopUpdatingQuitClosure();
  return std::move(received_notification_params_);
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
  std::string_view message_str(reinterpret_cast<const char*>(message.data()),
                               message.size());
  base::Value parsed = *base::JSONReader::Read(message_str);
  if (std::optional<int> id = parsed.GetDict().FindInt("id")) {
    received_responses_count_++;
    response_ = std::move(parsed).TakeDict();
    in_dispatch_ = false;
    if (*id && *id == waiting_for_command_result_id_) {
      waiting_for_command_result_id_ = 0;
      std::move(run_loop_quit_closure_).Run();
    }
  } else {
    const std::string* notification = parsed.GetDict().FindString("method");
    notifications_.push_back(std::move(parsed).TakeDict());
    if (waiting_for_notification_ != *notification)
      return;
    const base::Value* params = notifications_.back().Find(kParamsParam);
    if (waiting_for_notification_matcher_.is_null() ||
        waiting_for_notification_matcher_.Run(params->GetDict())) {
      waiting_for_notification_ = std::string();
      waiting_for_notification_matcher_ = NotificationMatcher();
      received_notification_params_ = params->GetDict().Clone();
      std::move(run_loop_quit_closure_).Run();
    }
  }
}

void TestDevToolsProtocolClient::AgentHostClosed(
    DevToolsAgentHost* agent_host) {
  if (!agent_host_can_close_)
    NOTREACHED_IN_MIGRATION();
}

bool TestDevToolsProtocolClient::AllowUnsafeOperations() {
  return allow_unsafe_operations_;
}

bool TestDevToolsProtocolClient::IsTrusted() {
  return is_trusted_;
}

bool TestDevToolsProtocolClient::MayReadLocalFiles() {
  return may_read_local_files_;
}

bool TestDevToolsProtocolClient::MayWriteLocalFiles() {
  return may_write_local_files_;
}

bool TestDevToolsProtocolClient::MayAttachToURL(const GURL& url,
                                                bool is_webui) {
  return not_attachable_hosts_.find(url.host()) == not_attachable_hosts_.end();
}

std::optional<url::Origin>
TestDevToolsProtocolClient::GetNavigationInitiatorOrigin() {
  return navigation_initiator_origin_;
}

}  // namespace content
