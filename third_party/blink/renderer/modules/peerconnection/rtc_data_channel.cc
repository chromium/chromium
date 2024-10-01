/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/peerconnection/rtc_data_channel.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/webrtc/thread_wrapper.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_client.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error_event.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_handler.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/webrtc/api/priority.h"

namespace WTF {

template <>
struct CrossThreadCopier<rtc::scoped_refptr<webrtc::DataChannelInterface>>
    : public CrossThreadCopierPassThrough<
          rtc::scoped_refptr<webrtc::DataChannelInterface>> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace blink {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DataChannelCounters {
  kCreated = 0,
  kOpened = 1,
  kReliable = 2,
  kOrdered = 3,
  kNegotiated = 4,
  kMaxValue = kNegotiated,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DataChannelAggregateType {
  kUnReliableUnordered = 0,
  kUnReliableOrdered = 1,
  kReliableUnordered = 2,
  kReliableOrdered = 3,
  kMaxValue = kReliableOrdered,
};

void IncrementCounter(DataChannelCounters counter) {
  base::UmaHistogramEnumeration("WebRTC.DataChannelCounters", counter);
}

void IncrementCounters(const webrtc::DataChannelInterface& channel) {
  int aggregate_type = 0;

  IncrementCounter(DataChannelCounters::kCreated);
  if (channel.reliable()) {
    IncrementCounter(DataChannelCounters::kReliable);
    aggregate_type += 2;
  }
  if (channel.ordered()) {
    IncrementCounter(DataChannelCounters::kOrdered);
    aggregate_type += 1;
  }
  if (channel.negotiated())
    IncrementCounter(DataChannelCounters::kNegotiated);

  base::UmaHistogramEnumeration(
      "WebRTC.DataChannelAggregateType",
      static_cast<DataChannelAggregateType>(aggregate_type));

  // Only record max retransmits and max packet life time if set.
  if (channel.maxRetransmitsOpt()) {
    base::UmaHistogramCustomCounts("WebRTC.DataChannelMaxRetransmits",
                                   *(channel.maxRetransmitsOpt()), 1,
                                   std::numeric_limits<uint16_t>::max(), 50);
  }
  if (channel.maxPacketLifeTime()) {
    base::UmaHistogramCustomCounts("WebRTC.DataChannelMaxPacketLifeTime",
                                   *channel.maxPacketLifeTime(), 1,
                                   std::numeric_limits<uint16_t>::max(), 50);
  }
}

void RecordMessageSent(const webrtc::DataChannelInterface& channel,
                       size_t num_bytes) {
  // Currently, messages are capped at some fairly low limit (16 Kb?)
  // but we may allow unlimited-size messages at some point, so making
  // the histogram maximum quite large (100 Mb) to have some
  // granularity at the higher end in that eventuality. The histogram
  // buckets are exponentially growing in size, so we'll still have
  // good granularity at the low end.

  // This makes the last bucket in the histogram count messages from
  // 100 Mb to infinity.
  const int kMaxBucketSize = 100 * 1024 * 1024;
  const int kNumBuckets = 50;

  if (channel.reliable()) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("WebRTC.ReliableDataChannelMessageSize",
                                base::checked_cast<int>(num_bytes), 1,
                                kMaxBucketSize, kNumBuckets);
  } else {
    UMA_HISTOGRAM_CUSTOM_COUNTS("WebRTC.UnreliableDataChannelMessageSize",
                                base::checked_cast<int>(num_bytes), 1,
                                kMaxBucketSize, kNumBuckets);
  }
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DataChannelSctpErrorCode {
  kUnspecified = 0,
  kInvalidStreamIdentifier = 1,
  kMissingMandatoryParameter = 2,
  kStaleCookieError = 3,
  kOutOfResource = 4,
  kUnresolvableAddress = 5,
  kUnrecognizedChunkType = 6,
  kInvalidMandatoryParameter = 7,
  kUnrecognizedParameters = 8,
  kNoUserData = 9,
  kCookieReceivedWhileShuttingDown = 10,
  kRestartWithNewAddresses = 11,
  kUserInitiatedAbort = 12,
  kProtocolViolation = 13,
  kOther = 14,
  kMaxValue = kOther,
};

void IncrementErrorCounter(const webrtc::RTCError& error) {
  DataChannelSctpErrorCode uma_code;
  auto code = error.sctp_cause_code();
  if (!code.has_value()) {
    uma_code = DataChannelSctpErrorCode::kUnspecified;
  } else if (*code >= static_cast<int>(DataChannelSctpErrorCode::kOther)) {
    uma_code = DataChannelSctpErrorCode::kOther;
  } else {
    uma_code = static_cast<DataChannelSctpErrorCode>(*code);
  }
  base::UmaHistogramEnumeration("WebRTC.DataChannelSctpErrorCode", uma_code);
}

}  // namespace

static void ThrowNotOpenException(ExceptionState* exception_state) {
  exception_state->ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                     "RTCDataChannel.readyState is not 'open'");
}

static void ThrowSendBufferFullException(ExceptionState* exception_state) {
  exception_state->ThrowDOMException(DOMExceptionCode::kOperationError,
                                     "RTCDataChannel send queue is full");
}

RTCDataChannel::Observer::Observer(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread,
    RTCDataChannel* blink_channel,
    rtc::scoped_refptr<webrtc::DataChannelInterface> channel)
    : main_thread_(main_thread),
      blink_channel_(blink_channel),
      webrtc_channel_(std::move(channel)) {
  CHECK(webrtc_channel_.get());
}

RTCDataChannel::Observer::~Observer() {
  CHECK(!is_registered()) << "Reference to blink channel hasn't been released.";
}

const rtc::scoped_refptr<webrtc::DataChannelInterface>&
RTCDataChannel::Observer::channel() const {
  return webrtc_channel_;
}

bool RTCDataChannel::Observer::is_registered() const {
  DCHECK(main_thread_->BelongsToCurrentThread());
  return blink_channel_ != nullptr;
}

void RTCDataChannel::Observer::Unregister() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  webrtc_channel_->UnregisterObserver();
  blink_channel_ = nullptr;
}

