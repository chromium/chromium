// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/network_service_instance.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/environment.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/browser/network_service_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "net/log/net_log_util.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/net_log.mojom.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

namespace {

network::mojom::NetworkServicePtr* g_network_service_ptr = nullptr;
network::NetworkConnectionTracker* g_network_connection_tracker;
network::NetworkService* g_network_service;

void CreateNetworkServiceOnIO(network::mojom::NetworkServiceRequest request) {
  if (g_network_service) {
    // GetNetworkServiceImpl() was already called and created the object, so
    // just bind it.
    g_network_service->Bind(std::move(request));
    return;
  }

  g_network_service = new network::NetworkService(
      nullptr, std::move(request), GetContentClient()->browser()->GetNetLog());
}

void BindNetworkChangeManagerRequest(
    network::mojom::NetworkChangeManagerRequest request) {
  GetNetworkService()->GetNetworkChangeManager(std::move(request));
}

using CrashHandlersMap =
    std::map<NetworkServiceCrashHandlerId, base::RepeatingClosure>;
CrashHandlersMap& GetCrashHandlersMap() {
  static base::NoDestructor<CrashHandlersMap> s_map;
  return *s_map;
}

void OnNetworkServiceCrash() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(g_network_service_ptr);
  DCHECK(g_network_service_ptr->is_bound());
  DCHECK(g_network_service_ptr->encountered_error());
  for (const auto& it : GetCrashHandlersMap()) {
    const base::RepeatingClosure& handler = it.second;
    handler.Run();
  }
}

}  // namespace

network::mojom::NetworkService* GetNetworkService() {
  service_manager::Connector* connector =
      base::FeatureList::IsEnabled(network::features::kNetworkService)
          ? ServiceManagerConnection::GetForProcess()->GetConnector()
          : nullptr;
  return GetNetworkServiceFromConnector(connector);
}

CONTENT_EXPORT network::mojom::NetworkService* GetNetworkServiceFromConnector(
    service_manager::Connector* connector) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!g_network_service_ptr)
    g_network_service_ptr = new network::mojom::NetworkServicePtr;
  static NetworkServiceClient* g_client;
  if (!g_network_service_ptr->is_bound() ||
      g_network_service_ptr->encountered_error()) {
    if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
      connector->BindInterface(mojom::kNetworkServiceName,
                               g_network_service_ptr);
      g_network_service_ptr->set_connection_error_handler(
          base::BindOnce(&OnNetworkServiceCrash));
    } else {
      DCHECK(!g_network_service_ptr->is_bound());
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(CreateNetworkServiceOnIO,
                         mojo::MakeRequest(g_network_service_ptr)));
    }

    network::mojom::NetworkServiceClientPtr client_ptr;
    delete g_client;  // In case we're recreating the network service.
    g_client = new NetworkServiceClient(mojo::MakeRequest(&client_ptr));
    (*g_network_service_ptr)->SetClient(std::move(client_ptr));

      const base::CommandLine* command_line =
          base::CommandLine::ForCurrentProcess();
      if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
        if (command_line->HasSwitch(network::switches::kLogNetLog)) {
          base::FilePath log_path =
              command_line->GetSwitchValuePath(network::switches::kLogNetLog);

          base::DictionaryValue client_constants =
              GetContentClient()->GetNetLogConstants();

          base::File file(log_path, base::File::FLAG_CREATE_ALWAYS |
                                        base::File::FLAG_WRITE);
          LOG_IF(ERROR, !file.IsValid())
              << "Failed opening: " << log_path.value();

          // TODO(mmenke): Get capture mode from the command line.
          (*g_network_service_ptr)
              ->StartNetLog(std::move(file),
                            network::mojom::NetLogCaptureMode::DEFAULT,
                            std::move(client_constants));
        }
      }

      if (command_line->HasSwitch(network::switches::kSSLKeyLogFile)) {
        base::FilePath log_path =
            command_line->GetSwitchValuePath(network::switches::kSSLKeyLogFile);
        LOG_IF(WARNING, log_path.empty())
            << "ssl-key-log-file argument missing";
        if (!log_path.empty())
          (*g_network_service_ptr)->SetSSLKeyLogFile(log_path);
      }

      std::unique_ptr<base::Environment> env(base::Environment::Create());
      std::string env_str;
      if (env->GetVar("SSLKEYLOGFILE", &env_str)) {
#if defined(OS_WIN)
        // base::Environment returns environment variables in UTF-8 on Windows.
        base::FilePath log_path(base::UTF8ToUTF16(env_str));
#else
        base::FilePath log_path(env_str);
#endif
        if (!log_path.empty())
          (*g_network_service_ptr)->SetSSLKeyLogFile(log_path);
      }

      GetContentClient()->browser()->OnNetworkServiceCreated(
          g_network_service_ptr->get());
  }
  return g_network_service_ptr->get();
}

NetworkServiceCrashHandlerId RegisterNetworkServiceCrashHandler(
    base::RepeatingClosure handler) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!handler.is_null());

  static int next_handler_id = 1;
  NetworkServiceCrashHandlerId handler_id =
      NetworkServiceCrashHandlerId::FromUnsafeValue(next_handler_id++);

  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    CrashHandlersMap& map = GetCrashHandlersMap();
    map[handler_id] = std::move(handler);
  }

  return handler_id;
}

void UnregisterNetworkServiceCrashHandler(
    NetworkServiceCrashHandlerId handler_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    CrashHandlersMap& map = GetCrashHandlersMap();
    map.erase(handler_id);
  }
}

network::NetworkService* GetNetworkServiceImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService));
  if (!g_network_service) {
    g_network_service = new network::NetworkService(
        nullptr, nullptr, GetContentClient()->browser()->GetNetLog());
  }

  return g_network_service;
}

void FlushNetworkServiceInstanceForTesting() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (g_network_service_ptr)
    g_network_service_ptr->FlushForTesting();
}

network::NetworkConnectionTracker* GetNetworkConnectionTracker() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!g_network_connection_tracker) {
    g_network_connection_tracker = new network::NetworkConnectionTracker(
        base::BindRepeating(&BindNetworkChangeManagerRequest));
  }
  return g_network_connection_tracker;
}

void GetNetworkConnectionTrackerFromUIThread(
    base::OnceCallback<void(network::NetworkConnectionTracker*)> callback) {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {BrowserThread::UI, base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&GetNetworkConnectionTracker), std::move(callback));
}

void SetNetworkConnectionTrackerForTesting(
    network::NetworkConnectionTracker* network_connection_tracker) {
  if (g_network_connection_tracker != network_connection_tracker) {
    DCHECK(!g_network_connection_tracker || !network_connection_tracker);
    g_network_connection_tracker = network_connection_tracker;
  }
}

}  // namespace content
