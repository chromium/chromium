// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/source_keyed_cached_metadata_handler.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_crypto.h"
#include "third_party/blink/renderer/platform/crypto.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

// Structure holding cache metadata sent to the platform.
struct CacheMetadataEntry {
  CacheMetadataEntry(const WebURL& url,
                     base::Time response_time,
                     const uint8_t* data,
                     wtf_size_t data_size)
      : url(url), response_time(response_time) {
    this->data.Append(data, data_size);
  }

  WebURL url;
  base::Time response_time;
  Vector<uint8_t> data;
};

// Mock Platform implementation that provides basic crypto and caching.
class SourceKeyedCachedMetadataHandlerMockPlatform final
    : public TestingPlatformSupportWithMockScheduler {
 public:
  SourceKeyedCachedMetadataHandlerMockPlatform() {}
  ~SourceKeyedCachedMetadataHandlerMockPlatform() override = default;

  void CacheMetadata(blink::mojom::CodeCacheType cache_type,
                     const WebURL& url,
                     base::Time response_time,
                     const uint8_t* data,
                     size_t data_size) override {
    cache_entries_.emplace_back(url, response_time, data,
                                SafeCast<wtf_size_t>(data_size));
  }

  bool HasCacheMetadataFor(const WebURL& url) {
    for (const CacheMetadataEntry& entry : cache_entries_) {
      if (entry.url == url) {
        return true;
      }
    }
    return false;
  }

  Vector<CacheMetadataEntry> GetCacheMetadatasFor(const WebURL& url) {
    Vector<CacheMetadataEntry> url_entries;
    for (const CacheMetadataEntry& entry : cache_entries_) {
      if (entry.url == url) {
        url_entries.push_back(entry);
      }
    }
    return url_entries;
  }

 private:
  Vector<CacheMetadataEntry> cache_entries_;
};

// Mock CachedMetadataSender implementation that forwards data to the platform.
class MockCachedMetadataSender final : public CachedMetadataSender {
 public:
  MockCachedMetadataSender(KURL response_url) : response_url_(response_url) {}

