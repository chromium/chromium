// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_SHARED_STORAGE_WORKLET_PRIVATE_AGGREGATION_H_
#define CONTENT_SERVICES_SHARED_STORAGE_WORKLET_PRIVATE_AGGREGATION_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "content/common/private_aggregation_host.mojom-forward.h"
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
  explicit PrivateAggregation(
      content::mojom::PrivateAggregationHost& private_aggregation_host);
  ~PrivateAggregation() override;

  static gin::WrapperInfo kWrapperInfo;

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  const char* GetTypeName() override;

 private:
  void SendHistogramReport(gin::Arguments* args);

  raw_ref<content::mojom::PrivateAggregationHost> private_aggregation_host_;

  base::WeakPtrFactory<PrivateAggregation> weak_ptr_factory_{this};
};

}  // namespace shared_storage_worklet

#endif  // CONTENT_SERVICES_SHARED_STORAGE_WORKLET_PRIVATE_AGGREGATION_H_
