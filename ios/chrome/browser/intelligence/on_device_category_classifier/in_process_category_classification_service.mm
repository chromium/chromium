// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/on_device_category_classifier/in_process_category_classification_service.h"

#import <utility>

#import "base/check.h"
#import "base/check_op.h"
#import "base/files/file.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/no_destructor.h"
#import "base/strings/strcat.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/page_content_annotations/core/page_embeddings_common.h"
#import "components/page_content_annotations/core/simple_page_content_verbalization.h"
#import "components/passage_embeddings/core/passage_embeddings_features.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace {

constexpr size_t kDefaultCacheCapacity = 50;

std::unique_ptr<KeyedService> BuildInProcessCategoryClassificationService(
    ProfileIOS* profile) {
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

  static TestingFactory GetDefaultFactory() {
    return base::BindRepeating(&BuildInProcessCategoryClassificationService);
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
    return BuildInProcessCategoryClassificationService(profile);
  }
};

}  // namespace

#pragma mark - Service Implementation

// static
InProcessCategoryClassificationService*
InProcessCategoryClassificationService::GetForProfile(ProfileIOS* profile) {
  return InProcessCategoryClassificationServiceFactory::GetForProfile(profile);
}

// static
void InProcessCategoryClassificationService::EnsureFactoryBuilt() {
  InProcessCategoryClassificationServiceFactory::GetInstance();
}

// static
ProfileKeyedServiceFactoryIOS*
InProcessCategoryClassificationService::GetFactory() {
  return InProcessCategoryClassificationServiceFactory::GetInstance();
}

// static
ProfileKeyedServiceFactoryIOS::TestingFactory
InProcessCategoryClassificationService::GetDefaultFactory() {
  return InProcessCategoryClassificationServiceFactory::GetDefaultFactory();
}

InProcessCategoryClassificationService::InProcessCategoryClassificationService(
    optimization_guide::OptimizationGuideModelProvider* model_provider)
    : background_task_runner_(
          base::ThreadPool::CreateUpdateableSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
               base::ThreadPolicy::MUST_USE_FOREGROUND})),
      // Note: `base::Unretained(this)` is safe here because `this` owns both
      // `embedder_wrapper_` and `model_loader_`. Any background tasks initiated
      // by them use their own WeakPtrs, preventing callback invocation after
      // destruction.
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
              base::Unretained(this))),
      model_provider_(model_provider),
      in_memory_embeddings_cache_(kDefaultCacheCapacity) {
  embedder_wrapper_.EnsurePassageEmbedder();
  category_classifier_ =
      std::make_unique<page_content_annotations::OnDeviceCategoryClassifier>(
          model_provider_, this);
  // Note: `this` is passed to OnDeviceCategoryClassifier's constructor as
  // `passage_embeddings::EmbedderMetadataProvider*`, not as an Observer.
  // Explicitly calling AddObserver is required to register as an
  // OnDeviceCategoryClassifier::Observer.
  category_classifier_->AddObserver(this);
}

InProcessCategoryClassificationService::
    ~InProcessCategoryClassificationService() {
  if (category_classifier_) {
    category_classifier_->RemoveObserver(this);
  }
}

void InProcessCategoryClassificationService::Shutdown() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  request_tracker_.CancelAll();
}

bool InProcessCategoryClassificationService::HasCachedEmbeddings(
    const GURL& url) const {
  if (!url.is_valid()) {
    return false;
  }
  return in_memory_embeddings_cache_.Peek(url) !=
         in_memory_embeddings_cache_.end();
}

std::optional<InProcessCategoryClassificationService::CachedEmbeddings>
InProcessCategoryClassificationService::GetCachedEmbeddings(
    const GURL& url) const {
  auto it = in_memory_embeddings_cache_.Peek(url);
  if (it != in_memory_embeddings_cache_.end()) {
    return it->second;
  }
  return std::nullopt;
}

void InProcessCategoryClassificationService::ClassifyWithCachedEmbeddings(
    const GURL& url,
    ukm::SourceId source_id,
    ClassificationCallback callback) {
  if (!url.is_valid()) {
    std::move(callback).Run({});
    return;
  }

  auto cached_it = in_memory_embeddings_cache_.Get(url);
  if (cached_it == in_memory_embeddings_cache_.end()) {
    std::move(callback).Run({});
    return;
  }

  if (request_tracker_.AttachInFlight(url, std::move(callback))) {
    return;
  }

  if (category_classifier_) {
    category_classifier_->OnPageEmbeddingAvailable(
        url, source_id, cached_it->second.title_url_embedding,
        cached_it->second.passage_embeddings);
  } else {
    request_tracker_.CompleteUrl(url, {});
  }
}

