// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mime_handler/mime_handler_body_cache.h"

#include <string>
#include <string_view>

#include "base/auto_reset.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "extensions/browser/mime_handler/mime_handler_test_helpers.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

class MimeHandlerBodyCacheTest : public testing::Test {
 protected:
  void CreateDataPipe(mojo::ScopedDataPipeProducerHandle* producer,
                      mojo::ScopedDataPipeConsumerHandle* consumer,
                      uint32_t capacity = 1024) {
    ASSERT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(capacity, *producer, *consumer));
  }

  void WriteData(mojo::ScopedDataPipeProducerHandle& producer,
                 const std::string& data) {
    ASSERT_TRUE(mojo::BlockingCopyFromString(data, producer));
  }

  std::string ReadAllData(mojo::ScopedDataPipeConsumerHandle consumer) {
    mime_handler::StringDrainerClient client;
    mojo::DataPipeDrainer drainer(&client, std::move(consumer));
    EXPECT_TRUE(base::test::RunUntil([&] { return client.complete(); }));
    return client.TakeAccumulated();
  }

  base::test::TaskEnvironment task_environment_;
};

TEST_F(MimeHandlerBodyCacheTest, CreatePipeFromCache) {
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CreateDataPipe(&producer, &consumer);

  const std::string kTestData = "Test PDF content data";
  WriteData(producer, kTestData);
  producer.reset();

  auto cache = MimeHandlerBodyCache::Create(std::move(consumer), nullptr);
  ASSERT_TRUE(cache);

  ASSERT_TRUE(base::test::RunUntil([&] { return cache->is_complete(); }));
  EXPECT_EQ(kTestData.size(), cache->cached_size());

  auto new_consumer = cache->CreatePipe();
  ASSERT_TRUE(new_consumer.is_valid());
  EXPECT_EQ(kTestData, ReadAllData(std::move(new_consumer)));
}

TEST_F(MimeHandlerBodyCacheTest, CreateMultiplePipes) {
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CreateDataPipe(&producer, &consumer);

  const std::string kTestData = "Multiple pipes test data";
  WriteData(producer, kTestData);
  producer.reset();

  auto cache = MimeHandlerBodyCache::Create(std::move(consumer), nullptr);
  ASSERT_TRUE(cache);

  ASSERT_TRUE(base::test::RunUntil([&] { return cache->is_complete(); }));

  auto consumer1 = cache->CreatePipe();
  ASSERT_TRUE(consumer1.is_valid());
  EXPECT_EQ(kTestData, ReadAllData(std::move(consumer1)));

  auto consumer2 = cache->CreatePipe();
  ASSERT_TRUE(consumer2.is_valid());
  EXPECT_EQ(kTestData, ReadAllData(std::move(consumer2)));
}

TEST_F(MimeHandlerBodyCacheTest, EmptyDataPipe) {
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CreateDataPipe(&producer, &consumer);
  producer.reset();

  auto cache = MimeHandlerBodyCache::Create(std::move(consumer), nullptr);
  ASSERT_TRUE(cache);

  ASSERT_TRUE(base::test::RunUntil([&] { return cache->is_complete(); }));

  auto new_consumer = cache->CreatePipe();
  ASSERT_TRUE(new_consumer.is_valid());
  EXPECT_EQ("", ReadAllData(std::move(new_consumer)));
}

TEST_F(MimeHandlerBodyCacheTest, LargeData) {
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CreateDataPipe(&producer, &consumer, 16384);

  std::string large_data(10000, 'X');
  for (size_t i = 0; i < large_data.size(); ++i) {
    large_data[i] = static_cast<char>('A' + (i % 26));
  }
  WriteData(producer, large_data);
  producer.reset();

  auto cache = MimeHandlerBodyCache::Create(std::move(consumer), nullptr);
  ASSERT_TRUE(cache);

  ASSERT_TRUE(base::test::RunUntil([&] { return cache->is_complete(); }));

  auto new_consumer = cache->CreatePipe();
  ASSERT_TRUE(new_consumer.is_valid());
  EXPECT_EQ(large_data, ReadAllData(std::move(new_consumer)));
}

