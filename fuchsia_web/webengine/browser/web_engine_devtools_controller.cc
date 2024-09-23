// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/web_engine_devtools_controller.h"

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/interface_ptr_set.h>
#include <lib/sys/cpp/component_context.h>

#include <optional>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/common/content_switches.h"
#include "fuchsia_web/webengine/switches.h"
#include "net/base/net_errors.h"
#include "net/base/port_util.h"
#include "net/socket/tcp_server_socket.h"

namespace {

using OnDevToolsPortChanged = base::OnceCallback<void(uint16_t)>;

class DevToolsSocketFactory : public content::DevToolsSocketFactory {
 public:
  DevToolsSocketFactory(OnDevToolsPortChanged on_devtools_port,
                        net::IPEndPoint ip_end_point)
      : on_devtools_port_(std::move(on_devtools_port)),
        ip_end_point_(std::move(ip_end_point)) {}
  ~DevToolsSocketFactory() override = default;

  DevToolsSocketFactory(const DevToolsSocketFactory&) = delete;
  DevToolsSocketFactory& operator=(const DevToolsSocketFactory&) = delete;

  // content::DevToolsSocketFactory implementation.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    const int kTcpListenBackLog = 5;
    auto socket =
        std::make_unique<net::TCPServerSocket>(nullptr, net::NetLogSource());
    int error = socket->Listen(ip_end_point_, kTcpListenBackLog,
                               /*ipv6_only=*/std::nullopt);
    if (error != net::OK) {
      LOG(WARNING) << "Failed to start the HTTP debugger service. "
                   << net::ErrorToString(error);
      std::move(on_devtools_port_).Run(0);
      return nullptr;
    }

    net::IPEndPoint end_point;
    socket->GetLocalAddress(&end_point);
    std::move(on_devtools_port_).Run(end_point.port());
    return socket;
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* out_name) override {
    return nullptr;
  }

 private:
  OnDevToolsPortChanged on_devtools_port_;
  net::IPEndPoint ip_end_point_;
};

void StartRemoteDebuggingServer(OnDevToolsPortChanged on_devtools_port,
                                net::IPEndPoint ip_end_point) {
  const base::FilePath kDisableActivePortOutputDirectory{};
  const base::FilePath kDisableDebugOutput{};

  content::DevToolsAgentHost::StartRemoteDebuggingServer(
      std::make_unique<DevToolsSocketFactory>(std::move(on_devtools_port),
                                              ip_end_point),
      kDisableActivePortOutputDirectory, kDisableDebugOutput);
}

class NoopController : public WebEngineDevToolsController {
 public:
  NoopController() = default;
  ~NoopController() override = default;

  NoopController(const NoopController&) = delete;
  NoopController& operator=(const NoopController&) = delete;

  // WebEngineDevToolsController implementation:
  bool OnFrameCreated(content::WebContents* contents,
                      bool user_debugging) override {
    return !user_debugging;
  }
  void OnFrameLoaded(content::WebContents* contents) override {}
  void OnFrameDestroyed(content::WebContents* contents) override {}
  content::DevToolsAgentHost::List RemoteDebuggingTargets() override {
    return {};
  }
  void GetDevToolsPort(base::OnceCallback<void(uint16_t)> callback) override {
    std::move(callback).Run(0);
  }
};

// "User-mode" makes DevTools accessible to remote devices for Frames specified
// by the web_instance owner. The controller, which starts DevTools when the
// first Frame is created, and shuts it down when no debuggable Frames remain.
class UserModeController : public WebEngineDevToolsController {
 public:
  explicit UserModeController(uint16_t server_port)
      : ip_endpoint_(net::IPAddress::IPv6AllZeros(), server_port) {}
  ~UserModeController() override {
    if (is_remote_debugging_started_) {
      content::DevToolsAgentHost::StopRemoteDebuggingServer();
    }
  }

  UserModeController(const UserModeController&) = delete;
  UserModeController& operator=(const UserModeController&) = delete;

