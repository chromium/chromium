// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/idb_cursor.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <utility>

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_idbcursor_idbindex_idbobjectstore.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_idbindex_idbobjectstore.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_metadata.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_test_helper.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_transaction.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/modules/indexeddb/mock_idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/mock_idb_transaction.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

static constexpr int64_t kStoreId = 5678;
static constexpr int64_t kTransactionId = 1234;

namespace {

class MockCursorImpl : public mojom::blink::IDBCursor {
 public:
  explicit MockCursorImpl(
      mojo::PendingAssociatedReceiver<mojom::blink::IDBCursor> receiver)
      : receiver_(this, std::move(receiver)) {
    receiver_.set_disconnect_handler(WTF::BindOnce(
        &MockCursorImpl::CursorDestroyed, WTF::Unretained(this)));
  }

  void Prefetch(int32_t count,
                mojom::blink::IDBCursor::PrefetchCallback callback) override {
    ++prefetch_calls_;
    last_prefetch_count_ = count;
    std::move(callback).Run(mojom::blink::IDBCursorResult::NewEmpty(true));
  }

  void PrefetchReset(int32_t used_prefetches) override {
    ++reset_calls_;
    last_used_count_ = used_prefetches;
  }

  void Advance(uint32_t count,
               mojom::blink::IDBCursor::AdvanceCallback callback) override {
    ++advance_calls_;
    std::move(callback).Run(mojom::blink::IDBCursorResult::NewEmpty(true));
  }

  void Continue(std::unique_ptr<IDBKey> key,
                std::unique_ptr<IDBKey> primary_key,
                mojom::blink::IDBCursor::ContinueCallback callback) override {
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

  mojo::AssociatedReceiver<mojom::blink::IDBCursor> receiver_;
};

}  // namespace

class IDBCursorTest : public testing::Test {
 public:
  IDBCursorTest() : null_key_(IDBKey::CreateNone()) {}

  void SetUpCursor(V8TestingScope& scope) {
    auto* execution_context = scope.GetExecutionContext();
    MockIDBDatabase mock_database;
    // Set up `transaction`.
    IDBDatabase* db = MakeGarbageCollected<IDBDatabase>(
        execution_context, mojo::NullAssociatedReceiver(), mojo::NullRemote(),
        mock_database.BindNewEndpointAndPassDedicatedRemote(), /*priority=*/0);

    MockIDBTransaction mock_transaction_remote;
    IDBTransaction::TransactionMojoRemote transaction_remote(execution_context);
    mojo::PendingAssociatedReceiver<mojom::blink::IDBTransaction> receiver =
        transaction_remote.BindNewEndpointAndPassReceiver(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting());
    receiver.EnableUnassociatedUsage();
    mock_transaction_remote.Bind(std::move(receiver));

    HashSet<String> transaction_scope = {"store"};
    IDBTransaction* transaction = IDBTransaction::CreateNonVersionChange(
        scope.GetScriptState(), std::move(transaction_remote), kTransactionId,
        transaction_scope, mojom::IDBTransactionMode::ReadOnly,
        mojom::IDBTransactionDurability::Relaxed, db);

    // Set up `store`.
    IDBKeyPath store_key_path("primaryKey");
    scoped_refptr<IDBObjectStoreMetadata> store_metadata = base::AdoptRef(
        new IDBObjectStoreMetadata("store", kStoreId, store_key_path, true, 1));
    IDBObjectStore* store =
        MakeGarbageCollected<IDBObjectStore>(store_metadata, transaction);

    // Create a `request` and a `source`.
    IDBRequest* request =
        IDBRequest::Create(scope.GetScriptState(), store, transaction,
                           IDBRequest::AsyncTraceState());
    IDBCursor::Source* source =
        MakeGarbageCollected<IDBCursor::Source>(
          request->source(scope.GetScriptState())->GetAsIDBObjectStore());

    // Set up mojo endpoints.
    mojo::AssociatedRemote<mojom::blink::IDBCursor> remote;
    mock_cursor_ = std::make_unique<MockCursorImpl>(
        remote.BindNewEndpointAndPassDedicatedReceiver());
    cursor_ = MakeGarbageCollected<IDBCursor>(
        remote.Unbind(), mojom::blink::IDBCursorDirection::NextNoDuplicate,
        request,
        source,
        transaction);
  }

  // Disallow copy and assign.
  IDBCursorTest(const IDBCursorTest&) = delete;
  IDBCursorTest& operator=(const IDBCursorTest&) = delete;

 protected:
  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
  std::unique_ptr<IDBKey> null_key_;
  Persistent<IDBCursor> cursor_;
  std::unique_ptr<MockCursorImpl> mock_cursor_;
};

TEST_F(IDBCursorTest, PrefetchTest) {
  V8TestingScope scope;
  SetUpCursor(scope);
  // Call continue() until prefetching should kick in.
  int continue_calls = 0;
  EXPECT_EQ(mock_cursor_->continue_calls(), 0);
  for (int i = 0; i < IDBCursor::kPrefetchContinueThreshold; ++i) {
    cursor_->CursorContinue(null_key_.get(), null_key_.get(), nullptr);
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
    cursor_->CursorContinue(null_key_.get(), null_key_.get(), nullptr);
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
      values.emplace_back(
          std::make_unique<IDBValue>(Vector<char>(), std::move(blob_info)));
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
      cursor_->CursorContinue(null_key_.get(), null_key_.get(), nullptr);
      platform_->RunUntilIdle();
      EXPECT_EQ(continue_calls, mock_cursor_->continue_calls());
      EXPECT_EQ(mock_cursor_->prefetch_calls(), repetitions + 1);
      EXPECT_EQ(cursor_->used_prefetches_, i + 1);
      EXPECT_EQ(static_cast<int>(cursor_->prefetch_keys_.size()),
                prefetch_count - cursor_->used_prefetches_);
    }
  }

