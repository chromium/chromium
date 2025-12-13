// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_SAFETY_BERT_SAFETY_OP_RESOLVER_H_
#define SERVICES_ON_DEVICE_MODEL_SAFETY_BERT_SAFETY_OP_RESOLVER_H_

#include "components/optimization_guide/core/tflite_op_resolver.h"

namespace on_device_model {

// TFLite Op Resolver for the Bert Safety model.
class BertSafetyOpResolver : public optimization_guide::TFLiteOpResolver {
 public:
  BertSafetyOpResolver();
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_SAFETY_BERT_SAFETY_OP_RESOLVER_H_