void RTCDataChannel::Observer::OnStateChange() {
  PostCrossThreadTask(
      *main_thread_, FROM_HERE,
      CrossThreadBindOnce(&RTCDataChannel::Observer::OnStateChangeImpl,
                          scoped_refptr<Observer>(this),
                          webrtc_channel_->state()));
}

void RTCDataChannel::Observer::OnBufferedAmountChange(uint64_t sent_data_size) {
  PostCrossThreadTask(
      *main_thread_, FROM_HERE,
      CrossThreadBindOnce(&RTCDataChannel::Observer::OnBufferedAmountChangeImpl,
                          scoped_refptr<Observer>(this),
                          base::checked_cast<unsigned>(sent_data_size)));
}

void RTCDataChannel::Observer::OnMessage(const webrtc::DataBuffer& buffer) {
  PostCrossThreadTask(
      *main_thread_, FROM_HERE,
      CrossThreadBindOnce(&RTCDataChannel::Observer::OnMessageImpl,
                          scoped_refptr<Observer>(this), buffer));
}

bool RTCDataChannel::Observer::IsOkToCallOnTheNetworkThread() {
  return true;
}

void RTCDataChannel::Observer::OnStateChangeImpl(
    webrtc::DataChannelInterface::DataState state) {
  DCHECK(main_thread_->BelongsToCurrentThread());
  if (blink_channel_)
    blink_channel_->OnStateChange(state);
}

void RTCDataChannel::Observer::OnBufferedAmountChangeImpl(
    unsigned sent_data_size) {
  DCHECK(main_thread_->BelongsToCurrentThread());
  if (blink_channel_)
    blink_channel_->OnBufferedAmountChange(sent_data_size);
}

