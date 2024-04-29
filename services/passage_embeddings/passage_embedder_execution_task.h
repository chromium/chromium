// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDER_EXECUTION_TASK_H_
#define SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDER_EXECUTION_TASK_H_

#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/base_task_api.h"

namespace passage_embeddings {

using OutputType = std::vector<float>;
using InputType = const std::vector<int>&;

class PassageEmbedderExecutionTask
    : public tflite::task::core::BaseTaskApi<OutputType, InputType> {
 public:
  explicit PassageEmbedderExecutionTask(
      std::unique_ptr<tflite::task::core::TfLiteEngine> tflite_engine);
  ~PassageEmbedderExecutionTask() override;

  std::optional<OutputType> Execute(InputType input);

 protected:
  // BaseTaskApi:
  absl::Status Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                          InputType input) override;
  tflite::support::StatusOr<OutputType> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors,
      InputType input) override;
};

}  // namespace passage_embeddings

#endif  // SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDER_EXECUTION_TASK_H_
