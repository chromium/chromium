// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PROXY_COMPLETION_CALLBACK_FACTORY_H_
#define PPAPI_PROXY_PROXY_COMPLETION_CALLBACK_FACTORY_H_

#include <stdint.h>

#include "base/check.h"
#include "base/sequence_checker.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace ppapi {
namespace proxy {

// This class is just like pp::NonThreadSafeThreadTraits but rather than using
// pp::Module::core (which doesn't exist), it uses Chrome threads which do.
class ProxyNonThreadSafeThreadTraits {
 public:
  class RefCount {
   public:
    RefCount() : ref_(0) {}

    ~RefCount() { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

    int32_t AddRef() {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return ++ref_;
    }

    int32_t Release() {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      DCHECK(ref_ > 0);
      return --ref_;
    }

   private:
    int32_t ref_;
    SEQUENCE_CHECKER(sequence_checker_);
  };

  // No-op lock class.
  class Lock {
   public:
    Lock() {}
    ~Lock() {}

    void Acquire() {}
    void Release() {}
  };

  // No-op AutoLock class.
  class AutoLock {
   public:
    explicit AutoLock(Lock&) {}
    ~AutoLock() {}
  };
};

template<typename T>
class ProxyCompletionCallbackFactory
    : public pp::CompletionCallbackFactory<T, ProxyNonThreadSafeThreadTraits> {
 public:
  ProxyCompletionCallbackFactory()
      : pp::CompletionCallbackFactory<T, ProxyNonThreadSafeThreadTraits>() {}
  ProxyCompletionCallbackFactory(T* t)
      : pp::CompletionCallbackFactory<T, ProxyNonThreadSafeThreadTraits>(t) {}
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PROXY_COMPLETION_CALLBACK_FACTORY_H_