void RTCDataChannel::Observer::OnMessageImpl(webrtc::DataBuffer buffer) {
  DCHECK(main_thread_->BelongsToCurrentThread());
  if (blink_channel_)
    blink_channel_->OnMessage(std::move(buffer));
}

// static
void RTCDataChannel::EnsureThreadWrappersForWorkerThread() {
  webrtc::ThreadWrapper::EnsureForCurrentMessageLoop();
  webrtc::ThreadWrapper::current()->set_send_allowed(true);
}

RTCDataChannel::RTCDataChannel(
    ExecutionContext* context,
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel)
    : ActiveScriptWrappable<RTCDataChannel>({}),
      ExecutionContextLifecycleObserver(context),
      observer_(base::MakeRefCounted<Observer>(
          context->GetTaskRunner(TaskType::kNetworking),
          this,
          std::move(data_channel))) {
  if (RuntimeEnabledFeatures::TransferableRTCDataChannelEnabled()) {
    // Delay connecting to the observer to give a chance for this RTCDataChannel
    // to be transferred. See:
    // https://w3c.github.io/webrtc-extensions/#rtcdatachannel-transferable
    context->GetTaskRunner(TaskType::kNetworking)
        ->PostTask(FROM_HERE, WTF::BindOnce(&RTCDataChannel::RegisterObserver,
                                            WrapWeakPersistent(this)));
  } else {
    RegisterObserver();
  }

  IncrementCounters(*channel().get());
}

RTCDataChannel::~RTCDataChannel() {
  // `Dispose()` must have been called to clear up webrtc references.
  CHECK(!observer_->is_registered());
}

void RTCDataChannel::RegisterObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_transferable_ = false;

  // Do not connect if `this` was transferred, as it should be going away soon.
  if (was_transferred_) {
    return;
  }

  // The context might have been destroyed already if registration was delayed.
  if (stopped_) {
    return;
  }

  channel()->RegisterObserver(observer_.get());
  if (channel()->state() != state_) {
    observer_->OnStateChange();
  }
}

String RTCDataChannel::label() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return String::FromUTF8(channel()->label());
}

bool RTCDataChannel::reliable() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return channel()->reliable();
}

bool RTCDataChannel::ordered() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return channel()->ordered();
}

std::optional<uint16_t> RTCDataChannel::maxPacketLifeTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (channel()->maxPacketLifeTime())
    return *channel()->maxPacketLifeTime();
  return std::nullopt;
}

std::optional<uint16_t> RTCDataChannel::maxRetransmits() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (channel()->maxRetransmitsOpt())
    return *channel()->maxRetransmitsOpt();
  return std::nullopt;
}

String RTCDataChannel::protocol() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return String::FromUTF8(channel()->protocol());
}

bool RTCDataChannel::negotiated() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return channel()->negotiated();
}

std::optional<uint16_t> RTCDataChannel::id() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (id_.has_value()) {
    return id_;
  }

  int id = channel()->id();
  if (id == -1) {
    return std::nullopt;
  }

  DCHECK(id >= 0 && id <= std::numeric_limits<uint16_t>::max());
  id_ = static_cast<uint16_t>(id);

  return id;
}

String RTCDataChannel::priority() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  webrtc::PriorityValue priority = channel()->priority();
  if (priority <= webrtc::PriorityValue(webrtc::Priority::kVeryLow)) {
    return "very-low";
  }
  if (priority <= webrtc::PriorityValue(webrtc::Priority::kLow)) {
    return "low";
  }
  if (priority <= webrtc::PriorityValue(webrtc::Priority::kMedium)) {
    return "medium";
  }
  return "high";
}

String RTCDataChannel::readyState() const {
  switch (state_) {
    case webrtc::DataChannelInterface::kConnecting:
      return "connecting";
    case webrtc::DataChannelInterface::kOpen:
      return "open";
    case webrtc::DataChannelInterface::kClosing:
      return "closing";
    case webrtc::DataChannelInterface::kClosed:
      return "closed";
  }

  NOTREACHED_IN_MIGRATION();
  return String();
}

unsigned RTCDataChannel::bufferedAmount() const {
  return buffered_amount_;
}

