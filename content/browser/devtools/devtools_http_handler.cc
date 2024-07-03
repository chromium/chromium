// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/devtools/devtools_http_handler.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread.h"
#include "base/uuid.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"
#include "net/server/http_server_response_info.h"
#include "net/socket/server_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "v8/include/v8-version-string.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
extern const int kCcompressedProtocolJSON;
#endif

namespace content {

namespace {

const base::FilePath::CharType kDevToolsActivePortFileName[] =
    FILE_PATH_LITERAL("DevToolsActivePort");

const char kDevToolsHandlerThreadName[] = "Chrome_DevToolsHandlerThread";

const char kPageUrlPrefix[] = "/devtools/page/";
const char kBrowserUrlPrefix[] = "/devtools/browser";

const char kTargetIdField[] = "id";
const char kTargetParentIdField[] = "parentId";
const char kTargetTypeField[] = "type";
const char kTargetTitleField[] = "title";
const char kTargetDescriptionField[] = "description";
const char kTargetUrlField[] = "url";
const char kTargetFaviconUrlField[] = "faviconUrl";
const char kTargetWebSocketDebuggerUrlField[] = "webSocketDebuggerUrl";
const char kTargetDevtoolsFrontendUrlField[] = "devtoolsFrontendUrl";

const int32_t kSendBufferSizeForDevTools = 256 * 1024 * 1024;  // 256Mb
const int32_t kReceiveBufferSizeForDevTools = 100 * 1024 * 1024;  // 100Mb

const char kRemoteUrlPattern[] =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    "https://chrome-devtools-frontend.appspot.com/serve_internal_file/%s/"
    "%s.html";
#else
    "https://chrome-devtools-frontend.appspot.com/serve_rev/%s/%s.html";
#endif

constexpr net::NetworkTrafficAnnotationTag
    kDevtoolsHttpHandlerTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("devtools_http_handler", R"(
      semantics {
        sender: "Devtools Http Handler"
        description:
          "This is a remote debugging server, only enabled by "
          "'--remote-debugging-port' switch. It exposes debugging protocol "
          "over websockets."
        trigger: "Run with '--remote-debugging-port' switch."
        data: "Debugging data, including any data on the open pages."
        destination: OTHER
        destination_other: "The data can be sent to any destination."
      }
      policy {
        cookies_allowed: NO
        setting:
          "This request cannot be disabled in settings. However it will never "
          "be made if user does not run with '--remote-debugging-port' switch."
        policy_exception_justification:
          "Not implemented, only used in Devtools and is behind a switch."
      })");

bool RequestIsSafeToServe(const net::HttpServerRequestInfo& info) {
  // For browser-originating requests, serve only those that are coming from
  // pages loaded off localhost or fixed IPs.
  std::string header = info.GetHeaderValue("host");
  if (header.empty())
    return true;
  GURL url = GURL("https://" + header);
  return url.HostIsIPAddress() || net::IsLocalHostname(url.host());
}

}  // namespace

// ServerWrapper -------------------------------------------------------------
// All methods in this class are only called on handler thread.
class ServerWrapper : net::HttpServer::Delegate {
 public:
  ServerWrapper(base::WeakPtr<DevToolsHttpHandler> handler,
                std::unique_ptr<net::ServerSocket> socket,
                const base::FilePath& debug_frontend_dir,
                bool bundles_resources);

  int GetLocalAddress(net::IPEndPoint* address);

  void AcceptWebSocket(int connection_id,
                       const net::HttpServerRequestInfo& request);
  void SendOverWebSocket(int connection_id, std::string message);
  void SendResponse(int connection_id,
                    const net::HttpServerResponseInfo& response);
  void Send200(int connection_id,
               const std::string& data,
               const std::string& mime_type);
  void Send404(int connection_id);
  void Send500(int connection_id, const std::string& message);
  void Close(int connection_id);

  ~ServerWrapper() override {}

 private:
  // net::HttpServer::Delegate implementation.
  void OnConnect(int connection_id) override {}
  void OnHttpRequest(int connection_id,
                     const net::HttpServerRequestInfo& info) override;
  void OnWebSocketRequest(int connection_id,
                          const net::HttpServerRequestInfo& info) override;
  void OnWebSocketMessage(int connection_id, std::string data) override;
  void OnClose(int connection_id) override;

