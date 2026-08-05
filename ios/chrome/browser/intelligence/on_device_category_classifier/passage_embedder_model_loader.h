// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_PASSAGE_EMBEDDER_MODEL_LOADER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_PASSAGE_EMBEDDER_MODEL_LOADER_H_

#include <optional>
#include <utility>

#import "base/files/file.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/observer_list.h"
#import "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#import "components/optimization_guide/core/delivery/optimization_target_model_observer.h"
#import "components/passage_embeddings/core/passage_embeddings_types.h"

// Observes optimization guide model updates, parses metadata, opens model files
// on a background task runner, and manages embedder metadata observers.
class PassageEmbedderModelLoader
    : public passage_embeddings::EmbedderMetadataProvider,
      public optimization_guide::OptimizationTargetModelObserver {
 public:
  using FilesOpenedCallback = base::RepeatingCallback<
      void(base::File, base::File, uint32_t, int64_t, size_t)>;
  using ModelUnloadedCallback = base::RepeatingClosure;

  PassageEmbedderModelLoader(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      FilesOpenedCallback on_files_opened_callback,
      ModelUnloadedCallback on_model_unloaded_callback);
  ~PassageEmbedderModelLoader() override;

  PassageEmbedderModelLoader(const PassageEmbedderModelLoader&) = delete;
  PassageEmbedderModelLoader& operator=(const PassageEmbedderModelLoader&) =
      delete;

  // Returns true if the model has been loaded and metadata is available.
  bool IsModelLoaded() const;

  // Notifies metadata observers that the model was successfully loaded.
  void NotifyEmbedderMetadata(
      const passage_embeddings::EmbedderMetadata& metadata);

  // Resets current embedder metadata.
  void ResetMetadata();

  // passage_embeddings::EmbedderMetadataProvider:
  void AddObserver(
      passage_embeddings::EmbedderMetadataObserver* observer) override;
  void RemoveObserver(
      passage_embeddings::EmbedderMetadataObserver* observer) override;

 private:
  // optimization_guide::OptimizationTargetModelObserver:
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      base::optional_ref<const optimization_guide::ModelInfo> model_info)
      override;

  void OnModelFilesOpened(int64_t model_version,
                          size_t output_size,
                          uint32_t window_size,
                          std::pair<base::File, base::File> files);

  raw_ptr<optimization_guide::OptimizationGuideModelProvider> model_provider_ =
      nullptr;
  FilesOpenedCallback on_files_opened_callback_;
  ModelUnloadedCallback on_model_unloaded_callback_;

  base::ObserverList<passage_embeddings::EmbedderMetadataObserver>
      metadata_observers_;
  std::optional<passage_embeddings::EmbedderMetadata> current_metadata_;

  base::WeakPtrFactory<PassageEmbedderModelLoader> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_PASSAGE_EMBEDDER_MODEL_LOADER_H_
