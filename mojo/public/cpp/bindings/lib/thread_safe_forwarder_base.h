// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_THREAD_SAFE_FORWARDER_BASE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_THREAD_SAFE_FORWARDER_BASE_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/thread_safe_proxy.h"

namespace mojo {
namespace internal {

// This class defines out-of-line logic common to the behavior of
// ThreadSafeForwarder<Interface>, which is in turn used to support the
// implementation of SharedRemote<Interface>.
//
// This object is sequence-affine and it provides an opaque interface to an
// underlying weakly-referenced interface proxy (e.g. a Remote) which may be
// bound on a different sequence and referenced weakly by any number of other
// ThreadSafeForwarders.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) ThreadSafeForwarderBase
    : public MessageReceiverWithResponder {
 public:
  // Constructs a new ThreadSafeForwarderBase which forwards requests through
  // an InterfaceEndpointClient's ThreadSafeProxy.
  explicit ThreadSafeForwarderBase(scoped_refptr<ThreadSafeProxy> proxy);
  ThreadSafeForwarderBase(const ThreadSafeForwarderBase&) = delete;
  ThreadSafeForwarderBase& operator=(const ThreadSafeForwarderBase&) = delete;
  ~ThreadSafeForwarderBase() override;

  // MessageReceiverWithResponder implementation:
  bool PrefersSerializedMessages() override;
  bool Accept(Message* message) override;
  bool AcceptWithResponder(Message* message,
                           std::unique_ptr<MessageReceiver> responder) override;

 private:
  const scoped_refptr<ThreadSafeProxy> proxy_;
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_THREAD_SAFE_FORWARDER_BASE_H_
