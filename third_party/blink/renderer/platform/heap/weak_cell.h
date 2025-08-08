// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_WEAK_CELL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_WEAK_CELL_H_

#include "gin/weak_cell.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/heap/thread_state_storage.h"

namespace blink {

template <typename T>
using WeakCell = gin::WeakCell<T>;

// See `gin::WeakCellFactory` for documentation.
template <typename T>
class WeakCellFactory : public gin::WeakCellFactory<T> {
 public:
  using gin::WeakCellFactory<T>::WeakCellFactory;

  WeakCell<T>* GetWeakCell() {
    return gin::WeakCellFactory<T>::GetWeakCell(
        ThreadStateStorageFor<ThreadingTrait<T>::kAffinity>::GetState()
            ->allocation_handle());
  }
};

}  // namespace blink

namespace base {

template <typename T>
struct IsWeakReceiver<blink::UnwrappingCrossThreadHandle<blink::WeakCell<T>>>
    : std::true_type {};

template <typename T>
struct BindUnwrapTraits<
    blink::UnwrappingCrossThreadHandle<blink::WeakCell<T>>> {
  static T* Unwrap(
      const blink::UnwrappingCrossThreadHandle<blink::WeakCell<T>>& wrapped) {
    return wrapped.GetOnCreationThread()->Get();
  }
};

template <typename T>
struct MaybeValidTraits<
    blink::UnwrappingCrossThreadHandle<blink::WeakCell<T>>> {
  static constexpr bool MaybeValid(
      const blink::UnwrappingCrossThreadHandle<blink::WeakCell<T>>& p) {
    // Not necessarily called on `UnwrappingCrossThreadHandle<T>` and
    // `WeakCell<T>`'s owning thread, so the only possible implementation is to
    // assume the weak cell has not been invalidated.
    return true;
  }
};

}  // namespace base

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_WEAK_CELL_H_
