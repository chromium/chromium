// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_PROXY_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_PROXY_H_

#include <memory>
#include <set>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/mojom/background_sync/background_sync.mojom.h"
#include "url/origin.h"

namespace content {

class BrowserContext;
class ServiceWorkerContextWrapper;

// This class is used to take messages from BackgroundSyncManager and pass them
// on BackgroundSyncController. It is owned by the BackgroundSyncManager and
// lives on the UI thread.
//
// TODO(crbug.com/40568315): This class was previously needed because
// BackgroundSyncManager and BackgroundSyncController were on different threads.
// It should no longer be needed.
class CONTENT_EXPORT BackgroundSyncProxy {
 public:
  explicit BackgroundSyncProxy(
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);

  BackgroundSyncProxy(const BackgroundSyncProxy&) = delete;
  BackgroundSyncProxy& operator=(const BackgroundSyncProxy&) = delete;

  virtual ~BackgroundSyncProxy();

  virtual void ScheduleDelayedProcessing(
      blink::mojom::BackgroundSyncType sync_type,
      base::TimeDelta delay,
      base::OnceClosure delayed_task);
  void CancelDelayedProcessing(blink::mojom::BackgroundSyncType sync_type);
  void SendSuspendedPeriodicSyncOrigins(
      std::set<url::Origin> suspended_origins);
  void SendRegisteredPeriodicSyncOrigins(
      std::set<url::Origin> registered_origins);
  void AddToTrackedOrigins(url::Origin origin);
  void RemoveFromTrackedOrigins(url::Origin origin);

 private:
  BrowserContext* browser_context();

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_PROXY_H_
