// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_CONTEXT_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_CONTEXT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "content/browser/background_fetch/background_fetch_delegate_proxy.h"
#include "content/browser/background_fetch/background_fetch_event_dispatcher.h"
#include "content/browser/background_fetch/storage/get_initialization_data_task.h"
#include "content/browser/devtools/devtools_background_services_context_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"

namespace storage {
class QuotaManagerProxy;
}  // namespace storage

namespace content {

class BackgroundFetchDataManager;
class BackgroundFetchRegistrationId;
class BackgroundFetchRegistrationNotifier;
class BackgroundFetchRequestMatchParams;
class BackgroundFetchScheduler;
class BrowserContext;
class CacheStorageContextImpl;
class ServiceWorkerContextWrapper;

// The BackgroundFetchContext is the central moderator of ongoing background
// fetch requests from the Mojo service and from other callers.
// Background Fetch requests function similarly to normal fetches except that
// they are persistent across Chromium or service worker shutdown.
//
// Deleted on the service worker core thread.
// TODO(crbug.com/824858): Make this single-threaded after the service worker
// core thread moves to the UI thread.
class CONTENT_EXPORT BackgroundFetchContext
    : public base::RefCountedDeleteOnSequence<BackgroundFetchContext> {
 public:
  // The BackgroundFetchContext will watch the ServiceWorkerContextWrapper so
  // that it can respond to service worker events such as unregister.
  BackgroundFetchContext(
      BrowserContext* browser_context,
      const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context,
      const scoped_refptr<CacheStorageContextImpl>& cache_storage_context,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      scoped_refptr<DevToolsBackgroundServicesContextImpl> devtools_context);

  void InitializeOnCoreThread();

  // Called by the StoragePartitionImpl destructor.
  void Shutdown();

  // Gets the active Background Fetch registration identified by |developer_id|
  // for the given |service_worker_id| and |origin|. The |callback| will be
  // invoked with the registration when it has been retrieved.
  void GetRegistration(
      int64_t service_worker_registration_id,
      const url::Origin& origin,
      const std::string& developer_id,
      blink::mojom::BackgroundFetchService::GetRegistrationCallback callback);

  // Gets all the Background Fetch registration |developer_id|s for a Service
  // Worker and invokes |callback| with that list.
  void GetDeveloperIdsForServiceWorker(
      int64_t service_worker_registration_id,
      const url::Origin& origin,
      blink::mojom::BackgroundFetchService::GetDeveloperIdsCallback callback);

  // Starts a Background Fetch for the |registration_id|. The |requests| will be
  // asynchronously fetched. The |callback| will be invoked when the fetch has
  // been registered, or an error occurred that prevents it from doing so.
  void StartFetch(const BackgroundFetchRegistrationId& registration_id,
                  std::vector<blink::mojom::FetchAPIRequestPtr> requests,
                  blink::mojom::BackgroundFetchOptionsPtr options,
                  const SkBitmap& icon,
                  blink::mojom::BackgroundFetchUkmDataPtr ukm_data,
                  int render_frame_tree_node_id,
                  const WebContents::Getter& wc_getter,
                  blink::mojom::BackgroundFetchService::FetchCallback callback);

  // Gets display size for the icon for Background Fetch UI.
  void GetIconDisplaySize(
      blink::mojom::BackgroundFetchService::GetIconDisplaySizeCallback
          callback);

  // Matches Background Fetch requests from the cache and returns responses.
  void MatchRequests(
      const BackgroundFetchRegistrationId& registration_id,
      std::unique_ptr<BackgroundFetchRequestMatchParams> match_params,
      blink::mojom::BackgroundFetchRegistrationService::MatchRequestsCallback
          callback);

  // Aborts the Background Fetch for the |registration_id|. The callback will be
  // invoked with INVALID_ID if the registration has already completed or
  // aborted, STORAGE_ERROR if an I/O error occurs, or NONE for success.
  void Abort(
      const BackgroundFetchRegistrationId& registration_id,
      blink::mojom::BackgroundFetchRegistrationService::AbortCallback callback);

  // Registers the |observer| to be notified of progress events for the
  // registration identified by |unique_id| whenever they happen. The observer
  // will unregister itself when the Mojo endpoint goes away.
  void AddRegistrationObserver(
      const std::string& unique_id,
      mojo::PendingRemote<blink::mojom::BackgroundFetchRegistrationObserver>
          observer);

  // Updates the title or icon of the Background Fetch identified by
  // |registration_id|. The |callback| will be invoked when the title has been
  // updated, or an error occurred that prevents it from doing so.
  // The icon is wrapped in an optional. If the optional has a value then the
  // internal |icon| is guarnteed to be not null.
  void UpdateUI(
      const BackgroundFetchRegistrationId& registration_id,
      const base::Optional<std::string>& title,
      const base::Optional<SkBitmap>& icon,
      blink::mojom::BackgroundFetchRegistrationService::UpdateUICallback
          callback);

  BackgroundFetchRegistrationNotifier* registration_notifier() const {
    return registration_notifier_.get();
  }

  base::WeakPtr<BackgroundFetchContext> GetWeakPtr();

 private:
  using GetPermissionCallback =
      base::OnceCallback<void(BackgroundFetchPermission)>;

  FRIEND_TEST_ALL_PREFIXES(BackgroundFetchServiceTest,
                           JobsInitializedOnBrowserRestart);
  friend class BackgroundFetchServiceTest;
  friend class BackgroundFetchJobControllerTest;
  friend class base::DeleteHelper<BackgroundFetchContext>;
  friend class base::RefCountedDeleteOnSequence<BackgroundFetchContext>;

  ~BackgroundFetchContext();

  void ShutdownOnCoreThread();

  // Called when an existing registration has been retrieved from the data
  // manager. If the registration does not exist then |registration| is nullptr.
  void DidGetRegistration(
      blink::mojom::BackgroundFetchService::GetRegistrationCallback callback,
      blink::mojom::BackgroundFetchError error,
      BackgroundFetchRegistrationId registration_id,
      blink::mojom::BackgroundFetchRegistrationDataPtr registration_data);

  // Called when a new registration has been created by the data manager.
  void DidCreateRegistration(
      const BackgroundFetchRegistrationId& registration_id,
      blink::mojom::BackgroundFetchError error,
      blink::mojom::BackgroundFetchRegistrationDataPtr registration_data);

  // Called when the sequence of matching settled fetches have been received
  // from storage, and |callback| can be invoked to pass these on to the
  // renderer.
  void DidGetMatchingRequests(
      const std::string& unique_id,
      blink::mojom::BackgroundFetchRegistrationService::MatchRequestsCallback
          callback,
      blink::mojom::BackgroundFetchError error,
      std::vector<blink::mojom::BackgroundFetchSettledFetchPtr>
          settled_fetches);

  // Called when the data manager finishes getting the initialization data.
  void DidGetInitializationData(
      blink::mojom::BackgroundFetchError error,
      std::vector<background_fetch::BackgroundFetchInitializationData>
          initialization_data);

  // Switches out |data_manager_| with a DataManager configured for testing
  // environments. Must be called directly after the constructor.
  void SetDataManagerForTesting(
      std::unique_ptr<BackgroundFetchDataManager> data_manager);

  // Callback for GetPermissionForOrigin.
  void DidGetPermission(const BackgroundFetchRegistrationId& registration_id,
                        std::vector<blink::mojom::FetchAPIRequestPtr> requests,
                        blink::mojom::BackgroundFetchOptionsPtr options,
                        const SkBitmap& icon,
                        blink::mojom::BackgroundFetchUkmDataPtr ukm_data,
                        int frame_tree_node_id,
                        BackgroundFetchPermission permission);

  // |this| is owned, indirectly, by the BrowserContext.
  BrowserContext* browser_context_;

  std::unique_ptr<BackgroundFetchDataManager> data_manager_;
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  scoped_refptr<DevToolsBackgroundServicesContextImpl> devtools_context_;
  std::unique_ptr<BackgroundFetchRegistrationNotifier> registration_notifier_;
  BackgroundFetchDelegateProxy delegate_proxy_;
  std::unique_ptr<BackgroundFetchScheduler> scheduler_;

  // Map from BackgroundFetchRegistrationIds to FetchCallbacks for active
  // fetches. Must be destroyed before |data_manager_| and
  // |registration_notifier_|. Since FetchCallback is a OnceCallback, please
  // erase the map entry once the calback has been invoked.
  std::map<BackgroundFetchRegistrationId,
           blink::mojom::BackgroundFetchService::FetchCallback>
      fetch_callbacks_;

  base::WeakPtrFactory<BackgroundFetchContext> weak_factory_{
      this};  // Must be last.

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_CONTEXT_H_
