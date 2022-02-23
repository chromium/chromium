// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/devtools_protocol_test_support.h"

#include <memory>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/mojom/loader/mixed_content.mojom.h"

namespace content {

namespace {

const char kIdParam[] = "id";
const char kSessionIdParam[] = "sessionId";
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
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id) {
  console_messages_.push_back(base::UTF16ToUTF8(message));
  return true;
}

base::DictionaryValue* DevToolsProtocolTest::SendSessionCommand(
    const std::string& method,
    std::unique_ptr<base::Value> params,
    const std::string& session_id,
    bool wait) {
  in_dispatch_ = true;
  base::DictionaryValue command;
  command.SetInteger(kIdParam, ++last_sent_id_);
  command.SetString(kMethodParam, method);
  if (params)
    command.SetKey(kParamsParam,
                   base::Value::FromUniquePtrValue(std::move(params)));
  if (!session_id.empty())
    command.SetString(kSessionIdParam, session_id);

  std::string json_command;
  base::JSONWriter::Write(command, &json_command);
  agent_host_->DispatchProtocolMessage(
      this, base::as_bytes(base::make_span(json_command)));
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

  for (const base::Value& item_value : list->GetListDeprecated()) {
    if (!item_value.is_dict())
      return false;
    const base::DictionaryValue& item =
        base::Value::AsDictionaryValue(item_value);
    std::string id;
    if (!item.GetString(name, &id))
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

bool DevToolsProtocolTest::HasExistingNotification(
    const std::string& search) const {
  for (const std::string& notification : notifications_) {
    if (notification == search)
      return true;
  }
  return false;
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
  while (!expected_navigations.empty()) {
    std::unique_ptr<base::DictionaryValue> params =
        WaitForNotification("Network.requestIntercepted");

    const std::string* interception_id =
        params->FindStringKey("interceptionId");
    ASSERT_TRUE(interception_id);
    bool is_redirect = params->FindKey("redirectUrl");
    absl::optional<bool> is_navigation =
        params->FindBoolKey("isNavigationRequest");
    ASSERT_TRUE(is_navigation);
    const std::string* resource_type = params->FindStringKey("resourceType");
    ASSERT_TRUE(resource_type);

    const std::string* url_in = params->FindStringPath("request.url");
    ASSERT_TRUE(url_in);
    if (is_redirect) {
      url_in = params->FindStringKey("redirectUrl");
      ASSERT_TRUE(url_in);
    }
    // The url will typically have a random port which we want to remove.
    const std::string url = RemovePort(GURL(*url_in));

    if (*is_navigation) {
      params = std::make_unique<base::DictionaryValue>();
      params->SetStringKey("interceptionId", *interception_id);
      SendCommand("Network.continueInterceptedRequest", std::move(params),
                  false);
      continue;
    }

    bool navigation_was_expected = false;
    for (auto it = expected_navigations.begin();
         it != expected_navigations.end(); it++) {
      if (url != it->url || is_redirect != it->is_redirect)
        continue;

      params = std::make_unique<base::DictionaryValue>();
      params->SetStringKey("interceptionId", *interception_id);
      if (it->abort)
        params->SetStringKey("errorReason", "Aborted");
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
    base::span<const uint8_t> message) {
  base::StringPiece message_str(reinterpret_cast<const char*>(message.data()),
                                message.size());
  auto root = base::DictionaryValue::From(
      base::JSONReader::ReadDeprecated(message_str));
  absl::optional<int> id = root->FindIntKey("id");
  if (id) {
    result_ids_.push_back(*id);
    base::DictionaryValue* result;
    bool have_result = root->GetDictionary("result", &result);
    result_.reset(have_result ? result->DeepCopy() : nullptr);
    base::Value* error = root->FindDictKey("error");
    error_ = error ? error->Clone() : base::Value();
    in_dispatch_ = false;
    if (*id && *id == waiting_for_command_result_id_) {
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

void DevToolsProtocolTest::AgentHostClosed(DevToolsAgentHost* agent_host) {
  if (!agent_host_can_close_)
    NOTREACHED();
}

bool DevToolsProtocolTest::AllowUnsafeOperations() {
  return allow_unsafe_operations_;
}

}  // namespace content
