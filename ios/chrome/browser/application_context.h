// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APPLICATION_CONTEXT_H_
#define IOS_CHROME_BROWSER_APPLICATION_CONTEXT_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"

namespace component_updater {
class ComponentUpdateService;
}

namespace gcm {
class GCMDriver;
}

namespace ios {
class ChromeBrowserStateManager;
}

namespace metrics {
class MetricsService;
}

namespace metrics_services_manager {
class MetricsServicesManager;
}

namespace net {
class NetLog;
class URLRequestContextGetter;
}

namespace net_log {
class NetExportFileWriter;
}

namespace network {
class NetworkConnectionTracker;
class SharedURLLoaderFactory;
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace network_time {
class NetworkTimeTracker;
}

namespace rappor {
class RapporServiceImpl;
}

namespace ukm {
class UkmRecorder;
}

namespace variations {
class VariationsService;
}

class ApplicationContext;
class IOSChromeIOThread;
class PrefService;

// Gets the global application context. Cannot return null.
ApplicationContext* GetApplicationContext();

class ApplicationContext {
 public:
  ApplicationContext();
  virtual ~ApplicationContext();

  // Invoked when application enters foreground. Cancels the effect of
  // OnAppEnterBackground(), in particular removes the boolean preference
  // indicating that the ChromeBrowserStates have been shutdown.
  virtual void OnAppEnterForeground() = 0;

  // Invoked when application enters background. Saves any state that must be
  // saved before shutdown can continue.
  virtual void OnAppEnterBackground() = 0;

  // Returns whether the last complete shutdown was clean (i.e. happened while
  // the application was backgrounded).
  virtual bool WasLastShutdownClean() = 0;

  // Gets the local state associated with this application.
  virtual PrefService* GetLocalState() = 0;

  // Gets the URL request context associated with this application.
  virtual net::URLRequestContextGetter* GetSystemURLRequestContext() = 0;

  // Gets the shared URL loader factory associated with this application.
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetSharedURLLoaderFactory() = 0;

  // Gets the NetworkContext object associated with the same context as
  // GetSystemURLRequestContext().
  virtual network::mojom::NetworkContext* GetSystemNetworkContext() = 0;

  // Gets the locale used by the application.
  virtual const std::string& GetApplicationLocale() = 0;

  // Gets the ChromeBrowserStateManager used by this application.
  virtual ios::ChromeBrowserStateManager* GetChromeBrowserStateManager() = 0;

  // Gets the manager for the various metrics-related service, constructing it
  // if necessary.
  virtual metrics_services_manager::MetricsServicesManager*
  GetMetricsServicesManager() = 0;

  // Gets the MetricsService used by this application.
  virtual metrics::MetricsService* GetMetricsService() = 0;

  // Gets the UkmRecorder used by this application.
  virtual ukm::UkmRecorder* GetUkmRecorder() = 0;

  // Gets the VariationsService used by this application.
  virtual variations::VariationsService* GetVariationsService() = 0;

  // Gets the RapporServiceImpl. May return null.
  virtual rappor::RapporServiceImpl* GetRapporServiceImpl() = 0;

  // Gets the NetLog.
  virtual net::NetLog* GetNetLog() = 0;

  virtual net_log::NetExportFileWriter* GetNetExportFileWriter() = 0;

  // Gets the NetworkTimeTracker.
  virtual network_time::NetworkTimeTracker* GetNetworkTimeTracker() = 0;

  // Gets the IOSChromeIOThread.
  virtual IOSChromeIOThread* GetIOSChromeIOThread() = 0;

  // Gets the GCMDriver.
  virtual gcm::GCMDriver* GetGCMDriver() = 0;

  // Gets the ComponentUpdateService.
  virtual component_updater::ComponentUpdateService*
  GetComponentUpdateService() = 0;

  // Returns the NetworkConnectionTracker instance for this ApplicationContext.
  virtual network::NetworkConnectionTracker* GetNetworkConnectionTracker() = 0;

 protected:
  // Sets the global ApplicationContext instance.
  static void SetApplicationContext(ApplicationContext* context);

 private:
  DISALLOW_COPY_AND_ASSIGN(ApplicationContext);
};

#endif  // IOS_CHROME_BROWSER_APPLICATION_CONTEXT_H_
