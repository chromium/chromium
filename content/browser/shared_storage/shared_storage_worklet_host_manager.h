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
#include "content/public/browser/frame_tree_node_id.h"
#include "third_party/blink/public/mojom/origin_trial_feature/origin_trial_feature.mojom-shared.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom.h"

namespace content {

class FencedFrameConfig;
class SharedStorageDocumentServiceImpl;
class SharedStorageWorkletHost;

// Manages the creation and destruction of the `SharedStorageWorkletHost`. The
// manager is bound to the StoragePartition.
class CONTENT_EXPORT SharedStorageWorkletHostManager {
 public:
  using WorkletHosts = std::map<SharedStorageWorkletHost*,
                                std::unique_ptr<SharedStorageWorkletHost>>;

  SharedStorageWorkletHostManager();
  virtual ~SharedStorageWorkletHostManager();

  class SharedStorageObserverInterface : public base::CheckedObserver {
   public:
    enum AccessType {
      // The "Document" prefix indicates that the method is called from the
      // Window scope, and the "Worklet" prefix indicates that the method is
      // called from SharedStorageWorkletGlobalScope.
      kDocumentAddModule,
      kDocumentSelectURL,
      kDocumentRun,
      kDocumentSet,
      kDocumentAppend,
      kDocumentDelete,
      kDocumentClear,
      kDocumentGet,
      kWorkletSet,
      kWorkletAppend,
      kWorkletDelete,
      kWorkletClear,
      kWorkletGet,
      kWorkletKeys,
      kWorkletEntries,
      kWorkletLength,
      kWorkletRemainingBudget,
      kHeaderSet,
      kHeaderAppend,
      kHeaderDelete,
      kHeaderClear,
    };

    virtual void OnSharedStorageAccessed(
        const base::Time& access_time,
        AccessType type,
        FrameTreeNodeId main_frame_id,
        const std::string& owner_origin,
        const SharedStorageEventParams& params) = 0;

    virtual void OnUrnUuidGenerated(const GURL& urn_uuid) = 0;

    virtual void OnConfigPopulated(
        const std::optional<FencedFrameConfig>& config) = 0;
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
      const GURL& script_source_url,
      network::mojom::CredentialsMode credentials_mode,
      const std::vector<blink::mojom::OriginTrialFeature>&
          origin_trial_features,
      mojo::PendingAssociatedReceiver<blink::mojom::SharedStorageWorkletHost>
          worklet_host_receiver,
      blink::mojom::SharedStorageDocumentService::CreateWorkletCallback
          callback);

  void AddSharedStorageObserver(SharedStorageObserverInterface* observer);

  void RemoveSharedStorageObserver(SharedStorageObserverInterface* observer);

  void NotifySharedStorageAccessed(
      SharedStorageObserverInterface::AccessType type,
      FrameTreeNodeId main_frame_id,
      const std::string& owner_origin,
      const SharedStorageEventParams& params);

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

 protected:
  void OnWorkletKeepAliveFinished(SharedStorageWorkletHost*);

  // virtual for testing
  virtual std::unique_ptr<SharedStorageWorkletHost> CreateWorkletHostHelper(
      SharedStorageDocumentServiceImpl& document_service,
      const url::Origin& frame_origin,
      const url::Origin& data_origin,
      const GURL& script_source_url,
      network::mojom::CredentialsMode credentials_mode,
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
