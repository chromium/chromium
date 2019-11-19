/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBSOCKETS_DOM_WEBSOCKET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBSOCKETS_DOM_WEBSOCKET_H_

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_state_observer.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/websockets/websocket_channel.h"
#include "third_party/blink/renderer/modules/websockets/websocket_channel_client.h"
#include "third_party/blink/renderer/modules/websockets/websocket_channel_impl.h"
#include "third_party/blink/renderer/modules/websockets/websocket_common.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Blob;
class DOMArrayBuffer;
class DOMArrayBufferView;
class ExceptionState;
class ExecutionContext;
class StringOrStringSequence;

class MODULES_EXPORT DOMWebSocket : public EventTargetWithInlineData,
                                    public ActiveScriptWrappable<DOMWebSocket>,
                                    public ContextLifecycleStateObserver,
                                    public WebSocketChannelClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(DOMWebSocket);

 public:
  // These definitions are required by V8DOMWebSocket.
  static constexpr auto kConnecting = WebSocketCommon::kConnecting;
  static constexpr auto kOpen = WebSocketCommon::kOpen;
  static constexpr auto kClosing = WebSocketCommon::kClosing;
  static constexpr auto kClosed = WebSocketCommon::kClosed;

  // DOMWebSocket instances must be used with a wrapper since this class's
  // lifetime management is designed assuming the V8 holds a ref on it while
  // hasPendingActivity() returns true.
  static DOMWebSocket* Create(ExecutionContext*,
                              const String& url,
                              ExceptionState&);
  static DOMWebSocket* Create(ExecutionContext*,
                              const String& url,
                              const StringOrStringSequence& protocols,
                              ExceptionState&);

  explicit DOMWebSocket(ExecutionContext*);
  ~DOMWebSocket() override;

  void Connect(const String& url,
               const Vector<String>& protocols,
               ExceptionState&);

  void send(const String& message, ExceptionState&);
  void send(DOMArrayBuffer*, ExceptionState&);
  void send(NotShared<DOMArrayBufferView>, ExceptionState&);
  void send(Blob*, ExceptionState&);

  // To distinguish close method call with the code parameter from one
  // without, we have these three signatures. Use of
  // Optional=DefaultIsUndefined in the IDL file doesn't help for now since
  // it's bound to a value of 0 which is indistinguishable from the case 0
  // is passed as code parameter.
  void close(uint16_t code, const String& reason, ExceptionState&);
  void close(ExceptionState&);
  void close(uint16_t code, ExceptionState&);

  const KURL& url() const;
  WebSocketCommon::State readyState() const;
  uint64_t bufferedAmount() const;

  String protocol() const;
  String extensions() const;

  String binaryType() const;
  void setBinaryType(const String&);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(open, kOpen)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(message, kMessage)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(error, kError)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(close, kClose)

  // EventTarget functions.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ContextLifecycleStateObserver functions.
  void ContextDestroyed(ExecutionContext*) override;
  void ContextLifecycleStateChanged(mojom::FrameLifecycleState) override;

  // ScriptWrappable functions.
  // Prevent this instance from being collected while it's not in CLOSED
  // state.
  bool HasPendingActivity() const final;

  // WebSocketChannelClient functions.
  void DidConnect(const String& subprotocol, const String& extensions) override;
  void DidReceiveTextMessage(const String& message) override;
  void DidReceiveBinaryMessage(
      const Vector<base::span<const char>>& data) override;
  void DidError() override;
  void DidConsumeBufferedAmount(uint64_t) override;
  void DidStartClosingHandshake() override;
  void DidClose(ClosingHandshakeCompletionStatus,
                uint16_t code,
                const String& reason) override;

  void Trace(blink::Visitor*) override;

 private:
  // FIXME: This should inherit blink::EventQueue.
  class EventQueue final : public GarbageCollected<EventQueue> {
   public:
    static EventQueue* Create(EventTarget* target) {
      return MakeGarbageCollected<EventQueue>(target);
    }

    explicit EventQueue(EventTarget*);
    ~EventQueue();

    // Dispatches the event if this queue is active.
    // Queues the event if this queue is suspended.
    // Does nothing otherwise.
    void Dispatch(Event* /* event */);

    bool IsEmpty() const;

    void Pause();
    void Unpause();
    void ContextDestroyed();

    bool IsPaused();

    void Trace(blink::Visitor*);

   private:
    enum State {
      kActive,
      kPaused,
      kUnpausePosted,
      kStopped,
    };

    // Dispatches queued events if this queue is active.
    // Does nothing otherwise.
    void DispatchQueuedEvents();
    void UnpauseTask();

    State state_;
    Member<EventTarget> target_;
    HeapDeque<Member<Event>> events_;
  };

  enum WebSocketSendType {
    kWebSocketSendTypeString,
    kWebSocketSendTypeArrayBuffer,
    kWebSocketSendTypeArrayBufferView,
    kWebSocketSendTypeBlob,
    kWebSocketSendTypeMax,
  };

  enum WebSocketReceiveType {
    kWebSocketReceiveTypeString,
    kWebSocketReceiveTypeArrayBuffer,
    kWebSocketReceiveTypeBlob,
    kWebSocketReceiveTypeMax,
  };

  enum BinaryType { kBinaryTypeBlob, kBinaryTypeArrayBuffer };

  // This function is virtual for unittests.
  virtual WebSocketChannel* CreateChannel(ExecutionContext* context,
                                          WebSocketChannelClient* client) {
    return WebSocketChannelImpl::Create(context, client,
                                        SourceLocation::Capture(context));
  }

  // Adds a console message with JSMessageSource and ErrorMessageLevel.
  void LogError(const String& message);

  // Handle the JavaScript close method call. close() methods on this class
  // are just for determining if the optional code argument is supplied or
  // not.
  void CloseInternal(int, const String&, ExceptionState&);

  // Updates |buffered_amount_after_close_| given the amount of data passed to
  // send() method after the state changed to CLOSING or CLOSED.
  void UpdateBufferedAmountAfterClose(uint64_t);

  // Causes |buffered_amount_| to be updated asynchronously after returning to
  // the event loop. Uses |buffered_amount_update_task_pending_| to avoid
  // posting multiple tasks simultaneously.
  void PostBufferedAmountUpdateTask();

  // Updates |buffered_amount_| and resets
  // |buffered_amount_update_task_pending_|.
  void BufferedAmountUpdateTask();

  // Updates |buffered_amount_| provided the object is not currently paused.
  void ReflectBufferedAmountConsumption();

  void ReleaseChannel();
  void RecordSendTypeHistogram(WebSocketSendType);
  void RecordSendMessageSizeHistogram(WebSocketSendType, size_t);
  void RecordReceiveTypeHistogram(WebSocketReceiveType);
  void RecordReceiveMessageSizeHistogram(WebSocketReceiveType, size_t);

  Member<WebSocketChannel> channel_;

  WebSocketCommon common_;

  String origin_string_;

  uint64_t buffered_amount_;
  // The consumed buffered amount that will be reflected to |buffered_amount_|
  // later. It will be cleared once reflected.
  uint64_t consumed_buffered_amount_;
  uint64_t buffered_amount_after_close_;
  BinaryType binary_type_;
  // The subprotocol the server selected.
  String subprotocol_;
  String extensions_;

  Member<EventQueue> event_queue_;

  bool buffered_amount_update_task_pending_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBSOCKETS_DOM_WEBSOCKET_H_
