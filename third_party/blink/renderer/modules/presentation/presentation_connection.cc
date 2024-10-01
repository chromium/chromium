// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation_connection.h"

#include <memory>
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_client.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/presentation/presentation.h"
#include "third_party/blink/renderer/modules/presentation/presentation_connection_available_event.h"
#include "third_party/blink/renderer/modules/presentation/presentation_connection_close_event.h"
#include "third_party/blink/renderer/modules/presentation/presentation_controller.h"
#include "third_party/blink/renderer/modules/presentation/presentation_receiver.h"
#include "third_party/blink/renderer/modules/presentation/presentation_request.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

namespace {

mojom::blink::PresentationConnectionMessagePtr MakeBinaryMessage(
    const DOMArrayBuffer* buffer) {
  // Mutating the data field on the message instead of passing in an already
  // populated Vector into message constructor is more efficient since the
  // latter does not support moves.
  auto message =
      mojom::blink::PresentationConnectionMessage::NewData(Vector<uint8_t>());
  Vector<uint8_t>& data = message->get_data();
  data.AppendSpan(buffer->ByteSpan());
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

  NOTREACHED_IN_MIGRATION();
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

  NOTREACHED_IN_MIGRATION();
  return error_value;
}

void ThrowPresentationDisconnectedError(ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                    "Presentation connection is disconnected.");
}

}  // namespace

class PresentationConnection::Message final
    : public GarbageCollected<PresentationConnection::Message> {
 public:
  Message(const String& text) : type(kMessageTypeText), text(text) {}

  Message(DOMArrayBuffer* array_buffer)
      : type(kMessageTypeArrayBuffer), array_buffer(array_buffer) {}

  Message(scoped_refptr<BlobDataHandle> blob_data_handle)
      : type(kMessageTypeBlob), blob_data_handle(std::move(blob_data_handle)) {}

  void Trace(Visitor* visitor) const { visitor->Trace(array_buffer); }

  MessageType type;
  String text;
  Member<DOMArrayBuffer> array_buffer;
  scoped_refptr<BlobDataHandle> blob_data_handle;
};

