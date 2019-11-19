// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_TEST_SUPPORT_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_TEST_SUPPORT_H_

#include <memory>
#include <string>
#include "base/callback.h"
#include "base/values.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/content_browser_test.h"
#include "net/test/cert_test_util.h"

namespace content {

class DevToolsProtocolTest : public ContentBrowserTest,
                             public DevToolsAgentHostClient,
                             public WebContentsDelegate {
 public:
  typedef base::RepeatingCallback<bool(base::DictionaryValue*)>
      NotificationMatcher;

  DevToolsProtocolTest();
  ~DevToolsProtocolTest() override;

  void SetUpOnMainThread() override;

 protected:
  // WebContentsDelegate methods:
  bool DidAddMessageToConsole(WebContents* source,
                              blink::mojom::ConsoleMessageLevel log_level,
                              const base::string16& message,
                              int32_t line_no,
                              const base::string16& source_id) override;

  blink::SecurityStyle GetSecurityStyle(
      content::WebContents* web_contents,
      content::SecurityStyleExplanations* security_style_explanations) override;

  base::DictionaryValue* SendCommand(const std::string& method,
                                     std::unique_ptr<base::Value> params) {
    return SendCommand(method, std::move(params), true);
  }

  base::DictionaryValue* SendCommand(const std::string& method,
                                     std::unique_ptr<base::Value> params,
                                     bool wait);

  void WaitForResponse();

  bool HasValue(const std::string& path);

  bool HasListItem(const std::string& path_to_list,
                   const std::string& name,
                   const std::string& value);

  void Attach();

  void AttachToBrowserTarget();

  void Detach() {
    if (agent_host_) {
      agent_host_->DetachClient(this);
      agent_host_ = nullptr;
    }
  }

  void TearDownOnMainThread() override;

  std::unique_ptr<base::DictionaryValue> WaitForNotification(
      const std::string& notification) {
    return WaitForNotification(notification, false);
  }

  std::unique_ptr<base::DictionaryValue> WaitForNotification(
      const std::string& notification,
      bool allow_existing);

  // Waits for a notification whose params, when passed to |matcher|, returns
  // true. Existing notifications are allowed.
  std::unique_ptr<base::DictionaryValue> WaitForMatchingNotification(
      const std::string& notification,
      const NotificationMatcher& matcher);

  void ClearNotifications() {
    notifications_.clear();
    notification_params_.clear();
  }

  struct ExpectedNavigation {
    std::string url;
    bool is_redirect;
    bool abort;
  };

  std::string RemovePort(const GURL& url) {
    GURL::Replacements remove_port;
    remove_port.ClearPort();
    return url.ReplaceComponents(remove_port).spec();
  }

  // Waits for the expected navigations to occur in any order. If an expected
  // navigation occurs, Network.continueInterceptedRequest is called with the
  // specified navigation_response to either allow it to proceed or to cancel
  // it.
  void ProcessNavigationsAnyOrder(
      std::vector<ExpectedNavigation> expected_navigations);

  std::vector<std::string> GetAllFrameUrls();

  void set_agent_host_can_close() { agent_host_can_close_ = true; }

  void SetSecurityExplanationCert(
      const scoped_refptr<net::X509Certificate>& cert) {
    cert_ = cert;
  }

  std::unique_ptr<base::DictionaryValue> result_;
  scoped_refptr<DevToolsAgentHost> agent_host_;
  int last_sent_id_;
  std::vector<int> result_ids_;
  std::vector<std::string> notifications_;
  std::vector<std::string> console_messages_;
  std::vector<std::unique_ptr<base::DictionaryValue>> notification_params_;

 private:
  void RunLoopUpdatingQuitClosure();
  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               const std::string& message) override;

  void AgentHostClosed(DevToolsAgentHost* agent_host) override;

  std::string waiting_for_notification_;
  NotificationMatcher waiting_for_notification_matcher_;
  std::unique_ptr<base::DictionaryValue> waiting_for_notification_params_;
  int waiting_for_command_result_id_;
  bool in_dispatch_;
  bool agent_host_can_close_;
  scoped_refptr<net::X509Certificate> cert_;
  base::OnceClosure run_loop_quit_closure_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_TEST_SUPPORT_H_
