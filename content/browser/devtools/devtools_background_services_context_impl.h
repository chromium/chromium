// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_BACKGROUND_SERVICES_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_BACKGROUND_SERVICES_CONTEXT_IMPL_H_

#include <array>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "content/browser/devtools/devtools_background_services.pb.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/content_export.h"
#include "content/public/browser/devtools_background_services_context.h"

namespace content {

class BrowserContext;
class ServiceWorkerContextWrapper;

// This class is responsible for persisting the debugging events for the
// relevant Web Platform Features. The contexts of the feature will have a
// reference to this, and perform the logging operation.
// This class is also responsible for reading back the data to the DevTools
// client, as the protocol handler will have access to an instance of the
// context.
//
// Lives on the UI thread.
class CONTENT_EXPORT DevToolsBackgroundServicesContextImpl
    : public DevToolsBackgroundServicesContext {
 public:
  using GetLoggedBackgroundServiceEventsCallback = base::OnceCallback<void(
      std::vector<devtools::proto::BackgroundServiceEvent>)>;

  class EventObserver : public base::CheckedObserver {
   public:
    // Notifies observers of the logged event.
    virtual void OnEventReceived(
        const devtools::proto::BackgroundServiceEvent& event) = 0;
    virtual void OnRecordingStateChanged(
        bool should_record,
        devtools::proto::BackgroundService service) = 0;
  };

  DevToolsBackgroundServicesContextImpl(
      BrowserContext* browser_context,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);
  ~DevToolsBackgroundServicesContextImpl() override;

  DevToolsBackgroundServicesContextImpl(
      const DevToolsBackgroundServicesContextImpl&) = delete;
  DevToolsBackgroundServicesContextImpl& operator=(
      const DevToolsBackgroundServicesContextImpl&) = delete;

  void AddObserver(EventObserver* observer);
  void RemoveObserver(const EventObserver* observer);

  // DevToolsBackgroundServicesContext overrides:
  bool IsRecording(DevToolsBackgroundService service) override;
  void LogBackgroundServiceEvent(
      uint64_t service_worker_registration_id,
      blink::StorageKey storage_key,
      DevToolsBackgroundService service,
      const std::string& event_name,
      const std::string& instance_id,
      const std::map<std::string, std::string>& event_metadata) override;

  // Helper function for public override. Can be used directly.
  bool IsRecording(devtools::proto::BackgroundService service);

  // Enables recording mode for |service|. This is capped at 3 days in case
  // developers forget to switch it off.
  void StartRecording(devtools::proto::BackgroundService service);

  // Disables recording mode for |service|.
  void StopRecording(devtools::proto::BackgroundService service);

  // Queries all logged events for |service| and returns them in sorted order
  // (by timestamp). |callback| is called with an empty vector if there was an
  // error.
  void GetLoggedBackgroundServiceEvents(
      devtools::proto::BackgroundService service,
      GetLoggedBackgroundServiceEventsCallback callback);

  // Clears all logged events related to |service|.
  void ClearLoggedBackgroundServiceEvents(
      devtools::proto::BackgroundService service);

 private:
  friend class DevToolsBackgroundServicesContextTest;

  // Whether |service| has an expiration time and it was exceeded.
  bool IsRecordingExpired(devtools::proto::BackgroundService service);

  void DidGetUserData(
      GetLoggedBackgroundServiceEventsCallback callback,
      const std::vector<std::pair<int64_t, std::string>>& user_data,
      blink::ServiceWorkerStatusCode status);

  void NotifyEventObservers(
      const devtools::proto::BackgroundServiceEvent& event);

  void OnRecordingTimeExpired(devtools::proto::BackgroundService service);

  const raw_ref<BrowserContext> browser_context_;
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  // Maps from the background service to the time up until the events can be
  // recorded. The BackgroundService enum is used as the index.
  std::array<base::Time, devtools::proto::BackgroundService::COUNT>
      expiration_times_;

  base::ObserverList<EventObserver> observers_;

  base::WeakPtrFactory<DevToolsBackgroundServicesContextImpl> weak_ptr_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_BACKGROUND_SERVICES_CONTEXT_IMPL_H_
