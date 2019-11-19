// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONTENT_INDEX_CONTENT_INDEX_DATABASE_H_
#define CONTENT_BROWSER_CONTENT_INDEX_CONTENT_INDEX_DATABASE_H_

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/content_export.h"
#include "content/public/browser/content_index_context.h"
#include "content/public/browser/content_index_provider.h"
#include "third_party/blink/public/mojom/content_index/content_index.mojom.h"

class GURL;

namespace url {
class Origin;
}  // namespace url

namespace content {

namespace proto {
class SerializedIcons;
}  // namespace proto

class BrowserContext;

// Handles interacting with the Service Worker Database for Content Index
// entries. This is owned by the ContentIndexContext.
class CONTENT_EXPORT ContentIndexDatabase {
 public:
  ContentIndexDatabase(
      BrowserContext* browser_context,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);
  ~ContentIndexDatabase();

  void AddEntry(int64_t service_worker_registration_id,
                const url::Origin& origin,
                blink::mojom::ContentDescriptionPtr description,
                const std::vector<SkBitmap>& icons,
                const GURL& launch_url,
                blink::mojom::ContentIndexService::AddCallback callback);

  void DeleteEntry(int64_t service_worker_registration_id,
                   const url::Origin& origin,
                   const std::string& entry_id,
                   blink::mojom::ContentIndexService::DeleteCallback callback);

  void GetDescriptions(
      int64_t service_worker_registration_id,
      blink::mojom::ContentIndexService::GetDescriptionsCallback callback);

  // Gets the icon for |description_id| and invokes |callback| on the UI
  // thread.
  void GetIcons(int64_t service_worker_registration_id,
                const std::string& description_id,
                ContentIndexContext::GetIconsCallback callback);

  // Returns all registered entries.
  void GetAllEntries(ContentIndexContext::GetAllEntriesCallback callback);

  // Returns the specified entry.
  void GetEntry(int64_t service_worker_registration_id,
                const std::string& description_id,
                ContentIndexContext::GetEntryCallback callback);

  // Deletes the entry and dispatches an event.
  void DeleteItem(int64_t service_worker_registration_id,
                  const url::Origin& origin,
                  const std::string& description_id);

  // Called when the storage partition is shutting down.
  void Shutdown();

 private:
  FRIEND_TEST_ALL_PREFIXES(ContentIndexDatabaseTest,
                           BlockedOriginsCannotRegisterContent);
  FRIEND_TEST_ALL_PREFIXES(ContentIndexDatabaseTest, UmaRecorded);

  // public method service worker core thread counterparts.
  void AddEntryOnCoreThread(
      int64_t service_worker_registration_id,
      const url::Origin& origin,
      blink::mojom::ContentDescriptionPtr description,
      const std::vector<SkBitmap>& icons,
      const GURL& launch_url,
      blink::mojom::ContentIndexService::AddCallback callback);
  void DeleteEntryOnCoreThread(
      int64_t service_worker_registration_id,
      const url::Origin& origin,
      const std::string& entry_id,
      blink::mojom::ContentIndexService::DeleteCallback callback);
  void GetDescriptionsOnCoreThread(
      int64_t service_worker_registration_id,
      blink::mojom::ContentIndexService::GetDescriptionsCallback callback);
  void GetIconsOnCoreThread(int64_t service_worker_registration_id,
                            const std::string& description_id,
                            ContentIndexContext::GetIconsCallback callback);
  void GetAllEntriesOnCoreThread(
      ContentIndexContext::GetAllEntriesCallback callback);
  void GetEntryOnCoreThread(int64_t service_worker_registration_id,
                            const std::string& description_id,
                            ContentIndexContext::GetEntryCallback callback);

  // Add Callbacks.
  void DidSerializeIcons(
      int64_t service_worker_registration_id,
      const url::Origin& origin,
      blink::mojom::ContentDescriptionPtr description,
      const GURL& launch_url,
      std::unique_ptr<proto::SerializedIcons> serialized_icons,
      blink::mojom::ContentIndexService::AddCallback callback);
  void DidAddEntry(blink::mojom::ContentIndexService::AddCallback callback,
                   ContentIndexEntry entry,
                   blink::ServiceWorkerStatusCode status);

  // Delete Callbacks.
  void DidDeleteEntry(
      int64_t service_worker_registration_id,
      const url::Origin& origin,
      const std::string& entry_id,
      blink::mojom::ContentIndexService::DeleteCallback callback,
      blink::ServiceWorkerStatusCode status);

  // GetDescriptions Callbacks.
  void DidGetDescriptions(
      int64_t service_worker_registration_id,
      blink::mojom::ContentIndexService::GetDescriptionsCallback callback,
      const std::vector<std::string>& data,
      blink::ServiceWorkerStatusCode status);

  // GetIcons Callbacks.
  void DidGetSerializedIcons(int64_t service_worker_registration_id,
                             ContentIndexContext::GetIconsCallback callback,
                             const std::vector<std::string>& data,
                             blink::ServiceWorkerStatusCode status);
  void DidDeserializeIcons(ContentIndexContext::GetIconsCallback callback,
                           std::unique_ptr<std::vector<SkBitmap>> icons);

  // GetEntries Callbacks.
  void DidGetEntries(
      ContentIndexContext::GetAllEntriesCallback callback,
      const std::vector<std::pair<int64_t, std::string>>& user_data,
      blink::ServiceWorkerStatusCode status);

  // GetEntry Callbacks.
  void DidGetEntry(int64_t service_worker_registration_id,
                   ContentIndexContext::GetEntryCallback callback,
                   const std::vector<std::string>& data,
                   blink::ServiceWorkerStatusCode status);

  // DeleteItem Callbacks.
  void DidDeleteItem(int64_t service_worker_registration_id,
                     const url::Origin& origin,
                     const std::string& description_id,
                     blink::mojom::ContentIndexError error);
  void StartActiveWorkerForDispatch(
      const std::string& description_id,
      blink::ServiceWorkerStatusCode service_worker_status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DeliverMessageToWorker(
      scoped_refptr<ServiceWorkerVersion> service_worker,
      scoped_refptr<ServiceWorkerRegistration> registration,
      const std::string& description_id,
      blink::ServiceWorkerStatusCode service_worker_status);
  void DidDispatchEvent(const url::Origin& origin,
                        blink::ServiceWorkerStatusCode service_worker_status);

  // Clears all the content index related data in a service worker.
  void ClearServiceWorkerDataOnCorruption(
      int64_t service_worker_registration_id);

  // Callbacks on the UI thread to notify |provider_| of updates.
  void NotifyProviderContentAdded(std::vector<ContentIndexEntry> entries);
  void NotifyProviderContentDeleted(int64_t service_worker_registration_id,
                                    const url::Origin& origin,
                                    const std::string& entry_id);

  // Block/Unblock DB operations for |origin|.
  void BlockOrigin(const url::Origin& origin);
  void UnblockOrigin(const url::Origin& origin);

  // Lives on the UI thread.
  ContentIndexProvider* provider_;

  // A map from origins to how many times it's been blocked.
  // Must be used on the service worker core thread.
  base::flat_map<url::Origin, int> blocked_origins_;

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  base::WeakPtrFactory<ContentIndexDatabase> weak_ptr_factory_core_{this};
  base::WeakPtrFactory<ContentIndexDatabase> weak_ptr_factory_ui_{this};

  DISALLOW_COPY_AND_ASSIGN(ContentIndexDatabase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONTENT_INDEX_CONTENT_INDEX_DATABASE_H_