  void Send(const uint8_t* data, size_t size) override {
    Platform::Current()->CacheMetadata(blink::mojom::CodeCacheType::kJavascript,
                                       response_url_, response_time_, data,
                                       size);
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
  for (size_t i = 0; i < expected.size(); ++i) {
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
  for (size_t i = 0; i < expected.size(); ++i) {
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
  ScopedTestingPlatformSupport<SourceKeyedCachedMetadataHandlerMockPlatform>
      platform;

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

  EXPECT_NE(nullptr, source1_handler);
  EXPECT_EQ(nullptr, source1_handler->GetCachedMetadata(0xbeef));
  EXPECT_NE(nullptr, source2_handler);
  EXPECT_EQ(nullptr, source2_handler->GetCachedMetadata(0x5eed));
}

TEST(SourceKeyedCachedMetadataHandlerTest,
     HandlerForSource_OneHandlerSetOtherNull) {
  ScopedTestingPlatformSupport<SourceKeyedCachedMetadataHandlerMockPlatform>
      platform;

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
  source1_handler->SetCachedMetadata(0xbeef, data1.data(), data1.size());

  EXPECT_NE(nullptr, source1_handler);
  EXPECT_METADATA(data1, source1_handler->GetCachedMetadata(0xbeef));

  EXPECT_NE(nullptr, source2_handler);
  EXPECT_EQ(nullptr, source2_handler->GetCachedMetadata(0x5eed));
}

TEST(SourceKeyedCachedMetadataHandlerTest, HandlerForSource_BothHandlersSet) {
  ScopedTestingPlatformSupport<SourceKeyedCachedMetadataHandlerMockPlatform>
      platform;

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
  source1_handler->SetCachedMetadata(0xbeef, data1.data(), data1.size());

  Vector<uint8_t> data2 = {3, 4, 5, 6};
  source2_handler->SetCachedMetadata(0x5eed, data2.data(), data2.size());

  EXPECT_NE(nullptr, source1_handler);
  EXPECT_METADATA(data1, source1_handler->GetCachedMetadata(0xbeef));

  EXPECT_NE(nullptr, source2_handler);
  EXPECT_METADATA(data2, source2_handler->GetCachedMetadata(0x5eed));
}

TEST(SourceKeyedCachedMetadataHandlerTest, Serialize_EmptyClearDoesSend) {
  ScopedTestingPlatformSupport<SourceKeyedCachedMetadataHandlerMockPlatform>
      platform;

  KURL url("http://SourceKeyedCachedMetadataHandlerTest.com");
  SourceKeyedCachedMetadataHandler* handler =
      MakeGarbageCollected<SourceKeyedCachedMetadataHandler>(
          WTF::TextEncoding(), std::make_unique<MockCachedMetadataSender>(url));

  // Clear and send to the platform
  handler->ClearCachedMetadata(CachedMetadataHandler::kSendToPlatform);

  // Load from platform
  Vector<CacheMetadataEntry> cache_metadatas =
      platform->GetCacheMetadatasFor(url);

  EXPECT_EQ(1u, cache_metadatas.size());
}

TEST(SourceKeyedCachedMetadataHandlerTest, Serialize_EachSetDoesSend) {
  ScopedTestingPlatformSupport<SourceKeyedCachedMetadataHandlerMockPlatform>
      platform;

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
  source1_handler->SetCachedMetadata(0xbeef, data1.data(), data1.size());

  Vector<uint8_t> data2 = {3, 4, 5, 6};
  source2_handler->SetCachedMetadata(0x5eed, data2.data(), data2.size());

  // Load from platform
  Vector<CacheMetadataEntry> cache_metadatas =
      platform->GetCacheMetadatasFor(url);

  EXPECT_EQ(2u, cache_metadatas.size());
}

TEST(SourceKeyedCachedMetadataHandlerTest, Serialize_SetWithNoSendDoesNotSend) {
  ScopedTestingPlatformSupport<SourceKeyedCachedMetadataHandlerMockPlatform>
      platform;

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
  source1_handler->SetCachedMetadata(0xbeef, data1.data(), data1.size(),
                                     CachedMetadataHandler::kCacheLocally);

  Vector<uint8_t> data2 = {3, 4, 5, 6};
  source2_handler->SetCachedMetadata(0x5eed, data2.data(), data2.size());

  // Load from platform
  Vector<CacheMetadataEntry> cache_metadatas =
      platform->GetCacheMetadatasFor(url);

  EXPECT_EQ(1u, cache_metadatas.size());
}

TEST(SourceKeyedCachedMetadataHandlerTest,
     SerializeAndDeserialize_NoHandlersSet) {
  ScopedTestingPlatformSupport<SourceKeyedCachedMetadataHandlerMockPlatform>
      platform;

  KURL url("http://SourceKeyedCachedMetadataHandlerTest.com");
  WTF::String source1("source1");
  WTF::String source2("source2");
  {
    SourceKeyedCachedMetadataHandler* handler =
        MakeGarbageCollected<SourceKeyedCachedMetadataHandler>(
            WTF::TextEncoding(),
            std::make_unique<MockCachedMetadataSender>(url));

    // Clear and send to the platform
    handler->ClearCachedMetadata(CachedMetadataHandler::kSendToPlatform);
  }

  // Reload from platform
  {
    Vector<CacheMetadataEntry> cache_metadatas =
        platform->GetCacheMetadatasFor(url);
    // Use the last data received by the platform
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
  ScopedTestingPlatformSupport<SourceKeyedCachedMetadataHandlerMockPlatform>
      platform;

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

    source1_handler->SetCachedMetadata(0xbeef, data1.data(), data1.size());
    source2_handler->SetCachedMetadata(0x5eed, data2.data(), data2.size());
  }

  // Reload from platform
  {
    Vector<CacheMetadataEntry> cache_metadatas =
        platform->GetCacheMetadatasFor(url);
    // Use the last data received by the platform
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
