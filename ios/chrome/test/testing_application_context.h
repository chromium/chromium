// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_TESTING_APPLICATION_CONTEXT_H_
#define IOS_CHROME_TEST_TESTING_APPLICATION_CONTEXT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "ios/chrome/browser/application_context.h"

namespace network {
class TestNetworkConnectionTracker;
class TestURLLoaderFactory;
class WeakWrapperSharedURLLoaderFactory;
}  // namespace network

class TestingApplicationContext : public ApplicationContext {
 public:
  TestingApplicationContext();
  ~TestingApplicationContext() override;

  // Convenience method to get the current application context as a
  // TestingApplicationContext.
  static TestingApplicationContext* GetGlobal();

  // Sets the local state.
  void SetLocalState(PrefService* local_state);

  // Sets the last shutdown "clean" state.
  void SetLastShutdownClean(bool clean);

  // Sets the ChromeBrowserStateManager.
  void SetChromeBrowserStateManager(ios::ChromeBrowserStateManager* manager);

  // ApplicationContext implementation.
  void OnAppEnterForeground() override;
  void OnAppEnterBackground() override;
  bool WasLastShutdownClean() override;

  PrefService* GetLocalState() override;
  net::URLRequestContextGetter* GetSystemURLRequestContext() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory()
      override;
  network::mojom::NetworkContext* GetSystemNetworkContext() override;
  const std::string& GetApplicationLocale() override;
  ios::ChromeBrowserStateManager* GetChromeBrowserStateManager() override;
  metrics_services_manager::MetricsServicesManager* GetMetricsServicesManager()
      override;
  metrics::MetricsService* GetMetricsService() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  variations::VariationsService* GetVariationsService() override;
  rappor::RapporServiceImpl* GetRapporServiceImpl() override;
  net::NetLog* GetNetLog() override;
  net_log::NetExportFileWriter* GetNetExportFileWriter() override;
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override;
  IOSChromeIOThread* GetIOSChromeIOThread() override;
  gcm::GCMDriver* GetGCMDriver() override;
  component_updater::ComponentUpdateService* GetComponentUpdateService()
      override;
  network::NetworkConnectionTracker* GetNetworkConnectionTracker() override;

 private:
  base::ThreadChecker thread_checker_;
  std::string application_locale_;
  PrefService* local_state_;
  ios::ChromeBrowserStateManager* chrome_browser_state_manager_;
  std::unique_ptr<network_time::NetworkTimeTracker> network_time_tracker_;
  bool was_last_shutdown_clean_;
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
  std::unique_ptr<network::TestNetworkConnectionTracker>
      test_network_connection_tracker_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      system_shared_url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestingApplicationContext);
};

#endif  // IOS_CHROME_TEST_TESTING_APPLICATION_CONTEXT_H_
