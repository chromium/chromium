// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/source_keyed_cached_metadata_handler.h"

#include "base/numerics/safe_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_crypto.h"
#include "third_party/blink/renderer/platform/crypto.h"
#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"

namespace blink {

namespace {

// Structure holding cache metadata sent to the platform.
struct CacheMetadataEntry {
  CacheMetadataEntry(const GURL& url,
                     base::Time response_time,
                     const uint8_t* data,
                     wtf_size_t data_size)
      : url(url), response_time(response_time) {
    this->data.Append(data, data_size);
  }

  GURL url;
  base::Time response_time;
  Vector<uint8_t> data;
};

// Mock GeneratedCodeCache implementation that provides basic caching.
class MockGeneratedCodeCache {
 public:
  bool HasCacheMetadataFor(const WebURL& url) {
    GURL gurl = WebStringToGURL(url.GetString());
    for (const CacheMetadataEntry& entry : cache_entries_) {
      if (entry.url == gurl) {
        return true;
      }
    }
    return false;
  }

  Vector<CacheMetadataEntry> GetCacheMetadatasFor(const WebURL& url) {
    Vector<CacheMetadataEntry> url_entries;
    GURL gurl = WebStringToGURL(url.GetString());
    for (const CacheMetadataEntry& entry : cache_entries_) {
      if (entry.url == gurl) {
        url_entries.push_back(entry);
      }
    }
    return url_entries;
  }

  void CacheMetadata(blink::mojom::CodeCacheType cache_type,
                     const GURL& url,
                     base::Time response_time,
                     const uint8_t* data,
                     size_t data_size) {
    cache_entries_.emplace_back(url, response_time, data,
                                base::checked_cast<wtf_size_t>(data_size));
  }

 private:
  Vector<CacheMetadataEntry> cache_entries_;
};

class CodeCacheHostMockImpl : public blink::mojom::CodeCacheHost {
 public:
  CodeCacheHostMockImpl(MockGeneratedCodeCache* mock_disk_cache)
      : mock_disk_cache_(mock_disk_cache) {}

 private:
  // blink::mojom::CodeCacheHost implementation.
  void DidGenerateCacheableMetadata(blink::mojom::CodeCacheType cache_type,
                                    const GURL& url,
                                    base::Time expected_response_time,
                                    mojo_base::BigBuffer data) override {
    mock_disk_cache_->CacheMetadata(cache_type, url, expected_response_time,
                                    data.data(), data.size());
  }

  void FetchCachedCode(blink::mojom::CodeCacheType cache_type,
                       const GURL& url,
                       FetchCachedCodeCallback) override {}
  void ClearCodeCacheEntry(blink::mojom::CodeCacheType cache_type,
                           const GURL& url) override {}
  void DidGenerateCacheableMetadataInCacheStorage(
      const GURL& url,
      base::Time expected_response_time,
      mojo_base::BigBuffer data,
      const url::Origin& cache_storage_origin,
      const std::string& cache_storage_cache_name) override {}

  MockGeneratedCodeCache* mock_disk_cache_;
};

// Mock CachedMetadataSender implementation that forwards data to the
// mock_disk_cache.
class MockCachedMetadataSender final : public CachedMetadataSender {
 public:
  MockCachedMetadataSender(KURL response_url) : response_url_(response_url) {}

  void Send(CodeCacheHost* code_cache_host,
            const uint8_t* data,
            size_t size) override {
    (*code_cache_host)
        ->DidGenerateCacheableMetadata(
            blink::mojom::CodeCacheType::kJavascript, GURL(response_url_),
            response_time_, mojo_base::BigBuffer(base::make_span(data, size)));
  }

  bool IsServedFromCacheStorage() override { return false; }

