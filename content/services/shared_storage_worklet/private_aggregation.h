// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_SHARED_STORAGE_WORKLET_PRIVATE_AGGREGATION_H_
#define CONTENT_SERVICES_SHARED_STORAGE_WORKLET_PRIVATE_AGGREGATION_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "content/common/private_aggregation_host.mojom.h"
#include "content/common/shared_storage_worklet_service.mojom.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "v8/include/v8-forward.h"

namespace gin {
class Arguments;
}  // namespace gin

namespace shared_storage_worklet {

class PrivateAggregation final : public gin::Wrappable<PrivateAggregation> {
 public:
  PrivateAggregation(
      mojom::SharedStorageWorkletServiceClient& client,
      content::mojom::PrivateAggregationHost& private_aggregation_host);
  ~PrivateAggregation() override;

  static gin::WrapperInfo kWrapperInfo;

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  const char* GetTypeName() override;

 private:
  void SendHistogramReport(gin::Arguments* args);
  void EnableDebugMode(gin::Arguments* args);

  void EnsureUseCountersAreRecorded();

  raw_ref<mojom::SharedStorageWorkletServiceClient> client_;
  raw_ref<content::mojom::PrivateAggregationHost> private_aggregation_host_;

  bool has_recorded_use_counters_ = false;

  // Defaults to debug mode being disabled.
  content::mojom::DebugModeDetails debug_mode_details_;

  base::WeakPtrFactory<PrivateAggregation> weak_ptr_factory_{this};
};

}  // namespace shared_storage_worklet

#endif  // CONTENT_SERVICES_SHARED_STORAGE_WORKLET_PRIVATE_AGGREGATION_H_