class PresentationConnection::BlobLoader final
    : public GarbageCollected<PresentationConnection::BlobLoader>,
      public FileReaderAccumulator {
 public:
  BlobLoader(scoped_refptr<BlobDataHandle> blob_data_handle,
             PresentationConnection* presentation_connection,
             scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : presentation_connection_(presentation_connection),
        loader_(
            MakeGarbageCollected<FileReaderLoader>(this,
                                                   std::move(task_runner))) {
    loader_->Start(std::move(blob_data_handle));
  }
  ~BlobLoader() override = default;

  // FileReaderAccumulator functions.
  void DidFinishLoading(FileReaderData contents) override {
    auto* buffer = std::move(contents).AsDOMArrayBuffer();
    presentation_connection_->DidFinishLoadingBlob(buffer);
  }
  void DidFail(FileErrorCode error_code) override {
    FileReaderAccumulator::DidFail(error_code);
    presentation_connection_->DidFailLoadingBlob(error_code);
  }

  void Cancel() { loader_->Cancel(); }

  void Trace(Visitor* visitor) const override {
    FileReaderAccumulator::Trace(visitor);
    visitor->Trace(presentation_connection_);
    visitor->Trace(loader_);
  }

 private:
  Member<PresentationConnection> presentation_connection_;
  Member<FileReaderLoader> loader_;
};

PresentationConnection::PresentationConnection(LocalDOMWindow& window,
                                               const String& id,
                                               const KURL& url)
    : ExecutionContextLifecycleStateObserver(&window),
      id_(id),
      url_(url),
      state_(mojom::blink::PresentationConnectionState::CONNECTING),
      connection_receiver_(this, &window),
      target_connection_(&window),
      file_reading_task_runner_(window.GetTaskRunner(TaskType::kFileReading)) {
  UpdateStateIfNeeded();
}

PresentationConnection::~PresentationConnection() {
  DCHECK(!blob_loader_);
}

void PresentationConnection::OnMessage(
    mojom::blink::PresentationConnectionMessagePtr message) {
  if (message->is_data()) {
    DidReceiveBinaryMessage(message->get_data());
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
      EnqueueEvent(*Event::Create(event_type_names::kConnect),
                   TaskType::kPresentation);
      return;
    case mojom::blink::PresentationConnectionState::CLOSED:
      return;
    case mojom::blink::PresentationConnectionState::TERMINATED:
      EnqueueEvent(*Event::Create(event_type_names::kTerminate),
                   TaskType::kPresentation);
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void PresentationConnection::DidClose(
    mojom::blink::PresentationConnectionCloseReason reason) {
  DidClose(reason, /* message */ String());
}

// static
ControllerPresentationConnection* ControllerPresentationConnection::Take(
    ScriptPromiseResolverBase* resolver,
    const mojom::blink::PresentationInfo& presentation_info,
    PresentationRequest* request) {
  DCHECK(resolver);
  DCHECK(request);

  PresentationController* controller =
      PresentationController::FromContext(resolver->GetExecutionContext());
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

  auto* connection = MakeGarbageCollected<ControllerPresentationConnection>(
      *controller->GetSupplementable(), controller, presentation_info.id,
      presentation_info.url);
  controller->RegisterConnection(connection);

  // Fire onconnectionavailable event asynchronously.
  request->EnqueueEvent(*PresentationConnectionAvailableEvent::Create(
                            event_type_names::kConnectionavailable, connection),
                        TaskType::kPresentation);
  return connection;
}

ControllerPresentationConnection::ControllerPresentationConnection(
    LocalDOMWindow& window,
    PresentationController* controller,
    const String& id,
    const KURL& url)
    : PresentationConnection(window, id, url), controller_(controller) {}

ControllerPresentationConnection::~ControllerPresentationConnection() {}

void ControllerPresentationConnection::Trace(Visitor* visitor) const {
  visitor->Trace(controller_);
  PresentationConnection::Trace(visitor);
}

void ControllerPresentationConnection::Init(
    mojo::PendingRemote<mojom::blink::PresentationConnection> connection_remote,
    mojo::PendingReceiver<mojom::blink::PresentationConnection>
        connection_receiver) {
  // Note that it is possible for the binding to be already bound here, because
  // the ControllerPresentationConnection object could be reused when
  // reconnecting in the same frame. In this case the existing connections are
  // discarded.
  if (connection_receiver_.is_bound()) {
    connection_receiver_.reset();
    target_connection_.reset();
  }

  DidChangeState(mojom::blink::PresentationConnectionState::CONNECTING);
  target_connection_.Bind(
      std::move(connection_remote),
      GetExecutionContext()->GetTaskRunner(blink::TaskType::kPresentation));
  connection_receiver_.Bind(
      std::move(connection_receiver),
      GetExecutionContext()->GetTaskRunner(blink::TaskType::kPresentation));
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
    mojo::PendingRemote<mojom::blink::PresentationConnection>
        controller_connection,
    mojo::PendingReceiver<mojom::blink::PresentationConnection>
        receiver_connection_receiver) {
  DCHECK(receiver);

  ReceiverPresentationConnection* connection =
      MakeGarbageCollected<ReceiverPresentationConnection>(
          *receiver->GetWindow(), receiver, presentation_info.id,
          presentation_info.url);
  connection->Init(std::move(controller_connection),
                   std::move(receiver_connection_receiver));

  receiver->RegisterConnection(connection);
  return connection;
}

ReceiverPresentationConnection::ReceiverPresentationConnection(
    LocalDOMWindow& window,
    PresentationReceiver* receiver,
    const String& id,
    const KURL& url)
    : PresentationConnection(window, id, url), receiver_(receiver) {}

ReceiverPresentationConnection::~ReceiverPresentationConnection() = default;

void ReceiverPresentationConnection::Init(
    mojo::PendingRemote<mojom::blink::PresentationConnection>
        controller_connection_remote,
    mojo::PendingReceiver<mojom::blink::PresentationConnection>
        receiver_connection_receiver) {
  target_connection_.Bind(
      std::move(controller_connection_remote),
      GetExecutionContext()->GetTaskRunner(blink::TaskType::kPresentation));
  connection_receiver_.Bind(
      std::move(receiver_connection_receiver),
      GetExecutionContext()->GetTaskRunner(blink::TaskType::kPresentation));

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
  if (target_connection_.is_bound())
    target_connection_->DidChangeState(state_);
}

void ReceiverPresentationConnection::Trace(Visitor* visitor) const {
  visitor->Trace(receiver_);
  PresentationConnection::Trace(visitor);
}

const AtomicString& PresentationConnection::InterfaceName() const {
  return event_target_names::kPresentationConnection;
}

ExecutionContext* PresentationConnection::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

void PresentationConnection::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTarget::AddedEventListener(event_type, registered_listener);
  if (event_type == event_type_names::kConnect) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kPresentationConnectionConnectEventListener);
  } else if (event_type == event_type_names::kClose) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kPresentationConnectionCloseEventListener);
  } else if (event_type == event_type_names::kTerminate) {
    UseCounter::Count(
        GetExecutionContext(),
        WebFeature::kPresentationConnectionTerminateEventListener);
  } else if (event_type == event_type_names::kMessage) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kPresentationConnectionMessageEventListener);
  }
}

