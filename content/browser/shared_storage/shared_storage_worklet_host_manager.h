// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_HOST_MANAGER_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_HOST_MANAGER_H_

#include <map>
#include <memory>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation_traits.h"
#include "base/time/time.h"
#include "content/browser/shared_storage/shared_storage_event_params.h"
#include "content/common/content_export.h"

namespace content {

struct FencedFrameConfig;
class SharedStorageDocumentServiceImpl;
class SharedStorageWorkletDriver;
class SharedStorageWorkletHost;

// Manages the creation and destruction of the `SharedStorageWorkletHost`. The
// manager is bound to the StoragePartition.
class CONTENT_EXPORT SharedStorageWorkletHostManager {
 public:
  SharedStorageWorkletHostManager();
  virtual ~SharedStorageWorkletHostManager();

  class SharedStorageObserverInterface : public base::CheckedObserver {
   public:
    enum AccessType {
      kDocumentAddModule,
      kDocumentSelectURL,
      kDocumentRun,
      kDocumentSet,
      kDocumentAppend,
      kDocumentDelete,
      kDocumentClear,
      kWorkletSet,
      kWorkletAppend,
      kWorkletDelete,
      kWorkletClear,
      kWorkletGet,
      kWorkletKeys,
      kWorkletEntries,
      kWorkletLength,
      kWorkletRemainingBudget
    };

    virtual void OnSharedStorageAccessed(
        const base::Time& access_time,
        AccessType type,
        const std::string& main_frame_id,
        const std::string& owner_origin,
        const SharedStorageEventParams& params) = 0;

    virtual void OnUrnUuidGenerated(const GURL& urn_uuid) = 0;

    virtual void OnConfigPopulated(
        const absl::optional<FencedFrameConfig>& config) = 0;
  };

  void OnDocumentServiceDestroyed(
      SharedStorageDocumentServiceImpl* document_service);

  void ExpireWorkletHostForDocumentService(
      SharedStorageDocumentServiceImpl* document_service);

  SharedStorageWorkletHost* GetOrCreateSharedStorageWorkletHost(
      SharedStorageDocumentServiceImpl* document_service);

  void AddSharedStorageObserver(SharedStorageObserverInterface* observer);

  void RemoveSharedStorageObserver(SharedStorageObserverInterface* observer);

  void NotifySharedStorageAccessed(
      SharedStorageObserverInterface::AccessType type,
      const std::string& main_frame_id,
      const std::string& owner_origin,
      const SharedStorageEventParams& params);

  const std::map<SharedStorageDocumentServiceImpl*,
                 std::unique_ptr<SharedStorageWorkletHost>>&
  GetAttachedWorkletHostsForTesting() {
    return attached_shared_storage_worklet_hosts_;
  }

  const std::map<SharedStorageWorkletHost*,
                 std::unique_ptr<SharedStorageWorkletHost>>&
  GetKeepAliveWorkletHostsForTesting() {
    return keep_alive_shared_storage_worklet_hosts_;
  }

  void NotifyUrnUuidGenerated(const GURL& urn_uuid);

  void NotifyConfigPopulated(const absl::optional<FencedFrameConfig>& config);

 protected:
  void OnWorkletKeepAliveFinished(SharedStorageWorkletHost*);

  // virtual for testing
  virtual std::unique_ptr<SharedStorageWorkletHost>
  CreateSharedStorageWorkletHost(
      std::unique_ptr<SharedStorageWorkletDriver> driver,
      SharedStorageDocumentServiceImpl& document_service);

 private:
  // The hosts that are attached to the worklet's owner document. Those hosts
  // are created on demand when the `SharedStorageDocumentServiceImpl` requests
  // it. When the corresponding document is destructed (where it will call
  // `OnDocumentServiceDestroyed`), those hosts will either be removed from this
  // map entirely, or will be moved from this map to
  // `keep_alive_shared_storage_worklet_hosts_`, depending on whether there are
  // pending operations.
  std::map<SharedStorageDocumentServiceImpl*,
           std::unique_ptr<SharedStorageWorkletHost>>
      attached_shared_storage_worklet_hosts_;

  // The hosts that are detached from the worklet's owner document and have
  // entered keep-alive phase.
  std::map<SharedStorageWorkletHost*, std::unique_ptr<SharedStorageWorkletHost>>
      keep_alive_shared_storage_worklet_hosts_;

  base::ObserverList<SharedStorageObserverInterface> observers_;
};

}  // namespace content

namespace base {

template <>
struct ScopedObservationTraits<
    content::SharedStorageWorkletHostManager,
    content::SharedStorageWorkletHostManager::SharedStorageObserverInterface> {
  static void AddObserver(
      content::SharedStorageWorkletHostManager* source,
      content::SharedStorageWorkletHostManager::SharedStorageObserverInterface*
          observer) {
    source->AddSharedStorageObserver(observer);
  }
  static void RemoveObserver(
      content::SharedStorageWorkletHostManager* source,
      content::SharedStorageWorkletHostManager::SharedStorageObserverInterface*
          observer) {
    source->RemoveSharedStorageObserver(observer);
  }
};

}  // namespace base

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_HOST_MANAGER_H_
