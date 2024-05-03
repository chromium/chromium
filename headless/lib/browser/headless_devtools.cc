// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_devtools.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/browser/navigation_entry.h"
#include "headless/public/headless_browser.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/tcp_server_socket.h"
#include "ui/base/resource/resource_bundle.h"

namespace headless {

namespace {

const int kBackLog = 10;

class TCPServerSocketFactory : public content::DevToolsSocketFactory {
 public:
  explicit TCPServerSocketFactory(int port) : port_(port) {}

  TCPServerSocketFactory(const TCPServerSocketFactory&) = delete;
  TCPServerSocketFactory& operator=(const TCPServerSocketFactory&) = delete;

 private:
  // This function, and the logic below that uses it, is copied from
  // chrome/browser/devtools/remote_debugging_server.cc
  static std::unique_ptr<net::ServerSocket> CreateLocalHostServerSocket(
      int port) {
    std::unique_ptr<net::ServerSocket> socket(
        new net::TCPServerSocket(nullptr, net::NetLogSource()));
    if (socket->ListenWithAddressAndPort("127.0.0.1", port, kBackLog) ==
        net::OK)
      return socket;
    if (socket->ListenWithAddressAndPort("::1", port, kBackLog) == net::OK)
      return socket;
    return nullptr;
  }

  // content::DevToolsSocketFactory.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    return CreateLocalHostServerSocket(port_);
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* out_name) override {
    return nullptr;
  }

  const int port_;
};

#if BUILDFLAG(IS_POSIX)
class TCPAdoptServerSocketFactory : public content::DevToolsSocketFactory {
 public:
  // Construct a factory to use an already-open, already-listening socket.
  explicit TCPAdoptServerSocketFactory(const size_t socket_fd)
      : socket_fd_(socket_fd) {}

  TCPAdoptServerSocketFactory(const TCPAdoptServerSocketFactory&) = delete;
  TCPAdoptServerSocketFactory& operator=(const TCPAdoptServerSocketFactory&) =
      delete;

 private:
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    std::unique_ptr<net::TCPServerSocket> tsock(
        new net::TCPServerSocket(nullptr, net::NetLogSource()));
    if (tsock->AdoptSocket(socket_fd_) != net::OK) {
      LOG(ERROR) << "Failed to adopt open socket";
      return nullptr;
    }
    // Note that we assume that the socket is already listening, so unlike
    // TCPServerSocketFactory, we don't call Listen.
    return std::unique_ptr<net::ServerSocket>(std::move(tsock));
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* out_name) override {
    return nullptr;
  }

  size_t socket_fd_;
};
#else   // BUILDFLAG(IS_POSIX)

// Placeholder class to use when a socket_fd is passed in on non-Posix.
class DummyTCPServerSocketFactory : public content::DevToolsSocketFactory {
 public:
  explicit DummyTCPServerSocketFactory() {}

  DummyTCPServerSocketFactory(const DummyTCPServerSocketFactory&) = delete;
  DummyTCPServerSocketFactory& operator=(const DummyTCPServerSocketFactory&) =
      delete;

 private:
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    return nullptr;
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* out_name) override {
    return nullptr;
  }
};
#endif  // BUILDFLAG(IS_POSIX)

void PostTaskToCloseBrowser(base::WeakPtr<HeadlessBrowserImpl> browser) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&HeadlessBrowserImpl::Shutdown, browser));
}

}  // namespace

void StartLocalDevToolsHttpHandler(HeadlessBrowserImpl* browser) {
  HeadlessBrowser::Options* options = browser->options();
  if (options->devtools_pipe_enabled) {
    content::DevToolsAgentHost::StartRemoteDebuggingPipeHandler(
        base::BindOnce(&PostTaskToCloseBrowser, browser->GetWeakPtr()));
  }

  if (!options->devtools_port.has_value()) {
    return;
  }

  std::unique_ptr<content::DevToolsSocketFactory> socket_factory;
  socket_factory =
      std::make_unique<TCPServerSocketFactory>(options->devtools_port.value());

  content::DevToolsAgentHost::StartRemoteDebuggingServer(
      std::move(socket_factory),
      options->user_data_dir,  // TODO(altimin): Figure a proper value for this.
      base::FilePath());
}

void StopLocalDevToolsHttpHandler() {
  content::DevToolsAgentHost::StopRemoteDebuggingServer();
  content::DevToolsAgentHost::StopRemoteDebuggingPipeHandler();
}

}  // namespace headless
