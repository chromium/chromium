// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Functions for transferable streams. See design doc
// https://docs.google.com/document/d/1_KuZzg5c3pncLJPFa8SuVm23AP4tft6mzPCL5at3I9M/edit

#include "third_party/blink/renderer/core/streams/transferable_streams.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/messaging/post_message_options.h"
#include "third_party/blink/renderer/core/streams/miscellaneous_operations.h"
#include "third_party/blink/renderer/core/streams/promise_handler.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/readable_stream_native.h"
#include "third_party/blink/renderer/core/streams/stream_algorithms.h"
#include "third_party/blink/renderer/core/streams/stream_promise_resolver.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/writable_stream_native.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
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

// These are the types of messages that are sent between peers.
enum class MessageType { kPull, kCancel, kChunk, kClose, kAbort, kError };

// These are the different ways an error reason can be encoded.
enum class ErrorType { kTypeError, kJson, kDomException, kUndefined };

bool IsATypeError(ScriptState* script_state, v8::Local<v8::Object> object) {
  // There isn't a 100% reliable way to identify a TypeError.
  return object->IsNativeError() &&
         object->GetConstructorName()
             ->Equals(script_state->GetContext(),
                      V8AtomicString(script_state->GetIsolate(), "TypeError"))
             .ToChecked();
}

bool IsADOMException(v8::Isolate* isolate, v8::Local<v8::Object> object) {
  return V8DOMException::HasInstance(object, isolate);
}

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
  static_assert(base::size(names) == base::size(values),
                "names and values arrays must be the same size");
  return v8::Object::New(isolate, v8::Null(isolate), names, values,
                         base::size(names));
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

// Send a message with type |type| and contents |value| over |port|. The type
// will be packed as a number with key "t", and the value will be packed with
// key "v".
void PackAndPostMessage(ScriptState* script_state,
                        MessagePort* port,
                        MessageType type,
                        v8::Local<v8::Value> value,
                        ExceptionState& exception_state) {
  DVLOG(3) << "PackAndPostMessage sending message type "
           << static_cast<int>(type);
  auto* isolate = script_state->GetIsolate();
  v8::Local<v8::Object> packed = CreateKeyValueObject(
      isolate, "t", v8::Number::New(isolate, static_cast<int>(type)), "v",
      value);
  port->postMessage(script_state, ScriptValue(isolate, packed),
                    PostMessageOptions::Create(), exception_state);
}

// Packs an error into an {e: number, s: string} object for transmission by
// postMessage. Serializing the resulting object should never fail.
v8::Local<v8::Object> PackErrorType(v8::Isolate* isolate,
                                    ErrorType type,
                                    v8::Local<v8::String> string) {
  auto error_as_number = v8::Number::New(isolate, static_cast<int>(type));
  return CreateKeyValueObject(isolate, "e", error_as_number, "s", string);
}

// Overload for the common case where |string| is a compile-time constant.
v8::Local<v8::Object> PackErrorType(v8::Isolate* isolate,
                                    ErrorType type,
                                    const char* string) {
  return PackErrorType(isolate, type, V8String(isolate, string));
}

