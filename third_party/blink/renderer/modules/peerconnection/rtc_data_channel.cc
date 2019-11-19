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

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_rtc_peer_connection_handler.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

namespace blink {

namespace {

enum class DataChannelCounters {
  kCreated,
  kOpened,
  kReliable,
  kOrdered,
  kNegotiated,
  kBoundary
};

void IncrementCounter(DataChannelCounters counter) {
  UMA_HISTOGRAM_ENUMERATION("WebRTC.DataChannelCounters", counter,
                            DataChannelCounters::kBoundary);
}

void IncrementCounters(const webrtc::DataChannelInterface& channel) {
  IncrementCounter(DataChannelCounters::kCreated);
  if (channel.reliable())
    IncrementCounter(DataChannelCounters::kReliable);
  if (channel.ordered())
    IncrementCounter(DataChannelCounters::kOrdered);
  if (channel.negotiated())
    IncrementCounter(DataChannelCounters::kNegotiated);

  // Only record max retransmits and max packet life time if set.
  if (channel.maxRetransmitsOpt()) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("WebRTC.DataChannelMaxRetransmits",
                                *(channel.maxRetransmitsOpt()), 1,
                                std::numeric_limits<uint16_t>::max(), 50);
  }
  if (channel.maxPacketLifeTime()) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("WebRTC.DataChannelMaxPacketLifeTime",
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
                                SafeCast<int>(num_bytes), 1, kMaxBucketSize,
                                kNumBuckets);
  } else {
    UMA_HISTOGRAM_CUSTOM_COUNTS("WebRTC.UnreliableDataChannelMessageSize",
                                SafeCast<int>(num_bytes), 1, kMaxBucketSize,
                                kNumBuckets);
  }
}

}  // namespace

static void ThrowNotOpenException(ExceptionState* exception_state) {
  exception_state->ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                     "RTCDataChannel.readyState is not 'open'");
}

static void ThrowCouldNotSendDataException(ExceptionState* exception_state) {
  exception_state->ThrowDOMException(DOMExceptionCode::kNetworkError,
                                     "Could not send data");
}

static void ThrowNoBlobSupportException(ExceptionState* exception_state) {
  exception_state->ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                     "Blob support not implemented yet");
}

RTCDataChannel::Observer::Observer(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread,
    RTCDataChannel* blink_channel,
    scoped_refptr<webrtc::DataChannelInterface> channel)
    : main_thread_(main_thread),
      blink_channel_(blink_channel),
      webrtc_channel_(channel) {}

RTCDataChannel::Observer::~Observer() {
  DCHECK(!blink_channel_) << "Reference to blink channel hasn't been released.";
  DCHECK(!webrtc_channel_.get()) << "Unregister hasn't been called.";
}

const scoped_refptr<webrtc::DataChannelInterface>&
RTCDataChannel::Observer::channel() const {
  return webrtc_channel_;
}

void RTCDataChannel::Observer::Unregister() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  blink_channel_ = nullptr;
  if (webrtc_channel_.get()) {
    webrtc_channel_->UnregisterObserver();
    // Now that we're guaranteed to not get further OnStateChange callbacks,
    // it's safe to release our reference to the channel.
    webrtc_channel_ = nullptr;
  }
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
                          SafeCast<unsigned>(sent_data_size)));
}

