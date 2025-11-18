// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mojo/mojo_binding_context.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/mojo/browser_interface_broker_proxy_impl.h"

namespace blink {

class MojoJSInterfaceBrokerWrapper final
    : public GarbageCollected<MojoJSInterfaceBrokerWrapper>,
      public GarbageCollectedMixin {
 public:
  explicit MojoJSInterfaceBrokerWrapper(MojoBindingContext& context)
      : impl_(&context) {}
  BrowserInterfaceBrokerProxyImpl& impl() { return impl_; }

  void Trace(Visitor* visitor) const override { visitor->Trace(impl_); }

 private:
  BrowserInterfaceBrokerProxyImpl impl_;
};

bool MojoBindingContext::ShouldUseMojoJSInterfaceBroker() const {
  return GetMojoJSInterfaceBrokerWrapper();
}

const BrowserInterfaceBrokerProxy&
MojoBindingContext::GetMojoJSInterfaceBroker() const {
  auto* wrapper = GetMojoJSInterfaceBrokerWrapper();
  CHECK(wrapper);
  return wrapper->impl();
}

void MojoBindingContext::SetMojoJSInterfaceBroker(
    mojo::PendingRemote<mojom::blink::BrowserInterfaceBroker> broker_remote) {
  auto* wrapper = GetMojoJSInterfaceBrokerWrapper();
  if (!wrapper) {
    wrapper = MakeGarbageCollected<MojoJSInterfaceBrokerWrapper>(*this);
    SetMojoJSInterfaceBrokerWrapper(wrapper);
  }
  wrapper->impl().Bind(std::move(broker_remote),
                       GetTaskRunner(TaskType::kInternalDefault));
}

void MojoBindingContext::Trace(Visitor* visitor) const {
  ContextLifecycleNotifier::Trace(visitor);
  visitor->Trace(mojo_jsinterface_broker_wrapper_);
  visitor->Trace(p2p_socket_dispatcher_);
}

}  // namespace blink
