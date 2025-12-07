// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_OP_RESOLVER_H_
#define SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_OP_RESOLVER_H_

#include "components/optimization_guide/core/tflite_op_resolver.h"

namespace passage_embeddings {

// This class maintains the supported TFLite operations for the passage
// embeddings model.
class PassageEmbeddingsOpResolver
    : public optimization_guide::TFLiteOpResolver {
 public:
  explicit PassageEmbeddingsOpResolver(bool allow_gpu_execution);
};

}  // namespace passage_embeddings

#endif  // SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_OP_RESOLVER_H_