void PresentationConnection::ContextDestroyed() {
  CloseConnection();
}

void PresentationConnection::ContextLifecycleStateChanged(
    mojom::FrameLifecycleState state) {
  if (state == mojom::FrameLifecycleState::kFrozen ||
      state == mojom::FrameLifecycleState::kFrozenAutoResumeMedia) {
    CloseConnection();
  }
}

void PresentationConnection::CloseConnection() {
  DoClose(mojom::blink::PresentationConnectionCloseReason::WENT_AWAY);
  target_connection_.reset();
  connection_receiver_.reset();
}

void PresentationConnection::Trace(Visitor* visitor) const {
  visitor->Trace(connection_receiver_);
  visitor->Trace(target_connection_);
  visitor->Trace(blob_loader_);
  visitor->Trace(messages_);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleStateObserver::Trace(visitor);
}

const AtomicString& PresentationConnection::state() const {
  return ConnectionStateToString(state_);
}

void PresentationConnection::send(const String& message,
                                  ExceptionState& exception_state) {
  if (!CanSendMessage(exception_state))
    return;

  messages_.push_back(MakeGarbageCollected<Message>(message));
  HandleMessageQueue();
}

void PresentationConnection::send(DOMArrayBuffer* array_buffer,
                                  ExceptionState& exception_state) {
  DCHECK(array_buffer);
  if (!CanSendMessage(exception_state))
    return;
  if (!base::CheckedNumeric<wtf_size_t>(array_buffer->ByteLength()).IsValid()) {
    static_assert(
        4294967295 == std::numeric_limits<wtf_size_t>::max(),
        "Change the error message below if this static_assert fails.");
    exception_state.ThrowRangeError(
        "ArrayBuffer size exceeds the maximum supported size (4294967295)");
    return;
  }

  messages_.push_back(MakeGarbageCollected<Message>(array_buffer));
  HandleMessageQueue();
}

void PresentationConnection::send(
    NotShared<DOMArrayBufferView> array_buffer_view,
    ExceptionState& exception_state) {
  DCHECK(array_buffer_view);
  if (!CanSendMessage(exception_state))
    return;
  if (!base::CheckedNumeric<wtf_size_t>(array_buffer_view->byteLength())
           .IsValid()) {
    static_assert(
        4294967295 == std::numeric_limits<wtf_size_t>::max(),
        "Change the error message below if this static_assert fails.");
    exception_state.ThrowRangeError(
        "ArrayBuffer size exceeds the maximum supported size (4294967295)");
    return;
  }

  messages_.push_back(
      MakeGarbageCollected<Message>(array_buffer_view->buffer()));
  HandleMessageQueue();
}

