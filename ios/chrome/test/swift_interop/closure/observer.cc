// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/swift_interop/include/observer.h"

SWIFT_DEFINE_INTEROP_WRAPPER(ValueDidChangeCallback,
                             base::RepeatingCallback<void(int)>)
SWIFT_DEFINE_REF_COUNTED_HELPERS(ValueObserver)

ValueObserver::ValueObserver() = default;

ValueObserver::~ValueObserver() = default;

ValueDidChangeCallback ValueObserver::GetValueDidChangeCallback() {
  return base::BindRepeating(&ValueObserver::ValueDidChange,
                             weak_ptr_factory_.GetWeakPtr());
}

void ValueObserver::ValueDidChange(int value) {
  value_ = value;
}

ValueObserver* ValueObserver::makeForSwift() {
  // We inentionally leak the reference, which is adopted by swift.
  return base::MakeRefCounted<ValueObserver>().release();
}
