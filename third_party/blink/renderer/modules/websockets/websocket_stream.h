// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBSOCKETS_WEBSOCKET_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBSOCKETS_WEBSOCKET_STREAM_H_

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/websockets/websocket_channel_client.h"
#include "third_party/blink/renderer/modules/websockets/websocket_common.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;
class ScriptValue;
class Visitor;
class WebSocketChannel;
class WebSocketCloseInfo;
class WebSocketStreamOptions;

// Implements of JavaScript-exposed WebSocketStream API. See design doc at
// https://docs.google.com/document/d/1XuxEshh5VYBYm1qRVKordTamCOsR-uGQBCYFcHXP4L0/edit

class MODULES_EXPORT WebSocketStream final
    : public ScriptWrappable,
      public ActiveScriptWrappable<WebSocketStream>,
      public ContextLifecycleObserver,
      public WebSocketChannelClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(WebSocketStream);

 public:
  // IDL constructors
  static WebSocketStream* Create(ScriptState*,
                                 const String& url,
                                 WebSocketStreamOptions*,
                                 ExceptionState&);
  static WebSocketStream* CreateForTesting(ScriptState*,
                                           const String& url,
                                           WebSocketStreamOptions*,
                                           WebSocketChannel*,
                                           ExceptionState&);

  // The constructor only creates the object. It cannot fail. The private
  // Connect() method is used by Create() to start connecting.
  WebSocketStream(ExecutionContext*, ScriptState*);
  ~WebSocketStream() override;

  // IDL properties
  const KURL& url() const { return common_.Url(); }
  ScriptPromise connection(ScriptState*) const;
  ScriptPromise closed(ScriptState*) const;

  // IDL functions
  void close(WebSocketCloseInfo*, ExceptionState&);

  // Implementation of WebSocketChannelClient
  void DidConnect(const String& subprotocol, const String& extensions) override;
  void DidReceiveTextMessage(const String&) override;
  void DidReceiveBinaryMessage(
      const Vector<base::span<const char>>& data) override;
  void DidError() override;
  void DidConsumeBufferedAmount(uint64_t consumed) override;
  void DidStartClosingHandshake() override;
  void DidClose(ClosingHandshakeCompletionStatus,
                uint16_t /* code */,
                const String& /* reason */) override;

  // Implementation of ContextLifecycleObserver.
  void ContextDestroyed(ExecutionContext*) override;

  // Implementation of ActiveScriptWrappable.
  bool HasPendingActivity() const override;

  void Trace(Visitor*) override;

 private:
  class UnderlyingSource;
  class UnderlyingSink;

  static WebSocketStream* CreateInternal(ScriptState*,
                                         const String& url,
                                         WebSocketStreamOptions*,
                                         WebSocketChannel*,
                                         ExceptionState&);
  void Connect(ScriptState*,
               const String& url,
               WebSocketStreamOptions*,
               ExceptionState&);

  // Closes the connection. If |maybe_reason| is an object with a valid "code"
  // property and optionally a valid "reason" property, will use them as the
  // code and reason, otherwise will close with unspecified close.
  void CloseMaybeWithReason(ScriptValue maybe_reason);

  void CloseWithUnspecifiedCode();
  void CloseInternal(int code,
                     const String& reason,
                     ExceptionState& exception_state);
  void OnAbort();

  v8::Local<v8::Value> CreateNetworkErrorDOMException();
  static WebSocketCloseInfo* MakeCloseInfo(uint16_t code, const String& reason);

  const Member<ScriptState> script_state_;
  const Member<ScriptPromiseResolver> connection_resolver_;
  const Member<ScriptPromiseResolver> closed_resolver_;

  // These need to be cached because the Promise() method on
  // ScriptPromiseResolver doesn't work any more once the promise is resolved or
  // rejected.
  const TraceWrapperV8Reference<v8::Promise> connection_;
  const TraceWrapperV8Reference<v8::Promise> closed_;

  Member<WebSocketChannel> channel_;

  Member<UnderlyingSource> source_;
  Member<UnderlyingSink> sink_;

  WebSocketCommon common_;

  // We need to distinguish between "closing during handshake" and "closing
  // after handshake" in order to reject the |connection_resolver_| correctly.
  bool was_ever_connected_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBSOCKETS_WEBSOCKET_STREAM_H_