void RTCDataChannel::Observer::OnMessage(const webrtc::DataBuffer& buffer) {
  // TODO(tommi): Figure out a way to transfer ownership of the buffer without
  // having to create a copy.  See webrtc bug 3967.
  std::unique_ptr<webrtc::DataBuffer> new_buffer(
      new webrtc::DataBuffer(buffer));
  PostCrossThreadTask(
      *main_thread_, FROM_HERE,
      CrossThreadBindOnce(&RTCDataChannel::Observer::OnMessageImpl,
                          scoped_refptr<Observer>(this),
                          WTF::Passed(std::move(new_buffer))));
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

void RTCDataChannel::Observer::OnMessageImpl(
    std::unique_ptr<webrtc::DataBuffer> buffer) {
  DCHECK(main_thread_->BelongsToCurrentThread());
  if (blink_channel_)
    blink_channel_->OnMessage(std::move(buffer));
}

RTCDataChannel::RTCDataChannel(
    ExecutionContext* context,
    scoped_refptr<webrtc::DataChannelInterface> channel,
    WebRTCPeerConnectionHandler* peer_connection_handler)
    : ContextLifecycleObserver(context),
      state_(webrtc::DataChannelInterface::kConnecting),
      binary_type_(kBinaryTypeArrayBuffer),
      scheduled_event_timer_(context->GetTaskRunner(TaskType::kNetworking),
                             this,
                             &RTCDataChannel::ScheduledEventTimerFired),
      buffered_amount_low_threshold_(0U),
      buffered_amount_(0U),
      stopped_(false),
      observer_(base::MakeRefCounted<Observer>(
          context->GetTaskRunner(TaskType::kNetworking),
          this,
          channel)) {
  DCHECK(peer_connection_handler);

  // Register observer and get state update to make up for state change updates
  // that might have been missed between creating the webrtc::DataChannel object
  // on the signaling thread and RTCDataChannel construction posted on the main
  // thread. Done in a single synchronous call to the signaling thread to ensure
  // channel state consistency.
  peer_connection_handler->RunSynchronousOnceClosureOnSignalingThread(
      ConvertToBaseOnceCallback(CrossThreadBindOnce(
          [](scoped_refptr<RTCDataChannel::Observer> observer,
             webrtc::DataChannelInterface::DataState current_state) {
            scoped_refptr<webrtc::DataChannelInterface> channel =
                observer->channel();
            channel->RegisterObserver(observer.get());
            if (channel->state() != current_state) {
              observer->OnStateChange();
            }
          },
          observer_, state_)),
      "RegisterObserverAndGetStateUpdate");

  IncrementCounters(*channel.get());
}

RTCDataChannel::~RTCDataChannel() = default;

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

uint16_t RTCDataChannel::maxPacketLifeTime(bool& is_null) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (channel()->maxPacketLifeTime()) {
    is_null = false;
    return *(channel()->maxPacketLifeTime());
  }
  is_null = true;
  return -1;
}

uint16_t RTCDataChannel::maxRetransmits(bool& is_null) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (channel()->maxRetransmitsOpt()) {
    is_null = false;
    return *(channel()->maxRetransmitsOpt());
  }
  is_null = true;
  return -1;
}

String RTCDataChannel::protocol() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return String::FromUTF8(channel()->protocol());
}

bool RTCDataChannel::negotiated() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return channel()->negotiated();
}

uint16_t RTCDataChannel::id(bool& is_null) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (channel()->id() == -1) {
    is_null = true;
    return 0;
  }
  is_null = false;
  return channel()->id();
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

  NOTREACHED();
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

String RTCDataChannel::binaryType() const {
  switch (binary_type_) {
    case kBinaryTypeBlob:
      return "blob";
    case kBinaryTypeArrayBuffer:
      return "arraybuffer";
  }
  NOTREACHED();
  return String();
}

void RTCDataChannel::setBinaryType(const String& binary_type,
                                   ExceptionState& exception_state) {
  if (binary_type == "blob")
    ThrowNoBlobSupportException(&exception_state);
  else if (binary_type == "arraybuffer")
    binary_type_ = kBinaryTypeArrayBuffer;
  else
    exception_state.ThrowDOMException(DOMExceptionCode::kTypeMismatchError,
                                      "Unknown binary type : " + binary_type);
}

void RTCDataChannel::send(const String& data, ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != webrtc::DataChannelInterface::kOpen) {
    ThrowNotOpenException(&exception_state);
    return;
  }

  webrtc::DataBuffer data_buffer(data.Utf8());
  buffered_amount_ += data_buffer.size();
  RecordMessageSent(*channel().get(), data_buffer.size());
  if (!channel()->Send(data_buffer)) {
    // TODO(https://crbug.com/937848): Don't throw an exception if data is
    // queued.
    ThrowCouldNotSendDataException(&exception_state);
  }
}

void RTCDataChannel::send(DOMArrayBuffer* data,
                          ExceptionState& exception_state) {
  if (state_ != webrtc::DataChannelInterface::kOpen) {
    ThrowNotOpenException(&exception_state);
    return;
  }

  size_t data_length = data->ByteLengthAsSizeT();
  if (!data_length)
    return;

  buffered_amount_ += data_length;
  if (!SendRawData(static_cast<const char*>((data->Data())), data_length)) {
    // TODO(https://crbug.com/937848): Don't throw an exception if data is
    // queued.
    ThrowCouldNotSendDataException(&exception_state);
  }
}

void RTCDataChannel::send(NotShared<DOMArrayBufferView> data,
                          ExceptionState& exception_state) {
  buffered_amount_ += data.View()->byteLength();
  if (!SendRawData(static_cast<const char*>(data.View()->BaseAddress()),
                   data.View()->byteLength())) {
    // TODO(https://crbug.com/937848): Don't throw an exception if data is
    // queued.
    ThrowCouldNotSendDataException(&exception_state);
  }
}

