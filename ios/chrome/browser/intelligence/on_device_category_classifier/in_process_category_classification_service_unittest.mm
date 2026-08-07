// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/on_device_category_classifier/in_process_category_classification_service.h"

#import <memory>
#import <string>
#import <vector>

#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/string_number_conversions.h"
#import "base/test/mock_callback.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#import "components/page_content_annotations/core/page_content_annotations_common.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

using ::testing::_;

class TestCategoryClassificationService
    : public InProcessCategoryClassificationService {
 public:
  using InProcessCategoryClassificationService::
      InProcessCategoryClassificationService;
  using InProcessCategoryClassificationService::OnGotEmbeddings;
};

passage_embeddings::mojom::PassageEmbeddingsResultPtr CreateNormalizedResult(
    size_t dim = 768,
    size_t index = 0) {
  auto res = passage_embeddings::mojom::PassageEmbeddingsResult::New();
  res->embeddings = std::vector<float>(dim, 0.0f);
  res->embeddings[index] = 1.0f;
  return res;
}

class InProcessCategoryClassificationServiceTest : public PlatformTest {
 public:
  InProcessCategoryClassificationServiceTest() = default;
  ~InProcessCategoryClassificationServiceTest() override = default;

  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    model_path_ = temp_dir_.GetPath().AppendASCII("model.tflite");
    ASSERT_TRUE(base::WriteFile(model_path_, "dummy"));

    model_provider_ = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();
    service_ = std::make_unique<TestCategoryClassificationService>(
        model_provider_.get());
    service_->ResetPassageEmbedderForTesting();
  }

  void TearDown() override {
    service_.reset();
    model_provider_.reset();
    PlatformTest::TearDown();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath model_path_;
  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      model_provider_;
  std::unique_ptr<TestCategoryClassificationService> service_;
};

// Tests that empty page content triggers the callback immediately with no
// results.
TEST_F(InProcessCategoryClassificationServiceTest, ClassifyEmptyContent) {
  base::test::TestFuture<const std::vector<page_content_annotations::Category>&>
      future;
  service_->ClassifyPageContext(GURL("https://example.com"), "Title", "",
                                ukm::SourceId(), future.GetCallback());

  EXPECT_TRUE(future.Get().empty());
}

