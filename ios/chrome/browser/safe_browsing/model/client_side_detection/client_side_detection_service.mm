// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/client_side_detection/client_side_detection_service.h"

#import "base/check.h"
#import "base/files/file.h"
#import "base/functional/bind.h"
#import "base/location.h"
#import "base/memory/read_only_shared_memory_region.h"
#import "base/metrics/histogram_functions.h"
#import "base/sequence_checker.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/browser/client_side_phishing_model.h"
#import "components/safe_browsing/core/common/phishing_classifier/scorer.h"
#import "components/safe_browsing/core/common/proto/csd.pb.h"
#import "components/safe_browsing/ios/browser/web_ui/web_ui_ios_info_singleton.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

namespace {

// Helper function to create a `Scorer` instance on a background thread.
// It initializes the `Scorer` with the provided visual model and optional
// image embedding model.
std::unique_ptr<Scorer> CreateScorerOnBackgroundThread(
    base::ReadOnlySharedMemoryRegion region,
    base::File visual_model,
    base::File embedding_model,
    int embedding_width,
    int embedding_height) {
  std::unique_ptr<Scorer> scorer =
      Scorer::Create(std::move(region), std::move(visual_model));
  if (scorer && embedding_model.IsValid()) {
    scorer->AttachImageEmbeddingModel(embedding_width, embedding_height,
                                      std::move(embedding_model));
  }
  return scorer;
}
}  // namespace

ClientSideDetectionService::ClientSideDetectionService(
    PrefService* prefs,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    optimization_guide::OptimizationGuideModelProvider* opt_guide)
    : ClientSideDetectionServiceBase(prefs, opt_guide) {
  CHECK(prefs);
  url_loader_factory_ = std::move(url_loader_factory);
  OnPrefsUpdated();
}

ClientSideDetectionService::~ClientSideDetectionService() = default;

void ClientSideDetectionService::OnModelUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Increment this now to ensure stale `Scorer`s are discarded when their
  // creation tasks complete, even if we exit early later in this method.
  current_model_generation_++;

  // Unlike the Content implementation (which sends models to renderers even if
  // disabled), the iOS implementation holds the `Scorer` in the browser
  // process. We must clear it here to free resources and stop detection
  // immediately if disabled.
  if (!IsEnabled()) {
    ClearScorerAndNotifyObservers();
    return;
  }

  if (!IsModelAvailable()) {
    ClearScorerAndNotifyObservers();
    return;
  }

  base::ReadOnlySharedMemoryRegion region =
      client_side_phishing_model_->GetModelSharedMemoryRegion();
  const base::File& visual_model =
      client_side_phishing_model_->GetVisualTfLiteModel();

  if (!region.IsValid() || !visual_model.IsValid()) {
    VLOG(1) << "Model update failed. Shared memory region is "
            << (region.IsValid() ? "valid" : "invalid")
            << ", Visual TF-Lite file is "
            << (visual_model.IsValid() ? "valid" : "invalid");

    ClearScorerAndNotifyObservers();
    return;
  }

  base::File embedding_model_duplicate;
  bool image_embedding_model_version_match =
      IsModelMetadataImageEmbeddingVersionMatching() &&
      HasImageEmbeddingModel();
  if (image_embedding_model_version_match) {
    if (const base::File& embedding_file = GetImageEmbeddingModel();
        embedding_file.IsValid()) {
      embedding_model_duplicate = embedding_file.Duplicate();
    }
  }
  base::UmaHistogramBoolean("SBClientPhishing.ImageEmbeddingModelVersionMatch",
                            image_embedding_model_version_match);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          &CreateScorerOnBackgroundThread, std::move(region),
          visual_model.Duplicate(), std::move(embedding_model_duplicate),
          GetImageEmbeddingInputWidth(), GetImageEmbeddingInputHeight()),
      base::BindOnce(&ClientSideDetectionService::OnScorerCreated,
                     weak_factory_.GetWeakPtr(), current_model_generation_));
}

void ClientSideDetectionService::OnScorerCreated(
    int generation_id,
    std::unique_ptr<Scorer> scorer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (generation_id != current_model_generation_) {
    return;  // Stale task, discard.
  }

  scorer_ = std::move(scorer);
  for (auto& observer : observers_) {
    observer.OnScorerChanged();
  }
}

void ClientSideDetectionService::ClearScorerAndNotifyObservers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scorer_.reset();
  for (auto& observer : observers_) {
    observer.OnScorerChanged();
  }
}

Scorer* ClientSideDetectionService::GetScorer() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return scorer_.get();
}

void ClientSideDetectionService::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void ClientSideDetectionService::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void ClientSideDetectionService::DidSendClientReportPhishingRequest(
    std::unique_ptr<ClientPhishingRequest> request,
    const std::string& access_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebUIIOSInfoSingleton::AddToClientPhishingRequestsSent,
                     base::Unretained(WebUIIOSInfoSingleton::GetInstance()),
                     std::move(request), access_token));
}

void ClientSideDetectionService::DidReceiveClientPhishingResponse(
    const ClientPhishingResponse& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebUIIOSInfoSingleton::AddToClientPhishingResponsesReceived,
          base::Unretained(WebUIIOSInfoSingleton::GetInstance()),
          std::make_unique<ClientPhishingResponse>(response)));
}

}  // namespace safe_browsing