void RTCDataChannel::send(Blob* data, ExceptionState& exception_state) {
  // FIXME: implement
  ThrowNoBlobSupportException(&exception_state);
}

void RTCDataChannel::close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (observer_)
    channel()->Close();
  // Note that even though Close() will run synchronously, the readyState has
  // not changed yet since the state changes that occurred on the signaling
  // thread have been posted to this thread and will be delivered later.
  // To work around this, we could have a nested loop here and deliver the
  // callbacks before running from this function, but doing so can cause
  // undesired side effects in webkit, so we don't, and instead rely on the
  // user of the API handling readyState notifications.
}

const AtomicString& RTCDataChannel::InterfaceName() const {
  return event_target_names::kRTCDataChannel;
}

ExecutionContext* RTCDataChannel::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

void RTCDataChannel::ContextDestroyed(ExecutionContext*) {
  Dispose();
  stopped_ = true;
  state_ = webrtc::DataChannelInterface::kClosed;
}

// ActiveScriptWrappable
bool RTCDataChannel::HasPendingActivity() const {
  if (stopped_)
    return false;

  // A RTCDataChannel object must not be garbage collected if its
  // * readyState is connecting and at least one event listener is registered
  //   for open events, message events, error events, or close events.
  // * readyState is open and at least one event listener is registered for
  //   message events, error events, or close events.
  // * readyState is closing and at least one event listener is registered for
  //   error events, or close events.
  // * underlying data transport is established and data is queued to be
  //   transmitted.
  bool has_valid_listeners = false;
  switch (state_) {
    case webrtc::DataChannelInterface::kConnecting:
      has_valid_listeners |= HasEventListeners(event_type_names::kOpen);
      FALLTHROUGH;
    case webrtc::DataChannelInterface::kOpen:
      has_valid_listeners |= HasEventListeners(event_type_names::kMessage);
      FALLTHROUGH;
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

void RTCDataChannel::Trace(Visitor* visitor) {
  visitor->Trace(scheduled_events_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

void RTCDataChannel::OnStateChange(
    webrtc::DataChannelInterface::DataState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ == webrtc::DataChannelInterface::kClosed)
    return;

  state_ = state;

  switch (state_) {
    case webrtc::DataChannelInterface::kOpen:
      IncrementCounter(DataChannelCounters::kOpened);
      ScheduleDispatchEvent(Event::Create(event_type_names::kOpen));
      break;
    case webrtc::DataChannelInterface::kClosed:
      ScheduleDispatchEvent(Event::Create(event_type_names::kClose));
      break;
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
    ScheduleDispatchEvent(Event::Create(event_type_names::kBufferedamountlow));
  }
}

void RTCDataChannel::OnMessage(std::unique_ptr<webrtc::DataBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (buffer->binary) {
    if (binary_type_ == kBinaryTypeBlob) {
      // FIXME: Implement.
      return;
    }
    if (binary_type_ == kBinaryTypeArrayBuffer) {
      DOMArrayBuffer* dom_buffer = DOMArrayBuffer::Create(
          buffer->data.cdata(), SafeCast<unsigned>(buffer->data.size()));
      ScheduleDispatchEvent(MessageEvent::Create(dom_buffer));
      return;
    }
    NOTREACHED();
  } else {
    String text =
        String::FromUTF8(buffer->data.cdata<char>(), buffer->data.size());
    if (!text) {
      LOG(ERROR) << "Failed convert received data to UTF16";
      return;
    }
    ScheduleDispatchEvent(MessageEvent::Create(text));
  }
}

void RTCDataChannel::Dispose() {
  if (stopped_)
    return;

  // Clears the weak persistent reference to this on-heap object.
  observer_->Unregister();
  observer_ = nullptr;
}

void RTCDataChannel::ScheduleDispatchEvent(Event* event) {
  scheduled_events_.push_back(event);

  if (!scheduled_event_timer_.IsActive())
    scheduled_event_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

void RTCDataChannel::ScheduledEventTimerFired(TimerBase*) {
  HeapVector<Member<Event>> events;
  events.swap(scheduled_events_);

  HeapVector<Member<Event>>::iterator it = events.begin();
  for (; it != events.end(); ++it)
    DispatchEvent(*it->Release());

  events.clear();
}

const scoped_refptr<webrtc::DataChannelInterface>& RTCDataChannel::channel()
    const {
  return observer_->channel();
}

bool RTCDataChannel::SendRawData(const char* data, size_t length) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rtc::CopyOnWriteBuffer buffer(data, length);
  webrtc::DataBuffer data_buffer(buffer, true);
  RecordMessageSent(*channel().get(), data_buffer.size());
  return channel()->Send(data_buffer);
}

}  // namespace blink