unsigned RTCDataChannel::bufferedAmountLowThreshold() const {
  return buffered_amount_low_threshold_;
}

void RTCDataChannel::setBufferedAmountLowThreshold(unsigned threshold) {
  buffered_amount_low_threshold_ = threshold;
}

V8BinaryType RTCDataChannel::binaryType() const {
  return V8BinaryType(binary_type_);
}

void RTCDataChannel::setBinaryType(const V8BinaryType& binary_type) {
  binary_type_ = binary_type.AsEnum();
}

bool RTCDataChannel::ValidateSendLength(uint64_t length,
                                        ExceptionState& exception_state) {
  // Send algorithm: https://w3c.github.io/webrtc-pc/#datachannel-send

  // TODO(orphis): Throw TypeError if length > transport.maxMessageSize

  auto updated_buffered_amount =
      base::CheckedNumeric<unsigned>(buffered_amount_) + length;
  if (!updated_buffered_amount.IsValid() ||
      updated_buffered_amount.ValueOrDie() >
          webrtc::DataChannelInterface::MaxSendQueueSize()) {
    ThrowSendBufferFullException(&exception_state);
    return false;
  }

  return true;
}

void RTCDataChannel::send(const String& data, ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_transferable_ = false;

  if (state_ != webrtc::DataChannelInterface::kOpen) {
    ThrowNotOpenException(&exception_state);
    return;
  }

  webrtc::DataBuffer data_buffer(data.Utf8());

  if (!ValidateSendLength(data_buffer.size(), exception_state))
    return;

  buffered_amount_ += data_buffer.size();
  RecordMessageSent(*channel().get(), data_buffer.size());
  SendDataBuffer(std::move(data_buffer));
}

void RTCDataChannel::send(DOMArrayBuffer* data,
                          ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_transferable_ = false;

  if (state_ != webrtc::DataChannelInterface::kOpen) {
    ThrowNotOpenException(&exception_state);
    return;
  }

  size_t data_length = data->ByteLength();

  if (!ValidateSendLength(data_length, exception_state))
    return;

  buffered_amount_ += data_length;
  SendRawData(static_cast<const char*>((data->Data())), data_length);
}

void RTCDataChannel::send(NotShared<DOMArrayBufferView> data,
                          ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_transferable_ = false;

  if (state_ != webrtc::DataChannelInterface::kOpen) {
    ThrowNotOpenException(&exception_state);
    return;
  }

  if (!ValidateSendLength(data->byteLength(), exception_state))
    return;

  buffered_amount_ += data->byteLength();
  SendRawData(static_cast<const char*>(data->BaseAddress()),
              data->byteLength());
}

void RTCDataChannel::send(Blob* data, ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_transferable_ = false;

  if (state_ != webrtc::DataChannelInterface::kOpen) {
    ThrowNotOpenException(&exception_state);
    return;
  }

  if (!ValidateSendLength(data->size(), exception_state)) {
    return;
  }

  buffered_amount_ += data->size();

  PendingMessage* message = MakeGarbageCollected<PendingMessage>();
  message->type_ = PendingMessage::Type::kBufferPending;
  message->blob_reader_ =
      BlobReader::Create(GetExecutionContext(), this, message);
  message->blob_reader_->Start(data);
  pending_messages_.push_back(message);
}

void RTCDataChannel::close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == webrtc::DataChannelInterface::kClosing ||
      state_ == webrtc::DataChannelInterface::kClosed) {
    return;
  }
  closed_from_owner_ = true;
  OnStateChange(webrtc::DataChannelInterface::kClosing);

  if (pending_messages_.empty()) {
    channel()->Close();
  } else {
    PendingMessage* message = MakeGarbageCollected<PendingMessage>();
    message->type_ = PendingMessage::Type::kCloseEvent;
    pending_messages_.push_back(message);
  }
}

bool RTCDataChannel::IsTransferable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return RuntimeEnabledFeatures::TransferableRTCDataChannelEnabled() &&
         is_transferable_;
}

