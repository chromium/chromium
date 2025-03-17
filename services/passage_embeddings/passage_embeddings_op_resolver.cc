// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "services/passage_embeddings/passage_embeddings_op_resolver.h"

#include "services/on_device_model/ml_internal_buildflags.h"

#if defined(BUILD_WITH_ML_INTERNAL)
#include "services/on_device_model/ml/chrome_ml.h"      // nogncheck
#include "services/on_device_model/ml/chrome_ml_api.h"  // nogncheck
#endif

namespace passage_embeddings {

PassageEmbeddingsOpResolver::PassageEmbeddingsOpResolver(
    bool allow_gpu_execution) {
#if defined(BUILD_WITH_ML_INTERNAL)
  if (allow_gpu_execution) {
    auto* chrome_ml = ml::ChromeML::Get();
    if (chrome_ml && chrome_ml->api().CreateGpuDelegate &&
        chrome_ml->api().DestroyGpuDelegate) {
      delegate_creators_.insert(
          delegate_creators_.begin(), [](TfLiteContext* context) {
            return std::unique_ptr<TfLiteDelegate, void (*)(TfLiteDelegate*)>(
                ml::ChromeML::Get()->api().CreateGpuDelegate(),
                [](TfLiteDelegate* delegate) {
                  ml::ChromeML::Get()->api().DestroyGpuDelegate(delegate);
                });
          });
    }
  }
#endif
}

}  // namespace passage_embeddings
