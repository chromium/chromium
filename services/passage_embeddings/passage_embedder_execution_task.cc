// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/passage_embeddings/passage_embedder_execution_task.h"

#include "base/check_op.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_utils.h"

namespace passage_embeddings {

PassageEmbedderExecutionTask::PassageEmbedderExecutionTask(
    std::unique_ptr<tflite::task::core::TfLiteEngine> tflite_engine)
    : tflite::task::core::BaseTaskApi<OutputType, InputType>(
          std::move(tflite_engine)) {}

PassageEmbedderExecutionTask::~PassageEmbedderExecutionTask() {
  GetTfLiteEngine()->Cancel();
}

std::optional<OutputType> PassageEmbedderExecutionTask::Execute(
    InputType input) {
  tflite::support::StatusOr<OutputType> maybe_output = this->Infer(input);
  if (!maybe_output.ok()) {
    return std::nullopt;
  }
  return maybe_output.value();
}

absl::Status PassageEmbedderExecutionTask::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    InputType input) {
  return tflite::task::core::PopulateTensor<int>(input, input_tensors[0]);
}

tflite::support::StatusOr<OutputType> PassageEmbedderExecutionTask::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors,
    InputType input) {
  std::vector<float> output;
  absl::Status status =
      tflite::task::core::PopulateVector<float>(output_tensors[0], &output);
  if (!status.ok()) {
    return status;
  }

  return output;
}

}  // namespace passage_embeddings