  base::WeakPtr<DevToolsHttpHandler> handler_;
  std::unique_ptr<net::HttpServer> server_;
  base::FilePath debug_frontend_dir_;
  bool bundles_resources_;
};

ServerWrapper::ServerWrapper(base::WeakPtr<DevToolsHttpHandler> handler,
                             std::unique_ptr<net::ServerSocket> socket,
                             const base::FilePath& debug_frontend_dir,
                             bool bundles_resources)
    : handler_(handler),
      server_(new net::HttpServer(std::move(socket), this)),
      debug_frontend_dir_(debug_frontend_dir),
      bundles_resources_(bundles_resources) {}

int ServerWrapper::GetLocalAddress(net::IPEndPoint* address) {
  return server_->GetLocalAddress(address);
}

void ServerWrapper::AcceptWebSocket(int connection_id,
                                    const net::HttpServerRequestInfo& request) {
  server_->SetSendBufferSize(connection_id, kSendBufferSizeForDevTools);
  server_->SetReceiveBufferSize(connection_id, kReceiveBufferSizeForDevTools);
  server_->AcceptWebSocket(connection_id, request,
                           kDevtoolsHttpHandlerTrafficAnnotation);
}

void ServerWrapper::SendOverWebSocket(int connection_id, std::string message) {
  server_->SendOverWebSocket(connection_id, std::move(message),
                             kDevtoolsHttpHandlerTrafficAnnotation);
}

void ServerWrapper::SendResponse(int connection_id,
                                 const net::HttpServerResponseInfo& response) {
  server_->SendResponse(connection_id, response,
                        kDevtoolsHttpHandlerTrafficAnnotation);
}

void ServerWrapper::Send200(int connection_id,
                            const std::string& data,
                            const std::string& mime_type) {
  server_->Send200(connection_id, data, mime_type,
                   kDevtoolsHttpHandlerTrafficAnnotation);
}

void ServerWrapper::Send404(int connection_id) {
  server_->Send404(connection_id, kDevtoolsHttpHandlerTrafficAnnotation);
}

void ServerWrapper::Send500(int connection_id,
                            const std::string& message) {
  server_->Send500(connection_id, message,
                   kDevtoolsHttpHandlerTrafficAnnotation);
}

void ServerWrapper::Close(int connection_id) {
  server_->Close(connection_id);
}

// Thread and ServerWrapper lifetime management ------------------------------

void TerminateOnUI(std::unique_ptr<base::Thread> thread,
                   std::unique_ptr<ServerWrapper> server_wrapper,
                   std::unique_ptr<DevToolsSocketFactory> socket_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (server_wrapper)
    thread->task_runner()->DeleteSoon(FROM_HERE, std::move(server_wrapper));
  if (socket_factory)
    thread->task_runner()->DeleteSoon(FROM_HERE, std::move(socket_factory));
  if (thread) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::WithBaseSyncPrimitives(), base::TaskPriority::BEST_EFFORT},
        DoNothingWithBoundArgs(std::move(thread)));
  }
}

void ServerStartedOnUI(base::WeakPtr<DevToolsHttpHandler> handler,
                       base::Thread* thread,
                       ServerWrapper* server_wrapper,
                       DevToolsSocketFactory* socket_factory,
                       std::unique_ptr<net::IPEndPoint> ip_address) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (handler && thread && server_wrapper) {
    handler->ServerStarted(
        std::unique_ptr<base::Thread>(thread),
        std::unique_ptr<ServerWrapper>(server_wrapper),
        std::unique_ptr<DevToolsSocketFactory>(socket_factory),
        std::move(ip_address));
  } else {
    TerminateOnUI(std::unique_ptr<base::Thread>(thread),
                  std::unique_ptr<ServerWrapper>(server_wrapper),
                  std::unique_ptr<DevToolsSocketFactory>(socket_factory));
  }
}

