// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/on_device_category_classifier/passage_embedder_model_loader.h"

#import <optional>
#import <utility>

#import "base/files/file.h"
#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/functional/callback.h"
#import "base/run_loop.h"
#import "base/test/bind.h"
#import "components/optimization_guide/core/delivery/model_info.h"
#import "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#import "components/optimization_guide/proto/common_types.pb.h"
#import "components/optimization_guide/proto/passage_embeddings_model_metadata.pb.h"
#import "components/passage_embeddings/core/passage_embeddings_types.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class TestEmbedderMetadataObserver
    : public passage_embeddings::EmbedderMetadataObserver {
 public:
  TestEmbedderMetadataObserver() = default;
  ~TestEmbedderMetadataObserver() override = default;

  void EmbedderMetadataUpdated(
      passage_embeddings::EmbedderMetadata metadata) override {
    metadata_updated_called_ = true;
    last_metadata_ = metadata;
  }

  bool metadata_updated_called_ = false;
  std::optional<passage_embeddings::EmbedderMetadata> last_metadata_;
};

}  // namespace

class PassageEmbedderModelLoaderTest : public PlatformTest {
 protected:
  PassageEmbedderModelLoaderTest() = default;

  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void TearDown() override { PlatformTest::TearDown(); }

  web::WebTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  optimization_guide::TestOptimizationGuideModelProvider model_provider_;
};

// Tests initial state and observer registration.
TEST_F(PassageEmbedderModelLoaderTest, InitialState) {
  PassageEmbedderModelLoader loader(
      &model_provider_,
      base::BindRepeating(
          [](base::File, base::File, uint32_t, int64_t, size_t) {}),
      base::BindRepeating([]() {}));

  EXPECT_FALSE(loader.IsModelLoaded());

  TestEmbedderMetadataObserver observer;
  loader.AddObserver(&observer);
  loader.RemoveObserver(&observer);
}

// Tests metadata notification and resetting.
TEST_F(PassageEmbedderModelLoaderTest, NotifyEmbedderMetadataAndReset) {
  PassageEmbedderModelLoader loader(
      &model_provider_,
      base::BindRepeating(
          [](base::File, base::File, uint32_t, int64_t, size_t) {}),
      base::BindRepeating([]() {}));

  TestEmbedderMetadataObserver observer;
  loader.AddObserver(&observer);

  passage_embeddings::EmbedderMetadata metadata(123, 768, 0.5);
  loader.NotifyEmbedderMetadata(metadata);

  EXPECT_TRUE(loader.IsModelLoaded());
  EXPECT_TRUE(observer.metadata_updated_called_);
  ASSERT_TRUE(observer.last_metadata_.has_value());
  EXPECT_EQ(123, observer.last_metadata_->model_version);
  EXPECT_EQ(768u, observer.last_metadata_->output_size);
  EXPECT_EQ(0.5, observer.last_metadata_->search_score_threshold);

  loader.ResetMetadata();
  EXPECT_FALSE(loader.IsModelLoaded());

  loader.RemoveObserver(&observer);
}

// Tests that unloaded callback runs when ModelInfo is nullopt.
TEST_F(PassageEmbedderModelLoaderTest, OnModelUpdatedUnloaded) {
  bool unloaded_called = false;
  PassageEmbedderModelLoader loader(
      &model_provider_,
      base::BindRepeating(
          [](base::File, base::File, uint32_t, int64_t, size_t) {}),
      base::BindLambdaForTesting([&]() { unloaded_called = true; }));

  auto* model_observer =
      static_cast<optimization_guide::OptimizationTargetModelObserver*>(
          &loader);
  model_observer->OnModelUpdated(
      optimization_guide::proto::OPTIMIZATION_TARGET_PASSAGE_EMBEDDER,
      std::nullopt);

  EXPECT_TRUE(unloaded_called);
}

// Tests that unloaded callback runs when model files do not exist.
TEST_F(PassageEmbedderModelLoaderTest, OnModelUpdatedInvalidFiles) {
  base::RunLoop run_loop;
  bool unloaded_called = false;
  PassageEmbedderModelLoader loader(
      &model_provider_,
      base::BindRepeating(
          [](base::File, base::File, uint32_t, int64_t, size_t) {}),
      base::BindLambdaForTesting([&]() {
        unloaded_called = true;
        run_loop.Quit();
      }));

  optimization_guide::ModelInfo model_info;
  model_info.model_file_path =
      temp_dir_.GetPath().AppendASCII("non_existent.bin");
  model_info.version = 1;

  auto* model_observer =
      static_cast<optimization_guide::OptimizationTargetModelObserver*>(
          &loader);
  model_observer->OnModelUpdated(
      optimization_guide::proto::OPTIMIZATION_TARGET_PASSAGE_EMBEDDER,
      model_info);

  run_loop.Run();
  EXPECT_TRUE(unloaded_called);
}

// Tests successful file opening and metadata parsing on model update.
TEST_F(PassageEmbedderModelLoaderTest, OnModelUpdatedValidFiles) {
  base::RunLoop run_loop;
  bool files_opened_called = false;
  uint32_t opened_window_size = 0;
  int64_t opened_version = 0;
  size_t opened_output_size = 0;
  bool files_valid = false;

  PassageEmbedderModelLoader loader(
      &model_provider_,
      base::BindLambdaForTesting([&](base::File file1, base::File file2,
                                     uint32_t window_size, int64_t version,
                                     size_t output_size) {
        files_opened_called = true;
        files_valid = file1.IsValid() && file2.IsValid();
        opened_window_size = window_size;
        opened_version = version;
        opened_output_size = output_size;
        run_loop.Quit();
      }),
      base::BindRepeating([]() {}));

  base::FilePath model_path =
      temp_dir_.GetPath().AppendASCII("embeddings.model");
  base::FilePath sp_path =
      temp_dir_.GetPath().AppendASCII("sentencepiece.model");
  ASSERT_TRUE(base::WriteFile(model_path, "dummy_model"));
  ASSERT_TRUE(base::WriteFile(sp_path, "dummy_sp"));

  optimization_guide::ModelInfo model_info;
  model_info.model_file_path = model_path;
  model_info.additional_files.push_back(sp_path);
  model_info.version = 42;

  optimization_guide::proto::PassageEmbeddingsModelMetadata embeddings_metadata;
  embeddings_metadata.set_input_window_size(128);
  embeddings_metadata.set_output_size(256);
  optimization_guide::proto::Any any;
  any.set_type_url("type.googleapis.com/"
                   "google.internal.chrome.optimizationguide.v1."
                   "PassageEmbeddingsModelMetadata");
  embeddings_metadata.SerializeToString(any.mutable_value());
  model_info.model_metadata = any;

  auto* model_observer =
      static_cast<optimization_guide::OptimizationTargetModelObserver*>(
          &loader);
  model_observer->OnModelUpdated(
      optimization_guide::proto::OPTIMIZATION_TARGET_PASSAGE_EMBEDDER,
      model_info);

  run_loop.Run();

  EXPECT_TRUE(files_opened_called);
  EXPECT_TRUE(files_valid);
  EXPECT_EQ(128u, opened_window_size);
  EXPECT_EQ(42, opened_version);
  EXPECT_EQ(256u, opened_output_size);
}