TEST_F(MimeHandlerBodyCacheTest, CreatePipeAsyncWaitsForDrain) {
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CreateDataPipe(&producer, &consumer);

  auto cache = MimeHandlerBodyCache::Create(std::move(consumer), nullptr);
  ASSERT_TRUE(cache);

  base::test::TestFuture<mojo::ScopedDataPipeConsumerHandle> pipe;
  cache->CreatePipeAsync(pipe.GetCallback());

  // Wait for bytes to land rather than for the cache to be ready: the point
  // is that a request goes unanswered while the source is still open.
  WriteData(producer, "data");
  ASSERT_TRUE(base::test::RunUntil([&] { return cache->cached_size() == 4u; }));
  EXPECT_FALSE(pipe.IsReady());

  producer.reset();
  ASSERT_TRUE(pipe.Wait());
  mojo::ScopedDataPipeConsumerHandle consumer_pipe = pipe.Take();
  ASSERT_TRUE(consumer_pipe.is_valid());
  EXPECT_EQ("data", ReadAllData(std::move(consumer_pipe)));
}

TEST_F(MimeHandlerBodyCacheTest, CreatePipeAsyncDefersRequestOnCompletedCache) {
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CreateDataPipe(&producer, &consumer);

  const std::string kData = "already-cached-body";
  WriteData(producer, kData);
  producer.reset();

  auto cache = MimeHandlerBodyCache::Create(std::move(consumer), nullptr);
  ASSERT_TRUE(cache);
  ASSERT_TRUE(base::test::RunUntil([&] { return cache->is_complete(); }));

  // A request made after caching settled is answered on a later task, so a
  // caller is never re-entered while its own state is half-built.
  base::test::TestFuture<mojo::ScopedDataPipeConsumerHandle> pipe;
  cache->CreatePipeAsync(pipe.GetCallback());
  EXPECT_FALSE(pipe.IsReady());

  ASSERT_TRUE(pipe.Wait());
  mojo::ScopedDataPipeConsumerHandle consumer_pipe = pipe.Take();
  ASSERT_TRUE(consumer_pipe.is_valid());
  EXPECT_EQ(kData, ReadAllData(std::move(consumer_pipe)));
}

TEST_F(MimeHandlerBodyCacheTest, CreatePipeAsyncMultipleRequestsDuringCaching) {
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CreateDataPipe(&producer, &consumer);

  auto cache = MimeHandlerBodyCache::Create(std::move(consumer), nullptr);
  ASSERT_TRUE(cache);

  base::test::TestFuture<mojo::ScopedDataPipeConsumerHandle> first;
  base::test::TestFuture<mojo::ScopedDataPipeConsumerHandle> second;
  base::test::TestFuture<mojo::ScopedDataPipeConsumerHandle> third;
  cache->CreatePipeAsync(first.GetCallback());
  cache->CreatePipeAsync(second.GetCallback());
  cache->CreatePipeAsync(third.GetCallback());

  // Wait for bytes to land rather than for the cache to be ready: the point
  // is that no request is answered while the source is still open.
  constexpr std::string_view kData = "queued-request-body";
  WriteData(producer, std::string(kData));
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return cache->cached_size() == kData.size(); }));
  EXPECT_FALSE(first.IsReady());
  EXPECT_FALSE(second.IsReady());
  EXPECT_FALSE(third.IsReady());

  producer.reset();
  ASSERT_TRUE(first.Wait());
  ASSERT_TRUE(second.Wait());
  ASSERT_TRUE(third.Wait());

  mojo::ScopedDataPipeConsumerHandle first_pipe = first.Take();
  mojo::ScopedDataPipeConsumerHandle second_pipe = second.Take();
  mojo::ScopedDataPipeConsumerHandle third_pipe = third.Take();
  ASSERT_TRUE(first_pipe.is_valid());
  ASSERT_TRUE(second_pipe.is_valid());
  ASSERT_TRUE(third_pipe.is_valid());

  // Each request owns a separate pipe, so one consumer draining the body
  // cannot starve another.
  EXPECT_NE(first_pipe.get().value(), second_pipe.get().value());
  EXPECT_NE(second_pipe.get().value(), third_pipe.get().value());
  EXPECT_EQ(kData, ReadAllData(std::move(first_pipe)));
  EXPECT_EQ(kData, ReadAllData(std::move(second_pipe)));
  EXPECT_EQ(kData, ReadAllData(std::move(third_pipe)));
}

