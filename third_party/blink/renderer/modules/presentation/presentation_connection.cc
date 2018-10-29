// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation_connection.h"

#include <memory>
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader_client.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/presentation/presentation.h"
#include "third_party/blink/renderer/modules/presentation/presentation_connection_available_event.h"
#include "third_party/blink/renderer/modules/presentation/presentation_connection_close_event.h"
#include "third_party/blink/renderer/modules/presentation/presentation_controller.h"
#include "third_party/blink/renderer/modules/presentation/presentation_receiver.h"
#include "third_party/blink/renderer/modules/presentation/presentation_request.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

namespace {

mojom::blink::PresentationConnectionMessagePtr MakeBinaryMessage(
    const DOMArrayBuffer* buffer) {
  // Mutating the data field on the message instead of passing in an already
  // populated Vector into message constructor is more efficient since the
  // latter does not support moves.
  auto message = mojom::blink::PresentationConnectionMessage::NewData(
      WTF::Vector<uint8_t>());
  WTF::Vector<uint8_t>& data = message->get_data();
  data.Append(static_cast<const uint8_t*>(buffer->Data()),
              buffer->ByteLength());
  return message;
}

mojom::blink::PresentationConnectionMessagePtr MakeTextMessage(
    const String& text) {
  return mojom::blink::PresentationConnectionMessage::NewMessage(text);
}

const AtomicString& ConnectionStateToString(
    mojom::blink::PresentationConnectionState state) {
  DEFINE_STATIC_LOCAL(const AtomicString, connecting_value, ("connecting"));
  DEFINE_STATIC_LOCAL(const AtomicString, connected_value, ("connected"));
  DEFINE_STATIC_LOCAL(const AtomicString, closed_value, ("closed"));
  DEFINE_STATIC_LOCAL(const AtomicString, terminated_value, ("terminated"));

  switch (state) {
    case mojom::blink::PresentationConnectionState::CONNECTING:
      return connecting_value;
    case mojom::blink::PresentationConnectionState::CONNECTED:
      return connected_value;
    case mojom::blink::PresentationConnectionState::CLOSED:
      return closed_value;
    case mojom::blink::PresentationConnectionState::TERMINATED:
      return terminated_value;
  }

  NOTREACHED();
  return terminated_value;
}

const AtomicString& ConnectionCloseReasonToString(
    mojom::blink::PresentationConnectionCloseReason reason) {
  DEFINE_STATIC_LOCAL(const AtomicString, error_value, ("error"));
  DEFINE_STATIC_LOCAL(const AtomicString, closed_value, ("closed"));
  DEFINE_STATIC_LOCAL(const AtomicString, went_away_value, ("wentaway"));

  switch (reason) {
    case mojom::blink::PresentationConnectionCloseReason::CONNECTION_ERROR:
      return error_value;
    case mojom::blink::PresentationConnectionCloseReason::CLOSED:
      return closed_value;
    case mojom::blink::PresentationConnectionCloseReason::WENT_AWAY:
      return went_away_value;
  }

  NOTREACHED();
  return error_value;
}

void ThrowPresentationDisconnectedError(ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                    "Presentation connection is disconnected.");
}

}  // namespace

class PresentationConnection::Message final
    : public GarbageCollectedFinalized<PresentationConnection::Message> {
 public:
  Message(const String& text) : type(kMessageTypeText), text(text) {}

  Message(DOMArrayBuffer* array_buffer)
      : type(kMessageTypeArrayBuffer), array_buffer(array_buffer) {}

  Message(scoped_refptr<BlobDataHandle> blob_data_handle)
      : type(kMessageTypeBlob), blob_data_handle(std::move(blob_data_handle)) {}

  void Trace(blink::Visitor* visitor) { visitor->Trace(array_buffer); }

  MessageType type;
  String text;
  Member<DOMArrayBuffer> array_buffer;
  scoped_refptr<BlobDataHandle> blob_data_handle;
};