  // WebEngineDevToolsController implementation:
  bool OnFrameCreated(content::WebContents* contents,
                      bool user_debugging) override {
    if (user_debugging) {
      debuggable_contents_.insert(contents);
      UpdateRemoteDebuggingServer();
    }
    return true;
  }
  void OnFrameLoaded(content::WebContents* contents) override {}
  void OnFrameDestroyed(content::WebContents* contents) override {
    debuggable_contents_.erase(contents);
    UpdateRemoteDebuggingServer();
  }
  content::DevToolsAgentHost::List RemoteDebuggingTargets() override {
    DCHECK(is_remote_debugging_started_);

    content::DevToolsAgentHost::List enabled_hosts;
    for (content::WebContents* contents : debuggable_contents_) {
      enabled_hosts.push_back(
          content::DevToolsAgentHost::GetOrCreateFor(contents));
    }
    return enabled_hosts;
  }
  void GetDevToolsPort(base::OnceCallback<void(uint16_t)> callback) override {
    get_port_callbacks_.emplace_back(std::move(callback));
    MaybeNotifyGetPortCallbacks();
  }

 private:
  // Starts or stops the remote debugging server, if necessary
  void UpdateRemoteDebuggingServer() {
    bool need_remote_debugging = !debuggable_contents_.empty();
    if (need_remote_debugging == is_remote_debugging_started_)
      return;
    is_remote_debugging_started_ = need_remote_debugging;

    if (need_remote_debugging) {
      StartRemoteDebuggingServer(
          base::BindOnce(&UserModeController::OnDevToolsPortChanged,
                         base::Unretained(this)),
          ip_endpoint_);
    } else {
      content::DevToolsAgentHost::StopRemoteDebuggingServer();
      devtools_port_.reset();
    }
  }

  void OnDevToolsPortChanged(uint16_t port) {
    devtools_port_ = port;
    MaybeNotifyGetPortCallbacks();
  }

  void MaybeNotifyGetPortCallbacks() {
    if (!devtools_port_.has_value())
      return;
    for (auto& callback : get_port_callbacks_)
      std::move(callback).Run(devtools_port_.value());
    get_port_callbacks_.clear();
  }

  const net::IPEndPoint ip_endpoint_;

  // True if the remote debugging server is started.
  bool is_remote_debugging_started_ = false;

  // Currently active DevTools port. Set to 0 on service startup error.
  std::optional<uint16_t> devtools_port_;

  // Set of Frames' content::WebContents which are remotely debuggable.
  base::flat_set<content::WebContents*> debuggable_contents_;

  std::vector<base::OnceCallback<void(uint16_t)>> get_port_callbacks_;
};

// "Debug-mode" is used for on-device testing, and makes all Frames available
// for debugging by clients on the same device. DevTools is only reported when
// the first Frame finishes loading its main document, so that the
// DevToolsPerContextListeners can start interacting with it immediately.
class DebugModeController : public WebEngineDevToolsController,
                            public fuchsia::web::Debug {
 public:
  DebugModeController()
      : DebugModeController(
            net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0)) {}
  ~DebugModeController() override {
    content::DevToolsAgentHost::StopRemoteDebuggingServer();
  }

  DebugModeController(const DebugModeController&) = delete;
  DebugModeController& operator=(const DebugModeController&) = delete;

  // DevToolsController implementation:
  bool OnFrameCreated(content::WebContents* contents,
                      bool user_debugging) override {
    return !user_debugging;
  }
  void OnFrameLoaded(content::WebContents* contents) override {
    if (!frame_loaded_) {
      frame_loaded_ = true;
      MaybeSendRemoteDebuggingCallbacks();
    }
  }
  void OnFrameDestroyed(content::WebContents* contents) override {}
  content::DevToolsAgentHost::List RemoteDebuggingTargets() override {
    return content::DevToolsAgentHost::GetOrCreateAll();
  }
  void GetDevToolsPort(base::OnceCallback<void(uint16_t)> callback) override {
    std::move(callback).Run(0);
  }

 protected:
  explicit DebugModeController(net::IPEndPoint ip_endpoint)
      : ip_endpoint_(std::move(ip_endpoint)),
        binding_(base::ComponentContextForProcess()->outgoing().get(), this) {
    // Immediately start the service.
    StartRemoteDebuggingServer(
        base::BindOnce(&DebugModeController::OnDevToolsPortChanged,
                       base::Unretained(this)),
        ip_endpoint_);
  }

  virtual void OnDevToolsPortChanged(uint16_t port) {
    devtools_port_ = port;
    MaybeSendRemoteDebuggingCallbacks();
  }

  // Currently active DevTools port. Set to 0 on service startup error.
  std::optional<uint16_t> devtools_port_;

 private:
  // fuchsia::web::Debug implementation.
  void EnableDevTools(
      fidl::InterfaceHandle<fuchsia::web::DevToolsListener> listener_handle,
      EnableDevToolsCallback callback) override {
    // Each web-instance has a single DevTools "context", so create a new
    // per-context channel, and pass it to |listener| immediately.
    fuchsia::web::DevToolsPerContextListenerPtr context_listener;
    auto listener = listener_handle.Bind();
    listener->OnContextDevToolsAvailable(context_listener.NewRequest());

    // Notify the per-context listener immediately, if the port is ready.
    if (frame_loaded_ && devtools_port_)
      context_listener->OnHttpPortOpen(devtools_port_.value());

    devtools_listeners_.AddInterfacePtr(std::move(context_listener));
  }

  void MaybeSendRemoteDebuggingCallbacks() {
    if (!frame_loaded_ || !devtools_port_)
      return;

    // If |devtools_port_| is zero then DevTools failed to initialize, and
    // all listener connections should be closed to indicate failure.
    if (devtools_port_.value() == 0) {
      devtools_listeners_.CloseAll();
      return;
    }

    for (const auto& listener : devtools_listeners_.ptrs()) {
      listener->get()->OnHttpPortOpen(devtools_port_.value());
    }
  }

  const net::IPEndPoint ip_endpoint_;

  bool frame_loaded_ = false;

  fidl::InterfacePtrSet<fuchsia::web::DevToolsPerContextListener>
      devtools_listeners_;

  const base::ScopedServiceBinding<fuchsia::web::Debug> binding_;
};

