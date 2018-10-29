// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/dom_storage/dom_storage_cached_area.h"

#include <stdint.h>
#include <list>
#include <memory>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "content/renderer/dom_storage/dom_storage_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/fake_renderer_scheduler.h"

namespace content {

namespace {
// A mock implementation of the DOMStorageProxy interface.
class MockProxy : public DOMStorageProxy {
 public:
  MockProxy() {
    ResetObservations();
  }

  // DOMStorageProxy interface for use by DOMStorageCachedArea.

  void LoadArea(int connection_id,
                DOMStorageValuesMap* values,
                CompletionCallback callback) override {
    pending_callbacks_.push_back(std::move(callback));
    observed_load_area_ = true;
    observed_connection_id_ = connection_id;
    *values = load_area_return_values_;
  }

  void SetItem(int connection_id,
               const base::string16& key,
               const base::string16& value,
               const base::NullableString16& old_value,
               const GURL& page_url,
               CompletionCallback callback) override {
    pending_callbacks_.push_back(std::move(callback));
    observed_set_item_ = true;
    observed_connection_id_ = connection_id;
    observed_key_ = key;
    observed_value_ = value;
    observed_page_url_ = page_url;
  }

  void RemoveItem(int connection_id,
                  const base::string16& key,
                  const base::NullableString16& old_value,
                  const GURL& page_url,
                  CompletionCallback callback) override {
    pending_callbacks_.push_back(std::move(callback));
    observed_remove_item_ = true;
    observed_connection_id_ = connection_id;
    observed_key_ = key;
    observed_page_url_ = page_url;
  }

  void ClearArea(int connection_id,
                 const GURL& page_url,
                 CompletionCallback callback) override {
    pending_callbacks_.push_back(std::move(callback));
    observed_clear_area_ = true;
    observed_connection_id_ = connection_id;
    observed_page_url_ = page_url;
  }

  // Methods and members for use by test fixtures.

  void ResetObservations() {
    observed_load_area_ = false;
    observed_set_item_ = false;
    observed_remove_item_ = false;
    observed_clear_area_ = false;
    observed_connection_id_ = 0;
    observed_key_.clear();
    observed_value_.clear();
    observed_page_url_ = GURL();
  }

  void CompleteAllPendingCallbacks() {
    while (!pending_callbacks_.empty())
      CompleteOnePendingCallback(true);
  }

  void CompleteOnePendingCallback(bool success) {
    ASSERT_TRUE(!pending_callbacks_.empty());
    std::move(pending_callbacks_.front()).Run(success);
    pending_callbacks_.pop_front();
  }

  typedef std::list<CompletionCallback> CallbackList;

  DOMStorageValuesMap load_area_return_values_;
  CallbackList pending_callbacks_;
  bool observed_load_area_;
  bool observed_set_item_;
  bool observed_remove_item_;
  bool observed_clear_area_;
  int observed_connection_id_;
  base::string16 observed_key_;
  base::string16 observed_value_;
  GURL observed_page_url_;

 private:
  ~MockProxy() override {}
};

}  // namespace

class DOMStorageCachedAreaTest : public testing::Test {
 public:
  DOMStorageCachedAreaTest()
      : kNamespaceId("id"),
        kOrigin("http://dom_storage/"),
        kKey(base::ASCIIToUTF16("key")),
        kValue(base::ASCIIToUTF16("value")),
        kPageUrl("http://dom_storage/page") {}

  const std::string kNamespaceId;
  const GURL kOrigin;
  const base::string16 kKey;
  const base::string16 kValue;
  const GURL kPageUrl;

  void SetUp() override {
    main_thread_scheduler_ =
        std::make_unique<blink::scheduler::FakeRendererScheduler>();
    mock_proxy_ = new MockProxy();
  }

  bool IsPrimed(DOMStorageCachedArea* cached_area) {
    return cached_area->map_.get();
  }

  bool IsIgnoringAllMutations(DOMStorageCachedArea* cached_area) {
    return cached_area->ignore_all_mutations_;
  }

  bool IsIgnoringKeyMutations(DOMStorageCachedArea* cached_area,
                              const base::string16& key) {
    return cached_area->should_ignore_key_mutation(key);
  }

  void ResetAll(DOMStorageCachedArea* cached_area) {
    cached_area->Reset();
    mock_proxy_->ResetObservations();
    mock_proxy_->pending_callbacks_.clear();
  }

