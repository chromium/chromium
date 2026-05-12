// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mime_handler/stream_container.h"

#include <string>

#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "extensions/browser/mime_handler/mime_handler_body_cache.h"
#include "extensions/browser/mime_handler/mime_handler_test_helpers.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/transferrable_url_loader.mojom.h"

namespace extensions {
namespace {

class StreamContainerBodyCacheTest : public testing::Test {
 protected:
  void SetUp() override {
    auto transferrable_loader = blink::mojom::TransferrableURLLoader::New();
    transferrable_loader->url = GURL("chrome-extension://abc/stream");
    transferrable_loader->head = network::mojom::URLResponseHead::New();
    transferrable_loader->head->mime_type = "application/pdf";

    stream_container_ = std::make_unique<StreamContainer>(
        /*tab_id=*/1, /*embedded=*/false,
        GURL("chrome-extension://abc/handler.html"), "abc",
        std::move(transferrable_loader), GURL("https://example.com/file.pdf"));
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<StreamContainer> stream_container_;
};

TEST_F(StreamContainerBodyCacheTest, InitiallyNoBodyCache) {
  EXPECT_FALSE(stream_container_->GetFallbackDataPipe().is_valid());
}

TEST_F(StreamContainerBodyCacheTest, GetFallbackDataPipeReplaysCachedBytes) {
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(64u, producer, consumer));

  const std::string kData = "fallback data";
  ASSERT_EQ(MOJO_RESULT_OK, producer->WriteAllData(base::as_byte_span(kData)));
  producer.reset();

  auto cache = MimeHandlerBodyCache::Create(std::move(consumer), nullptr);
  ASSERT_TRUE(cache);
  ASSERT_TRUE(base::test::RunUntil([&] { return cache->is_complete(); }));

  stream_container_->SetBodyCache(cache);
  auto fallback_pipe = stream_container_->GetFallbackDataPipe();
  ASSERT_TRUE(fallback_pipe.is_valid());

  mime_handler::StringDrainerClient client;
  mojo::DataPipeDrainer drainer(&client, std::move(fallback_pipe));
  ASSERT_TRUE(base::test::RunUntil([&] { return client.complete(); }));
  EXPECT_EQ(kData, client.TakeAccumulated());
}

}  // namespace
}  // namespace extensions
