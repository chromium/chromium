// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/devtools_protocol_test_bindings.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/string_escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/web_test/common/web_test_switches.h"
#include "ipc/ipc_channel.h"

#if !BUILDFLAG(IS_ANDROID)
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
    WebContents* devtools)
    : WebContentsObserver(devtools),
      agent_host_(DevToolsAgentHost::CreateForBrowser(
          nullptr,
          DevToolsAgentHost::CreateServerSocketCallback())) {
  agent_host_->AttachClient(this);
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

void DevToolsProtocolTestBindings::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
#if !BUILDFLAG(IS_ANDROID)
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

    if (agent_host_) {
      agent_host_->DispatchProtocolMessage(
          this, base::as_bytes(base::make_span(*protocol_message)));
    }
    return;
  }
}

void DevToolsProtocolTestBindings::DispatchProtocolMessage(
    DevToolsAgentHost* agent_host,
    base::span<const uint8_t> message) {
  base::StringPiece str_message(reinterpret_cast<const char*>(message.data()),
                                message.size());
  if (str_message.size() < kWebTestMaxMessageChunkSize) {
    std::string param;
    base::EscapeJSONString(str_message, true, &param);
    std::string code = "DevToolsAPI.dispatchMessage(" + param + ");";
    std::u16string javascript = base::UTF8ToUTF16(code);
    web_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        javascript, base::NullCallback());
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
        javascript, base::NullCallback());
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
