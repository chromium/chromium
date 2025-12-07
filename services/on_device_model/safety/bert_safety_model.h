// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_SAFETY_BERT_SAFETY_MODEL_H_
#define SERVICES_ON_DEVICE_MODEL_SAFETY_BERT_SAFETY_MODEL_H_

#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/base_task_api.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/text/nlclassifier/nl_classifier.h"

namespace translate {
class LanguageDetectionModel;
}  // namespace translate

namespace on_device_model {

class BertSafetyModel final : public mojom::TextSafetyModel,
                              public mojom::TextSafetySession {
 public:
  using BertExecutionTask =
      tflite::task::core::BaseTaskApi<std::vector<tflite::task::core::Category>,
                                      const std::string&>;

  ~BertSafetyModel() override;

  static std::unique_ptr<BertSafetyModel> Create(
      mojom::TextSafetyModelParamsPtr params);

  // mojom::TextSafetyModel
  void StartSession(
      mojo::PendingReceiver<mojom::TextSafetySession> session) override;

  // mojom::TextSafetySession
  void ClassifyTextSafety(const std::string& text,
                          ClassifyTextSafetyCallback callback) override;
  void DetectLanguage(const std::string& text,
                      DetectLanguageCallback callback) override;
  void Clone(mojo::PendingReceiver<mojom::TextSafetySession> session) override;

  mojom::SafetyInfoPtr ClassifyTextSafety(const std::string& text);
  mojom::LanguageDetectionResultPtr DetectLanguage(std::string_view text);

 private:
  BertSafetyModel();
  bool InitLanguageDetection(mojom::LanguageModelAssetsPtr assets);
  bool InitTextSafetyModel(mojom::BertSafetyModelAssetsPtr assets);

  std::unique_ptr<translate::LanguageDetectionModel> language_detector_;
  std::unique_ptr<BertExecutionTask> loaded_bert_model_;
  mojo::ReceiverSet<mojom::TextSafetySession> sessions_;
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_SAFETY_BERT_SAFETY_MODEL_H_
