// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/background_sync_test_util.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "content/browser/background_sync/background_sync_context_impl.h"
#include "content/browser/background_sync/background_sync_manager.h"
#include "content/browser/background_sync/background_sync_network_observer.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/network_change_notifier.h"

namespace content {
namespace background_sync_test_util {

namespace {

StoragePartitionImpl* GetStoragePartition(WebContents* web_contents) {
  return static_cast<StoragePartitionImpl*>(
      web_contents->GetBrowserContext()->GetStoragePartition(
          web_contents->GetSiteInstance()));
}

}  // namespace

// static
void SetIgnoreNetworkChanges(bool ignore) {
  BackgroundSyncNetworkObserver::SetIgnoreNetworkChangesForTests(ignore);
}

// static
void SetOnline(WebContents* web_contents, bool online) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BackgroundSyncContextImpl* sync_context =
      GetStoragePartition(web_contents)->GetBackgroundSyncContext();
  BackgroundSyncManager* sync_manager = sync_context->background_sync_manager();
  BackgroundSyncNetworkObserver* network_observer =
      sync_manager->GetNetworkObserverForTesting();
  if (online) {
    network_observer->NotifyManagerIfConnectionChangedForTesting(
        network::mojom::ConnectionType::CONNECTION_WIFI);
  } else {
    network_observer->NotifyManagerIfConnectionChangedForTesting(
        network::mojom::ConnectionType::CONNECTION_NONE);
  }

  base::RunLoop().RunUntilIdle();
}

}  // namespace background_sync_test_util

}  // namespace content
