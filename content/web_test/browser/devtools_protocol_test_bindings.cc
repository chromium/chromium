// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/devtools_protocol_test_bindings.h"

#include <string_view>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/web_test/browser/web_test_control_host.h"
#include "content/web_test/common/web_test_switches.h"
#include "ipc/ipc_channel.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "content/public/browser/devtools_frontend_host.h"
#endif

namespace content {

namespace {
// This constant should be in sync with
// the constant
// kMaxMessageChunkSize in chrome/browser/devtools/devtools_ui_bindings.cc.
constexpr size_t kWebTestMaxMessageChunkSize =
    IPC::Channel::kMaximumMessageSize / 4;
}  // namespace

DevToolsProtocolTestBindings::DevToolsProtocolTestBindings(
    WebContents* devtools,
    std::string log)
    : WebContentsObserver(devtools),
      agent_host_(DevToolsAgentHost::CreateForBrowser(
          nullptr,
          DevToolsAgentHost::CreateServerSocketCallback())) {
  agent_host_->AttachClient(this);
  ParseLog(log);
}

DevToolsProtocolTestBindings::~DevToolsProtocolTestBindings() {
  if (agent_host_) {
    agent_host_->DetachClient(this);
    agent_host_ = nullptr;
  }
}

// static
GURL DevToolsProtocolTestBindings::MapTestURLIfNeeded(const GURL& test_url,
                                                      bool* is_protocol_test) {
  *is_protocol_test = false;
  std::string spec = test_url.spec();
  std::string dir = "/inspector-protocol/";
  size_t pos = spec.find(dir);
  if (pos == std::string::npos)
    return test_url;
  if (spec.rfind(".js") != spec.length() - 3)
    return test_url;
  spec = spec.substr(0, pos + dir.length()) +
         "resources/inspector-protocol-test.html?test=" + spec;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDebugDevTools))
    spec += "&debug=true";
  *is_protocol_test = true;
  return GURL(spec);
}

void DevToolsProtocolTestBindings::ParseLog(std::string_view log) {
  if (log.empty()) {
    return;
  }
  std::vector<std::string> lines = base::SplitStringUsingSubstr(
      log, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const std::string& line : lines) {
    std::optional<base::Value::Dict> item = base::JSONReader::ReadDict(line);
    CHECK(!item->empty());
    log_.push_back(std::move(item.value()));
  }
  log_enabled_ = true;
}

void DevToolsProtocolTestBindings::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  content::RenderFrameHost* frame = navigation_handle->GetRenderFrameHost();
  if (frame->GetParent())
    return;
  frontend_host_ = DevToolsFrontendHost::Create(
      frame,
      base::BindRepeating(&DevToolsProtocolTestBindings::HandleMessageFromTest,
                          base::Unretained(this)));
#endif
}

void DevToolsProtocolTestBindings::WebContentsDestroyed() {
  if (agent_host_) {
    agent_host_->DetachClient(this);
    agent_host_ = nullptr;
  }
}

void DevToolsProtocolTestBindings::HandleMessagesFromLog(
    std::string_view protocol_message_string) {
  std::optional<base::Value::Dict> parsed =
      base::JSONReader::ReadDict(protocol_message_string);
  if (!parsed) {
    return;
  }
  base::Value::Dict protocol_message = std::move(parsed.value());

  CHECK(log_pos_ < log_.size()) << "Test sent commands but the log is empty";
  const base::Value::Dict& top = log_[log_pos_];
  CHECK(protocol_message == top)
      << "Test sent a command that is not the next in the log \n"
      << protocol_message << "\n"
      << top;
  log_pos_++;
  while (log_pos_ < log_.size()) {
    const base::Value::Dict& item = log_[log_pos_];
    // Stop when the next command is encountered in the log.
    if (item.FindString("method") && item.FindInt("id")) {
      break;
    }
    log_pos_++;
    std::optional<std::string> str_message = base::WriteJson(item);
    CHECK(str_message) << "Could not convert log message to JSON";
    std::string param;
    base::EscapeJSONString(str_message.value(), true, &param);
    std::string javascript = "DevToolsAPI.dispatchMessage(" + param + ");";
    web_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        base::UTF8ToUTF16(javascript), base::NullCallback(),
        ISOLATED_WORLD_ID_GLOBAL);
  }
}

void DevToolsProtocolTestBindings::HandleMessageFromTest(
    base::Value::Dict message) {
  const std::string* method = message.FindString("method");
  if (!method)
    return;

  const base::Value::List* params = message.FindList("params");
  if (*method == "dispatchProtocolMessage" && params && params->size() == 1) {
    const std::string* protocol_message = (*params)[0].GetIfString();
    if (!protocol_message)
      return;

    if (log_enabled_) {
      HandleMessagesFromLog(*protocol_message);
      return;
    }

    if (agent_host_) {
      WebTestControlHost::Get()->PrintMessageToStderr(
          "Protocol message: " + *protocol_message + "\n");
      agent_host_->DispatchProtocolMessage(
          this, base::as_bytes(base::make_span(*protocol_message)));
    }
    return;
  }
}

void DevToolsProtocolTestBindings::DispatchProtocolMessage(
    DevToolsAgentHost* agent_host,
    base::span<const uint8_t> message) {
  if (log_enabled_) {
    NOTREACHED() << "Unexpected messages dispatched by the browser";
  }
  std::string_view str_message(reinterpret_cast<const char*>(message.data()),
                                message.size());
  WebTestControlHost::Get()->PrintMessageToStderr(
      "Protocol message: " + std::string(str_message) + "\n");

  if (str_message.size() < kWebTestMaxMessageChunkSize) {
    std::string param;
    base::EscapeJSONString(str_message, true, &param);
    std::string code = "DevToolsAPI.dispatchMessage(" + param + ");";
    std::u16string javascript = base::UTF8ToUTF16(code);
    web_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        javascript, base::NullCallback(), ISOLATED_WORLD_ID_GLOBAL);
    return;
  }

  size_t total_size = str_message.length();
  for (size_t pos = 0; pos < str_message.length();
       pos += kWebTestMaxMessageChunkSize) {
    std::string param;
    base::EscapeJSONString(str_message.substr(pos, kWebTestMaxMessageChunkSize),
                           true, &param);
    std::string code = "DevToolsAPI.dispatchMessageChunk(" + param + "," +
                       base::NumberToString(pos ? 0 : total_size) + ");";
    std::u16string javascript = base::UTF8ToUTF16(code);
    web_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        javascript, base::NullCallback(), ISOLATED_WORLD_ID_GLOBAL);
  }
}

void DevToolsProtocolTestBindings::AgentHostClosed(
    DevToolsAgentHost* agent_host) {
  agent_host_ = nullptr;
}

bool DevToolsProtocolTestBindings::AllowUnsafeOperations() {
  return true;
}

}  // namespace content