 private:
  const KURL response_url_;
  const base::Time response_time_;
};

::testing::AssertionResult CachedMetadataFailure(
    const char* failure_msg,
    const char* actual_expression,
    const Vector<uint8_t>& expected,
    const scoped_refptr<CachedMetadata>& actual) {
  ::testing::Message msg;
  msg << failure_msg << " for " << actual_expression;
  msg << "\n  Expected: [" << expected.size() << "] { ";
  for (wtf_size_t i = 0; i < expected.size(); ++i) {
    if (i > 0)
      msg << ", ";
    msg << std::hex << static_cast<int>(expected[i]);
  }
  msg << " }";
  if (actual) {
    msg << "\n  Actual:   [" << actual->size() << "] { ";
    for (size_t i = 0; i < actual->size(); ++i) {
      if (i > 0)
        msg << ", ";
      msg << std::hex << static_cast<int>(actual->Data()[i]);
    }
    msg << " }";
  } else {
    msg << "\n  Actual:   (null)";
  }

  return testing::AssertionFailure() << msg;
}

::testing::AssertionResult CachedMetadataEqual(
    const char* expected_expression,
    const char* actual_expression,
    const Vector<uint8_t>& expected,
    const scoped_refptr<CachedMetadata>& actual) {
  if (!actual) {
    return CachedMetadataFailure("Expected non-null data", actual_expression,
                                 expected, actual);
  }
  if (actual->size() != expected.size()) {
    return CachedMetadataFailure("Wrong size", actual_expression, expected,
                                 actual);
  }
  const uint8_t* actual_data = actual->Data();
  for (wtf_size_t i = 0; i < expected.size(); ++i) {
    if (actual_data[i] != expected[i]) {
      return CachedMetadataFailure("Wrong data", actual_expression, expected,
                                   actual);
    }
  }

  return testing::AssertionSuccess();
}

#define EXPECT_METADATA(data_array, cached_metadata) \
  EXPECT_PRED_FORMAT2(CachedMetadataEqual, data_array, cached_metadata)

}  // namespace

TEST(SourceKeyedCachedMetadataHandlerTest,
     HandlerForSource_InitiallyNonNullHandlersWithNullData) {
  base::test::SingleThreadTaskEnvironment task_environment;
  MockGeneratedCodeCache mock_disk_cache;

  KURL url("http://SourceKeyedCachedMetadataHandlerTest.com");
  SourceKeyedCachedMetadataHandler* handler =
      MakeGarbageCollected<SourceKeyedCachedMetadataHandler>(
          WTF::TextEncoding(), std::make_unique<MockCachedMetadataSender>(url));

  WTF::String source1("source1");
  SingleCachedMetadataHandler* source1_handler =
      handler->HandlerForSource(source1);

  WTF::String source2("source2");
  SingleCachedMetadataHandler* source2_handler =
      handler->HandlerForSource(source2);

  // Drain the task queue.
  task_environment.RunUntilIdle();

  EXPECT_NE(nullptr, source1_handler);
  EXPECT_EQ(nullptr, source1_handler->GetCachedMetadata(0xbeef));
  EXPECT_NE(nullptr, source2_handler);
  EXPECT_EQ(nullptr, source2_handler->GetCachedMetadata(0x5eed));
}

TEST(SourceKeyedCachedMetadataHandlerTest,
     HandlerForSource_OneHandlerSetOtherNull) {
  base::test::SingleThreadTaskEnvironment task_environment;
  MockGeneratedCodeCache mock_disk_cache;

  std::unique_ptr<mojom::CodeCacheHost> mojo_code_cache_host =
      std::make_unique<CodeCacheHostMockImpl>(&mock_disk_cache);
  mojo::Remote<mojom::CodeCacheHost> remote;
  mojo::Receiver<mojom::CodeCacheHost> receiver(
      mojo_code_cache_host.get(), remote.BindNewPipeAndPassReceiver());
  CodeCacheHost code_cache_host(std::move(remote));

  KURL url("http://SourceKeyedCachedMetadataHandlerTest.com");
  SourceKeyedCachedMetadataHandler* handler =
      MakeGarbageCollected<SourceKeyedCachedMetadataHandler>(
          WTF::TextEncoding(), std::make_unique<MockCachedMetadataSender>(url));

  WTF::String source1("source1");
  SingleCachedMetadataHandler* source1_handler =
      handler->HandlerForSource(source1);

  WTF::String source2("source2");
  SingleCachedMetadataHandler* source2_handler =
      handler->HandlerForSource(source2);

  Vector<uint8_t> data1 = {1, 2, 3};
  source1_handler->SetCachedMetadata(&code_cache_host, 0xbeef, data1.data(),
                                     data1.size());

  // Drain the task queue.
  task_environment.RunUntilIdle();

  EXPECT_NE(nullptr, source1_handler);
  EXPECT_METADATA(data1, source1_handler->GetCachedMetadata(0xbeef));

  EXPECT_NE(nullptr, source2_handler);
  EXPECT_EQ(nullptr, source2_handler->GetCachedMetadata(0x5eed));
}

TEST(SourceKeyedCachedMetadataHandlerTest, HandlerForSource_BothHandlersSet) {
  base::test::SingleThreadTaskEnvironment task_environment;
  MockGeneratedCodeCache mock_disk_cache;

  std::unique_ptr<mojom::CodeCacheHost> mojo_code_cache_host =
      std::make_unique<CodeCacheHostMockImpl>(&mock_disk_cache);
  mojo::Remote<mojom::CodeCacheHost> remote;
  mojo::Receiver<mojom::CodeCacheHost> receiver(
      mojo_code_cache_host.get(), remote.BindNewPipeAndPassReceiver());
  CodeCacheHost code_cache_host(std::move(remote));

  KURL url("http://SourceKeyedCachedMetadataHandlerTest.com");
  SourceKeyedCachedMetadataHandler* handler =
      MakeGarbageCollected<SourceKeyedCachedMetadataHandler>(
          WTF::TextEncoding(), std::make_unique<MockCachedMetadataSender>(url));

  WTF::String source1("source1");
  SingleCachedMetadataHandler* source1_handler =
      handler->HandlerForSource(source1);

  WTF::String source2("source2");
  SingleCachedMetadataHandler* source2_handler =
      handler->HandlerForSource(source2);

  Vector<uint8_t> data1 = {1, 2, 3};
  source1_handler->SetCachedMetadata(&code_cache_host, 0xbeef, data1.data(),
                                     data1.size());

  Vector<uint8_t> data2 = {3, 4, 5, 6};
  source2_handler->SetCachedMetadata(&code_cache_host, 0x5eed, data2.data(),
                                     data2.size());

  // Drain the task queue.
  task_environment.RunUntilIdle();

  EXPECT_NE(nullptr, source1_handler);
  EXPECT_METADATA(data1, source1_handler->GetCachedMetadata(0xbeef));

  EXPECT_NE(nullptr, source2_handler);
  EXPECT_METADATA(data2, source2_handler->GetCachedMetadata(0x5eed));
}

TEST(SourceKeyedCachedMetadataHandlerTest, Serialize_EmptyClearDoesSend) {
  base::test::SingleThreadTaskEnvironment task_environment;
  MockGeneratedCodeCache mock_disk_cache;

  std::unique_ptr<mojom::CodeCacheHost> mojo_code_cache_host =
      std::make_unique<CodeCacheHostMockImpl>(&mock_disk_cache);
  mojo::Remote<mojom::CodeCacheHost> remote;
  mojo::Receiver<mojom::CodeCacheHost> receiver(
      mojo_code_cache_host.get(), remote.BindNewPipeAndPassReceiver());
  CodeCacheHost code_cache_host(std::move(remote));

  KURL url("http://SourceKeyedCachedMetadataHandlerTest.com");
  SourceKeyedCachedMetadataHandler* handler =
      MakeGarbageCollected<SourceKeyedCachedMetadataHandler>(
          WTF::TextEncoding(), std::make_unique<MockCachedMetadataSender>(url));

  // Clear and send to the mock_disk_cache
  handler->ClearCachedMetadata(&code_cache_host,
                               CachedMetadataHandler::kClearPersistentStorage);

  // Drain the task queue.
  task_environment.RunUntilIdle();

  // Load from mock_disk_cache
  Vector<CacheMetadataEntry> cache_metadatas =
      mock_disk_cache.GetCacheMetadatasFor(url);

  EXPECT_EQ(1u, cache_metadatas.size());
}

TEST(SourceKeyedCachedMetadataHandlerTest, Serialize_EachSetDoesSend) {
  base::test::SingleThreadTaskEnvironment task_environment;
  MockGeneratedCodeCache mock_disk_cache;

  std::unique_ptr<mojom::CodeCacheHost> mojo_code_cache_host =
      std::make_unique<CodeCacheHostMockImpl>(&mock_disk_cache);
  mojo::Remote<mojom::CodeCacheHost> remote;
  mojo::Receiver<mojom::CodeCacheHost> receiver(
      mojo_code_cache_host.get(), remote.BindNewPipeAndPassReceiver());
  CodeCacheHost code_cache_host(std::move(remote));

  KURL url("http://SourceKeyedCachedMetadataHandlerTest.com");
  SourceKeyedCachedMetadataHandler* handler =
      MakeGarbageCollected<SourceKeyedCachedMetadataHandler>(
          WTF::TextEncoding(), std::make_unique<MockCachedMetadataSender>(url));

  WTF::String source1("source1");
  SingleCachedMetadataHandler* source1_handler =
      handler->HandlerForSource(source1);

  WTF::String source2("source2");
  SingleCachedMetadataHandler* source2_handler =
      handler->HandlerForSource(source2);

  Vector<uint8_t> data1 = {1, 2, 3};
  source1_handler->SetCachedMetadata(&code_cache_host, 0xbeef, data1.data(),
                                     data1.size());

  Vector<uint8_t> data2 = {3, 4, 5, 6};
  source2_handler->SetCachedMetadata(&code_cache_host, 0x5eed, data2.data(),
                                     data2.size());

  // Drain the task queue.
  task_environment.RunUntilIdle();

  // Load from mock_disk_cache
  Vector<CacheMetadataEntry> cache_metadatas =
      mock_disk_cache.GetCacheMetadatasFor(url);

  EXPECT_EQ(2u, cache_metadatas.size());
}

TEST(SourceKeyedCachedMetadataHandlerTest, Serialize_SetWithNoSendDoesNotSend) {
  base::test::SingleThreadTaskEnvironment task_environment;
  MockGeneratedCodeCache mock_disk_cache;

  std::unique_ptr<mojom::CodeCacheHost> mojo_code_cache_host =
      std::make_unique<CodeCacheHostMockImpl>(&mock_disk_cache);
  mojo::Remote<mojom::CodeCacheHost> remote;
  mojo::Receiver<mojom::CodeCacheHost> receiver(
      mojo_code_cache_host.get(), remote.BindNewPipeAndPassReceiver());
  CodeCacheHost code_cache_host(std::move(remote));

  KURL url("http://SourceKeyedCachedMetadataHandlerTest.com");
  SourceKeyedCachedMetadataHandler* handler =
      MakeGarbageCollected<SourceKeyedCachedMetadataHandler>(
          WTF::TextEncoding(), std::make_unique<MockCachedMetadataSender>(url));

  WTF::String source1("source1");
  SingleCachedMetadataHandler* source1_handler =
      handler->HandlerForSource(source1);

  WTF::String source2("source2");
  SingleCachedMetadataHandler* source2_handler =
      handler->HandlerForSource(source2);

  Vector<uint8_t> data1 = {1, 2, 3};
  source1_handler->DisableSendToPlatformForTesting();
  source1_handler->SetCachedMetadata(&code_cache_host, 0xbeef, data1.data(),
                                     data1.size());

  Vector<uint8_t> data2 = {3, 4, 5, 6};
  source2_handler->SetCachedMetadata(&code_cache_host, 0x5eed, data2.data(),
                                     data2.size());

  // Drain the task queue.
  task_environment.RunUntilIdle();

  // Load from mock_disk_cache
  Vector<CacheMetadataEntry> cache_metadatas =
      mock_disk_cache.GetCacheMetadatasFor(url);

  EXPECT_EQ(1u, cache_metadatas.size());
}

TEST(SourceKeyedCachedMetadataHandlerTest,
     SerializeAndDeserialize_NoHandlersSet) {
  base::test::SingleThreadTaskEnvironment task_environment;
  MockGeneratedCodeCache mock_disk_cache;

  std::unique_ptr<mojom::CodeCacheHost> mojo_code_cache_host =
      std::make_unique<CodeCacheHostMockImpl>(&mock_disk_cache);
  mojo::Remote<mojom::CodeCacheHost> remote;
  mojo::Receiver<mojom::CodeCacheHost> receiver(
      mojo_code_cache_host.get(), remote.BindNewPipeAndPassReceiver());
  CodeCacheHost code_cache_host(std::move(remote));

  KURL url("http://SourceKeyedCachedMetadataHandlerTest.com");
  WTF::String source1("source1");
  WTF::String source2("source2");
  {
    SourceKeyedCachedMetadataHandler* handler =
        MakeGarbageCollected<SourceKeyedCachedMetadataHandler>(
            WTF::TextEncoding(),
            std::make_unique<MockCachedMetadataSender>(url));

    // Clear and persist in the mock_disk_cache.
    handler->ClearCachedMetadata(
        &code_cache_host, CachedMetadataHandler::kClearPersistentStorage);
  }

  // Drain the task queue.
  task_environment.RunUntilIdle();

  // Reload from mock_disk_cache
  {
    Vector<CacheMetadataEntry> cache_metadatas =
        mock_disk_cache.GetCacheMetadatasFor(url);
    // Use the last data received by the mock_disk_cache
    EXPECT_EQ(1u, cache_metadatas.size());
    CacheMetadataEntry& last_cache_metadata = cache_metadatas[0];

    SourceKeyedCachedMetadataHandler* handler =
        MakeGarbageCollected<SourceKeyedCachedMetadataHandler>(
            WTF::TextEncoding(),
            std::make_unique<MockCachedMetadataSender>(url));
    auto data = base::make_span(last_cache_metadata.data.data(),
                                last_cache_metadata.data.size());
    handler->SetSerializedCachedMetadata(mojo_base::BigBuffer(data));

    SingleCachedMetadataHandler* source1_handler =
        handler->HandlerForSource(source1);
    SingleCachedMetadataHandler* source2_handler =
        handler->HandlerForSource(source2);

    EXPECT_NE(nullptr, source1_handler);
    EXPECT_EQ(nullptr, source1_handler->GetCachedMetadata(0xbeef));

    EXPECT_NE(nullptr, source2_handler);
    EXPECT_EQ(nullptr, source2_handler->GetCachedMetadata(0x5eed));
  }
}

TEST(SourceKeyedCachedMetadataHandlerTest,
     SerializeAndDeserialize_BothHandlersSet) {
  base::test::SingleThreadTaskEnvironment task_environment;
  MockGeneratedCodeCache mock_disk_cache;

  std::unique_ptr<mojom::CodeCacheHost> mojo_code_cache_host =
      std::make_unique<CodeCacheHostMockImpl>(&mock_disk_cache);
  mojo::Remote<mojom::CodeCacheHost> remote;
  mojo::Receiver<mojom::CodeCacheHost> receiver(
      mojo_code_cache_host.get(), remote.BindNewPipeAndPassReceiver());
  CodeCacheHost code_cache_host(std::move(remote));

  KURL url("http://SourceKeyedCachedMetadataHandlerTest.com");
  WTF::String source1("source1");
  WTF::String source2("source2");
  Vector<uint8_t> data1 = {1, 2, 3};
  Vector<uint8_t> data2 = {3, 4, 5, 6};
  {
    SourceKeyedCachedMetadataHandler* handler =
        MakeGarbageCollected<SourceKeyedCachedMetadataHandler>(
            WTF::TextEncoding(),
            std::make_unique<MockCachedMetadataSender>(url));

    SingleCachedMetadataHandler* source1_handler =
        handler->HandlerForSource(source1);
    SingleCachedMetadataHandler* source2_handler =
        handler->HandlerForSource(source2);

    source1_handler->SetCachedMetadata(&code_cache_host, 0xbeef, data1.data(),
                                       data1.size());
    source2_handler->SetCachedMetadata(&code_cache_host, 0x5eed, data2.data(),
                                       data2.size());
  }

  // Drain the task queue.
  task_environment.RunUntilIdle();

  // Reload from mock_disk_cache
  {
    Vector<CacheMetadataEntry> cache_metadatas =
        mock_disk_cache.GetCacheMetadatasFor(url);
    // Use the last data received by the mock_disk_cache
    EXPECT_EQ(2u, cache_metadatas.size());
    CacheMetadataEntry& last_cache_metadata = cache_metadatas[1];

    SourceKeyedCachedMetadataHandler* handler =
        MakeGarbageCollected<SourceKeyedCachedMetadataHandler>(
            WTF::TextEncoding(),
            std::make_unique<MockCachedMetadataSender>(url));
    auto data = base::make_span(last_cache_metadata.data.data(),
                                last_cache_metadata.data.size());
    handler->SetSerializedCachedMetadata(mojo_base::BigBuffer(data));

    SingleCachedMetadataHandler* source1_handler =
        handler->HandlerForSource(source1);
    SingleCachedMetadataHandler* source2_handler =
        handler->HandlerForSource(source2);

    EXPECT_NE(nullptr, source1_handler);
    EXPECT_METADATA(data1, source1_handler->GetCachedMetadata(0xbeef));

    EXPECT_NE(nullptr, source2_handler);
    EXPECT_METADATA(data2, source2_handler->GetCachedMetadata(0x5eed));
  }
}

}  // namespace blink
