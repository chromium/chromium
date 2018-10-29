// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/rtc_data_channel_handler.h"

#include <limits>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"

namespace content {

namespace {

enum DataChannelCounters {
  CHANNEL_CREATED,
  CHANNEL_OPENED,
  CHANNEL_RELIABLE,
  CHANNEL_ORDERED,
  CHANNEL_NEGOTIATED,
  CHANNEL_BOUNDARY
};

void IncrementCounter(DataChannelCounters counter) {
  UMA_HISTOGRAM_ENUMERATION("WebRTC.DataChannelCounters",
                            counter,
                            CHANNEL_BOUNDARY);
}

}  // namespace

// Implementation of DataChannelObserver that receives events on libjingle's
// signaling thread and forwards them over to the main thread for handling.
// Since the handler's lifetime is scoped potentially narrower than what
// the callbacks allow for, we use reference counting here to make sure
// all callbacks have a valid pointer but won't do anything if the handler
// has gone away.
RtcDataChannelHandler::Observer::Observer(
    RtcDataChannelHandler* handler,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
    webrtc::DataChannelInterface* channel)
    : handler_(handler), main_thread_(main_thread), channel_(channel) {
  channel_->RegisterObserver(this);
}

RtcDataChannelHandler::Observer::~Observer() {
  DVLOG(3) << "dtor";
  DCHECK(!channel_.get()) << "Unregister hasn't been called.";
}

const scoped_refptr<base::SingleThreadTaskRunner>&
RtcDataChannelHandler::Observer::main_thread() const {
  return main_thread_;
}

const scoped_refptr<webrtc::DataChannelInterface>&
RtcDataChannelHandler::Observer::channel() const {
  DCHECK(main_thread_->BelongsToCurrentThread());
  return channel_;
}

void RtcDataChannelHandler::Observer::Unregister() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  handler_ = nullptr;
  if (channel_.get()) {
    channel_->UnregisterObserver();
    // Now that we're guaranteed to not get further OnStateChange callbacks,
    // it's safe to release our reference to the channel.
    channel_ = nullptr;
  }
}

void RtcDataChannelHandler::Observer::OnStateChange() {
  main_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&RtcDataChannelHandler::Observer::OnStateChangeImpl, this,
                     channel_->state()));
}

void RtcDataChannelHandler::Observer::OnBufferedAmountChange(
    uint64_t previous_amount) {
  // Optimization: Only post a task if the change is a decrease, because the web
  // interface does not perform any action when there is an increase.
  if (previous_amount > channel_->buffered_amount()) {
    main_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &RtcDataChannelHandler::Observer::OnBufferedAmountDecreaseImpl,
            this, previous_amount));
  }
}

void RtcDataChannelHandler::Observer::OnMessage(
    const webrtc::DataBuffer& buffer) {
  // TODO(tommi): Figure out a way to transfer ownership of the buffer without
  // having to create a copy.  See webrtc bug 3967.
  std::unique_ptr<webrtc::DataBuffer> new_buffer(
      new webrtc::DataBuffer(buffer));
  main_thread_->PostTask(
      FROM_HERE, base::BindOnce(&RtcDataChannelHandler::Observer::OnMessageImpl,
                                this, std::move(new_buffer)));
}

void RtcDataChannelHandler::Observer::OnStateChangeImpl(
    webrtc::DataChannelInterface::DataState state) {
  DCHECK(main_thread_->BelongsToCurrentThread());
  if (handler_)
    handler_->OnStateChange(state);
}

void RtcDataChannelHandler::Observer::OnBufferedAmountDecreaseImpl(
    unsigned previous_amount) {
  DCHECK(main_thread_->BelongsToCurrentThread());
  if (handler_)
    handler_->OnBufferedAmountDecrease(previous_amount);
}

void RtcDataChannelHandler::Observer::OnMessageImpl(
    std::unique_ptr<webrtc::DataBuffer> buffer) {
  DCHECK(main_thread_->BelongsToCurrentThread());
  if (handler_)
    handler_->OnMessage(std::move(buffer));
}

RtcDataChannelHandler::RtcDataChannelHandler(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
    webrtc::DataChannelInterface* channel)
    : observer_(new Observer(this, main_thread, channel)),
      webkit_client_(nullptr) {
  DVLOG(1) << "RtcDataChannelHandler " << channel->label();

  // Detach from the ctor thread since we can be constructed on either the main
  // or signaling threads.
  DETACH_FROM_THREAD(thread_checker_);

  IncrementCounter(CHANNEL_CREATED);
  if (channel->reliable())
    IncrementCounter(CHANNEL_RELIABLE);
  if (channel->ordered())
    IncrementCounter(CHANNEL_ORDERED);
  if (channel->negotiated())
    IncrementCounter(CHANNEL_NEGOTIATED);

  UMA_HISTOGRAM_CUSTOM_COUNTS("WebRTC.DataChannelMaxRetransmits",
                              channel->maxRetransmits(), 1,
                              std::numeric_limits<unsigned short>::max(), 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("WebRTC.DataChannelMaxRetransmitTime",
                              channel->maxRetransmitTime(), 1,
                              std::numeric_limits<unsigned short>::max(), 50);
}

RtcDataChannelHandler::~RtcDataChannelHandler() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << "::dtor";
  // setClient might not have been called at all if the data channel was not
  // passed to Blink.  So, we call it here explicitly just to make sure the
  // observer gets properly unregistered.
  // See RTCPeerConnectionHandler::OnDataChannel for more.
  SetClient(nullptr);
}

