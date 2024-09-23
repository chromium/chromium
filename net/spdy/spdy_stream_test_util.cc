// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_stream_test_util.h"

#include <cstddef>
#include <string_view>
#include <utility>

#include "net/spdy/spdy_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::test {

ClosingDelegate::ClosingDelegate(
    const base::WeakPtr<SpdyStream>& stream) : stream_(stream) {
  DCHECK(stream_);
}

ClosingDelegate::~ClosingDelegate() = default;

void ClosingDelegate::OnHeadersSent() {}

void ClosingDelegate::OnEarlyHintsReceived(
    const quiche::HttpHeaderBlock& headers) {}

void ClosingDelegate::OnHeadersReceived(
    const quiche::HttpHeaderBlock& response_headers) {}

void ClosingDelegate::OnDataReceived(std::unique_ptr<SpdyBuffer> buffer) {}

void ClosingDelegate::OnDataSent() {}

void ClosingDelegate::OnTrailers(const quiche::HttpHeaderBlock& trailers) {}

void ClosingDelegate::OnClose(int status) {
  DCHECK(stream_);
  stream_->Close();
  // The |stream_| may still be alive (if it is our delegate).
}

bool ClosingDelegate::CanGreaseFrameType() const {
  return false;
}

NetLogSource ClosingDelegate::source_dependency() const {
  return NetLogSource();
}

StreamDelegateBase::StreamDelegateBase(const base::WeakPtr<SpdyStream>& stream)
    : stream_(stream) {}

StreamDelegateBase::~StreamDelegateBase() = default;

void StreamDelegateBase::OnHeadersSent() {
  stream_id_ = stream_->stream_id();
  EXPECT_NE(stream_id_, 0u);
  send_headers_completed_ = true;
}

void StreamDelegateBase::OnEarlyHintsReceived(
    const quiche::HttpHeaderBlock& headers) {
  EXPECT_TRUE(send_headers_completed_);
  early_hints_.push_back(headers.Clone());
}

void StreamDelegateBase::OnHeadersReceived(
    const quiche::HttpHeaderBlock& response_headers) {
  EXPECT_TRUE(send_headers_completed_);
  response_headers_ = response_headers.Clone();
}

void StreamDelegateBase::OnDataReceived(std::unique_ptr<SpdyBuffer> buffer) {
  if (buffer)
    received_data_queue_.Enqueue(std::move(buffer));
}

void StreamDelegateBase::OnDataSent() {}

void StreamDelegateBase::OnTrailers(const quiche::HttpHeaderBlock& trailers) {}

void StreamDelegateBase::OnClose(int status) {
  if (!stream_.get())
    return;
  stream_id_ = stream_->stream_id();
  stream_->GetLoadTimingInfo(&load_timing_info_);
  stream_.reset();
  callback_.callback().Run(status);
}

bool StreamDelegateBase::CanGreaseFrameType() const {
  return false;
}

NetLogSource StreamDelegateBase::source_dependency() const {
  return NetLogSource();
}

int StreamDelegateBase::WaitForClose() {
  int result = callback_.WaitForResult();
  EXPECT_TRUE(!stream_.get());
  return result;
}

std::string StreamDelegateBase::TakeReceivedData() {
  size_t len = received_data_queue_.GetTotalSize();
  std::string received_data(len, '\0');
  if (len > 0) {
    EXPECT_EQ(len, received_data_queue_.Dequeue(std::data(received_data), len));
  }
  return received_data;
}

std::string StreamDelegateBase::GetResponseHeaderValue(
    const std::string& name) const {
  quiche::HttpHeaderBlock::const_iterator it = response_headers_.find(name);
  return (it == response_headers_.end()) ? std::string()
                                         : std::string(it->second);
}

const LoadTimingInfo& StreamDelegateBase::GetLoadTimingInfo() {
  DCHECK(StreamIsClosed());
  return load_timing_info_;
}

StreamDelegateDoNothing::StreamDelegateDoNothing(
    const base::WeakPtr<SpdyStream>& stream)
    : StreamDelegateBase(stream) {}

StreamDelegateDoNothing::~StreamDelegateDoNothing() = default;

StreamDelegateConsumeData::StreamDelegateConsumeData(
    const base::WeakPtr<SpdyStream>& stream)
    : StreamDelegateBase(stream) {}

StreamDelegateConsumeData::~StreamDelegateConsumeData() = default;

void StreamDelegateConsumeData::OnDataReceived(
    std::unique_ptr<SpdyBuffer> buffer) {
  buffer->Consume(buffer->GetRemainingSize());
}

StreamDelegateSendImmediate::StreamDelegateSendImmediate(
    const base::WeakPtr<SpdyStream>& stream,
    std::string_view data)
    : StreamDelegateBase(stream), data_(data) {}

StreamDelegateSendImmediate::~StreamDelegateSendImmediate() = default;

void StreamDelegateSendImmediate::OnHeadersReceived(
    const quiche::HttpHeaderBlock& response_headers) {
  StreamDelegateBase::OnHeadersReceived(response_headers);
  if (data_.data()) {
    scoped_refptr<StringIOBuffer> buf =
        base::MakeRefCounted<StringIOBuffer>(std::string(data_));
    stream()->SendData(buf.get(), buf->size(), MORE_DATA_TO_SEND);
  }
}

StreamDelegateWithBody::StreamDelegateWithBody(
    const base::WeakPtr<SpdyStream>& stream,
    std::string_view data)
    : StreamDelegateBase(stream),
      buf_(base::MakeRefCounted<StringIOBuffer>(std::string(data))) {}

StreamDelegateWithBody::~StreamDelegateWithBody() = default;

void StreamDelegateWithBody::OnHeadersSent() {
  StreamDelegateBase::OnHeadersSent();
  stream()->SendData(buf_.get(), buf_->size(), NO_MORE_DATA_TO_SEND);
}

StreamDelegateCloseOnHeaders::StreamDelegateCloseOnHeaders(
    const base::WeakPtr<SpdyStream>& stream)
    : StreamDelegateBase(stream) {
}

StreamDelegateCloseOnHeaders::~StreamDelegateCloseOnHeaders() = default;

void StreamDelegateCloseOnHeaders::OnHeadersReceived(
    const quiche::HttpHeaderBlock& response_headers) {
  stream()->Cancel(ERR_ABORTED);
}

StreamDelegateDetectEOF::StreamDelegateDetectEOF(
    const base::WeakPtr<SpdyStream>& stream)
    : StreamDelegateBase(stream) {}

StreamDelegateDetectEOF::~StreamDelegateDetectEOF() = default;

void StreamDelegateDetectEOF::OnDataReceived(
    std::unique_ptr<SpdyBuffer> buffer) {
  if (!buffer)
    eof_detected_ = true;
}

}  // namespace net::test
