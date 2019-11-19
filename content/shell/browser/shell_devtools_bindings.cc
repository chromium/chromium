// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_devtools_bindings.h"

#include <stddef.h>

#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_browser_main_parts.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_devtools_manager_delegate.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

#if !defined(OS_ANDROID)
#include "content/public/browser/devtools_frontend_host.h"
#endif

namespace content {

namespace {

std::vector<ShellDevToolsBindings*>* GetShellDevtoolsBindingsInstances() {
  static base::NoDestructor<std::vector<ShellDevToolsBindings*>> instance;
  return instance.get();
}

std::unique_ptr<base::DictionaryValue> BuildObjectForResponse(
    const net::HttpResponseHeaders* rh,
    bool success) {
  auto response = std::make_unique<base::DictionaryValue>();
  int responseCode = 200;
  if (rh) {
    responseCode = rh->response_code();
  } else if (!success) {
    // In case of no headers, assume file:// URL and failed to load
    responseCode = 404;
  }
  response->SetInteger("statusCode", responseCode);

  auto headers = std::make_unique<base::DictionaryValue>();
  size_t iterator = 0;
  std::string name;
  std::string value;
  // TODO(caseq): this probably needs to handle duplicate header names
  // correctly by folding them.
  while (rh && rh->EnumerateHeaderLines(&iterator, &name, &value))
    headers->SetString(name, value);

  response->Set("headers", std::move(headers));
  return response;
}

}  // namespace

class ShellDevToolsBindings::NetworkResourceLoader
    : public network::SimpleURLLoaderStreamConsumer {
 public:
  NetworkResourceLoader(int stream_id,
                        int request_id,
                        ShellDevToolsBindings* bindings,
                        std::unique_ptr<network::SimpleURLLoader> loader,
                        network::mojom::URLLoaderFactory* url_loader_factory)
      : stream_id_(stream_id),
        request_id_(request_id),
        bindings_(bindings),
        loader_(std::move(loader)) {
    loader_->SetOnResponseStartedCallback(base::BindOnce(
        &NetworkResourceLoader::OnResponseStarted, base::Unretained(this)));
    loader_->DownloadAsStream(url_loader_factory, this);
  }

 private:
  void OnResponseStarted(const GURL& final_url,
                         const network::mojom::URLResponseHead& response_head) {
    response_headers_ = response_head.headers;
  }

  void OnDataReceived(base::StringPiece chunk,
                      base::OnceClosure resume) override {
    base::Value chunkValue;

    bool encoded = !base::IsStringUTF8(chunk);
    if (encoded) {
      std::string encoded_string;
      base::Base64Encode(chunk, &encoded_string);
      chunkValue = base::Value(std::move(encoded_string));
    } else {
      chunkValue = base::Value(chunk);
    }
    base::Value id(stream_id_);
    base::Value encodedValue(encoded);

    bindings_->CallClientFunction("DevToolsAPI.streamWrite", &id, &chunkValue,
                                  &encodedValue);
    std::move(resume).Run();
  }

  void OnComplete(bool success) override {
    auto response = BuildObjectForResponse(response_headers_.get(), success);
    bindings_->SendMessageAck(request_id_, response.get());
    bindings_->loaders_.erase(bindings_->loaders_.find(this));
  }

  void OnRetry(base::OnceClosure start_retry) override { NOTREACHED(); }

  const int stream_id_;
  const int request_id_;
  ShellDevToolsBindings* const bindings_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;

  DISALLOW_COPY_AND_ASSIGN(NetworkResourceLoader);
};

// This constant should be in sync with
// the constant
// kMaxMessageChunkSize in chrome/browser/devtools/devtools_ui_bindings.cc.
constexpr size_t kShellMaxMessageChunkSize =
    IPC::Channel::kMaximumMessageSize / 4;

void ShellDevToolsBindings::InspectElementAt(int x, int y) {
  if (agent_host_) {
    agent_host_->InspectElement(inspected_contents_->GetFocusedFrame(), x, y);
  } else {
    inspect_element_at_x_ = x;
    inspect_element_at_y_ = y;
  }
}

ShellDevToolsBindings::ShellDevToolsBindings(WebContents* devtools_contents,
                                             WebContents* inspected_contents,
                                             ShellDevToolsDelegate* delegate)
    : WebContentsObserver(devtools_contents),
      inspected_contents_(inspected_contents),
      delegate_(delegate),
      inspect_element_at_x_(-1),
      inspect_element_at_y_(-1) {
  auto* bindings = GetShellDevtoolsBindingsInstances();
  DCHECK(!base::Contains(*bindings, this));
  bindings->push_back(this);
}

ShellDevToolsBindings::~ShellDevToolsBindings() {
  if (agent_host_)
    agent_host_->DetachClient(this);

  auto* bindings = GetShellDevtoolsBindingsInstances();
  DCHECK(base::Contains(*bindings, this));
  base::Erase(*bindings, this);
}

// static
std::vector<ShellDevToolsBindings*>
ShellDevToolsBindings::GetInstancesForWebContents(WebContents* web_contents) {
  auto* bindings = GetShellDevtoolsBindingsInstances();
  std::vector<ShellDevToolsBindings*> result;
  std::copy_if(bindings->begin(), bindings->end(), std::back_inserter(result),
               [web_contents](ShellDevToolsBindings* binding) {
                 return binding->inspected_contents() == web_contents;
               });
  return result;
}

void ShellDevToolsBindings::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
#if !defined(OS_ANDROID)
  content::RenderFrameHost* frame = navigation_handle->GetRenderFrameHost();
  if (navigation_handle->IsInMainFrame()) {
    frontend_host_ = DevToolsFrontendHost::Create(
        frame, base::BindRepeating(
                   &ShellDevToolsBindings::HandleMessageFromDevToolsFrontend,
                   base::Unretained(this)));
    return;
  }
  std::string origin = navigation_handle->GetURL().GetOrigin().spec();
  auto it = extensions_api_.find(origin);
  if (it == extensions_api_.end())
    return;
  std::string script = base::StringPrintf("%s(\"%s\")", it->second.c_str(),
                                          base::GenerateGUID().c_str());
  DevToolsFrontendHost::SetupExtensionsAPI(frame, script);
#endif
}

