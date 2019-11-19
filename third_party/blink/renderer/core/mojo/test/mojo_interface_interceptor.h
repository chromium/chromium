// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_MOJO_TEST_MOJO_INTERFACE_INTERCEPTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_MOJO_TEST_MOJO_INTERFACE_INTERCEPTOR_H_

#include "base/util/type_safety/strong_alias.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace service_manager {
class InterfaceProvider;
}

namespace blink {

class ExceptionState;
class ExecutionContext;

// A MojoInterfaceInterceptor can be constructed by test scripts in order to
// intercept all outgoing requests for a specific named interface from the
// owning document, whether the requests come from other script or from native
// code (e.g. native API implementation code.) In production, such requests are
// normally routed to the browser to be bound to real interface implementations,
// but in test environments it's often useful to mock them out locally.
class MojoInterfaceInterceptor final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<MojoInterfaceInterceptor>,
      public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(MojoInterfaceInterceptor);

 public:
  static MojoInterfaceInterceptor* Create(ExecutionContext*,
                                          const String& interface_name,
                                          const String& scope,
                                          bool use_browser_interface_broker,
                                          ExceptionState&);

  using UseBrowserInterfaceBroker =
      util::StrongAlias<class UseBrowserInterfaceBrokerTag, bool>;
  MojoInterfaceInterceptor(
      ExecutionContext*,
      const String& interface_name,
      bool process_scope,
      UseBrowserInterfaceBroker use_browser_interface_broker);
  ~MojoInterfaceInterceptor() override;

  void start(ExceptionState&);
  void stop();

  DEFINE_ATTRIBUTE_EVENT_LISTENER(interfacerequest, kInterfacerequest)

  void Trace(blink::Visitor*) override;

  // EventTargetWithInlineData
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ActiveScriptWrappable
  bool HasPendingActivity() const final;

  // ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) final;

 private:
  service_manager::InterfaceProvider* GetInterfaceProvider() const;
  void OnInterfaceRequest(mojo::ScopedMessagePipeHandle);
  void DispatchInterfaceRequestEvent(mojo::ScopedMessagePipeHandle);

  const String interface_name_;
  bool started_ = false;
  bool process_scope_ = false;
  bool use_browser_interface_broker_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_MOJO_TEST_MOJO_INTERFACE_INTERCEPTOR_H_
