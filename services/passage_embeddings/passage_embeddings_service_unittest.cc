// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/passage_embeddings/passage_embeddings_service.h"

#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
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

 private:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::PassageEmbeddingsService> service_;
  PassageEmbeddingsService service_impl_;
};

TEST_F(PassageEmbeddingsServiceTest, LoadValidModels) {
  base::RunLoop run_loop;
  mojom::PassageEmbeddingsLoadModelsParamsPtr params =
      MakeParams(embeddings_path_, sp_path_, kInputWindowSize);
  mojo::Remote<mojom::PassageEmbedder> embedder_remote;
  service()->LoadModels(
      std::move(params), embedder_remote.BindNewPipeAndPassReceiver(),
      base::BindLambdaForTesting([&](bool load_model_success) {
        EXPECT_TRUE(load_model_success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(PassageEmbeddingsServiceTest, LoadModelsWithInvalidEmbeddingsModel) {
  base::RunLoop run_loop;
  mojom::PassageEmbeddingsLoadModelsParamsPtr params =
      MakeParams(sp_path_, sp_path_, kInputWindowSize);
  mojo::Remote<mojom::PassageEmbedder> embedder_remote;
  service()->LoadModels(
      std::move(params), embedder_remote.BindNewPipeAndPassReceiver(),
      base::BindLambdaForTesting([&](bool load_model_success) {
        EXPECT_FALSE(load_model_success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(PassageEmbeddingsServiceTest, LoadModelsWithInvalidSpModel) {
  base::RunLoop run_loop;
  mojom::PassageEmbeddingsLoadModelsParamsPtr params =
      MakeParams(embeddings_path_, embeddings_path_, kInputWindowSize);
  mojo::Remote<mojom::PassageEmbedder> embedder_remote;
  service()->LoadModels(
      std::move(params), embedder_remote.BindNewPipeAndPassReceiver(),
      base::BindLambdaForTesting([&](bool load_model_success) {
        EXPECT_FALSE(load_model_success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(PassageEmbeddingsServiceTest, LoadModelsWithInvalidInputWindowSize) {
  base::RunLoop run_loop;
  mojom::PassageEmbeddingsLoadModelsParamsPtr params =
      MakeParams(embeddings_path_, sp_path_, 0u);
  mojo::Remote<mojom::PassageEmbedder> embedder_remote;
  service()->LoadModels(
      std::move(params), embedder_remote.BindNewPipeAndPassReceiver(),
      base::BindLambdaForTesting([&](bool load_model_success) {
        EXPECT_FALSE(load_model_success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(PassageEmbeddingsServiceTest, RespondsWithEmbeddings) {
  base::RunLoop load_models_run_loop;
  mojom::PassageEmbeddingsLoadModelsParamsPtr params =
      MakeParams(embeddings_path_, sp_path_, kInputWindowSize);
  mojo::Remote<mojom::PassageEmbedder> embedder_remote;
  service()->LoadModels(
      std::move(params), embedder_remote.BindNewPipeAndPassReceiver(),
      base::BindLambdaForTesting([&](bool load_model_success) {
        EXPECT_TRUE(load_model_success);
        load_models_run_loop.Quit();
      }));
  load_models_run_loop.Run();

  base::RunLoop generate_embeddings_run_loop;
  embedder_remote->GenerateEmbeddings(
      {"hello", "world"},
      base::BindLambdaForTesting(
          [&](const std::vector<mojom::PassageEmbeddingsResultPtr> results) {
            EXPECT_EQ(results.size(), 2u);
            EXPECT_EQ(results[0]->passage, "hello");
            EXPECT_EQ(results[1]->passage, "world");

            for (const auto& result : results) {
              EXPECT_EQ(result->embeddings.size(), kEmbeddingsOutputSize);
            }

            generate_embeddings_run_loop.Quit();
          }));
  generate_embeddings_run_loop.Run();
}

}  // namespace
}  // namespace passage_embeddings
