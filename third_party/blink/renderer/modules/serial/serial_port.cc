// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/serial/serial_port.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_unsignedlong.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_serial_input_signals.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_serial_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_serial_output_signals.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_serial_port_info.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"
#include "third_party/blink/renderer/modules/serial/serial.h"
#include "third_party/blink/renderer/modules/serial/serial_port_underlying_sink.h"
#include "third_party/blink/renderer/modules/serial/serial_port_underlying_source.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

using ::device::mojom::blink::SerialReceiveError;
using ::device::mojom::blink::SerialSendError;

const char kResourcesExhaustedReadBuffer[] =
    "Resources exhausted allocating read buffer.";
const char kResourcesExhaustedWriteBuffer[] =
    "Resources exhausted allocation write buffer.";
const char kNoSignals[] =
    "Signals dictionary must contain at least one member.";
const char kPortClosed[] = "The port is closed.";
const char kOpenError[] = "Failed to open serial port.";
const char kDeviceLostError[] = "The device has been lost.";
const int kMaxBufferSize = 16 * 1024 * 1024; /* 16 MiB */

bool SendErrorIsFatal(SerialSendError error) {
  switch (error) {
    case SerialSendError::NONE:
      NOTREACHED_IN_MIGRATION();
      return false;
    case SerialSendError::SYSTEM_ERROR:
      return false;
    case SerialSendError::DISCONNECTED:
      return true;
  }
}

bool ReceiveErrorIsFatal(SerialReceiveError error) {
  switch (error) {
    case SerialReceiveError::NONE:
      NOTREACHED_IN_MIGRATION();
      return false;
    case SerialReceiveError::BREAK:
    case SerialReceiveError::FRAME_ERROR:
    case SerialReceiveError::OVERRUN:
    case SerialReceiveError::BUFFER_OVERFLOW:
    case SerialReceiveError::PARITY_ERROR:
    case SerialReceiveError::SYSTEM_ERROR:
      return false;
    case SerialReceiveError::DISCONNECTED:
    case SerialReceiveError::DEVICE_LOST:
      return true;
  }
}

}  // namespace

SerialPort::SerialPort(Serial* parent, mojom::blink::SerialPortInfoPtr info)
    : ActiveScriptWrappable<SerialPort>({}),
      info_(std::move(info)),
      connected_(info_->connected),
      parent_(parent),
      port_(parent->GetExecutionContext()),
      client_receiver_(this, parent->GetExecutionContext()) {}

SerialPort::~SerialPort() = default;

SerialPortInfo* SerialPort::getInfo() {
  auto* info = MakeGarbageCollected<SerialPortInfo>();
  if (info_->has_usb_vendor_id)
    info->setUsbVendorId(info_->usb_vendor_id);
  if (info_->has_usb_product_id)
    info->setUsbProductId(info_->usb_product_id);
  if (RuntimeEnabledFeatures::WebSerialBluetoothEnabled() &&
      info_->bluetooth_service_class_id) {
    info->setBluetoothServiceClassId(
        MakeGarbageCollected<V8UnionStringOrUnsignedLong>(
            info_->bluetooth_service_class_id->uuid));
  }
  return info;
}

