// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "services/on_device_model/safety/bert_safety_op_resolver.h"

#include "third_party/tensorflow-text/src/tensorflow_text/core/kernels/fast_bert_normalizer_tflite.h"
#include "third_party/tensorflow-text/src/tensorflow_text/core/kernels/fast_wordpiece_tokenizer_tflite.h"

namespace on_device_model {

BertSafetyOpResolver::BertSafetyOpResolver() {
  tflite::ops::custom::text::AddFastBertNormalize(this);
  tflite::ops::custom::text::AddFastWordpieceTokenize(this);
  tflite::ops::custom::text::AddFastWordpieceDetokenize(this);
}

}  // namespace on_device_model
