// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_IN_PROCESS_CATEGORY_CLASSIFICATION_SERVICE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_IN_PROCESS_CATEGORY_CLASSIFICATION_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#import "base/containers/flat_map.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/observer_list.h"
#import "base/task/updateable_sequenced_task_runner.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#import "components/page_content_annotations/core/page_content_annotations_common.h"
#import "components/passage_embeddings/core/passage_embeddings_types.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/classification_request_tracker.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/in_process_passage_embedder_wrapper.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/passage_embedder_model_loader.h"
#import "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"
#import "url/gurl.h"

class ProfileIOS;

// In-process service that manages passage embedding generation and on-device
// category classification.
class InProcessCategoryClassificationService
    : public KeyedService,
      public passage_embeddings::EmbedderMetadataProvider {
 public:
  using ClassificationCallback = base::OnceCallback<void(
      const std::vector<page_content_annotations::Category>&)>;

  static InProcessCategoryClassificationService* GetForProfile(
      ProfileIOS* profile);
  static void EnsureFactoryBuilt();

  explicit InProcessCategoryClassificationService(
      optimization_guide::OptimizationGuideModelProvider* model_provider);
  ~InProcessCategoryClassificationService() override;

  InProcessCategoryClassificationService(
      const InProcessCategoryClassificationService&) = delete;
  InProcessCategoryClassificationService& operator=(
      const InProcessCategoryClassificationService&) = delete;

  // Classifies the extracted page context using local models.
  virtual void ClassifyPageContext(const GURL& url,
                                   const std::string& title,
                                   const std::string& page_content,
                                   ClassificationCallback callback);

  // passage_embeddings::EmbedderMetadataProvider:
  void AddObserver(
      passage_embeddings::EmbedderMetadataObserver* observer) override;
  void RemoveObserver(
      passage_embeddings::EmbedderMetadataObserver* observer) override;

 protected:
  virtual void OnGotEmbeddings(
      const GURL& url,
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

  base::WeakPtrFactory<InProcessCategoryClassificationService>
      weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_IN_PROCESS_CATEGORY_CLASSIFICATION_SERVICE_H_
