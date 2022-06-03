// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

    ~RefCount() {
      DCHECK(sequence_checker_.CalledOnValidSequence());
    }

    int32_t AddRef() {
      DCHECK(sequence_checker_.CalledOnValidSequence());
      return ++ref_;
    }

    int32_t Release() {
      DCHECK(sequence_checker_.CalledOnValidSequence());
      DCHECK(ref_ > 0);
      return --ref_;
    }

   private:
    int32_t ref_;
    base::SequenceChecker sequence_checker_;
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
