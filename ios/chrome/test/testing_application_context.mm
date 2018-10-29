// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/testing_application_context.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "components/network_time/network_time_tracker.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TestingApplicationContext::TestingApplicationContext()
    : application_locale_("en"),
      local_state_(nullptr),
      chrome_browser_state_manager_(nullptr),
      was_last_shutdown_clean_(false),
      test_url_loader_factory_(
          std::make_unique<network::TestURLLoaderFactory>()),
      test_network_connection_tracker_(
          network::TestNetworkConnectionTracker::CreateInstance()),
      system_shared_url_loader_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              test_url_loader_factory_.get())) {
  DCHECK(!GetApplicationContext());
  SetApplicationContext(this);
}

TestingApplicationContext::~TestingApplicationContext() {
  system_shared_url_loader_factory_->Detach();
  DCHECK_EQ(this, GetApplicationContext());
  DCHECK(!local_state_);
  SetApplicationContext(nullptr);
}

// static
TestingApplicationContext* TestingApplicationContext::GetGlobal() {
  return static_cast<TestingApplicationContext*>(GetApplicationContext());
}

void TestingApplicationContext::SetLocalState(PrefService* local_state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!local_state) {
    // The local state is owned outside of TestingApplicationContext, but
    // some of the members of TestingApplicationContext hold references to it.
    // Given our test infrastructure which tears down individual tests before
    // freeing the TestingApplicationContext, there's no good way to make the
    // local state outlive these dependencies. As a workaround, whenever
    // local state is cleared (assumedly as part of exiting the test) any
    // components owned by TestingApplicationContext that depends on the local
    // state are also freed.
    network_time_tracker_.reset();
  }
  local_state_ = local_state;
}

void TestingApplicationContext::SetLastShutdownClean(bool clean) {
  was_last_shutdown_clean_ = clean;
}

void TestingApplicationContext::SetChromeBrowserStateManager(
    ios::ChromeBrowserStateManager* manager) {
  DCHECK(thread_checker_.CalledOnValidThread());
  chrome_browser_state_manager_ = manager;
}

void TestingApplicationContext::OnAppEnterForeground() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void TestingApplicationContext::OnAppEnterBackground() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool TestingApplicationContext::WasLastShutdownClean() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return was_last_shutdown_clean_;
}

PrefService* TestingApplicationContext::GetLocalState() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return local_state_;
}

net::URLRequestContextGetter*
TestingApplicationContext::GetSystemURLRequestContext() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

scoped_refptr<network::SharedURLLoaderFactory>
TestingApplicationContext::GetSharedURLLoaderFactory() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return system_shared_url_loader_factory_;
}

network::mojom::NetworkContext*
TestingApplicationContext::GetSystemNetworkContext() {
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTREACHED();
  return nullptr;
}

const std::string& TestingApplicationContext::GetApplicationLocale() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!application_locale_.empty());
  return application_locale_;
}

ios::ChromeBrowserStateManager*
TestingApplicationContext::GetChromeBrowserStateManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return chrome_browser_state_manager_;
}

metrics_services_manager::MetricsServicesManager*
TestingApplicationContext::GetMetricsServicesManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

metrics::MetricsService* TestingApplicationContext::GetMetricsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

ukm::UkmRecorder* TestingApplicationContext::GetUkmRecorder() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

variations::VariationsService*
TestingApplicationContext::GetVariationsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

rappor::RapporServiceImpl* TestingApplicationContext::GetRapporServiceImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

net::NetLog* TestingApplicationContext::GetNetLog() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

net_log::NetExportFileWriter*
TestingApplicationContext::GetNetExportFileWriter() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

network_time::NetworkTimeTracker*
TestingApplicationContext::GetNetworkTimeTracker() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!network_time_tracker_) {
    DCHECK(local_state_);
    network_time_tracker_.reset(new network_time::NetworkTimeTracker(
        base::WrapUnique(new base::DefaultClock),
        base::WrapUnique(new base::DefaultTickClock), local_state_, nullptr));
  }
  return network_time_tracker_.get();
}

IOSChromeIOThread* TestingApplicationContext::GetIOSChromeIOThread() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

gcm::GCMDriver* TestingApplicationContext::GetGCMDriver() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

component_updater::ComponentUpdateService*
TestingApplicationContext::GetComponentUpdateService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

network::NetworkConnectionTracker*
TestingApplicationContext::GetNetworkConnectionTracker() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return test_network_connection_tracker_.get();
}
