// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/on_device_category_classifier/passage_embedder_model_loader.h"

#import "base/check.h"
#import "base/check_op.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/task/thread_pool.h"
#import "components/optimization_guide/core/delivery/model_info.h"
#import "components/optimization_guide/core/optimization_guide_util.h"
#import "components/optimization_guide/proto/passage_embeddings_model_metadata.pb.h"

namespace {

std::pair<base::File, base::File> OpenModelFiles(
    const base::FilePath& embeddings_path,
    const base::FilePath& sp_path) {
  base::File embeddings_file(embeddings_path,
                             base::File::FLAG_OPEN | base::File::FLAG_READ);
  base::File sp_file(sp_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  return {std::move(embeddings_file), std::move(sp_file)};
}

}  // namespace

PassageEmbedderModelLoader::PassageEmbedderModelLoader(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    FilesOpenedCallback on_files_opened_callback,
    ModelUnloadedCallback on_model_unloaded_callback)
    : model_provider_(model_provider),
      on_files_opened_callback_(std::move(on_files_opened_callback)),
      on_model_unloaded_callback_(std::move(on_model_unloaded_callback)) {
  DCHECK(model_provider_);
  model_provider_->AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OPTIMIZATION_TARGET_PASSAGE_EMBEDDER,
      /*model_metadata=*/std::nullopt,
      base::SequencedTaskRunner::GetCurrentDefault(), this);
}

PassageEmbedderModelLoader::~PassageEmbedderModelLoader() {
  model_provider_->RemoveObserverForOptimizationTargetModel(
      optimization_guide::proto::OPTIMIZATION_TARGET_PASSAGE_EMBEDDER, this);
}

bool PassageEmbedderModelLoader::IsModelLoaded() const {
  return current_metadata_.has_value();
}

void PassageEmbedderModelLoader::NotifyEmbedderMetadata(
    const passage_embeddings::EmbedderMetadata& metadata) {
  current_metadata_ = metadata;
  for (auto& observer : metadata_observers_) {
    observer.EmbedderMetadataUpdated(*current_metadata_);
  }
}

void PassageEmbedderModelLoader::ResetMetadata() {
  current_metadata_.reset();
}

void PassageEmbedderModelLoader::AddObserver(
    passage_embeddings::EmbedderMetadataObserver* observer) {
  metadata_observers_.AddObserver(observer);
}

void PassageEmbedderModelLoader::RemoveObserver(
    passage_embeddings::EmbedderMetadataObserver* observer) {
  metadata_observers_.RemoveObserver(observer);
}

void PassageEmbedderModelLoader::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  DCHECK_EQ(optimization_target,
            optimization_guide::proto::OPTIMIZATION_TARGET_PASSAGE_EMBEDDER);

  if (!model_info.has_value()) {
    on_model_unloaded_callback_.Run();
    return;
  }

  base::FilePath embeddings_path = model_info->model_file_path;
  base::flat_set<base::FilePath> additional_files =
      model_info->additional_files;
  base::FilePath sp_path;
  for (const auto& file : additional_files) {
    if (file.Extension() == FILE_PATH_LITERAL(".model") ||
        file.BaseName().value() == FILE_PATH_LITERAL("sentencepiece.model")) {
      sp_path = file;
      break;
    }
  }
  if (sp_path.empty() && !additional_files.empty()) {
    sp_path = *(additional_files.begin());
  }
  if (sp_path.empty()) {
    sp_path = embeddings_path.DirName().AppendASCII("sentencepiece.model");
  }

  uint32_t input_window_size = 64;
  size_t output_size = 768;
  const std::optional<optimization_guide::proto::Any>& metadata =
      model_info->model_metadata;
  if (metadata.has_value()) {
    std::optional<optimization_guide::proto::PassageEmbeddingsModelMetadata>
        embeddings_metadata = optimization_guide::ParsedAnyMetadata<
            optimization_guide::proto::PassageEmbeddingsModelMetadata>(
            *metadata);
    if (embeddings_metadata) {
      if (embeddings_metadata->input_window_size() > 0) {
        input_window_size = embeddings_metadata->input_window_size();
      }
      if (embeddings_metadata->output_size() > 0) {
        output_size = embeddings_metadata->output_size();
      }
    }
  }

  int64_t model_version = model_info->version;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&OpenModelFiles, embeddings_path, sp_path),
      base::BindOnce(&PassageEmbedderModelLoader::OnModelFilesOpened,
                     weak_ptr_factory_.GetWeakPtr(), model_version, output_size,
                     input_window_size));
}

void PassageEmbedderModelLoader::OnModelFilesOpened(
    int64_t model_version,
    size_t output_size,
    uint32_t window_size,
    std::pair<base::File, base::File> files) {
  if (!files.first.IsValid() || !files.second.IsValid()) {
    on_model_unloaded_callback_.Run();
    return;
  }
  on_files_opened_callback_.Run(std::move(files.first), std::move(files.second),
                                window_size, model_version, output_size);
}
