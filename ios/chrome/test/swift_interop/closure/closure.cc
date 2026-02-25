// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/swift_interop/closure/closure.h"

#include "base/apple/swift_callback_helpers.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

class ClosureProviderImpl final
    : public ClosureProvider,
      public base::RefCountedThreadSafe<ClosureProviderImpl> {
 public:
  ClosureProviderImpl() = default;

  std::function<void()> GetRepeatingClosure() override;
  std::function<void()> GetOnceClosure() override;
  int call_count() override { return call_count_; }

  void Callback() { call_count_++; }

 private:
  friend class base::RefCountedThreadSafe<ClosureProviderImpl>;
  ~ClosureProviderImpl() override = default;

  int call_count_ = 0;

  base::WeakPtrFactory<ClosureProviderImpl> weak_ptr_factory_{this};
};

std::function<void()> ClosureProviderImpl::GetRepeatingClosure() {
  base::RepeatingClosure cb = base::BindRepeating(
      &ClosureProviderImpl::Callback, weak_ptr_factory_.GetWeakPtr());
  return base::swift_helpers::ToStdFunction(std::move(cb));
}

std::function<void()> ClosureProviderImpl::GetOnceClosure() {
  base::OnceClosure cb = base::BindOnce(&ClosureProviderImpl::Callback,
                                        weak_ptr_factory_.GetWeakPtr());
  return base::swift_helpers::ToStdFunction(std::move(cb));
}

ClosureProvider* ClosureProvider::MakeForSwift() {
  scoped_refptr<ClosureProviderImpl> ptr =
      base::MakeRefCounted<ClosureProviderImpl>();
  // The SWIFT_RETURNS_RETAINED annotation requires that the returned value is
  // passed with +1 ownership.
  Retain_ClosureProvider(ptr.get());
  // We intentionally leak the reference, which is adopted by swift.
  return ptr.release();
}

void Retain_ClosureProvider(ClosureProvider* provider) {
  if (provider) {
    static_cast<ClosureProviderImpl*>(provider)->AddRef();
  }
}

void Release_ClosureProvider(ClosureProvider* provider) {
  if (provider) {
    static_cast<ClosureProviderImpl*>(provider)->Release();
  }
}