void ShellDevToolsBindings::AttachInternal() {
  if (agent_host_)
    agent_host_->DetachClient(this);
  agent_host_ = DevToolsAgentHost::GetOrCreateFor(inspected_contents_);
  agent_host_->AttachClient(this);
  if (inspect_element_at_x_ != -1) {
    agent_host_->InspectElement(inspected_contents_->GetFocusedFrame(),
                                inspect_element_at_x_, inspect_element_at_y_);
    inspect_element_at_x_ = -1;
    inspect_element_at_y_ = -1;
  }
}

void ShellDevToolsBindings::Attach() {
  AttachInternal();
}

void ShellDevToolsBindings::UpdateInspectedWebContents(
    WebContents* new_contents) {
  inspected_contents_ = new_contents;
  if (!agent_host_)
    return;
  AttachInternal();
  CallClientFunction("DevToolsAPI.reattachMainTarget", nullptr, nullptr,
                     nullptr);
}

void ShellDevToolsBindings::WebContentsDestroyed() {
  if (agent_host_) {
    agent_host_->DetachClient(this);
    agent_host_ = nullptr;
  }
}

void ShellDevToolsBindings::HandleMessageFromDevToolsFrontend(
    const std::string& message) {
  std::string method;
  base::ListValue* params = nullptr;
  base::DictionaryValue* dict = nullptr;
  std::unique_ptr<base::Value> parsed_message =
      base::JSONReader::ReadDeprecated(message);
  if (!parsed_message || !parsed_message->GetAsDictionary(&dict) ||
      !dict->GetString("method", &method)) {
    return;
  }
  int request_id = 0;
  dict->GetInteger("id", &request_id);
  dict->GetList("params", &params);

  if (method == "dispatchProtocolMessage" && params && params->GetSize() == 1) {
    std::string protocol_message;
    if (!agent_host_ || !params->GetString(0, &protocol_message))
      return;
    agent_host_->DispatchProtocolMessage(this, protocol_message);
  } else if (method == "loadCompleted") {
    web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16("DevToolsAPI.setUseSoftMenu(true);"),
        base::NullCallback());
  } else if (method == "loadNetworkResource" && params->GetSize() == 3) {
    // TODO(pfeldman): handle some of the embedder messages in content.
    std::string url;
    std::string headers;
    int stream_id;
    if (!params->GetString(0, &url) || !params->GetString(1, &headers) ||
        !params->GetInteger(2, &stream_id)) {
      return;
    }

    GURL gurl(url);
    if (!gurl.is_valid()) {
      base::DictionaryValue response;
      response.SetInteger("statusCode", 404);
      SendMessageAck(request_id, &response);
      return;
    }

    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation(
            "devtools_handle_front_end_messages", R"(
            semantics {
              sender: "Developer Tools"
              description:
                "When user opens Developer Tools, the browser may fetch "
                "additional resources from the network to enrich the debugging "
                "experience (e.g. source map resources)."
              trigger: "User opens Developer Tools to debug a web page."
              data: "Any resources requested by Developer Tools."
              destination: OTHER
            }
            policy {
              cookies_allowed: YES
              cookies_store: "user"
              setting:
                "It's not possible to disable this feature from settings."
              chrome_policy {
                DeveloperToolsAvailability {
                  policy_options {mode: MANDATORY}
                  DeveloperToolsAvailability: 2
                }
              }
            })");

    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = gurl;
    // TODO(caseq): this preserves behavior of URLFetcher-based implementation.
    // We really need to pass proper first party origin from the front-end.
    resource_request->site_for_cookies = gurl;
    resource_request->headers.AddHeadersFromString(headers);

    auto* partition = content::BrowserContext::GetStoragePartitionForSite(
        web_contents()->GetBrowserContext(), gurl);
    auto factory = partition->GetURLLoaderFactoryForBrowserProcess();

    auto simple_url_loader = network::SimpleURLLoader::Create(
        std::move(resource_request), traffic_annotation);
    auto resource_loader = std::make_unique<NetworkResourceLoader>(
        stream_id, request_id, this, std::move(simple_url_loader),
        factory.get());
    loaders_.insert(std::move(resource_loader));
    return;
  } else if (method == "getPreferences") {
    SendMessageAck(request_id, &preferences_);
    return;
  } else if (method == "setPreference") {
    std::string name;
    std::string value;
    if (!params->GetString(0, &name) || !params->GetString(1, &value)) {
      return;
    }
    preferences_.SetKey(name, base::Value(value));
  } else if (method == "removePreference") {
    std::string name;
    if (!params->GetString(0, &name))
      return;
    preferences_.RemoveWithoutPathExpansion(name, nullptr);
  } else if (method == "requestFileSystems") {
    web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16("DevToolsAPI.fileSystemsLoaded([]);"),
        base::NullCallback());
  } else if (method == "reattach") {
    if (!agent_host_)
      return;
    agent_host_->DetachClient(this);
    agent_host_->AttachClient(this);
  } else if (method == "registerExtensionsAPI") {
    std::string origin;
    std::string script;
    if (!params->GetString(0, &origin) || !params->GetString(1, &script))
      return;
    extensions_api_[origin + "/"] = script;
  } else {
    return;
  }

  if (request_id)
    SendMessageAck(request_id, nullptr);
}

