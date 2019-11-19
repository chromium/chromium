// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_TESTING_MOCK_STORAGE_AREA_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_TESTING_MOCK_STORAGE_AREA_H_

#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom-blink.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace blink {

// Mock StorageArea that records all read and write events.
class MockStorageArea : public mojom::blink::StorageArea {
 public:
  using ResultCallback = base::OnceCallback<void(bool)>;

  MockStorageArea();
  ~MockStorageArea() override;

  mojo::PendingRemote<mojom::blink::StorageArea> GetInterfaceRemote();
  mojo::PendingAssociatedRemote<mojom::blink::StorageArea>
  GetAssociatedInterfaceRemote();

  // StorageArea implementation:
  void AddObserver(
      mojo::PendingAssociatedRemote<mojom::blink::StorageAreaObserver> observer)
      override;

  void Put(const Vector<uint8_t>& key,
           const Vector<uint8_t>& value,
           const base::Optional<Vector<uint8_t>>& client_old_value,
           const String& source,
           PutCallback callback) override;

  void Delete(const Vector<uint8_t>& key,
              const base::Optional<Vector<uint8_t>>& client_old_value,
              const String& source,
              DeleteCallback callback) override;

  void DeleteAll(const String& source, DeleteAllCallback callback) override;

  void Get(const Vector<uint8_t>& key, GetCallback callback) override;

  void GetAll(mojo::PendingAssociatedRemote<
                  mojom::blink::StorageAreaGetAllCallback> complete_callback,
              GetAllCallback callback) override;

  // Methods and members for use by test fixtures.
  bool HasBindings() {
    return !receivers_.empty() || !associated_receivers_.empty();
  }

  void ResetObservations() {
    observed_get_all_ = false;
    observed_put_ = false;
    observed_delete_ = false;
    observed_delete_all_ = false;
    observed_key_.clear();
    observed_value_.clear();
    observed_source_ = String();
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

  void Flush() {
    receivers_.FlushForTesting();
    associated_receivers_.FlushForTesting();
  }

  void CloseAllBindings() {
    receivers_.Clear();
    associated_receivers_.Clear();
  }

  size_t pending_callbacks_count() const { return pending_callbacks_.size(); }

  bool observed_get_all() const { return observed_get_all_; }
  bool observed_put() const { return observed_put_; }
  bool observed_delete() const { return observed_delete_; }
  bool observed_delete_all() const { return observed_delete_all_; }
  const Vector<uint8_t>& observed_key() const { return observed_key_; }
  const Vector<uint8_t>& observed_value() const { return observed_value_; }
  const String& observed_source() const { return observed_source_; }
  size_t observer_count() const { return observer_count_; }

  Vector<mojom::blink::KeyValuePtr>& mutable_get_all_return_values() {
    return get_all_return_values_;
  }

 private:
  Deque<ResultCallback> pending_callbacks_;
  bool observed_get_all_ = false;
  bool observed_put_ = false;
  bool observed_delete_ = false;
  bool observed_delete_all_ = false;
  Vector<uint8_t> observed_key_;
  Vector<uint8_t> observed_value_;
  String observed_source_;
  size_t observer_count_ = 0;

  Vector<mojom::blink::KeyValuePtr> get_all_return_values_;

  mojo::ReceiverSet<mojom::blink::StorageArea> receivers_;
  mojo::AssociatedReceiverSet<mojom::blink::StorageArea> associated_receivers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_TESTING_MOCK_STORAGE_AREA_H_