// "Mixed-mode" is used when both user and debug remote debugging are active at
// the same time. The service is enabled for the lifespan of the web_instance.
class MixedModeController : public DebugModeController {
 public:
  explicit MixedModeController(uint16_t server_port)
      : DebugModeController(
            net::IPEndPoint(net::IPAddress::IPv6AllZeros(), server_port)) {}
  ~MixedModeController() override = default;

  // WebEngineDevToolsController overrides:
  bool OnFrameCreated(content::WebContents* contents,
                      bool user_debugging) override {
    return true;
  }
  void GetDevToolsPort(base::OnceCallback<void(uint16_t)> callback) override {
    get_port_callbacks_.emplace_back(std::move(callback));
    MaybeNotifyGetPortCallbacks();
  }

  // DebugModeController overrides:
  void OnDevToolsPortChanged(uint16_t port) override {
    DebugModeController::OnDevToolsPortChanged(port);
    MaybeNotifyGetPortCallbacks();
  }

  void MaybeNotifyGetPortCallbacks() {
    if (!devtools_port_)
      return;
    for (auto& callback : get_port_callbacks_)
      std::move(callback).Run(devtools_port_.value());
    get_port_callbacks_.clear();
  }

  std::vector<base::OnceCallback<void(uint16_t)>> get_port_callbacks_;
};

}  //  namespace

// static
std::unique_ptr<WebEngineDevToolsController>
WebEngineDevToolsController::CreateFromCommandLine(
    const base::CommandLine& command_line) {
  std::optional<uint16_t> devtools_port;
  if (command_line.HasSwitch(switches::kRemoteDebuggingPort)) {
    // Set up DevTools to listen on all network routes on the command-line
    // provided port.
    std::string command_line_port_value =
        command_line.GetSwitchValueASCII(switches::kRemoteDebuggingPort);
    int parsed_port = 0;

    // The command-line option can only be provided by the ContextProvider
    // process, it should not fail to parse to an int.
    bool parsed = base::StringToInt(command_line_port_value, &parsed_port);
    DCHECK(parsed);

    if (parsed_port != 0 &&
        (!net::IsPortValid(parsed_port) || net::IsWellKnownPort(parsed_port))) {
      LOG(WARNING) << "Invalid HTTP debugger service port number "
                   << command_line_port_value;
    } else {
      devtools_port = parsed_port;
    }
  }

  bool enable_debug_mode =
      command_line.HasSwitch(switches::kEnableRemoteDebugMode);
  if (devtools_port) {
    if (enable_debug_mode) {
      return std::make_unique<MixedModeController>(devtools_port.value());
    } else {
      return std::make_unique<UserModeController>(devtools_port.value());
    }
  } else if (enable_debug_mode) {
    return std::make_unique<DebugModeController>();
  } else {
    return std::make_unique<NoopController>();
  }
}
