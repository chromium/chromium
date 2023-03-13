// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_DEVTOOLS_PROTOCOL_CLIENT_H_
#define CONTENT_PUBLIC_TEST_TEST_DEVTOOLS_PROTOCOL_CLIENT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/values.h"
#include "content/public/browser/devtools_agent_host.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

class TestDevToolsProtocolClient : public DevToolsAgentHostClient {
 public:
  typedef base::RepeatingCallback<bool(const base::Value::Dict&)>
      NotificationMatcher;

  TestDevToolsProtocolClient();
  ~TestDevToolsProtocolClient() override;

 protected:
  const base::Value::Dict* SendCommand(std::string method,
                                       base::Value::Dict params,
                                       bool wait = true) {
    return SendSessionCommand(method, std::move(params), std::string(), wait);
  }

  const base::Value::Dict* SendCommandSync(std::string method) {
    return SendCommand(std::move(method), base::Value::Dict(), true);
  }
  const base::Value::Dict* SendCommandSync(std::string method,
                                           base::Value::Dict params) {
    return SendCommand(std::move(method), std::move(params), true);
  }
  const base::Value::Dict* SendCommandAsync(std::string method) {
    return SendCommand(std::move(method), base::Value::Dict(), false);
  }
  const base::Value::Dict* SendCommandAsync(std::string method,
                                            base::Value::Dict params) {
    return SendCommand(std::move(method), std::move(params), false);
  }

  const base::Value::Dict* SendSessionCommand(const std::string method,
                                              base::Value::Dict params,
                                              const std::string session_id,
                                              bool wait);

  void AttachToWebContents(WebContents* web_contents);
  void AttachToTabTarget(WebContents* web_contents);
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
  void SetIsTrusted(bool is_trusted) { is_trusted_ = is_trusted; }
  void SetNavigationInitiatorOrigin(
      const url::Origin& navigation_initiator_origin) {
    navigation_initiator_origin_ = navigation_initiator_origin;
  }

  void SetMayReadLocalFiles(bool may_read_local_files) {
    may_read_local_files_ = may_read_local_files;
  }

  const base::Value::Dict* result() const;
  const base::Value::Dict* error() const;
  int received_responses_count() const { return received_responses_count_; }

  scoped_refptr<DevToolsAgentHost> agent_host_;

  void SetProtocolCommandId(int id) { last_sent_id_ = id - 1; }

 private:
  void WaitForResponse();
  void RunLoopUpdatingQuitClosure();

  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override;
  void AgentHostClosed(DevToolsAgentHost* agent_host) override;
  absl::optional<url::Origin> GetNavigationInitiatorOrigin() override;
  bool AllowUnsafeOperations() override;
  bool IsTrusted() override;
  bool MayReadLocalFiles() override;

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
  bool is_trusted_ = true;
  absl::optional<url::Origin> navigation_initiator_origin_;
  bool may_read_local_files_ = true;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_DEVTOOLS_PROTOCOL_CLIENT_H_
