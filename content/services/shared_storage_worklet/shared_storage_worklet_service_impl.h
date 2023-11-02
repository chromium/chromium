// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_SHARED_STORAGE_WORKLET_SHARED_STORAGE_WORKLET_SERVICE_IMPL_H_
#define CONTENT_SERVICES_SHARED_STORAGE_WORKLET_SHARED_STORAGE_WORKLET_SERVICE_IMPL_H_

#include "content/common/private_aggregation_host.mojom.h"
#include "content/common/shared_storage_worklet_service.mojom.h"
#include "content/services/shared_storage_worklet/shared_storage_worklet_global_scope.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"

namespace shared_storage_worklet {

// mojom::SharedStorageWorkletService implementation. Responsible for handling
// worklet operations. The service is started through
// `SharedStorageWorkletDriver::StartWorkletService`.
class SharedStorageWorkletServiceImpl
    : public mojom::SharedStorageWorkletService {
 public:
  explicit SharedStorageWorkletServiceImpl(
      mojo::PendingReceiver<mojom::SharedStorageWorkletService> receiver,
      base::OnceClosure disconnect_handler);
  ~SharedStorageWorkletServiceImpl() override;

  // mojom::SharedStorageWorkletService implementation:
  void Initialize(mojo::PendingAssociatedRemote<
                      mojom::SharedStorageWorkletServiceClient> client,
                  mojo::PendingRemote<content::mojom::PrivateAggregationHost>
                      private_aggregation_host) override;
  void AddModule(mojo::PendingRemote<network::mojom::URLLoaderFactory>
                     pending_url_loader_factory,
                 const GURL& script_source_url,
                 AddModuleCallback callback) override;
  void RunURLSelectionOperation(
      const std::string& name,
      const std::vector<GURL>& urls,
      const std::vector<uint8_t>& serialized_data,
      RunURLSelectionOperationCallback callback) override;
  void RunOperation(const std::string& name,
                    const std::vector<uint8_t>& serialized_data,
                    RunOperationCallback callback) override;

 private:
  SharedStorageWorkletGlobalScope* GetGlobalScope();

  // `receiver_`'s disconnect handler explicitly deletes the worklet thread
  // object that owns this service, thus deleting `this` upon disconnect. To
  // ensure that the worklet thread object and this service are not leaked,
  // `receiver_` must be cut off from the remote side when the worklet is
  // supposed to be destroyed.
  mojo::Receiver<mojom::SharedStorageWorkletService> receiver_;

  // This is associated because on the client side (i.e. worklet host), we want
  // the call-in methods (e.g. storage access) and the callback methods
  // (e.g. finish of a run-operation) to preserve their invocation order. This
  // guarantee is desirable, as the client may shut down the service immediately
  // after it gets the callback and sees no more outstanding operations, thus we
  // want it to be more likely for the worklet to finish its intended work.
  //
  // In contrast, the `receiver_` doesn't need to be associated. This is a
  // standalone service, so the starting of a worklet operation doesn't have to
  // depend on / preserve the order with messages of other types.
  mojo::AssociatedRemote<mojom::SharedStorageWorkletServiceClient> client_;

  // No need to be associated as message ordering (relative to shared storage
  // operations) is unimportant.
  mojo::Remote<content::mojom::PrivateAggregationHost>
      private_aggregation_host_;

  std::unique_ptr<SharedStorageWorkletGlobalScope> global_scope_;
};

}  // namespace shared_storage_worklet

#endif  // CONTENT_SERVICES_SHARED_STORAGE_WORKLET_SHARED_STORAGE_WORKLET_SERVICE_IMPL_H_