void InProcessCategoryClassificationService::ClassifyPageContext(
    const GURL& url,
    const std::string& title,
    const std::string& page_content,
    ukm::SourceId source_id,
    ClassificationCallback callback) {
  // Desktop's EmbeddingsCandidateGenerator only creates a title/URL candidate
  // when both title and URL are present. If a title/URL embedding is missing,
  // OnDeviceCategoryClassifier skips classification and returns empty results.
  if (!url.is_valid() || title.empty()) {
    std::move(callback).Run({});
    return;
  }

  if (page_content.empty()) {
    std::move(callback).Run({});
    return;
  }

  // In-Memory Cache hit: Reuse existing embeddings without re-embedding.
  if (HasCachedEmbeddings(url)) {
    ClassifyWithCachedEmbeddings(url, source_id, std::move(callback));
    return;
  }

  if (!model_loader_.IsModelLoaded()) {
    request_tracker_.EnqueuePending(
        ClassificationRequestTracker::PendingClassification{
            .url = url,
            .title = title,
            .page_content = page_content,
            .source_id = source_id,
            .callback = std::move(callback),
        });
    return;
  }

  if (request_tracker_.AttachInFlight(url, std::move(callback))) {
    return;
  }

  const size_t max_words_per_aggregate_passage =
      passage_embeddings::kMaxWordsPerAggregatePassage.Get();
  const size_t min_words_per_passage =
      passage_embeddings::kMinWordsPerPassage.Get();

  // Create candidate passages matching Desktop's GenerateEmbeddingsCandidates
  // in page_content_annotations/content/embeddings_candidate_generator.cc.
  std::vector<std::string> content_passages =
      page_content_annotations::CreatePassagesFromText(
          page_content, max_words_per_aggregate_passage, min_words_per_passage);

  std::vector<std::string> string_passages;
  std::vector<page_content_annotations::EmbeddingPassageType> passage_types;
  string_passages.reserve(content_passages.size() + 1);
  passage_types.reserve(content_passages.size() + 1);

  for (std::string& passage : content_passages) {
    string_passages.push_back(std::move(passage));
    passage_types.push_back(
        page_content_annotations::EmbeddingPassageType::kPageContent);
  }

  string_passages.push_back(base::StrCat({title, " - ", url.spec()}));
  passage_types.push_back(
      page_content_annotations::EmbeddingPassageType::kTitleAndUrl);

  embedder_wrapper_.GenerateEmbeddings(
      string_passages,
      base::BindOnce(&InProcessCategoryClassificationService::OnGotEmbeddings,
                     weak_ptr_factory_.GetWeakPtr(), url, source_id,
                     std::move(passage_types)));
}

void InProcessCategoryClassificationService::OnGotEmbeddings(
    const GURL& url,
    ukm::SourceId source_id,
    std::vector<page_content_annotations::EmbeddingPassageType> passage_types,
    std::vector<passage_embeddings::mojom::PassageEmbeddingsResultPtr>
        results) {
  if (results.empty() || !category_classifier_) {
    request_tracker_.CompleteUrl(url, {});
    return;
  }

  // Matches Desktop's
  // PageCategoryClassifierBridgeImpl::OnPageEmbeddingsAvailable in
  // page_category_classifier_bridge_impl.cc.
  std::optional<passage_embeddings::Embedding> title_url_embedding;
  std::vector<passage_embeddings::Embedding> passage_embeddings;
  for (size_t i = 0; i < results.size() && i < passage_types.size(); ++i) {
    if (!results[i] || results[i]->embeddings.empty()) {
      continue;
    }
    if (passage_types[i] ==
        page_content_annotations::EmbeddingPassageType::kTitleAndUrl) {
      title_url_embedding =
          passage_embeddings::Embedding(std::move(results[i]->embeddings));
    } else if (passage_types[i] ==
               page_content_annotations::EmbeddingPassageType::kPageContent) {
      passage_embeddings.emplace_back(std::move(results[i]->embeddings));
    }
  }

  // Store in-memory cached embeddings.
  in_memory_embeddings_cache_.Put(
      url, CachedEmbeddings{
               .title_url_embedding = title_url_embedding,
               .passage_embeddings = passage_embeddings,
           });

  category_classifier_->OnPageEmbeddingAvailable(url, source_id,
                                                 std::move(title_url_embedding),
                                                 std::move(passage_embeddings));
}

void InProcessCategoryClassificationService::OnCategoriesClassified(
    const GURL& url,
    ukm::SourceId source_id,
    const std::vector<page_content_annotations::Category>& categories) {
  request_tracker_.CompleteUrl(url, categories);
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
    // CancelAll drains all pending requests and in-flight callbacks, running
    // each callback with empty results so requests never hang on load failure.
    request_tracker_.CancelAll();
    in_memory_embeddings_cache_.Clear();
    return;
  }

  // Clear stale embeddings from any previous model version.
  in_memory_embeddings_cache_.Clear();

  model_loader_.NotifyEmbedderMetadata(
      passage_embeddings::EmbedderMetadata(model_version, output_size));

  std::vector<ClassificationRequestTracker::PendingClassification> queued =
      request_tracker_.DrainPending();
  for (auto& req : queued) {
    ClassifyPageContext(req.url, req.title, req.page_content, req.source_id,
                        std::move(req.callback));
  }
}

void InProcessCategoryClassificationService::OnEmbedderDisconnect() {
  embedder_wrapper_.Reset();
  model_loader_.ResetMetadata();
  request_tracker_.CancelAll();
  in_memory_embeddings_cache_.Clear();
}

void InProcessCategoryClassificationService::AddObserver(
    passage_embeddings::EmbedderMetadataObserver* observer) {
  model_loader_.AddObserver(observer);
}

void InProcessCategoryClassificationService::RemoveObserver(
    passage_embeddings::EmbedderMetadataObserver* observer) {
  model_loader_.RemoveObserver(observer);
}
