// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_RUNTIME_MANAGER_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_RUNTIME_MANAGER_H_

#include <map>
#include <memory>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation_traits.h"
#include "base/time/time.h"
#include "content/browser/shared_storage/shared_storage_event_params.h"
#include "content/browser/shared_storage/shared_storage_lock_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "third_party/blink/public/mojom/origin_trials/origin_trial_feature.mojom-shared.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom.h"

namespace content {

class FencedFrameConfig;
class SharedStorageDocumentServiceImpl;
class SharedStorageWorkletHost;
class StoragePartitionImpl;

// Manages in-memory components related to shared storage, such as
// `SharedStorageWorkletHost` and `LockManager`. The manager is bound to the
// `StoragePartition`.
class CONTENT_EXPORT SharedStorageRuntimeManager {
 public:
  using AccessScope = blink::SharedStorageAccessScope;
  using WorkletHosts = std::map<SharedStorageWorkletHost*,
                                std::unique_ptr<SharedStorageWorkletHost>>;

  explicit SharedStorageRuntimeManager(StoragePartitionImpl& storage_partition);
  virtual ~SharedStorageRuntimeManager();

  class SharedStorageObserverInterface : public base::CheckedObserver {
   public:
    enum AccessMethod {
      kAddModule,
      kCreateWorklet,
      kSelectURL,
      kRun,
      kBatchUpdate,
      kSet,
      kAppend,
      kDelete,
      kClear,
      kGet,
      kKeys,
      kValues,
      kEntries,
      kLength,
      kRemainingBudget,
    };

    virtual GlobalRenderFrameHostId AssociatedFrameHostId() const = 0;

    virtual bool ShouldReceiveAllSharedStorageReports() const = 0;

    virtual void OnSharedStorageAccessed(
        base::Time access_time,
        AccessScope scope,
        AccessMethod method,
        GlobalRenderFrameHostId main_frame_id,
        const std::string& owner_origin,
        const SharedStorageEventParams& params) = 0;

    virtual void OnSharedStorageSelectUrlUrnUuidGenerated(
        const GURL& urn_uuid) = 0;

    virtual void OnSharedStorageSelectUrlConfigPopulated(
        const std::optional<FencedFrameConfig>& config) = 0;

    virtual void OnSharedStorageWorkletOperationExecutionFinished(
        base::Time finished_time,
        base::TimeDelta execution_time,
        AccessMethod method,
        int operation_id,
        const base::UnguessableToken& worklet_devtools_token,
        GlobalRenderFrameHostId main_frame_id,
        const std::string& owner_origin) = 0;
  };

  void OnDocumentServiceDestroyed(
      SharedStorageDocumentServiceImpl* document_service);

  void ExpireWorkletHostForDocumentService(
      SharedStorageDocumentServiceImpl* document_service,
      SharedStorageWorkletHost* worklet_host);

  void CreateWorkletHost(
      SharedStorageDocumentServiceImpl* document_service,
      const url::Origin& frame_origin,
      const url::Origin& data_origin,
      blink::mojom::SharedStorageDataOriginType data_origin_type,
      const GURL& script_source_url,
      network::mojom::CredentialsMode credentials_mode,
      blink::mojom::SharedStorageWorkletCreationMethod creation_method,
      const std::vector<blink::mojom::OriginTrialFeature>&
          origin_trial_features,
      mojo::PendingAssociatedReceiver<blink::mojom::SharedStorageWorkletHost>
          worklet_host_receiver,
      blink::mojom::SharedStorageDocumentService::CreateWorkletCallback
          callback);

  void AddSharedStorageObserver(SharedStorageObserverInterface* observer);

  void RemoveSharedStorageObserver(SharedStorageObserverInterface* observer);

  void NotifySharedStorageAccessed(
      AccessScope scope,
      SharedStorageObserverInterface::AccessMethod method,
      GlobalRenderFrameHostId main_frame_id,
      const std::string& owner_origin,
      const SharedStorageEventParams& params);

  void NotifyWorkletOperationExecutionFinished(
      base::TimeDelta execution_time,
      SharedStorageObserverInterface::AccessMethod method,
      int operation_id,
      const base::UnguessableToken& worklet_devtools_token,
      GlobalRenderFrameHostId main_frame_id,
      const std::string& owner_origin);

  std::map<SharedStorageDocumentServiceImpl*, WorkletHosts>&
  GetAttachedWorkletHostsForTesting() {
    return attached_shared_storage_worklet_hosts_;
  }

  const std::map<SharedStorageWorkletHost*,
                 std::unique_ptr<SharedStorageWorkletHost>>&
  GetKeepAliveWorkletHostsForTesting() {
    return keep_alive_shared_storage_worklet_hosts_;
  }

  void NotifyUrnUuidGenerated(const GURL& urn_uuid);

  void NotifyConfigPopulated(const std::optional<FencedFrameConfig>& config);

  SharedStorageLockManager& lock_manager() { return lock_manager_; }

 protected:
  void OnWorkletKeepAliveFinished(SharedStorageWorkletHost*);

  // virtual for testing
  virtual std::unique_ptr<SharedStorageWorkletHost> CreateWorkletHostHelper(
      SharedStorageDocumentServiceImpl& document_service,
      const url::Origin& frame_origin,
      const url::Origin& data_origin,
      blink::mojom::SharedStorageDataOriginType data_origin_type,
      const GURL& script_source_url,
      network::mojom::CredentialsMode credentials_mode,
      blink::mojom::SharedStorageWorkletCreationMethod creation_method,
      int worklet_ordinal,
      const std::vector<blink::mojom::OriginTrialFeature>&
          origin_trial_features,
      mojo::PendingAssociatedReceiver<blink::mojom::SharedStorageWorkletHost>
          worklet_host,
      blink::mojom::SharedStorageDocumentService::CreateWorkletCallback
          callback);

 private:
  // The hosts that are attached to the worklet's owner document. Those hosts
  // are created on demand when the `SharedStorageDocumentServiceImpl` requests
  // it. When the corresponding document is destructed (where it will call
  // `OnDocumentServiceDestroyed`), its associated worklet hosts will either be
  // destroyed, or will be moved from this map to
  // `keep_alive_shared_storage_worklet_hosts_`, depending on whether there are
  // pending operations.
  std::map<SharedStorageDocumentServiceImpl*, WorkletHosts>
      attached_shared_storage_worklet_hosts_;

  // The hosts that are detached from the worklet's owner document and have
  // entered keep-alive phase.
  WorkletHosts keep_alive_shared_storage_worklet_hosts_;

  // Manages shared storage locks.
  SharedStorageLockManager lock_manager_;

  base::ObserverList<SharedStorageObserverInterface> observers_;

  // A monotonically increasing number assigned to each
  // SharedStorageWorkletHost. This ordinal is assigned during construction of
  // the SharedStorageWorkletHost.
  int next_worklet_ordinal_ = 0;
};

}  // namespace content

namespace base {

template <>
struct ScopedObservationTraits<
    content::SharedStorageRuntimeManager,
    content::SharedStorageRuntimeManager::SharedStorageObserverInterface> {
  static void AddObserver(
      content::SharedStorageRuntimeManager* source,
      content::SharedStorageRuntimeManager::SharedStorageObserverInterface*
          observer) {
    source->AddSharedStorageObserver(observer);
  }
  static void RemoveObserver(
      content::SharedStorageRuntimeManager* source,
      content::SharedStorageRuntimeManager::SharedStorageObserverInterface*
          observer) {
    source->RemoveSharedStorageObserver(observer);
  }
};

}  // namespace base

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_RUNTIME_MANAGER_H_