ScriptPromise<IDLUndefined> SerialPort::open(ScriptState* script_state,
                                             const SerialOptions* options,
                                             ExceptionState& exception_state) {
  if (!GetExecutionContext()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Script context has shut down.");
    return EmptyPromise();
  }

  if (open_resolver_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "A call to open() is already in progress.");
    return EmptyPromise();
  }

  if (port_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The port is already open.");
    return EmptyPromise();
  }

  auto mojo_options = device::mojom::blink::SerialConnectionOptions::New();

  if (options->baudRate() == 0) {
    exception_state.ThrowTypeError(
        "Requested baud rate must be greater than zero.");
    return EmptyPromise();
  }
  mojo_options->bitrate = options->baudRate();

  switch (options->dataBits()) {
    case 7:
      mojo_options->data_bits = device::mojom::blink::SerialDataBits::SEVEN;
      break;
    case 8:
      mojo_options->data_bits = device::mojom::blink::SerialDataBits::EIGHT;
      break;
    default:
      exception_state.ThrowTypeError(
          "Requested number of data bits must be 7 or 8.");
      return EmptyPromise();
  }

  if (options->parity() == "none") {
    mojo_options->parity_bit = device::mojom::blink::SerialParityBit::NO_PARITY;
  } else if (options->parity() == "even") {
    mojo_options->parity_bit = device::mojom::blink::SerialParityBit::EVEN;
  } else if (options->parity() == "odd") {
    mojo_options->parity_bit = device::mojom::blink::SerialParityBit::ODD;
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  switch (options->stopBits()) {
    case 1:
      mojo_options->stop_bits = device::mojom::blink::SerialStopBits::ONE;
      break;
    case 2:
      mojo_options->stop_bits = device::mojom::blink::SerialStopBits::TWO;
      break;
    default:
      exception_state.ThrowTypeError(
          "Requested number of stop bits must be 1 or 2.");
      return EmptyPromise();
  }

  if (options->bufferSize() == 0) {
    exception_state.ThrowTypeError(String::Format(
        "Requested buffer size (%d bytes) must be greater than zero.",
        options->bufferSize()));
    return EmptyPromise();
  }

  if (options->bufferSize() > kMaxBufferSize) {
    exception_state.ThrowTypeError(
        String::Format("Requested buffer size (%d bytes) is greater than "
                       "the maximum allowed (%d bytes).",
                       options->bufferSize(), kMaxBufferSize));
    return EmptyPromise();
  }
  buffer_size_ = options->bufferSize();

  hardware_flow_control_ = options->flowControl() == "hardware";
  mojo_options->has_cts_flow_control = true;
  mojo_options->cts_flow_control = hardware_flow_control_;

  mojo::PendingRemote<device::mojom::blink::SerialPortClient> client;
  open_resolver_ = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto callback = WTF::BindOnce(&SerialPort::OnOpen, WrapPersistent(this),
                                client.InitWithNewPipeAndPassReceiver());

  parent_->OpenPort(info_->token, std::move(mojo_options), std::move(client),
                    std::move(callback));

  return open_resolver_->Promise();
}

ReadableStream* SerialPort::readable(ScriptState* script_state,
                                     ExceptionState& exception_state) {
  if (readable_)
    return readable_.Get();

  if (!port_.is_bound() || open_resolver_ || IsClosing() || read_fatal_)
    return nullptr;

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  if (!CreateDataPipe(&producer, &consumer)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kQuotaExceededError,
                                      kResourcesExhaustedReadBuffer);
    return nullptr;
  }

  port_->StartReading(std::move(producer));

  DCHECK(!underlying_source_);
  underlying_source_ = MakeGarbageCollected<SerialPortUnderlyingSource>(
      script_state, this, std::move(consumer));
  readable_ =
      ReadableStream::CreateByteStream(script_state, underlying_source_);
  return readable_.Get();
}

WritableStream* SerialPort::writable(ScriptState* script_state,
                                     ExceptionState& exception_state) {
  if (writable_)
    return writable_.Get();

  if (!port_.is_bound() || open_resolver_ || IsClosing() || write_fatal_)
    return nullptr;

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  if (!CreateDataPipe(&producer, &consumer)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kQuotaExceededError,
                                      kResourcesExhaustedWriteBuffer);
    return nullptr;
  }

  port_->StartWriting(std::move(consumer));

  DCHECK(!underlying_sink_);
  underlying_sink_ =
      MakeGarbageCollected<SerialPortUnderlyingSink>(this, std::move(producer));
  // Ideally the stream would report the number of bytes that could be written
  // to the underlying Mojo data pipe. As an approximation the high water mark
  // is set to 1 so that the stream appears ready but producers observing
  // backpressure won't queue additional chunks in the stream and thus add an
  // extra layer of buffering.
  writable_ = WritableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_sink_, /*high_water_mark=*/1);
  return writable_.Get();
}

