// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/web_idb_cursor_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/mock_web_idb_callbacks.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

class MockCursorImpl : public mojom::blink::IDBCursor {
 public:
  explicit MockCursorImpl(
      mojo::PendingAssociatedReceiver<mojom::blink::IDBCursor> receiver)
      : receiver_(this, std::move(receiver)) {
    receiver_.set_disconnect_handler(base::BindOnce(
        &MockCursorImpl::CursorDestroyed, base::Unretained(this)));
  }

  void Prefetch(int32_t count,
                mojom::blink::IDBCursor::PrefetchCallback callback) override {
    ++prefetch_calls_;
    last_prefetch_count_ = count;
    std::move(callback).Run(mojom::blink::IDBCursorResult::NewEmpty(true));
  }

  void PrefetchReset(int32_t used_prefetches,
                     int32_t unused_prefetches) override {
    ++reset_calls_;
    last_used_count_ = used_prefetches;
  }

  void Advance(uint32_t count,
               mojom::blink::IDBCursor::AdvanceCallback callback) override {
    ++advance_calls_;
    std::move(callback).Run(mojom::blink::IDBCursorResult::NewEmpty(true));
  }

  void CursorContinue(
      std::unique_ptr<IDBKey> key,
      std::unique_ptr<IDBKey> primary_key,
      mojom::blink::IDBCursor::CursorContinueCallback callback) override {
    ++continue_calls_;
    std::move(callback).Run(mojom::blink::IDBCursorResult::NewEmpty(true));
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

  mojo::AssociatedReceiver<IDBCursor> receiver_;
};

class MockContinueCallbacks : public testing::StrictMock<MockWebIDBCallbacks> {
 public:
  MockContinueCallbacks(std::unique_ptr<IDBKey>* key = nullptr,
                        Vector<WebBlobInfo>* blobs = nullptr)
      : key_(key), blobs_(blobs) {}

  void SetState(base::WeakPtr<WebIDBCursorImpl> cursor,
                int64_t transaction_id) override {}
  void SuccessValue(mojom::blink::IDBReturnValuePtr return_value) override {}

  void SuccessCursorContinue(
      std::unique_ptr<IDBKey> key,
      std::unique_ptr<IDBKey> primaryKey,
      base::Optional<std::unique_ptr<IDBValue>> value) override {
    if (key_)
      *key_ = IDBKey::Clone(key);
    if (blobs_ && value.has_value())
      *blobs_ = value.value()->BlobInfo();
  }

 private:
  std::unique_ptr<IDBKey>* key_;
  Vector<WebBlobInfo>* blobs_;
};

}  // namespace

