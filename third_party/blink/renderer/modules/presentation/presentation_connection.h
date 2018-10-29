// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PRESENTATION_PRESENTATION_CONNECTION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PRESENTATION_PRESENTATION_CONNECTION_H_

#include <memory>
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace WTF {
class AtomicString;
}  // namespace WTF

namespace blink {

class DOMArrayBuffer;
class DOMArrayBufferView;
class PresentationController;
class PresentationReceiver;
class PresentationRequest;
class WebString;

class PresentationConnection : public EventTargetWithInlineData,
                               public ContextLifecycleObserver,
                               public mojom::blink::PresentationConnection {
  USING_GARBAGE_COLLECTED_MIXIN(PresentationConnection);
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~PresentationConnection() override;

  // EventTarget implementation.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  void Trace(blink::Visitor*) override;

  const String& id() const { return id_; }
  const String& url() const { return url_; }
  const WTF::AtomicString& state() const;

  void send(const String& message, ExceptionState&);
  void send(DOMArrayBuffer*, ExceptionState&);
  void send(NotShared<DOMArrayBufferView>, ExceptionState&);
  void send(Blob*, ExceptionState&);

  // Closes the connection to the ongoing presentation.
  void close();

  // Terminates the ongoing presentation that this PresentationConnection is
  // connected to.
  void terminate();

  String binaryType() const;
  void setBinaryType(const String&);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(message);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(connect);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(close);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(terminate);

  // Returns true if this connection's id equals to |id| and its url equals to
  // |url|.
  bool Matches(const String& id, const KURL&) const;

  // Notifies the connection about its state change to 'closed'.
  void DidClose(mojom::blink::PresentationConnectionCloseReason,
                const String& message);

  // mojom::blink::PresentationConnection implementation.
  void OnMessage(mojom::blink::PresentationConnectionMessagePtr) override;
  void DidChangeState(mojom::blink::PresentationConnectionState) override;
  void DidClose(mojom::blink::PresentationConnectionCloseReason) override;

  mojom::blink::PresentationConnectionState GetState() const;

 protected:
  static void DispatchEventAsync(EventTarget*, Event*);

  PresentationConnection(LocalFrame&, const String& id, const KURL&);

  // EventTarget implementation.
  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) override;

  // ContextLifecycleObserver implementation.
  void ContextDestroyed(ExecutionContext*) override;

  String id_;
  KURL url_;
  mojom::blink::PresentationConnectionState state_;

  mojo::Binding<mojom::blink::PresentationConnection> connection_binding_;

  // The other end of a PresentationConnection. For controller connections, this
  // can point to the browser (2-UA) or another renderer (1-UA). For receiver
  // connections, this currently only points to another renderer. This ptr can
  // be used to send messages directly to the other end.
  mojom::blink::PresentationConnectionPtr target_connection_;

 private:
  class BlobLoader;

  enum MessageType {
    kMessageTypeText,
    kMessageTypeArrayBuffer,
    kMessageTypeBlob,
  };

  enum BinaryType { kBinaryTypeBlob, kBinaryTypeArrayBuffer };

  class Message;

  // Implemented by controller/receiver subclasses to perform additional
  // operations when close() / terminate() is called.
  virtual void CloseInternal() = 0;
  virtual void TerminateInternal() = 0;

  bool CanSendMessage(ExceptionState&);
  void HandleMessageQueue();

  // Callbacks invoked from BlobLoader.
  void DidFinishLoadingBlob(DOMArrayBuffer*);
  void DidFailLoadingBlob(FileError::ErrorCode);

  void SendMessageToTargetConnection(
      mojom::blink::PresentationConnectionMessagePtr);
  void DidReceiveTextMessage(const WebString&);
  void DidReceiveBinaryMessage(const uint8_t*, uint32_t length);

  // Closes the PresentationConnection with the given reason and notifies the
  // target connection.
  void DoClose(mojom::blink::PresentationConnectionCloseReason);

  // Internal helper function to dispatch state change events asynchronously.
  void DispatchStateChangeEvent(Event*);

  // Cancel loads and pending messages when the connection is closed.
  void TearDown();

  // For Blob data handling.
  Member<BlobLoader> blob_loader_;
  HeapDeque<Member<Message>> messages_;

  BinaryType binary_type_;
};

// Represents the controller side of a connection of either a 1-UA or 2-UA
// presentation.
class ControllerPresentationConnection final : public PresentationConnection {
 public:
  // For CallbackPromiseAdapter.
  static ControllerPresentationConnection* Take(
      ScriptPromiseResolver*,
      const mojom::blink::PresentationInfo&,
      PresentationRequest*);
  static ControllerPresentationConnection* Take(
      PresentationController*,
      const mojom::blink::PresentationInfo&,
      PresentationRequest*);

  ControllerPresentationConnection(LocalFrame&,
                                   PresentationController*,
                                   const String& id,
                                   const KURL&);
  ~ControllerPresentationConnection() override;

  void Trace(blink::Visitor*) override;

  // Initializes Mojo message pipes and registers with the PresentationService.
  void Init(mojom::blink::PresentationConnectionPtr connection_ptr,
            mojom::blink::PresentationConnectionRequest connection_request);

 private:
  // PresentationConnection implementation.
  void CloseInternal() override;
  void TerminateInternal() override;

  Member<PresentationController> controller_;
};

// Represents the receiver side connection of a 1-UA presentation. Instances of
// this class are created as a result of
// PresentationReceiver::OnReceiverConnectionAvailable, which in turn is a
// result of creating the controller side connection of a 1-UA presentation.
class ReceiverPresentationConnection final : public PresentationConnection {
 public:
  static ReceiverPresentationConnection* Take(
      PresentationReceiver*,
      const mojom::blink::PresentationInfo&,
      mojom::blink::PresentationConnectionPtr controller_connection,
      mojom::blink::PresentationConnectionRequest receiver_connection_request);

  ReceiverPresentationConnection(LocalFrame&,
                                 PresentationReceiver*,
                                 const String& id,
                                 const KURL&);
  ~ReceiverPresentationConnection() override;

  void Trace(blink::Visitor*) override;

  void Init(
      mojom::blink::PresentationConnectionPtr controller_connection_ptr,
      mojom::blink::PresentationConnectionRequest receiver_connection_request);

  // PresentationConnection override
  void DidChangeState(mojom::blink::PresentationConnectionState) override;
  void DidClose(mojom::blink::PresentationConnectionCloseReason) override;

 private:
  // PresentationConnection implementation.
  void CloseInternal() override;

  // Changes the presentation state to TERMINATED and notifies the sender
  // connection. This method does not dispatch a state change event to the page.
  // This method is only suitable for use when the presentation receiver frame
  // containing the connection object is going away.
  void TerminateInternal() override;

  Member<PresentationReceiver> receiver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PRESENTATION_PRESENTATION_CONNECTION_H_
