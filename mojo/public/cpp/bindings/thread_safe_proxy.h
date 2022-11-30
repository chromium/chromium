// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_THREAD_SAFE_PROXY_H_
#define MOJO_PUBLIC_CPP_BINDINGS_THREAD_SAFE_PROXY_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {

// Generic API used to expose thread-safe interface access in various contexts.
class ThreadSafeProxy : public base::RefCountedThreadSafe<ThreadSafeProxy> {
 public:
  // Polymorphic helper to allow the proxy to retain an opaque back-reference to
  // the internal object which uses it.
  class Target : public base::RefCountedThreadSafe<Target> {
   protected:
    friend class base::RefCountedThreadSafe<Target>;
    virtual ~Target() = default;
  };

  virtual void SendMessage(Message& message) = 0;
  virtual void SendMessageWithResponder(
      Message& message,
      std::unique_ptr<MessageReceiver> responder) = 0;

 protected:
  friend class base::RefCountedThreadSafe<ThreadSafeProxy>;

  virtual ~ThreadSafeProxy() = default;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_THREAD_SAFE_PROXY_H_
