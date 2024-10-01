// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Functions for transferable streams. See design doc
// https://docs.google.com/document/d/1_KuZzg5c3pncLJPFa8SuVm23AP4tft6mzPCL5at3I9M/edit

#include "third_party/blink/renderer/core/streams/transferable_streams.h"

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_post_message_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream_default_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/streams/miscellaneous_operations.h"
#include "third_party/blink/renderer/core/streams/promise_handler.h"
#include "third_party/blink/renderer/core/streams/read_request.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/core/streams/readable_stream_transferring_optimizer.h"
#include "third_party/blink/renderer/core/streams/stream_algorithms.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/writable_stream_transferring_optimizer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "v8/include/v8.h"

// See the design doc at
// https://docs.google.com/document/d/1_KuZzg5c3pncLJPFa8SuVm23AP4tft6mzPCL5at3I9M/edit
// for explanation of how transferable streams are constructed from the "cross
// realm identity transform" implemented in this file.

// The peer (the other end of the MessagePort) is untrusted as it may be
// compromised. This means we have to be very careful in unpacking the messages
// from the peer. LOG(WARNING) is used for cases where a message from the peer
// appears to be invalid. If this appears during ordinary testing it indicates a
// bug.
//
// The -vmodule=transferable_streams=3 command-line argument can be used for
// debugging of the protocol.

