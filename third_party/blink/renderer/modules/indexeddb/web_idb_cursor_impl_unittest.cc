// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/web_idb_cursor_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/indexeddb/indexed_db_key_builder.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/mock_web_idb_callbacks.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

using blink::IndexedDBKey;
using blink::WebBlobInfo;
using blink::WebData;
using blink::WebIDBCallbacks;
using blink::WebIDBKey;
using blink::kWebIDBKeyTypeNumber;
using blink::WebIDBValue;
using blink::WebString;
using blink::WebVector;
using blink::mojom::blink::IDBCursor;
using testing::StrictMock;

namespace blink {

namespace {

class MockCursorImpl : public IDBCursor {
 public:
  explicit MockCursorImpl(mojom::blink::IDBCursorAssociatedRequest request)
      : binding_(this, std::move(request)) {
    binding_.set_connection_error_handler(base::BindOnce(
        &MockCursorImpl::CursorDestroyed, base::Unretained(this)));
  }

  void Prefetch(
      int32_t count,
      mojom::blink::IDBCallbacksAssociatedPtrInfo callbacks) override {
    ++prefetch_calls_;
    last_prefetch_count_ = count;
  }

  void PrefetchReset(int32_t used_prefetches,
                     int32_t unused_prefetches) override {
    ++reset_calls_;
    last_used_count_ = used_prefetches;
  }

  void Advance(uint32_t count,
               mojom::blink::IDBCallbacksAssociatedPtrInfo callbacks) override {
    ++advance_calls_;
  }

  void CursorContinue(
      WebIDBKey key,
      WebIDBKey primary_key,
      mojom::blink::IDBCallbacksAssociatedPtrInfo callbacks) override {
    ++continue_calls_;
  }

  void CursorDestroyed() { destroyed_ = true; }

  int prefetch_calls() { return prefetch_calls_; }
  int last_prefetch_count() { return last_prefetch_count_; }
  int reset_calls() { return reset_calls_; }
  int last_used_count() { return last_used_count_; }
  int advance_calls() { return advance_calls_; }
  int continue_calls() { return continue_calls_; }
  bool destroyed() { return destroyed_; }

 private:
  int prefetch_calls_ = 0;
  int last_prefetch_count_ = 0;
  int reset_calls_ = 0;
  int last_used_count_ = 0;
  int advance_calls_ = 0;
  int continue_calls_ = 0;
  bool destroyed_ = false;

  mojo::AssociatedBinding<IDBCursor> binding_;
};

class MockContinueCallbacks : public StrictMock<MockWebIDBCallbacks> {
 public:
  MockContinueCallbacks(IndexedDBKey* key = nullptr,
                        WebVector<WebBlobInfo>* blobs = nullptr)
      : key_(key), blobs_(blobs) {}

  void OnSuccess(WebIDBKey key,
                 WebIDBKey primaryKey,
                 WebIDBValue value) override {
    if (key_)
      *key_ = IndexedDBKeyBuilder::Build(key.View());
    if (blobs_)
      *blobs_ = value.BlobInfoForTesting();
  }

 private:
  IndexedDBKey* key_;
  WebVector<WebBlobInfo>* blobs_;
};

}  // namespace

class WebIDBCursorImplTest : public testing::Test {
 public:
  WebIDBCursorImplTest() : null_key_(WebIDBKey::CreateNull()) {
    mojom::blink::IDBCursorAssociatedPtr ptr;
    mock_cursor_ = std::make_unique<MockCursorImpl>(
        mojo::MakeRequestAssociatedWithDedicatedPipe(&ptr));
    cursor_ = std::make_unique<WebIDBCursorImpl>(ptr.PassInterface(), 1);
  }

 protected:
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
  WebIDBKey null_key_;
  std::unique_ptr<WebIDBCursorImpl> cursor_;
  std::unique_ptr<MockCursorImpl> mock_cursor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebIDBCursorImplTest);
};