  scope.GetExecutionContext()->NotifyContextDestroyed();
  platform_->RunUntilIdle();
  EXPECT_TRUE(mock_cursor_->destroyed());
}

TEST_F(IDBCursorTest, AdvancePrefetchTest) {
  V8TestingScope scope;
  SetUpCursor(scope);
  // Call continue() until prefetching should kick in.
  EXPECT_EQ(0, mock_cursor_->continue_calls());
  for (int i = 0; i < IDBCursor::kPrefetchContinueThreshold; ++i) {
    cursor_->CursorContinue(null_key_.get(), null_key_.get(), nullptr);
  }
  platform_->RunUntilIdle();
  EXPECT_EQ(0, mock_cursor_->prefetch_calls());

  // Initiate the prefetch
  cursor_->CursorContinue(null_key_.get(), null_key_.get(), nullptr);

  platform_->RunUntilIdle();
  EXPECT_EQ(1, mock_cursor_->prefetch_calls());
  EXPECT_EQ(static_cast<int>(IDBCursor::kPrefetchContinueThreshold),
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
    values.emplace_back(
        std::make_unique<IDBValue>(Vector<char>(), std::move(blob_info)));
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
  EXPECT_EQ(0, cursor_->prefetch_keys_.back()->Number());
  cursor_->CursorContinue(null_key_.get(), null_key_.get(), nullptr);
  platform_->RunUntilIdle();
  EXPECT_EQ(1, cursor_->prefetch_keys_.back()->Number());

  // IDBCursor.advance(1)
  cursor_->AdvanceImpl(1, nullptr);
  platform_->RunUntilIdle();
  EXPECT_EQ(2, cursor_->prefetch_keys_.back()->Number());

  // IDBCursor.continue()
  EXPECT_EQ(2, cursor_->prefetch_keys_.back()->Number());
  cursor_->CursorContinue(null_key_.get(), null_key_.get(), nullptr);
  platform_->RunUntilIdle();
  EXPECT_EQ(3, cursor_->prefetch_keys_.back()->Number());

  // IDBCursor.advance(2)
  cursor_->AdvanceImpl(2, nullptr);
  platform_->RunUntilIdle();
  EXPECT_EQ(0u, cursor_->prefetch_keys_.size());

  EXPECT_EQ(0, mock_cursor_->advance_calls());

  // IDBCursor.advance(lots) - beyond the fetched amount
  cursor_->AdvanceImpl(IDBCursor::kMaxPrefetchAmount, nullptr);
  platform_->RunUntilIdle();
  EXPECT_EQ(1, mock_cursor_->advance_calls());
  EXPECT_EQ(1, mock_cursor_->prefetch_calls());
  EXPECT_EQ(static_cast<int>(IDBCursor::kPrefetchContinueThreshold),
            mock_cursor_->continue_calls());

  scope.GetExecutionContext()->NotifyContextDestroyed();
  platform_->RunUntilIdle();
  EXPECT_TRUE(mock_cursor_->destroyed());
}

TEST_F(IDBCursorTest, PrefetchReset) {
  V8TestingScope scope;
  SetUpCursor(scope);
  // Call continue() until prefetching should kick in.
  int continue_calls = 0;
  EXPECT_EQ(mock_cursor_->continue_calls(), 0);
  for (int i = 0; i < IDBCursor::kPrefetchContinueThreshold; ++i) {
    cursor_->CursorContinue(null_key_.get(), null_key_.get(), nullptr);
    platform_->RunUntilIdle();
    EXPECT_EQ(++continue_calls, mock_cursor_->continue_calls());
    EXPECT_EQ(0, mock_cursor_->prefetch_calls());
  }

  // Initiate the prefetch
  cursor_->CursorContinue(null_key_.get(), null_key_.get(), nullptr);
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
    values.emplace_back(
        std::make_unique<IDBValue>(Vector<char>(), Vector<WebBlobInfo>()));
  }
  cursor_->SetPrefetchData(std::move(keys), std::move(primary_keys),
                           std::move(values));

  // No reset should have been sent since prefetch data hasn't been used.
  platform_->RunUntilIdle();
  EXPECT_EQ(0, mock_cursor_->reset_calls());

  // The real dispatcher would call cursor->CachedContinue(), so do that:
  cursor_->CachedContinue(nullptr);

  // Now the cursor should have reset the rest of the cache.
  platform_->RunUntilIdle();
  EXPECT_EQ(1, mock_cursor_->reset_calls());
  EXPECT_EQ(1, mock_cursor_->last_used_count());

  scope.GetExecutionContext()->NotifyContextDestroyed();
  platform_->RunUntilIdle();
  EXPECT_TRUE(mock_cursor_->destroyed());
}

}  // namespace blink
