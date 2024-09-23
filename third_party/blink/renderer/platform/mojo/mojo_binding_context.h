// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_MOJO_BINDING_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_MOJO_BINDING_CONTEXT_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/renderer/platform/context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/mojo/browser_interface_broker_proxy_impl.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/gc_plugin.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {

class BrowserInterfaceBrokerProxy;

// This class encapsulates the necessary information for binding Mojo
// interfaces, to enable interfaces provided by the platform to be aware of the
// context in which they are intended to be used.
class PLATFORM_EXPORT MojoBindingContext
    : public ContextLifecycleNotifier,
      public Supplementable<MojoBindingContext> {
 public:
  MojoBindingContext() : mojo_js_interface_broker_(this) {}

  virtual const BrowserInterfaceBrokerProxy& GetBrowserInterfaceBroker()
      const = 0;
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      TaskType) = 0;

  bool use_mojo_js_interface_broker() { return use_mojo_js_interface_broker_; }

  // Returns a BrowserInterfaceBroker that should be used to handle JavaScript
  // Mojo.bindInterface calls. Returns nullptr if there isn't one.
  const BrowserInterfaceBrokerProxy& GetMojoJSInterfaceBroker() {
    return mojo_js_interface_broker_;
  }

  void SetMojoJSInterfaceBroker(
      mojo::PendingRemote<mojom::blink::BrowserInterfaceBroker> broker_remote) {
    use_mojo_js_interface_broker_ = true;
    mojo_js_interface_broker_.Bind(std::move(broker_remote),
                                   GetTaskRunner(TaskType::kInternalDefault));
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(mojo_js_interface_broker_);
    ContextLifecycleNotifier::Trace(visitor);
    Supplementable::Trace(visitor);
  }

 private:
  bool use_mojo_js_interface_broker_;
  BrowserInterfaceBrokerProxyImpl mojo_js_interface_broker_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_MOJO_BINDING_CONTEXT_H_