void PresentationConnection::send(Blob* data, ExceptionState& exception_state) {
  DCHECK(data);
  if (!CanSendMessage(exception_state))
    return;

  messages_.push_back(MakeGarbageCollected<Message>(data->GetBlobDataHandle()));
  HandleMessageQueue();
}

void PresentationConnection::DoClose(
    mojom::blink::PresentationConnectionCloseReason reason) {
  if (state_ != mojom::blink::PresentationConnectionState::CONNECTING &&
      state_ != mojom::blink::PresentationConnectionState::CONNECTED) {
    return;
  }

  if (target_connection_.is_bound())
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

  return !!target_connection_.is_bound();
}

void PresentationConnection::HandleMessageQueue() {
  if (!target_connection_.is_bound())
    return;

  while (!messages_.empty() && !blob_loader_) {
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
        blob_loader_ = MakeGarbageCollected<BlobLoader>(
            message->blob_data_handle, this, file_reading_task_runner_);
        break;
    }
  }
}

V8BinaryType PresentationConnection::binaryType() const {
  return V8BinaryType(binary_type_);
}

void PresentationConnection::setBinaryType(const V8BinaryType& binary_type) {
  binary_type_ = binary_type.AsEnum();
}

void PresentationConnection::SendMessageToTargetConnection(
    mojom::blink::PresentationConnectionMessagePtr message) {
  if (target_connection_.is_bound())
    target_connection_->OnMessage(std::move(message));
}

void PresentationConnection::DidReceiveTextMessage(const WebString& message) {
  if (state_ != mojom::blink::PresentationConnectionState::CONNECTED)
    return;

  DispatchEvent(*MessageEvent::Create(message));
}

void PresentationConnection::DidReceiveBinaryMessage(
    base::span<const uint8_t> data) {
  if (state_ != mojom::blink::PresentationConnectionState::CONNECTED)
    return;

  switch (binary_type_) {
    case V8BinaryType::Enum::kBlob: {
      auto blob_data = std::make_unique<BlobData>();
      blob_data->AppendBytes(data);
      auto* blob = MakeGarbageCollected<Blob>(
          BlobDataHandle::Create(std::move(blob_data), data.size()));
      DispatchEvent(*MessageEvent::Create(blob));
      return;
    }
    case V8BinaryType::Enum::kArraybuffer:
      DOMArrayBuffer* buffer = DOMArrayBuffer::Create(data);
      DispatchEvent(*MessageEvent::Create(buffer));
      return;
  }
  NOTREACHED_IN_MIGRATION();
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
  EnqueueEvent(*PresentationConnectionCloseEvent::Create(
                   event_type_names::kClose,
                   ConnectionCloseReasonToString(reason), message),
               TaskType::kPresentation);
}

void PresentationConnection::DidFinishLoadingBlob(DOMArrayBuffer* buffer) {
  DCHECK(!messages_.empty());
  DCHECK_EQ(messages_.front()->type, kMessageTypeBlob);
  DCHECK(buffer);
  if (!base::CheckedNumeric<wtf_size_t>(buffer->ByteLength()).IsValid()) {
    // TODO(crbug.com/1036565): generate error message? The problem is that the
    // content of {buffer} is copied into a WTF::Vector, but a DOMArrayBuffer
    // has a bigger maximum size than a WTF::Vector. Ignore the current failed
    // blob item and continue with next items.
    messages_.pop_front();
    blob_loader_.Clear();
    HandleMessageQueue();
  }
  // Send the loaded blob immediately here and continue processing the queue.
  SendMessageToTargetConnection(MakeBinaryMessage(buffer));

  messages_.pop_front();
  blob_loader_.Clear();
  HandleMessageQueue();
}

void PresentationConnection::DidFailLoadingBlob(FileErrorCode error_code) {
  DCHECK(!messages_.empty());
  DCHECK_EQ(messages_.front()->type, kMessageTypeBlob);
  // TODO(crbug.com/1036565): generate error message?
  // Ignore the current failed blob item and continue with next items.
  messages_.pop_front();
  blob_loader_.Clear();
  HandleMessageQueue();
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