void ShellDevToolsBindings::DispatchProtocolMessage(
    DevToolsAgentHost* agent_host,
    const std::string& message) {
  if (message.length() < kShellMaxMessageChunkSize) {
    std::string param;
    base::EscapeJSONString(message, true, &param);
    std::string code = "DevToolsAPI.dispatchMessage(" + param + ");";
    base::string16 javascript = base::UTF8ToUTF16(code);
    web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
        javascript, base::NullCallback());
    return;
  }

  size_t total_size = message.length();
  for (size_t pos = 0; pos < message.length();
       pos += kShellMaxMessageChunkSize) {
    std::string param;
    base::EscapeJSONString(message.substr(pos, kShellMaxMessageChunkSize), true,
                           &param);
    std::string code = "DevToolsAPI.dispatchMessageChunk(" + param + "," +
                       std::to_string(pos ? 0 : total_size) + ");";
    base::string16 javascript = base::UTF8ToUTF16(code);
    web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
        javascript, base::NullCallback());
  }
}

void ShellDevToolsBindings::CallClientFunction(const std::string& function_name,
                                               const base::Value* arg1,
                                               const base::Value* arg2,
                                               const base::Value* arg3) {
  std::string javascript = function_name + "(";
  if (arg1) {
    std::string json;
    base::JSONWriter::Write(*arg1, &json);
    javascript.append(json);
    if (arg2) {
      base::JSONWriter::Write(*arg2, &json);
      javascript.append(", ").append(json);
      if (arg3) {
        base::JSONWriter::Write(*arg3, &json);
        javascript.append(", ").append(json);
      }
    }
  }
  javascript.append(");");
  web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16(javascript), base::NullCallback());
}

void ShellDevToolsBindings::SendMessageAck(int request_id,
                                           const base::Value* arg) {
  base::Value id_value(request_id);
  CallClientFunction("DevToolsAPI.embedderMessageAck", &id_value, arg, nullptr);
}

void ShellDevToolsBindings::AgentHostClosed(DevToolsAgentHost* agent_host) {
  agent_host_ = nullptr;
  if (delegate_)
    delegate_->Close();
}

}  // namespace content