TEST_F(WebIDBCursorImplTest, PrefetchTest) {
  // Call continue() until prefetching should kick in.
  int continue_calls = 0;
  EXPECT_EQ(mock_cursor_->continue_calls(), 0);
  for (int i = 0; i < WebIDBCursorImpl::kPrefetchContinueThreshold; ++i) {
    cursor_->CursorContinue(null_key_.View(), null_key_.View(),
                            new MockContinueCallbacks());
    platform_->RunUntilIdle();
    EXPECT_EQ(++continue_calls, mock_cursor_->continue_calls());
    EXPECT_EQ(0, mock_cursor_->prefetch_calls());
  }

  // Do enough repetitions to verify that the count grows each time,
  // but not so many that the maximum limit is hit.
  const int kPrefetchRepetitions = 5;

  int expected_key = 0;
  int last_prefetch_count = 0;
  for (int repetitions = 0; repetitions < kPrefetchRepetitions; ++repetitions) {
    // Initiate the prefetch
    cursor_->CursorContinue(null_key_.View(), null_key_.View(),
                            new MockContinueCallbacks());
    platform_->RunUntilIdle();
    EXPECT_EQ(continue_calls, mock_cursor_->continue_calls());
    EXPECT_EQ(repetitions + 1, mock_cursor_->prefetch_calls());

    // Verify that the requested count has increased since last time.
    int prefetch_count = mock_cursor_->last_prefetch_count();
    EXPECT_GT(prefetch_count, last_prefetch_count);
    last_prefetch_count = prefetch_count;

    // Fill the prefetch cache as requested.
    Vector<WebIDBKey> keys;
    Vector<WebIDBKey> primary_keys;
    Vector<WebIDBValue> values;
    size_t expected_size = 0;
    for (int i = 0; i < prefetch_count; ++i) {
      WebIDBKey key = WebIDBKey::CreateNumber(expected_key + i);
      keys.emplace_back(std::move(key));
      primary_keys.emplace_back();
      expected_size++;
      EXPECT_EQ(expected_size, keys.size());
      EXPECT_EQ(expected_size, primary_keys.size());
      WebVector<WebBlobInfo> blob_info;
      blob_info.reserve(expected_key + i);
      for (int j = 0; j < expected_key + i; ++j) {
        blob_info.emplace_back(WebBlobInfo::BlobForTesting(
            WebString("blobuuid"), "text/plain", 123));
      }
      values.emplace_back(WebData(), std::move(blob_info));
    }
    cursor_->SetPrefetchData(std::move(keys), std::move(primary_keys),
                             std::move(values));

    // Note that the real dispatcher would call cursor->CachedContinue()
    // immediately after cursor->SetPrefetchData() to service the request
    // that initiated the prefetch.

    // Verify that the cache is used for subsequent continue() calls.
    for (int i = 0; i < prefetch_count; ++i) {
      IndexedDBKey key;
      WebVector<WebBlobInfo> blobs;
      cursor_->CursorContinue(null_key_.View(), null_key_.View(),
                              new MockContinueCallbacks(&key, &blobs));
      platform_->RunUntilIdle();
      EXPECT_EQ(continue_calls, mock_cursor_->continue_calls());
      EXPECT_EQ(repetitions + 1, mock_cursor_->prefetch_calls());

      EXPECT_EQ(kWebIDBKeyTypeNumber, key.type());
      EXPECT_EQ(expected_key, static_cast<int>(blobs.size()));
      EXPECT_EQ(expected_key++, key.number());
    }
  }

  cursor_.reset();
  platform_->RunUntilIdle();
  EXPECT_TRUE(mock_cursor_->destroyed());
}