rtc::scoped_refptr<webrtc::DataChannelInterface>
RTCDataChannel::TransferUnderlyingChannel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsTransferable()) {
    return nullptr;
  }

  // Only allow a single transfer.
  was_transferred_ = true;
  is_transferable_ = false;

  // Bypass OnStateChange() to avoid emitting an event.
  state_ = webrtc::DataChannelInterface::kClosed;
  feature_handle_for_scheduler_.reset();

  return channel();
}

const AtomicString& RTCDataChannel::InterfaceName() const {
  return event_target_names::kRTCDataChannel;
}

ExecutionContext* RTCDataChannel::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

void RTCDataChannel::ContextDestroyed() {
  Dispose();
  stopped_ = true;
  state_ = webrtc::DataChannelInterface::kClosed;
  feature_handle_for_scheduler_.reset();
}

// ActiveScriptWrappable
bool RTCDataChannel::HasPendingActivity() const {
  if (stopped_)
    return false;

  // A RTCDataChannel object must not be garbage collected if its
  // * readyState is connecting and at least one event listener is registered
  //   for open events, message events, error events, closing events
  //   or close events.
  // * readyState is open and at least one event listener is registered for
  //   message events, error events, closing events, or close events.
  // * readyState is closing and at least one event listener is registered for
  //   error events, or close events.
  // * underlying data transport is established and data is queued to be
  //   transmitted.
  bool has_valid_listeners = false;
  switch (state_) {
    case webrtc::DataChannelInterface::kConnecting:
      has_valid_listeners |= HasEventListeners(event_type_names::kOpen);
      [[fallthrough]];
    case webrtc::DataChannelInterface::kOpen:
      has_valid_listeners |= HasEventListeners(event_type_names::kMessage) ||
                             HasEventListeners(event_type_names::kClosing);
      [[fallthrough]];
    case webrtc::DataChannelInterface::kClosing:
      has_valid_listeners |= HasEventListeners(event_type_names::kError) ||
                             HasEventListeners(event_type_names::kClose);
      break;
    default:
      break;
  }

  if (has_valid_listeners)
    return true;

  return state_ != webrtc::DataChannelInterface::kClosed &&
         bufferedAmount() > 0;
}

void RTCDataChannel::Trace(Visitor* visitor) const {
  visitor->Trace(pending_messages_);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void RTCDataChannel::SetStateToOpenWithoutEvent() {
  DCHECK_NE(state_, webrtc::DataChannelInterface::kOpen);
  IncrementCounter(DataChannelCounters::kOpened);
  state_ = webrtc::DataChannelInterface::kOpen;
  CreateFeatureHandleForScheduler();
}

void RTCDataChannel::DispatchOpenEvent() {
  DispatchEvent(*Event::Create(event_type_names::kOpen));
}

void RTCDataChannel::OnStateChange(
    webrtc::DataChannelInterface::DataState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ == webrtc::DataChannelInterface::kClosed)
    return;

  if (state_ == webrtc::DataChannelInterface::kClosing &&
      state != webrtc::DataChannelInterface::kClosed) {
    return;
  }

  if (state == state_) {
    return;
  }

  state_ = state;

  switch (state_) {
    case webrtc::DataChannelInterface::kOpen:
      IncrementCounter(DataChannelCounters::kOpened);
      CreateFeatureHandleForScheduler();
      DispatchEvent(*Event::Create(event_type_names::kOpen));
      break;
    case webrtc::DataChannelInterface::kClosing:
      if (!closed_from_owner_) {
        DispatchEvent(*Event::Create(event_type_names::kClosing));
      }
      break;
    case webrtc::DataChannelInterface::kClosed: {
      feature_handle_for_scheduler_.reset();
      auto error = channel()->error();
      if (!error.ok()) {
        LOG(ERROR) << "DataChannel error: \"" << error.message() << "\""
                   << ", code: " << error.sctp_cause_code().value_or(-1);
        IncrementErrorCounter(error);
        DispatchEvent(*MakeGarbageCollected<RTCErrorEvent>(
            event_type_names::kError, error));
      }
      DispatchEvent(*Event::Create(event_type_names::kClose));
      break;
    }
    default:
      break;
  }
}