// Answering a request can drop the last reference to the cache, because the
// stream that owns the cache is torn down from that callback. This verifies
// the cache holds itself until every queued callback has run. It is only a
// valid test while the cache stays empty: a replayed body makes the pipe hold
// its own reference and the cache would survive for the wrong reason.
TEST_F(MimeHandlerBodyCacheTest,
       CreatePipeAsyncOutlivesRequestDroppingLastReference) {
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CreateDataPipe(&producer, &consumer);

  auto cache = MimeHandlerBodyCache::Create(std::move(consumer), nullptr);
  ASSERT_TRUE(cache);

  // The body must stay empty: a replayed body makes the pipe hold its own
  // reference to the cache, which would keep the cache alive here for the
  // wrong reason. With nothing to replay, dropping the last external reference
  // from the first request leaves the run itself as the only owner.
  cache->CreatePipeAsync(base::BindLambdaForTesting(
      [&](mojo::ScopedDataPipeConsumerHandle) { cache.reset(); }));
  base::test::TestFuture<mojo::ScopedDataPipeConsumerHandle> second;
  cache->CreatePipeAsync(second.GetCallback());

  producer.reset();
  ASSERT_TRUE(base::test::RunUntil([&] { return !cache; }));
  ASSERT_TRUE(second.IsReady());
  EXPECT_TRUE(second.Take().is_valid());
}

TEST_F(MimeHandlerBodyCacheTest, ForwardingBasic) {
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle source_consumer;
  CreateDataPipe(&producer, &source_consumer);

  const std::string kTestData = "Forwarding test data";

  mojo::ScopedDataPipeConsumerHandle forwarding_consumer;
  auto cache = MimeHandlerBodyCache::Create(std::move(source_consumer),
                                            &forwarding_consumer);
  ASSERT_TRUE(cache);
  ASSERT_TRUE(forwarding_consumer.is_valid());

  WriteData(producer, kTestData);
  producer.reset();

  ASSERT_TRUE(base::test::RunUntil([&] { return cache->is_complete(); }));

  EXPECT_EQ(kTestData, ReadAllData(std::move(forwarding_consumer)));

  auto fallback_consumer = cache->CreatePipe();
  ASSERT_TRUE(fallback_consumer.is_valid());
  EXPECT_EQ(kTestData, ReadAllData(std::move(fallback_consumer)));
}

