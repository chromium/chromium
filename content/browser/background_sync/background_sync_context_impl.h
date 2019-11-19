// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTEXT_IMPL_H_

#include <map>
#include <memory>

#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/background_sync_context.h"
#include "third_party/blink/public/mojom/background_sync/background_sync.mojom.h"

namespace content {

class BackgroundSyncManager;
class DevToolsBackgroundServicesContextImpl;
class OneShotBackgroundSyncServiceImpl;
class PeriodicBackgroundSyncServiceImpl;
class ServiceWorkerContextWrapper;

// One instance of this exists per StoragePartition, and services multiple child
// processes/origins. Most logic is delegated to the owned BackgroundSyncManager
// instance, which is only accessed on the service worker core thread
// (ServiceWorkerContext::GetCoreThreadId()).
//
// TODO(crbug.com/824858): Update this comment when service worker core thread
// becomes the UI thread, and simplify/remove the delegating.
class CONTENT_EXPORT BackgroundSyncContextImpl
    : public BackgroundSyncContext,
      public base::RefCountedDeleteOnSequence<BackgroundSyncContextImpl> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  BackgroundSyncContextImpl();

  // Init and Shutdown are for use on the UI thread when the
  // StoragePartition is being setup and torn down.
  void Init(
      const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context,
      const scoped_refptr<DevToolsBackgroundServicesContextImpl>&
          devtools_context);

  // Shutdown must be called before deleting this. Call on the UI thread.
  void Shutdown();

  // Create a OneShotBackgroundSyncServiceImpl that is owned by this. Call on
  // the UI thread.
  void CreateOneShotSyncService(
      mojo::PendingReceiver<blink::mojom::OneShotBackgroundSyncService>
          receiver);

  // Create a PeriodicBackgroundSyncServiceImpl that is owned by this. Call on
  // the UI thread.
  void CreatePeriodicSyncService(
      mojo::PendingReceiver<blink::mojom::PeriodicBackgroundSyncService>
          receiver);

  // Called by *BackgroundSyncServiceImpl objects so that they can
  // be deleted. Call on the service worker core thread.
  void OneShotSyncServiceHadConnectionError(
      OneShotBackgroundSyncServiceImpl* service);
  void PeriodicSyncServiceHadConnectionError(
      PeriodicBackgroundSyncServiceImpl* service);

  // Call on the service worker core thread.
  BackgroundSyncManager* background_sync_manager() const;

  // BackgroundSyncContext implementation.
  void FireBackgroundSyncEvents(blink::mojom::BackgroundSyncType sync_type,
                                base::OnceClosure done_closure) override;
  void GetSoonestWakeupDelta(
      blink::mojom::BackgroundSyncType sync_type,
      base::Time last_browser_wakeup_for_periodic_sync,
      base::OnceCallback<void(base::TimeDelta)> callback) override;
  void RevivePeriodicBackgroundSyncRegistrations(url::Origin origin) override;

 protected:
  friend class base::RefCountedDeleteOnSequence<BackgroundSyncContextImpl>;
  friend class base::DeleteHelper<BackgroundSyncContextImpl>;
  ~BackgroundSyncContextImpl() override;

  void set_background_sync_manager_for_testing(
      std::unique_ptr<BackgroundSyncManager> manager);
  void set_wakeup_delta_for_testing(blink::mojom::BackgroundSyncType sync_type,
                                    base::TimeDelta wakeup_delta);

 private:
  friend class OneShotBackgroundSyncServiceImplTest;
  friend class PeriodicBackgroundSyncServiceImplTest;
  friend class BackgroundSyncLauncherTest;
  friend class BackgroundSyncManagerTest;

  void FireBackgroundSyncEventsOnCoreThread(
      blink::mojom::BackgroundSyncType sync_type,
      base::OnceClosure done_closure);
  void DidFireBackgroundSyncEventsOnCoreThread(base::OnceClosure done_closure);
  virtual void CreateBackgroundSyncManager(
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      scoped_refptr<DevToolsBackgroundServicesContextImpl> devtools_context);

  void CreateOneShotSyncServiceOnCoreThread(
      mojo::PendingReceiver<blink::mojom::OneShotBackgroundSyncService>
          receiver);
  void CreatePeriodicSyncServiceOnCoreThread(
      mojo::PendingReceiver<blink::mojom::PeriodicBackgroundSyncService>
          receiver);

  void ShutdownOnCoreThread();

  base::TimeDelta GetSoonestWakeupDeltaOnCoreThread(
      blink::mojom::BackgroundSyncType sync_type,
      base::Time last_browser_wakeup_for_periodic_sync);
  void DidGetSoonestWakeupDelta(
      base::OnceCallback<void(base::TimeDelta)> callback,
      base::TimeDelta soonest_wakeup_delta);

  void RevivePeriodicBackgroundSyncRegistrationsOnCoreThread(
      url::Origin origin);

  // The services are owned by this. They're either deleted
  // during ShutdownOnCoreThread() or when the channel is closed via
  // *ServiceHadConnectionError. Only accessed on the core thread.
  std::set<std::unique_ptr<OneShotBackgroundSyncServiceImpl>,
           base::UniquePtrComparator>
      one_shot_sync_services_;
  std::set<std::unique_ptr<PeriodicBackgroundSyncServiceImpl>,
           base::UniquePtrComparator>
      periodic_sync_services_;

  // Only accessed on the core thread.
  std::unique_ptr<BackgroundSyncManager> background_sync_manager_;

  std::map<blink::mojom::BackgroundSyncType, base::TimeDelta>
      test_wakeup_delta_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncContextImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTEXT_IMPL_H_