ScriptPromise<SerialInputSignals> SerialPort::getSignals(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!port_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kPortClosed);
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<SerialInputSignals>>(
          script_state, exception_state.GetContext());
  signal_resolvers_.insert(resolver);
  port_->GetControlSignals(resolver->WrapCallbackInScriptScope(
      WTF::BindOnce(&SerialPort::OnGetSignals, WrapPersistent(this))));
  return resolver->Promise();
}

ScriptPromise<IDLUndefined> SerialPort::setSignals(
    ScriptState* script_state,
    const SerialOutputSignals* signals,
    ExceptionState& exception_state) {
  ExecutionContext* context = GetExecutionContext();
  if (!context) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Script context has shut down.");
    return EmptyPromise();
  }

  if (!port_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kPortClosed);
    return EmptyPromise();
  }

  if (!signals->hasDataTerminalReady() && !signals->hasRequestToSend() &&
      !signals->hasBrk()) {
    exception_state.ThrowTypeError(kNoSignals);
    return EmptyPromise();
  }

  auto mojo_signals = device::mojom::blink::SerialHostControlSignals::New();
  if (signals->hasDataTerminalReady()) {
    mojo_signals->has_dtr = true;
    mojo_signals->dtr = signals->dataTerminalReady();
  }
  if (signals->hasRequestToSend()) {
    mojo_signals->has_rts = true;
    mojo_signals->rts = signals->requestToSend();

    if (hardware_flow_control_) {
      // This combination may be deprecated in the future but generate a console
      // warning for now: https://github.com/WICG/serial/issues/158
      context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kRecommendation,
          mojom::blink::ConsoleMessageLevel::kInfo,
          "The RTS (request to send) signal should not be configured manually "
          "when using hardware flow control. This combination may not be "
          "supported on all platforms."));
    }
  }
  if (signals->hasBrk()) {
    mojo_signals->has_brk = true;
    mojo_signals->brk = signals->brk();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  signal_resolvers_.insert(resolver);
  port_->SetControlSignals(
      std::move(mojo_signals),
      resolver->WrapCallbackInScriptScope(
          WTF::BindOnce(&SerialPort::OnSetSignals, WrapPersistent(this))));
  return resolver->Promise();
}

ScriptPromise<IDLUndefined> SerialPort::close(ScriptState* script_state,
                                              ExceptionState& exception_state) {
  if (!port_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The port is already closed.");
    return EmptyPromise();
  }

  if (IsClosing()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "A call to close() is already in progress.");
    return EmptyPromise();
  }

  close_resolver_ = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = close_resolver_->Promise();

  if (!readable_ && !writable_) {
    StreamsClosed();
    return promise;
  }

  if (readable_) {
    readable_->cancel(script_state, exception_state);
    if (exception_state.HadException()) {
      AbortClose();
      return EmptyPromise();
    }
  }
  if (writable_) {
    ScriptValue reason(script_state->GetIsolate(),
                       V8ThrowDOMException::CreateOrDie(
                           script_state->GetIsolate(),
                           DOMExceptionCode::kInvalidStateError, kPortClosed));
    writable_->abort(script_state, reason, exception_state);
    if (exception_state.HadException()) {
      AbortClose();
      return EmptyPromise();
    }
  }

  return promise;
}

ScriptPromise<IDLUndefined> SerialPort::forget(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  ExecutionContext* context = GetExecutionContext();
  if (!context) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Script context has shut down.");
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  parent_->ForgetPort(info_->token,
                      WTF::BindOnce(
                          [](ScriptPromiseResolver<IDLUndefined>* resolver) {
                            resolver->Resolve();
                          },
                          WrapPersistent(resolver)));

  return resolver->Promise();
}

void SerialPort::AbortClose() {
  DCHECK(IsClosing());
  // Dropping |close_resolver_| is okay because the Promise it is attached to
  // won't be returned to script in this case.
  close_resolver_ = nullptr;
}

void SerialPort::StreamsClosed() {
  DCHECK(!readable_);
  DCHECK(!writable_);
  DCHECK(IsClosing());

  port_->Close(/*flush=*/true,
               WTF::BindOnce(&SerialPort::OnClose, WrapPersistent(this)));
}