class WebIDBCursorImplTest : public testing::Test {
 public:
  WebIDBCursorImplTest() : null_key_(IDBKey::CreateNone()) {
    mojo::AssociatedRemote<mojom::blink::IDBCursor> remote;
    mock_cursor_ = std::make_unique<MockCursorImpl>(
        remote.BindNewEndpointAndPassDedicatedReceiverForTesting());
    cursor_ = std::make_unique<WebIDBCursorImpl>(
        remote.Unbind(), 1,
        blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  }

 protected:
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
  std::unique_ptr<IDBKey> null_key_;
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
    cursor_->CursorContinue(null_key_.get(), null_key_.get(),
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
    cursor_->CursorContinue(null_key_.get(), null_key_.get(),
                            new MockContinueCallbacks());
    platform_->RunUntilIdle();
    EXPECT_EQ(continue_calls, mock_cursor_->continue_calls());
    EXPECT_EQ(repetitions + 1, mock_cursor_->prefetch_calls());

    // Verify that the requested count has increased since last time.
    int prefetch_count = mock_cursor_->last_prefetch_count();
    EXPECT_GT(prefetch_count, last_prefetch_count);
    last_prefetch_count = prefetch_count;

    // Fill the prefetch cache as requested.
    Vector<std::unique_ptr<IDBKey>> keys;
    Vector<std::unique_ptr<IDBKey>> primary_keys;
    Vector<std::unique_ptr<IDBValue>> values;
    size_t expected_size = 0;
    for (int i = 0; i < prefetch_count; ++i) {
      std::unique_ptr<IDBKey> key = IDBKey::CreateNumber(expected_key + i);
      keys.emplace_back(std::move(key));
      primary_keys.emplace_back();
      expected_size++;
      EXPECT_EQ(expected_size, keys.size());
      EXPECT_EQ(expected_size, primary_keys.size());
      Vector<WebBlobInfo> blob_info;
      blob_info.ReserveInitialCapacity(expected_key + i);
      for (int j = 0; j < expected_key + i; ++j) {
        blob_info.emplace_back(WebBlobInfo::BlobForTesting(
            WebString("blobuuid"), "text/plain", 123));
      }
      values.emplace_back(std::make_unique<IDBValue>(
          scoped_refptr<SharedBuffer>(), std::move(blob_info)));
    }
    cursor_->SetPrefetchData(std::move(keys), std::move(primary_keys),
                             std::move(values));

    // Note that the real dispatcher would call cursor->CachedContinue()
    // immediately after cursor->SetPrefetchData() to service the request
    // that initiated the prefetch.

    // Verify that the cache is used for subsequent continue() calls.
    for (int i = 0; i < prefetch_count; ++i) {
      std::unique_ptr<IDBKey> key;
      Vector<WebBlobInfo> blobs;
      cursor_->CursorContinue(null_key_.get(), null_key_.get(),
                              new MockContinueCallbacks(&key, &blobs));
      platform_->RunUntilIdle();
      EXPECT_EQ(continue_calls, mock_cursor_->continue_calls());
      EXPECT_EQ(repetitions + 1, mock_cursor_->prefetch_calls());

      EXPECT_EQ(mojom::IDBKeyType::Number, key->GetType());
      EXPECT_EQ(expected_key, static_cast<int>(blobs.size()));
      EXPECT_EQ(expected_key++, key->Number());
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
    cursor_->CursorContinue(null_key_.get(), null_key_.get(),
                            new MockContinueCallbacks());
  }
  platform_->RunUntilIdle();
  EXPECT_EQ(0, mock_cursor_->prefetch_calls());

  // Initiate the prefetch
  cursor_->CursorContinue(null_key_.get(), null_key_.get(),
                          new MockContinueCallbacks());

  platform_->RunUntilIdle();
  EXPECT_EQ(1, mock_cursor_->prefetch_calls());
  EXPECT_EQ(static_cast<int>(WebIDBCursorImpl::kPrefetchContinueThreshold),
            mock_cursor_->continue_calls());
  EXPECT_EQ(0, mock_cursor_->advance_calls());

  const int prefetch_count = mock_cursor_->last_prefetch_count();

  // Fill the prefetch cache as requested.
  int expected_key = 0;
  Vector<std::unique_ptr<IDBKey>> keys;
  Vector<std::unique_ptr<IDBKey>> primary_keys;
  Vector<std::unique_ptr<IDBValue>> values;
  size_t expected_size = 0;
  for (int i = 0; i < prefetch_count; ++i) {
    std::unique_ptr<IDBKey> key = IDBKey::CreateNumber(expected_key + i);
    keys.emplace_back(std::move(key));
    primary_keys.emplace_back();
    expected_size++;
    EXPECT_EQ(expected_size, keys.size());
    EXPECT_EQ(expected_size, primary_keys.size());
    Vector<WebBlobInfo> blob_info;
    blob_info.ReserveInitialCapacity(expected_key + i);
    for (int j = 0; j < expected_key + i; ++j) {
      blob_info.emplace_back(WebBlobInfo::BlobForTesting(WebString("blobuuid"),
                                                         "text/plain", 123));
    }
    values.emplace_back(std::make_unique<IDBValue>(
        scoped_refptr<SharedBuffer>(), std::move(blob_info)));
  }
  cursor_->SetPrefetchData(std::move(keys), std::move(primary_keys),
                           std::move(values));

  // Note that the real dispatcher would call cursor->CachedContinue()
  // immediately after cursor->SetPrefetchData() to service the request
  // that initiated the prefetch.

  // Need at least this many in the cache for the test steps.
  ASSERT_GE(prefetch_count, 5);

  // IDBCursor.continue()
  std::unique_ptr<IDBKey> key;
  cursor_->CursorContinue(null_key_.get(), null_key_.get(),
                          new MockContinueCallbacks(&key));
  platform_->RunUntilIdle();
  EXPECT_EQ(0, key->Number());

  // IDBCursor.advance(1)
  cursor_->Advance(1, new MockContinueCallbacks(&key));
  platform_->RunUntilIdle();
  EXPECT_EQ(1, key->Number());

  // IDBCursor.continue()
  cursor_->CursorContinue(null_key_.get(), null_key_.get(),
                          new MockContinueCallbacks(&key));
  platform_->RunUntilIdle();
  EXPECT_EQ(2, key->Number());

  // IDBCursor.advance(2)
  cursor_->Advance(2, new MockContinueCallbacks(&key));
  platform_->RunUntilIdle();
  EXPECT_EQ(4, key->Number());

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
    cursor_->CursorContinue(null_key_.get(), null_key_.get(),
                            new MockContinueCallbacks());
    platform_->RunUntilIdle();
    EXPECT_EQ(++continue_calls, mock_cursor_->continue_calls());
    EXPECT_EQ(0, mock_cursor_->prefetch_calls());
  }

  // Initiate the prefetch
  cursor_->CursorContinue(null_key_.get(), null_key_.get(),
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
  Vector<std::unique_ptr<IDBKey>> keys(prefetch_count);
  Vector<std::unique_ptr<IDBKey>> primary_keys(prefetch_count);
  Vector<std::unique_ptr<IDBValue>> values;
  for (int i = 0; i < prefetch_count; ++i) {
    values.emplace_back(std::make_unique<IDBValue>(
        scoped_refptr<SharedBuffer>(), Vector<WebBlobInfo>()));
  }
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
