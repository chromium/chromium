// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mojo/mojo_binding_context.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/mojo/browser_interface_broker_proxy_impl.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

namespace {

class MojoJSInterfaceBrokerWrapper final
    : public GarbageCollected<MojoJSInterfaceBrokerWrapper>,
      public Supplement<MojoBindingContext> {
 public:
  static constexpr char kSupplementName[] = "MojoJSInterfaceBrokerWrapper";

  explicit MojoJSInterfaceBrokerWrapper(MojoBindingContext& context)
      : Supplement(context), impl_(&context) {}
  BrowserInterfaceBrokerProxyImpl& impl() { return impl_; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(impl_);
    Supplement::Trace(visitor);
  }

 private:
  BrowserInterfaceBrokerProxyImpl impl_;
};

}  // namespace

bool MojoBindingContext::ShouldUseMojoJSInterfaceBroker() const {
  return RequireSupplement<MojoJSInterfaceBrokerWrapper>();
}

const BrowserInterfaceBrokerProxy&
MojoBindingContext::GetMojoJSInterfaceBroker() const {
  auto* wrapper = RequireSupplement<MojoJSInterfaceBrokerWrapper>();
  CHECK(wrapper);
  return wrapper->impl();
}

void MojoBindingContext::SetMojoJSInterfaceBroker(
    mojo::PendingRemote<mojom::blink::BrowserInterfaceBroker> broker_remote) {
  auto* wrapper = RequireSupplement<MojoJSInterfaceBrokerWrapper>();
  if (!wrapper) {
    wrapper = MakeGarbageCollected<MojoJSInterfaceBrokerWrapper>(*this);
    ProvideSupplement(wrapper);
  }
  wrapper->impl().Bind(std::move(broker_remote),
                       GetTaskRunner(TaskType::kInternalDefault));
}

}  // namespace blink
