// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_SHARED_STORAGE_WORKLET_PRIVATE_AGGREGATION_H_
#define CONTENT_SERVICES_SHARED_STORAGE_WORKLET_PRIVATE_AGGREGATION_H_

#include <stdint.h>

#include <map>

#include "base/memory/raw_ref.h"
#include "content/services/shared_storage_worklet/shared_storage_worklet_global_scope.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom-forward.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom.h"

namespace gin {
class Arguments;
}  // namespace gin

namespace shared_storage_worklet {

class PrivateAggregation final : public gin::Wrappable<PrivateAggregation> {
 public:
  PrivateAggregation(blink::mojom::SharedStorageWorkletServiceClient& client,
                     bool private_aggregation_permissions_policy_allowed,
                     SharedStorageWorkletGlobalScope& global_scope);
  ~PrivateAggregation() override;

  static gin::WrapperInfo kWrapperInfo;

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  const char* GetTypeName() override;

  void OnOperationStarted(
      int64_t operation_id,
      mojo::PendingRemote<blink::mojom::PrivateAggregationHost>
          private_aggregation_host);
  void OnOperationFinished(int64_t operation_id);

 private:
  struct OperationState;

  void SendHistogramReport(gin::Arguments* args);
  void EnableDebugMode(gin::Arguments* args);

  void EnsureUseCountersAreRecorded();

  raw_ref<blink::mojom::SharedStorageWorkletServiceClient> client_;

  bool private_aggregation_permissions_policy_allowed_;

  raw_ref<SharedStorageWorkletGlobalScope> global_scope_;

  bool has_recorded_use_counters_ = false;

  // Keyed by a per-invocation operation ID.
  std::map<int64_t, OperationState> operation_states_;
};

}  // namespace shared_storage_worklet

#endif  // CONTENT_SERVICES_SHARED_STORAGE_WORKLET_PRIVATE_AGGREGATION_H_