TEST_F(WebIDBCursorImplTest, AdvancePrefetchTest) {
  // Call continue() until prefetching should kick in.
  EXPECT_EQ(0, mock_cursor_->continue_calls());
  for (int i = 0; i < WebIDBCursorImpl::kPrefetchContinueThreshold; ++i) {
    cursor_->CursorContinue(null_key_.View(), null_key_.View(),
                            new MockContinueCallbacks());
  }
  platform_->RunUntilIdle();
  EXPECT_EQ(0, mock_cursor_->prefetch_calls());

  // Initiate the prefetch
  cursor_->CursorContinue(null_key_.View(), null_key_.View(),
                          new MockContinueCallbacks());

  platform_->RunUntilIdle();
  EXPECT_EQ(1, mock_cursor_->prefetch_calls());
  EXPECT_EQ(static_cast<int>(WebIDBCursorImpl::kPrefetchContinueThreshold),
            mock_cursor_->continue_calls());
  EXPECT_EQ(0, mock_cursor_->advance_calls());

  const int prefetch_count = mock_cursor_->last_prefetch_count();

  // Fill the prefetch cache as requested.
  int expected_key = 0;
  Vector<WebIDBKey> keys;
  Vector<WebIDBKey> primary_keys;
  Vector<WebIDBValue> values;
  size_t expected_size = 0;
  for (int i = 0; i < prefetch_count; ++i) {
    WebIDBKey key = WebIDBKey::CreateNumber(expected_key + i);
    keys.emplace_back(std::move(key));
    primary_keys.emplace_back();
    expected_size++;
    EXPECT_EQ(expected_size, keys.size());
    EXPECT_EQ(expected_size, primary_keys.size());
    WebVector<WebBlobInfo> blob_info;
    blob_info.reserve(expected_key + i);
    for (int j = 0; j < expected_key + i; ++j) {
      blob_info.emplace_back(WebBlobInfo::BlobForTesting(WebString("blobuuid"),
                                                         "text/plain", 123));
    }
    values.emplace_back(WebData(), std::move(blob_info));
  }
  cursor_->SetPrefetchData(std::move(keys), std::move(primary_keys),
                           std::move(values));

  // Note that the real dispatcher would call cursor->CachedContinue()
  // immediately after cursor->SetPrefetchData() to service the request
  // that initiated the prefetch.

  // Need at least this many in the cache for the test steps.
  ASSERT_GE(prefetch_count, 5);

  // IDBCursor.continue()
  IndexedDBKey key;
  cursor_->CursorContinue(null_key_.View(), null_key_.View(),
                          new MockContinueCallbacks(&key));
  platform_->RunUntilIdle();
  EXPECT_EQ(0, key.number());

  // IDBCursor.advance(1)
  cursor_->Advance(1, new MockContinueCallbacks(&key));
  platform_->RunUntilIdle();
  EXPECT_EQ(1, key.number());

  // IDBCursor.continue()
  cursor_->CursorContinue(null_key_.View(), null_key_.View(),
                          new MockContinueCallbacks(&key));
  platform_->RunUntilIdle();
  EXPECT_EQ(2, key.number());

  // IDBCursor.advance(2)
  cursor_->Advance(2, new MockContinueCallbacks(&key));
  platform_->RunUntilIdle();
  EXPECT_EQ(4, key.number());

  EXPECT_EQ(0, mock_cursor_->advance_calls());

  // IDBCursor.advance(lots) - beyond the fetched amount
  cursor_->Advance(WebIDBCursorImpl::kMaxPrefetchAmount,
                   new MockContinueCallbacks(&key));
  platform_->RunUntilIdle();
  EXPECT_EQ(1, mock_cursor_->advance_calls());
  EXPECT_EQ(1, mock_cursor_->prefetch_calls());
  EXPECT_EQ(static_cast<int>(WebIDBCursorImpl::kPrefetchContinueThreshold),
            mock_cursor_->continue_calls());

  cursor_.reset();
  platform_->RunUntilIdle();
  EXPECT_TRUE(mock_cursor_->destroyed());
}

TEST_F(WebIDBCursorImplTest, PrefetchReset) {
  // Call continue() until prefetching should kick in.
  int continue_calls = 0;
  EXPECT_EQ(mock_cursor_->continue_calls(), 0);
  for (int i = 0; i < WebIDBCursorImpl::kPrefetchContinueThreshold; ++i) {
    cursor_->CursorContinue(null_key_.View(), null_key_.View(),
                            new MockContinueCallbacks());
    platform_->RunUntilIdle();
    EXPECT_EQ(++continue_calls, mock_cursor_->continue_calls());
    EXPECT_EQ(0, mock_cursor_->prefetch_calls());
  }

  // Initiate the prefetch
  cursor_->CursorContinue(null_key_.View(), null_key_.View(),
                          new MockContinueCallbacks());
  platform_->RunUntilIdle();
  EXPECT_EQ(continue_calls, mock_cursor_->continue_calls());
  EXPECT_EQ(1, mock_cursor_->prefetch_calls());
  EXPECT_EQ(0, mock_cursor_->reset_calls());

  // Now invalidate it
  cursor_->ResetPrefetchCache();

  // No reset should have been sent since nothing has been received yet.
  platform_->RunUntilIdle();
  EXPECT_EQ(0, mock_cursor_->reset_calls());

  // Fill the prefetch cache as requested.
  int prefetch_count = mock_cursor_->last_prefetch_count();
  Vector<WebIDBKey> keys(prefetch_count);
  Vector<WebIDBKey> primary_keys(prefetch_count);
  Vector<WebIDBValue> values;
  for (int i = 0; i < prefetch_count; ++i)
    values.emplace_back(WebData(), WebVector<WebBlobInfo>());
  cursor_->SetPrefetchData(std::move(keys), std::move(primary_keys),
                           std::move(values));

  // No reset should have been sent since prefetch data hasn't been used.
  platform_->RunUntilIdle();
  EXPECT_EQ(0, mock_cursor_->reset_calls());

  // The real dispatcher would call cursor->CachedContinue(), so do that:
  MockContinueCallbacks callbacks;
  cursor_->CachedContinue(&callbacks);

  // Now the cursor should have reset the rest of the cache.
  platform_->RunUntilIdle();
  EXPECT_EQ(1, mock_cursor_->reset_calls());
  EXPECT_EQ(1, mock_cursor_->last_used_count());

  cursor_.reset();
  platform_->RunUntilIdle();
  EXPECT_TRUE(mock_cursor_->destroyed());
}

}  // namespace blink