// We'd like to able to transfer TypeError exceptions, but we can't, so we hack
// around it. PackReason() is guaranteed to succeed and the object produced is
// guaranteed to be serializable by postMessage(), however data may be lost. It
// is not very efficient, and has fairly arbitrary semantics.
// TODO(ricea): Replace once Errors are serializable.
v8::Local<v8::Value> PackReason(ScriptState* script_state,
                                v8::Local<v8::Value> reason) {
  auto* isolate = script_state->GetIsolate();
  auto context = script_state->GetContext();
  if (reason->IsString() || reason->IsNumber() || reason->IsBoolean()) {
    v8::TryCatch try_catch(isolate);
    v8::Local<v8::String> stringified;
    if (!v8::JSON::Stringify(context, reason).ToLocal(&stringified)) {
      return PackErrorType(isolate, ErrorType::kTypeError,
                           "Cannot transfer message");
    }

    return PackErrorType(isolate, ErrorType::kJson, stringified);
  }

  if (reason->IsNull()) {
    return PackErrorType(isolate, ErrorType::kJson, "null");
  }

  if (reason->IsFunction() || reason->IsSymbol() || !reason->IsObject()) {
    // Squash to undefined
    return PackErrorType(isolate, ErrorType::kUndefined, "");
  }

  if (IsATypeError(script_state, reason.As<v8::Object>())) {
    v8::TryCatch try_catch(isolate);
    // "message" on TypeError is a normal property, meaning that if it
    // is set, it is set on the object itself. We can take advantage of
    // this to avoid executing user JavaScript in the case when the
    // TypeError was generated internally.
    v8::Local<v8::Value> descriptor;
    if (!reason.As<v8::Object>()
             ->GetOwnPropertyDescriptor(context,
                                        V8AtomicString(isolate, "message"))
             .ToLocal(&descriptor)) {
      return PackErrorType(isolate, ErrorType::kTypeError,
                           "Cannot transfer message");
    }
    if (descriptor->IsUndefined()) {
      return PackErrorType(isolate, ErrorType::kTypeError, "");
    }
    v8::Local<v8::Value> message;
    CHECK(descriptor->IsObject());
    if (!descriptor.As<v8::Object>()
             ->Get(context, V8AtomicString(isolate, "value"))
             .ToLocal(&message)) {
      message = V8String(isolate, "Cannot transfer message");
    } else if (!message->IsString()) {
      message = V8String(isolate, "");
    }
    return PackErrorType(isolate, ErrorType::kTypeError,
                         message.As<v8::String>());
  }

  if (IsADOMException(isolate, reason.As<v8::Object>())) {
    DOMException* dom_exception =
        V8DOMException::ToImpl(reason.As<v8::Object>());
    String message = dom_exception->message();
    String name = dom_exception->name();
    v8::Local<v8::Value> packed = CreateKeyValueObject(
        isolate, "m", V8String(isolate, message), "n", V8String(isolate, name));
    // It should be impossible for this to fail, except for out-of-memory.
    v8::Local<v8::String> packed_string =
        v8::JSON::Stringify(context, packed).ToLocalChecked();
    return PackErrorType(isolate, ErrorType::kDomException, packed_string);
  }

  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Value> json;
  if (!v8::JSON::Stringify(context, reason).ToLocal(&json)) {
    return PackErrorType(isolate, ErrorType::kTypeError,
                         "Cannot transfer message");
  }

  return PackErrorType(isolate, ErrorType::kJson, json.As<v8::String>());
}

