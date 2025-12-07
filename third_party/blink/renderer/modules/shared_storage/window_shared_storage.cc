// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/window_shared_storage.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/shared_storage/shared_storage.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class WindowSharedStorageImpl final
    : public GarbageCollected<WindowSharedStorageImpl>,
      public GarbageCollectedMixin {
 public:
  static WindowSharedStorageImpl& From(LocalDOMWindow& window) {
    WindowSharedStorageImpl* supplement = window.GetWindowSharedStorageImpl();
    if (!supplement) {
      supplement = MakeGarbageCollected<WindowSharedStorageImpl>();
      window.SetWindowSharedStorageImpl(supplement);
    }
    return *supplement;
  }

  WindowSharedStorageImpl() = default;

  SharedStorage* GetOrCreate(LocalDOMWindow& fetching_scope) {
    if (!shared_storage_)
      shared_storage_ = MakeGarbageCollected<SharedStorage>();
    return shared_storage_.Get();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(shared_storage_);
  }

 private:
  Member<SharedStorage> shared_storage_;
};

SharedStorage* WindowSharedStorage::sharedStorage(
    LocalDOMWindow& window,
    ExceptionState& exception_state) {
  return WindowSharedStorageImpl::From(window).GetOrCreate(window);
}

}  // namespace blink