class PresentationConnection::BlobLoader final
    : public GarbageCollectedFinalized<PresentationConnection::BlobLoader>,
      public FileReaderLoaderClient {
 public:
  BlobLoader(scoped_refptr<BlobDataHandle> blob_data_handle,
             PresentationConnection* presentation_connection)
      : presentation_connection_(presentation_connection),
        loader_(FileReaderLoader::Create(FileReaderLoader::kReadAsArrayBuffer,
                                         this)) {
    loader_->Start(std::move(blob_data_handle));
  }
  ~BlobLoader() override = default;

  // FileReaderLoaderClient functions.
  void DidStartLoading() override {}
  void DidReceiveData() override {}
  void DidFinishLoading() override {
    presentation_connection_->DidFinishLoadingBlob(
        loader_->ArrayBufferResult());
  }
  void DidFail(FileError::ErrorCode error_code) override {
    presentation_connection_->DidFailLoadingBlob(error_code);
  }

  void Cancel() { loader_->Cancel(); }

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(presentation_connection_);
  }

 private:
  Member<PresentationConnection> presentation_connection_;
  std::unique_ptr<FileReaderLoader> loader_;
};

PresentationConnection::PresentationConnection(LocalFrame& frame,
                                               const String& id,
                                               const KURL& url)
    : ContextLifecycleObserver(frame.GetDocument()),
      id_(id),
      url_(url),
      state_(mojom::blink::PresentationConnectionState::CONNECTING),
      connection_binding_(this),
      binary_type_(kBinaryTypeArrayBuffer) {}

PresentationConnection::~PresentationConnection() {
  DCHECK(!blob_loader_);
}

void PresentationConnection::OnMessage(
    mojom::blink::PresentationConnectionMessagePtr message) {
  if (message->is_data()) {
    const auto& data = message->get_data();
    DidReceiveBinaryMessage(&data.front(), data.size());
  } else {
    DidReceiveTextMessage(message->get_message());
  }
}

void PresentationConnection::DidChangeState(
    mojom::blink::PresentationConnectionState state) {
  // Closed state is handled in |DidClose()|.
  DCHECK_NE(mojom::blink::PresentationConnectionState::CLOSED, state);

  if (state_ == state)
    return;

  state_ = state;

  switch (state_) {
    case mojom::blink::PresentationConnectionState::CONNECTING:
      return;
    case mojom::blink::PresentationConnectionState::CONNECTED:
      DispatchStateChangeEvent(Event::Create(EventTypeNames::connect));
      return;
    case mojom::blink::PresentationConnectionState::CLOSED:
      return;
    case mojom::blink::PresentationConnectionState::TERMINATED:
      DispatchStateChangeEvent(Event::Create(EventTypeNames::terminate));
      return;
  }
  NOTREACHED();
}

void PresentationConnection::DidClose(
    mojom::blink::PresentationConnectionCloseReason reason) {
  DidClose(reason, /* message */ String());
}

// static
ControllerPresentationConnection* ControllerPresentationConnection::Take(
    ScriptPromiseResolver* resolver,
    const mojom::blink::PresentationInfo& presentation_info,
    PresentationRequest* request) {
  DCHECK(resolver);
  DCHECK(request);

  Document* document = To<Document>(resolver->GetExecutionContext());
  if (!document->GetFrame())
    return nullptr;

  PresentationController* controller =
      PresentationController::From(*document->GetFrame());
  if (!controller)
    return nullptr;

  return Take(controller, presentation_info, request);
}

// static
ControllerPresentationConnection* ControllerPresentationConnection::Take(
    PresentationController* controller,
    const mojom::blink::PresentationInfo& presentation_info,
    PresentationRequest* request) {
  DCHECK(controller);
  DCHECK(request);

  auto* connection = new ControllerPresentationConnection(
      *controller->GetFrame(), controller, presentation_info.id,
      presentation_info.url);
  controller->RegisterConnection(connection);

  // Fire onconnectionavailable event asynchronously.
  auto* event = PresentationConnectionAvailableEvent::Create(
      EventTypeNames::connectionavailable, connection);
  request->GetExecutionContext()
      ->GetTaskRunner(TaskType::kPresentation)
      ->PostTask(FROM_HERE,
                 WTF::Bind(&PresentationConnection::DispatchEventAsync,
                           WrapPersistent(request), WrapPersistent(event)));

  return connection;
}

