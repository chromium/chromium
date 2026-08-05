// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/on_device_category_classifier/in_process_category_classification_service.h"

#import "base/files/file.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/no_destructor.h"
#import "base/strings/strcat.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "components/page_content_annotations/core/simple_page_content_verbalization.h"
#import "components/passage_embeddings/core/passage_embeddings_features.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace {

class InProcessCategoryClassificationServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static InProcessCategoryClassificationService* GetForProfile(
      ProfileIOS* profile) {
    return GetInstance()
        ->GetServiceForProfileAs<InProcessCategoryClassificationService>(
            profile, /*create=*/true);
  }

  static InProcessCategoryClassificationServiceFactory* GetInstance() {
    static base::NoDestructor<InProcessCategoryClassificationServiceFactory>
        instance;
    return instance.get();
  }

 private:
  friend class base::NoDestructor<
      InProcessCategoryClassificationServiceFactory>;

  InProcessCategoryClassificationServiceFactory()
      : ProfileKeyedServiceFactoryIOS("InProcessCategoryClassificationService",
                                      ProfileSelection::kOwnInstanceInIncognito,
                                      ServiceCreation::kCreateWithProfile,
                                      TestingCreation::kNoServiceForTests) {
    DependsOn(OptimizationGuideServiceFactory::GetInstance());
  }

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override {
    if (profile->IsOffTheRecord()) {
      return nullptr;
    }
    OptimizationGuideService* opt_guide =
        OptimizationGuideServiceFactory::GetForProfile(profile);
    if (!opt_guide) {
      return nullptr;
    }
    return std::make_unique<InProcessCategoryClassificationService>(opt_guide);
  }
};

}  // namespace

// static
InProcessCategoryClassificationService*
InProcessCategoryClassificationService::GetForProfile(ProfileIOS* profile) {
  return InProcessCategoryClassificationServiceFactory::GetForProfile(profile);
}

// static
void InProcessCategoryClassificationService::EnsureFactoryBuilt() {
  InProcessCategoryClassificationServiceFactory::GetInstance();
}

void InProcessCategoryClassificationService::OnEmbedderDisconnect() {
  embedder_wrapper_.Reset();
  model_loader_.ResetMetadata();
  request_tracker_.CancelAll();
}

InProcessCategoryClassificationService::InProcessCategoryClassificationService(
    optimization_guide::OptimizationGuideModelProvider* model_provider)
    : background_task_runner_(
          base::ThreadPool::CreateUpdateableSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
               base::ThreadPolicy::MUST_USE_FOREGROUND})),
      embedder_wrapper_(
          background_task_runner_,
          base::BindRepeating(
              &InProcessCategoryClassificationService::OnEmbedderDisconnect,
              base::Unretained(this))),
      model_loader_(
          model_provider,
          base::BindRepeating(
              &InProcessCategoryClassificationService::OnModelFilesOpened,
              base::Unretained(this)),
          base::BindRepeating(
              &InProcessCategoryClassificationService::OnEmbedderDisconnect,
              base::Unretained(this))) {
  embedder_wrapper_.EnsurePassageEmbedder();
}

InProcessCategoryClassificationService::
    ~InProcessCategoryClassificationService() = default;

void InProcessCategoryClassificationService::ClassifyPageContext(
    const GURL& url,
    const std::string& title,
    const std::string& page_content,
    ClassificationCallback callback) {
  if (page_content.empty()) {
    std::move(callback).Run({});
    return;
  }

  if (!model_loader_.IsModelLoaded()) {
    request_tracker_.EnqueuePending(
        {url, title, page_content, std::move(callback)});
    return;
  }

  const size_t max_words_per_aggregate_passage =
      passage_embeddings::kMaxWordsPerAggregatePassage.Get();
  const size_t min_words_per_passage =
      passage_embeddings::kMinWordsPerPassage.Get();
  std::vector<std::string> passages =
      page_content_annotations::CreatePassagesFromText(
          page_content, max_words_per_aggregate_passage, min_words_per_passage);

  if (!title.empty()) {
    passages.push_back(title);
    if (!url.is_empty()) {
      passages.push_back(base::StrCat({title, " - ", url.spec()}));
    }
  }

  if (passages.empty()) {
    std::move(callback).Run({});
    return;
  }

  if (request_tracker_.AttachInFlight(url, std::move(callback))) {
    return;
  }

  embedder_wrapper_.GenerateEmbeddings(
      passages,
      base::BindOnce(&InProcessCategoryClassificationService::OnGotEmbeddings,
                     weak_ptr_factory_.GetWeakPtr(), url));
}

void InProcessCategoryClassificationService::OnGotEmbeddings(
    const GURL& url,
    std::vector<passage_embeddings::mojom::PassageEmbeddingsResultPtr>
        results) {
  // TODO(crbug.com/391851838): Feed passage embeddings into the category
  // classification model to produce actual category scores.
  request_tracker_.CompleteUrl(url, {});
}

void InProcessCategoryClassificationService::OnModelFilesOpened(
    base::File embeddings_file,
    base::File sp_file,
    uint32_t window_size,
    int64_t model_version,
    size_t output_size) {
  embedder_wrapper_.LoadModels(
      std::move(embeddings_file), std::move(sp_file), window_size,
      base::BindOnce(
          &InProcessCategoryClassificationService::OnPassageEmbedderLoaded,
          weak_ptr_factory_.GetWeakPtr(), model_version, output_size));
}

void InProcessCategoryClassificationService::OnPassageEmbedderLoaded(
    int64_t model_version,
    size_t output_size,
    bool success) {
  if (!success) {
    model_loader_.ResetMetadata();
    embedder_wrapper_.Reset();
    request_tracker_.CancelAll();
    return;
  }

  model_loader_.NotifyEmbedderMetadata(
      passage_embeddings::EmbedderMetadata(model_version, output_size));

  std::vector<ClassificationRequestTracker::PendingClassification> queued =
      request_tracker_.DrainPending();
  for (auto& req : queued) {
    ClassifyPageContext(req.url, req.title, req.page_content,
                        std::move(req.callback));
  }
}

void InProcessCategoryClassificationService::AddObserver(
    passage_embeddings::EmbedderMetadataObserver* observer) {
  model_loader_.AddObserver(observer);
}

void InProcessCategoryClassificationService::RemoveObserver(
    passage_embeddings::EmbedderMetadataObserver* observer) {
  model_loader_.RemoveObserver(observer);
}
