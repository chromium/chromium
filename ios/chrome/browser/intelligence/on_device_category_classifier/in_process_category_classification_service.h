// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_IN_PROCESS_CATEGORY_CLASSIFICATION_SERVICE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_IN_PROCESS_CATEGORY_CLASSIFICATION_SERVICE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#import "base/containers/lru_cache.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/observer_list.h"
#import "base/task/updateable_sequenced_task_runner.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#import "components/page_content_annotations/core/on_device_category_classifier.h"
#import "components/page_content_annotations/core/page_content_annotations_common.h"
#import "components/page_content_annotations/core/page_embeddings_common.h"
#import "components/passage_embeddings/core/passage_embeddings_types.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/classification_request_tracker.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/in_process_passage_embedder_wrapper.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/passage_embedder_model_loader.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"
#import "services/metrics/public/cpp/ukm_source_id.h"
#import "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"
#import "url/gurl.h"

class ProfileIOS;

// In-process service that manages passage embedding generation and on-device
// category classification.
class InProcessCategoryClassificationService
    : public KeyedService,
      public passage_embeddings::EmbedderMetadataProvider,
      public page_content_annotations::OnDeviceCategoryClassifier::Observer {
 public:
  using ClassificationCallback = base::OnceCallback<void(
      const std::vector<page_content_annotations::Category>&)>;

  struct CachedEmbeddings {
    std::optional<passage_embeddings::Embedding> title_url_embedding;
    std::vector<passage_embeddings::Embedding> passage_embeddings;
  };

  static InProcessCategoryClassificationService* GetForProfile(
      ProfileIOS* profile);
  static void EnsureFactoryBuilt();
  static ProfileKeyedServiceFactoryIOS* GetFactory();
  static ProfileKeyedServiceFactoryIOS::TestingFactory GetDefaultFactory();

  explicit InProcessCategoryClassificationService(
      optimization_guide::OptimizationGuideModelProvider* model_provider);
  ~InProcessCategoryClassificationService() override;

  InProcessCategoryClassificationService(
      const InProcessCategoryClassificationService&) = delete;
  InProcessCategoryClassificationService& operator=(
      const InProcessCategoryClassificationService&) = delete;

  // KeyedService:
  void Shutdown() override;

  // Classifies the extracted page context using local models. Uses
  // in-memory cache if embeddings are already available for the URL.
  //
  // On Desktop/content platforms, this equivalent pipeline is handled across
  // `page_content_annotations::GenerateEmbeddingsCandidates`,
  // `page_content_annotations::PageEmbeddingsService`, and
  // `page_content_annotations::PageCategoryClassifierBridgeImpl` before
  // invoking `page_content_annotations::OnDeviceCategoryClassifier`.
  virtual void ClassifyPageContext(const GURL& url,
                                   const std::string& title,
                                   const std::string& page_content,
                                   ukm::SourceId source_id,
                                   ClassificationCallback callback);

  // Returns true if embeddings for `url` exist in the in-memory cache.
  bool HasCachedEmbeddings(const GURL& url) const;

  // Classifies `url` using in-memory cached embeddings. If `url` is invalid
  // or not cached, `callback` is run with an empty result vector.
  virtual void ClassifyWithCachedEmbeddings(const GURL& url,
                                            ukm::SourceId source_id,
                                            ClassificationCallback callback);

  // Retrieves in-memory cached embeddings for `url`, if present.
  std::optional<CachedEmbeddings> GetCachedEmbeddings(const GURL& url) const;

  // Seeds the in-memory cache directly for testing.
  void SetCachedEmbeddingsForTesting(const GURL& url,
                                     CachedEmbeddings embeddings) {
    in_memory_embeddings_cache_.Put(url, std::move(embeddings));
  }

  // Resets the internal passage embedder for testing.
  void ResetPassageEmbedderForTesting() { embedder_wrapper_.Reset(); }

  // Simulates passage embedder load completion for testing.
  void OnPassageEmbedderLoadedForTesting(int64_t model_version,
                                         size_t output_size,
                                         bool success) {
    OnPassageEmbedderLoaded(model_version, output_size, success);
  }

  // Simulates embedder disconnect for testing.
  void OnEmbedderDisconnectForTesting() { OnEmbedderDisconnect(); }

  // passage_embeddings::EmbedderMetadataProvider:
  void AddObserver(
      passage_embeddings::EmbedderMetadataObserver* observer) override;
  void RemoveObserver(
      passage_embeddings::EmbedderMetadataObserver* observer) override;

  // page_content_annotations::OnDeviceCategoryClassifier::Observer:
  void OnCategoriesClassified(
      const GURL& url,
      ukm::SourceId source_id,
      const std::vector<page_content_annotations::Category>& categories)
      override;

 protected:
  virtual void OnGotEmbeddings(
      const GURL& url,
      ukm::SourceId source_id,
      std::vector<page_content_annotations::EmbeddingPassageType> passage_types,
      std::vector<passage_embeddings::mojom::PassageEmbeddingsResultPtr>
          results);

 private:
  void OnModelFilesOpened(base::File embeddings_file,
                          base::File sp_file,
                          uint32_t window_size,
                          int64_t model_version,
                          size_t output_size);

  void OnPassageEmbedderLoaded(int64_t model_version,
                               size_t output_size,
                               bool success);

  void OnEmbedderDisconnect();

  ClassificationRequestTracker request_tracker_;

  scoped_refptr<base::UpdateableSequencedTaskRunner> background_task_runner_;
  InProcessPassageEmbedderWrapper embedder_wrapper_;
  PassageEmbedderModelLoader model_loader_;

  raw_ptr<optimization_guide::OptimizationGuideModelProvider> model_provider_ =
      nullptr;

  // In-memory LRU cache of embeddings keyed by URL.
  base::LRUCache<GURL, CachedEmbeddings> in_memory_embeddings_cache_;

  std::unique_ptr<page_content_annotations::OnDeviceCategoryClassifier>
      category_classifier_;

  base::WeakPtrFactory<InProcessCategoryClassificationService>
      weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_IN_PROCESS_CATEGORY_CLASSIFICATION_SERVICE_H_