ControllerPresentationConnection::ControllerPresentationConnection(
    LocalFrame& frame,
    PresentationController* controller,
    const String& id,
    const KURL& url)
    : PresentationConnection(frame, id, url), controller_(controller) {}

ControllerPresentationConnection::~ControllerPresentationConnection() {}

void ControllerPresentationConnection::Trace(blink::Visitor* visitor) {
  visitor->Trace(controller_);
  PresentationConnection::Trace(visitor);
}

void ControllerPresentationConnection::Init(
    mojom::blink::PresentationConnectionPtr connection_ptr,
    mojom::blink::PresentationConnectionRequest connection_request) {
  // Note that it is possible for the binding to be already bound here, because
  // the ControllerPresentationConnection object could be reused when
  // reconnecting in the same frame. In this case the existing connections are
  // discarded.
  if (connection_binding_.is_bound()) {
    connection_binding_.Close();
    target_connection_.reset();
  }

  DidChangeState(mojom::blink::PresentationConnectionState::CONNECTING);
  target_connection_ = std::move(connection_ptr);
  connection_binding_.Bind(std::move(connection_request));
}

void ControllerPresentationConnection::CloseInternal() {
  auto& service = controller_->GetPresentationService();
  if (service)
    service->CloseConnection(url_, id_);
}

void ControllerPresentationConnection::TerminateInternal() {
  auto& service = controller_->GetPresentationService();
  if (service)
    service->Terminate(url_, id_);
}

// static
ReceiverPresentationConnection* ReceiverPresentationConnection::Take(
    PresentationReceiver* receiver,
    const mojom::blink::PresentationInfo& presentation_info,
    mojom::blink::PresentationConnectionPtr controller_connection,
    mojom::blink::PresentationConnectionRequest receiver_connection_request) {
  DCHECK(receiver);

  ReceiverPresentationConnection* connection =
      new ReceiverPresentationConnection(*receiver->GetFrame(), receiver,
                                         presentation_info.id,
                                         presentation_info.url);
  connection->Init(std::move(controller_connection),
                   std::move(receiver_connection_request));

  receiver->RegisterConnection(connection);

  return connection;
}

ReceiverPresentationConnection::ReceiverPresentationConnection(
    LocalFrame& frame,
    PresentationReceiver* receiver,
    const String& id,
    const KURL& url)
    : PresentationConnection(frame, id, url), receiver_(receiver) {}

ReceiverPresentationConnection::~ReceiverPresentationConnection() = default;

void ReceiverPresentationConnection::Init(
    mojom::blink::PresentationConnectionPtr controller_connection_ptr,
    mojom::blink::PresentationConnectionRequest receiver_connection_request) {
  target_connection_ = std::move(controller_connection_ptr);
  connection_binding_.Bind(std::move(receiver_connection_request));

  target_connection_->DidChangeState(
      mojom::blink::PresentationConnectionState::CONNECTED);
  DidChangeState(mojom::blink::PresentationConnectionState::CONNECTED);
}

void ReceiverPresentationConnection::DidChangeState(
    mojom::blink::PresentationConnectionState state) {
  PresentationConnection::DidChangeState(state);
}

void ReceiverPresentationConnection::DidClose(
    mojom::blink::PresentationConnectionCloseReason reason) {
  PresentationConnection::DidClose(reason);
  receiver_->RemoveConnection(this);
}

void ReceiverPresentationConnection::CloseInternal() {
  // No-op
}

void ReceiverPresentationConnection::TerminateInternal() {
  // This will close the receiver window. Change the state to TERMINATED now
  // since ReceiverPresentationConnection won't get a state change notification.
  if (state_ == mojom::blink::PresentationConnectionState::TERMINATED)
    return;

  receiver_->Terminate();

  state_ = mojom::blink::PresentationConnectionState::TERMINATED;
  if (target_connection_)
    target_connection_->DidChangeState(state_);
}

void ReceiverPresentationConnection::Trace(blink::Visitor* visitor) {
  visitor->Trace(receiver_);
  PresentationConnection::Trace(visitor);
}

const AtomicString& PresentationConnection::InterfaceName() const {
  return EventTargetNames::PresentationConnection;
}

