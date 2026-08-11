// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/passage_embeddings/passage_embeddings_op_resolver.h"

#include <memory>

#include "build/build_config.h"
#include "services/on_device_model/ml_internal_buildflags.h"
#include "third_party/tflite/buildflags.h"

#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
#include "third_party/tflite/src/tensorflow/lite/delegates/xnnpack/xnnpack_delegate.h"
#endif
#if BUILDFLAG(ENABLE_ML_INTERNAL)
#include "services/on_device_model/ml/chrome_ml.h"  // nogncheck
#endif
#if BUILDFLAG(IS_IOS)
#include "third_party/tflite/src/tensorflow/lite/delegates/coreml/coreml_delegate.h"
#endif

namespace passage_embeddings {

HistoryOpResolver::HistoryOpResolver(bool allow_gpu_execution) {
#if BUILDFLAG(IS_IOS)
  if (allow_gpu_execution) {
    delegate_creators_.insert(
        delegate_creators_.begin(), [](TfLiteContext* context) {
          TfLiteCoreMlDelegateOptions options = {};
          options.enabled_devices = TfLiteCoreMlDelegateDevicesWithNeuralEngine;
          return std::unique_ptr<TfLiteDelegate, void (*)(TfLiteDelegate*)>(
              TfLiteCoreMlDelegateCreate(&options), TfLiteCoreMlDelegateDelete);
        });
  }
#elif BUILDFLAG(ENABLE_ML_INTERNAL)
  if (allow_gpu_execution) {
    auto* chrome_ml = ml::ChromeML::Get();
    if (chrome_ml && chrome_ml->HasCreateGpuDelegate() &&
        chrome_ml->HasDestroyGpuDelegate()) {
      delegate_creators_.insert(
          delegate_creators_.begin(), [](TfLiteContext* context) {
            return std::unique_ptr<TfLiteDelegate, void (*)(TfLiteDelegate*)>(
                ml::ChromeML::Get()->CreateGpuDelegate(),
                [](TfLiteDelegate* delegate) {
                  ml::ChromeML::Get()->DestroyGpuDelegate(delegate);
                });
          });
    }
  }
#endif
}

GemmaOpResolver::GemmaOpResolver() {
#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
  delegate_creators_ = {[](TfLiteContext* context) {
    TfLiteXNNPackDelegateOptions options =
        TfLiteXNNPackDelegateOptionsDefault();
    options.flags |= TFLITE_XNNPACK_DELEGATE_FLAG_DYNAMIC_FULLY_CONNECTED |
                     TFLITE_XNNPACK_DELEGATE_FLAG_ENABLE_LATEST_OPERATORS |
                     TFLITE_XNNPACK_DELEGATE_FLAG_ENABLE_SUBGRAPH_RESHAPING;
    options.weight_cache_file_path = TfLiteXNNPackDelegateInMemoryFilePath();

    return std::unique_ptr<TfLiteDelegate, void (*)(TfLiteDelegate*)>(
        TfLiteXNNPackDelegateCreateWithThreadpool(&options, context),
        TfLiteXNNPackDelegateDelete);
  }};
#endif
}

}  // namespace passage_embeddings
