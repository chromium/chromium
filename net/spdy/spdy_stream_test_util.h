// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_STREAM_TEST_UTIL_H_
#define NET_SPDY_SPDY_STREAM_TEST_UTIL_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/io_buffer.h"
#include "net/base/load_timing_info.h"
#include "net/base/test_completion_callback.h"
#include "net/log/net_log_source.h"
#include "net/spdy/spdy_read_queue.h"
#include "net/spdy/spdy_stream.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"

namespace net::test {

// Delegate that calls Close() on |stream_| on OnClose. Used by tests
// to make sure that such an action is harmless.
class ClosingDelegate : public SpdyStream::Delegate {
 public:
  explicit ClosingDelegate(const base::WeakPtr<SpdyStream>& stream);
  ~ClosingDelegate() override;

  // SpdyStream::Delegate implementation.
  void OnEarlyHintsReceived(const quiche::HttpHeaderBlock& headers) override;
  void OnHeadersSent() override;
  void OnHeadersReceived(
      const quiche::HttpHeaderBlock& response_headers) override;
  void OnDataReceived(std::unique_ptr<SpdyBuffer> buffer) override;
  void OnDataSent() override;
  void OnTrailers(const quiche::HttpHeaderBlock& trailers) override;
  void OnClose(int status) override;
  bool CanGreaseFrameType() const override;
  NetLogSource source_dependency() const override;

  // Returns whether or not the stream is closed.
  bool StreamIsClosed() const { return !stream_.get(); }

 private:
  base::WeakPtr<SpdyStream> stream_;
};

// Base class with shared functionality for test delegate
// implementations below.
class StreamDelegateBase : public SpdyStream::Delegate {
 public:
  explicit StreamDelegateBase(const base::WeakPtr<SpdyStream>& stream);
  ~StreamDelegateBase() override;

  void OnHeadersSent() override;
  void OnEarlyHintsReceived(const quiche::HttpHeaderBlock& headers) override;
  void OnHeadersReceived(
      const quiche::HttpHeaderBlock& response_headers) override;
  void OnDataReceived(std::unique_ptr<SpdyBuffer> buffer) override;
  void OnDataSent() override;
  void OnTrailers(const quiche::HttpHeaderBlock& trailers) override;
  void OnClose(int status) override;
  bool CanGreaseFrameType() const override;
  NetLogSource source_dependency() const override;

  // Waits for the stream to be closed and returns the status passed
  // to OnClose().
  int WaitForClose();

  // Drains all data from the underlying read queue and returns it as
  // a string.
  std::string TakeReceivedData();

  // Returns whether or not the stream is closed.
  bool StreamIsClosed() const { return !stream_.get(); }

  // Returns the stream's ID. If called when the stream is closed,
  // returns the stream's ID when it was open.
  spdy::SpdyStreamId stream_id() const { return stream_id_; }

  // Returns 103 Early Hints response headers.
  const std::vector<quiche::HttpHeaderBlock>& early_hints() const {
    return early_hints_;
  }

  std::string GetResponseHeaderValue(const std::string& name) const;
  bool send_headers_completed() const { return send_headers_completed_; }

  // Returns the load timing info on the stream. This must be called after the
  // stream is closed in order to get the up-to-date information.
  const LoadTimingInfo& GetLoadTimingInfo();

 protected:
  const base::WeakPtr<SpdyStream>& stream() { return stream_; }

 private:
  base::WeakPtr<SpdyStream> stream_;
  spdy::SpdyStreamId stream_id_ = 0;
  TestCompletionCallback callback_;
  bool send_headers_completed_ = false;
  std::vector<quiche::HttpHeaderBlock> early_hints_;
  quiche::HttpHeaderBlock response_headers_;
  SpdyReadQueue received_data_queue_;
  LoadTimingInfo load_timing_info_;
};

// Test delegate that does nothing. Used to capture data about the
// stream, e.g. its id when it was open.
class StreamDelegateDoNothing : public StreamDelegateBase {
 public:
  explicit StreamDelegateDoNothing(const base::WeakPtr<SpdyStream>& stream);
  ~StreamDelegateDoNothing() override;
};

// Test delegate that consumes data as it arrives.
class StreamDelegateConsumeData : public StreamDelegateBase {
 public:
  explicit StreamDelegateConsumeData(const base::WeakPtr<SpdyStream>& stream);
  ~StreamDelegateConsumeData() override;

  void OnDataReceived(std::unique_ptr<SpdyBuffer> buffer) override;
};

// Test delegate that sends data immediately in OnHeadersReceived().
class StreamDelegateSendImmediate : public StreamDelegateBase {
 public:
  // |data| can be NULL.
  StreamDelegateSendImmediate(const base::WeakPtr<SpdyStream>& stream,
                              std::string_view data);
  ~StreamDelegateSendImmediate() override;

  void OnHeadersReceived(
      const quiche::HttpHeaderBlock& response_headers) override;

 private:
  std::string_view data_;
};

// Test delegate that sends body data.
class StreamDelegateWithBody : public StreamDelegateBase {
 public:
  StreamDelegateWithBody(const base::WeakPtr<SpdyStream>& stream,
                         std::string_view data);
  ~StreamDelegateWithBody() override;

  void OnHeadersSent() override;

 private:
  scoped_refptr<StringIOBuffer> buf_;
};

// Test delegate that closes stream in OnHeadersReceived().
class StreamDelegateCloseOnHeaders : public StreamDelegateBase {
 public:
  explicit StreamDelegateCloseOnHeaders(
      const base::WeakPtr<SpdyStream>& stream);
  ~StreamDelegateCloseOnHeaders() override;

  void OnHeadersReceived(
      const quiche::HttpHeaderBlock& response_headers) override;
};

// Test delegate that sets a flag when EOF is detected.
class StreamDelegateDetectEOF : public StreamDelegateBase {
 public:
  explicit StreamDelegateDetectEOF(const base::WeakPtr<SpdyStream>& stream);
  ~StreamDelegateDetectEOF() override;

  void OnDataReceived(std::unique_ptr<SpdyBuffer> buffer) override;

  bool eof_detected() const { return eof_detected_; }

 private:
  bool eof_detected_ = false;
};

}  // namespace net::test

#endif  // NET_SPDY_SPDY_STREAM_TEST_UTIL_H_
