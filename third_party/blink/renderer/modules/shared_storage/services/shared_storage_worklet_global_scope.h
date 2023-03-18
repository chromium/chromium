// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SERVICES_SHARED_STORAGE_WORKLET_GLOBAL_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SERVICES_SHARED_STORAGE_WORKLET_GLOBAL_SCOPE_H_

#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/shared_storage/services/shared_storage_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

struct GlobalScopeCreationParams;
class ModuleScriptDownloader;

// mojom::SharedStorageWorkletService implementation. Responsible for
// handling worklet operations. This object lives on the worklet thread.
class MODULES_EXPORT SharedStorageWorkletGlobalScope final
    : public WorkletGlobalScope,
      public mojom::SharedStorageWorkletService {
 public:
  SharedStorageWorkletGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams> creation_params,
      WorkerThread* thread);
  ~SharedStorageWorkletGlobalScope() override;

  void BindSharedStorageWorkletService(
      mojo::PendingReceiver<mojom::SharedStorageWorkletService> receiver,
      base::OnceClosure disconnect_handler);

  // WorkletGlobalScope implementation:
  bool IsSharedStorageWorkletGlobalScope() const override { return true; }

  WorkletToken GetWorkletToken() const override { return token_; }
  ExecutionContextToken GetExecutionContextToken() const override {
    return token_;
  }

  void Trace(Visitor*) const override;

  // mojom::SharedStorageWorkletService implementation:
  void Initialize(
      mojo::PendingAssociatedRemote<mojom::SharedStorageWorkletServiceClient>
          client,
      bool private_aggregation_permissions_policy_allowed,
      mojo::PendingRemote<mojom::PrivateAggregationHost>
          private_aggregation_host,
      const absl::optional<std::u16string>& embedder_context) override;
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
  void OnModuleScriptDownloaded(
      const GURL& script_source_url,
      mojom::SharedStorageWorkletService::AddModuleCallback callback,
      std::unique_ptr<std::string> response_body,
      std::string error_message);

  network::mojom::RequestDestination GetDestination() const override {
    // Not called as the current implementation uses the custom module script
    // loader.
    NOTREACHED();

    // Once we migrate to the blink-worklet's script loading infra, this needs
    // to return a valid destination defined in the Fetch standard:
    // https://fetch.spec.whatwg.org/#concept-request-destination
    return network::mojom::RequestDestination::kEmpty;
  }

  // `receiver_`'s disconnect handler explicitly deletes the worklet thread
  // object that owns this service, thus deleting `this` upon disconnect. To
  // ensure that the worklet thread object and this service are not leaked,
  // `receiver_` must be cut off from the remote side when the worklet is
  // supposed to be destroyed.
  HeapMojoReceiver<mojom::SharedStorageWorkletService,
                   SharedStorageWorkletGlobalScope>
      receiver_{this, this};

  std::unique_ptr<ModuleScriptDownloader> module_script_downloader_;

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
  HeapMojoAssociatedRemote<mojom::SharedStorageWorkletServiceClient> client_{
      this};

  // Whether the "private-aggregation" permissions policy is enabled in the
  // worklet.
  bool private_aggregation_permissions_policy_allowed_ = false;

  // No need to be associated as message ordering (relative to shared storage
  // operations) is unimportant.
  HeapMojoRemote<mojom::PrivateAggregationHost> private_aggregation_host_{this};

  const SharedStorageWorkletToken token_;
};

template <>
struct DowncastTraits<SharedStorageWorkletGlobalScope> {
  static bool AllowFrom(const ExecutionContext& context) {
    return context.IsSharedStorageWorkletGlobalScope();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SERVICES_SHARED_STORAGE_WORKLET_GLOBAL_SCOPE_H_