  void ResetCacheOnly(DOMStorageCachedArea* cached_area) {
    cached_area->Reset();
  }

 protected:
  base::test::ScopedTaskEnvironment
      task_environment_;  // Needed to construct a RendererScheduler.
  std::unique_ptr<blink::scheduler::WebThreadScheduler> main_thread_scheduler_;
  scoped_refptr<MockProxy> mock_proxy_;
};

TEST_F(DOMStorageCachedAreaTest, Basics) {
  EXPECT_TRUE(mock_proxy_->HasOneRef());
  scoped_refptr<DOMStorageCachedArea> cached_area = new DOMStorageCachedArea(
      kNamespaceId, kOrigin, mock_proxy_.get(), main_thread_scheduler_.get());
  EXPECT_EQ(kNamespaceId, cached_area->namespace_id());
  EXPECT_EQ(kOrigin, cached_area->origin());
  EXPECT_FALSE(mock_proxy_->HasOneRef());
  cached_area->ApplyMutation(base::NullableString16(kKey, false),
                             base::NullableString16(kValue, false));
  EXPECT_FALSE(IsPrimed(cached_area.get()));

  ResetAll(cached_area.get());
  EXPECT_EQ(kNamespaceId, cached_area->namespace_id());
  EXPECT_EQ(kOrigin, cached_area->origin());

  const int kConnectionId = 1;
  EXPECT_EQ(0u, cached_area->GetLength(kConnectionId));
  EXPECT_TRUE(cached_area->SetItem(kConnectionId, kKey, kValue, kPageUrl));
  EXPECT_EQ(1u, cached_area->GetLength(kConnectionId));
  EXPECT_EQ(kKey, cached_area->GetKey(kConnectionId, 0).string());
  EXPECT_EQ(kValue, cached_area->GetItem(kConnectionId, kKey).string());
  cached_area->RemoveItem(kConnectionId, kKey, kPageUrl);
  EXPECT_EQ(0u, cached_area->GetLength(kConnectionId));
}

TEST_F(DOMStorageCachedAreaTest, Getters) {
  const int kConnectionId = 7;
  scoped_refptr<DOMStorageCachedArea> cached_area = new DOMStorageCachedArea(
      kNamespaceId, kOrigin, mock_proxy_.get(), main_thread_scheduler_.get());

  // GetLength, we expect to see one call to load in the proxy.
  EXPECT_FALSE(IsPrimed(cached_area.get()));
  EXPECT_EQ(0u, cached_area->GetLength(kConnectionId));
  EXPECT_TRUE(IsPrimed(cached_area.get()));
  EXPECT_TRUE(mock_proxy_->observed_load_area_);
  EXPECT_EQ(kConnectionId, mock_proxy_->observed_connection_id_);
  EXPECT_EQ(1u, mock_proxy_->pending_callbacks_.size());
  EXPECT_TRUE(IsIgnoringAllMutations(cached_area.get()));
  mock_proxy_->CompleteAllPendingCallbacks();
  EXPECT_FALSE(IsIgnoringAllMutations(cached_area.get()));

  // GetKey, expect the one call to load.
  ResetAll(cached_area.get());
  EXPECT_FALSE(IsPrimed(cached_area.get()));
  EXPECT_TRUE(cached_area->GetKey(kConnectionId, 2).is_null());
  EXPECT_TRUE(IsPrimed(cached_area.get()));
  EXPECT_TRUE(mock_proxy_->observed_load_area_);
  EXPECT_EQ(kConnectionId, mock_proxy_->observed_connection_id_);
  EXPECT_EQ(1u, mock_proxy_->pending_callbacks_.size());

  // GetItem, ditto.
  ResetAll(cached_area.get());
  EXPECT_FALSE(IsPrimed(cached_area.get()));
  EXPECT_TRUE(cached_area->GetItem(kConnectionId, kKey).is_null());
  EXPECT_TRUE(IsPrimed(cached_area.get()));
  EXPECT_TRUE(mock_proxy_->observed_load_area_);
  EXPECT_EQ(kConnectionId, mock_proxy_->observed_connection_id_);
  EXPECT_EQ(1u, mock_proxy_->pending_callbacks_.size());
}

TEST_F(DOMStorageCachedAreaTest, Setters) {
  const int kConnectionId = 7;
  scoped_refptr<DOMStorageCachedArea> cached_area = new DOMStorageCachedArea(
      kNamespaceId, kOrigin, mock_proxy_.get(), main_thread_scheduler_.get());

  // SetItem, we expect a call to load followed by a call to set item
  // in the proxy.
  EXPECT_FALSE(IsPrimed(cached_area.get()));
  EXPECT_TRUE(cached_area->SetItem(kConnectionId, kKey, kValue, kPageUrl));
  EXPECT_TRUE(IsPrimed(cached_area.get()));
  EXPECT_TRUE(mock_proxy_->observed_load_area_);
  EXPECT_TRUE(mock_proxy_->observed_set_item_);
  EXPECT_EQ(kConnectionId, mock_proxy_->observed_connection_id_);
  EXPECT_EQ(kPageUrl, mock_proxy_->observed_page_url_);
  EXPECT_EQ(kKey, mock_proxy_->observed_key_);
  EXPECT_EQ(kValue, mock_proxy_->observed_value_);
  EXPECT_EQ(2u, mock_proxy_->pending_callbacks_.size());

  // Clear, we expect a just the one call to clear in the proxy since
  // there's no need to load the data prior to deleting it.
  ResetAll(cached_area.get());
  EXPECT_FALSE(IsPrimed(cached_area.get()));
  cached_area->Clear(kConnectionId, kPageUrl);
  EXPECT_TRUE(IsPrimed(cached_area.get()));
  EXPECT_TRUE(mock_proxy_->observed_clear_area_);
  EXPECT_EQ(kConnectionId, mock_proxy_->observed_connection_id_);
  EXPECT_EQ(kPageUrl, mock_proxy_->observed_page_url_);
  EXPECT_EQ(1u, mock_proxy_->pending_callbacks_.size());

  // RemoveItem with nothing to remove, expect just one call to load.
  ResetAll(cached_area.get());
  EXPECT_FALSE(IsPrimed(cached_area.get()));
  cached_area->RemoveItem(kConnectionId, kKey, kPageUrl);
  EXPECT_TRUE(IsPrimed(cached_area.get()));
  EXPECT_TRUE(mock_proxy_->observed_load_area_);
  EXPECT_FALSE(mock_proxy_->observed_remove_item_);
  EXPECT_EQ(kConnectionId, mock_proxy_->observed_connection_id_);
  EXPECT_EQ(1u, mock_proxy_->pending_callbacks_.size());

  // RemoveItem with something to remove, expect a call to load followed
  // by a call to remove.
  ResetAll(cached_area.get());
  mock_proxy_->load_area_return_values_[kKey] =
      base::NullableString16(kValue, false);
  EXPECT_FALSE(IsPrimed(cached_area.get()));
  cached_area->RemoveItem(kConnectionId, kKey, kPageUrl);
  EXPECT_TRUE(IsPrimed(cached_area.get()));
  EXPECT_TRUE(mock_proxy_->observed_load_area_);
  EXPECT_TRUE(mock_proxy_->observed_remove_item_);
  EXPECT_EQ(kConnectionId, mock_proxy_->observed_connection_id_);
  EXPECT_EQ(kPageUrl, mock_proxy_->observed_page_url_);
  EXPECT_EQ(kKey, mock_proxy_->observed_key_);
  EXPECT_EQ(2u, mock_proxy_->pending_callbacks_.size());
}

TEST_F(DOMStorageCachedAreaTest, MutationsAreIgnoredUntilLoadCompletion) {
  const int kConnectionId = 7;
  scoped_refptr<DOMStorageCachedArea> cached_area = new DOMStorageCachedArea(
      kNamespaceId, kOrigin, mock_proxy_.get(), main_thread_scheduler_.get());
  EXPECT_TRUE(cached_area->GetItem(kConnectionId, kKey).is_null());
  EXPECT_TRUE(IsPrimed(cached_area.get()));
  EXPECT_TRUE(IsIgnoringAllMutations(cached_area.get()));

  // Before load completion, the mutation should be ignored.
  cached_area->ApplyMutation(base::NullableString16(kKey, false),
                             base::NullableString16(kValue, false));
  EXPECT_TRUE(cached_area->GetItem(kConnectionId, kKey).is_null());

  // Call the load completion callback.
  mock_proxy_->CompleteOnePendingCallback(true);
  EXPECT_FALSE(IsIgnoringAllMutations(cached_area.get()));

  // Verify that mutations are now applied.
  cached_area->ApplyMutation(base::NullableString16(kKey, false),
                             base::NullableString16(kValue, false));
  EXPECT_EQ(kValue, cached_area->GetItem(kConnectionId, kKey).string());
}

TEST_F(DOMStorageCachedAreaTest, MutationsAreIgnoredUntilClearCompletion) {
  const int kConnectionId = 4;
  scoped_refptr<DOMStorageCachedArea> cached_area = new DOMStorageCachedArea(
      kNamespaceId, kOrigin, mock_proxy_.get(), main_thread_scheduler_.get());
  cached_area->Clear(kConnectionId, kPageUrl);
  EXPECT_TRUE(IsIgnoringAllMutations(cached_area.get()));
  mock_proxy_->CompleteOnePendingCallback(true);
  EXPECT_FALSE(IsIgnoringAllMutations(cached_area.get()));

  // Verify that calling Clear twice works as expected, the first
  // completion callback should have been cancelled.
  ResetCacheOnly(cached_area.get());
  cached_area->Clear(kConnectionId, kPageUrl);
  EXPECT_TRUE(IsIgnoringAllMutations(cached_area.get()));
  cached_area->Clear(kConnectionId, kPageUrl);
  EXPECT_TRUE(IsIgnoringAllMutations(cached_area.get()));
  mock_proxy_->CompleteOnePendingCallback(true);
  EXPECT_TRUE(IsIgnoringAllMutations(cached_area.get()));
  mock_proxy_->CompleteOnePendingCallback(true);
  EXPECT_FALSE(IsIgnoringAllMutations(cached_area.get()));
}

TEST_F(DOMStorageCachedAreaTest, KeyMutationsAreIgnoredUntilCompletion) {
  const int kConnectionId = 8;
  scoped_refptr<DOMStorageCachedArea> cached_area = new DOMStorageCachedArea(
      kNamespaceId, kOrigin, mock_proxy_.get(), main_thread_scheduler_.get());

  // SetItem
  EXPECT_TRUE(cached_area->SetItem(kConnectionId, kKey, kValue, kPageUrl));
  mock_proxy_->CompleteOnePendingCallback(true);  // load completion
  EXPECT_FALSE(IsIgnoringAllMutations(cached_area.get()));
  EXPECT_TRUE(IsIgnoringKeyMutations(cached_area.get(), kKey));
  cached_area->ApplyMutation(base::NullableString16(kKey, false),
                             base::NullableString16());
  EXPECT_EQ(kValue, cached_area->GetItem(kConnectionId, kKey).string());
  mock_proxy_->CompleteOnePendingCallback(true);  // set completion
  EXPECT_FALSE(IsIgnoringKeyMutations(cached_area.get(), kKey));

  // RemoveItem
  cached_area->RemoveItem(kConnectionId, kKey, kPageUrl);
  EXPECT_TRUE(IsIgnoringKeyMutations(cached_area.get(), kKey));
  mock_proxy_->CompleteOnePendingCallback(true);  // remove completion
  EXPECT_FALSE(IsIgnoringKeyMutations(cached_area.get(), kKey));

  // Multiple mutations to the same key.
  EXPECT_TRUE(cached_area->SetItem(kConnectionId, kKey, kValue, kPageUrl));
  cached_area->RemoveItem(kConnectionId, kKey, kPageUrl);
  EXPECT_TRUE(IsIgnoringKeyMutations(cached_area.get(), kKey));
  mock_proxy_->CompleteOnePendingCallback(true);  // set completion
  EXPECT_TRUE(IsIgnoringKeyMutations(cached_area.get(), kKey));
  mock_proxy_->CompleteOnePendingCallback(true);  // remove completion
  EXPECT_FALSE(IsIgnoringKeyMutations(cached_area.get(), kKey));

  // A failed set item operation should Reset the cache.
  EXPECT_TRUE(cached_area->SetItem(kConnectionId, kKey, kValue, kPageUrl));
  EXPECT_TRUE(IsIgnoringKeyMutations(cached_area.get(), kKey));
  mock_proxy_->CompleteOnePendingCallback(false);
  EXPECT_FALSE(IsPrimed(cached_area.get()));
}

}  // namespace content
