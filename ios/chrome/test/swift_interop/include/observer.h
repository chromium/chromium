// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_SWIFT_INTEROP_INCLUDE_OBSERVER_H_
#define IOS_CHROME_TEST_SWIFT_INTEROP_INCLUDE_OBSERVER_H_

#include "base/apple/swift_interop_util.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"

SWIFT_DECLARE_INTEROP_WRAPPER(ValueDidChangeCallback,
                              base::RepeatingCallback<void(int)>)

SWIFT_DECLARE_REF_COUNTED_HELPERS(ValueObserver)

class ValueObserver final : public base::RefCounted<ValueObserver> {
 public:
  ValueObserver();
  ValueDidChangeCallback GetValueDidChangeCallback();
  int value() { return value_; }
  static ValueObserver* makeForSwift() SWIFT_RETURNS_RETAINED;
  void ValueDidChange(int value);

 private:
  friend class base::RefCounted<ValueObserver>;

  ~ValueObserver();
  int value_ = 0;
  base::WeakPtrFactory<ValueObserver> weak_ptr_factory_{this};
} SWIFT_REF_COUNTED(ValueObserver);

#endif  // IOS_CHROME_TEST_SWIFT_INTEROP_INCLUDE_OBSERVER_H_