void SerialPort::Flush(
    device::mojom::blink::SerialPortFlushMode mode,
    device::mojom::blink::SerialPort::FlushCallback callback) {
  DCHECK(port_.is_bound());
  port_->Flush(mode, std::move(callback));
}

void SerialPort::Drain(
    device::mojom::blink::SerialPort::DrainCallback callback) {
  DCHECK(port_.is_bound());
  port_->Drain(std::move(callback));
}

void SerialPort::UnderlyingSourceClosed() {
  DCHECK(readable_);
  readable_ = nullptr;
  underlying_source_ = nullptr;

  if (IsClosing() && !writable_) {
    StreamsClosed();
  }
}

void SerialPort::UnderlyingSinkClosed() {
  DCHECK(writable_);
  writable_ = nullptr;
  underlying_sink_ = nullptr;

  if (IsClosing() && !readable_) {
    StreamsClosed();
  }
}

void SerialPort::ContextDestroyed() {
  // Release connection-related resources as quickly as possible.
  port_.reset();
}

void SerialPort::Trace(Visitor* visitor) const {
  visitor->Trace(parent_);
  visitor->Trace(port_);
  visitor->Trace(client_receiver_);
  visitor->Trace(readable_);
  visitor->Trace(underlying_source_);
  visitor->Trace(writable_);
  visitor->Trace(underlying_sink_);
  visitor->Trace(open_resolver_);
  visitor->Trace(signal_resolvers_);
  visitor->Trace(close_resolver_);
  EventTarget::Trace(visitor);
  ActiveScriptWrappable<SerialPort>::Trace(visitor);
}

bool SerialPort::HasPendingActivity() const {
  // There is no need to check if the execution context has been destroyed, this
  // is handled by the common tracing logic.
  //
  // This object should be considered active as long as it is open so that any
  // chain of streams originating from this port are not closed prematurely.
  return port_.is_bound();
}

ExecutionContext* SerialPort::GetExecutionContext() const {
  return parent_->GetExecutionContext();
}

const AtomicString& SerialPort::InterfaceName() const {
  return event_target_names::kSerialPort;
}

DispatchEventResult SerialPort::DispatchEventInternal(Event& event) {
  event.SetTarget(this);

  // Events fired on a SerialPort instance bubble to the parent Serial instance.
  event.SetEventPhase(Event::PhaseType::kCapturingPhase);
  event.SetCurrentTarget(parent_);
  parent_->FireEventListeners(event);
  if (event.PropagationStopped())
    goto doneDispatching;

  event.SetEventPhase(Event::PhaseType::kAtTarget);
  event.SetCurrentTarget(this);
  FireEventListeners(event);
  if (event.PropagationStopped() || !event.bubbles())
    goto doneDispatching;

  event.SetEventPhase(Event::PhaseType::kBubblingPhase);
  event.SetCurrentTarget(parent_);
  parent_->FireEventListeners(event);

doneDispatching:
  event.SetCurrentTarget(nullptr);
  event.SetEventPhase(Event::PhaseType::kNone);
  return EventTarget::GetDispatchEventResult(event);
}

void SerialPort::OnReadError(device::mojom::blink::SerialReceiveError error) {
  if (ReceiveErrorIsFatal(error))
    read_fatal_ = true;
  if (underlying_source_)
    underlying_source_->SignalErrorOnClose(error);
}

void SerialPort::OnSendError(device::mojom::blink::SerialSendError error) {
  if (SendErrorIsFatal(error))
    write_fatal_ = true;
  if (underlying_sink_)
    underlying_sink_->SignalError(error);
}

bool SerialPort::CreateDataPipe(mojo::ScopedDataPipeProducerHandle* producer,
                                mojo::ScopedDataPipeConsumerHandle* consumer) {
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = buffer_size_;

  MojoResult result = mojo::CreateDataPipe(&options, *producer, *consumer);
  if (result == MOJO_RESULT_OK)
    return true;

  DCHECK_EQ(result, MOJO_RESULT_RESOURCE_EXHAUSTED);
  return false;
}

