// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/security_style_explanations.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

namespace {

const char kIdParam[] = "id";
const char kMethodParam[] = "method";
const char kParamsParam[] = "params";

}  // namespace

DevToolsProtocolTest::DevToolsProtocolTest()
    : last_sent_id_(0),
      waiting_for_command_result_id_(0),
      in_dispatch_(false),
      agent_host_can_close_(false) {}

DevToolsProtocolTest::~DevToolsProtocolTest() = default;

void DevToolsProtocolTest::SetUpOnMainThread() {
  host_resolver()->AddRule("*", "127.0.0.1");
}

bool DevToolsProtocolTest::DidAddMessageToConsole(
    WebContents* source,
    blink::mojom::ConsoleMessageLevel log_level,
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  console_messages_.push_back(base::UTF16ToUTF8(message));
  return true;
}

base::DictionaryValue* DevToolsProtocolTest::SendCommand(
    const std::string& method,
    std::unique_ptr<base::Value> params,
    bool wait) {
  in_dispatch_ = true;
  base::DictionaryValue command;
  command.SetInteger(kIdParam, ++last_sent_id_);
  command.SetString(kMethodParam, method);
  if (params)
    command.Set(kParamsParam, std::move(params));

  std::string json_command;
  base::JSONWriter::Write(command, &json_command);
  agent_host_->DispatchProtocolMessage(this, json_command);
  // Some messages are dispatched synchronously.
  // Only run loop if we are not finished yet.
  if (in_dispatch_ && wait) {
    WaitForResponse();
    in_dispatch_ = false;
    return result_.get();
  }
  in_dispatch_ = false;
  return result_.get();
}

void DevToolsProtocolTest::WaitForResponse() {
  waiting_for_command_result_id_ = last_sent_id_;
  RunLoopUpdatingQuitClosure();
}

bool DevToolsProtocolTest::HasValue(const std::string& path) {
  base::Value* value = nullptr;
  return result_->Get(path, &value);
}

bool DevToolsProtocolTest::HasListItem(const std::string& path_to_list,
                                       const std::string& name,
                                       const std::string& value) {
  base::ListValue* list;
  if (!result_->GetList(path_to_list, &list))
    return false;

  for (size_t i = 0; i != list->GetSize(); i++) {
    base::DictionaryValue* item;
    if (!list->GetDictionary(i, &item))
      return false;
    std::string id;
    if (!item->GetString(name, &id))
      return false;
    if (id == value)
      return true;
  }
  return false;
}

void DevToolsProtocolTest::Attach() {
  agent_host_ = DevToolsAgentHost::GetOrCreateFor(shell()->web_contents());
  agent_host_->AttachClient(this);
  shell()->web_contents()->SetDelegate(this);
}

void DevToolsProtocolTest::AttachToBrowserTarget() {
  // Tethering domain is not used in tests.
  agent_host_ = DevToolsAgentHost::CreateForBrowser(
      nullptr, DevToolsAgentHost::CreateServerSocketCallback());
  agent_host_->AttachClient(this);
  shell()->web_contents()->SetDelegate(this);
}

void DevToolsProtocolTest::TearDownOnMainThread() {
  Detach();
}

std::unique_ptr<base::DictionaryValue>
DevToolsProtocolTest::WaitForNotification(const std::string& notification,
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

blink::SecurityStyle DevToolsProtocolTest::GetSecurityStyle(
    content::WebContents* web_contents,
    content::SecurityStyleExplanations* security_style_explanations) {
  security_style_explanations->secure_explanations.push_back(
      SecurityStyleExplanation(
          "an explanation title", "an explanation summary",
          "an explanation description", cert_,
          blink::WebMixedContentContextType::kNotMixedContent));
  return blink::SecurityStyle::kNeutral;
}

std::unique_ptr<base::DictionaryValue>
DevToolsProtocolTest::WaitForMatchingNotification(
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

void DevToolsProtocolTest::ProcessNavigationsAnyOrder(
    std::vector<ExpectedNavigation> expected_navigations) {
  std::unique_ptr<base::DictionaryValue> params;
  while (!expected_navigations.empty()) {
    std::unique_ptr<base::DictionaryValue> params =
        WaitForNotification("Network.requestIntercepted");

    std::string interception_id;
    ASSERT_TRUE(params->GetString("interceptionId", &interception_id));
    bool is_redirect = params->HasKey("redirectUrl");
    bool is_navigation;
    ASSERT_TRUE(params->GetBoolean("isNavigationRequest", &is_navigation));
    std::string resource_type;
    ASSERT_TRUE(params->GetString("resourceType", &resource_type));
    std::string url;
    ASSERT_TRUE(params->GetString("request.url", &url));
    if (is_redirect)
      ASSERT_TRUE(params->GetString("redirectUrl", &url));
    // The url will typically have a random port which we want to remove.
    url = RemovePort(GURL(url));

    if (!is_navigation) {
      params.reset(new base::DictionaryValue());
      params->SetString("interceptionId", interception_id);
      SendCommand("Network.continueInterceptedRequest", std::move(params),
                  false);
      continue;
    }

    bool navigation_was_expected = false;
    for (auto it = expected_navigations.begin();
         it != expected_navigations.end(); it++) {
      if (url != it->url || is_redirect != it->is_redirect)
        continue;

      params.reset(new base::DictionaryValue());
      params->SetString("interceptionId", interception_id);
      if (it->abort)
        params->SetString("errorReason", "Aborted");
      SendCommand("Network.continueInterceptedRequest", std::move(params),
                  false);

      navigation_was_expected = true;
      expected_navigations.erase(it);
      break;
    }
    EXPECT_TRUE(navigation_was_expected)
        << "url = " << url << "is_redirect = " << is_redirect;
  }
}

void DevToolsProtocolTest::RunLoopUpdatingQuitClosure() {
  base::RunLoop run_loop;
  run_loop_quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

void DevToolsProtocolTest::DispatchProtocolMessage(
    DevToolsAgentHost* agent_host,
    const std::string& message) {
  std::unique_ptr<base::DictionaryValue> root(
      static_cast<base::DictionaryValue*>(
          base::JSONReader::ReadDeprecated(message).release()));
  int id;
  if (root->GetInteger("id", &id)) {
    result_ids_.push_back(id);
    base::DictionaryValue* result;
    ASSERT_TRUE(root->GetDictionary("result", &result));
    result_.reset(result->DeepCopy());
    in_dispatch_ = false;
    if (id && id == waiting_for_command_result_id_) {
      waiting_for_command_result_id_ = 0;
      std::move(run_loop_quit_closure_).Run();
    }
  } else {
    std::string notification;
    EXPECT_TRUE(root->GetString("method", &notification));
    notifications_.push_back(notification);
    base::DictionaryValue* params;
    if (root->GetDictionary("params", &params)) {
      notification_params_.push_back(params->CreateDeepCopy());
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

std::vector<std::string> DevToolsProtocolTest::GetAllFrameUrls() {
  std::vector<std::string> urls;
  for (RenderFrameHost* render_frame_host :
       shell()->web_contents()->GetAllFrames()) {
    urls.push_back(RemovePort(render_frame_host->GetLastCommittedURL()));
  }
  return urls;
}

void DevToolsProtocolTest::AgentHostClosed(DevToolsAgentHost* agent_host) {
  if (!agent_host_can_close_)
    NOTREACHED();
}

}  // namespace content