namespace blink {

namespace {

template <typename T, typename... Args>
ScriptFunction* CreateFunction(ScriptState* script_state, Args&&... args) {
  return MakeGarbageCollected<ScriptFunction>(
      script_state, MakeGarbageCollected<T>(std::forward<Args>(args)...));
}

// These are the types of messages that are sent between peers.
enum class MessageType { kPull, kChunk, kClose, kError };

// Creates a JavaScript object with a null prototype structured like {key1:
// value2, key2: value2}. This is used to create objects to be serialized by
// postMessage.
v8::Local<v8::Object> CreateKeyValueObject(v8::Isolate* isolate,
                                           const char* key1,
                                           v8::Local<v8::Value> value1,
                                           const char* key2,
                                           v8::Local<v8::Value> value2) {
  v8::Local<v8::Name> names[] = {V8AtomicString(isolate, key1),
                                 V8AtomicString(isolate, key2)};
  v8::Local<v8::Value> values[] = {value1, value2};
  static_assert(std::size(names) == std::size(values),
                "names and values arrays must be the same size");
  return v8::Object::New(isolate, v8::Null(isolate), names, values,
                         std::size(names));
}

// Unpacks an object created by CreateKeyValueObject(). |value1| and |value2|
// are out parameters. Returns false on failure.
bool UnpackKeyValueObject(ScriptState* script_state,
                          v8::Local<v8::Object> object,
                          const char* key1,
                          v8::Local<v8::Value>* value1,
                          const char* key2,
                          v8::Local<v8::Value>* value2) {
  auto* isolate = script_state->GetIsolate();
  v8::TryCatch try_catch(isolate);
  auto context = script_state->GetContext();
  if (!object->Get(context, V8AtomicString(isolate, key1)).ToLocal(value1)) {
    DLOG(WARNING) << "Error reading key: '" << key1 << "'";
    return false;
  }
  if (!object->Get(context, V8AtomicString(isolate, key2)).ToLocal(value2)) {
    DLOG(WARNING) << "Error reading key: '" << key2 << "'";
    return false;
  }
  return true;
}

// Sends a message with type |type| and contents |value| over |port|. The type
// is packed as a number with key "t", and the value is packed with key "v".
void PackAndPostMessage(ScriptState* script_state,
                        MessagePort* port,
                        MessageType type,
                        v8::Local<v8::Value> value,
                        AllowPerChunkTransferring allow_per_chunk_transferring,
                        ExceptionState& exception_state) {
  DVLOG(3) << "PackAndPostMessage sending message type "
           << static_cast<int>(type);
  v8::Context::Scope v8_context_scope(script_state->GetContext());
  auto* isolate = script_state->GetIsolate();

  // https://streams.spec.whatwg.org/#abstract-opdef-packandpostmessage
  // 1. Let message be OrdinaryObjectCreate(null).
  // 2. Perform ! CreateDataProperty(message, "type", type).
  // 3. Perform ! CreateDataProperty(message, "value", value).
  v8::Local<v8::Object> packed = CreateKeyValueObject(
      isolate, "t", v8::Number::New(isolate, static_cast<int>(type)), "v",
      value);

  // 5. Let options be «[ "transfer" → « » ]».
  PostMessageOptions* options = PostMessageOptions::Create();
  if (allow_per_chunk_transferring && type == MessageType::kChunk) {
    // Here we set a non-empty transfer list: This is a non-standardized and
    // non-default behavior, and the one who set `allow_per_chunk_transferring`
    // to true must guarantee the validity.
    HeapVector<ScriptValue> transfer;
    transfer.push_back(ScriptValue(isolate, value));
    options->setTransfer(transfer);
  }

  // 4. Let targetPort be the port with which port is entangled, if any;
  //    otherwise let it be null.
  // 6. Run the message port post message steps providing targetPort, message,
  //    and options.
  port->postMessage(script_state, ScriptValue(isolate, packed), options,
                    exception_state);
}

// Sends a kError message to the remote side, disregarding failure.
void CrossRealmTransformSendError(ScriptState* script_state,
                                  MessagePort* port,
                                  v8::Local<v8::Value> error) {
  v8::TryCatch try_catch(script_state->GetIsolate());

  // https://streams.spec.whatwg.org/#abstract-opdef-crossrealmtransformsenderror
  // 1. Perform PackAndPostMessage(port, "error", error), discarding the result.
  PackAndPostMessage(script_state, port, MessageType::kError, error,
                     AllowPerChunkTransferring(false),
                     PassThroughException(script_state->GetIsolate()));
  if (try_catch.HasCaught()) {
    DLOG(WARNING) << "Disregarding exception while sending error";
  }
}

// Same as PackAndPostMessage(), except that it attempts to handle exceptions by
// sending a kError message to the remote side. Any error from sending the
// kError message is ignored.
//
// The calling convention differs slightly from the standard to minimize
// verbosity at the calling sites. The function returns true for a normal
// completion and false for an abrupt completion.When there's an abrupt
// completion result.[[Value]] is stored into |error|.
bool PackAndPostMessageHandlingError(
    ScriptState* script_state,
    MessagePort* port,
    MessageType type,
    v8::Local<v8::Value> value,
    AllowPerChunkTransferring allow_per_chunk_transferring,
    v8::Local<v8::Value>* error) {
  v8::TryCatch try_catch(script_state->GetIsolate());
  // https://streams.spec.whatwg.org/#abstract-opdef-packandpostmessagehandlingerror
  // 1. Let result be PackAndPostMessage(port, type, value).
  PackAndPostMessage(script_state, port, type, value,
                     allow_per_chunk_transferring,
                     PassThroughException(script_state->GetIsolate()));

  // 2. If result is an abrupt completion,
  if (try_catch.HasCaught()) {
    //   1. Perform ! CrossRealmTransformSendError(port, result.[[Value]]).
    // 3. Return result as a completion record.
    *error = try_catch.Exception();
    CrossRealmTransformSendError(script_state, port, try_catch.Exception());
    return false;
  }

  return true;
}

bool PackAndPostMessageHandlingError(ScriptState* script_state,
                                     MessagePort* port,
                                     MessageType type,
                                     v8::Local<v8::Value> value,
                                     v8::Local<v8::Value>* error) {
  return PackAndPostMessageHandlingError(
      script_state, port, type, value, AllowPerChunkTransferring(false), error);
}

// Base class for CrossRealmTransformWritable and CrossRealmTransformReadable.
// Contains common methods that are used when handling MessagePort events.
class CrossRealmTransformStream
    : public GarbageCollected<CrossRealmTransformStream> {
 public:
  // Neither of the subclasses require finalization, so no destructor.

  virtual ScriptState* GetScriptState() const = 0;
  virtual MessagePort* GetMessagePort() const = 0;

  // HandleMessage() is called by CrossRealmTransformMessageListener to handle
  // an incoming message from the MessagePort.
  virtual void HandleMessage(MessageType type, v8::Local<v8::Value> value) = 0;

  // HandleError() is called by CrossRealmTransformErrorListener when an error
  // event is fired on the message port. It should error the stream.
  virtual void HandleError(v8::Local<v8::Value> error) = 0;

  virtual void Trace(Visitor*) const {}
};

// Handles MessageEvents from the MessagePort.
class CrossRealmTransformMessageListener final : public NativeEventListener {
 public:
  explicit CrossRealmTransformMessageListener(CrossRealmTransformStream* target)
      : target_(target) {}

  void Invoke(ExecutionContext*, Event* event) override {
    // TODO(ricea): Find a way to guarantee this cast is safe.
    MessageEvent* message = static_cast<MessageEvent*>(event);
    ScriptState* script_state = target_->GetScriptState();
    // The deserializer code called by message->data() looks up the ScriptState
    // from the current context, so we need to make sure it is set.
    ScriptState::Scope scope(script_state);

    // Common to
    // https://streams.spec.whatwg.org/#abstract-opdef-setupcrossrealmtransformreadable
    // and
    // https://streams.spec.whatwg.org/#abstract-opdef-setupcrossrealmtransformwritable.

    // 1. Let data be the data of the message.
    v8::Local<v8::Value> data = message->data(script_state).V8Value();

    // 2. Assert: Type(data) is Object.
    // In the world of the standard, this is guaranteed to be true. In the real
    // world, the data could come from a compromised renderer and be malicious.
    if (!data->IsObject()) {
      DLOG(WARNING) << "Invalid message from peer ignored (not object)";
      return;
    }

    // 3. Let type be ! Get(data, "type").
    // 4. Let value be ! Get(data, "value").
    v8::Local<v8::Value> type;
    v8::Local<v8::Value> value;
    if (!UnpackKeyValueObject(script_state, data.As<v8::Object>(), "t", &type,
                              "v", &value)) {
      DLOG(WARNING) << "Invalid message from peer ignored";
      return;
    }

    // 5. Assert: Type(type) is String
    // This implementation uses numbers for types rather than strings.
    if (!type->IsNumber()) {
      DLOG(WARNING) << "Invalid message from peer ignored (type is not number)";
      return;
    }

    int type_value = type.As<v8::Number>()->Value();
    DVLOG(3) << "MessageListener saw message type " << type_value;
    target_->HandleMessage(static_cast<MessageType>(type_value), value);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(target_);
    NativeEventListener::Trace(visitor);
  }