void StartServerOnHandlerThread(
    base::WeakPtr<DevToolsHttpHandler> handler,
    std::unique_ptr<base::Thread> thread,
    std::unique_ptr<DevToolsSocketFactory> socket_factory,
    const base::FilePath& output_directory,
    const base::FilePath& debug_frontend_dir,
    const std::string& browser_guid,
    bool bundles_resources) {
  DCHECK(thread->task_runner()->BelongsToCurrentThread());
  std::unique_ptr<ServerWrapper> server_wrapper;
  std::unique_ptr<net::ServerSocket> server_socket =
      socket_factory->CreateForHttpServer();
  std::unique_ptr<net::IPEndPoint> ip_address(new net::IPEndPoint);
  if (server_socket) {
    server_wrapper =
        std::make_unique<ServerWrapper>(handler, std::move(server_socket),
                                        debug_frontend_dir, bundles_resources);
    if (server_wrapper->GetLocalAddress(ip_address.get()) != net::OK)
      ip_address.reset();
  } else {
    ip_address.reset();
  }

  if (ip_address) {
    std::string message = base::StringPrintf(
        "\nDevTools listening on ws://%s%s\n", ip_address->ToString().c_str(),
        browser_guid.c_str());
    fprintf(stderr, "%s", message.c_str());
    fflush(stderr);

    // Write this port to a well-known file in the profile directory
    // so Telemetry, ChromeDriver, etc. can pick it up.
    if (!output_directory.empty()) {
      base::FilePath path =
          output_directory.Append(kDevToolsActivePortFileName);
      std::string port_target_string = base::StringPrintf(
          "%d\n%s", ip_address->port(), browser_guid.c_str());
      if (!base::WriteFile(path, port_target_string)) {
        PLOG(ERROR) << "Error writing DevTools active port to file " << path;
      }
    }
  } else {
#if !BUILDFLAG(IS_ANDROID)
    // Android uses UNIX domain sockets which don't have an IP address.
    LOG(ERROR) << "Cannot start http server for devtools.";
#endif
  }

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ServerStartedOnUI, std::move(handler), thread.release(),
                     server_wrapper.release(), socket_factory.release(),
                     std::move(ip_address)));
}

// DevToolsAgentHostClientImpl -----------------------------------------------
// An internal implementation of DevToolsAgentHostClient that delegates
// messages sent to a DebuggerShell instance.
class DevToolsAgentHostClientImpl : public DevToolsAgentHostClient {
 public:
  DevToolsAgentHostClientImpl(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      ServerWrapper* server_wrapper,
      int connection_id,
      scoped_refptr<DevToolsAgentHost> agent_host)
      : task_runner_(std::move(task_runner)),
        server_wrapper_(server_wrapper),
        connection_id_(connection_id),
        agent_host_(agent_host) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // TODO(dgozman): handle return value of AttachClient.
    agent_host_->AttachClient(this);
  }

  ~DevToolsAgentHostClientImpl() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (agent_host_)
      agent_host_->DetachClient(this);
  }

  std::string GetTypeForMetrics() override { return "RemoteDebugger"; }

  void AgentHostClosed(DevToolsAgentHost* agent_host) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(agent_host == agent_host_.get());

    constexpr char kMsg[] =
        "{\"method\":\"Inspector.detached\","
        "\"params\":{\"reason\":\"target_closed\"}}";
    DispatchProtocolMessage(
        agent_host, base::as_bytes(base::make_span(kMsg, strlen(kMsg))));

    agent_host_ = nullptr;
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServerWrapper::Close, base::Unretained(server_wrapper_),
                       connection_id_));
  }

  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(agent_host == agent_host_.get());
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServerWrapper::SendOverWebSocket,
                       base::Unretained(server_wrapper_), connection_id_,
                       std::string(message.begin(), message.end())));
  }

  void OnMessage(base::span<const uint8_t> message) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (agent_host_)
      agent_host_->DispatchProtocolMessage(this, message);
  }

 private:
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const raw_ptr<ServerWrapper> server_wrapper_;
  const int connection_id_;
  scoped_refptr<DevToolsAgentHost> agent_host_;
};

