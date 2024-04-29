// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/passage_embeddings/passage_embeddings_service.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/passage_embeddings/passage_embedder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace passage_embeddings {
namespace {

class PassageEmbeddingsServiceTest : public testing::Test {
 public:
  PassageEmbeddingsServiceTest()
      : service_impl_(service_.BindNewPipeAndPassReceiver()),
        embedder_impl_(embedder_.BindNewPipeAndPassReceiver()) {}

  mojo::Remote<mojom::PassageEmbeddingsService>& service() { return service_; }
  mojo::Remote<mojom::PassageEmbedder>& embedder() { return embedder_; }

 private:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::PassageEmbeddingsService> service_;
  PassageEmbeddingsService service_impl_;
  mojo::Remote<mojom::PassageEmbedder> embedder_;
  PassageEmbedder embedder_impl_;
};

TEST_F(PassageEmbeddingsServiceTest, TestStub) {
  base::RunLoop run_loop;

  embedder()->GenerateEmbeddings(
      {"hello", "world"},
      base::BindLambdaForTesting(
          [&](const std::vector<mojom::PassageEmbeddingsResultPtr> results) {
            EXPECT_EQ(results.size(), 0u);
            run_loop.Quit();
          }));
  run_loop.Run();
}

}  // namespace
}  // namespace passage_embeddings