 private:
  const Member<CrossRealmTransformStream> target_;
};

// Handles "error" events from the MessagePort.
class CrossRealmTransformErrorListener final : public NativeEventListener {
 public:
  explicit CrossRealmTransformErrorListener(CrossRealmTransformStream* target)
      : target_(target) {}

  void Invoke(ExecutionContext*, Event*) override {
    ScriptState* script_state = target_->GetScriptState();

    // Need to enter a script scope to manipulate JavaScript objects.
    ScriptState::Scope scope(script_state);

    // Common to
    // https://streams.spec.whatwg.org/#abstract-opdef-setupcrossrealmtransformreadable
    // and
    // https://streams.spec.whatwg.org/#abstract-opdef-setupcrossrealmtransformwritable.

    // 1. Let error be a new "DataCloneError" DOMException.
    v8::Local<v8::Value> error = V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kDataCloneError,
        "chunk could not be cloned");

    // 2. Perform ! CrossRealmTransformSendError(port, error).
    auto* message_port = target_->GetMessagePort();
    CrossRealmTransformSendError(script_state, message_port, error);

    // 4. Disentangle port.
    message_port->close();

    DVLOG(3) << "ErrorListener saw messageerror";
    target_->HandleError(error);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(target_);
    NativeEventListener::Trace(visitor);
  }

 private:
  const Member<CrossRealmTransformStream> target_;
};

// Class for data associated with the writable side of the cross realm transform
// stream.
class CrossRealmTransformWritable final : public CrossRealmTransformStream {
 public:
  CrossRealmTransformWritable(
      ScriptState* script_state,
      MessagePort* port,
      AllowPerChunkTransferring allow_per_chunk_transferring)
      : script_state_(script_state),
        message_port_(port),
        backpressure_promise_(
            MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
                script_state)),
        allow_per_chunk_transferring_(allow_per_chunk_transferring) {}

  WritableStream* CreateWritableStream(ExceptionState&);

  ScriptState* GetScriptState() const override { return script_state_.Get(); }
  MessagePort* GetMessagePort() const override { return message_port_.Get(); }
  void HandleMessage(MessageType type, v8::Local<v8::Value> value) override;
  void HandleError(v8::Local<v8::Value> error) override;

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    visitor->Trace(message_port_);
    visitor->Trace(backpressure_promise_);
    visitor->Trace(controller_);
    CrossRealmTransformStream::Trace(visitor);
  }

 private:
  class WriteAlgorithm;
  class CloseAlgorithm;
  class AbortAlgorithm;

  const Member<ScriptState> script_state_;
  const Member<MessagePort> message_port_;
  Member<ScriptPromiseResolver<IDLUndefined>> backpressure_promise_;
  Member<WritableStreamDefaultController> controller_;
  const AllowPerChunkTransferring allow_per_chunk_transferring_;
};