static bool TimeComparator(scoped_refptr<DevToolsAgentHost> host1,
                           scoped_refptr<DevToolsAgentHost> host2) {
  return host1->GetLastActivityTime() > host2->GetLastActivityTime();
}

// DevToolsHttpHandler -------------------------------------------------------

DevToolsHttpHandler::~DevToolsHttpHandler() {
  // Disconnecting sessions might lead to the last minute messages generated
  // by the targets. It is essential that this happens before we issue delete
  // soon for the server wrapper.
  connection_to_client_.clear();
  TerminateOnUI(std::move(thread_), std::move(server_wrapper_),
                std::move(socket_factory_));
}

static std::string PathWithoutParams(const std::string& path) {
  size_t query_position = path.find('?');
  if (query_position != std::string::npos)
    return path.substr(0, query_position);
  return path;
}

static std::string GetMimeType(const std::string& filename) {
  if (base::EndsWith(filename, ".html", base::CompareCase::INSENSITIVE_ASCII)) {
    return "text/html";
  } else if (base::EndsWith(filename, ".css",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    return "text/css";
  } else if (base::EndsWith(filename, ".js",
                            base::CompareCase::INSENSITIVE_ASCII) ||
             base::EndsWith(filename, ".mjs",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    return "text/javascript";
  } else if (base::EndsWith(filename, ".png",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    return "image/png";
  } else if (base::EndsWith(filename, ".gif",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    return "image/gif";
  } else if (base::EndsWith(filename, ".json",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    return "application/json";
  } else if (base::EndsWith(filename, ".svg",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    return "image/svg+xml";
  } else if (base::EndsWith(filename, ".avif",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    return "image/avif";
  }
  LOG(ERROR) << "GetMimeType doesn't know mime type for: "
             << filename
             << " text/plain will be returned";
  return "text/plain";
}

void ServerWrapper::OnHttpRequest(int connection_id,
                                  const net::HttpServerRequestInfo& info) {
  if (!RequestIsSafeToServe(info)) {
    Send500(connection_id,
            "Host header is specified and is not an IP address or localhost.");
    return;
  }

  server_->SetSendBufferSize(connection_id, kSendBufferSizeForDevTools);

  if (base::StartsWith(info.path, "/json", base::CompareCase::SENSITIVE)) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&DevToolsHttpHandler::OnJsonRequest, handler_,
                                  connection_id, info));
    return;
  }

  if (info.path.empty() || info.path == "/") {
    // Discovery page request.
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&DevToolsHttpHandler::OnDiscoveryPageRequest,
                                  handler_, connection_id));
    return;
  }

  if (!base::StartsWith(info.path, "/devtools/",
                        base::CompareCase::SENSITIVE)) {
    server_->Send404(connection_id, kDevtoolsHttpHandlerTrafficAnnotation);
    return;
  }

  std::string filename = PathWithoutParams(info.path.substr(10));
  std::string mime_type = GetMimeType(filename);

  if (!debug_frontend_dir_.empty()) {
    base::FilePath path = debug_frontend_dir_.AppendASCII(filename);
    std::string data;
    base::ReadFileToString(path, &data);
    server_->Send200(connection_id, data, mime_type,
                     kDevtoolsHttpHandlerTrafficAnnotation);
    return;
  }

  if (bundles_resources_) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&DevToolsHttpHandler::OnFrontendResourceRequest,
                       handler_, connection_id, filename));
    return;
  }
  server_->Send404(connection_id, kDevtoolsHttpHandlerTrafficAnnotation);
}

void ServerWrapper::OnWebSocketRequest(
    int connection_id,
    const net::HttpServerRequestInfo& request) {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&DevToolsHttpHandler::OnWebSocketRequest,
                                handler_, connection_id, request));
}

void ServerWrapper::OnWebSocketMessage(int connection_id, std::string data) {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&DevToolsHttpHandler::OnWebSocketMessage,
                                handler_, connection_id, std::move(data)));
}

void ServerWrapper::OnClose(int connection_id) {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&DevToolsHttpHandler::OnClose, handler_, connection_id));
}

