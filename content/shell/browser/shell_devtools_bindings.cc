// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_devtools_bindings.h"

#include <stddef.h>

#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "base/values.h"
#include "build/build_config.h"
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
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "base/command_line.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/shell/common/shell_switches.h"
#endif

namespace content {

namespace {

std::vector<ShellDevToolsBindings*>* GetShellDevtoolsBindingsInstances() {
  static base::NoDestructor<std::vector<ShellDevToolsBindings*>> instance;
  return instance.get();
}

base::Value::Dict BuildObjectForResponse(const net::HttpResponseHeaders* rh,
                                         bool success,
                                         int net_error) {
  base::Value::Dict response;
  int responseCode = 200;
  if (rh) {
    responseCode = rh->response_code();
  } else if (!success) {
    // In case of no headers, assume file:// URL and failed to load
    responseCode = 404;
  }
  response.Set("statusCode", responseCode);
  response.Set("netError", net_error);
  response.Set("netErrorName", net::ErrorToString(net_error));

  base::Value::Dict headers;
  size_t iterator = 0;
  std::string name;
  std::string value;
  // TODO(caseq): this probably needs to handle duplicate header names
  // correctly by folding them.
  while (rh && rh->EnumerateHeaderLines(&iterator, &name, &value))
    headers.Set(name, value);

  response.Set("headers", std::move(headers));
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

  NetworkResourceLoader(const NetworkResourceLoader&) = delete;
  NetworkResourceLoader& operator=(const NetworkResourceLoader&) = delete;

 private:
  void OnResponseStarted(const GURL& final_url,
                         const network::mojom::URLResponseHead& response_head) {
    response_headers_ = response_head.headers;
  }

  void OnDataReceived(std::string_view chunk,
                      base::OnceClosure resume) override {
    base::Value chunkValue;

    bool encoded = !base::IsStringUTF8(chunk);
    if (encoded) {
      chunkValue = base::Value(base::Base64Encode(chunk));
    } else {
      chunkValue = base::Value(chunk);
    }
    base::Value id(stream_id_);
    base::Value encodedValue(encoded);

    bindings_->CallClientFunction("DevToolsAPI", "streamWrite", std::move(id),
                                  std::move(chunkValue),
                                  std::move(encodedValue));
    std::move(resume).Run();
  }

  void OnComplete(bool success) override {
    auto response = BuildObjectForResponse(response_headers_.get(), success,
                                           loader_->NetError());
    bindings_->SendMessageAck(request_id_, std::move(response));
    bindings_->loaders_.erase(bindings_->loaders_.find(this));
  }

  void OnRetry(base::OnceClosure start_retry) override {
    NOTREACHED_IN_MIGRATION();
  }

  const int stream_id_;
  const int request_id_;
  const raw_ptr<ShellDevToolsBindings> bindings_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
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
  std::erase(*bindings, this);
}

void ShellDevToolsBindings::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  content::RenderFrameHost* frame = navigation_handle->GetRenderFrameHost();
  if (navigation_handle->IsInPrimaryMainFrame()) {
    frontend_host_ = DevToolsFrontendHost::Create(
        frame, base::BindRepeating(
                   &ShellDevToolsBindings::HandleMessageFromDevToolsFrontend,
                   base::Unretained(this)));
    return;
  }
  std::string origin =
      navigation_handle->GetURL().DeprecatedGetOriginAsURL().spec();
  auto it = extensions_api_.find(origin);
  if (it == extensions_api_.end())
    return;
  std::string script = base::StringPrintf(
      "%s(\"%s\")", it->second.c_str(),
      base::Uuid::GenerateRandomV4().AsLowercaseString().c_str());
  DevToolsFrontendHost::SetupExtensionsAPI(frame, script);
#endif
}

void ShellDevToolsBindings::AttachInternal() {
  if (agent_host_)
    agent_host_->DetachClient(this);
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const bool create_for_tab = false;
#else
  const bool create_for_tab = true;
#endif
  agent_host_ = create_for_tab
                    ? DevToolsAgentHost::GetOrCreateForTab(inspected_contents_)
                    : DevToolsAgentHost::GetOrCreateFor(inspected_contents_);
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

void ShellDevToolsBindings::WebContentsDestroyed() {
  if (agent_host_) {
    agent_host_->DetachClient(this);
    agent_host_ = nullptr;
  }
}

void ShellDevToolsBindings::HandleMessageFromDevToolsFrontend(
    base::Value::Dict message) {
  const std::string* method = message.FindString("method");
  if (!method)
    return;

  int request_id = message.FindInt("id").value_or(0);
  base::Value::List* params_value = message.FindList("params");

  // Since we've received message by value, we can take the list.
  base::Value::List params;
  if (params_value) {
    params = std::move(*params_value);
  }

  if (*method == "dispatchProtocolMessage" && params.size() == 1) {
    const std::string* protocol_message = params[0].GetIfString();
    if (!agent_host_ || !protocol_message)
      return;
    agent_host_->DispatchProtocolMessage(
        this, base::as_bytes(base::make_span(*protocol_message)));
  } else if (*method == "loadCompleted") {
    CallClientFunction("DevToolsAPI", "setUseSoftMenu", base::Value(true));
  } else if (*method == "loadNetworkResource" && params.size() == 3) {
    // TODO(pfeldman): handle some of the embedder messages in content.
    const std::string* url = params[0].GetIfString();
    const std::string* headers = params[1].GetIfString();
    std::optional<const int> stream_id = params[2].GetIfInt();
    if (!url || !headers || !stream_id.has_value()) {
      return;
    }

    GURL gurl(*url);
    if (!gurl.is_valid()) {
      base::Value::Dict response;
      response.Set("statusCode", 404);
      response.Set("urlValid", false);
      SendMessageAck(request_id, std::move(response));
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
    resource_request->site_for_cookies = net::SiteForCookies::FromUrl(gurl);
    resource_request->headers.AddHeadersFromString(*headers);

    auto* partition =
        inspected_contents()->GetPrimaryMainFrame()->GetStoragePartition();
    auto factory = partition->GetURLLoaderFactoryForBrowserProcess();

    auto simple_url_loader = network::SimpleURLLoader::Create(
        std::move(resource_request), traffic_annotation);
    auto resource_loader = std::make_unique<NetworkResourceLoader>(
        *stream_id, request_id, this, std::move(simple_url_loader),
        factory.get());
    loaders_.insert(std::move(resource_loader));
    return;
  } else if (*method == "getPreferences") {
    SendMessageAck(request_id, std::move(preferences_));
    return;
  } else if (*method == "getHostConfig") {
    SendMessageAck(request_id, {});
    return;
  } else if (*method == "setPreference") {
    if (params.size() < 2)
      return;
    const std::string* name = params[0].GetIfString();

    // We're just setting params[1] as a value anyways, so just make sure it's
    // the type we want, but don't worry about getting it.
    if (!name || !params[1].is_string())
      return;

    preferences_.Set(*name, std::move(params[1]));
  } else if (*method == "removePreference") {
    const std::string* name = params[0].GetIfString();
    if (!name)
      return;
    preferences_.Remove(*name);
  } else if (*method == "requestFileSystems") {
    CallClientFunction("DevToolsAPI", "fileSystemsLoaded",
                       base::Value(base::Value::Type::LIST));
  } else if (*method == "reattach") {
    if (!agent_host_)
      return;
    agent_host_->DetachClient(this);
    agent_host_->AttachClient(this);
  } else if (*method == "registerExtensionsAPI") {
    if (params.size() < 2)
      return;
    const std::string* origin = params[0].GetIfString();
    const std::string* script = params[1].GetIfString();
    if (!origin || !script)
      return;
    extensions_api_[*origin + "/"] = *script;
  } else {
    return;
  }

  if (request_id)
    SendMessageAck(request_id, {});
}

void ShellDevToolsBindings::DispatchProtocolMessage(
    DevToolsAgentHost* agent_host,
    base::span<const uint8_t> message) {
  std::string_view str_message(reinterpret_cast<const char*>(message.data()),
                               message.size());
  if (str_message.length() < kShellMaxMessageChunkSize) {
    CallClientFunction("DevToolsAPI", "dispatchMessage",
                       base::Value(std::string(str_message)));
  } else {
    size_t total_size = str_message.length();
    for (size_t pos = 0; pos < str_message.length();
         pos += kShellMaxMessageChunkSize) {
      std::string_view str_message_chunk =
          str_message.substr(pos, kShellMaxMessageChunkSize);

      CallClientFunction(
          "DevToolsAPI", "dispatchMessageChunk",
          base::Value(std::string(str_message_chunk)),
          base::Value(base::NumberToString(pos ? 0 : total_size)));
    }
  }
}

void ShellDevToolsBindings::CallClientFunction(
    const std::string& object_name,
    const std::string& method_name,
    base::Value arg1,
    base::Value arg2,
    base::Value arg3,
    base::OnceCallback<void(base::Value)> cb) {
  std::string javascript;

  web_contents()->GetPrimaryMainFrame()->AllowInjectingJavaScript();

  base::Value::List arguments;
  if (!arg1.is_none()) {
    arguments.Append(std::move(arg1));
    if (!arg2.is_none()) {
      arguments.Append(std::move(arg2));
      if (!arg3.is_none()) {
        arguments.Append(std::move(arg3));
      }
    }
  }
  web_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptMethod(
      base::ASCIIToUTF16(object_name), base::ASCIIToUTF16(method_name),
      std::move(arguments), std::move(cb));
}

void ShellDevToolsBindings::SendMessageAck(int request_id,
                                           base::Value::Dict arg) {
  CallClientFunction("DevToolsAPI", "embedderMessageAck",
                     base::Value(request_id), base::Value(std::move(arg)));
}

void ShellDevToolsBindings::AgentHostClosed(DevToolsAgentHost* agent_host) {
  agent_host_ = nullptr;
  if (delegate_)
    delegate_->Close();
}

}  // namespace content