class CrossRealmTransformWritable::WriteAlgorithm final
    : public StreamAlgorithm {
 public:
  explicit WriteAlgorithm(CrossRealmTransformWritable* writable)
      : writable_(writable) {}

  // Sends the chunk to the readable side, possibly after waiting for
  // backpressure.
  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int argc,
                             v8::Local<v8::Value> argv[]) override {
    // https://streams.spec.whatwg.org/#abstract-opdef-setupcrossrealmtransformwritable
    // 8. Let writeAlgorithm be the following steps, taking a chunk argument:
    DCHECK_EQ(argc, 1);
    auto chunk = argv[0];

    // 1. If backpressurePromise is undefined, set backpressurePromise to a
    //    promise resolved with undefined.

    // As an optimization for the common case, we call DoWrite() synchronously
    // instead. The difference is not observable because the result is only
    // visible asynchronously anyway. This avoids doing an extra allocation and
    // creating a TraceWrappertV8Reference.
    if (!writable_->backpressure_promise_) {
      return DoWrite(script_state, chunk);
    }

    // 2. Return the result of reacting to backpressurePromise with the
    //    following fulfillment steps:

    return StreamThenPromise(
        script_state->GetContext(),
        writable_->backpressure_promise_->V8Promise(),
        MakeGarbageCollected<ScriptFunction>(
            script_state,
            MakeGarbageCollected<DoWriteOnResolve>(script_state, chunk, this)));
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(writable_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  // A promise handler which calls DoWrite() when the promise resolves.
  class DoWriteOnResolve final : public PromiseHandlerWithValue {
   public:
    DoWriteOnResolve(ScriptState* script_state,
                     v8::Local<v8::Value> chunk,
                     WriteAlgorithm* target)
        : chunk_(script_state->GetIsolate(), chunk), target_(target) {}

    v8::Local<v8::Value> CallWithLocal(ScriptState* script_state,
                                       v8::Local<v8::Value>) override {
      return target_->DoWrite(script_state,
                              chunk_.Get(script_state->GetIsolate()));
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(chunk_);
      visitor->Trace(target_);
      PromiseHandlerWithValue::Trace(visitor);
    }

   private:
    const TraceWrapperV8Reference<v8::Value> chunk_;
    const Member<WriteAlgorithm> target_;
  };

  // Sends a chunk over the message port to the readable side.
  v8::Local<v8::Promise> DoWrite(ScriptState* script_state,
                                 v8::Local<v8::Value> chunk) {
    // https://streams.spec.whatwg.org/#abstract-opdef-setupcrossrealmtransformwritable
    // 8. Let writeAlgorithm be the following steps, taking a chunk argument:
    //   2. Return the result of reacting to backpressurePromise with the
    //      following fulfillment steps:
    //     1. Set backpressurePromise to a new promise.
    writable_->backpressure_promise_ =
        MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);

    v8::Local<v8::Value> error;

    //     2. Let result be PackAndPostMessageHandlingError(port, "chunk",
    //        chunk).
    bool success = PackAndPostMessageHandlingError(
        script_state, writable_->message_port_, MessageType::kChunk, chunk,
        writable_->allow_per_chunk_transferring_, &error);
    //     3. If result is an abrupt completion,
    if (!success) {
      //     1. Disentangle port.
      writable_->message_port_->close();

      //     2. Return a promise rejected with result.[[Value]].
      return PromiseReject(script_state, error);
    }

    //     4. Otherwise, return a promise resolved with undefined.
    return PromiseResolveWithUndefined(script_state);
  }

  const Member<CrossRealmTransformWritable> writable_;
};

