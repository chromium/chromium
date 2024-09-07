// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTROLLEE_REQUEST_HANDLER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTROLLEE_REQUEST_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_accessed_callback.h"
#include "content/browser/service_worker/service_worker_main_resource_loader.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerClient;
class ServiceWorkerContextCore;
class ServiceWorkerRegistration;
class ServiceWorkerVersion;

// Handles a main resource request for service worker clients (documents and
// shared workers). This manages state for a single request and does not
// live across redirects. ServiceWorkerMainResourceLoaderInterceptor creates
// one instance of this class for each request/redirect.
//
// This class associates the ServiceWorkerClient undergoing navigation
// with a controller service worker, after looking up the registration and
// activating the service worker if needed.  Once ready, it creates
// ServiceWorkerMainResourceLoader to perform the resource load.
class CONTENT_EXPORT ServiceWorkerControlleeRequestHandler final {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // These have to be consistent with ServiceWorkerFetchHandlerSkipReason in
  // tools/metrics/histograms/enums.xml
  //
  // Only one reason is recorded even if multiple reasons are matched.
  // The order is following:
  // 1. kSkippedForEmptyFetchHandler
  // 2. kMainResourceSkippedDueToOriginTrial
  // 3. kMainResourceSkippedDueToFeatureFlag
  // 4. kMainResourceSkippedBecauseMatchedWithAllowedScriptList
  // 5.
  // kBypassFetchHandlerForAllOnlyIfServiceWorkerNotStarted_Status_[Stop|Starting]
  enum class FetchHandlerSkipReason {
    kNoFetchHandler = 0,
    kNotSkipped = 1,
    kSkippedForEmptyFetchHandler = 2,
    // kMainResourceSkippedDueToOriginTrial = 3,
    // kMainResourceSkippedDueToFeatureFlag = 4,
    // kMainResourceSkippedBecauseMatchedWithAllowedOriginList = 5,
    // kMainResourceSkippedBecauseMatchedWithAllowedScriptList = 6,
    // kBypassFetchHandlerForAllOnlyIfServiceWorkerNotStarted_Status_Stop = 7,
    // kBypassFetchHandlerForAllOnlyIfServiceWorkerNotStarted_Status_Starting =
    // 8,

    kMaxValue = kSkippedForEmptyFetchHandler,
  };

  // Default duration to start fetch handler when service worker is started in
  // `TaskRunner::PostDelayTask`.
  static constexpr int kStartServiceWorkerForEmptyFetchHandlerDurationInMs = 50;

  static void SetStartServiceWorkerForEmptyFetchHandlerDurationForTesting(
      int duration);

  // If |skip_service_worker| is true, service workers are bypassed for
  // request interception.
  ServiceWorkerControlleeRequestHandler(
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::WeakPtr<ServiceWorkerClient> service_worker_client,
      network::mojom::RequestDestination destination,
      bool skip_service_worker,
      FrameTreeNodeId frame_tree_node_id,
      ServiceWorkerAccessedCallback service_worker_accessed_callback);

  ServiceWorkerControlleeRequestHandler(
      const ServiceWorkerControlleeRequestHandler&) = delete;
  ServiceWorkerControlleeRequestHandler& operator=(
      const ServiceWorkerControlleeRequestHandler&) = delete;

  ~ServiceWorkerControlleeRequestHandler();

  // This is called only once. On redirects, a new instance of this
  // class is created.
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_request,
      const blink::StorageKey& storage_key,
      BrowserContext* browser_context,
      NavigationLoaderInterceptor::LoaderCallback loader_callback,
      NavigationLoaderInterceptor::FallbackCallback fallback_callback);

  // Exposed for testing.
  ServiceWorkerMainResourceLoader* loader() {
    return loader_wrapper_ ? loader_wrapper_->get() : nullptr;
  }

  void set_parent_client_uuid(std::string uuid) {
    parent_client_uuid_ = std::move(uuid);
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerControlleeRequestHandlerTest,
                           ActivateWaitingVersion);

  // Does all initialization of |service_worker_client_| for a request.
  void InitializeServiceWorkerClient(
      const network::ResourceRequest& tentative_request,
      const blink::StorageKey& storage_key);

  void ContinueWithRegistration(
      // True when FindRegistrationForClientUrl() is called for navigation.
      bool is_for_navigation,
      base::TimeTicks find_registration_start_time,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void ContinueWithActivatedVersion(
      scoped_refptr<ServiceWorkerRegistration> registration,
      scoped_refptr<ServiceWorkerVersion> version,
      base::TimeTicks find_registration_start_time);

  // For forced update.
  void DidUpdateRegistration(
      scoped_refptr<ServiceWorkerRegistration> original_registration,
      blink::ServiceWorkerStatusCode status,
      const std::string& status_message,
      int64_t registration_id);
  void OnUpdatedVersionStatusChanged(
      scoped_refptr<ServiceWorkerRegistration> registration,
      scoped_refptr<ServiceWorkerVersion> version);

  void CompleteWithoutLoader();
  void CreateLoaderAndStartRequest(
      base::TimeTicks find_registration_start_time);

  // Schedules a service worker update to occur shortly after the page and its
  // initial subresources load, if this handler was for a navigation.
  void MaybeScheduleUpdate();

  // Runs service worker if not running.
  static void MaybeStartServiceWorker(
      scoped_refptr<ServiceWorkerVersion> active_version,
      ServiceWorkerMetrics::EventType event_type,
      uintptr_t trace_id);

  // Runs after ServiceWorker has started.
  // Normally ServiceWorker starts before dispatching the main resource request,
  // but if the browser handles an empty fetch handler, this runs after the
  // service worker starts.
  static void DidStartWorker(uintptr_t trace_id,
                             blink::ServiceWorkerStatusCode status);

  int GetServiceWorkerForEmptyFetchHandlerDurationMs();

  static std::optional<int>
      start_service_worker_for_empty_fetch_handler_duration_for_testing_;

  const base::WeakPtr<ServiceWorkerContextCore> context_;
  const base::WeakPtr<ServiceWorkerClient> service_worker_client_;
  const network::mojom::RequestDestination destination_;

  // If true, service workers are bypassed for request interception.
  const bool skip_service_worker_;

  std::unique_ptr<ServiceWorkerMainResourceLoaderWrapper> loader_wrapper_;
  GURL stripped_url_;
  blink::StorageKey storage_key_;
  bool force_update_started_;
  const FrameTreeNodeId frame_tree_node_id_;
  std::string parent_client_uuid_;

  NavigationLoaderInterceptor::LoaderCallback loader_callback_;
  NavigationLoaderInterceptor::FallbackCallback fallback_callback_;

  ServiceWorkerAccessedCallback service_worker_accessed_callback_;

  base::WeakPtrFactory<ServiceWorkerControlleeRequestHandler> weak_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTROLLEE_REQUEST_HANDLER_H_
