// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/mock_web_idb_callbacks.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_database_impl.h"

using testing::_;
using testing::Invoke;
using testing::StrictMock;
using testing::WithArgs;

namespace blink {

class WebIDBDatabaseImplTest : public testing::Test {};

TEST_F(WebIDBDatabaseImplTest, ValueSizeTest) {
  // For testing use a much smaller maximum size to prevent allocating >100 MB
  // of memory, which crashes on memory-constrained systems.
  const size_t kMaxValueSizeForTesting = 10 * 1024 * 1024;  // 10 MB

  const std::vector<char> data(kMaxValueSizeForTesting + 1);
  const WebData value(&data.front(), data.size());
  const Vector<WebBlobInfo> blob_info;
  const WebIDBKey key = WebIDBKey::CreateNumber(0);
  const int64_t transaction_id = 1;
  const int64_t object_store_id = 2;
  StrictMock<MockWebIDBCallbacks> callbacks;

  ASSERT_GT(value.size() + key.SizeEstimate(), kMaxValueSizeForTesting);
  ThreadState::Current()->CollectAllGarbage();
  EXPECT_CALL(callbacks, OnError(_)).Times(1);

  WebIDBDatabaseImpl database_impl(nullptr);
  database_impl.max_put_value_size_ = kMaxValueSizeForTesting;
  database_impl.Put(transaction_id, object_store_id, value, blob_info,
                    key.View(), blink::kWebIDBPutModeAddOrUpdate, &callbacks,
                    Vector<blink::WebIDBIndexKeys>());
}

TEST_F(WebIDBDatabaseImplTest, KeyAndValueSizeTest) {
  // For testing use a much smaller maximum size to prevent allocating >100 MB
  // of memory, which crashes on memory-constrained systems.
  const size_t kMaxValueSizeForTesting = 10 * 1024 * 1024;  // 10 MB
  const size_t kKeySize = 1024 * 1024;

  const std::vector<char> data(kMaxValueSizeForTesting - kKeySize);
  const WebData value(&data.front(), data.size());
  const Vector<WebBlobInfo> blob_info;
  const int64_t transaction_id = 1;
  const int64_t object_store_id = 2;
  StrictMock<MockWebIDBCallbacks> callbacks;

  // For this test, we want IDBKey::SizeEstimate() minus kKeySize to be the
  // smallest value > 0.  An IDBKey with a string has a size_estimate_ equal to
  // kOverheadSize (~16) + (string.length * sizeof(UChar)).  Create
  // |kKeySize / sizeof(UChar)| characters in String.
  const unsigned int number_of_chars = kKeySize / sizeof(UChar);
  Vector<UChar> key_string_vector;
  key_string_vector.ReserveInitialCapacity(number_of_chars);
  key_string_vector.Fill(u'0', number_of_chars);
  String key_string(key_string_vector);
  DCHECK_EQ(key_string.length(), number_of_chars);

  WebIDBKey key = WebIDBKey::CreateString(key_string);
  DCHECK_EQ(value.size(), kMaxValueSizeForTesting - kKeySize);
  DCHECK_GT(key.SizeEstimate() - kKeySize, static_cast<unsigned long>(0));
  DCHECK_GT(value.size() + key.SizeEstimate(), kMaxValueSizeForTesting);

  ThreadState::Current()->CollectAllGarbage();
  EXPECT_CALL(callbacks, OnError(_)).Times(1);

  WebIDBDatabaseImpl database_impl(nullptr);
  database_impl.max_put_value_size_ = kMaxValueSizeForTesting;
  database_impl.Put(transaction_id, object_store_id, value, blob_info,
                    key.View(), blink::kWebIDBPutModeAddOrUpdate, &callbacks,
                    Vector<blink::WebIDBIndexKeys>());
}

}  // namespace blink