TEST_F(MimeHandlerBodyCacheTest, ForwardingLargeData) {
  // 1 MiB payload exceeds the cache's internal forwarding pipe
  // capacity (kDefaultPipeCapacity, 512 KiB), forcing the watcher to
  // re-arm and exercising the back-pressure path in
  // `WritePendingToForwarding()`. The source pipe is sized to hold
  // the entire payload so `BlockingCopyFromString()` does not park
  // the test thread waiting for the same-sequence drainer to run.
  constexpr size_t kPayloadSize = 1024 * 1024;

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle source_consumer;
  CreateDataPipe(&producer, &source_consumer, kPayloadSize);

  std::string large_data(kPayloadSize, '\0');
  for (size_t i = 0; i < large_data.size(); ++i) {
    large_data[i] = static_cast<char>('A' + (i % 26));
  }

  mojo::ScopedDataPipeConsumerHandle forwarding_consumer;
  auto cache = MimeHandlerBodyCache::Create(std::move(source_consumer),
                                            &forwarding_consumer);
  ASSERT_TRUE(cache);
  ASSERT_TRUE(forwarding_consumer.is_valid());

  WriteData(producer, large_data);
  producer.reset();

  // Drain the forwarding consumer concurrently with the source drain
  // so the cache's forwarding watcher can re-arm as the consumer
  // makes room.
  mime_handler::StringDrainerClient forwarding_client;
  mojo::DataPipeDrainer forwarding_drainer(&forwarding_client,
                                           std::move(forwarding_consumer));
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return cache->is_complete() && forwarding_client.complete(); }));

  EXPECT_EQ(large_data, forwarding_client.TakeAccumulated());

  auto fallback_consumer = cache->CreatePipe();
  ASSERT_TRUE(fallback_consumer.is_valid());
  EXPECT_EQ(large_data, ReadAllData(std::move(fallback_consumer)));
}

TEST_F(MimeHandlerBodyCacheTest, ForwardingPipeClosedEarly) {
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle source_consumer;
  CreateDataPipe(&producer, &source_consumer);

  const std::string kTestData = "Data when consumer closes early";

  mojo::ScopedDataPipeConsumerHandle forwarding_consumer;
  auto cache = MimeHandlerBodyCache::Create(std::move(source_consumer),
                                            &forwarding_consumer);
  ASSERT_TRUE(cache);

  forwarding_consumer.reset();

  WriteData(producer, kTestData);
  producer.reset();

  ASSERT_TRUE(base::test::RunUntil([&] { return cache->is_complete(); }));

  EXPECT_EQ(kTestData.size(), cache->cached_size());

  auto fallback_consumer = cache->CreatePipe();
  ASSERT_TRUE(fallback_consumer.is_valid());
  EXPECT_EQ(kTestData, ReadAllData(std::move(fallback_consumer)));
}

TEST_F(MimeHandlerBodyCacheTest, CreateWithForwardingInvalidSource) {
  mojo::ScopedDataPipeConsumerHandle invalid_source;
  mojo::ScopedDataPipeConsumerHandle forwarding_consumer;

  auto cache = MimeHandlerBodyCache::Create(std::move(invalid_source),
                                            &forwarding_consumer);
  EXPECT_FALSE(cache);
}

TEST_F(MimeHandlerBodyCacheTest, CreateWithInvalidSource) {
  mojo::ScopedDataPipeConsumerHandle invalid_source;
  auto cache = MimeHandlerBodyCache::Create(std::move(invalid_source), nullptr);
  EXPECT_FALSE(cache);
}

TEST_F(MimeHandlerBodyCacheTest, OverCapResponseForwardsFullBody) {
  constexpr size_t kCap = 32;
  auto reset_cap = MimeHandlerBodyCache::SetMaxCacheBytesForTesting(kCap);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle source_consumer;
  CreateDataPipe(&producer, &source_consumer);

  std::string oversize_data(kCap * 4, '\0');
  for (size_t i = 0; i < oversize_data.size(); ++i) {
    oversize_data[i] = static_cast<char>('A' + (i % 26));
  }

  mojo::ScopedDataPipeConsumerHandle forwarding_consumer;
  auto cache = MimeHandlerBodyCache::Create(std::move(source_consumer),
                                            &forwarding_consumer);
  ASSERT_TRUE(cache);
  ASSERT_TRUE(forwarding_consumer.is_valid());

  WriteData(producer, oversize_data);
  producer.reset();

  mime_handler::StringDrainerClient forwarding_client;
  mojo::DataPipeDrainer forwarding_drainer(&forwarding_client,
                                           std::move(forwarding_consumer));
  ASSERT_TRUE(
      base::test::RunUntil([&] { return forwarding_client.complete(); }));

  // The cap only limits replay memory; the live consumer still receives
  // the entire body.
  EXPECT_EQ(oversize_data, forwarding_client.TakeAccumulated());

  // Replay is abandoned, so the fallback path refetches from the network.
  EXPECT_TRUE(cache->is_abandoned());
  EXPECT_FALSE(cache->is_complete());
  EXPECT_EQ(0u, cache->cached_size());
  EXPECT_FALSE(cache->CreatePipe().is_valid());
}