std::string DevToolsHttpHandler::GetFrontendURLInternal(
    scoped_refptr<DevToolsAgentHost> agent_host,
    const std::string& id,
    const std::string& host) {
  std::string frontend_url;
  if (delegate_->HasBundledFrontendResources()) {
    frontend_url = "/devtools/inspector.html";
  } else {
    std::string type = agent_host->GetType();
    bool is_worker = type == DevToolsAgentHost::kTypeServiceWorker ||
                     type == DevToolsAgentHost::kTypeSharedWorker;
    frontend_url =
        base::StringPrintf(kRemoteUrlPattern, GetChromiumGitRevision().c_str(),
                           is_worker ? "worker_app" : "inspector");
  }
  return base::StringPrintf("%s?ws=%s%s%s", frontend_url.c_str(), host.c_str(),
                            kPageUrlPrefix, id.c_str());
}

static bool ParseJsonPath(
    const std::string& path,
    std::string* command,
    std::string* target_id) {

  // Fall back to list in case of empty query.
  if (path.empty()) {
    *command = "list";
    return true;
  }

  if (!base::StartsWith(path, "/", base::CompareCase::SENSITIVE)) {
    // Malformed command.
    return false;
  }
  *command = path.substr(1);

  size_t separator_pos = command->find("/");
  if (separator_pos != std::string::npos) {
    *target_id = command->substr(separator_pos + 1);
    *command = command->substr(0, separator_pos);
  }
  return true;
}

void DevToolsHttpHandler::OnJsonRequest(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  // Trim /json
  std::string path = info.path.substr(5);

  // Trim fragment and query
  std::string query;
  size_t query_pos = path.find('?');
  if (query_pos != std::string::npos) {
    query = path.substr(query_pos + 1);
    path = path.substr(0, query_pos);
  }

  size_t fragment_pos = path.find('#');
  if (fragment_pos != std::string::npos)
    path = path.substr(0, fragment_pos);

  std::string command;
  std::string target_id;
  if (!ParseJsonPath(path, &command, &target_id)) {
    SendJson(connection_id, net::HTTP_NOT_FOUND, std::nullopt,
             "Malformed query: " + info.path);
    return;
  }

  if (command == "version") {
    base::Value::Dict version;
    version.Set("Protocol-Version", DevToolsAgentHost::GetProtocolVersion());
    version.Set("WebKit-Version", GetWebKitVersion());
    version.Set("Browser", GetContentClient()->browser()->GetProduct());
    version.Set("User-Agent", GetContentClient()->browser()->GetUserAgent());
    version.Set("V8-Version", V8_VERSION_STRING);
    std::string host = info.GetHeaderValue("host");
    version.Set(
        kTargetWebSocketDebuggerUrlField,
        base::StringPrintf("ws://%s%s", host.c_str(), browser_guid_.c_str()));
#if BUILDFLAG(IS_ANDROID)
    version.Set("Android-Package",
                base::android::BuildInfo::GetInstance()->host_package_name());
#endif
    SendJson(connection_id, net::HTTP_OK, version, std::string());
    return;
  }

  if (command == "protocol") {
    DecompressAndSendJsonProtocol(connection_id);
    return;
  }
  std::vector<std::string_view> query_components = base::SplitStringPiece(
      query, "&", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  bool for_tab = base::Contains(query_components, "for_tab");

  if (command == "list") {
    DevToolsManager* manager = DevToolsManager::GetInstance();
    DevToolsAgentHost::List list =
        manager->delegate() ? manager->delegate()->RemoteDebuggingTargets(
                                  for_tab ? DevToolsManagerDelegate::kTab
                                          : DevToolsManagerDelegate::kFrame)
            : DevToolsAgentHost::GetOrCreateAll();

    RespondToJsonList(connection_id, info.GetHeaderValue("host"),
                      std::move(list), for_tab);
    return;
  }

  if (command == "new") {
    if (!base::EqualsCaseInsensitiveASCII(
            info.method, net::HttpRequestHeaders::kPutMethod)) {
      SendJson(
          connection_id, net::HTTP_METHOD_NOT_ALLOWED, std::nullopt,
          base::StringPrintf("Using unsafe HTTP verb %s to invoke /json/new. "
                             "This action supports only PUT verb.",
                             info.method.c_str()));
      return;
    }

    std::string_view escaped_url =
        query_components.empty() ? "" : query_components[0];
    GURL url(base::UnescapeBinaryURLComponent(escaped_url));
    if (!url.is_valid())
      url = GURL(url::kAboutBlankURL);
    // TODO(dsv): Remove for "for_tab" support once DevTools Frontend
    // no longer needs it for e2e tests
    scoped_refptr<DevToolsAgentHost> agent_host = delegate_->CreateNewTarget(
        url, for_tab ? DevToolsManagerDelegate::kTab
                     : DevToolsManagerDelegate::kFrame);
    if (!agent_host) {
      SendJson(connection_id, net::HTTP_INTERNAL_SERVER_ERROR, std::nullopt,
               "Could not create new page");
      return;
    }
    std::string host = info.GetHeaderValue("host");
    base::Value::Dict descriptor = SerializeDescriptor(agent_host, host);
    SendJson(connection_id, net::HTTP_OK, descriptor, std::string());
    return;
  }

  if (command == "activate" || command == "close") {
    scoped_refptr<DevToolsAgentHost> agent_host =
        DevToolsAgentHost::GetForId(target_id);
    if (!agent_host) {
      SendJson(connection_id, net::HTTP_NOT_FOUND, std::nullopt,
               "No such target id: " + target_id);
      return;
    }

    if (command == "activate") {
      if (agent_host->Activate()) {
        SendJson(connection_id, net::HTTP_OK, std::nullopt, "Target activated");
      } else {
        SendJson(connection_id, net::HTTP_INTERNAL_SERVER_ERROR, std::nullopt,
                 "Could not activate target id: " + target_id);
      }
      return;
    }

    if (command == "close") {
      if (agent_host->Close()) {
        SendJson(connection_id, net::HTTP_OK, std::nullopt,
                 "Target is closing");
      } else {
        SendJson(connection_id, net::HTTP_INTERNAL_SERVER_ERROR, std::nullopt,
                 "Could not close target id: " + target_id);
      }
      return;
    }
  }
  SendJson(connection_id, net::HTTP_NOT_FOUND, std::nullopt,
           "Unknown command: " + command);
}

void DevToolsHttpHandler::DecompressAndSendJsonProtocol(int connection_id) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_IOS)
  NOTREACHED_IN_MIGRATION();