class CrossRealmTransformWritable::CloseAlgorithm final
    : public StreamAlgorithm {
 public:
  explicit CloseAlgorithm(CrossRealmTransformWritable* writable)
      : writable_(writable) {}

  // Sends a close message to the readable side and closes the message port.
  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int argc,
                             v8::Local<v8::Value> argv[]) override {
    DCHECK_EQ(argc, 0);

    // https://streams.spec.whatwg.org/#abstract-opdef-setupcrossrealmtransformwritable
    // 9. Let closeAlgorithm be the folowing steps:
    v8::Local<v8::Value> error;
    //   1. Perform ! PackAndPostMessage(port, "close", undefined).
    // In the standard, this can't fail. However, in the implementation failure
    // is possible, so we have to handle it.
    bool success = PackAndPostMessageHandlingError(
        script_state, writable_->message_port_, MessageType::kClose,
        v8::Undefined(script_state->GetIsolate()), &error);

    //   2. Disentangle port.
    writable_->message_port_->close();

    // Error the stream if an error occurred.
    if (!success) {
      return PromiseReject(script_state, error);
    }

    //   3. Return a promise resolved with undefined.
    return PromiseResolveWithUndefined(script_state);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(writable_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  const Member<CrossRealmTransformWritable> writable_;
};

class CrossRealmTransformWritable::AbortAlgorithm final
    : public StreamAlgorithm {
 public:
  explicit AbortAlgorithm(CrossRealmTransformWritable* writable)
      : writable_(writable) {}

  // Sends an abort message to the readable side and closes the message port.
  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int argc,
                             v8::Local<v8::Value> argv[]) override {
    // https://streams.spec.whatwg.org/#abstract-opdef-setupcrossrealmtransformwritable
    // 10. Let abortAlgorithm be the following steps, taking a reason argument:
    DCHECK_EQ(argc, 1);
    auto reason = argv[0];

    v8::Local<v8::Value> error;

    //   1. Let result be PackAndPostMessageHandlingError(port, "error",
    //      reason).
    bool success =
        PackAndPostMessageHandlingError(script_state, writable_->message_port_,
                                        MessageType::kError, reason, &error);

    //   2. Disentangle port.
    writable_->message_port_->close();

    //   3. If result is an abrupt completion, return a promise rejected with
    //      result.[[Value]].
    if (!success) {
      return PromiseReject(script_state, error);
    }

    //   4. Otherwise, return a promise resolved with undefined.
    return PromiseResolveWithUndefined(script_state);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(writable_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  const Member<CrossRealmTransformWritable> writable_;
};

WritableStream* CrossRealmTransformWritable::CreateWritableStream(
    ExceptionState& exception_state) {
  DCHECK(!controller_) << "CreateWritableStream() can only be called once";

  // https://streams.spec.whatwg.org/#abstract-opdef-setupcrossrealmtransformwritable
  // The order of operations is significantly different from the standard, but
  // functionally equivalent.

  //  3. Let backpressurePromise be a new promise.
  // |backpressure_promise_| is initialized by the constructor.

  //  4. Add a handler for port’s message event with the following steps:
  //  6. Enable port’s port message queue.
  message_port_->setOnmessage(
      MakeGarbageCollected<CrossRealmTransformMessageListener>(this));

  //  5. Add a handler for port’s messageerror event with the following steps:
  message_port_->setOnmessageerror(
      MakeGarbageCollected<CrossRealmTransformErrorListener>(this));

  //  1. Perform ! InitializeWritableStream(stream).
  //  2. Let controller be a new WritableStreamDefaultController.
  //  7. Let startAlgorithm be an algorithm that returns undefined.
  // 11. Let sizeAlgorithm be an algorithm that returns 1.
  // 12. Perform ! SetUpWritableStreamDefaultController(stream, controller,
  //     startAlgorithm, writeAlgorithm, closeAlgorithm, abortAlgorithm, 1,
  //     sizeAlgorithm).
  auto* stream =
      WritableStream::Create(script_state_, CreateTrivialStartAlgorithm(),
                             MakeGarbageCollected<WriteAlgorithm>(this),
                             MakeGarbageCollected<CloseAlgorithm>(this),
                             MakeGarbageCollected<AbortAlgorithm>(this), 1,
                             CreateDefaultSizeAlgorithm(), exception_state);

  if (exception_state.HadException()) {
    return nullptr;
  }

  controller_ = stream->Controller();
  return stream;
}

void CrossRealmTransformWritable::HandleMessage(MessageType type,
                                                v8::Local<v8::Value> value) {
  // https://streams.spec.whatwg.org/#abstract-opdef-setupcrossrealmtransformwritable
  // 4. Add a handler for port’s message event with the following steps:
  // The initial steps are done by CrossRealmTransformMessageListener
  switch (type) {
    // 6. If type is "pull",
    case MessageType::kPull:
      // 1. If backpressurePromise is not undefined,
      if (backpressure_promise_) {
        // 1. Resolve backpressurePromise with undefined.
        backpressure_promise_->Resolve();
        // 2. Set backpressurePromise to undefined.
        backpressure_promise_ = nullptr;
      }
      return;

    // 7. Otherwise if type is "error",
    case MessageType::kError:
      // 1. Perform ! WritableStreamDefaultControllerErrorIfNeeded(controller,
      //    value).
      WritableStreamDefaultController::ErrorIfNeeded(script_state_, controller_,
                                                     value);
      // 2. If backpressurePromise is not undefined,
      if (backpressure_promise_) {
        // 1. Resolve backpressurePromise with undefined.
        // 2. Set backpressurePromise to undefined.
        backpressure_promise_->Resolve();
        backpressure_promise_ = nullptr;
      }
      return;

    default:
      DLOG(WARNING) << "Invalid message from peer ignored (invalid type): "
                    << static_cast<int>(type);
      return;
  }
}

void CrossRealmTransformWritable::HandleError(v8::Local<v8::Value> error) {
  // https://streams.spec.whatwg.org/#abstract-opdef-setupcrossrealmtransformwritable
  // 5. Add a handler for port’s messageerror event with the following steps:
  // The first two steps, and the last step, are performed by
  // CrossRealmTransformErrorListener.

  //   3. Perform ! WritableStreamDefaultControllerError(controller, error).
  // TODO(ricea): Fix the standard to say ErrorIfNeeded and update the above
  // line once that is done.
  WritableStreamDefaultController::ErrorIfNeeded(script_state_, controller_,
                                                 error);
}

// Class for data associated with the readable side of the cross realm transform
// stream.
class CrossRealmTransformReadable final : public CrossRealmTransformStream {
 public:
  CrossRealmTransformReadable(ScriptState* script_state, MessagePort* port)
      : script_state_(script_state), message_port_(port) {}

  ReadableStream* CreateReadableStream(ExceptionState&);

  ScriptState* GetScriptState() const override { return script_state_.Get(); }
  MessagePort* GetMessagePort() const override { return message_port_.Get(); }
  void HandleMessage(MessageType type, v8::Local<v8::Value> value) override;
  void HandleError(v8::Local<v8::Value> error) override;

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    visitor->Trace(message_port_);
    visitor->Trace(controller_);
    CrossRealmTransformStream::Trace(visitor);
  }

 private:
  class PullAlgorithm;
  class CancelAlgorithm;

  const Member<ScriptState> script_state_;
  const Member<MessagePort> message_port_;
  Member<ReadableStreamDefaultController> controller_;
};

class CrossRealmTransformReadable::PullAlgorithm final
    : public StreamAlgorithm {
 public:
  explicit PullAlgorithm(CrossRealmTransformReadable* readable)
      : readable_(readable) {}

  // Sends a pull message to the writable side and then waits for backpressure
  // to clear.
  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int argc,
                             v8::Local<v8::Value> argv[]) override {
    DCHECK_EQ(argc, 0);
    auto* isolate = script_state->GetIsolate();

    // https://streams.spec.whatwg.org/#abstract-opdef-setupcrossrealmtransformreadable
    // 7. Let pullAlgorithm be the following steps:

    v8::Local<v8::Value> error;

    //   1. Perform ! PackAndPostMessage(port, "pull", undefined).
    // In the standard this can't throw an exception, but in the implementation
    // it can, so we need to be able to handle it.
    bool success = PackAndPostMessageHandlingError(
        script_state, readable_->message_port_, MessageType::kPull,
        v8::Undefined(isolate), &error);

    if (!success) {
      readable_->message_port_->close();
      return PromiseReject(script_state, error);
    }

    //   2. Return a promise resolved with undefined.
    // The Streams Standard guarantees that PullAlgorithm won't be called again
    // until Enqueue() is called.
    return PromiseResolveWithUndefined(script_state);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(readable_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  const Member<CrossRealmTransformReadable> readable_;
};

class CrossRealmTransformReadable::CancelAlgorithm final
    : public StreamAlgorithm {
 public:
  explicit CancelAlgorithm(CrossRealmTransformReadable* readable)
      : readable_(readable) {}

  // Sends a cancel message to the writable side and closes the message port.
  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int argc,
                             v8::Local<v8::Value> argv[]) override {
    // https://streams.spec.whatwg.org/#abstract-opdef-setupcrossrealmtransformreadable
    // 8. Let cancelAlgorithm be the following steps, taking a reason argument:
    DCHECK_EQ(argc, 1);
    auto reason = argv[0];

    v8::Local<v8::Value> error;

    //   1. Let result be PackAndPostMessageHandlingError(port, "error",
    //      reason).
    bool success =
        PackAndPostMessageHandlingError(script_state, readable_->message_port_,
                                        MessageType::kError, reason, &error);

    //   2. Disentangle port.
    readable_->message_port_->close();

    //   3. If result is an abrupt completion, return a promise rejected with
    //      result.[[Value]].
    if (!success) {
      return PromiseReject(script_state, error);
    }

    //   4. Otherwise, return a promise resolved with undefined.
    return PromiseResolveWithUndefined(script_state);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(readable_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  const Member<CrossRealmTransformReadable> readable_;
};

class ConcatenatingUnderlyingSource final : public UnderlyingSourceBase {
 public:
  class PullSource2 final : public ScriptFunction::Callable {
   public:
    explicit PullSource2(ConcatenatingUnderlyingSource* source,
                         const ExceptionContext& exception_context)
        : source_(source), exception_context_(exception_context) {}

    ScriptValue Call(ScriptState* script_state, ScriptValue value) override {
      v8::Isolate* isolate = script_state->GetIsolate();
      ExceptionState exception_state(isolate, exception_context_);
      return ScriptValue(
          isolate,
          source_->source2_->Pull(script_state, exception_state).V8Promise());
    }
    void Trace(Visitor* visitor) const override {
      visitor->Trace(source_);
      ScriptFunction::Callable::Trace(visitor);
    }

   private:
    const Member<ConcatenatingUnderlyingSource> source_;
    const ExceptionContext exception_context_;
  };

  class ConcatenatingUnderlyingSourceReadRequest final : public ReadRequest {
   public:
    explicit ConcatenatingUnderlyingSourceReadRequest(
        ConcatenatingUnderlyingSource* source,
        ScriptPromiseResolver<IDLPromise<IDLAny>>* resolver)
        : source_(source), resolver_(resolver) {}

    void ChunkSteps(ScriptState* script_state,
                    v8::Local<v8::Value> chunk,
                    ExceptionState&) const override {
      source_->Controller()->Enqueue(chunk);
      resolver_->Resolve();
    }

    void CloseSteps(ScriptState* script_state) const override {
      // We've finished reading `source1_`. Let's start reading `source2_`.
      source_->has_finished_reading_stream1_ = true;
      ReadableStreamDefaultController* controller =
          source_->Controller()->GetOriginalController();
      auto* isolate = script_state->GetIsolate();
      if (controller) {
        ExceptionState exception_state(script_state->GetIsolate(),
                                       v8::ExceptionContext::kUnknown, "", "");
        resolver_->Resolve(
            source_->source2_
                ->StartWrapper(script_state, controller, exception_state)
                .Then(CreateFunction<PullSource2>(
                    script_state, source_, exception_state.GetContext())));
      } else {
        // TODO(crbug.com/1418910): Investigate how to handle cases when the
        // controller is cleared.
        resolver_->Reject(v8::Exception::TypeError(
            V8String(isolate,
                     "The readable stream controller has been cleared "
                     "and cannot be used to start reading the second "
                     "stream.")));
      }
    }

    void ErrorSteps(ScriptState* script_state,
                    v8::Local<v8::Value> e) const override {
      ReadableStream* dummy_stream =
          ReadableStream::CreateWithCountQueueingStrategy(
              script_state, source_->source2_,
              /*high_water_mark=*/0);

      v8::Isolate* isolate = script_state->GetIsolate();
      // We don't care about the result of the cancellation, including
      // exceptions.
      dummy_stream->cancel(script_state,
                           ScriptValue(isolate, v8::Undefined(isolate)),
                           IGNORE_EXCEPTION);
      resolver_->Reject(e);
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(source_);
      visitor->Trace(resolver_);
      ReadRequest::Trace(visitor);
    }

   private:
    Member<ConcatenatingUnderlyingSource> source_;
    Member<ScriptPromiseResolver<IDLPromise<IDLAny>>> resolver_;
  };

  ConcatenatingUnderlyingSource(ScriptState* script_state,
                                ReadableStream* stream1,
                                UnderlyingSourceBase* source2)
      : UnderlyingSourceBase(script_state),
        stream1_(stream1),
        source2_(source2) {}

  ScriptPromiseUntyped Start(ScriptState* script_state,
                             ExceptionState&) override {
    v8::TryCatch try_catch(script_state->GetIsolate());
    reader_for_stream1_ = ReadableStream::AcquireDefaultReader(
        script_state, stream1_,
        PassThroughException(script_state->GetIsolate()));
    if (try_catch.HasCaught()) {
      return ScriptPromiseUntyped::Reject(script_state, try_catch.Exception());
    }
    DCHECK(reader_for_stream1_);
    return ToResolvedUndefinedPromise(script_state);
  }

  ScriptPromiseUntyped Pull(ScriptState* script_state,
                            ExceptionState& exception_state) override {
    if (has_finished_reading_stream1_) {
      return source2_->Pull(script_state, exception_state);
    }
    auto* promise =
        MakeGarbageCollected<ScriptPromiseResolver<IDLPromise<IDLAny>>>(
            script_state);
    auto* read_request =
        MakeGarbageCollected<ConcatenatingUnderlyingSourceReadRequest>(this,
                                                                       promise);
    ReadableStreamDefaultReader::Read(script_state, reader_for_stream1_,
                                      read_request, exception_state);
    return promise->Promise();
  }

  ScriptPromiseUntyped Cancel(ScriptState* script_state,
                              ScriptValue reason,
                              ExceptionState& exception_state) override {
    if (has_finished_reading_stream1_) {
      return source2_->Cancel(script_state, reason, exception_state);
    }
    v8::TryCatch try_catch(script_state->GetIsolate());
    ScriptPromiseUntyped cancel_promise1 = reader_for_stream1_->cancel(
        script_state, reason, PassThroughException(script_state->GetIsolate()));
    if (try_catch.HasCaught()) {
      cancel_promise1 =
          ScriptPromiseUntyped::Reject(script_state, try_catch.Exception());
    }

    ReadableStream* dummy_stream =
        ReadableStream::CreateWithCountQueueingStrategy(script_state, source2_,
                                                        /*high_water_mark=*/0);
    ScriptPromiseUntyped cancel_promise2 = dummy_stream->cancel(
        script_state, reason, PassThroughException(script_state->GetIsolate()));
    if (try_catch.HasCaught()) {
      cancel_promise2 =
          ScriptPromiseUntyped::Reject(script_state, try_catch.Exception());
    }

    return ScriptPromiseUntyped::All(script_state,
                                     {cancel_promise1, cancel_promise2});
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(stream1_);
    visitor->Trace(reader_for_stream1_);
    visitor->Trace(source2_);
    UnderlyingSourceBase::Trace(visitor);
  }

 private:
  Member<ReadableStream> stream1_;
  Member<ReadableStreamDefaultReader> reader_for_stream1_;
  bool has_finished_reading_stream1_ = false;
  Member<UnderlyingSourceBase> source2_;
};

ReadableStream* CrossRealmTransformReadable::CreateReadableStream(
    ExceptionState& exception_state) {
  DCHECK(!controller_) << "CreateReadableStream can only be called once";

  // https://streams.spec.whatwg.org/#abstract-opdef-setupcrossrealmtransformreadable
  // The order of operations is significantly different from the standard, but
  // functionally equivalent.

  //  3. Add a handler for port’s message event with the following steps:
  //  5. Enable port’s port message queue.
  message_port_->setOnmessage(
      MakeGarbageCollected<CrossRealmTransformMessageListener>(this));

  //  4. Add a handler for port’s messageerror event with the following steps:
  message_port_->setOnmessageerror(
      MakeGarbageCollected<CrossRealmTransformErrorListener>(this));

  //  6. Let startAlgorithm be an algorithm that returns undefined.
  //  7. Let pullAlgorithm be the following steps:
  //  8. Let cancelAlgorithm be the following steps, taking a reason argument:
  //  9. Let sizeAlgorithm be an algorithm that returns 1.
  // 10. Perform ! SetUpReadableStreamDefaultController(stream, controller,
  //     startAlgorithm, pullAlgorithm, cancelAlgorithm, 0, sizeAlgorithm).
  auto* stream = ReadableStream::Create(
      script_state_, CreateTrivialStartAlgorithm(),
      MakeGarbageCollected<PullAlgorithm>(this),
      MakeGarbageCollected<CancelAlgorithm>(this),
      /* highWaterMark = */ 0, CreateDefaultSizeAlgorithm(), exception_state);

  if (exception_state.HadException()) {
    return nullptr;
  }

  // The stream is created right above, and the type of the source is not given,
  // hence it is guaranteed that the controller is a
  // ReadableStreamDefaultController.
  controller_ = To<ReadableStreamDefaultController>(stream->GetController());
  return stream;
}

void CrossRealmTransformReadable::HandleMessage(MessageType type,
                                                v8::Local<v8::Value> value) {
  // https://streams.spec.whatwg.org/#abstract-opdef-setupcrossrealmtransformreadable
  // 3. Add a handler for port’s message event with the following steps:
  // The first 5 steps are handled by CrossRealmTransformMessageListener.
  switch (type) {
    // 6. If type is "chunk",
    case MessageType::kChunk:
      // 1. Perform ! ReadableStreamDefaultControllerEnqueue(controller,
      //    value).
      // TODO(ricea): Update ReadableStreamDefaultController::Enqueue() to match
      // the standard so this extra check is not needed.
      if (ReadableStreamDefaultController::CanCloseOrEnqueue(controller_)) {
        // This can't throw because we always use the default strategy size
        // algorithm, which doesn't throw, and always returns a valid value of
        // 1.0.
        ReadableStreamDefaultController::Enqueue(script_state_, controller_,
                                                 value, ASSERT_NO_EXCEPTION);
      }
      return;

    // 7. Otherwise, if type is "close",
    case MessageType::kClose:
      // 1. Perform ! ReadableStreamDefaultControllerClose(controller).
      // TODO(ricea): Update ReadableStreamDefaultController::Close() to match
      // the standard so this extra check is not needed.
      if (ReadableStreamDefaultController::CanCloseOrEnqueue(controller_)) {
        ReadableStreamDefaultController::Close(script_state_, controller_);
      }

      // Disentangle port.
      message_port_->close();
      return;

    // 8. Otherwise, if type is "error",
    case MessageType::kError:
      // 1. Perform ! ReadableStreamDefaultControllerError(controller, value).
      ReadableStreamDefaultController::Error(script_state_, controller_, value);

      // 2. Disentangle port.
      message_port_->close();
      return;

    default:
      DLOG(WARNING) << "Invalid message from peer ignored (invalid type): "
                    << static_cast<int>(type);
      return;
  }
}

void CrossRealmTransformReadable::HandleError(v8::Local<v8::Value> error) {
  // https://streams.spec.whatwg.org/#abstract-opdef-setupcrossrealmtransformreadable
  // 4. Add a handler for port’s messageerror event with the following steps:
  // The first two steps, and the last step, are performed by
  // CrossRealmTransformErrorListener.

  //   3. Perform ! ReadableStreamDefaultControllerError(controller, error).
  ReadableStreamDefaultController::Error(script_state_, controller_, error);
}

}  // namespace

CORE_EXPORT WritableStream* CreateCrossRealmTransformWritable(
    ScriptState* script_state,
    MessagePort* port,
    AllowPerChunkTransferring allow_per_chunk_transferring,
    std::unique_ptr<WritableStreamTransferringOptimizer> optimizer,
    ExceptionState& exception_state) {
  WritableStream* stream = MakeGarbageCollected<CrossRealmTransformWritable>(
                               script_state, port, allow_per_chunk_transferring)
                               ->CreateWritableStream(exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  if (!optimizer) {
    return stream;
  }
  UnderlyingSinkBase* sink =
      optimizer->PerformInProcessOptimization(script_state);
  if (!sink) {
    return stream;
  }
  stream->close(script_state, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  return WritableStream::CreateWithCountQueueingStrategy(script_state, sink,
                                                         /*high_water_mark=*/1);
}

CORE_EXPORT ReadableStream* CreateCrossRealmTransformReadable(
    ScriptState* script_state,
    MessagePort* port,
    std::unique_ptr<ReadableStreamTransferringOptimizer> optimizer,
    ExceptionState& exception_state) {
  ReadableStream* stream =
      MakeGarbageCollected<CrossRealmTransformReadable>(script_state, port)
          ->CreateReadableStream(exception_state);
  if (!optimizer) {
    return stream;
  }
  UnderlyingSourceBase* source2 =
      optimizer->PerformInProcessOptimization(script_state);
  if (!source2) {
    return stream;
  }

  return ReadableStream::CreateWithCountQueueingStrategy(
      script_state,
      MakeGarbageCollected<ConcatenatingUnderlyingSource>(script_state, stream,
                                                          source2),
      /*high_water_mark=*/0);
}

ReadableStream* CreateConcatenatedReadableStream(
    ScriptState* script_state,
    UnderlyingSourceBase* source1,
    UnderlyingSourceBase* source2) {
  auto* const stream1 =
      ReadableStream::CreateWithCountQueueingStrategy(script_state, source1,
                                                      /*high_water_mark=*/0);
  return ReadableStream::CreateWithCountQueueingStrategy(
      script_state,
      MakeGarbageCollected<ConcatenatingUnderlyingSource>(script_state, stream1,
                                                          source2),
      /*high_water_mark=*/0);
}

}  // namespace blink