TEST_F(MimeHandlerBodyCacheTest, OverCapForwardsBodyLargerThanPipeCapacity) {
  // The payload exceeds the cache's internal forwarding pipe capacity
  // (kDefaultPipeCapacity, 512 KiB), so the cache cannot park the whole
  // body in the pipe: it must pause source reads and resume as the
  // consumer drains. The source pipe is sized to hold the entire
  // payload so `BlockingCopyFromString()` does not park the test thread
  // waiting for the same-sequence pump to run.
  constexpr size_t kCap = 16 * 1024;
  constexpr size_t kPayloadSize = 2 * 1024 * 1024;
  auto reset_cap = MimeHandlerBodyCache::SetMaxCacheBytesForTesting(kCap);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle source_consumer;
  CreateDataPipe(&producer, &source_consumer, kPayloadSize);

  std::string oversize_data(kPayloadSize, '\0');
  for (size_t i = 0; i < oversize_data.size(); ++i) {
    oversize_data[i] = static_cast<char>('A' + (i % 26));
  }

  mojo::ScopedDataPipeConsumerHandle forwarding_consumer;
  auto cache = MimeHandlerBodyCache::Create(std::move(source_consumer),
                                            &forwarding_consumer);
  ASSERT_TRUE(cache);
  ASSERT_TRUE(forwarding_consumer.is_valid());

  WriteData(producer, oversize_data);
  producer.reset();

  mime_handler::StringDrainerClient forwarding_client;
  mojo::DataPipeDrainer forwarding_drainer(&forwarding_client,
                                           std::move(forwarding_consumer));
  ASSERT_TRUE(
      base::test::RunUntil([&] { return forwarding_client.complete(); }));

  EXPECT_EQ(oversize_data, forwarding_client.TakeAccumulated());
  EXPECT_TRUE(cache->is_abandoned());
  EXPECT_EQ(0u, cache->cached_size());
}

TEST_F(MimeHandlerBodyCacheTest, OverCapStalledConsumerStillReceivesFullBody) {
  // The consumer does not read at all until the source has finished;
  // bytes beyond the forwarding pipe's capacity (512 KiB) must survive
  // the stall through back-pressure rather than being dropped.
  constexpr size_t kCap = 32;
  constexpr size_t kPayloadSize = 1024 * 1024;
  auto reset_cap = MimeHandlerBodyCache::SetMaxCacheBytesForTesting(kCap);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle source_consumer;
  CreateDataPipe(&producer, &source_consumer, kPayloadSize);

  std::string oversize_data(kPayloadSize, '\0');
  for (size_t i = 0; i < oversize_data.size(); ++i) {
    oversize_data[i] = static_cast<char>('A' + (i % 26));
  }

  mojo::ScopedDataPipeConsumerHandle forwarding_consumer;
  auto cache = MimeHandlerBodyCache::Create(std::move(source_consumer),
                                            &forwarding_consumer);
  ASSERT_TRUE(cache);
  ASSERT_TRUE(forwarding_consumer.is_valid());

  WriteData(producer, oversize_data);
  producer.reset();

  ASSERT_TRUE(base::test::RunUntil([&] { return cache->is_abandoned(); }));

  // Only now start reading the forwarded body.
  mime_handler::StringDrainerClient forwarding_client;
  mojo::DataPipeDrainer forwarding_drainer(&forwarding_client,
                                           std::move(forwarding_consumer));
  ASSERT_TRUE(
      base::test::RunUntil([&] { return forwarding_client.complete(); }));

  EXPECT_EQ(oversize_data, forwarding_client.TakeAccumulated());
  EXPECT_EQ(0u, cache->cached_size());
}

