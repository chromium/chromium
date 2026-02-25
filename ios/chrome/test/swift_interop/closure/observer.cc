// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/swift_interop/closure/observer.h"

#include "base/apple/swift_callback_helpers.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

class ValueObserverImpl final
    : public ValueObserver,
      public base::RefCountedThreadSafe<ValueObserverImpl> {
 public:
  ValueObserverImpl() = default;

  ValueDidChangeCallback GetValueDidChangeCallback() override;
  void ValueDidChange(int value) override { value_ = value; }
  int value() override { return value_; }

 private:
  friend class base::RefCountedThreadSafe<ValueObserverImpl>;
  ~ValueObserverImpl() override = default;

  int value_ = 0;

  base::WeakPtrFactory<ValueObserverImpl> weak_ptr_factory_{this};
};

ValueObserverImpl::ValueDidChangeCallback
ValueObserverImpl::GetValueDidChangeCallback() {
  base::RepeatingCallback<void(int)> cb = base::BindRepeating(
      &ValueObserverImpl::ValueDidChange, weak_ptr_factory_.GetWeakPtr());
  return base::swift_helpers::ToStdFunction(std::move(cb));
}

ValueObserver* ValueObserver::MakeForSwift() {
  scoped_refptr<ValueObserverImpl> ptr =
      base::MakeRefCounted<ValueObserverImpl>();
  // The SWIFT_RETURNS_RETAINED annotation requires that the returned value is
  // passed with +1 ownership.
  Retain_ValueObserver(ptr.get());
  // We intentionally leak the reference, which is adopted by swift.
  return ptr.release();
}

void Retain_ValueObserver(ValueObserver* provider) {
  if (provider) {
    static_cast<ValueObserverImpl*>(provider)->AddRef();
  }
}

void Release_ValueObserver(ValueObserver* provider) {
  if (provider) {
    static_cast<ValueObserverImpl*>(provider)->Release();
  }
}
