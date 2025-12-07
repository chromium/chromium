// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_MOJO_BINDING_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_MOJO_BINDING_CONTEXT_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/forward_declared_member.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace mojo {
template <typename Interface>
class PendingRemote;
}

namespace blink {

class BrowserInterfaceBrokerProxy;
class MojoJSInterfaceBrokerWrapper;
class P2PSocketDispatcher;

namespace mojom::blink {
class BrowserInterfaceBroker;
}

// This class encapsulates the necessary information for binding Mojo
// interfaces, to enable interfaces provided by the platform to be aware of the
// context in which they are intended to be used.
class PLATFORM_EXPORT MojoBindingContext : public ContextLifecycleNotifier {
 public:
  virtual const BrowserInterfaceBrokerProxy& GetBrowserInterfaceBroker()
      const = 0;
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      TaskType) = 0;

  bool ShouldUseMojoJSInterfaceBroker() const;

  // Returns a BrowserInterfaceBroker that should be used to handle JavaScript
  // Mojo.bindInterface calls.
  const BrowserInterfaceBrokerProxy& GetMojoJSInterfaceBroker() const;

  void SetMojoJSInterfaceBroker(
      mojo::PendingRemote<mojom::blink::BrowserInterfaceBroker> broker_remote);

  void Trace(Visitor* visitor) const override;
  MojoJSInterfaceBrokerWrapper* GetMojoJSInterfaceBrokerWrapper() const {
    return mojo_jsinterface_broker_wrapper_;
  }
  void SetMojoJSInterfaceBrokerWrapper(
      MojoJSInterfaceBrokerWrapper* mojo_jsinterface_broker_wrapper) {
    mojo_jsinterface_broker_wrapper_ = mojo_jsinterface_broker_wrapper;
  }

  ForwardDeclaredMember<P2PSocketDispatcher> GetP2PSocketDispatcher() const {
    return p2p_socket_dispatcher_;
  }
  void SetP2PSocketDispatcher(
      ForwardDeclaredMember<P2PSocketDispatcher> p2p_socket_dispatcher) {
    p2p_socket_dispatcher_ = p2p_socket_dispatcher;
  }

 private:
  Member<MojoJSInterfaceBrokerWrapper> mojo_jsinterface_broker_wrapper_;
  ForwardDeclaredMember<P2PSocketDispatcher> p2p_socket_dispatcher_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_MOJO_BINDING_CONTEXT_H_