// Tests that when the passage embedder is null, ClassifyPageContext runs
// pending callbacks with empty results instead of hanging.
TEST_F(InProcessCategoryClassificationServiceTest, ClassifyWithNullEmbedder) {
  service_->OnPassageEmbedderLoadedForTesting(1, 768, /*success=*/true);

  base::test::TestFuture<const std::vector<page_content_annotations::Category>&>
      future;
  service_->ClassifyPageContext(GURL("https://example.com"), "Title",
                                "Some paragraph content.", ukm::SourceId(),
                                future.GetCallback());

  ASSERT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

// Tests that concurrent classification requests for the same URL are merged
// when queued before model load, and executing pending classifications runs all
// callbacks for that URL. Note: Verifying true async in-flight merging after
// model load requires a mocked asynchronous Mojo embedder.
TEST_F(InProcessCategoryClassificationServiceTest,
       ConcurrentClassificationsSameURL) {
  base::test::TestFuture<const std::vector<page_content_annotations::Category>&>
      future1;
  base::test::TestFuture<const std::vector<page_content_annotations::Category>&>
      future2;

  GURL url("https://example.com");

  // Call ClassifyPageContext before model update (no metadata) so requests are
  // queued in pending_classifications_.
  service_->ClassifyPageContext(url, "Title", "Some paragraph content.",
                                ukm::SourceId(), future1.GetCallback());
  service_->ClassifyPageContext(url, "Title", "Some paragraph content.",
                                ukm::SourceId(), future2.GetCallback());

  // Verify that neither callback has run yet.
  EXPECT_FALSE(future1.IsReady());
  EXPECT_FALSE(future2.IsReady());

  // Trigger model loaded so pending classifications are processed.
  service_->OnPassageEmbedderLoadedForTesting(1, 768, /*success=*/true);

  // Since embedder_wrapper_ was reset for testing, processing the queue
  // invokes all merged callbacks synchronously for the URL with the same
  // shared result rather than hanging.
  ASSERT_TRUE(future1.IsReady());
  ASSERT_TRUE(future2.IsReady());

  EXPECT_TRUE(future1.Get().empty());
  EXPECT_TRUE(future2.Get().empty());
}

// Tests that classifications are queued when metadata is not yet updated,
// and then processed once the model update occurs.
TEST_F(InProcessCategoryClassificationServiceTest, QueueingBeforeModelUpdate) {
  base::test::TestFuture<const std::vector<page_content_annotations::Category>&>
      future;

  GURL url("https://example.com");

  // Call ClassifyPageContext before model update (no metadata).
  service_->ClassifyPageContext(url, "Title", "Paragraph content.",
                                ukm::SourceId(), future.GetCallback());

  // Verify that the request is queued and not run.
  EXPECT_FALSE(future.IsReady());

  // Now trigger the model loaded callback.
  service_->OnPassageEmbedderLoadedForTesting(1, 768, /*success=*/true);

  // Since embedder_wrapper_ was reset to null, processing the queue
  // invokes the callback with empty results.
  ASSERT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

// Tests that calling ClassifyPageContext without both a valid URL and title
// returns empty categories immediately even when page content is non-empty,
// matching desktop behavior.
TEST_F(InProcessCategoryClassificationServiceTest,
       ClassifyMissingTitleOrInvalidUrl) {
  service_->OnPassageEmbedderLoadedForTesting(1, 768, /*success=*/true);

  {
    base::test::TestFuture<
        const std::vector<page_content_annotations::Category>&>
        future;
    service_->ClassifyPageContext(GURL(), "", "Some valid paragraph content.",
                                  ukm::SourceId(), future.GetCallback());
    EXPECT_TRUE(future.IsReady());
    EXPECT_TRUE(future.Get().empty());
  }

  {
    base::test::TestFuture<
        const std::vector<page_content_annotations::Category>&>
        future;
    service_->ClassifyPageContext(GURL("https://example.com"), "",
                                  "Some valid paragraph content.",
                                  ukm::SourceId(), future.GetCallback());
    EXPECT_TRUE(future.IsReady());
    EXPECT_TRUE(future.Get().empty());
  }

  {
    base::test::TestFuture<
        const std::vector<page_content_annotations::Category>&>
        future;
    service_->ClassifyPageContext(GURL(), "Title",
                                  "Some valid paragraph content.",
                                  ukm::SourceId(), future.GetCallback());
    EXPECT_TRUE(future.IsReady());
    EXPECT_TRUE(future.Get().empty());
  }
}

// Tests that if classification requests are queued before model load, and the
// model load fails (success=false), all queued callbacks are cancelled and
// invoked with empty results.
TEST_F(InProcessCategoryClassificationServiceTest,
       CancelQueuedRequestsOnModelLoadFailure) {
  base::test::TestFuture<const std::vector<page_content_annotations::Category>&>
      future1;
  base::test::TestFuture<const std::vector<page_content_annotations::Category>&>
      future2;

  GURL url1("https://example1.com");
  GURL url2("https://example2.com");

  service_->ClassifyPageContext(url1, "Title 1", "Paragraph content 1.",
                                ukm::SourceId(), future1.GetCallback());
  service_->ClassifyPageContext(url2, "Title 2", "Paragraph content 2.",
                                ukm::SourceId(), future2.GetCallback());

  EXPECT_FALSE(future1.IsReady());
  EXPECT_FALSE(future2.IsReady());

  // Simulate model load failure.
  service_->OnPassageEmbedderLoadedForTesting(1, 768, /*success=*/false);

  ASSERT_TRUE(future1.IsReady());
  ASSERT_TRUE(future2.IsReady());
  EXPECT_TRUE(future1.Get().empty());
  EXPECT_TRUE(future2.Get().empty());
}

// Tests in-memory caching: OnGotEmbeddings caches embeddings in memory
// and ClassifyPageContext uses them directly without re-embedding.
TEST_F(InProcessCategoryClassificationServiceTest,
       InMemoryCacheStoresAndReusesEmbeddings) {
  service_->OnPassageEmbedderLoadedForTesting(1, 768, /*success=*/true);

  GURL url("https://example.com");
  std::vector<page_content_annotations::EmbeddingPassageType> passage_types = {
      page_content_annotations::EmbeddingPassageType::kPageContent,
      page_content_annotations::EmbeddingPassageType::kTitleAndUrl};

  std::vector<passage_embeddings::mojom::PassageEmbeddingsResultPtr> results;
  results.push_back(CreateNormalizedResult(768, 0));
  results.push_back(CreateNormalizedResult(768, 1));

  service_->OnGotEmbeddings(url, ukm::SourceId(), passage_types,
                            std::move(results));

  std::optional<InProcessCategoryClassificationService::CachedEmbeddings>
      cached = service_->GetCachedEmbeddings(url);
  ASSERT_TRUE(cached.has_value());
  ASSERT_TRUE(cached->title_url_embedding.has_value());
  EXPECT_EQ(cached->passage_embeddings.size(), 1u);

  // Calling ClassifyPageContext for the same URL hits the in-memory cache
  // and invokes OnPageEmbeddingAvailable without re-queueing.
  base::test::TestFuture<const std::vector<page_content_annotations::Category>&>
      future;
  service_->ClassifyPageContext(url, "Title", "Some paragraph content.",
                                ukm::SourceId(), future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get().empty());
}

// Tests HasCachedEmbeddings and ClassifyWithCachedEmbeddings.
TEST_F(InProcessCategoryClassificationServiceTest,
       ClassifyWithCachedEmbeddingsHitAndMiss) {
  GURL uncached_url("https://uncached.com");
  EXPECT_FALSE(service_->HasCachedEmbeddings(uncached_url));

  base::test::TestFuture<const std::vector<page_content_annotations::Category>&>
      miss_future;
  service_->ClassifyWithCachedEmbeddings(uncached_url, ukm::SourceId(),
                                         miss_future.GetCallback());
  EXPECT_TRUE(miss_future.Wait());
  EXPECT_TRUE(miss_future.Get().empty());

  GURL cached_url("https://cached.com");
  std::vector<page_content_annotations::EmbeddingPassageType> passage_types = {
      page_content_annotations::EmbeddingPassageType::kPageContent,
      page_content_annotations::EmbeddingPassageType::kTitleAndUrl};

  std::vector<passage_embeddings::mojom::PassageEmbeddingsResultPtr> results;
  results.push_back(CreateNormalizedResult(768, 0));
  results.push_back(CreateNormalizedResult(768, 1));

  service_->OnGotEmbeddings(cached_url, ukm::SourceId(), passage_types,
                            std::move(results));

  EXPECT_TRUE(service_->HasCachedEmbeddings(cached_url));

  base::test::TestFuture<const std::vector<page_content_annotations::Category>&>
      hit_future;
  service_->ClassifyWithCachedEmbeddings(cached_url, ukm::SourceId(),
                                         hit_future.GetCallback());
  EXPECT_TRUE(hit_future.Wait());
  EXPECT_TRUE(hit_future.Get().empty());
}

// Tests that cached embeddings are cleared when the embedder disconnects.
TEST_F(InProcessCategoryClassificationServiceTest,
       CacheClearedOnEmbedderDisconnect) {
  GURL url("https://example.com");
  std::vector<page_content_annotations::EmbeddingPassageType> passage_types = {
      page_content_annotations::EmbeddingPassageType::kPageContent,
      page_content_annotations::EmbeddingPassageType::kTitleAndUrl};

  std::vector<passage_embeddings::mojom::PassageEmbeddingsResultPtr> results;
  results.push_back(CreateNormalizedResult(768, 0));
  results.push_back(CreateNormalizedResult(768, 1));

  service_->OnGotEmbeddings(url, ukm::SourceId(), passage_types,
                            std::move(results));
  EXPECT_TRUE(service_->HasCachedEmbeddings(url));

  service_->OnEmbedderDisconnectForTesting();
  EXPECT_FALSE(service_->HasCachedEmbeddings(url));
}

// Tests that cached embeddings are cleared when a new model version is loaded.
TEST_F(InProcessCategoryClassificationServiceTest, CacheClearedOnModelReload) {
  GURL url("https://example.com");
  std::vector<page_content_annotations::EmbeddingPassageType> passage_types = {
      page_content_annotations::EmbeddingPassageType::kPageContent,
      page_content_annotations::EmbeddingPassageType::kTitleAndUrl};

  std::vector<passage_embeddings::mojom::PassageEmbeddingsResultPtr> results;
  results.push_back(CreateNormalizedResult(768, 0));
  results.push_back(CreateNormalizedResult(768, 1));

  service_->OnGotEmbeddings(url, ukm::SourceId(), passage_types,
                            std::move(results));
  EXPECT_TRUE(service_->HasCachedEmbeddings(url));

  // Loading a new model clears stale cached embeddings from the old version.
  service_->OnPassageEmbedderLoadedForTesting(2, 768, /*success=*/true);
  EXPECT_FALSE(service_->HasCachedEmbeddings(url));
}

}  // namespace
