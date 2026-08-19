// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/passage_embeddings/passage_embedder_impl.h"

#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/timer/elapsed_timer.h"
#include "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace passage_embeddings {
namespace {

constexpr uint32_t kInputWindowSize = 256;
constexpr size_t kEmbeddingsOutputSize = 768;

class PassageEmbedderImplTest : public testing::Test {
 public:
  PassageEmbedderImplTest() = default;

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

  mojom::PassageEmbedderParamsPtr MakeEmbedderParams(
      bool execute_for_gemma = false) {
    auto params = mojom::PassageEmbedderParams::New();
    params->execute_for_gemma = execute_for_gemma;
    params->user_initiated_priority_num_threads = 4;
    params->passive_priority_num_threads = 1;
    params->embedder_cache_size = 1000;
    return params;
  }

 protected:
  base::FilePath embeddings_path_;
  base::FilePath sp_path_;
  base::HistogramTester histogram_tester_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(PassageEmbedderImplTest, LoadValidModels) {
  PassageEmbedderImpl embedder(MakeEmbedderParams());
  EXPECT_TRUE(embedder.LoadModels(
      base::File(embeddings_path_,
                 base::File::FLAG_OPEN | base::File::FLAG_READ),
      base::File(sp_path_, base::File::FLAG_OPEN | base::File::FLAG_READ),
      kInputWindowSize));
}

TEST_F(PassageEmbedderImplTest, LoadModelsWithInvalidEmbeddingsModelFile) {
  PassageEmbedderImpl embedder(MakeEmbedderParams());
  // LoadModels fails immediately when given an invalid embeddings model file
  // handle.
  EXPECT_FALSE(embedder.LoadModels(
      base::File(),
      base::File(sp_path_, base::File::FLAG_OPEN | base::File::FLAG_READ),
      kInputWindowSize));
}

TEST_F(PassageEmbedderImplTest, LoadModelsWithCorruptedEmbeddingsModel) {
  PassageEmbedderImpl embedder(MakeEmbedderParams());
  // LoadModels succeeds when file handles are valid, deferring flatbuffer
  // verification until the first execution.
  EXPECT_TRUE(embedder.LoadModels(
      base::File(sp_path_, base::File::FLAG_OPEN | base::File::FLAG_READ),
      base::File(sp_path_, base::File::FLAG_OPEN | base::File::FLAG_READ),
      kInputWindowSize));

  // Execution fails since the embeddings model flatbuffer is invalid.
  std::vector<mojom::PassageEmbeddingsResultPtr> results =
      embedder.GenerateEmbeddings({"foo"},
                                  mojom::PassagePriority::kUserInitiated);
  EXPECT_EQ(results.size(), 0u);
}

TEST_F(PassageEmbedderImplTest, LoadModelsWithInvalidSpModelFile) {
  PassageEmbedderImpl embedder(MakeEmbedderParams());
  // LoadModels fails immediately when given an invalid SentencePiece file
  // handle.
  EXPECT_FALSE(embedder.LoadModels(
      base::File(embeddings_path_,
                 base::File::FLAG_OPEN | base::File::FLAG_READ),
      base::File(), kInputWindowSize));
}

TEST_F(PassageEmbedderImplTest, LoadModelsWithCorruptedSpModel) {
  PassageEmbedderImpl embedder(MakeEmbedderParams());
  EXPECT_FALSE(embedder.LoadModels(
      base::File(embeddings_path_,
                 base::File::FLAG_OPEN | base::File::FLAG_READ),
      base::File(embeddings_path_,
                 base::File::FLAG_OPEN | base::File::FLAG_READ),
      kInputWindowSize));
}

TEST_F(PassageEmbedderImplTest, RespondsWithEmbeddings) {
  PassageEmbedderImpl embedder(MakeEmbedderParams());
  ASSERT_TRUE(embedder.LoadModels(
      base::File(embeddings_path_,
                 base::File::FLAG_OPEN | base::File::FLAG_READ),
      base::File(sp_path_, base::File::FLAG_OPEN | base::File::FLAG_READ),
      kInputWindowSize));

  auto results = embedder.GenerateEmbeddings(
      {"hello", "world", ""}, mojom::PassagePriority::kUserInitiated);
  EXPECT_EQ(results.size(), 3u);
  for (const auto& result : results) {
    EXPECT_EQ(result->embeddings.size(), kEmbeddingsOutputSize);
  }

  histogram_tester_.ExpectUniqueSample(kCacheHitMetricName, false, 3);
}

TEST_F(PassageEmbedderImplTest, CacheHits) {
  PassageEmbedderImpl embedder(MakeEmbedderParams());
  ASSERT_TRUE(embedder.LoadModels(
      base::File(embeddings_path_,
                 base::File::FLAG_OPEN | base::File::FLAG_READ),
      base::File(sp_path_, base::File::FLAG_OPEN | base::File::FLAG_READ),
      kInputWindowSize));

  auto results = embedder.GenerateEmbeddings(
      {"hello", "world", "hello", "world", "foo", ""},
      mojom::PassagePriority::kUserInitiated);

  EXPECT_EQ(results.size(), 6u);

  EXPECT_EQ(results[0]->embeddings, results[2]->embeddings);
  EXPECT_EQ(results[1]->embeddings, results[3]->embeddings);

  for (const auto& result : results) {
    EXPECT_EQ(result->embeddings.size(), kEmbeddingsOutputSize);
  }

  histogram_tester_.ExpectTotalCount(kCacheHitMetricName, 6);
  histogram_tester_.ExpectBucketCount(kCacheHitMetricName, true, 2);
  histogram_tester_.ExpectBucketCount(kCacheHitMetricName, false, 4);
}

TEST_F(PassageEmbedderImplTest, RecordsDurationHistogramsWithPriority) {
  PassageEmbedderImpl embedder(MakeEmbedderParams());
  ASSERT_TRUE(embedder.LoadModels(
      base::File(embeddings_path_,
                 base::File::FLAG_OPEN | base::File::FLAG_READ),
      base::File(sp_path_, base::File::FLAG_OPEN | base::File::FLAG_READ),
      kInputWindowSize));

  std::ignore = embedder.GenerateEmbeddings({"hello", "world"},
                                            mojom::PassagePriority::kPassive);

  std::ignore = embedder.GenerateEmbeddings(
      {"foo"}, mojom::PassagePriority::kUserInitiated);

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

TEST_F(PassageEmbedderImplTest, RecordsGemmaHistograms) {
  PassageEmbedderImpl embedder(MakeEmbedderParams(/*execute_for_gemma=*/true));
  ASSERT_TRUE(embedder.LoadModels(
      base::File(embeddings_path_,
                 base::File::FLAG_OPEN | base::File::FLAG_READ),
      base::File(sp_path_, base::File::FLAG_OPEN | base::File::FLAG_READ),
      kInputWindowSize));

  std::ignore = embedder.GenerateEmbeddings({"hello", "world"},
                                            mojom::PassagePriority::kPassive);

  std::ignore = embedder.GenerateEmbeddings(
      {"foo"}, mojom::PassagePriority::kUserInitiated);

  histogram_tester_.ExpectTotalCount(
      "AI.SemanticEmbedder.SentencePieceModelLoadDuration", 1);
  histogram_tester_.ExpectTotalCount(
      "AI.SemanticEmbedder.SentencePieceModelLoadSucceeded", 1);
  histogram_tester_.ExpectTotalCount(
      "AI.SemanticEmbedder.EmbeddingsModelLoadDuration", 2);
  histogram_tester_.ExpectTotalCount(
      "AI.SemanticEmbedder.EmbeddingsModelLoadSucceeded", 2);
  histogram_tester_.ExpectTotalCount("AI.SemanticEmbedder.TokenizationDuration",
                                     3);
  histogram_tester_.ExpectTotalCount(
      "AI.SemanticEmbedder.TokenizationSucceeded", 3);
  histogram_tester_.ExpectTotalCount("AI.SemanticEmbedder.PassageTokenCount",
                                     3);
  histogram_tester_.ExpectTotalCount(
      "AI.SemanticEmbedder.EmbeddingsGenerationSucceeded", 3);
  histogram_tester_.ExpectTotalCount(
      "AI.SemanticEmbedder.PassageEmbeddingsGenerationDuration.256", 3);
  base::ElapsedThreadTimer execute_thread_timer;
  if (execute_thread_timer.is_supported()) {
    histogram_tester_.ExpectTotalCount(
        "AI.SemanticEmbedder.PassageEmbeddingsGenerationThreadDuration.256", 3);
  }

  // Gemma metrics should be emitted instead of History embeddings metrics.
  histogram_tester_.ExpectTotalCount(
      "History.Embeddings.Embedder.PassageEmbeddingsGenerationDuration", 0);
  histogram_tester_.ExpectTotalCount(
      "History.Embeddings.Embedder.PassageEmbeddingsGenerationThreadDuration",
      0);
  histogram_tester_.ExpectTotalCount(
      "History.Embeddings.Embedder.QueryEmbeddingsGenerationDuration", 0);
  histogram_tester_.ExpectTotalCount(
      "History.Embeddings.Embedder.QueryEmbeddingsGenerationThreadDuration", 0);
  histogram_tester_.ExpectTotalCount(
      "History.Embeddings.Embedder.SentencePieceModelLoadDuration", 0);
  histogram_tester_.ExpectTotalCount(
      "History.Embeddings.Embedder.EmbeddingsModelLoadDuration", 0);
}

}  // namespace
}  // namespace passage_embeddings
