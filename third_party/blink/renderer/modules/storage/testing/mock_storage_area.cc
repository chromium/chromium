// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/storage/testing/mock_storage_area.h"

#include "base/functional/bind.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

MockStorageArea::MockStorageArea() = default;
MockStorageArea::~MockStorageArea() = default;

mojo::PendingRemote<mojom::blink::StorageArea>
MockStorageArea::GetInterfaceRemote() {
  mojo::PendingRemote<mojom::blink::StorageArea> result;
  receivers_.Add(this, result.InitWithNewPipeAndPassReceiver());
  return result;
}

void MockStorageArea::InjectKeyValue(const Vector<uint8_t>& key,
                                     const Vector<uint8_t>& value) {
  key_values_.push_back(KeyValue{key, value});
}

void MockStorageArea::Clear() {
  key_values_.clear();
}

void MockStorageArea::AddObserver(
    mojo::PendingRemote<mojom::blink::StorageAreaObserver> observer) {
  ++observer_count_;
}

void MockStorageArea::Put(
    const Vector<uint8_t>& key,
    const Vector<uint8_t>& value,
    const std::optional<Vector<uint8_t>>& client_old_value,
    mojom::blink::StorageAreaSourcePtr source,
    PutCallback callback) {
  observed_puts_.push_back(ObservedPut(key, value, std::move(source)));
  std::move(callback).Run(true);
}

void MockStorageArea::Delete(
    const Vector<uint8_t>& key,
    const std::optional<Vector<uint8_t>>& client_old_value,
    mojom::blink::StorageAreaSourcePtr source,
    DeleteCallback callback) {
  observed_deletes_.push_back(ObservedDelete(key, std::move(source)));
  std::move(callback).Run();
}

void MockStorageArea::DeleteAll(
    mojom::blink::StorageAreaSourcePtr source,
    mojo::PendingRemote<mojom::blink::StorageAreaObserver> new_observer,
    DeleteAllCallback callback) {
  observed_delete_alls_.push_back(std::move(source));
  ++observer_count_;
  std::move(callback).Run();
}

void MockStorageArea::GetAll(
    mojo::PendingRemote<mojom::blink::StorageAreaObserver> new_observer,
    GetAllCallback callback) {
  ++observed_get_alls_;
  ++observer_count_;

  Vector<mojom::blink::KeyValuePtr> entries;
  for (const auto& entry : key_values_)
    entries.push_back(mojom::blink::KeyValue::New(entry.key, entry.value));
  std::move(callback).Run(std::move(entries));
}

}  // namespace blink
