/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_MESSAGING_MESSAGE_PORT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_MESSAGING_MESSAGE_PORT_H_

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/connector.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

struct BlinkTransferableMessage;
class ExceptionState;
class ExecutionContext;
class PostMessageOptions;
class ScriptState;

class CORE_EXPORT MessagePort : public EventTargetWithInlineData,
                                public mojo::MessageReceiver,
                                public ActiveScriptWrappable<MessagePort>,
                                public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(MessagePort);

 public:
  explicit MessagePort(ExecutionContext&);
  ~MessagePort() override;

  void postMessage(ScriptState*,
                   const ScriptValue& message,
                   HeapVector<ScriptValue>& transfer,
                   ExceptionState&);
  void postMessage(ScriptState*,
                   const ScriptValue& message,
                   const PostMessageOptions*,
                   ExceptionState&);

  void start();
  void close();

  void Entangle(mojo::ScopedMessagePipeHandle);
  void Entangle(MessagePortChannel);
  MessagePortChannel Disentangle();

  // Returns an empty array if there is an exception, or if the passed array is
  // empty.
  static Vector<MessagePortChannel> DisentanglePorts(ExecutionContext*,
                                                     const MessagePortArray&,
                                                     ExceptionState&);

  // Returns an empty array if the passed array is empty.
  static MessagePortArray* EntanglePorts(ExecutionContext&,
                                         Vector<MessagePortChannel>);
  static MessagePortArray* EntanglePorts(ExecutionContext&,
                                         WebVector<MessagePortChannel>);

  bool Started() const { return started_; }

  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override {
    return ContextLifecycleObserver::GetExecutionContext();
  }
  MessagePort* ToMessagePort() override { return this; }

  // ScriptWrappable implementation.
  bool HasPendingActivity() const final;

  // ContextLifecycleObserver implementation.
  void ContextDestroyed(ExecutionContext*) override { close(); }

  void setOnmessage(EventListener* listener) {
    SetAttributeEventListener(event_type_names::kMessage, listener);
    start();
  }
  EventListener* onmessage() {
    return GetAttributeEventListener(event_type_names::kMessage);
  }

  void setOnmessageerror(EventListener* listener) {
    SetAttributeEventListener(event_type_names::kMessageerror, listener);
    start();
  }
  EventListener* onmessageerror() {
    return GetAttributeEventListener(event_type_names::kMessageerror);
  }

  // A port starts out its life entangled, and remains entangled until it is
  // closed or is cloned.
  bool IsEntangled() const { return !closed_ && !IsNeutered(); }

  // A port gets neutered when it is transferred to a new owner via
  // postMessage().
  bool IsNeutered() const { return !connector_ || !connector_->is_valid(); }

  // For testing only: allows inspection of the entangled channel.
  ::MojoHandle EntangledHandleForTesting() const;

  void Trace(blink::Visitor*) override;

 private:
  // mojo::MessageReceiver implementation.
  bool Accept(mojo::Message*) override;
  void ResetMessageCount();
  bool ShouldYieldAfterNewMessage();
  Event* CreateMessageEvent(BlinkTransferableMessage& message);

  std::unique_ptr<mojo::Connector> connector_;
  int messages_in_current_task_ = 0;

  bool started_ = false;
  bool closed_ = false;

  base::Optional<base::TimeTicks> task_start_time_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_MESSAGING_MESSAGE_PORT_H_