void RTCDataChannel::OnBufferedAmountChange(unsigned sent_data_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  unsigned previous_amount = buffered_amount_;
  DVLOG(1) << "OnBufferedAmountChange " << previous_amount;
  DCHECK_GE(buffered_amount_, sent_data_size);
  buffered_amount_ -= sent_data_size;

  if (previous_amount > buffered_amount_low_threshold_ &&
      buffered_amount_ <= buffered_amount_low_threshold_) {
    DispatchEvent(*Event::Create(event_type_names::kBufferedamountlow));
  }
}

void RTCDataChannel::OnMessage(webrtc::DataBuffer buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (buffer.binary) {
    switch (binary_type_) {
      case V8BinaryType::Enum::kBlob: {
        auto blob_data = std::make_unique<BlobData>();
        blob_data->AppendBytes(base::make_span(buffer.data));
        uint64_t blob_size = blob_data->length();
        auto* blob = MakeGarbageCollected<Blob>(
            BlobDataHandle::Create(std::move(blob_data), blob_size));
        DispatchEvent(*MessageEvent::Create(blob));
        return;
      }
      case V8BinaryType::Enum::kArraybuffer: {
        DOMArrayBuffer* dom_buffer = DOMArrayBuffer::Create(buffer.data);
        DispatchEvent(*MessageEvent::Create(dom_buffer));
        return;
      }
    }
    NOTREACHED();
  } else {
    String text =
        buffer.data.size() > 0
            ? String::FromUTF8(buffer.data.cdata<char>(), buffer.data.size())
            : g_empty_string;
    if (!text) {
      LOG(ERROR) << "Failed convert received data to UTF16";
      return;
    }
    DispatchEvent(*MessageEvent::Create(text));
  }
}

void RTCDataChannel::Dispose() {
  if (stopped_)
    return;

  // If `this` was transferred, DelayObserverRegistration() should have never
  // registered `observer_`.
  if (!was_transferred_) {
    // Clear the weak persistent reference to this on-heap object.
    observer_->Unregister();
  }
}

const rtc::scoped_refptr<webrtc::DataChannelInterface>&
RTCDataChannel::channel() const {
  return observer_->channel();
}

void RTCDataChannel::SendRawData(const char* data, size_t length) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!was_transferred_);
  rtc::CopyOnWriteBuffer buffer(data, length);
  webrtc::DataBuffer data_buffer(buffer, true);
  RecordMessageSent(*channel().get(), data_buffer.size());

  if (pending_messages_.empty()) {
    SendDataBuffer(std::move(data_buffer));
  } else {
    PendingMessage* message = MakeGarbageCollected<PendingMessage>();
    message->type_ = PendingMessage::Type::kBufferReady;
    message->buffer_ = std::move(data_buffer);
    pending_messages_.push_back(message);
  }
}

void RTCDataChannel::SendDataBuffer(webrtc::DataBuffer data_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!was_transferred_);

  // SCTP data channels queue the packet on failure and always return true, so
  // Send can be called asynchronously for them.
  channel()->SendAsync(std::move(data_buffer), [](webrtc::RTCError error) {
    // TODO(orphis): Use this callback in combination with SendAsync to report
    // completion of the send API to the JS layer.
    // The possible failures per the spec are:
    // - Channel not in open state. Although we check the state in each Send()
    // implementation, it's possible to have a short race between the WebRTC
    // state and the Chrome state, i.e. sending while a remote close event is
    // pending. In this case, it's safe to ignore send failures.
    // - Data longer than the transport maxMessageSize (not yet implemented in
    // WebRTC or Blink).
    // - Send Buffers full (buffered amount accounting in Blink layer to check
    // for it).
    if (!error.ok()) {
      // TODO(orphis): Add collect UMA stats about failure.
      // Note that when we get this callback, we're on WebRTC's network thread
      // So the callback needs to be propagated to the main (JS) thread.
      LOG(ERROR) << "Send failed" << webrtc::ToString(error.type());
    }
  });
}