#else
  scoped_refptr<base::RefCountedMemory> bytes =
      GetContentClient()->GetDataResourceBytes(kCcompressedProtocolJSON);
  CHECK(bytes) << "Could not load protocol";

  net::HttpServerResponseInfo response(net::HTTP_OK);
  response.SetBody(std::string(base::as_string_view(*bytes)),
                   "application/json; charset=UTF-8");

  thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ServerWrapper::SendResponse,
                                base::Unretained(server_wrapper_.get()),
                                connection_id, response));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_IOS)
}

void DevToolsHttpHandler::RespondToJsonList(int connection_id,
                                            const std::string& host,
                                            DevToolsAgentHost::List hosts,
                                            bool for_tab) {
  DevToolsAgentHost::List agent_hosts = std::move(hosts);
  std::sort(agent_hosts.begin(), agent_hosts.end(), TimeComparator);
  base::Value::List list_value;
  for (auto& agent_host : agent_hosts) {
    if (agent_host->GetType() != DevToolsAgentHost::kTypeTab || for_tab) {
      list_value.Append(SerializeDescriptor(agent_host, host));
    }
  }
  SendJson(connection_id, net::HTTP_OK, list_value, std::string());
}

void DevToolsHttpHandler::OnDiscoveryPageRequest(int connection_id) {
  net::HttpServerResponseInfo response(net::HTTP_OK);
  response.AddHeader("X-Frame-Options", "DENY");
  response.SetBody(delegate_->GetDiscoveryPageHTML(),
                   "text/html; charset=UTF-8");

  thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ServerWrapper::SendResponse,
                                base::Unretained(server_wrapper_.get()),
                                connection_id, response));
}

