// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_devtools_manager_delegate.h"

#include <stdint.h>

#include <vector>

#include "base/atomicops.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client_channel.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "content/shell/browser/protocol/shell_devtools_session.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_content_client.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/grit/shell_resources.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/tcp_server_socket.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/public/browser/android/devtools_auth.h"
#include "net/socket/unix_domain_server_socket_posix.h"
#endif

namespace content {

namespace {

const int kBackLog = 10;

base::subtle::Atomic32 g_last_used_port;

#if BUILDFLAG(IS_ANDROID)
class UnixDomainServerSocketFactory : public content::DevToolsSocketFactory {
 public:
  explicit UnixDomainServerSocketFactory(const std::string& socket_name)
      : socket_name_(socket_name) {}

  UnixDomainServerSocketFactory(const UnixDomainServerSocketFactory&) = delete;
  UnixDomainServerSocketFactory& operator=(
      const UnixDomainServerSocketFactory&) = delete;

 private:
  // content::DevToolsSocketFactory.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    std::unique_ptr<net::UnixDomainServerSocket> socket(
        new net::UnixDomainServerSocket(
            base::BindRepeating(&CanUserConnectToDevTools),
            true /* use_abstract_namespace */));
    if (socket->BindAndListen(socket_name_, kBackLog) != net::OK)
      return nullptr;

    return std::move(socket);
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* out_name) override {
    return nullptr;
  }

  std::string socket_name_;
};
#else
class TCPServerSocketFactory : public content::DevToolsSocketFactory {
 public:
  TCPServerSocketFactory(const std::string& address, uint16_t port)
      : address_(address), port_(port) {}

  TCPServerSocketFactory(const TCPServerSocketFactory&) = delete;
  TCPServerSocketFactory& operator=(const TCPServerSocketFactory&) = delete;

 private:
  // content::DevToolsSocketFactory.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    std::unique_ptr<net::ServerSocket> socket(
        new net::TCPServerSocket(nullptr, net::NetLogSource()));
    if (socket->ListenWithAddressAndPort(address_, port_, kBackLog) != net::OK)
      return nullptr;

    net::IPEndPoint endpoint;
    if (socket->GetLocalAddress(&endpoint) == net::OK)
      base::subtle::NoBarrier_Store(&g_last_used_port, endpoint.port());

    return socket;
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* out_name) override {
    return nullptr;
  }

  std::string address_;
  uint16_t port_;
};
#endif

std::unique_ptr<content::DevToolsSocketFactory> CreateSocketFactory() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
#if BUILDFLAG(IS_ANDROID)
  std::string socket_name = "content_shell_devtools_remote";
  if (command_line.HasSwitch(switches::kRemoteDebuggingSocketName)) {
    socket_name = command_line.GetSwitchValueASCII(
        switches::kRemoteDebuggingSocketName);
  }
  return std::unique_ptr<content::DevToolsSocketFactory>(
      new UnixDomainServerSocketFactory(socket_name));
#else
  // See if the user specified a port on the command line (useful for
  // automation). If not, use an ephemeral port by specifying 0.
  uint16_t port = 0;
  if (command_line.HasSwitch(switches::kRemoteDebuggingPort)) {
    int temp_port;
    std::string port_str =
        command_line.GetSwitchValueASCII(switches::kRemoteDebuggingPort);
    if (base::StringToInt(port_str, &temp_port) &&
        temp_port >= 0 && temp_port < 65535) {
      port = static_cast<uint16_t>(temp_port);
    } else {
      DLOG(WARNING) << "Invalid http debugger port number " << temp_port;
    }
  }
  // By default listen to incoming DevTools connections on localhost.
  std::string address_str = net::IPAddress::IPv4Localhost().ToString();
  if (command_line.HasSwitch(switches::kRemoteDebuggingAddress)) {
    net::IPAddress address;
    address_str =
        command_line.GetSwitchValueASCII(switches::kRemoteDebuggingAddress);
    if (!address.AssignFromIPLiteral(address_str)) {
      DLOG(WARNING) << "Invalid devtools server address: " << address_str;
    }
  }
  return std::unique_ptr<content::DevToolsSocketFactory>(
      new TCPServerSocketFactory(address_str, port));
#endif
}

} //  namespace

// ShellDevToolsManagerDelegate ----------------------------------------------

// static
int ShellDevToolsManagerDelegate::GetHttpHandlerPort() {
  return base::subtle::NoBarrier_Load(&g_last_used_port);
}

// static
void ShellDevToolsManagerDelegate::StartHttpHandler(
    BrowserContext* browser_context) {
  std::string frontend_url;
  DevToolsAgentHost::StartRemoteDebuggingServer(
      CreateSocketFactory(), browser_context->GetPath(), base::FilePath());

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kRemoteDebuggingPipe))
    DevToolsAgentHost::StartRemoteDebuggingPipeHandler(base::OnceClosure());
}

// static
void ShellDevToolsManagerDelegate::StopHttpHandler() {
  DevToolsAgentHost::StopRemoteDebuggingServer();
}

ShellDevToolsManagerDelegate::ShellDevToolsManagerDelegate(
    BrowserContext* browser_context)
    : browser_context_(browser_context) {
}

ShellDevToolsManagerDelegate::~ShellDevToolsManagerDelegate() {
}

BrowserContext* ShellDevToolsManagerDelegate::GetDefaultBrowserContext() {
  return browser_context_;
}

void ShellDevToolsManagerDelegate::ClientAttached(
    content::DevToolsAgentHostClientChannel* channel) {
  // Make sure we don't receive notifications twice for the same client.
  CHECK(!base::Contains(sessions_, channel));
  sessions_.emplace(
      channel,
      std::make_unique<shell::protocol::ShellDevToolsSession>(
          base::raw_ref<BrowserContext>::from_ptr(browser_context_), channel));
}

void ShellDevToolsManagerDelegate::ClientDetached(
    content::DevToolsAgentHostClientChannel* channel) {
  sessions_.erase(channel);
}

void ShellDevToolsManagerDelegate::HandleCommand(
    content::DevToolsAgentHostClientChannel* channel,
    base::span<const uint8_t> message,
    NotHandledCallback callback) {
  auto& session = sessions_.at(channel);
  session->HandleCommand(message, std::move(callback));
}

scoped_refptr<DevToolsAgentHost> ShellDevToolsManagerDelegate::CreateNewTarget(
    const GURL& url,
    content::DevToolsManagerDelegate::TargetType target_type) {
  Shell* shell = Shell::CreateNewWindow(browser_context_, url, nullptr,
                                        Shell::GetShellDefaultSize());
  return target_type == content::DevToolsManagerDelegate::kTab
             ? DevToolsAgentHost::GetOrCreateForTab(shell->web_contents())
             : DevToolsAgentHost::GetOrCreateFor(shell->web_contents());
}

std::string ShellDevToolsManagerDelegate::GetDiscoveryPageHTML() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return std::string();
#else
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
      IDR_CONTENT_SHELL_DEVTOOLS_DISCOVERY_PAGE);
#endif
}

bool ShellDevToolsManagerDelegate::HasBundledFrontendResources() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return false;
#else
  return true;
#endif
}

}  // namespace content