ExecutionContext* PresentationConnection::GetExecutionContext() const {
  if (!GetFrame())
    return nullptr;
  return GetFrame()->GetDocument();
}

void PresentationConnection::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::AddedEventListener(event_type,
                                                registered_listener);
  if (event_type == EventTypeNames::connect) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kPresentationConnectionConnectEventListener);
  } else if (event_type == EventTypeNames::close) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kPresentationConnectionCloseEventListener);
  } else if (event_type == EventTypeNames::terminate) {
    UseCounter::Count(
        GetExecutionContext(),
        WebFeature::kPresentationConnectionTerminateEventListener);
  } else if (event_type == EventTypeNames::message) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kPresentationConnectionMessageEventListener);
  }
}

void PresentationConnection::ContextDestroyed(ExecutionContext*) {
  DoClose(mojom::blink::PresentationConnectionCloseReason::WENT_AWAY);
  target_connection_.reset();
  connection_binding_.Close();
}

void PresentationConnection::Trace(blink::Visitor* visitor) {
  visitor->Trace(blob_loader_);
  visitor->Trace(messages_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

const AtomicString& PresentationConnection::state() const {
  return ConnectionStateToString(state_);
}

void PresentationConnection::send(const String& message,
                                  ExceptionState& exception_state) {
  if (!CanSendMessage(exception_state))
    return;

  messages_.push_back(new Message(message));
  HandleMessageQueue();
}

void PresentationConnection::send(DOMArrayBuffer* array_buffer,
                                  ExceptionState& exception_state) {
  DCHECK(array_buffer);
  DCHECK(array_buffer->Buffer());
  if (!CanSendMessage(exception_state))
    return;

  messages_.push_back(new Message(array_buffer));
  HandleMessageQueue();
}

void PresentationConnection::send(
    NotShared<DOMArrayBufferView> array_buffer_view,
    ExceptionState& exception_state) {
  DCHECK(array_buffer_view);
  if (!CanSendMessage(exception_state))
    return;

  messages_.push_back(new Message(array_buffer_view.View()->buffer()));
  HandleMessageQueue();
}

void PresentationConnection::send(Blob* data, ExceptionState& exception_state) {
  DCHECK(data);
  if (!CanSendMessage(exception_state))
    return;

  messages_.push_back(new Message(data->GetBlobDataHandle()));
  HandleMessageQueue();
}

void PresentationConnection::DoClose(
    mojom::blink::PresentationConnectionCloseReason reason) {
  if (state_ != mojom::blink::PresentationConnectionState::CONNECTING &&
      state_ != mojom::blink::PresentationConnectionState::CONNECTED) {
    return;
  }

  if (target_connection_)
    target_connection_->DidClose(reason);

  DidClose(reason);
  CloseInternal();
  TearDown();
}

bool PresentationConnection::CanSendMessage(ExceptionState& exception_state) {
  if (state_ != mojom::blink::PresentationConnectionState::CONNECTED) {
    ThrowPresentationDisconnectedError(exception_state);
    return false;
  }

  return !!target_connection_;
}

void PresentationConnection::HandleMessageQueue() {
  if (!target_connection_)
    return;

  while (!messages_.IsEmpty() && !blob_loader_) {
    Message* message = messages_.front().Get();
    switch (message->type) {
      case kMessageTypeText:
        SendMessageToTargetConnection(MakeTextMessage(message->text));
        messages_.pop_front();
        break;
      case kMessageTypeArrayBuffer:
        SendMessageToTargetConnection(MakeBinaryMessage(message->array_buffer));
        messages_.pop_front();
        break;
      case kMessageTypeBlob:
        DCHECK(!blob_loader_);
        blob_loader_ = new BlobLoader(message->blob_data_handle, this);
        break;
    }
  }
}

String PresentationConnection::binaryType() const {
  switch (binary_type_) {
    case kBinaryTypeBlob:
      return "blob";
    case kBinaryTypeArrayBuffer:
      return "arraybuffer";
  }
  NOTREACHED();
  return String();
}

void PresentationConnection::setBinaryType(const String& binary_type) {
  if (binary_type == "blob") {
    binary_type_ = kBinaryTypeBlob;
    return;
  }
  if (binary_type == "arraybuffer") {
    binary_type_ = kBinaryTypeArrayBuffer;
    return;
  }
  NOTREACHED();
}

void PresentationConnection::SendMessageToTargetConnection(
    mojom::blink::PresentationConnectionMessagePtr message) {
  if (target_connection_)
    target_connection_->OnMessage(std::move(message));
}

void PresentationConnection::DidReceiveTextMessage(const WebString& message) {
  if (state_ != mojom::blink::PresentationConnectionState::CONNECTED)
    return;

  DispatchEvent(*MessageEvent::Create(message));
}

void PresentationConnection::DidReceiveBinaryMessage(const uint8_t* data,
                                                     uint32_t length) {
  if (state_ != mojom::blink::PresentationConnectionState::CONNECTED)
    return;

  switch (binary_type_) {
    case kBinaryTypeBlob: {
      std::unique_ptr<BlobData> blob_data = BlobData::Create();
      blob_data->AppendBytes(data, length);
      Blob* blob =
          Blob::Create(BlobDataHandle::Create(std::move(blob_data), length));
      DispatchEvent(*MessageEvent::Create(blob));
      return;
    }
    case kBinaryTypeArrayBuffer:
      DOMArrayBuffer* buffer = DOMArrayBuffer::Create(data, length);
      DispatchEvent(*MessageEvent::Create(buffer));
      return;
  }
  NOTREACHED();
}

mojom::blink::PresentationConnectionState PresentationConnection::GetState()
    const {
  return state_;
}

void PresentationConnection::close() {
  DoClose(mojom::blink::PresentationConnectionCloseReason::CLOSED);
}

void PresentationConnection::terminate() {
  if (state_ != mojom::blink::PresentationConnectionState::CONNECTED)
    return;

  TerminateInternal();
  TearDown();
}

bool PresentationConnection::Matches(const String& id, const KURL& url) const {
  return url_ == url && id_ == id;
}

void PresentationConnection::DidClose(
    mojom::blink::PresentationConnectionCloseReason reason,
    const String& message) {
  if (state_ == mojom::blink::PresentationConnectionState::CLOSED ||
      state_ == mojom::blink::PresentationConnectionState::TERMINATED) {
    return;
  }

  state_ = mojom::blink::PresentationConnectionState::CLOSED;
  DispatchStateChangeEvent(PresentationConnectionCloseEvent::Create(
      EventTypeNames::close, ConnectionCloseReasonToString(reason), message));
}

void PresentationConnection::DidFinishLoadingBlob(DOMArrayBuffer* buffer) {
  DCHECK(!messages_.IsEmpty());
  DCHECK_EQ(messages_.front()->type, kMessageTypeBlob);
  DCHECK(buffer);
  DCHECK(buffer->Buffer());

  // Send the loaded blob immediately here and continue processing the queue.
  SendMessageToTargetConnection(MakeBinaryMessage(buffer));

  messages_.pop_front();
  blob_loader_.Clear();
  HandleMessageQueue();
}

void PresentationConnection::DidFailLoadingBlob(
    FileError::ErrorCode error_code) {
  DCHECK(!messages_.IsEmpty());
  DCHECK_EQ(messages_.front()->type, kMessageTypeBlob);
  // FIXME: generate error message?
  // Ignore the current failed blob item and continue with next items.
  messages_.pop_front();
  blob_loader_.Clear();
  HandleMessageQueue();
}

void PresentationConnection::DispatchStateChangeEvent(Event* event) {
  GetExecutionContext()
      ->GetTaskRunner(TaskType::kPresentation)
      ->PostTask(FROM_HERE,
                 WTF::Bind(&PresentationConnection::DispatchEventAsync,
                           WrapPersistent(this), WrapPersistent(event)));
}

// static
void PresentationConnection::DispatchEventAsync(EventTarget* target,
                                                Event* event) {
  DCHECK(target);
  DCHECK(event);
  target->DispatchEvent(*event);
}

void PresentationConnection::TearDown() {
  // Cancel current Blob loading if any.
  if (blob_loader_) {
    blob_loader_->Cancel();
    blob_loader_.Clear();
  }
  messages_.clear();
}

}  // namespace blink
