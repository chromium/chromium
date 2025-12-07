// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/ts_model.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/notimplemented.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/language_detection/core/language_detection_provider.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/translate/core/language_detection/language_detection_model.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/ml/chrome_ml_api.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"
#include "services/on_device_model/safety/safety_util.h"

#if !BUILDFLAG(IS_FUCHSIA) && BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "services/on_device_model/safety/bert_safety_model.h"
#endif

namespace ml {

namespace mojom = ::on_device_model::mojom;

class TsModel final : public mojom::TextSafetyModel,
                      public mojom::TextSafetySession {
 public:
  ~TsModel() override;

  static std::unique_ptr<TsModel> Create(
      const ChromeML& chrome_ml,
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
  explicit TsModel(const ChromeML& chrome_ml);
  bool InitLanguageDetection(mojom::LanguageModelAssetsPtr assets);
  bool InitTextSafetyModel(mojom::TextSafetyModelAssetsPtr assets);

  const raw_ref<const ChromeML> chrome_ml_;
  ChromeMLTSModel model_ = 0;
  std::unique_ptr<translate::LanguageDetectionModel> language_detector_;
  base::MemoryMappedFile data_;
  base::MemoryMappedFile sp_model_;
  mojo::ReceiverSet<mojom::TextSafetySession> sessions_;
};

TsModel::TsModel(const ChromeML& chrome_ml) : chrome_ml_(chrome_ml) {}

DISABLE_CFI_DLSYM
TsModel::~TsModel() {
  if (model_ != 0) {
    chrome_ml_->api().ts_api.DestroyModel(model_);
  }
}

// static
std::unique_ptr<TsModel> TsModel::Create(
    const ChromeML& chrome_ml,
    mojom::TextSafetyModelParamsPtr params) {
  TRACE_EVENT("optimization_guide", "TsModel::Create");
  auto ts_model = base::WrapUnique(new TsModel(chrome_ml));
  if (params->language_assets &&
      !ts_model->InitLanguageDetection(std::move(params->language_assets))) {
    return {};
  }
  if (params->safety_assets && !ts_model->InitTextSafetyModel(std::move(
                                   params->safety_assets->get_ts_assets()))) {
    return {};
  }
  return ts_model;
}

bool TsModel::InitLanguageDetection(mojom::LanguageModelAssetsPtr assets) {
  TRACE_EVENT("optimization_guide", "TsModel::InitLanguageDetection");
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  auto tflite_model =
      std::make_unique<language_detection::LanguageDetectionModel>();
  tflite_model->UpdateWithFile(std::move(assets->model));

  language_detector_ = std::make_unique<translate::LanguageDetectionModel>(
      std::move(tflite_model));
  return language_detector_->IsAvailable();
#else
  return false;
#endif
}

DISABLE_CFI_DLSYM
bool TsModel::InitTextSafetyModel(mojom::TextSafetyModelAssetsPtr assets) {
  TRACE_EVENT("optimization_guide", "TsModel::InitTextSafetyModel");
  if (!data_.Initialize(std::move(assets->data)) ||
      !sp_model_.Initialize(std::move(assets->sp_model))) {
    return false;
  }
  ChromeMLTSModelDescriptor desc = {
      .model = {.data = data_.data(), .size = data_.length()},
      .sp_model = {.data = sp_model_.data(), .size = sp_model_.length()},
  };
  model_ = chrome_ml_->api().ts_api.CreateModel(&desc);
  return bool(model_);
}

void TsModel::StartSession(
    mojo::PendingReceiver<mojom::TextSafetySession> session) {
  TRACE_EVENT("optimization_guide", "TsModel::StartSession");
  sessions_.Add(this, std::move(session));
}

void TsModel::ClassifyTextSafety(const std::string& text,
                                 ClassifyTextSafetyCallback callback) {
  TRACE_EVENT("optimization_guide", "TsModel::ClassifyTextSafety");
  std::move(callback).Run(ClassifyTextSafety(text));
}
void TsModel::DetectLanguage(const std::string& text,
                             DetectLanguageCallback callback) {
  TRACE_EVENT("optimization_guide", "TsModel::DetectLanguage");
  std::move(callback).Run(DetectLanguage(text));
}

void TsModel::Clone(mojo::PendingReceiver<mojom::TextSafetySession> session) {
  TRACE_EVENT("optimization_guide", "TsModel::Clone");
  StartSession(std::move(session));
}

DISABLE_CFI_DLSYM
mojom::SafetyInfoPtr TsModel::ClassifyTextSafety(const std::string& text) {
  TRACE_EVENT("optimization_guide", "TsModel::ClassifyTextSafety");
  if (!model_) {
    return nullptr;
  }

  // First query the API to see how much storage we need for class scores.
  size_t num_scores = 0;
  if (chrome_ml_->api().ts_api.ClassifyTextSafety(model_, text.c_str(), nullptr,
                                                  &num_scores) !=
      ChromeMLSafetyResult::kInsufficientStorage) {
    return nullptr;
  }

  auto safety_info = mojom::SafetyInfo::New();
  safety_info->class_scores.resize(num_scores);
  const auto result = chrome_ml_->api().ts_api.ClassifyTextSafety(
      model_, text.c_str(), safety_info->class_scores.data(), &num_scores);
  if (result != ChromeMLSafetyResult::kOk) {
    return nullptr;
  }
  CHECK_EQ(num_scores, safety_info->class_scores.size());
  if (language_detector_) {
    safety_info->language = DetectLanguage(text);
  }
  return safety_info;
}

mojom::LanguageDetectionResultPtr TsModel::DetectLanguage(
    std::string_view text) {
  if (!language_detector_) {
    return nullptr;
  }
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  language_detection::Prediction prediction = on_device_model::PredictLanguage(
      language_detector_->tflite_model(), text);
  return mojom::LanguageDetectionResult::New(prediction.language,
                                             prediction.score);
#else
  return nullptr;
#endif
}

TsHolder::TsHolder(raw_ref<const ChromeML> chrome_ml) : chrome_ml_(chrome_ml) {}
TsHolder::~TsHolder() = default;

// static
base::SequenceBound<TsHolder> TsHolder::Create(const ChromeML& chrome_ml) {
  return base::SequenceBound<TsHolder>(
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}),
      ToRawRef(chrome_ml));
}

void TsHolder::Reset(mojom::TextSafetyModelParamsPtr params,
                     mojo::PendingReceiver<mojom::TextSafetyModel> model) {
  model_.Clear();

#if !BUILDFLAG(IS_FUCHSIA) && BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (!params->safety_assets || params->safety_assets->which() ==
                                    mojom::SafetyModelAssets::Tag::kTsAssets) {
    auto impl = TsModel::Create(*chrome_ml_, std::move(params));
    if (impl) {
      model_.Add(std::move(impl), std::move(model));
    }
  } else {
    auto impl = on_device_model::BertSafetyModel::Create(std::move(params));
    if (impl) {
      model_.Add(std::move(impl), std::move(model));
    }
  }
#else
  CHECK(!params->safety_assets || params->safety_assets->which() ==
                                      mojom::SafetyModelAssets::Tag::kTsAssets);
  auto impl = TsModel::Create(*chrome_ml_, std::move(params));
  if (impl) {
    model_.Add(std::move(impl), std::move(model));
  }
#endif
}

}  // namespace ml
