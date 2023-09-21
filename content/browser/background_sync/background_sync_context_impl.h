// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTEXT_IMPL_H_

#include <map>
#include <memory>

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
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
class RenderProcessHost;
class ServiceWorkerContextWrapper;

// One instance of this exists per StoragePartition, and services multiple child
// processes/origins. Most logic is delegated to the owned BackgroundSyncManager
// instance. Lives on the UI thread.
//
// TODO(falken): Consider removing this delegating. Previously these were
// separate classes because this lived on the IO and UI thread until
// https://crbug.com/824858.
class CONTENT_EXPORT BackgroundSyncContextImpl
    : public BackgroundSyncContext,
      public base::RefCounted<BackgroundSyncContextImpl> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  BackgroundSyncContextImpl();

  BackgroundSyncContextImpl(const BackgroundSyncContextImpl&) = delete;
  BackgroundSyncContextImpl& operator=(const BackgroundSyncContextImpl&) =
      delete;

  // Called when StoragePartition is being setup.
  void Init(
      const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context,
      DevToolsBackgroundServicesContextImpl& devtools_context);

  // Called when StoragePartition is being torn down. Must be called before
  // deleting `this`.
  void Shutdown();

  // Creates a OneShotBackgroundSyncServiceImpl that is owned by `this`.
  void CreateOneShotSyncService(
      const url::Origin& origin,
      RenderProcessHost* render_process_host,
      mojo::PendingReceiver<blink::mojom::OneShotBackgroundSyncService>
          receiver);

  // Creates a PeriodicBackgroundSyncServiceImpl that is owned by `this`.
  void CreatePeriodicSyncService(
      const url::Origin& origin,
      RenderProcessHost* render_process_host,
      mojo::PendingReceiver<blink::mojom::PeriodicBackgroundSyncService>
          receiver);

  // Called by *BackgroundSyncServiceImpl objects so that they can be deleted.
  void OneShotSyncServiceHadConnectionError(
      OneShotBackgroundSyncServiceImpl* service);
  void PeriodicSyncServiceHadConnectionError(
      PeriodicBackgroundSyncServiceImpl* service);

  BackgroundSyncManager* background_sync_manager() const;

  // BackgroundSyncContext implementation.
  void FireBackgroundSyncEvents(blink::mojom::BackgroundSyncType sync_type,
                                base::OnceClosure done_closure) override;
  void RevivePeriodicBackgroundSyncRegistrations(url::Origin origin) override;
  void UnregisterPeriodicSyncForOrigin(url::Origin origin) override;

  // Gets the soonest time delta from now, when the browser should be woken up
  // to fire any Background Sync events.
  base::TimeDelta GetSoonestWakeupDelta(
      blink::mojom::BackgroundSyncType sync_type,
      base::Time last_browser_wakeup_for_periodic_sync);

 protected:
  friend class base::RefCounted<BackgroundSyncContextImpl>;
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

  virtual void CreateBackgroundSyncManager(
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      DevToolsBackgroundServicesContextImpl& devtools_context);

  // The services are owned by this. They're either deleted during Shutdown()
  // or when the channel is closed via *ServiceHadConnectionError.
  std::set<std::unique_ptr<OneShotBackgroundSyncServiceImpl>,
           base::UniquePtrComparator>
      one_shot_sync_services_;
  std::set<std::unique_ptr<PeriodicBackgroundSyncServiceImpl>,
           base::UniquePtrComparator>
      periodic_sync_services_;

  std::unique_ptr<BackgroundSyncManager> background_sync_manager_;

  std::map<blink::mojom::BackgroundSyncType, base::TimeDelta>
      test_wakeup_delta_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTEXT_IMPL_H_