void DevToolsHttpHandler::OnFrontendResourceRequest(
    int connection_id, const std::string& path) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  Send404(connection_id);
#else
  Send200(connection_id,
          content::DevToolsFrontendHost::GetFrontendResource(path),
          GetMimeType(path));
#endif
}

void DevToolsHttpHandler::OnWebSocketRequest(
    int connection_id,
    const net::HttpServerRequestInfo& request) {
  if (!thread_)
    return;

  if (request.headers.count("origin") &&
      !remote_allow_origins_.count(request.headers.at("origin")) &&
      !remote_allow_origins_.count("*")) {
    const std::string& origin = request.headers.at("origin");
    const std::string message = base::StringPrintf(
        "Rejected an incoming WebSocket connection from the %s origin. "
        "Use the command line flag --remote-allow-origins=%s to allow "
        "connections from this origin or --remote-allow-origins=* to allow all "
        "origins.",
        origin.c_str(), origin.c_str());
    Send403(connection_id, message);
    LOG(ERROR) << message;
    return;
  }

  if (base::StartsWith(request.path, browser_guid_,
                       base::CompareCase::SENSITIVE)) {
    scoped_refptr<DevToolsAgentHost> browser_agent =
        DevToolsAgentHost::CreateForBrowser(
            thread_->task_runner(),
            base::BindRepeating(&DevToolsSocketFactory::CreateForTethering,
                                base::Unretained(socket_factory_.get())));
    connection_to_client_[connection_id] =
        std::make_unique<DevToolsAgentHostClientImpl>(
            thread_->task_runner(), server_wrapper_.get(), connection_id,
            browser_agent);
    AcceptWebSocket(connection_id, request);
    return;
  }

  if (!base::StartsWith(request.path, kPageUrlPrefix,
                        base::CompareCase::SENSITIVE)) {
    Send404(connection_id);
    return;
  }

  std::string target_id = request.path.substr(strlen(kPageUrlPrefix));
  scoped_refptr<DevToolsAgentHost> agent =
      DevToolsAgentHost::GetForId(target_id);
  if (!agent) {
    Send500(connection_id, "No such target id: " + target_id);
    return;
  }

  connection_to_client_[connection_id] =
      std::make_unique<DevToolsAgentHostClientImpl>(
          thread_->task_runner(), server_wrapper_.get(), connection_id, agent);

  AcceptWebSocket(connection_id, request);
}

void DevToolsHttpHandler::OnWebSocketMessage(int connection_id,
                                             std::string data) {
  auto it = connection_to_client_.find(connection_id);
  if (it != connection_to_client_.end()) {
    it->second->OnMessage(base::as_bytes(base::make_span(data)));
  }
}

void DevToolsHttpHandler::OnClose(int connection_id) {
  connection_to_client_.erase(connection_id);
}

DevToolsHttpHandler::DevToolsHttpHandler(
    DevToolsManagerDelegate* delegate,
    std::unique_ptr<DevToolsSocketFactory> socket_factory,
    const base::FilePath& output_directory,
    const base::FilePath& debug_frontend_dir)
    : delegate_(delegate) {
  browser_guid_ =
      delegate_->IsBrowserTargetDiscoverable()
          ? kBrowserUrlPrefix
          : base::StringPrintf(
                "%s/%s", kBrowserUrlPrefix,
                base::Uuid::GenerateRandomV4().AsLowercaseString().c_str());
  std::unique_ptr<base::Thread> thread(
      new base::Thread(kDevToolsHandlerThreadName));
  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  if (thread->StartWithOptions(std::move(options))) {
    auto task_runner = thread->task_runner();
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&StartServerOnHandlerThread, weak_factory_.GetWeakPtr(),
                       std::move(thread), std::move(socket_factory),
                       output_directory, debug_frontend_dir, browser_guid_,
                       delegate_->HasBundledFrontendResources()));
  }
  std::string remote_allow_origins = base::ToLowerASCII(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kRemoteAllowOrigins));

  auto origins =
      base::SplitString(remote_allow_origins, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  remote_allow_origins_.insert(origins.begin(), origins.end());
}

