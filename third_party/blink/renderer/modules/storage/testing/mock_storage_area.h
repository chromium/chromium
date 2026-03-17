// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_TESTING_MOCK_STORAGE_AREA_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_TESTING_MOCK_STORAGE_AREA_H_

#include <utility>

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

  struct ObservedPut {
    Vector<uint8_t> key;
    Vector<uint8_t> value;
    mojom::blink::StorageAreaSourcePtr source;

    ObservedPut(Vector<uint8_t> key,
                Vector<uint8_t> value,
                mojom::blink::StorageAreaSourcePtr source)
        : key(std::move(key)),
          value(std::move(value)),
          source(std::move(source)) {}
    ObservedPut(const ObservedPut& other)
        : key(other.key), value(other.value), source(other.source.Clone()) {}
    ObservedPut(ObservedPut&&) = default;
    ObservedPut& operator=(const ObservedPut& other) {
      key = other.key;
      value = other.value;
      source = other.source.Clone();
      return *this;
    }
    ObservedPut& operator=(ObservedPut&&) = default;

    bool operator==(const ObservedPut& other) const {
      return key == other.key && value == other.value && source == other.source;
    }
  };

  struct ObservedDelete {
    Vector<uint8_t> key;
    mojom::blink::StorageAreaSourcePtr source;

    ObservedDelete(Vector<uint8_t> key,
                   mojom::blink::StorageAreaSourcePtr source)
        : key(std::move(key)), source(std::move(source)) {}
    ObservedDelete(const ObservedDelete& other)
        : key(other.key), source(other.source.Clone()) {}
    ObservedDelete(ObservedDelete&&) = default;
    ObservedDelete& operator=(const ObservedDelete& other) {
      key = other.key;
      source = other.source.Clone();
      return *this;
    }
    ObservedDelete& operator=(ObservedDelete&&) = default;

    bool operator==(const ObservedDelete& other) const {
      return key == other.key && source == other.source;
    }
  };

  struct KeyValue {
    Vector<uint8_t> key;
    Vector<uint8_t> value;
  };

  MockStorageArea();
  ~MockStorageArea() override;

  mojo::PendingRemote<mojom::blink::StorageArea> GetInterfaceRemote();

  void InjectKeyValue(const Vector<uint8_t>& key, const Vector<uint8_t>& value);
  void Clear();

  // StorageArea implementation:
  void AddObserver(
      mojo::PendingRemote<mojom::blink::StorageAreaObserver> observer) override;
  void Put(const Vector<uint8_t>& key,
           const Vector<uint8_t>& value,
           const std::optional<Vector<uint8_t>>& client_old_value,
           mojom::blink::StorageAreaSourcePtr source,
           PutCallback callback) override;
  void Delete(const Vector<uint8_t>& key,
              const std::optional<Vector<uint8_t>>& client_old_value,
              mojom::blink::StorageAreaSourcePtr source,
              DeleteCallback callback) override;
  void DeleteAll(
      mojom::blink::StorageAreaSourcePtr source,
      mojo::PendingRemote<mojom::blink::StorageAreaObserver> new_observer,
      DeleteAllCallback callback) override;
  void GetAll(
      mojo::PendingRemote<mojom::blink::StorageAreaObserver> new_observer,
      GetAllCallback callback) override;

  // Methods and members for use by test fixtures.
  bool HasBindings() { return !receivers_.empty(); }

  void ResetObservations() {
    observed_get_alls_ = 0;
    observed_puts_.clear();
    observed_deletes_.clear();
    observed_delete_alls_.clear();
  }

  void Flush() {
    receivers_.FlushForTesting();
  }

  void CloseAllBindings() {
    receivers_.Clear();
  }

  int observed_get_alls() const { return observed_get_alls_; }
  const Vector<ObservedPut>& observed_puts() const { return observed_puts_; }
  const Vector<ObservedDelete>& observed_deletes() const {
    return observed_deletes_;
  }

  const Vector<mojom::blink::StorageAreaSourcePtr>& observed_delete_alls()
      const {
    return observed_delete_alls_;
  }

  size_t observer_count() const { return observer_count_; }

 private:
  int observed_get_alls_ = 0;
  Vector<ObservedPut> observed_puts_;
  Vector<ObservedDelete> observed_deletes_;
  Vector<mojom::blink::StorageAreaSourcePtr> observed_delete_alls_;
  size_t observer_count_ = 0;

  Vector<KeyValue> key_values_;

  mojo::ReceiverSet<mojom::blink::StorageArea> receivers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_TESTING_MOCK_STORAGE_AREA_H_