void RTCDataChannel::CreateFeatureHandleForScheduler() {
  DCHECK(!feature_handle_for_scheduler_);
  LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(GetExecutionContext());
  // Ideally we'd use To<LocalDOMWindow>, but in unittests the ExecutionContext
  // may not be a LocalDOMWindow.
  if (!window)
    return;
  // This can happen for detached frames.
  if (!window->GetFrame())
    return;
  feature_handle_for_scheduler_ =
      window->GetFrame()->GetFrameScheduler()->RegisterFeature(
          SchedulingPolicy::Feature::kWebRTC,
          {SchedulingPolicy::DisableAggressiveThrottling(),
           SchedulingPolicy::DisableAlignWakeUps()});
}

void RTCDataChannel::ProcessSendQueue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool stop_processing = false;
  while (!pending_messages_.empty() && !stop_processing) {
    auto& message = pending_messages_.front();
    switch (message->type_) {
      case PendingMessage::Type::kBufferReady:
        SendDataBuffer(std::move(*message->buffer_));
        pending_messages_.pop_front();
        break;
      case PendingMessage::Type::kBufferPending:
        if (message->blob_reader_->HasFinishedLoading()) {
          SendDataBuffer(std::move(*message->buffer_));
          pending_messages_.pop_front();
        } else {
          stop_processing = true;
        }
        break;
      case PendingMessage::Type::kCloseEvent:
        channel()->Close();
        pending_messages_.pop_front();
        break;
      case PendingMessage::Type::kBlobFailure:
        pending_messages_.pop_front();
        break;
    }
  }
}

void RTCDataChannel::PendingMessage::Trace(Visitor* visitor) const {
  visitor->Trace(blob_reader_);
}

void RTCDataChannel::BlobReader::DidFinishLoading(FileReaderData data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DOMArrayBuffer* array_buffer = std::move(data).AsDOMArrayBuffer();
  rtc::CopyOnWriteBuffer buffer(
      static_cast<const char*>((array_buffer->Data())),
      array_buffer->ByteLength());
  message_->buffer_ = webrtc::DataBuffer(buffer, true);
  message_->type_ = RTCDataChannel::PendingMessage::Type::kBufferReady;
  data_channel_->ProcessSendQueue();
}

void RTCDataChannel::BlobReader::DidFail(FileErrorCode error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetExecutionContext()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kJavaScript,
      mojom::ConsoleMessageLevel::kError,
      "Couldn't read Blob content, skipping message."));
  message_->type_ = RTCDataChannel::PendingMessage::Type::kBlobFailure;
  data_channel_->ProcessSendQueue();
}

RTCDataChannel::BlobReader::BlobReader(ExecutionContext* context,
                                       RTCDataChannel* data_channel,
                                       PendingMessage* message)
    : ExecutionContextLifecycleObserver(context),
      loader_(MakeGarbageCollected<FileReaderLoader>(
          this,
          GetExecutionContext()->GetTaskRunner(TaskType::kFileReading))),
      data_channel_(data_channel),
      message_(message) {}

RTCDataChannel::BlobReader::~BlobReader() = default;

void RTCDataChannel::BlobReader::Start(Blob* blob) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  loader_->Start(blob->GetBlobDataHandle());
}

void RTCDataChannel::BlobReader::Trace(Visitor* visitor) const {
  ExecutionContextLifecycleObserver::Trace(visitor);
  FileReaderAccumulator::Trace(visitor);
  visitor->Trace(loader_);
  visitor->Trace(data_channel_);
  visitor->Trace(message_);
}

bool RTCDataChannel::BlobReader::HasFinishedLoading() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return loader_->HasFinishedLoading();
}

void RTCDataChannel::BlobReader::ContextDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  loader_->Cancel();
}

}  // namespace blink