void RtcDataChannelHandler::SetClient(
    blink::WebRTCDataChannelHandlerClient* client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(3) << "setClient " << client;
  webkit_client_ = client;
  if (!client && observer_.get()) {
    observer_->Unregister();
    observer_ = nullptr;
  }
}

blink::WebString RtcDataChannelHandler::Label() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return blink::WebString::FromUTF8(channel()->label());
}

bool RtcDataChannelHandler::IsReliable() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return channel()->reliable();
}

bool RtcDataChannelHandler::Ordered() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return channel()->ordered();
}

unsigned short RtcDataChannelHandler::MaxRetransmitTime() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return channel()->maxRetransmitTime();
}

unsigned short RtcDataChannelHandler::MaxRetransmits() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return channel()->maxRetransmits();
}

blink::WebString RtcDataChannelHandler::Protocol() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return blink::WebString::FromUTF8(channel()->protocol());
}

bool RtcDataChannelHandler::Negotiated() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return channel()->negotiated();
}

unsigned short RtcDataChannelHandler::Id() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return channel()->id();
}

blink::WebRTCDataChannelHandlerClient::ReadyState convertReadyState(
    webrtc::DataChannelInterface::DataState state) {
  switch (state) {
    case webrtc::DataChannelInterface::kConnecting:
      return blink::WebRTCDataChannelHandlerClient::kReadyStateConnecting;
      break;
    case webrtc::DataChannelInterface::kOpen:
      return blink::WebRTCDataChannelHandlerClient::kReadyStateOpen;
      break;
    case webrtc::DataChannelInterface::kClosing:
      return blink::WebRTCDataChannelHandlerClient::kReadyStateClosing;
      break;
    case webrtc::DataChannelInterface::kClosed:
      return blink::WebRTCDataChannelHandlerClient::kReadyStateClosed;
      break;
    default:
      NOTREACHED();
      // MSVC does not respect |NOTREACHED()|, so we need a return value.
      return blink::WebRTCDataChannelHandlerClient::kReadyStateClosed;
  }
}

blink::WebRTCDataChannelHandlerClient::ReadyState
RtcDataChannelHandler::GetState() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!observer_.get()) {
    return blink::WebRTCDataChannelHandlerClient::kReadyStateConnecting;
  } else {
    return convertReadyState(observer_->channel()->state());
  }
}

unsigned long RtcDataChannelHandler::BufferedAmount() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return channel()->buffered_amount();
}

bool RtcDataChannelHandler::SendStringData(const blink::WebString& data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::string utf8_buffer = data.Utf8();
  webrtc::DataBuffer data_buffer(utf8_buffer);
  RecordMessageSent(data_buffer.size());
  return channel()->Send(data_buffer);
}

bool RtcDataChannelHandler::SendRawData(const char* data, size_t length) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  rtc::CopyOnWriteBuffer buffer(data, length);
  webrtc::DataBuffer data_buffer(buffer, true);
  RecordMessageSent(data_buffer.size());
  return channel()->Send(data_buffer);
}

void RtcDataChannelHandler::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  channel()->Close();
  // Note that even though Close() will run synchronously, the readyState has
  // not changed yet since the state changes that occured on the signaling
  // thread have been posted to this thread and will be delivered later.
  // To work around this, we could have a nested loop here and deliver the
  // callbacks before running from this function, but doing so can cause
  // undesired side effects in webkit, so we don't, and instead rely on the
  // user of the API handling readyState notifications.
}

const scoped_refptr<webrtc::DataChannelInterface>&
RtcDataChannelHandler::channel() const {
  return observer_->channel();
}

void RtcDataChannelHandler::OnStateChange(
    webrtc::DataChannelInterface::DataState state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << "OnStateChange " << state;

  if (!webkit_client_) {
    // If this happens, the web application will not get notified of changes.
    NOTREACHED() << "WebRTCDataChannelHandlerClient not set.";
    return;
  }

  if (state == webrtc::DataChannelInterface::kOpen)
    IncrementCounter(CHANNEL_OPENED);

  webkit_client_->DidChangeReadyState(convertReadyState(state));
}

void RtcDataChannelHandler::OnBufferedAmountDecrease(
    unsigned previous_amount) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << "OnBufferedAmountDecrease " << previous_amount;

  if (!webkit_client_) {
    // If this happens, the web application will not get notified of changes.
    NOTREACHED() << "WebRTCDataChannelHandlerClient not set.";
    return;
  }

  webkit_client_->DidDecreaseBufferedAmount(previous_amount);
}

void RtcDataChannelHandler::OnMessage(
    std::unique_ptr<webrtc::DataBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!webkit_client_) {
    // If this happens, the web application will not get notified of changes.
    NOTREACHED() << "WebRTCDataChannelHandlerClient not set.";
    return;
  }

  if (buffer->binary) {
    webkit_client_->DidReceiveRawData(buffer->data.data<char>(),
                                      buffer->data.size());
  } else {
    base::string16 utf16;
    if (!base::UTF8ToUTF16(buffer->data.data<char>(), buffer->data.size(),
                           &utf16)) {
      LOG(ERROR) << "Failed convert received data to UTF16";
      return;
    }
    webkit_client_->DidReceiveStringData(blink::WebString::FromUTF16(utf16));
  }
}

void RtcDataChannelHandler::RecordMessageSent(size_t num_bytes) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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

  if (channel()->reliable()) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("WebRTC.ReliableDataChannelMessageSize",
                                num_bytes,
                                1, kMaxBucketSize, kNumBuckets);
  } else {
    UMA_HISTOGRAM_CUSTOM_COUNTS("WebRTC.UnreliableDataChannelMessageSize",
                                num_bytes,
                                1, kMaxBucketSize, kNumBuckets);
  }
}

}  // namespace content