void DevToolsHttpHandler::ServerStarted(
    std::unique_ptr<base::Thread> thread,
    std::unique_ptr<ServerWrapper> server_wrapper,
    std::unique_ptr<DevToolsSocketFactory> socket_factory,
    std::unique_ptr<net::IPEndPoint> ip_address) {
  thread_ = std::move(thread);
  server_wrapper_ = std::move(server_wrapper);
  socket_factory_ = std::move(socket_factory);
  server_ip_address_ = std::move(ip_address);
}

void DevToolsHttpHandler::SendJson(int connection_id,
                                   net::HttpStatusCode status_code,
                                   std::optional<base::ValueView> value,
                                   const std::string& message) {
  if (!thread_)
    return;

  // Serialize value and message.
  std::string json_value;
  if (value) {
    base::JSONWriter::WriteWithOptions(
        *value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json_value);
  }
  std::string json_message;
  base::JSONWriter::Write(base::Value(message), &json_message);

  net::HttpServerResponseInfo response(status_code);
  response.AddHeader("Content-Security-Policy", "frame-ancestors 'none'");
  response.SetBody(json_value + message, "application/json; charset=UTF-8");

  thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ServerWrapper::SendResponse,
                                base::Unretained(server_wrapper_.get()),
                                connection_id, response));
}

void DevToolsHttpHandler::Send200(int connection_id,
                                  const std::string& data,
                                  const std::string& mime_type) {
  if (!thread_)
    return;
  thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ServerWrapper::Send200,
                                base::Unretained(server_wrapper_.get()),
                                connection_id, data, mime_type));
}

void DevToolsHttpHandler::Send404(int connection_id) {
  if (!thread_)
    return;
  thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ServerWrapper::Send404,
                     base::Unretained(server_wrapper_.get()), connection_id));
}

void DevToolsHttpHandler::Send403(int connection_id,
                                  const std::string& message) {
  if (!thread_) {
    return;
  }
  net::HttpServerResponseInfo response(net::HTTP_FORBIDDEN);
  response.SetBody(message, "text/html");
  thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ServerWrapper::SendResponse,
                                base::Unretained(server_wrapper_.get()),
                                connection_id, response));
}

void DevToolsHttpHandler::Send500(int connection_id,
                                  const std::string& message) {
  if (!thread_)
    return;
  thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ServerWrapper::Send500,
                                base::Unretained(server_wrapper_.get()),
                                connection_id, message));
}

void DevToolsHttpHandler::AcceptWebSocket(
    int connection_id,
    const net::HttpServerRequestInfo& request) {
  if (!thread_)
    return;
  thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ServerWrapper::AcceptWebSocket,
                                base::Unretained(server_wrapper_.get()),
                                connection_id, request));
}

base::Value::Dict DevToolsHttpHandler::SerializeDescriptor(
    scoped_refptr<DevToolsAgentHost> agent_host,
    const std::string& host) {
  base::Value::Dict dictionary;
  std::string id = agent_host->GetId();
  dictionary.Set(kTargetIdField, id);
  std::string parent_id = agent_host->GetParentId();
  if (!parent_id.empty())
    dictionary.Set(kTargetParentIdField, parent_id);
  dictionary.Set(kTargetTypeField, agent_host->GetType());
  dictionary.Set(kTargetTitleField,
                 base::EscapeForHTML(agent_host->GetTitle()));
  dictionary.Set(kTargetDescriptionField, agent_host->GetDescription());

  GURL url = agent_host->GetURL();
  dictionary.Set(kTargetUrlField, url.spec());

  GURL favicon_url = agent_host->GetFaviconURL();
  if (favicon_url.is_valid())
    dictionary.Set(kTargetFaviconUrlField, favicon_url.spec());

  dictionary.Set(kTargetWebSocketDebuggerUrlField,
                 base::StringPrintf("ws://%s%s%s", host.c_str(), kPageUrlPrefix,
                                    id.c_str()));
  dictionary.Set(kTargetDevtoolsFrontendUrlField,
                 GetFrontendURLInternal(agent_host, id, host));

  return dictionary;
}

}  // namespace content
