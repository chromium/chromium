// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a small utility that watches for and logs network changes.
// It prints out the current network connection type and proxy configuration
// upon startup and then prints out changes as they happen.
// It's useful for testing NetworkChangeNotifier and ProxyConfigService.
// The only command line option supported is --ignore-netif which is followed
// by a comma seperated list of network interfaces to ignore when computing
// connection type; this option is only supported on linux.

#include <memory>
#include <string>
#include <unordered_set>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "net/base/network_change_notifier.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"

#if BUILDFLAG(IS_LINUX)
#include "net/base/network_change_notifier_linux.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "base/apple/scoped_nsautorelease_pool.h"
#endif

namespace {

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX)
// Flag to specifies which network interfaces to ignore. Interfaces should
// follow as a comma seperated list.
const char kIgnoreNetifFlag[] = "ignore-netif";
#endif

// Conversions from various network-related types to string.

const char* ConnectionTypeToString(
    net::NetworkChangeNotifier::ConnectionType type) {
  switch (type) {
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
      return "CONNECTION_UNKNOWN";
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
      return "CONNECTION_ETHERNET";
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
      return "CONNECTION_WIFI";
    case net::NetworkChangeNotifier::CONNECTION_2G:
      return "CONNECTION_2G";
    case net::NetworkChangeNotifier::CONNECTION_3G:
      return "CONNECTION_3G";
    case net::NetworkChangeNotifier::CONNECTION_4G:
      return "CONNECTION_4G";
    case net::NetworkChangeNotifier::CONNECTION_5G:
      return "CONNECTION_5G";
    case net::NetworkChangeNotifier::CONNECTION_NONE:
      return "CONNECTION_NONE";
    case net::NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      return "CONNECTION_BLUETOOTH";
    default:
      return "CONNECTION_UNEXPECTED";
  }
}

std::string ProxyConfigToString(const net::ProxyConfig& config) {
  base::Value config_value = config.ToValue();
  std::string str;
  base::JSONWriter::Write(config_value, &str);
  return str;
}

const char* ConfigAvailabilityToString(
    net::ProxyConfigService::ConfigAvailability availability) {
  switch (availability) {
    case net::ProxyConfigService::CONFIG_PENDING:
      return "CONFIG_PENDING";
    case net::ProxyConfigService::CONFIG_VALID:
      return "CONFIG_VALID";
    case net::ProxyConfigService::CONFIG_UNSET:
      return "CONFIG_UNSET";
    default:
      return "CONFIG_UNEXPECTED";
  }
}

// The main observer class that logs network events.
class NetWatcher :
      public net::NetworkChangeNotifier::IPAddressObserver,
      public net::NetworkChangeNotifier::ConnectionTypeObserver,
      public net::NetworkChangeNotifier::DNSObserver,
      public net::NetworkChangeNotifier::NetworkChangeObserver,
      public net::ProxyConfigService::Observer {
 public:
  NetWatcher() = default;

  NetWatcher(const NetWatcher&) = delete;
  NetWatcher& operator=(const NetWatcher&) = delete;

  ~NetWatcher() override = default;

  // net::NetworkChangeNotifier::IPAddressObserver implementation.
  void OnIPAddressChanged() override { LOG(INFO) << "OnIPAddressChanged()"; }

  // net::NetworkChangeNotifier::ConnectionTypeObserver implementation.
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override {
    LOG(INFO) << "OnConnectionTypeChanged("
              << ConnectionTypeToString(type) << ")";
  }

  // net::NetworkChangeNotifier::DNSObserver implementation.
  void OnDNSChanged() override { LOG(INFO) << "OnDNSChanged()"; }

  // net::NetworkChangeNotifier::NetworkChangeObserver implementation.
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override {
    LOG(INFO) << "OnNetworkChanged("
              << ConnectionTypeToString(type) << ")";
  }

  // net::ProxyConfigService::Observer implementation.
  void OnProxyConfigChanged(
      const net::ProxyConfigWithAnnotation& config,
      net::ProxyConfigService::ConfigAvailability availability) override {
    LOG(INFO) << "OnProxyConfigChanged(" << ProxyConfigToString(config.value())
              << ", " << ConfigAvailabilityToString(availability) << ")";
  }
};

}  // namespace

int main(int argc, char* argv[]) {
#if BUILDFLAG(IS_APPLE)
  base::apple::ScopedNSAutoreleasePool pool;
#endif
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  // Just make the main task executor the network loop.
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);

  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("NetWatcher");

  NetWatcher net_watcher;

#if BUILDFLAG(IS_LINUX)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string ignored_netifs_str =
      command_line->GetSwitchValueASCII(kIgnoreNetifFlag);
  std::unordered_set<std::string> ignored_interfaces;
  if (!ignored_netifs_str.empty()) {
    for (const std::string& ignored_netif :
         base::SplitString(ignored_netifs_str, ",", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_ALL)) {
      LOG(INFO) << "Ignoring: " << ignored_netif;
      ignored_interfaces.insert(ignored_netif);
    }
  }
  auto network_change_notifier =
      std::make_unique<net::NetworkChangeNotifierLinux>(ignored_interfaces);
#else
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier(
      net::NetworkChangeNotifier::CreateIfNeeded());
#endif

  // Use the network loop as the file loop also.
  std::unique_ptr<net::ProxyConfigService> proxy_config_service(
      net::ProxyConfigService::CreateSystemProxyConfigService(
          io_task_executor.task_runner()));

  // Uses |network_change_notifier|.
  net::NetworkChangeNotifier::AddIPAddressObserver(&net_watcher);
  net::NetworkChangeNotifier::AddConnectionTypeObserver(&net_watcher);
  net::NetworkChangeNotifier::AddDNSObserver(&net_watcher);
  net::NetworkChangeNotifier::AddNetworkChangeObserver(&net_watcher);

  proxy_config_service->AddObserver(&net_watcher);

  LOG(INFO) << "Initial connection type: "
            << ConnectionTypeToString(
                   net::NetworkChangeNotifier::GetConnectionType());

  {
    net::ProxyConfigWithAnnotation config;
    const net::ProxyConfigService::ConfigAvailability availability =
        proxy_config_service->GetLatestProxyConfig(&config);
    LOG(INFO) << "Initial proxy config: " << ProxyConfigToString(config.value())
              << ", " << ConfigAvailabilityToString(availability);
  }

  LOG(INFO) << "Watching for network events...";

  // Start watching for events.
  base::RunLoop().Run();

  proxy_config_service->RemoveObserver(&net_watcher);

  // Uses |network_change_notifier|.
  net::NetworkChangeNotifier::RemoveDNSObserver(&net_watcher);
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(&net_watcher);
  net::NetworkChangeNotifier::RemoveIPAddressObserver(&net_watcher);
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(&net_watcher);

  return 0;
}
