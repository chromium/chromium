// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/passage_embeddings/passage_embeddings_service.h"

#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/passage_embeddings/passage_embedder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace passage_embeddings {
namespace {

constexpr uint32_t kInputWindowSize = 256;
constexpr size_t kEmbeddingsOutputSize = 768;

class PassageEmbeddingsServiceTest : public testing::Test {
 public:
  PassageEmbeddingsServiceTest()
      : service_impl_(service_.BindNewPipeAndPassReceiver()) {}

  mojo::Remote<mojom::PassageEmbeddingsService>& service() { return service_; }

  void SetUp() override {
    base::FilePath test_data_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir);
    test_data_dir = test_data_dir.AppendASCII("services")
                        .AppendASCII("test")
                        .AppendASCII("data")
                        .AppendASCII("passage_embeddings");
    embeddings_path_ =
        test_data_dir.AppendASCII("dummy_embeddings_model.tflite");
    sp_path_ = test_data_dir.AppendASCII("sentencepiece.model");
  }

  mojom::PassageEmbeddingsLoadModelsParamsPtr MakeParams(
      base::FilePath embeddings_path,
      base::FilePath sp_path,
      uint32_t input_window_size) {
    auto params = mojom::PassageEmbeddingsLoadModelsParams::New();
    params->embeddings_model = base::File(
        embeddings_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    params->sp_model =
        base::File(sp_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    params->input_window_size = input_window_size;
    return params;
  }

 protected:
  base::FilePath embeddings_path_;
  base::FilePath sp_path_;
  base::HistogramTester histogram_tester_;

 private:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::PassageEmbeddingsService> service_;
  PassageEmbeddingsService service_impl_;
};

TEST_F(PassageEmbeddingsServiceTest, LoadValidModels) {
  mojom::PassageEmbeddingsLoadModelsParamsPtr params =
      MakeParams(embeddings_path_, sp_path_, kInputWindowSize);
  mojo::Remote<mojom::PassageEmbedder> embedder_remote;
  base::test::TestFuture<bool> future;
  service()->LoadModels(std::move(params),
                        embedder_remote.BindNewPipeAndPassReceiver(),
                        future.GetCallback());
  bool load_models_success = future.Get();
  EXPECT_TRUE(load_models_success);
}

TEST_F(PassageEmbeddingsServiceTest, LoadModelsWithInvalidEmbeddingsModel) {
  mojom::PassageEmbeddingsLoadModelsParamsPtr params =
      MakeParams(sp_path_, sp_path_, kInputWindowSize);
  mojo::Remote<mojom::PassageEmbedder> embedder_remote;

  base::test::TestFuture<bool> load_models_future;
  service()->LoadModels(std::move(params),
                        embedder_remote.BindNewPipeAndPassReceiver(),
                        load_models_future.GetCallback());
  bool load_models_success = load_models_future.Get();
  // LoadModels succeeds since the model file can still be read.
  EXPECT_TRUE(load_models_success);

  base::test::TestFuture<std::vector<mojom::PassageEmbeddingsResultPtr>>
      execute_future;
  embedder_remote->GenerateEmbeddings({"foo"},
                                      mojom::PassagePriority::kUserInitiated,
                                      execute_future.GetCallback());
  std::vector<mojom::PassageEmbeddingsResultPtr> results =
      execute_future.Take();
  // Execution fails since the embeddings model is invalid.
  EXPECT_EQ(results.size(), 0u);
}

TEST_F(PassageEmbeddingsServiceTest, LoadModelsWithInvalidSpModel) {
  mojom::PassageEmbeddingsLoadModelsParamsPtr params =
      MakeParams(embeddings_path_, embeddings_path_, kInputWindowSize);
  mojo::Remote<mojom::PassageEmbedder> embedder_remote;
  base::test::TestFuture<bool> future;
  service()->LoadModels(std::move(params),
                        embedder_remote.BindNewPipeAndPassReceiver(),
                        future.GetCallback());
  bool load_models_success = future.Get();
  EXPECT_FALSE(load_models_success);
}

TEST_F(PassageEmbeddingsServiceTest, LoadModelsWithInvalidInputWindowSize) {
  mojom::PassageEmbeddingsLoadModelsParamsPtr params =
      MakeParams(embeddings_path_, sp_path_, 0u);
  mojo::Remote<mojom::PassageEmbedder> embedder_remote;
  base::test::TestFuture<bool> future;
  service()->LoadModels(std::move(params),
                        embedder_remote.BindNewPipeAndPassReceiver(),
                        future.GetCallback());
  bool load_models_success = future.Get();
  EXPECT_FALSE(load_models_success);
}

TEST_F(PassageEmbeddingsServiceTest, RespondsWithEmbeddings) {
  mojom::PassageEmbeddingsLoadModelsParamsPtr params =
      MakeParams(embeddings_path_, sp_path_, kInputWindowSize);
  mojo::Remote<mojom::PassageEmbedder> embedder_remote;

  base::test::TestFuture<bool> load_models_future;
  service()->LoadModels(std::move(params),
                        embedder_remote.BindNewPipeAndPassReceiver(),
                        load_models_future.GetCallback());
  bool load_models_success = load_models_future.Get();
  EXPECT_TRUE(load_models_success);

  base::test::TestFuture<std::vector<mojom::PassageEmbeddingsResultPtr>>
      execute_future;
  embedder_remote->GenerateEmbeddings({"hello", "world"},
                                      mojom::PassagePriority::kUserInitiated,
                                      execute_future.GetCallback());
  auto results = execute_future.Take();
  EXPECT_EQ(results.size(), 2u);
  EXPECT_EQ(results[0]->passage, "hello");
  EXPECT_EQ(results[1]->passage, "world");
  for (const auto& result : results) {
    EXPECT_EQ(result->embeddings.size(), kEmbeddingsOutputSize);
  }

  histogram_tester_.ExpectUniqueSample(kCacheHitMetricName, false, 2);
}

TEST_F(PassageEmbeddingsServiceTest, CacheHits) {
  mojom::PassageEmbeddingsLoadModelsParamsPtr params =
      MakeParams(embeddings_path_, sp_path_, kInputWindowSize);
  mojo::Remote<mojom::PassageEmbedder> embedder_remote;

  base::test::TestFuture<bool> load_models_future;
  service()->LoadModels(std::move(params),
                        embedder_remote.BindNewPipeAndPassReceiver(),
                        load_models_future.GetCallback());
  bool load_models_success = load_models_future.Get();
  EXPECT_TRUE(load_models_success);

  base::test::TestFuture<std::vector<mojom::PassageEmbeddingsResultPtr>>
      execute_future;
  embedder_remote->GenerateEmbeddings(
      {"hello", "world", "hello", "world", "foo"},
      mojom::PassagePriority::kUserInitiated, execute_future.GetCallback());
  auto results = execute_future.Take();

  EXPECT_EQ(results.size(), 5u);
  EXPECT_EQ(results[0]->passage, "hello");
  EXPECT_EQ(results[1]->passage, "world");
  EXPECT_EQ(results[2]->passage, "hello");
  EXPECT_EQ(results[3]->passage, "world");

  EXPECT_EQ(results[0]->embeddings, results[2]->embeddings);
  EXPECT_EQ(results[1]->embeddings, results[3]->embeddings);

  for (const auto& result : results) {
    EXPECT_EQ(result->embeddings.size(), kEmbeddingsOutputSize);
  }

  histogram_tester_.ExpectTotalCount(kCacheHitMetricName, 5);
  histogram_tester_.ExpectBucketCount(kCacheHitMetricName, true, 2);
  histogram_tester_.ExpectBucketCount(kCacheHitMetricName, false, 3);
}

TEST_F(PassageEmbeddingsServiceTest, RecordsDurationHistogramsWithPriority) {
  mojom::PassageEmbeddingsLoadModelsParamsPtr params =
      MakeParams(embeddings_path_, sp_path_, kInputWindowSize);
  mojo::Remote<mojom::PassageEmbedder> embedder_remote;

  base::test::TestFuture<bool> load_models_future;
  service()->LoadModels(std::move(params),
                        embedder_remote.BindNewPipeAndPassReceiver(),
                        load_models_future.GetCallback());
  std::ignore = load_models_future.Take();

  base::test::TestFuture<std::vector<mojom::PassageEmbeddingsResultPtr>>
      execute_future;
  embedder_remote->GenerateEmbeddings({"hello", "world"},
                                      mojom::PassagePriority::kPassive,
                                      execute_future.GetCallback());
  std::ignore = execute_future.Take();

  embedder_remote->GenerateEmbeddings({"foo"},
                                      mojom::PassagePriority::kUserInitiated,
                                      execute_future.GetCallback());
  std::ignore = execute_future.Take();

  histogram_tester_.ExpectTotalCount(
      "History.Embeddings.Embedder.PassageEmbeddingsGenerationDuration", 2);
  histogram_tester_.ExpectTotalCount(
      "History.Embeddings.Embedder.PassageEmbeddingsGenerationThreadDuration",
      2);
  histogram_tester_.ExpectTotalCount(
      "History.Embeddings.Embedder.QueryEmbeddingsGenerationDuration", 1);
  histogram_tester_.ExpectTotalCount(
      "History.Embeddings.Embedder.QueryEmbeddingsGenerationThreadDuration", 1);
}

}  // namespace
}  // namespace passage_embeddings