// Converts an object created by PackReason() back into a clone of the original
// object, minus any data that was discarded by PackReason().
bool UnpackReason(ScriptState* script_state,
                  v8::Local<v8::Value> packed_reason,
                  v8::Local<v8::Value>* reason) {
  // We need to be robust against malformed input because it could come from a
  // compromised renderer.
  if (!packed_reason->IsObject()) {
    DLOG(WARNING) << "packed_reason is not an object";
    return false;
  }

  v8::Local<v8::Value> encoder_value;
  v8::Local<v8::Value> string_value;
  if (!UnpackKeyValueObject(script_state, packed_reason.As<v8::Object>(), "e",
                            &encoder_value, "s", &string_value)) {
    return false;
  }

  if (!encoder_value->IsNumber()) {
    DLOG(WARNING) << "encoder_value is not a number";
    return false;
  }

  int encoder = encoder_value.As<v8::Number>()->Value();
  if (!string_value->IsString()) {
    DLOG(WARNING) << "string_value is not a string";
    return false;
  }

  v8::Local<v8::String> string = string_value.As<v8::String>();
  auto* isolate = script_state->GetIsolate();
  auto context = script_state->GetContext();
  switch (static_cast<ErrorType>(encoder)) {
    case ErrorType::kJson: {
      v8::TryCatch try_catch(isolate);
      if (!v8::JSON::Parse(context, string).ToLocal(reason)) {
        DLOG(WARNING) << "JSON Parse failed. Content: " << ToCoreString(string);
        return false;
      }
      return true;
    }

    case ErrorType::kTypeError:
      *reason = v8::Exception::TypeError(string);
      return true;

    case ErrorType::kDomException: {
      v8::TryCatch try_catch(isolate);
      v8::Local<v8::Value> packed_exception;
      if (!v8::JSON::Parse(context, string).ToLocal(&packed_exception)) {
        DLOG(WARNING) << "Packed DOMException JSON parse failed";
        return false;
      }

      if (!packed_exception->IsObject()) {
        DLOG(WARNING) << "Packed DOMException is not an object";
        return false;
      }

      v8::Local<v8::Value> message;
      v8::Local<v8::Value> name;
      if (!UnpackKeyValueObject(script_state, packed_exception.As<v8::Object>(),
                                "m", &message, "n", &name)) {
        DLOG(WARNING) << "Failed unpacking packed DOMException";
        return false;
      }

      if (!message->IsString()) {
        DLOG(WARNING) << "DOMException message is not a string";
        return false;
      }

      if (!name->IsString()) {
        DLOG(WARNING) << "DOMException name is not a string";
        return false;
      }

      auto ToBlink = [](v8::Local<v8::Value> value) {
        return ToBlinkString<String>(value.As<v8::String>(), kDoNotExternalize);
      };
      *reason = ToV8(DOMException::Create(ToBlink(message), ToBlink(name)),
                     script_state);
      return true;
    }

    case ErrorType::kUndefined:
      *reason = v8::Undefined(isolate);
      return true;

    default:
      DLOG(WARNING) << "Invalid ErrorType: " << encoder;
      return false;
  }
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

  virtual void Trace(Visitor*) {}
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
    v8::Local<v8::Value> data = message->data(script_state).V8Value();
    if (!data->IsObject()) {
      DLOG(WARNING) << "Invalid message from peer ignored (not object)";
      return;
    }

    v8::Local<v8::Value> type;
    v8::Local<v8::Value> value;
    if (!UnpackKeyValueObject(script_state, data.As<v8::Object>(), "t", &type,
                              "v", &value)) {
      DLOG(WARNING) << "Invalid message from peer ignored";
      return;
    }

    if (!type->IsNumber()) {
      DLOG(WARNING) << "Invalid message from peer ignored (type is not number)";
      return;
    }

    int type_value = type.As<v8::Number>()->Value();
    DVLOG(3) << "MessageListener saw message type " << type_value;
    target_->HandleMessage(static_cast<MessageType>(type_value), value);
  }

  void Trace(Visitor* visitor) override {
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
    const auto* error =
        DOMException::Create("chunk could not be cloned", "DataCloneError");
    auto* message_port = target_->GetMessagePort();
    v8::Local<v8::Value> error_value = ToV8(error, script_state);
    ExceptionState exception_state(script_state->GetIsolate(),
                                   ExceptionState::kUnknownContext, "", "");

    PackAndPostMessage(script_state, message_port, MessageType::kError,
                       PackReason(script_state, error_value), exception_state);
    if (exception_state.HadException()) {
      DLOG(WARNING) << "Ignoring postMessage failure in error listener";
      exception_state.ClearException();
    }

    message_port->close();
    target_->HandleError(error_value);
  }

  void Trace(Visitor* visitor) override {
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
  CrossRealmTransformWritable(ScriptState* script_state, MessagePort* port)
      : script_state_(script_state),
        message_port_(port),
        backpressure_promise_(
            MakeGarbageCollected<StreamPromiseResolver>(script_state)) {}

  WritableStreamNative* CreateWritableStream(ExceptionState&);

  ScriptState* GetScriptState() const override { return script_state_; }
  MessagePort* GetMessagePort() const override { return message_port_; }
  void HandleMessage(MessageType type, v8::Local<v8::Value> value) override;
  void HandleError(v8::Local<v8::Value> error) override;

  void Trace(Visitor* visitor) override {
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
  Member<StreamPromiseResolver> backpressure_promise_;
  Member<WritableStreamDefaultController> controller_;
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
    DCHECK_EQ(argc, 1);
    auto chunk = argv[0];

    if (!writable_->backpressure_promise_) {
      return DoWrite(script_state, chunk);
    }

    auto* isolate = script_state->GetIsolate();
    return StreamThenPromise(
        script_state->GetContext(),
        writable_->backpressure_promise_->V8Promise(isolate),
        MakeGarbageCollected<DoWriteOnResolve>(script_state, chunk, this));
  }

  void Trace(Visitor* visitor) override {
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
        : PromiseHandlerWithValue(script_state),
          chunk_(script_state->GetIsolate(), chunk),
          target_(target) {}

    v8::Local<v8::Value> CallWithLocal(v8::Local<v8::Value>) override {
      ScriptState* script_state = GetScriptState();
      return target_->DoWrite(script_state,
                              chunk_.NewLocal(script_state->GetIsolate()));
    }

    void Trace(Visitor* visitor) override {
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
    writable_->backpressure_promise_ =
        MakeGarbageCollected<StreamPromiseResolver>(script_state);
    ExceptionState exception_state(script_state->GetIsolate(),
                                   ExceptionState::kUnknownContext, "", "");
    PackAndPostMessage(script_state, writable_->message_port_,
                       MessageType::kChunk, chunk, exception_state);
    if (exception_state.HadException()) {
      auto exception = exception_state.GetException();
      exception_state.ClearException();

      PackAndPostMessage(
          script_state, writable_->message_port_, MessageType::kError,
          PackReason(writable_->script_state_, exception), exception_state);
      if (exception_state.HadException()) {
        DLOG(WARNING) << "Disregarding exception while sending error";
        exception_state.ClearException();
      }

      writable_->message_port_->close();
      return PromiseReject(script_state, exception);
    }

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
    ExceptionState exception_state(script_state->GetIsolate(),
                                   ExceptionState::kUnknownContext, "", "");
    PackAndPostMessage(
        script_state, writable_->message_port_, MessageType::kClose,
        v8::Undefined(script_state->GetIsolate()), exception_state);
    if (exception_state.HadException()) {
      DLOG(WARNING) << "Ignoring exception from PackAndPostMessage kClose";
      exception_state.ClearException();
    }

    writable_->message_port_->close();
    return PromiseResolveWithUndefined(script_state);
  }

  void Trace(Visitor* visitor) override {
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
    DCHECK_EQ(argc, 1);
    auto reason = argv[0];
    ExceptionState exception_state(script_state->GetIsolate(),
                                   ExceptionState::kUnknownContext, "", "");
    PackAndPostMessage(
        script_state, writable_->message_port_, MessageType::kAbort,
        PackReason(writable_->script_state_, reason), exception_state);
    if (exception_state.HadException()) {
      DLOG(WARNING) << "Ignoring exception from PackAndPostMessage kAbort";
      exception_state.ClearException();
    }
    writable_->message_port_->close();
    return PromiseResolveWithUndefined(script_state);
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(writable_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  const Member<CrossRealmTransformWritable> writable_;
};

WritableStreamNative* CrossRealmTransformWritable::CreateWritableStream(
    ExceptionState& exception_state) {
  DCHECK(!controller_) << "CreateWritableStream() can only be called once";

  message_port_->setOnmessage(
      MakeGarbageCollected<CrossRealmTransformMessageListener>(this));
  message_port_->setOnmessageerror(
      MakeGarbageCollected<CrossRealmTransformErrorListener>(this));

  auto* stream = WritableStreamNative::Create(
      script_state_, CreateTrivialStartAlgorithm(),
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
  switch (type) {
    case MessageType::kPull:
      DCHECK(backpressure_promise_);
      backpressure_promise_->ResolveWithUndefined(script_state_);
      backpressure_promise_ = nullptr;
      return;

    case MessageType::kCancel:
    case MessageType::kError: {
      v8::Local<v8::Value> reason;
      if (!UnpackReason(script_state_, value, &reason)) {
        DLOG(WARNING)
            << "Invalid message from peer ignored (unable to unpack value)";
        return;
      }
      WritableStreamDefaultController::ErrorIfNeeded(script_state_, controller_,
                                                     reason);
      if (backpressure_promise_) {
        backpressure_promise_->ResolveWithUndefined(script_state_);
        backpressure_promise_ = nullptr;
      }
      return;
    }

    default:
      DLOG(WARNING) << "Invalid message from peer ignored (invalid type): "
                    << static_cast<int>(type);
      return;
  }
}

void CrossRealmTransformWritable::HandleError(v8::Local<v8::Value> error) {
  WritableStreamDefaultController::ErrorIfNeeded(script_state_, controller_,
                                                 error);
}

// Class for data associated with the readable side of the cross realm transform
// stream.
class CrossRealmTransformReadable final : public CrossRealmTransformStream {
 public:
  CrossRealmTransformReadable(ScriptState* script_state, MessagePort* port)
      : script_state_(script_state),
        message_port_(port),
        backpressure_promise_(
            MakeGarbageCollected<StreamPromiseResolver>(script_state)) {}

  ReadableStreamNative* CreateReadableStream(ExceptionState&);

  ScriptState* GetScriptState() const override { return script_state_; }
  MessagePort* GetMessagePort() const override { return message_port_; }
  void HandleMessage(MessageType type, v8::Local<v8::Value> value) override;
  void HandleError(v8::Local<v8::Value> error) override;

  void Trace(Visitor* visitor) override {
    visitor->Trace(script_state_);
    visitor->Trace(message_port_);
    visitor->Trace(backpressure_promise_);
    visitor->Trace(controller_);
    CrossRealmTransformStream::Trace(visitor);
  }

 private:
  class PullAlgorithm;
  class CancelAlgorithm;

  const Member<ScriptState> script_state_;
  const Member<MessagePort> message_port_;
  Member<StreamPromiseResolver> backpressure_promise_;
  Member<ReadableStreamDefaultController> controller_;
  bool finished_ = false;
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
    ExceptionState exception_state(isolate, ExceptionState::kUnknownContext, "",
                                   "");

    PackAndPostMessage(
        script_state, readable_->message_port_, MessageType::kPull,
        v8::Undefined(script_state->GetIsolate()), exception_state);
    if (exception_state.HadException()) {
      DLOG(WARNING) << "Ignoring exception from PackAndPostMessage kClose";
      exception_state.ClearException();
    }

    return readable_->backpressure_promise_->V8Promise(isolate);
  }

  void Trace(Visitor* visitor) override {
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
    DCHECK_EQ(argc, 1);
    auto reason = argv[0];
    readable_->finished_ = true;
    ExceptionState exception_state(script_state->GetIsolate(),
                                   ExceptionState::kUnknownContext, "", "");

    PackAndPostMessage(script_state, readable_->message_port_,
                       MessageType::kCancel, PackReason(script_state, reason),
                       exception_state);
    if (exception_state.HadException()) {
      DLOG(WARNING) << "Ignoring exception from PackAndPostMessage kClose";
      exception_state.ClearException();
    }

    readable_->message_port_->close();
    return PromiseResolveWithUndefined(script_state);
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(readable_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  const Member<CrossRealmTransformReadable> readable_;
};

ReadableStreamNative* CrossRealmTransformReadable::CreateReadableStream(
    ExceptionState& exception_state) {
  DCHECK(!controller_) << "CreateReadableStream can only be called once";

  message_port_->setOnmessage(
      MakeGarbageCollected<CrossRealmTransformMessageListener>(this));
  message_port_->setOnmessageerror(
      MakeGarbageCollected<CrossRealmTransformErrorListener>(this));

  auto* stream = ReadableStreamNative::Create(
      script_state_, CreateTrivialStartAlgorithm(),
      MakeGarbageCollected<PullAlgorithm>(this),
      MakeGarbageCollected<CancelAlgorithm>(this),
      /* highWaterMark = */ 0, CreateDefaultSizeAlgorithm(), exception_state);

  if (exception_state.HadException()) {
    return nullptr;
  }

  controller_ = stream->GetController();
  return stream;
}

void CrossRealmTransformReadable::HandleMessage(MessageType type,
                                                v8::Local<v8::Value> value) {
  switch (type) {
    case MessageType::kChunk: {
      // This can't throw because we always use the default strategy size
      // algorithm, which doesn't throw, and always returns a valid value of
      // 1.0.
      ReadableStreamDefaultController::Enqueue(script_state_, controller_,
                                               value, ASSERT_NO_EXCEPTION);

      backpressure_promise_->ResolveWithUndefined(script_state_);
      backpressure_promise_ =
          MakeGarbageCollected<StreamPromiseResolver>(script_state_);
      return;
    }

    case MessageType::kClose:
      finished_ = true;
      ReadableStreamDefaultController::Close(script_state_, controller_);
      message_port_->close();
      return;

    case MessageType::kAbort:
    case MessageType::kError: {
      finished_ = true;
      v8::Local<v8::Value> reason;
      if (!UnpackReason(script_state_, value, &reason)) {
        DLOG(WARNING)
            << "Invalid message from peer ignored (unable to unpack value)";
        return;
      }

      ReadableStreamDefaultController::Error(script_state_, controller_,
                                             reason);
      message_port_->close();
      return;
    }

    default:
      DLOG(WARNING) << "Invalid message from peer ignored (invalid type): "
                    << static_cast<int>(type);
      return;
  }
}

void CrossRealmTransformReadable::HandleError(v8::Local<v8::Value> error) {
  ReadableStreamDefaultController::Error(script_state_, controller_, error);
}

}  // namespace

CORE_EXPORT WritableStreamNative* CreateCrossRealmTransformWritable(
    ScriptState* script_state,
    MessagePort* port,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<CrossRealmTransformWritable>(script_state, port)
      ->CreateWritableStream(exception_state);
}

CORE_EXPORT ReadableStreamNative* CreateCrossRealmTransformReadable(
    ScriptState* script_state,
    MessagePort* port,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<CrossRealmTransformReadable>(script_state, port)
      ->CreateReadableStream(exception_state);
}

}  // namespace blink