TEST_F(MimeHandlerBodyCacheTest,
       ForwardingConsumerDisconnectAfterOverflowClosesSource) {
  constexpr size_t kCap = 32;
  auto reset_cap = MimeHandlerBodyCache::SetMaxCacheBytesForTesting(kCap);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle source_consumer;
  CreateDataPipe(&producer, &source_consumer);

  mojo::ScopedDataPipeConsumerHandle forwarding_consumer;
  auto cache = MimeHandlerBodyCache::Create(std::move(source_consumer),
                                            &forwarding_consumer);
  ASSERT_TRUE(cache);

  // Cross the cap but keep the source producer open: more bytes could
  // still arrive.
  WriteData(producer, std::string(kCap * 2, 'X'));
  ASSERT_TRUE(base::test::RunUntil([&] { return cache->is_abandoned(); }));

  // Once the live consumer goes away, nothing needs the remaining
  // bytes: the cache must stop reading and close the source pipe.
  forwarding_consumer.reset();
  EXPECT_TRUE(base::test::RunUntil(
      [&] { return producer->QuerySignalsState().peer_closed(); }));
}

TEST_F(MimeHandlerBodyCacheTest,
       ConsumerDisconnectThenOverCapChunkStopsAndClosesSource) {
  constexpr size_t kCap = 32;
  auto reset_cap = MimeHandlerBodyCache::SetMaxCacheBytesForTesting(kCap);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle source_consumer;
  CreateDataPipe(&producer, &source_consumer);

  mojo::ScopedDataPipeConsumerHandle forwarding_consumer;
  auto cache = MimeHandlerBodyCache::Create(std::move(source_consumer),
                                            &forwarding_consumer);
  ASSERT_TRUE(cache);

  // Close the consumer and cross the cap before the cache has had a
  // chance to observe either event. Whichever notification arrives
  // first, the outcome must converge: the body is over cap so replay
  // is forbidden, and with no live consumer the source must be closed.
  forwarding_consumer.reset();
  WriteData(producer, std::string(kCap * 2, 'X'));

  ASSERT_TRUE(base::test::RunUntil([&] { return cache->is_abandoned(); }));
  EXPECT_FALSE(cache->is_complete());
  EXPECT_EQ(0u, cache->cached_size());
  EXPECT_FALSE(cache->CreatePipe().is_valid());
  EXPECT_TRUE(base::test::RunUntil(
      [&] { return producer->QuerySignalsState().peer_closed(); }));
}

TEST_F(MimeHandlerBodyCacheTest, OverCapCacheOnlyClosesSourceAndAbandons) {
  constexpr size_t kCap = 32;
  auto reset_cap = MimeHandlerBodyCache::SetMaxCacheBytesForTesting(kCap);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle source_consumer;
  CreateDataPipe(&producer, &source_consumer);

  auto cache =
      MimeHandlerBodyCache::Create(std::move(source_consumer), nullptr);
  ASSERT_TRUE(cache);

  base::test::TestFuture<mojo::ScopedDataPipeConsumerHandle> pipe;
  cache->CreatePipeAsync(pipe.GetCallback());
  EXPECT_FALSE(pipe.IsReady());

  // Without a live consumer there is no one to forward to, so an
  // over-cap response leaves nothing to do: stop reading and close the
  // source pipe.
  WriteData(producer, std::string(kCap * 2, 'X'));
  ASSERT_TRUE(base::test::RunUntil([&] { return cache->is_abandoned(); }));

  EXPECT_FALSE(cache->is_complete());
  EXPECT_EQ(0u, cache->cached_size());
  ASSERT_TRUE(pipe.Wait());
  EXPECT_FALSE(pipe.Get().is_valid());
  EXPECT_TRUE(base::test::RunUntil(
      [&] { return producer->QuerySignalsState().peer_closed(); }));
}

}  // namespace
}  // namespace extensions
