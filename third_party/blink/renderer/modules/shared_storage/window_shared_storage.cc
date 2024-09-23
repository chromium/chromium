// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/window_shared_storage.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/shared_storage/shared_storage.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

namespace {

class WindowSharedStorageImpl final
    : public GarbageCollected<WindowSharedStorageImpl>,
      public Supplement<LocalDOMWindow> {
 public:
  static const char kSupplementName[];

  static WindowSharedStorageImpl& From(LocalDOMWindow& window) {
    WindowSharedStorageImpl* supplement =
        Supplement<LocalDOMWindow>::template From<WindowSharedStorageImpl>(
            window);
    if (!supplement) {
      supplement = MakeGarbageCollected<WindowSharedStorageImpl>(window);
      Supplement<LocalDOMWindow>::ProvideTo(window, supplement);
    }
    return *supplement;
  }

  explicit WindowSharedStorageImpl(LocalDOMWindow& window)
      : Supplement<LocalDOMWindow>(window) {}

  SharedStorage* GetOrCreate(LocalDOMWindow& fetching_scope) {
    if (!shared_storage_)
      shared_storage_ = MakeGarbageCollected<SharedStorage>();
    return shared_storage_.Get();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(shared_storage_);
    Supplement<LocalDOMWindow>::Trace(visitor);
  }

 private:
  Member<SharedStorage> shared_storage_;
};

// static
const char WindowSharedStorageImpl::kSupplementName[] =
    "WindowSharedStorageImpl";

}  // namespace

SharedStorage* WindowSharedStorage::sharedStorage(
    LocalDOMWindow& window,
    ExceptionState& exception_state) {
  return WindowSharedStorageImpl::From(window).GetOrCreate(window);
}

}  // namespace blink
