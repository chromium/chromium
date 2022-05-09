// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_DEVTOOLS_PROTOCOL_CLIENT_H_
#define CONTENT_PUBLIC_TEST_TEST_DEVTOOLS_PROTOCOL_CLIENT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "base/callback.h"
#include "base/values.h"
#include "content/public/browser/devtools_agent_host.h"

namespace content {

class TestDevToolsProtocolClient : public DevToolsAgentHostClient {
 public:
  typedef base::RepeatingCallback<bool(const base::DictionaryValue*)>
      NotificationMatcher;

  TestDevToolsProtocolClient();
  ~TestDevToolsProtocolClient() override;

 protected:
  const base::Value::Dict* SendCommand(const std::string& method,
                                       std::unique_ptr<base::Value> params) {
    return SendCommand(method, std::move(params), true);
  }

  const base::Value::Dict* SendCommand(const std::string& method,
                                       std::unique_ptr<base::Value> params,
                                       bool wait) {
    return SendSessionCommand(method, std::move(params), std::string(), wait);
  }

  const base::Value::Dict* SendSessionCommand(
      const std::string& method,
      std::unique_ptr<base::Value> params,
      const std::string& session_id) {
    return SendSessionCommand(method, std::move(params), session_id, true);
  }

  const base::Value::Dict* SendSessionCommand(
      const std::string& method,
      std::unique_ptr<base::Value> params,
      const std::string& session_id,
      bool wait);

  void AttachToWebContents(WebContents* web_contents);
  void AttachToBrowserTarget();

  void DetachProtocolClient() {
    if (agent_host_) {
      agent_host_->DetachClient(this);
      agent_host_ = nullptr;
    }
  }

  bool HasExistingNotification() const { return !notifications_.empty(); }
  bool HasExistingNotification(const std::string& notification) const;

  base::Value::Dict WaitForNotification(const std::string& notification,
                                        bool allow_existing);

  base::Value::Dict WaitForNotification(const std::string& notification) {
    return WaitForNotification(notification, false);
  }

  // Waits for a notification whose params, when passed to |matcher|, returns
  // true. Existing notifications are allowed.
  base::Value::Dict WaitForMatchingNotification(
      const std::string& notification,
      const NotificationMatcher& matcher);

  void ClearNotifications() { notifications_.clear(); }

  void set_agent_host_can_close() { agent_host_can_close_ = true; }

  void SetAllowUnsafeOperations(bool allow) {
    allow_unsafe_operations_ = allow;
  }

  const base::Value::Dict* result() const;
  const base::Value::Dict* error() const;
  int received_responses_count() const { return received_responses_count_; }

  scoped_refptr<DevToolsAgentHost> agent_host_;

 private:
  void WaitForResponse();
  void RunLoopUpdatingQuitClosure();

  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override;
  void AgentHostClosed(DevToolsAgentHost* agent_host) override;
  bool AllowUnsafeOperations() override;

  int last_sent_id_ = 0;
  int waiting_for_command_result_id_ = 0;
  std::string waiting_for_notification_;
  NotificationMatcher waiting_for_notification_matcher_;

  int received_responses_count_ = 0;
  base::Value::Dict response_;
  base::Value::Dict received_notification_params_;
  std::vector<base::Value::Dict> notifications_;

  bool in_dispatch_ = false;
  bool agent_host_can_close_ = false;
  base::OnceClosure run_loop_quit_closure_;
  bool allow_unsafe_operations_ = true;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_DEVTOOLS_PROTOCOL_CLIENT_H_
