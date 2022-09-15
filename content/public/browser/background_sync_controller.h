// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BACKGROUND_SYNC_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_BACKGROUND_SYNC_CONTROLLER_H_

#include <stdint.h>

#include <set>

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/background_sync_registration.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/background_sync/background_sync.mojom-shared.h"

namespace url {
class Origin;
}  // namespace url

namespace content {

struct BackgroundSyncParameters;

// An interface that the Background Sync API uses to access services from the
// embedder. Must only be used on the UI thread.
class CONTENT_EXPORT BackgroundSyncController {
 public:
  class BackgroundSyncEventKeepAlive {
   public:
    virtual ~BackgroundSyncEventKeepAlive() = default;

   protected:
    BackgroundSyncEventKeepAlive() = default;
  };

  virtual ~BackgroundSyncController() {}

  // This function allows the controller to alter the parameters used by
  // background sync. Note that disable can be overridden from false to true
  // but overrides from true to false will be ignored.
  virtual void GetParameterOverrides(BackgroundSyncParameters* parameters) {}

  // Notification that a service worker registration with origin |origin| just
  // registered a one-shot background sync event. Also includes information
  // about the registration.
  virtual void NotifyOneShotBackgroundSyncRegistered(const url::Origin& origin,
                                                     bool can_fire,
                                                     bool is_reregistered) {}

  // Notification that a service worker registration with origin |origin| just
  // registered a periodic background sync event. Also includes information
  // about the registration.
  virtual void NotifyPeriodicBackgroundSyncRegistered(const url::Origin& origin,
                                                      int min_interval,
                                                      bool is_reregistered) {}

  // Notification that a service worker registration with origin |origin| just
  // completed a one-shot background sync registration. Also include the
  // |status_code| the registration finished with, the number of attempts, and
  // the max allowed number of attempts.
  virtual void NotifyOneShotBackgroundSyncCompleted(
      const url::Origin& origin,
      blink::ServiceWorkerStatusCode status_code,
      int num_attempts,
      int max_attempts) {}

  // Notification that a service worker registration with origin |origin| just
  // completed a periodic background sync registration. Also include the
  // |status_code| the registration finished with, the number of attempts, and
  // the max allowed number of attempts.
  virtual void NotifyPeriodicBackgroundSyncCompleted(
      const url::Origin& origin,
      blink::ServiceWorkerStatusCode status_code,
      int num_attempts,
      int max_attempts) {}

  // Schedules a background task with delay |delay| to wake up the browser to
  // process Background Sync registrations of type |sync_type|.
  virtual void ScheduleBrowserWakeUpWithDelay(
      blink::mojom::BackgroundSyncType sync_type,
      base::TimeDelta delay) {}

  // Cancel the background task that wakes the browser up to process Background
  // Sync registrations of type |sync_type|.
  virtual void CancelBrowserWakeup(blink::mojom::BackgroundSyncType sync_type) {
  }

  // Calculates the delay after which the next sync event should be fired
  // for a BackgroundSync registration. The delay is based on the sync_type of
  // the |registration|, the |parameters| for the feature, the soonest time
  // a (periodic)sync event is scheduled to fire for this origin, and other
  // browser-specific considerations.
  virtual base::TimeDelta GetNextEventDelay(
      const BackgroundSyncRegistration& registration,
      content::BackgroundSyncParameters* parameters,
      base::TimeDelta time_till_soonest_scheduled_event_for_origin) = 0;

  // Keeps the browser alive to allow a one-shot Background Sync registration
  // to finish firing one sync event.
  virtual std::unique_ptr<BackgroundSyncEventKeepAlive>
  CreateBackgroundSyncEventKeepAlive() = 0;

  // Updates its internal list of origins for which we have suspended periodic
  // Background Sync registrations. This is compiled from each
  // BackgroundSyncManager when they are initialized. This list used to ignore
  // changes concerning origins we don't care about.
  virtual void NoteSuspendedPeriodicSyncOrigins(
      std::set<url::Origin> suspended_origins) = 0;

  // Updates its internal list of origins for which we have periodic Background
  // Sync registrations. This is compiled from each BackgroundSyncManager when
  // they are initialized and subsequently kept up to date. This list is used to
  // respond to changes in site settings affecting the feature.
  virtual void NoteRegisteredPeriodicSyncOrigins(
      std::set<url::Origin> registered_origins) = 0;

  // Adds |origin| to its internal list of origins for which we have periodic
  // Background Sync registrations. This list used to unregister periodic
  // Background Sync when the controller deems necessary, for instance, upon
  // revocation of permission.
  virtual void AddToTrackedOrigins(const url::Origin& origin) = 0;

  // Removes |origin| from its internal list of origins for which we have
  // periodic Background Sync registrations. This list used to unregister
  // periodic Background Sync when the controller deems necessary, for instance,
  //  upon revocation of permission.
  virtual void RemoveFromTrackedOrigins(const url::Origin& origin) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACKGROUND_SYNC_CONTROLLER_H_