void SerialPort::OnConnectionError() {
  read_fatal_ = false;
  write_fatal_ = false;
  port_.reset();
  client_receiver_.reset();

  if (open_resolver_) {
    ScriptState* script_state = open_resolver_->GetScriptState();
    if (IsInParallelAlgorithmRunnable(open_resolver_->GetExecutionContext(),
                                      script_state)) {
      ScriptState::Scope script_state_scope(script_state);
      open_resolver_->RejectWithDOMException(DOMExceptionCode::kNetworkError,
                                             kOpenError);
      open_resolver_ = nullptr;
    }
  }

  for (auto& resolver : signal_resolvers_) {
    ScriptState* script_state = resolver->GetScriptState();
    if (IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                      script_state)) {
      ScriptState::Scope script_state_scope(script_state);
      resolver->RejectWithDOMException(DOMExceptionCode::kNetworkError,
                                       kDeviceLostError);
    }
  }
  signal_resolvers_.clear();

  if (IsClosing()) {
    close_resolver_->Resolve();
    close_resolver_ = nullptr;
  }

  if (underlying_source_)
    underlying_source_->SignalErrorOnClose(SerialReceiveError::DISCONNECTED);

  if (underlying_sink_)
    underlying_sink_->SignalError(SerialSendError::DISCONNECTED);
}

void SerialPort::OnOpen(
    mojo::PendingReceiver<device::mojom::blink::SerialPortClient>
        client_receiver,
    mojo::PendingRemote<device::mojom::blink::SerialPort> port) {
  if (!port) {
    open_resolver_->RejectWithDOMException(DOMExceptionCode::kNetworkError,
                                           kOpenError);
    open_resolver_ = nullptr;
    return;
  }

  auto* execution_context = GetExecutionContext();
  feature_handle_for_scheduler_ =
      execution_context->GetScheduler()->RegisterFeature(
          SchedulingPolicy::Feature::kWebSerial,
          SchedulingPolicy{SchedulingPolicy::DisableAggressiveThrottling()});

  port_.Bind(std::move(port),
             execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI));
  port_.set_disconnect_handler(
      WTF::BindOnce(&SerialPort::OnConnectionError, WrapWeakPersistent(this)));
  client_receiver_.Bind(
      std::move(client_receiver),
      execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI));

  open_resolver_->Resolve();
  open_resolver_ = nullptr;
}

void SerialPort::OnGetSignals(
    ScriptPromiseResolver<SerialInputSignals>* resolver,
    device::mojom::blink::SerialPortControlSignalsPtr mojo_signals) {
  DCHECK(signal_resolvers_.Contains(resolver));
  signal_resolvers_.erase(resolver);

  if (!mojo_signals) {
    resolver->RejectWithDOMException(DOMExceptionCode::kNetworkError,
                                     "Failed to get control signals.");
    return;
  }

  auto* signals = MakeGarbageCollected<SerialInputSignals>();
  signals->setDataCarrierDetect(mojo_signals->dcd);
  signals->setClearToSend(mojo_signals->cts);
  signals->setRingIndicator(mojo_signals->ri);
  signals->setDataSetReady(mojo_signals->dsr);
  resolver->Resolve(signals);
}

void SerialPort::OnSetSignals(ScriptPromiseResolver<IDLUndefined>* resolver,
                              bool success) {
  DCHECK(signal_resolvers_.Contains(resolver));
  signal_resolvers_.erase(resolver);

  if (!success) {
    resolver->RejectWithDOMException(DOMExceptionCode::kNetworkError,
                                     "Failed to set control signals.");
    return;
  }

  resolver->Resolve();
}

void SerialPort::OnClose() {
  read_fatal_ = false;
  write_fatal_ = false;
  port_.reset();
  client_receiver_.reset();

  DCHECK(IsClosing());
  close_resolver_->Resolve();
  close_resolver_ = nullptr;
  feature_handle_for_scheduler_.reset();
}

}  // namespace blink
