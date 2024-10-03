// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_APPLICATION_CONTEXT_APPLICATION_CONTEXT_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_APPLICATION_CONTEXT_APPLICATION_CONTEXT_H_

#import <Foundation/Foundation.h>

#include <string>

#include "base/memory/scoped_refptr.h"
#import "components/optimization_guide/optimization_guide_buildflags.h"

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
#include "base/memory/weak_ptr.h"
#endif  // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

namespace component_updater {
class ComponentUpdateService;
}

namespace gcm {
class GCMDriver;
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
}  // namespace net

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

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
namespace optimization_guide {
class OnDeviceModelComponentStateManager;
class OnDeviceModelServiceController;
}  // namespace optimization_guide
#endif  // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

namespace os_crypt_async {
class OSCryptAsync;
}

namespace signin {
class ActivePrimaryAccountsMetricsRecorder;
}

namespace ukm {
class UkmRecorder;
}

namespace variations {
class VariationsService;
}

class AdditionalFeaturesController;
class AccountProfileMapper;
class ApplicationContext;
class BrowserPolicyConnectorIOS;
class IncognitoSessionTracker;
class IOSChromeIOThread;
class PrefService;

class ProfileManagerIOS;

class PushNotificationService;
class SafeBrowsingService;
@protocol SingleSignOnService;
class SystemIdentityManager;

// Gets the global application context. Cannot return null.
ApplicationContext* GetApplicationContext();

class ApplicationContext {
 public:
  ApplicationContext();

  ApplicationContext(const ApplicationContext&) = delete;
  ApplicationContext& operator=(const ApplicationContext&) = delete;

  virtual ~ApplicationContext();

  // Invoked when application enters foreground. Cancels the effect of
  // OnAppEnterBackground(), in particular removes the boolean preference
  // indicating that the Profiles have been shutdown.
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

  // Gets the country locale used by the application
  virtual const std::string& GetApplicationCountry() = 0;

  // Gets the Profile Manager used by this application.
  virtual ProfileManagerIOS* GetProfileManager() = 0;

  // Gets the manager for the various metrics-related service, constructing it
  // if necessary. May return null.
  virtual metrics_services_manager::MetricsServicesManager*
  GetMetricsServicesManager() = 0;

  // Gets the MetricsService used by this application. May return null.
  virtual metrics::MetricsService* GetMetricsService() = 0;

  // Gets the ActivePrimaryAccountsMetricsRecorder used by this application. May
  // return null.
  virtual signin::ActivePrimaryAccountsMetricsRecorder*
  GetActivePrimaryAccountsMetricsRecorder() = 0;

  // Gets the UkmRecorder used by this application. May return null.
  virtual ukm::UkmRecorder* GetUkmRecorder() = 0;

  // Gets the VariationsService used by this application. May return null.
  virtual variations::VariationsService* GetVariationsService() = 0;

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

  // Gets the SafeBrowsingService.
  virtual SafeBrowsingService* GetSafeBrowsingService() = 0;

  // Returns the NetworkConnectionTracker instance for this ApplicationContext.
  virtual network::NetworkConnectionTracker* GetNetworkConnectionTracker() = 0;

  // Returns the BrowserPolicyConnectorIOS that starts and manages the policy
  // system. May be null if policy is not enabled.
  virtual BrowserPolicyConnectorIOS* GetBrowserPolicyConnector() = 0;

  // Returns the SingleSignOnService instance used by this application.
  virtual id<SingleSignOnService> GetSingleSignOnService() = 0;

  // Returns the SystemIdentityManager instance used by this application.
  virtual SystemIdentityManager* GetSystemIdentityManager() = 0;

  // Returns the AccountProfileMapper instance used by this application.
  virtual AccountProfileMapper* GetAccountProfileMapper() = 0;

  // Returns the application's IncognitoSessionTracker instance.
  virtual IncognitoSessionTracker* GetIncognitoSessionTracker() = 0;

  // Returns the application's PushNotificationService that handles all
  // interactions with the push notification server
  virtual PushNotificationService* GetPushNotificationService() = 0;

  // Returns the application's OSCryptAsync instance which can be used to create
  // instances of Encryptor for data encryption.
  virtual os_crypt_async::OSCryptAsync* GetOSCryptAsync() = 0;

  // Returns the application's AdditionalFeaturesController that manages some
  // features not declared by `BASE_DECLARE_FEATURE()`.
  virtual AdditionalFeaturesController* GetAdditionalFeaturesController() = 0;

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
  // Returns the application's OnDeviceModelServiceController which manages the
  // on-device model service.
  virtual optimization_guide::OnDeviceModelServiceController*
  GetOnDeviceModelServiceController(
      base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
          on_device_component_manager) = 0;
#endif  // BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE

 protected:
  // Sets the global ApplicationContext instance.
  static void SetApplicationContext(ApplicationContext* context);
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_APPLICATION_CONTEXT_APPLICATION_CONTEXT_H_
