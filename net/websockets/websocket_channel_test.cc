// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_channel.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include <algorithm>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/log/net_log_with_source.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/url_request_context.h"
#include "net/websockets/websocket_errors.h"
#include "net/websockets/websocket_event_interface.h"
#include "net/websockets/websocket_handshake_request_info.h"
#include "net/websockets/websocket_handshake_response_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

// Hacky macros to construct the body of a Close message from a code and a
// string, while ensuring the result is a compile-time constant string.
// Use like CLOSE_DATA(NORMAL_CLOSURE, "Explanation String")
#define CLOSE_DATA(code, string) WEBSOCKET_CLOSE_CODE_AS_STRING_##code string
#define WEBSOCKET_CLOSE_CODE_AS_STRING_NORMAL_CLOSURE "\x03\xe8"
#define WEBSOCKET_CLOSE_CODE_AS_STRING_GOING_AWAY "\x03\xe9"
#define WEBSOCKET_CLOSE_CODE_AS_STRING_PROTOCOL_ERROR "\x03\xea"
#define WEBSOCKET_CLOSE_CODE_AS_STRING_ABNORMAL_CLOSURE "\x03\xee"
#define WEBSOCKET_CLOSE_CODE_AS_STRING_SERVER_ERROR "\x03\xf3"

namespace net {

class WebSocketBasicHandshakeStream;
class WebSocketHttp2HandshakeStream;

// Printing helpers to allow GoogleMock to print frames. These are explicitly
// designed to look like the static initialisation format we use in these
// tests. They have to live in the net namespace in order to be found by
// GoogleMock; a nested anonymous namespace will not work.

std::ostream& operator<<(std::ostream& os, const WebSocketFrameHeader& header) {
  return os << (header.final ? "FINAL_FRAME" : "NOT_FINAL_FRAME") << ", "
            << header.opcode << ", "
            << (header.masked ? "MASKED" : "NOT_MASKED");
}

std::ostream& operator<<(std::ostream& os, const WebSocketFrame& frame) {
  os << "{" << frame.header << ", ";
  if (frame.payload) {
    return os << "\""
              << base::StringPiece(frame.payload, frame.header.payload_length)
              << "\"}";
  }
  return os << "NULL}";
}

std::ostream& operator<<(
    std::ostream& os,
    const std::vector<std::unique_ptr<WebSocketFrame>>& frames) {
  os << "{";
  bool first = true;
  for (const auto& frame : frames) {
    if (!first) {
      os << ",\n";
    } else {
      first = false;
    }
    os << *frame;
  }
  return os << "}";
}

std::ostream& operator<<(
    std::ostream& os,
    const std::vector<std::unique_ptr<WebSocketFrame>>* vector) {
  return os << '&' << *vector;
}

namespace {

using ::base::TimeDelta;

using ::testing::AnyNumber;
using ::testing::DefaultValue;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::_;

// A selection of characters that have traditionally been mangled in some
// environment or other, for testing 8-bit cleanliness.
const char kBinaryBlob[] = {'\n',   '\r',    // BACKWARDS CRNL
                            '\0',            // nul
                            '\x7F',          // DEL
                            '\x80', '\xFF',  // NOT VALID UTF-8
                            '\x1A',          // Control-Z, EOF on DOS
                            '\x03',          // Control-C
                            '\x04',          // EOT, special for Unix terms
                            '\x1B',          // ESC, often special
                            '\b',            // backspace
                            '\'',            // single-quote, special in PHP
};
const size_t kBinaryBlobSize = base::size(kBinaryBlob);

// The amount of quota a new connection gets by default.
// TODO(ricea): If kDefaultSendQuotaHighWaterMark changes, then this value will
// need to be updated.
const size_t kDefaultInitialQuota = 1 << 17;
// The amount of bytes we need to send after the initial connection to trigger a
// quota refresh. TODO(ricea): Change this if kDefaultSendQuotaHighWaterMark or
// kDefaultSendQuotaLowWaterMark change.
const size_t kDefaultQuotaRefreshTrigger = (1 << 16) + 1;

const int kVeryBigTimeoutMillis = 60 * 60 * 24 * 1000;

// TestTimeouts::tiny_timeout() is 100ms! I could run halfway around the world
// in that time! I would like my tests to run a bit quicker.
const int kVeryTinyTimeoutMillis = 1;

using ChannelState = WebSocketChannel::ChannelState;
constexpr ChannelState CHANNEL_ALIVE = WebSocketChannel::CHANNEL_ALIVE;
constexpr ChannelState CHANNEL_DELETED = WebSocketChannel::CHANNEL_DELETED;

// This typedef mainly exists to avoid having to repeat the "NOLINT" incantation
// all over the place.
typedef StrictMock< MockFunction<void(int)> > Checkpoint;  // NOLINT

// This mock is for testing expectations about how the EventInterface is used.
class MockWebSocketEventInterface : public WebSocketEventInterface {
 public:
  MockWebSocketEventInterface() = default;

  void OnDataFrame(bool fin,
                   WebSocketMessageType type,
                   base::span<const char> payload) override {
    return OnDataFrameVector(fin, type,
                             std::vector<char>(payload.begin(), payload.end()));
  }

  MOCK_METHOD1(OnCreateURLRequest, void(URLRequest*));
  MOCK_METHOD3(OnAddChannelResponse,
               void(const std::string&,
                    const std::string&,
                    int64_t));  // NOLINT
  MOCK_METHOD3(OnDataFrameVector,
               void(bool,
                    WebSocketMessageType,
                    const std::vector<char>&));           // NOLINT
  MOCK_METHOD0(HasPendingDataFrames, bool(void));         // NOLINT
  MOCK_METHOD1(OnSendFlowControlQuotaAdded, void(int64_t));  // NOLINT
  MOCK_METHOD0(OnClosingHandshake, void(void));           // NOLINT
  MOCK_METHOD1(OnFailChannel, void(const std::string&));  // NOLINT
  MOCK_METHOD3(OnDropChannel,
               void(bool, uint16_t, const std::string&));  // NOLINT

  // We can't use GMock with std::unique_ptr.
  void OnStartOpeningHandshake(
      std::unique_ptr<WebSocketHandshakeRequestInfo>) override {
    OnStartOpeningHandshakeCalled();
  }
  void OnFinishOpeningHandshake(
      std::unique_ptr<WebSocketHandshakeResponseInfo>) override {
    OnFinishOpeningHandshakeCalled();
  }
  void OnSSLCertificateError(
      std::unique_ptr<SSLErrorCallbacks> ssl_error_callbacks,
      const GURL& url,
      int net_error,
      const SSLInfo& ssl_info,
      bool fatal) override {
    OnSSLCertificateErrorCalled(
        ssl_error_callbacks.get(), url, ssl_info, fatal);
  }
  int OnAuthRequired(const AuthChallengeInfo& auth_info,
                     scoped_refptr<HttpResponseHeaders> response_headers,
                     const IPEndPoint& remote_endpoint,
                     base::OnceCallback<void(const AuthCredentials*)> callback,
                     base::Optional<AuthCredentials>* credentials) override {
    return OnAuthRequiredCalled(std::move(auth_info),
                                std::move(response_headers), remote_endpoint,
                                credentials);
  }

  MOCK_METHOD0(OnStartOpeningHandshakeCalled, void());  // NOLINT
  MOCK_METHOD0(OnFinishOpeningHandshakeCalled, void());  // NOLINT
  MOCK_METHOD4(
      OnSSLCertificateErrorCalled,
      void(SSLErrorCallbacks*, const GURL&, const SSLInfo&, bool));  // NOLINT
  MOCK_METHOD4(OnAuthRequiredCalled,
               int(const AuthChallengeInfo&,
                   scoped_refptr<HttpResponseHeaders>,
                   const IPEndPoint&,
                   base::Optional<AuthCredentials>*));
};

// This fake EventInterface is for tests which need a WebSocketEventInterface
// implementation but are not verifying how it is used.
class FakeWebSocketEventInterface : public WebSocketEventInterface {
  void OnCreateURLRequest(URLRequest* request) override {}
  void OnAddChannelResponse(const std::string& selected_protocol,
                            const std::string& extensions,
                            int64_t send_flow_control_quota) override {}
  void OnDataFrame(bool fin,
                   WebSocketMessageType type,
                   base::span<const char> data_span) override {}
  bool HasPendingDataFrames() override { return false; }
  void OnSendFlowControlQuotaAdded(int64_t quota) override {}
  void OnClosingHandshake() override {}
  void OnFailChannel(const std::string& message) override {}
  void OnDropChannel(bool was_clean,
                     uint16_t code,
                     const std::string& reason) override {}
  void OnStartOpeningHandshake(
      std::unique_ptr<WebSocketHandshakeRequestInfo> request) override {}
  void OnFinishOpeningHandshake(
      std::unique_ptr<WebSocketHandshakeResponseInfo> response) override {}
  void OnSSLCertificateError(
      std::unique_ptr<SSLErrorCallbacks> ssl_error_callbacks,
      const GURL& url,
      int net_error,
      const SSLInfo& ssl_info,
      bool fatal) override {}
  int OnAuthRequired(const AuthChallengeInfo& auth_info,
                     scoped_refptr<HttpResponseHeaders> response_headers,
                     const IPEndPoint& remote_endpoint,
                     base::OnceCallback<void(const AuthCredentials*)> callback,
                     base::Optional<AuthCredentials>* credentials) override {
    *credentials = base::nullopt;
    return OK;
  }
};

// This fake WebSocketStream is for tests that require a WebSocketStream but are
// not testing the way it is used. It has minimal functionality to return
// the |protocol| and |extensions| that it was constructed with.
class FakeWebSocketStream : public WebSocketStream {
 public:
  // Constructs with empty protocol and extensions.
  FakeWebSocketStream() = default;

  // Constructs with specified protocol and extensions.
  FakeWebSocketStream(const std::string& protocol,
                      const std::string& extensions)
      : protocol_(protocol), extensions_(extensions) {}

  int ReadFrames(std::vector<std::unique_ptr<WebSocketFrame>>* frames,
                 CompletionOnceCallback callback) override {
    return ERR_IO_PENDING;
  }

  int WriteFrames(std::vector<std::unique_ptr<WebSocketFrame>>* frames,
                  CompletionOnceCallback callback) override {
    return ERR_IO_PENDING;
  }

  void Close() override {}

  // Returns the string passed to the constructor.
  std::string GetSubProtocol() const override { return protocol_; }

  // Returns the string passed to the constructor.
  std::string GetExtensions() const override { return extensions_; }

 private:
  // The string to return from GetSubProtocol().
  std::string protocol_;

  // The string to return from GetExtensions().
  std::string extensions_;
};

// To make the static initialisers easier to read, we use enums rather than
// bools.
enum IsFinal { NOT_FINAL_FRAME, FINAL_FRAME };

enum IsMasked { NOT_MASKED, MASKED };

// This is used to initialise a WebSocketFrame but is statically initialisable.
struct InitFrame {
  IsFinal final;
  // Reserved fields omitted for now. Add them if you need them.
  WebSocketFrameHeader::OpCode opcode;
  IsMasked masked;

  // Will be used to create the IOBuffer member. Can be null for null data. Is a
  // nul-terminated string for ease-of-use. |header.payload_length| is
  // initialised from |strlen(data)|. This means it is not 8-bit clean, but this
  // is not an issue for test data.
  const char* const data;
};

// For GoogleMock
std::ostream& operator<<(std::ostream& os, const InitFrame& frame) {
  os << "{" << (frame.final == FINAL_FRAME ? "FINAL_FRAME" : "NOT_FINAL_FRAME")
     << ", " << frame.opcode << ", "
     << (frame.masked == MASKED ? "MASKED" : "NOT_MASKED") << ", ";
  if (frame.data) {
    return os << "\"" << frame.data << "\"}";
  }
  return os << "NULL}";
}

template <size_t N>
std::ostream& operator<<(std::ostream& os, const InitFrame (&frames)[N]) {
  os << "{";
  bool first = true;
  for (size_t i = 0; i < N; ++i) {
    if (!first) {
      os << ",\n";
    } else {
      first = false;
    }
    os << frames[i];
  }
  return os << "}";
}

// Convert a const array of InitFrame structs to the format used at
// runtime. Templated on the size of the array to save typing.
template <size_t N>
std::vector<std::unique_ptr<WebSocketFrame>> CreateFrameVector(
    const InitFrame (&source_frames)[N],
    std::vector<scoped_refptr<IOBuffer>>* result_frame_data) {
  std::vector<std::unique_ptr<WebSocketFrame>> result_frames;
  result_frames.reserve(N);
  for (size_t i = 0; i < N; ++i) {
    const InitFrame& source_frame = source_frames[i];
    auto result_frame = std::make_unique<WebSocketFrame>(source_frame.opcode);
    size_t frame_length = source_frame.data ? strlen(source_frame.data) : 0;
    WebSocketFrameHeader& result_header = result_frame->header;
    result_header.final = (source_frame.final == FINAL_FRAME);
    result_header.masked = (source_frame.masked == MASKED);
    result_header.payload_length = frame_length;
    if (source_frame.data) {
      auto buffer = base::MakeRefCounted<IOBuffer>(frame_length);
      result_frame_data->push_back(buffer);
      memcpy(buffer->data(), source_frame.data, frame_length);
      result_frame->payload = buffer->data();
    }
    result_frames.push_back(std::move(result_frame));
  }
  return result_frames;
}

// A GoogleMock action which can be used to respond to call to ReadFrames with
// some frames. Use like ReadFrames(_, _).WillOnce(ReturnFrames(&frames,
// &result_frame_data_)); |frames| is an array of InitFrame. |frames| needs to
// be passed by pointer because otherwise it will be treated as a pointer and
// the array size information will be lost.
ACTION_P2(ReturnFrames, source_frames, result_frame_data) {
  *arg0 = CreateFrameVector(*source_frames, result_frame_data);
  return OK;
}

// The implementation of a GoogleMock matcher which can be used to compare a
// std::vector<std::unique_ptr<WebSocketFrame>>* against an expectation defined
// as an
// array of InitFrame objects. Although it is possible to compose built-in
// GoogleMock matchers to check the contents of a WebSocketFrame, the results
// are so unreadable that it is better to use this matcher.
template <size_t N>
class EqualsFramesMatcher : public ::testing::MatcherInterface<
                                std::vector<std::unique_ptr<WebSocketFrame>>*> {
 public:
  explicit EqualsFramesMatcher(const InitFrame (*expect_frames)[N])
      : expect_frames_(expect_frames) {}

  virtual bool MatchAndExplain(
      std::vector<std::unique_ptr<WebSocketFrame>>* actual_frames,
      ::testing::MatchResultListener* listener) const {
    if (actual_frames->size() != N) {
      *listener << "the vector size is " << actual_frames->size();
      return false;
    }
    for (size_t i = 0; i < N; ++i) {
      const WebSocketFrame& actual_frame = *(*actual_frames)[i];
      const InitFrame& expected_frame = (*expect_frames_)[i];
      if (actual_frame.header.final != (expected_frame.final == FINAL_FRAME)) {
        *listener << "the frame is marked as "
                  << (actual_frame.header.final ? "" : "not ") << "final";
        return false;
      }
      if (actual_frame.header.opcode != expected_frame.opcode) {
        *listener << "the opcode is " << actual_frame.header.opcode;
        return false;
      }
      if (actual_frame.header.masked != (expected_frame.masked == MASKED)) {
        *listener << "the frame is "
                  << (actual_frame.header.masked ? "masked" : "not masked");
        return false;
      }
      const size_t expected_length =
          expected_frame.data ? strlen(expected_frame.data) : 0;
      if (actual_frame.header.payload_length != expected_length) {
        *listener << "the payload length is "
                  << actual_frame.header.payload_length;
        return false;
      }
      if (expected_length != 0 &&
          memcmp(actual_frame.payload, expected_frame.data,
                 actual_frame.header.payload_length) != 0) {
        *listener << "the data content differs";
        return false;
      }
    }
    return true;
  }

  virtual void DescribeTo(std::ostream* os) const {
    *os << "matches " << *expect_frames_;
  }

  virtual void DescribeNegationTo(std::ostream* os) const {
    *os << "does not match " << *expect_frames_;
  }

 private:
  const InitFrame (*expect_frames_)[N];
};

// The definition of EqualsFrames GoogleMock matcher. Unlike the ReturnFrames
// action, this can take the array by reference.
template <size_t N>
::testing::Matcher<std::vector<std::unique_ptr<WebSocketFrame>>*> EqualsFrames(
    const InitFrame (&frames)[N]) {
  return ::testing::MakeMatcher(new EqualsFramesMatcher<N>(&frames));
}

// A GoogleMock action to run a Closure.
ACTION_P(InvokeClosure, closure) { closure.Run(); }

// A FakeWebSocketStream whose ReadFrames() function returns data.
class ReadableFakeWebSocketStream : public FakeWebSocketStream {
 public:
  enum IsSync { SYNC, ASYNC };

  // After constructing the object, call PrepareReadFrames() once for each
  // time you wish it to return from the test.
  ReadableFakeWebSocketStream() : index_(0), read_frames_pending_(false) {}

  // Check that all the prepared responses have been consumed.
  ~ReadableFakeWebSocketStream() override {
    CHECK(index_ >= responses_.size());
    CHECK(!read_frames_pending_);
  }

  // Prepares a fake response. Fake responses will be returned from ReadFrames()
  // in the same order they were prepared with PrepareReadFrames() and
  // PrepareReadFramesError(). If |async| is ASYNC, then ReadFrames() will
  // return ERR_IO_PENDING and the callback will be scheduled to run on the
  // message loop. This requires the test case to run the message loop. If
  // |async| is SYNC, the response will be returned synchronously. |error| is
  // returned directly from ReadFrames() in the synchronous case, or passed to
  // the callback in the asynchronous case. |frames| will be converted to a
  // std::vector<std::unique_ptr<WebSocketFrame>> and copied to the pointer that
  // was
  // passed to ReadFrames().
  template <size_t N>
  void PrepareReadFrames(IsSync async,
                         int error,
                         const InitFrame (&frames)[N]) {
    responses_.push_back(std::make_unique<Response>(
        async, error, CreateFrameVector(frames, &result_frame_data_)));
  }

  // An alternate version of PrepareReadFrames for when we need to construct
  // the frames manually.
  void PrepareRawReadFrames(
      IsSync async,
      int error,
      std::vector<std::unique_ptr<WebSocketFrame>> frames) {
    responses_.push_back(
        std::make_unique<Response>(async, error, std::move(frames)));
  }

  // Prepares a fake error response (ie. there is no data).
  void PrepareReadFramesError(IsSync async, int error) {
    responses_.push_back(std::make_unique<Response>(
        async, error, std::vector<std::unique_ptr<WebSocketFrame>>()));
  }

  int ReadFrames(std::vector<std::unique_ptr<WebSocketFrame>>* frames,
                 CompletionOnceCallback callback) override {
    CHECK(!read_frames_pending_);
    if (index_ >= responses_.size())
      return ERR_IO_PENDING;
    if (responses_[index_]->async == ASYNC) {
      read_frames_pending_ = true;
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&ReadableFakeWebSocketStream::DoCallback,
                         base::Unretained(this), frames, std::move(callback)));
      return ERR_IO_PENDING;
    } else {
      frames->swap(responses_[index_]->frames);
      return responses_[index_++]->error;
    }
  }

 private:
  void DoCallback(std::vector<std::unique_ptr<WebSocketFrame>>* frames,
                  CompletionOnceCallback callback) {
    read_frames_pending_ = false;
    frames->swap(responses_[index_]->frames);
    std::move(callback).Run(responses_[index_++]->error);
    return;
  }

  struct Response {
    Response(IsSync async,
             int error,
             std::vector<std::unique_ptr<WebSocketFrame>> frames)
        : async(async), error(error), frames(std::move(frames)) {}

    IsSync async;
    int error;
    std::vector<std::unique_ptr<WebSocketFrame>> frames;

   private:
    // Bad things will happen if we attempt to copy or assign |frames|.
    DISALLOW_COPY_AND_ASSIGN(Response);
  };
  std::vector<std::unique_ptr<Response>> responses_;

  // The index into the responses_ array of the next response to be returned.
  size_t index_;

  // True when an async response from ReadFrames() is pending. This only applies
  // to "real" async responses. Once all the prepared responses have been
  // returned, ReadFrames() returns ERR_IO_PENDING but read_frames_pending_ is
  // not set to true.
  bool read_frames_pending_;

  std::vector<scoped_refptr<IOBuffer>> result_frame_data_;
};

// A FakeWebSocketStream where writes always complete successfully and
// synchronously.
class WriteableFakeWebSocketStream : public FakeWebSocketStream {
 public:
  int WriteFrames(std::vector<std::unique_ptr<WebSocketFrame>>* frames,
                  CompletionOnceCallback callback) override {
    return OK;
  }
};

// A FakeWebSocketStream where writes always fail.
class UnWriteableFakeWebSocketStream : public FakeWebSocketStream {
 public:
  int WriteFrames(std::vector<std::unique_ptr<WebSocketFrame>>* frames,
                  CompletionOnceCallback callback) override {
    return ERR_CONNECTION_RESET;
  }
};

// A FakeWebSocketStream which echoes any frames written back. Clears the
// "masked" header bit, but makes no other checks for validity. Tests using this
// must run the MessageLoop to receive the callback(s). If a message with opcode
// Close is echoed, then an ERR_CONNECTION_CLOSED is returned in the next
// callback. The test must do something to cause WriteFrames() to be called,
// otherwise the ReadFrames() callback will never be called.
class EchoeyFakeWebSocketStream : public FakeWebSocketStream {
 public:
  EchoeyFakeWebSocketStream() : read_frames_(nullptr), done_(false) {}

  int WriteFrames(std::vector<std::unique_ptr<WebSocketFrame>>* frames,
                  CompletionOnceCallback callback) override {
    for (const auto& frame : *frames) {
      auto buffer = base::MakeRefCounted<IOBuffer>(
          static_cast<size_t>(frame->header.payload_length));
      memcpy(buffer->data(), frame->payload, frame->header.payload_length);
      frame->payload = buffer->data();
      buffers_.push_back(buffer);
    }
    stored_frames_.insert(stored_frames_.end(),
                          std::make_move_iterator(frames->begin()),
                          std::make_move_iterator(frames->end()));
    frames->clear();
    // Users of WebSocketStream will not expect the ReadFrames() callback to be
    // called from within WriteFrames(), so post it to the message loop instead.
    PostCallback();
    return OK;
  }

  int ReadFrames(std::vector<std::unique_ptr<WebSocketFrame>>* frames,
                 CompletionOnceCallback callback) override {
    read_callback_ = std::move(callback);
    read_frames_ = frames;
    if (done_)
      PostCallback();
    return ERR_IO_PENDING;
  }

 private:
  void PostCallback() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&EchoeyFakeWebSocketStream::DoCallback,
                                  base::Unretained(this)));
  }

  void DoCallback() {
    if (done_) {
      std::move(read_callback_).Run(ERR_CONNECTION_CLOSED);
    } else if (!stored_frames_.empty()) {
      done_ = MoveFrames(read_frames_);
      read_frames_ = nullptr;
      std::move(read_callback_).Run(OK);
    }
  }

  // Copy the frames stored in stored_frames_ to |out|, while clearing the
  // "masked" header bit. Returns true if a Close Frame was seen, false
  // otherwise.
  bool MoveFrames(std::vector<std::unique_ptr<WebSocketFrame>>* out) {
    bool seen_close = false;
    *out = std::move(stored_frames_);
    for (const auto& frame : *out) {
      WebSocketFrameHeader& header = frame->header;
      header.masked = false;
      if (header.opcode == WebSocketFrameHeader::kOpCodeClose)
        seen_close = true;
    }
    return seen_close;
  }

  std::vector<std::unique_ptr<WebSocketFrame>> stored_frames_;
  CompletionOnceCallback read_callback_;
  // Owned by the caller of ReadFrames().
  std::vector<std::unique_ptr<WebSocketFrame>>* read_frames_;
  std::vector<scoped_refptr<IOBuffer>> buffers_;
  // True if we should close the connection.
  bool done_;
};

// A FakeWebSocketStream where writes trigger a connection reset.
// This differs from UnWriteableFakeWebSocketStream in that it is asynchronous
// and triggers ReadFrames to return a reset as well. Tests using this need to
// run the message loop. There are two tricky parts here:
// 1. Calling the write callback may call Close(), after which the read callback
//    should not be called.
// 2. Calling either callback may delete the stream altogether.
class ResetOnWriteFakeWebSocketStream : public FakeWebSocketStream {
 public:
  ResetOnWriteFakeWebSocketStream() : closed_(false) {}

  int WriteFrames(std::vector<std::unique_ptr<WebSocketFrame>>* frames,
                  CompletionOnceCallback callback) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ResetOnWriteFakeWebSocketStream::CallCallbackUnlessClosed,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback),
            ERR_CONNECTION_RESET));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ResetOnWriteFakeWebSocketStream::CallCallbackUnlessClosed,
            weak_ptr_factory_.GetWeakPtr(), std::move(read_callback_),
            ERR_CONNECTION_RESET));
    return ERR_IO_PENDING;
  }

  int ReadFrames(std::vector<std::unique_ptr<WebSocketFrame>>* frames,
                 CompletionOnceCallback callback) override {
    read_callback_ = std::move(callback);
    return ERR_IO_PENDING;
  }

  void Close() override { closed_ = true; }

 private:
  void CallCallbackUnlessClosed(CompletionOnceCallback callback, int value) {
    if (!closed_)
      std::move(callback).Run(value);
  }

  CompletionOnceCallback read_callback_;
  bool closed_;
  // An IO error can result in the socket being deleted, so we use weak pointers
  // to ensure correct behaviour in that case.
  base::WeakPtrFactory<ResetOnWriteFakeWebSocketStream> weak_ptr_factory_{this};
};

// This mock is for verifying that WebSocket protocol semantics are obeyed (to
// the extent that they are implemented in WebSocketCommon).
class MockWebSocketStream : public WebSocketStream {
 public:
  // GMock cannot save or forward move-only types like CompletionOnceCallback,
  // therefore they have to be converted into a copyable type like
  // CompletionRepeatingCallback.
  int ReadFrames(std::vector<std::unique_ptr<WebSocketFrame>>* frames,
                 CompletionOnceCallback callback) {
    return ReadFramesInternal(
        frames, callback ? base::AdaptCallbackForRepeating(std::move(callback))
                         : CompletionRepeatingCallback());
  }
  int WriteFrames(std::vector<std::unique_ptr<WebSocketFrame>>* frames,
                  CompletionOnceCallback callback) {
    return WriteFramesInternal(
        frames, callback ? base::AdaptCallbackForRepeating(std::move(callback))
                         : CompletionRepeatingCallback());
  }

  MOCK_METHOD2(ReadFramesInternal,
               int(std::vector<std::unique_ptr<WebSocketFrame>>*,
                   const CompletionRepeatingCallback&));
  MOCK_METHOD2(WriteFramesInternal,
               int(std::vector<std::unique_ptr<WebSocketFrame>>*,
                   const CompletionRepeatingCallback&));

  MOCK_METHOD0(Close, void());
  MOCK_CONST_METHOD0(GetSubProtocol, std::string());
  MOCK_CONST_METHOD0(GetExtensions, std::string());
  MOCK_METHOD0(AsWebSocketStream, WebSocketStream*());
};

class MockWebSocketStreamRequest : public WebSocketStreamRequest {
 public:
  MOCK_METHOD1(OnBasicHandshakeStreamCreated,
               void(WebSocketBasicHandshakeStream* handshake_stream));
  MOCK_METHOD1(OnHttp2HandshakeStreamCreated,
               void(WebSocketHttp2HandshakeStream* handshake_stream));
  MOCK_METHOD1(OnFailure, void(const std::string& message));
};

struct WebSocketStreamCreationCallbackArgumentSaver {
  std::unique_ptr<WebSocketStreamRequest> Create(
      const GURL& socket_url,
      const std::vector<std::string>& requested_subprotocols,
      const url::Origin& origin,
      const GURL& site_for_cookies,
      const net::NetworkIsolationKey& network_isolation_key,
      const HttpRequestHeaders& additional_headers,
      URLRequestContext* url_request_context,
      const NetLogWithSource& net_log,
      std::unique_ptr<WebSocketStream::ConnectDelegate> connect_delegate) {
    this->socket_url = socket_url;
    this->origin = origin;
    this->site_for_cookies = site_for_cookies;
    this->network_isolation_key = network_isolation_key;
    this->url_request_context = url_request_context;
    this->connect_delegate = std::move(connect_delegate);
    return std::make_unique<MockWebSocketStreamRequest>();
  }

  GURL socket_url;
  url::Origin origin;
  GURL site_for_cookies;
  net::NetworkIsolationKey network_isolation_key;
  URLRequestContext* url_request_context;
  std::unique_ptr<WebSocketStream::ConnectDelegate> connect_delegate;
};

std::vector<char> AsVector(const base::StringPiece& s) {
  return std::vector<char>(s.begin(), s.end());
}

// Converts a base::StringPiece to a IOBuffer. For test purposes, it is
// convenient to be able to specify data as a string, but the
// WebSocketEventInterface requires the IOBuffer type.
scoped_refptr<IOBuffer> AsIOBuffer(const base::StringPiece& s) {
  auto buffer = base::MakeRefCounted<IOBuffer>(s.size());
  std::copy(s.begin(), s.end(), buffer->data());
  return buffer;
}

class FakeSSLErrorCallbacks
    : public WebSocketEventInterface::SSLErrorCallbacks {
 public:
  void CancelSSLRequest(int error, const SSLInfo* ssl_info) override {}
  void ContinueSSLRequest() override {}
};

// Base class for all test fixtures.
class WebSocketChannelTest : public TestWithTaskEnvironment {
 protected:
  WebSocketChannelTest() : stream_(std::make_unique<FakeWebSocketStream>()) {}

  // Creates a new WebSocketChannel and connects it, using the settings stored
  // in |connect_data_|.
  void CreateChannelAndConnect() {
    channel_ = std::make_unique<WebSocketChannel>(
        CreateEventInterface(), &connect_data_.url_request_context);
    channel_->SendAddChannelRequestForTesting(
        connect_data_.socket_url, connect_data_.requested_subprotocols,
        connect_data_.origin, connect_data_.site_for_cookies,
        connect_data_.network_isolation_key, HttpRequestHeaders(),
        base::Bind(&WebSocketStreamCreationCallbackArgumentSaver::Create,
                   base::Unretained(&connect_data_.argument_saver)));
  }

  // Same as CreateChannelAndConnect(), but calls the on_success callback as
  // well. This method is virtual so that subclasses can also set the stream.
  virtual void CreateChannelAndConnectSuccessfully() {
    CreateChannelAndConnect();
    connect_data_.argument_saver.connect_delegate->OnSuccess(
        std::move(stream_));
    ignore_result(channel_->ReadFrames());
  }

  // Returns a WebSocketEventInterface to be passed to the WebSocketChannel.
  // This implementation returns a newly-created fake. Subclasses may return a
  // mock instead.
  virtual std::unique_ptr<WebSocketEventInterface> CreateEventInterface() {
    return std::make_unique<FakeWebSocketEventInterface>();
  }

  // This method serves no other purpose than to provide a nice syntax for
  // assigning to stream_. class T must be a subclass of WebSocketStream or you
  // will have unpleasant compile errors.
  template <class T>
  void set_stream(std::unique_ptr<T> stream) {
    stream_ = std::move(stream);
  }

  // A struct containing the data that will be used to connect the channel.
  // Grouped for readability.
  struct ConnectData {
    ConnectData()
        : socket_url("ws://ws/"),
          origin(url::Origin::Create(GURL("http://ws"))),
          site_for_cookies("http://ws/") {
      url::Origin top_frame_origin = url::Origin::Create(GURL("http://ws-1"));
      this->network_isolation_key =
          net::NetworkIsolationKey(top_frame_origin, origin);
    }

    // URLRequestContext object.
    URLRequestContext url_request_context;

    // URL to (pretend to) connect to.
    GURL socket_url;
    // Requested protocols for the request.
    std::vector<std::string> requested_subprotocols;
    // Origin of the request
    url::Origin origin;
    // First party for cookies for the request.
    GURL site_for_cookies;
    // NetworkIsolationKey created from the origin of the top level frame.
    net::NetworkIsolationKey network_isolation_key;

    WebSocketStreamCreationCallbackArgumentSaver argument_saver;
  };
  ConnectData connect_data_;

  // The channel we are testing. Not initialised until SetChannel() is called.
  std::unique_ptr<WebSocketChannel> channel_;

  // A mock or fake stream for tests that need one.
  std::unique_ptr<WebSocketStream> stream_;

  std::vector<scoped_refptr<IOBuffer>> result_frame_data_;
};

// enum of WebSocketEventInterface calls. These are intended to be or'd together
// in order to instruct WebSocketChannelDeletingTest when it should fail.
enum EventInterfaceCall {
  EVENT_ON_ADD_CHANNEL_RESPONSE = 0x1,
  EVENT_ON_DATA_FRAME = 0x2,
  EVENT_ON_FLOW_CONTROL = 0x4,
  EVENT_ON_CLOSING_HANDSHAKE = 0x8,
  EVENT_ON_FAIL_CHANNEL = 0x10,
  EVENT_ON_DROP_CHANNEL = 0x20,
  EVENT_ON_START_OPENING_HANDSHAKE = 0x40,
  EVENT_ON_FINISH_OPENING_HANDSHAKE = 0x80,
  EVENT_ON_SSL_CERTIFICATE_ERROR = 0x100,
};

// Base class for tests which verify that EventInterface methods are called
// appropriately.
class WebSocketChannelEventInterfaceTest : public WebSocketChannelTest {
 public:
  void SetUp() override {
    EXPECT_CALL(*event_interface_, HasPendingDataFrames()).Times(AnyNumber());
  }

 protected:
  WebSocketChannelEventInterfaceTest()
      : event_interface_(
            std::make_unique<StrictMock<MockWebSocketEventInterface>>()) {
  }

  ~WebSocketChannelEventInterfaceTest() override {
  }

  // Tests using this fixture must set expectations on the event_interface_ mock
  // object before calling CreateChannelAndConnect() or
  // CreateChannelAndConnectSuccessfully(). This will only work once per test
  // case, but once should be enough.
  std::unique_ptr<WebSocketEventInterface> CreateEventInterface() override {
    return std::move(event_interface_);
  }

  std::unique_ptr<MockWebSocketEventInterface> event_interface_;
};

// Base class for tests which verify that WebSocketStream methods are called
// appropriately by using a MockWebSocketStream.
class WebSocketChannelStreamTest : public WebSocketChannelEventInterfaceTest {
 public:
  void SetUp() override {
    WebSocketChannelEventInterfaceTest::SetUp();
    // For the purpose of the tests using this fixture, it doesn't matter
    // whether these methods are called or not.
    EXPECT_CALL(*mock_stream_, GetSubProtocol()).Times(AnyNumber());
    EXPECT_CALL(*mock_stream_, GetExtensions()).Times(AnyNumber());
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _))
        .Times(AnyNumber());
    EXPECT_CALL(*event_interface_, OnDataFrameVector(_, _, _))
        .Times(AnyNumber());
    EXPECT_CALL(*event_interface_, OnClosingHandshake()).Times(AnyNumber());
    EXPECT_CALL(*event_interface_, OnFailChannel(_)).Times(AnyNumber());
    EXPECT_CALL(*event_interface_, OnDropChannel(_, _, _)).Times(AnyNumber());
  }

 protected:
  WebSocketChannelStreamTest()
      : mock_stream_(std::make_unique<StrictMock<MockWebSocketStream>>()) {}

  void CreateChannelAndConnectSuccessfully() override {
    set_stream(std::move(mock_stream_));
    WebSocketChannelTest::CreateChannelAndConnectSuccessfully();
  }

  std::unique_ptr<MockWebSocketStream> mock_stream_;
};

// Fixture for tests which test UTF-8 validation of sent Text frames via the
// EventInterface.
class WebSocketChannelSendUtf8Test
    : public WebSocketChannelEventInterfaceTest {
 public:
  void SetUp() override {
    WebSocketChannelEventInterfaceTest::SetUp();
    set_stream(std::make_unique<WriteableFakeWebSocketStream>());
    // For the purpose of the tests using this fixture, it doesn't matter
    // whether these methods are called or not.
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _))
        .Times(AnyNumber());
  }
};

// Fixture for tests which test UTF-8 validation of received Text frames using a
// mock WebSocketStream.
class WebSocketChannelReceiveUtf8Test : public WebSocketChannelStreamTest {
 public:
  void SetUp() override {
    WebSocketChannelStreamTest::SetUp();
    // For the purpose of the tests using this fixture, it doesn't matter
    // whether these methods are called or not.
  }
};

// Simple test that everything that should be passed to the stream creation
// callback is passed to the argument saver.
TEST_F(WebSocketChannelTest, EverythingIsPassedToTheCreatorFunction) {
  connect_data_.socket_url = GURL("ws://example.com/test");
  connect_data_.origin = url::Origin::Create(GURL("http://example.com"));
  connect_data_.site_for_cookies = GURL("http://example.com/");
  url::Origin top_frame_origin =
      url::Origin::Create(GURL("http://example-1.com"));
  connect_data_.network_isolation_key =
      net::NetworkIsolationKey(top_frame_origin, connect_data_.origin);
  connect_data_.requested_subprotocols.push_back("Sinbad");

  CreateChannelAndConnect();

  const WebSocketStreamCreationCallbackArgumentSaver& actual =
      connect_data_.argument_saver;

  EXPECT_EQ(&connect_data_.url_request_context, actual.url_request_context);

  EXPECT_EQ(connect_data_.socket_url, actual.socket_url);
  EXPECT_EQ(connect_data_.origin.Serialize(), actual.origin.Serialize());
  EXPECT_EQ(connect_data_.site_for_cookies, actual.site_for_cookies);
  EXPECT_EQ(connect_data_.network_isolation_key, actual.network_isolation_key);
}

TEST_F(WebSocketChannelEventInterfaceTest, ConnectSuccessReported) {
  // false means success.
  EXPECT_CALL(*event_interface_, OnAddChannelResponse("", "", _));

  CreateChannelAndConnect();

  connect_data_.argument_saver.connect_delegate->OnSuccess(std::move(stream_));
  ignore_result(channel_->ReadFrames());
}

TEST_F(WebSocketChannelEventInterfaceTest, ConnectFailureReported) {
  EXPECT_CALL(*event_interface_, OnFailChannel("hello"));

  CreateChannelAndConnect();

  connect_data_.argument_saver.connect_delegate->OnFailure("hello");
}

TEST_F(WebSocketChannelEventInterfaceTest, NonWebSocketSchemeRejected) {
  EXPECT_CALL(*event_interface_, OnFailChannel("Invalid scheme"));
  connect_data_.socket_url = GURL("http://www.google.com/");
  CreateChannelAndConnect();
}

TEST_F(WebSocketChannelEventInterfaceTest, ProtocolPassed) {
  EXPECT_CALL(*event_interface_, OnAddChannelResponse("Bob", "", _));

  CreateChannelAndConnect();

  connect_data_.argument_saver.connect_delegate->OnSuccess(
      std::make_unique<FakeWebSocketStream>("Bob", ""));
  ignore_result(channel_->ReadFrames());
}

TEST_F(WebSocketChannelEventInterfaceTest, ExtensionsPassed) {
  EXPECT_CALL(*event_interface_,
              OnAddChannelResponse("", "extension1, extension2", _));

  CreateChannelAndConnect();

  connect_data_.argument_saver.connect_delegate->OnSuccess(
      std::make_unique<FakeWebSocketStream>("", "extension1, extension2"));
  ignore_result(channel_->ReadFrames());
}

// The first frames from the server can arrive together with the handshake, in
// which case they will be available as soon as ReadFrames() is called the first
// time.
TEST_F(WebSocketChannelEventInterfaceTest, DataLeftFromHandshake) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, "HELLO"}};
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::SYNC, OK, frames);
  set_stream(std::move(stream));
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
    EXPECT_CALL(*event_interface_,
                OnDataFrameVector(true, WebSocketFrameHeader::kOpCodeText,
                                  AsVector("HELLO")));
  }

  CreateChannelAndConnectSuccessfully();
}

// A remote server could accept the handshake, but then immediately send a
// Close frame.
TEST_F(WebSocketChannelEventInterfaceTest, CloseAfterHandshake) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose,
       NOT_MASKED,  CLOSE_DATA(SERVER_ERROR, "Internal Server Error")}};
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::SYNC, OK, frames);
  stream->PrepareReadFramesError(ReadableFakeWebSocketStream::SYNC,
                                 ERR_CONNECTION_CLOSED);
  set_stream(std::move(stream));
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
    EXPECT_CALL(*event_interface_, OnClosingHandshake());
    EXPECT_CALL(
        *event_interface_,
        OnDropChannel(
            true, kWebSocketErrorInternalServerError, "Internal Server Error"));
  }

  CreateChannelAndConnectSuccessfully();
}

// Do not close until browser has sent all pending frames.
TEST_F(WebSocketChannelEventInterfaceTest, ShouldCloseWhileNoDataFrames) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose, NOT_MASKED,
       CLOSE_DATA(SERVER_ERROR, "Internal Server Error")}};
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::SYNC, OK, frames);
  stream->PrepareReadFramesError(ReadableFakeWebSocketStream::SYNC,
                                 ERR_CONNECTION_CLOSED);
  set_stream(std::move(stream));
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
    EXPECT_CALL(*event_interface_, HasPendingDataFrames())
        .WillOnce(Return(false))
        .WillOnce(Return(true))
        .WillOnce(Return(true));
    EXPECT_CALL(checkpoint, Call(1));
#if DCHECK_IS_ON()
    EXPECT_CALL(*event_interface_, HasPendingDataFrames())
        .WillOnce(Return(false));
#endif
    EXPECT_CALL(*event_interface_, OnClosingHandshake());
    EXPECT_CALL(*event_interface_,
                OnDropChannel(true, kWebSocketErrorInternalServerError,
                              "Internal Server Error"));
  }

  CreateChannelAndConnectSuccessfully();
  checkpoint.Call(1);
  ASSERT_EQ(CHANNEL_DELETED, channel_->ReadFrames());
}

// A remote server could close the connection immediately after sending the
// handshake response (most likely a bug in the server).
TEST_F(WebSocketChannelEventInterfaceTest, ConnectionCloseAfterHandshake) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  stream->PrepareReadFramesError(ReadableFakeWebSocketStream::SYNC,
                                 ERR_CONNECTION_CLOSED);
  set_stream(std::move(stream));
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
    EXPECT_CALL(*event_interface_,
                OnDropChannel(false, kWebSocketErrorAbnormalClosure, _));
  }

  CreateChannelAndConnectSuccessfully();
}

TEST_F(WebSocketChannelEventInterfaceTest, NormalAsyncRead) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, "HELLO"}};
  // We use this checkpoint object to verify that the callback isn't called
  // until we expect it to be.
  Checkpoint checkpoint;
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, frames);
  set_stream(std::move(stream));
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*event_interface_,
                OnDataFrameVector(true, WebSocketFrameHeader::kOpCodeText,
                                  AsVector("HELLO")));
    EXPECT_CALL(checkpoint, Call(2));
  }

  CreateChannelAndConnectSuccessfully();
  checkpoint.Call(1);
  base::RunLoop().RunUntilIdle();
  checkpoint.Call(2);
}

// Extra data can arrive while a read is being processed, resulting in the next
// read completing synchronously.
TEST_F(WebSocketChannelEventInterfaceTest, AsyncThenSyncRead) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  static const InitFrame frames1[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, "HELLO"}};
  static const InitFrame frames2[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, "WORLD"}};
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, frames1);
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::SYNC, OK, frames2);
  set_stream(std::move(stream));
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
    EXPECT_CALL(*event_interface_,
                OnDataFrameVector(true, WebSocketFrameHeader::kOpCodeText,
                                  AsVector("HELLO")));
    EXPECT_CALL(*event_interface_,
                OnDataFrameVector(true, WebSocketFrameHeader::kOpCodeText,
                                  AsVector("WORLD")));
  }

  CreateChannelAndConnectSuccessfully();
  base::RunLoop().RunUntilIdle();
}

// Data frames are delivered the same regardless of how many reads they arrive
// as.
TEST_F(WebSocketChannelEventInterfaceTest, FragmentedMessage) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  // Here we have one message which arrived in five frames split across three
  // reads. It may have been reframed on arrival, but this class doesn't care
  // about that.
  static const InitFrame frames1[] = {
      {NOT_FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, "THREE"},
      {NOT_FINAL_FRAME, WebSocketFrameHeader::kOpCodeContinuation,
       NOT_MASKED,      " "}};
  static const InitFrame frames2[] = {
      {NOT_FINAL_FRAME, WebSocketFrameHeader::kOpCodeContinuation,
       NOT_MASKED,      "SMALL"}};
  static const InitFrame frames3[] = {
      {NOT_FINAL_FRAME, WebSocketFrameHeader::kOpCodeContinuation,
       NOT_MASKED,      " "},
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeContinuation,
       NOT_MASKED,  "FRAMES"}};
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, frames1);
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, frames2);
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, frames3);
  set_stream(std::move(stream));
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
    EXPECT_CALL(*event_interface_,
                OnDataFrameVector(false, WebSocketFrameHeader::kOpCodeText,
                                  AsVector("THREE")));
    EXPECT_CALL(
        *event_interface_,
        OnDataFrameVector(false, WebSocketFrameHeader::kOpCodeContinuation,
                          AsVector(" ")));
    EXPECT_CALL(
        *event_interface_,
        OnDataFrameVector(false, WebSocketFrameHeader::kOpCodeContinuation,
                          AsVector("SMALL")));
    EXPECT_CALL(
        *event_interface_,
        OnDataFrameVector(false, WebSocketFrameHeader::kOpCodeContinuation,
                          AsVector(" ")));
    EXPECT_CALL(
        *event_interface_,
        OnDataFrameVector(true, WebSocketFrameHeader::kOpCodeContinuation,
                          AsVector("FRAMES")));
  }

  CreateChannelAndConnectSuccessfully();
  base::RunLoop().RunUntilIdle();
}

// A message can consist of one frame with null payload.
TEST_F(WebSocketChannelEventInterfaceTest, NullMessage) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, nullptr}};
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::SYNC, OK, frames);
  set_stream(std::move(stream));
  EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
  EXPECT_CALL(
      *event_interface_,
      OnDataFrameVector(true, WebSocketFrameHeader::kOpCodeText, AsVector("")));
  CreateChannelAndConnectSuccessfully();
}

// Connection closed by the remote host without a closing handshake.
TEST_F(WebSocketChannelEventInterfaceTest, AsyncAbnormalClosure) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  stream->PrepareReadFramesError(ReadableFakeWebSocketStream::ASYNC,
                                 ERR_CONNECTION_CLOSED);
  set_stream(std::move(stream));
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
    EXPECT_CALL(*event_interface_,
                OnDropChannel(false, kWebSocketErrorAbnormalClosure, _));
  }

  CreateChannelAndConnectSuccessfully();
  base::RunLoop().RunUntilIdle();
}

// A connection reset should produce the same event as an unexpected closure.
TEST_F(WebSocketChannelEventInterfaceTest, ConnectionReset) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  stream->PrepareReadFramesError(ReadableFakeWebSocketStream::ASYNC,
                                 ERR_CONNECTION_RESET);
  set_stream(std::move(stream));
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
    EXPECT_CALL(*event_interface_,
                OnDropChannel(false, kWebSocketErrorAbnormalClosure, _));
  }

  CreateChannelAndConnectSuccessfully();
  base::RunLoop().RunUntilIdle();
}

// RFC6455 5.1 "A client MUST close a connection if it detects a masked frame."
TEST_F(WebSocketChannelEventInterfaceTest, MaskedFramesAreRejected) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, MASKED, "HELLO"}};

  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, frames);
  set_stream(std::move(stream));
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
    EXPECT_CALL(
        *event_interface_,
        OnFailChannel(
            "A server must not mask any frames that it sends to the client."));
  }

  CreateChannelAndConnectSuccessfully();
  base::RunLoop().RunUntilIdle();
}

// RFC6455 5.2 "If an unknown opcode is received, the receiving endpoint MUST
// _Fail the WebSocket Connection_."
TEST_F(WebSocketChannelEventInterfaceTest, UnknownOpCodeIsRejected) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  static const InitFrame frames[] = {{FINAL_FRAME, 4, NOT_MASKED, "HELLO"}};

  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, frames);
  set_stream(std::move(stream));
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
    EXPECT_CALL(*event_interface_,
                OnFailChannel("Unrecognized frame opcode: 4"));
  }

  CreateChannelAndConnectSuccessfully();
  base::RunLoop().RunUntilIdle();
}

// RFC6455 5.4 "Control frames ... MAY be injected in the middle of a
// fragmented message."
TEST_F(WebSocketChannelEventInterfaceTest, ControlFrameInDataMessage) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  // We have one message of type Text split into two frames. In the middle is a
  // control message of type Pong.
  static const InitFrame frames1[] = {
      {NOT_FINAL_FRAME, WebSocketFrameHeader::kOpCodeText,
       NOT_MASKED,      "SPLIT "}};
  static const InitFrame frames2[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodePong, NOT_MASKED, ""}};
  static const InitFrame frames3[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeContinuation,
       NOT_MASKED,  "MESSAGE"}};
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, frames1);
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, frames2);
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, frames3);
  set_stream(std::move(stream));
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
    EXPECT_CALL(*event_interface_,
                OnDataFrameVector(false, WebSocketFrameHeader::kOpCodeText,
                                  AsVector("SPLIT ")));
    EXPECT_CALL(
        *event_interface_,
        OnDataFrameVector(true, WebSocketFrameHeader::kOpCodeContinuation,
                          AsVector("MESSAGE")));
  }

  CreateChannelAndConnectSuccessfully();
  base::RunLoop().RunUntilIdle();
}

// It seems redundant to repeat the entirety of the above test, so just test a
// Pong with null data.
TEST_F(WebSocketChannelEventInterfaceTest, PongWithNullData) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodePong, NOT_MASKED, nullptr}};
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, frames);
  set_stream(std::move(stream));
  EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));

  CreateChannelAndConnectSuccessfully();
  base::RunLoop().RunUntilIdle();
}

// If a frame has an invalid header, then the connection is closed and
// subsequent frames must not trigger events.
TEST_F(WebSocketChannelEventInterfaceTest, FrameAfterInvalidFrame) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  static const InitFrame frames[] = {
      {NOT_FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, MASKED, "HELLO"},
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, " WORLD"}};

  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, frames);
  set_stream(std::move(stream));
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
    EXPECT_CALL(
        *event_interface_,
        OnFailChannel(
            "A server must not mask any frames that it sends to the client."));
  }

  CreateChannelAndConnectSuccessfully();
  base::RunLoop().RunUntilIdle();
}

// If the renderer sends lots of small writes, we don't want to update the quota
// for each one.
TEST_F(WebSocketChannelEventInterfaceTest, SmallWriteDoesntUpdateQuota) {
  set_stream(std::make_unique<WriteableFakeWebSocketStream>());
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
  }

  CreateChannelAndConnectSuccessfully();
  channel_->SendFrame(true, WebSocketFrameHeader::kOpCodeText, AsIOBuffer("B"),
                      1U);
}

// If we send enough to go below |send_quota_low_water_mark_| we should get our
// quota refreshed.
TEST_F(WebSocketChannelEventInterfaceTest, LargeWriteUpdatesQuota) {
  set_stream(std::make_unique<WriteableFakeWebSocketStream>());
  // We use this checkpoint object to verify that the quota update comes after
  // the write.
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*event_interface_, OnSendFlowControlQuotaAdded(_));
    EXPECT_CALL(checkpoint, Call(2));
  }

  CreateChannelAndConnectSuccessfully();
  checkpoint.Call(1);
  channel_->SendFrame(true, WebSocketFrameHeader::kOpCodeText,
                      AsIOBuffer(std::string(kDefaultInitialQuota, 'B')),
                      kDefaultInitialQuota);
  checkpoint.Call(2);
}

// Verify that our quota actually is refreshed when we are told it is.
TEST_F(WebSocketChannelEventInterfaceTest, QuotaReallyIsRefreshed) {
  set_stream(std::make_unique<WriteableFakeWebSocketStream>());
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*event_interface_, OnSendFlowControlQuotaAdded(_));
    EXPECT_CALL(checkpoint, Call(2));
    // If quota was not really refreshed, we would get an OnDropChannel()
    // message.
    EXPECT_CALL(*event_interface_, OnSendFlowControlQuotaAdded(_));
    EXPECT_CALL(checkpoint, Call(3));
  }

  CreateChannelAndConnectSuccessfully();
  checkpoint.Call(1);
  channel_->SendFrame(true, WebSocketFrameHeader::kOpCodeText,
                      AsIOBuffer(std::string(kDefaultQuotaRefreshTrigger, 'D')),
                      kDefaultQuotaRefreshTrigger);
  checkpoint.Call(2);
  // We should have received more quota at this point.
  channel_->SendFrame(true, WebSocketFrameHeader::kOpCodeText,
                      AsIOBuffer(std::string(kDefaultQuotaRefreshTrigger, 'E')),
                      kDefaultQuotaRefreshTrigger);
  checkpoint.Call(3);
}

// If we send more than the available quota then the connection will be closed
// with an error.
TEST_F(WebSocketChannelEventInterfaceTest, WriteOverQuotaIsRejected) {
  set_stream(std::make_unique<WriteableFakeWebSocketStream>());
  {
    InSequence s;
    EXPECT_CALL(*event_interface_,
                OnAddChannelResponse(_, _, kDefaultInitialQuota));
    EXPECT_CALL(*event_interface_, OnFailChannel("Send quota exceeded"));
  }

  CreateChannelAndConnectSuccessfully();
  channel_->SendFrame(true, WebSocketFrameHeader::kOpCodeText,
                      AsIOBuffer(std::string(kDefaultInitialQuota + 1, 'C')),
                      kDefaultInitialQuota + 1);
}

// If a write fails, the channel is dropped.
TEST_F(WebSocketChannelEventInterfaceTest, FailedWrite) {
  set_stream(std::make_unique<UnWriteableFakeWebSocketStream>());
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*event_interface_,
                OnDropChannel(false, kWebSocketErrorAbnormalClosure, _));
    EXPECT_CALL(checkpoint, Call(2));
  }

  CreateChannelAndConnectSuccessfully();
  checkpoint.Call(1);

  channel_->SendFrame(true, WebSocketFrameHeader::kOpCodeText, AsIOBuffer("H"),
                      1U);
  checkpoint.Call(2);
}

// OnDropChannel() is called exactly once when StartClosingHandshake() is used.
TEST_F(WebSocketChannelEventInterfaceTest, SendCloseDropsChannel) {
  set_stream(std::make_unique<EchoeyFakeWebSocketStream>());
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
    EXPECT_CALL(*event_interface_,
                OnDropChannel(true, kWebSocketNormalClosure, "Fred"));
  }

  CreateChannelAndConnectSuccessfully();

  ASSERT_EQ(CHANNEL_ALIVE,
            channel_->StartClosingHandshake(kWebSocketNormalClosure, "Fred"));
  base::RunLoop().RunUntilIdle();
}

// StartClosingHandshake() also works before connection completes, and calls
// OnDropChannel.
TEST_F(WebSocketChannelEventInterfaceTest, CloseDuringConnection) {
  EXPECT_CALL(*event_interface_,
              OnDropChannel(false, kWebSocketErrorAbnormalClosure, ""));

  CreateChannelAndConnect();
  ASSERT_EQ(CHANNEL_DELETED,
            channel_->StartClosingHandshake(kWebSocketNormalClosure, "Joe"));
}

// OnDropChannel() is only called once when a write() on the socket triggers a
// connection reset.
TEST_F(WebSocketChannelEventInterfaceTest, OnDropChannelCalledOnce) {
  set_stream(std::make_unique<ResetOnWriteFakeWebSocketStream>());
  EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));

  EXPECT_CALL(*event_interface_,
              OnDropChannel(false, kWebSocketErrorAbnormalClosure, ""))
      .Times(1);

  CreateChannelAndConnectSuccessfully();

  channel_->SendFrame(true, WebSocketFrameHeader::kOpCodeText,
                      AsIOBuffer("yt?"), 3U);
  base::RunLoop().RunUntilIdle();
}

// When the remote server sends a Close frame with an empty payload,
// WebSocketChannel should report code 1005, kWebSocketErrorNoStatusReceived.
TEST_F(WebSocketChannelEventInterfaceTest, CloseWithNoPayloadGivesStatus1005) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose, NOT_MASKED, ""}};
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::SYNC, OK, frames);
  stream->PrepareReadFramesError(ReadableFakeWebSocketStream::SYNC,
                                 ERR_CONNECTION_CLOSED);
  set_stream(std::move(stream));
  EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
  EXPECT_CALL(*event_interface_, OnClosingHandshake());
  EXPECT_CALL(*event_interface_,
              OnDropChannel(true, kWebSocketErrorNoStatusReceived, _));

  CreateChannelAndConnectSuccessfully();
}

// A version of the above test with null payload.
TEST_F(WebSocketChannelEventInterfaceTest,
       CloseWithNullPayloadGivesStatus1005) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose, NOT_MASKED, nullptr}};
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::SYNC, OK, frames);
  stream->PrepareReadFramesError(ReadableFakeWebSocketStream::SYNC,
                                 ERR_CONNECTION_CLOSED);
  set_stream(std::move(stream));
  EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
  EXPECT_CALL(*event_interface_, OnClosingHandshake());
  EXPECT_CALL(*event_interface_,
              OnDropChannel(true, kWebSocketErrorNoStatusReceived, _));

  CreateChannelAndConnectSuccessfully();
}

// If ReadFrames() returns ERR_WS_PROTOCOL_ERROR, then the connection must be
// failed.
TEST_F(WebSocketChannelEventInterfaceTest, SyncProtocolErrorGivesStatus1002) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  stream->PrepareReadFramesError(ReadableFakeWebSocketStream::SYNC,
                                 ERR_WS_PROTOCOL_ERROR);
  set_stream(std::move(stream));
  EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));

  EXPECT_CALL(*event_interface_, OnFailChannel("Invalid frame header"));

  CreateChannelAndConnectSuccessfully();
}

// Async version of above test.
TEST_F(WebSocketChannelEventInterfaceTest, AsyncProtocolErrorGivesStatus1002) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  stream->PrepareReadFramesError(ReadableFakeWebSocketStream::ASYNC,
                                 ERR_WS_PROTOCOL_ERROR);
  set_stream(std::move(stream));
  EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
  EXPECT_CALL(*event_interface_, OnFailChannel("Invalid frame header"));

  CreateChannelAndConnectSuccessfully();
  base::RunLoop().RunUntilIdle();
}

TEST_F(WebSocketChannelEventInterfaceTest, StartHandshakeRequest) {
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
    EXPECT_CALL(*event_interface_, OnStartOpeningHandshakeCalled());
  }

  CreateChannelAndConnectSuccessfully();

  auto request_info = std::make_unique<WebSocketHandshakeRequestInfo>(
      GURL("ws://www.example.com/"), base::Time());
  connect_data_.argument_saver.connect_delegate->OnStartOpeningHandshake(
      std::move(request_info));

  base::RunLoop().RunUntilIdle();
}

TEST_F(WebSocketChannelEventInterfaceTest, FinishHandshakeRequest) {
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
    EXPECT_CALL(*event_interface_, OnFinishOpeningHandshakeCalled());
  }

  CreateChannelAndConnectSuccessfully();

  auto response_headers =
      base::MakeRefCounted<HttpResponseHeaders>("HTTP/1.1 200 OK");
  auto response_info = std::make_unique<WebSocketHandshakeResponseInfo>(
      GURL("ws://www.example.com/"), response_headers, IPEndPoint(),
      base::Time());
  connect_data_.argument_saver.connect_delegate->OnFinishOpeningHandshake(
      std::move(response_info));
  base::RunLoop().RunUntilIdle();
}

TEST_F(WebSocketChannelEventInterfaceTest, FailJustAfterHandshake) {
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnStartOpeningHandshakeCalled());
    EXPECT_CALL(*event_interface_, OnFinishOpeningHandshakeCalled());
    EXPECT_CALL(*event_interface_, OnFailChannel("bye"));
  }

  CreateChannelAndConnect();

  WebSocketStream::ConnectDelegate* connect_delegate =
      connect_data_.argument_saver.connect_delegate.get();
  GURL url("ws://www.example.com/");
  auto request_info =
      std::make_unique<WebSocketHandshakeRequestInfo>(url, base::Time());
  auto response_headers =
      base::MakeRefCounted<HttpResponseHeaders>("HTTP/1.1 200 OK");
  auto response_info = std::make_unique<WebSocketHandshakeResponseInfo>(
      url, response_headers, IPEndPoint(), base::Time());
  connect_delegate->OnStartOpeningHandshake(std::move(request_info));
  connect_delegate->OnFinishOpeningHandshake(std::move(response_info));

  connect_delegate->OnFailure("bye");
  base::RunLoop().RunUntilIdle();
}

// Any frame after close is invalid. This test uses a Text frame. See also
// test "PingAfterCloseIfRejected".
TEST_F(WebSocketChannelEventInterfaceTest, DataAfterCloseIsRejected) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose, NOT_MASKED,
       CLOSE_DATA(NORMAL_CLOSURE, "OK")},
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, "Payload"}};
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::SYNC, OK, frames);
  set_stream(std::move(stream));
  EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));

  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnClosingHandshake());
    EXPECT_CALL(*event_interface_,
                OnFailChannel("Data frame received after close"));
  }

  CreateChannelAndConnectSuccessfully();
}

// A Close frame with a one-byte payload elicits a specific console error
// message.
TEST_F(WebSocketChannelEventInterfaceTest, OneByteClosePayloadMessage) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose, NOT_MASKED, "\x03"}};
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::SYNC, OK, frames);
  set_stream(std::move(stream));
  EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
  EXPECT_CALL(
      *event_interface_,
      OnFailChannel(
          "Received a broken close frame containing an invalid size body."));

  CreateChannelAndConnectSuccessfully();
}

// A Close frame with a reserved status code also elicits a specific console
// error message.
TEST_F(WebSocketChannelEventInterfaceTest, ClosePayloadReservedStatusMessage) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose,
       NOT_MASKED,  CLOSE_DATA(ABNORMAL_CLOSURE, "Not valid on wire")}};
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::SYNC, OK, frames);
  set_stream(std::move(stream));
  EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
  EXPECT_CALL(
      *event_interface_,
      OnFailChannel(
          "Received a broken close frame containing a reserved status code."));

  CreateChannelAndConnectSuccessfully();
}

// A Close frame with invalid UTF-8 also elicits a specific console error
// message.
TEST_F(WebSocketChannelEventInterfaceTest, ClosePayloadInvalidReason) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose,
       NOT_MASKED,  CLOSE_DATA(NORMAL_CLOSURE, "\xFF")}};
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::SYNC, OK, frames);
  set_stream(std::move(stream));
  EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
  EXPECT_CALL(
      *event_interface_,
      OnFailChannel(
          "Received a broken close frame containing invalid UTF-8."));

  CreateChannelAndConnectSuccessfully();
}

// The reserved bits must all be clear on received frames. Extensions should
// clear the bits when they are set correctly before passing on the frame.
TEST_F(WebSocketChannelEventInterfaceTest, ReservedBitsMustNotBeSet) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText,
       NOT_MASKED,  "sakana"}};
  // It is not worth adding support for reserved bits to InitFrame just for this
  // one test, so set the bit manually.
  std::vector<std::unique_ptr<WebSocketFrame>> raw_frames =
      CreateFrameVector(frames, &result_frame_data_);
  raw_frames[0]->header.reserved1 = true;
  stream->PrepareRawReadFrames(ReadableFakeWebSocketStream::SYNC, OK,
                               std::move(raw_frames));
  set_stream(std::move(stream));
  EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
  EXPECT_CALL(*event_interface_,
              OnFailChannel(
                  "One or more reserved bits are on: reserved1 = 1, "
                  "reserved2 = 0, reserved3 = 0"));

  CreateChannelAndConnectSuccessfully();
}

// The closing handshake times out and sends an OnDropChannel event if no
// response to the client Close message is received.
TEST_F(WebSocketChannelEventInterfaceTest,
       ClientInitiatedClosingHandshakeTimesOut) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  stream->PrepareReadFramesError(ReadableFakeWebSocketStream::SYNC,
                                 ERR_IO_PENDING);
  set_stream(std::move(stream));
  EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
  // This checkpoint object verifies that the OnDropChannel message comes after
  // the timeout.
  Checkpoint checkpoint;
  TestClosure completion;
  {
    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*event_interface_,
                OnDropChannel(false, kWebSocketErrorAbnormalClosure, _))
        .WillOnce(InvokeClosure(completion.closure()));
  }
  CreateChannelAndConnectSuccessfully();
  // OneShotTimer is not very friendly to testing; there is no apparent way to
  // set an expectation on it. Instead the tests need to infer that the timeout
  // was fired by the behaviour of the WebSocketChannel object.
  channel_->SetClosingHandshakeTimeoutForTesting(
      TimeDelta::FromMilliseconds(kVeryTinyTimeoutMillis));
  channel_->SetUnderlyingConnectionCloseTimeoutForTesting(
      TimeDelta::FromMilliseconds(kVeryBigTimeoutMillis));
  ASSERT_EQ(CHANNEL_ALIVE,
            channel_->StartClosingHandshake(kWebSocketNormalClosure, ""));
  checkpoint.Call(1);
  completion.WaitForResult();
}

// The closing handshake times out and sends an OnDropChannel event if a Close
// message is received but the connection isn't closed by the remote host.
TEST_F(WebSocketChannelEventInterfaceTest,
       ServerInitiatedClosingHandshakeTimesOut) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose,
       NOT_MASKED,  CLOSE_DATA(NORMAL_CLOSURE, "OK")}};
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, frames);
  set_stream(std::move(stream));
  EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
  Checkpoint checkpoint;
  TestClosure completion;
  {
    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*event_interface_, OnClosingHandshake());
    EXPECT_CALL(*event_interface_,
                OnDropChannel(false, kWebSocketErrorAbnormalClosure, _))
        .WillOnce(InvokeClosure(completion.closure()));
  }
  CreateChannelAndConnectSuccessfully();
  channel_->SetClosingHandshakeTimeoutForTesting(
      TimeDelta::FromMilliseconds(kVeryBigTimeoutMillis));
  channel_->SetUnderlyingConnectionCloseTimeoutForTesting(
      TimeDelta::FromMilliseconds(kVeryTinyTimeoutMillis));
  checkpoint.Call(1);
  completion.WaitForResult();
}

// We should stop calling ReadFrames() when data frames are pending.
TEST_F(WebSocketChannelStreamTest, PendingDataFrameStopsReadFrames) {
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, "FOUR"}};
  Checkpoint checkpoint;

  {
    InSequence s;
    EXPECT_CALL(*event_interface_, HasPendingDataFrames())
        .WillOnce(Return(false));
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
        .WillOnce(ReturnFrames(&frames, &result_frame_data_));
    EXPECT_CALL(*event_interface_, HasPendingDataFrames())
        .WillOnce(Return(true));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*event_interface_, HasPendingDataFrames())
        .WillOnce(Return(true));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*event_interface_, HasPendingDataFrames())
        .WillOnce(Return(false));
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
        .WillOnce(Return(ERR_IO_PENDING));
  }

  CreateChannelAndConnectSuccessfully();
  checkpoint.Call(1);
  ASSERT_EQ(CHANNEL_ALIVE, channel_->ReadFrames());
  checkpoint.Call(2);
  ASSERT_EQ(CHANNEL_ALIVE, channel_->ReadFrames());
}

TEST_F(WebSocketChannelEventInterfaceTest, SingleFrameMessage) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, "FOUR"}};
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::SYNC, OK, frames);
  set_stream(std::move(stream));
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
    EXPECT_CALL(*event_interface_,
                OnDataFrameVector(true, WebSocketFrameHeader::kOpCodeText,
                                  AsVector("FOUR")));
  }

  CreateChannelAndConnectSuccessfully();
  ASSERT_EQ(CHANNEL_ALIVE, channel_->ReadFrames());
}

TEST_F(WebSocketChannelEventInterfaceTest, EmptyMessage) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED,
       "FIRST MESSAGE"},
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, nullptr},
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED,
       "THIRD MESSAGE"}};
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::SYNC, OK, frames);
  set_stream(std::move(stream));
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
    EXPECT_CALL(*event_interface_,
                OnDataFrameVector(true, WebSocketFrameHeader::kOpCodeText,
                                  AsVector("FIRST MESSAGE")));
    EXPECT_CALL(*event_interface_,
                OnDataFrameVector(true, WebSocketFrameHeader::kOpCodeText,
                                  AsVector("")));
    EXPECT_CALL(*event_interface_,
                OnDataFrameVector(true, WebSocketFrameHeader::kOpCodeText,
                                  AsVector("THIRD MESSAGE")));
  }

  CreateChannelAndConnectSuccessfully();
  ASSERT_EQ(CHANNEL_ALIVE, channel_->ReadFrames());
}

// A close frame should not overtake data frames.
TEST_F(WebSocketChannelEventInterfaceTest,
       CloseFrameShouldNotOvertakeDataFrames) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  static const InitFrame frames[] = {
      {NOT_FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED,
       "FIRST "},
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeContinuation, NOT_MASKED,
       "MESSAGE"},
      {NOT_FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED,
       "SECOND "},
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose, NOT_MASKED,
       CLOSE_DATA(NORMAL_CLOSURE, "GOOD BYE")},
  };
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::SYNC, OK, frames);
  set_stream(std::move(stream));
  Checkpoint checkpoint;
  InSequence s;
  EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
  EXPECT_CALL(*event_interface_, HasPendingDataFrames()).WillOnce(Return(true));
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*event_interface_, HasPendingDataFrames())
      .WillOnce(Return(false));
  EXPECT_CALL(*event_interface_,
              OnDataFrameVector(false, WebSocketFrameHeader::kOpCodeText,
                                AsVector("FIRST ")));
  EXPECT_CALL(*event_interface_,
              OnDataFrameVector(true, WebSocketFrameHeader::kOpCodeContinuation,
                                AsVector("MESSAGE")));
  EXPECT_CALL(*event_interface_,
              OnDataFrameVector(false, WebSocketFrameHeader::kOpCodeText,
                                AsVector("SECOND ")));
  EXPECT_CALL(*event_interface_, OnClosingHandshake());

  CreateChannelAndConnectSuccessfully();
  checkpoint.Call(1);
  ASSERT_EQ(CHANNEL_ALIVE, channel_->ReadFrames());
}

// RFC6455 5.1 "a client MUST mask all frames that it sends to the server".
// WebSocketChannel actually only sets the mask bit in the header, it doesn't
// perform masking itself (not all transports actually use masking).
TEST_F(WebSocketChannelStreamTest, SentFramesAreMasked) {
  static const InitFrame expected[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText,
       MASKED,      "NEEDS MASKING"}};
  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillOnce(Return(ERR_IO_PENDING));
  EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected), _))
      .WillOnce(Return(OK));

  CreateChannelAndConnectSuccessfully();
  channel_->SendFrame(true, WebSocketFrameHeader::kOpCodeText,
                      AsIOBuffer("NEEDS MASKING"), 13U);
}

// RFC6455 5.5.1 "The application MUST NOT send any more data frames after
// sending a Close frame."
TEST_F(WebSocketChannelStreamTest, NothingIsSentAfterClose) {
  static const InitFrame expected[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose,
       MASKED,      CLOSE_DATA(NORMAL_CLOSURE, "Success")}};
  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillOnce(Return(ERR_IO_PENDING));
  EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected), _))
      .WillOnce(Return(OK));

  CreateChannelAndConnectSuccessfully();
  ASSERT_EQ(CHANNEL_ALIVE, channel_->StartClosingHandshake(1000, "Success"));
  channel_->SendFrame(true, WebSocketFrameHeader::kOpCodeText,
                      AsIOBuffer("SHOULD  BE IGNORED"), 18U);
}

// RFC6455 5.5.1 "If an endpoint receives a Close frame and did not previously
// send a Close frame, the endpoint MUST send a Close frame in response."
TEST_F(WebSocketChannelStreamTest, CloseIsEchoedBack) {
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose,
       NOT_MASKED,  CLOSE_DATA(NORMAL_CLOSURE, "Close")}};
  static const InitFrame expected[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose,
       MASKED,      CLOSE_DATA(NORMAL_CLOSURE, "Close")}};
  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillOnce(ReturnFrames(&frames, &result_frame_data_))
      .WillRepeatedly(Return(ERR_IO_PENDING));
  EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected), _))
      .WillOnce(Return(OK));

  CreateChannelAndConnectSuccessfully();
}

// The converse of the above case; after sending a Close frame, we should not
// send another one.
TEST_F(WebSocketChannelStreamTest, CloseOnlySentOnce) {
  static const InitFrame expected[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose,
       MASKED,      CLOSE_DATA(NORMAL_CLOSURE, "Close")}};
  static const InitFrame frames_init[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose,
       NOT_MASKED,  CLOSE_DATA(NORMAL_CLOSURE, "Close")}};

  // We store the parameters that were passed to ReadFrames() so that we can
  // call them explicitly later.
  CompletionOnceCallback read_callback;
  std::vector<std::unique_ptr<WebSocketFrame>>* frames = nullptr;

  // Use a checkpoint to make the ordering of events clearer.
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
        .WillOnce(DoAll(SaveArg<0>(&frames), SaveArg<1>(&read_callback),
                        Return(ERR_IO_PENDING)));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected), _))
        .WillOnce(Return(OK));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
        .WillOnce(Return(ERR_IO_PENDING));
    EXPECT_CALL(checkpoint, Call(3));
    // WriteFrames() must not be called again. GoogleMock will ensure that the
    // test fails if it is.
  }

  CreateChannelAndConnectSuccessfully();
  checkpoint.Call(1);
  ASSERT_EQ(CHANNEL_ALIVE,
            channel_->StartClosingHandshake(kWebSocketNormalClosure, "Close"));
  checkpoint.Call(2);
  *frames = CreateFrameVector(frames_init, &result_frame_data_);
  std::move(read_callback).Run(OK);
  checkpoint.Call(3);
}

// Invalid close status codes should not be sent on the network.
TEST_F(WebSocketChannelStreamTest, InvalidCloseStatusCodeNotSent) {
  static const InitFrame expected[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose,
       MASKED,      CLOSE_DATA(SERVER_ERROR, "")}};

  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillOnce(Return(ERR_IO_PENDING));

  EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected), _));

  CreateChannelAndConnectSuccessfully();
  ASSERT_EQ(CHANNEL_ALIVE, channel_->StartClosingHandshake(999, ""));
}

// A Close frame with a reason longer than 123 bytes cannot be sent on the
// network.
TEST_F(WebSocketChannelStreamTest, LongCloseReasonNotSent) {
  static const InitFrame expected[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose,
       MASKED,      CLOSE_DATA(SERVER_ERROR, "")}};

  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillOnce(Return(ERR_IO_PENDING));

  EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected), _));

  CreateChannelAndConnectSuccessfully();
  ASSERT_EQ(CHANNEL_ALIVE,
            channel_->StartClosingHandshake(1000, std::string(124, 'A')));
}

// We generate code 1005, kWebSocketErrorNoStatusReceived, when there is no
// status in the Close message from the other side. Code 1005 is not allowed to
// appear on the wire, so we should not echo it back. See test
// CloseWithNoPayloadGivesStatus1005, above, for confirmation that code 1005 is
// correctly generated internally.
TEST_F(WebSocketChannelStreamTest, Code1005IsNotEchoed) {
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose, NOT_MASKED, ""}};
  static const InitFrame expected[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose, MASKED, ""}};
  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillOnce(ReturnFrames(&frames, &result_frame_data_))
      .WillRepeatedly(Return(ERR_IO_PENDING));
  EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected), _))
      .WillOnce(Return(OK));

  CreateChannelAndConnectSuccessfully();
}

TEST_F(WebSocketChannelStreamTest, Code1005IsNotEchoedNull) {
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose, NOT_MASKED, nullptr}};
  static const InitFrame expected[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose, MASKED, ""}};
  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillOnce(ReturnFrames(&frames, &result_frame_data_))
      .WillRepeatedly(Return(ERR_IO_PENDING));
  EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected), _))
      .WillOnce(Return(OK));

  CreateChannelAndConnectSuccessfully();
}

// Receiving an invalid UTF-8 payload in a Close frame causes us to fail the
// connection.
TEST_F(WebSocketChannelStreamTest, CloseFrameInvalidUtf8) {
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose,
       NOT_MASKED, CLOSE_DATA(NORMAL_CLOSURE, "\xFF")}};
  static const InitFrame expected[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose,
       MASKED, CLOSE_DATA(PROTOCOL_ERROR, "Invalid UTF-8 in Close frame")}};

  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillOnce(ReturnFrames(&frames, &result_frame_data_))
      .WillRepeatedly(Return(ERR_IO_PENDING));
  EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected), _))
      .WillOnce(Return(OK));
  EXPECT_CALL(*mock_stream_, Close());

  CreateChannelAndConnectSuccessfully();
}

// RFC6455 5.5.2 "Upon receipt of a Ping frame, an endpoint MUST send a Pong
// frame in response"
// 5.5.3 "A Pong frame sent in response to a Ping frame must have identical
// "Application data" as found in the message body of the Ping frame being
// replied to."
TEST_F(WebSocketChannelStreamTest, PingRepliedWithPong) {
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodePing,
       NOT_MASKED,  "Application data"}};
  static const InitFrame expected[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodePong,
       MASKED,      "Application data"}};
  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillOnce(ReturnFrames(&frames, &result_frame_data_))
      .WillRepeatedly(Return(ERR_IO_PENDING));
  EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected), _))
      .WillOnce(Return(OK));

  CreateChannelAndConnectSuccessfully();
}

// A ping with a null payload should be responded to with a Pong with a null
// payload.
TEST_F(WebSocketChannelStreamTest, NullPingRepliedWithNullPong) {
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodePing, NOT_MASKED, nullptr}};
  static const InitFrame expected[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodePong, MASKED, nullptr}};
  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillOnce(ReturnFrames(&frames, &result_frame_data_))
      .WillRepeatedly(Return(ERR_IO_PENDING));
  EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected), _))
      .WillOnce(Return(OK));

  CreateChannelAndConnectSuccessfully();
}

TEST_F(WebSocketChannelStreamTest, PongInTheMiddleOfDataMessage) {
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodePing,
       NOT_MASKED,  "Application data"}};
  static const InitFrame expected1[] = {
      {NOT_FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, MASKED, "Hello "}};
  static const InitFrame expected2[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodePong,
       MASKED,      "Application data"}};
  static const InitFrame expected3[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeContinuation,
       MASKED,      "World"}};
  std::vector<std::unique_ptr<WebSocketFrame>>* read_frames;
  CompletionOnceCallback read_callback;
  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillOnce(DoAll(SaveArg<0>(&read_frames), SaveArg<1>(&read_callback),
                      Return(ERR_IO_PENDING)))
      .WillRepeatedly(Return(ERR_IO_PENDING));
  {
    InSequence s;

    EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected1), _))
        .WillOnce(Return(OK));
    EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected2), _))
        .WillOnce(Return(OK));
    EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected3), _))
        .WillOnce(Return(OK));
  }

  CreateChannelAndConnectSuccessfully();
  channel_->SendFrame(false, WebSocketFrameHeader::kOpCodeText,
                      AsIOBuffer("Hello "), 6U);
  *read_frames = CreateFrameVector(frames, &result_frame_data_);
  std::move(read_callback).Run(OK);
  channel_->SendFrame(true, WebSocketFrameHeader::kOpCodeContinuation,
                      AsIOBuffer("World"), 5U);
}

// WriteFrames() may not be called until the previous write has completed.
// WebSocketChannel must buffer writes that happen in the meantime.
TEST_F(WebSocketChannelStreamTest, WriteFramesOneAtATime) {
  static const InitFrame expected1[] = {
      {NOT_FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, MASKED, "Hello "}};
  static const InitFrame expected2[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, MASKED, "World"}};
  CompletionOnceCallback write_callback;
  Checkpoint checkpoint;

  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillOnce(Return(ERR_IO_PENDING));
  {
    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected1), _))
        .WillOnce(DoAll(SaveArg<1>(&write_callback), Return(ERR_IO_PENDING)));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected2), _))
        .WillOnce(Return(ERR_IO_PENDING));
    EXPECT_CALL(checkpoint, Call(3));
  }

  CreateChannelAndConnectSuccessfully();
  checkpoint.Call(1);
  channel_->SendFrame(false, WebSocketFrameHeader::kOpCodeText,
                      AsIOBuffer("Hello "), 6U);
  channel_->SendFrame(true, WebSocketFrameHeader::kOpCodeText,
                      AsIOBuffer("World"), 5U);
  checkpoint.Call(2);
  std::move(write_callback).Run(OK);
  checkpoint.Call(3);
}

// WebSocketChannel must buffer frames while it is waiting for a write to
// complete, and then send them in a single batch. The batching behaviour is
// important to get good throughput in the "many small messages" case.
TEST_F(WebSocketChannelStreamTest, WaitingMessagesAreBatched) {
  static const char input_letters[] = "Hello";
  static const InitFrame expected1[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, MASKED, "H"}};
  static const InitFrame expected2[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, MASKED, "e"},
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, MASKED, "l"},
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, MASKED, "l"},
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, MASKED, "o"}};
  CompletionOnceCallback write_callback;

  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillOnce(Return(ERR_IO_PENDING));
  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected1), _))
        .WillOnce(DoAll(SaveArg<1>(&write_callback), Return(ERR_IO_PENDING)));
    EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected2), _))
        .WillOnce(Return(ERR_IO_PENDING));
  }

  CreateChannelAndConnectSuccessfully();
  for (size_t i = 0; i < strlen(input_letters); ++i) {
    channel_->SendFrame(true, WebSocketFrameHeader::kOpCodeText,
                        AsIOBuffer(std::string(1, input_letters[i])), 1U);
  }
  std::move(write_callback).Run(OK);
}

// When the renderer sends more on a channel than it has quota for, we send the
// remote server a kWebSocketErrorGoingAway error code.
TEST_F(WebSocketChannelStreamTest, SendGoingAwayOnRendererQuotaExceeded) {
  static const InitFrame expected[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose,
       MASKED,      CLOSE_DATA(GOING_AWAY, "")}};
  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillOnce(Return(ERR_IO_PENDING));
  EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected), _))
      .WillOnce(Return(OK));
  EXPECT_CALL(*mock_stream_, Close());

  CreateChannelAndConnectSuccessfully();
  channel_->SendFrame(true, WebSocketFrameHeader::kOpCodeText,
                      AsIOBuffer(std::string(kDefaultInitialQuota + 1, 'C')),
                      kDefaultInitialQuota + 1);
}

// For convenience, most of these tests use Text frames. However, the WebSocket
// protocol also has Binary frames and those need to be 8-bit clean. For the
// sake of completeness, this test verifies that they are.
TEST_F(WebSocketChannelStreamTest, WrittenBinaryFramesAre8BitClean) {
  std::vector<std::unique_ptr<WebSocketFrame>>* frames = nullptr;

  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillOnce(Return(ERR_IO_PENDING));
  EXPECT_CALL(*mock_stream_, WriteFramesInternal(_, _))
      .WillOnce(DoAll(SaveArg<0>(&frames), Return(ERR_IO_PENDING)));

  CreateChannelAndConnectSuccessfully();
  channel_->SendFrame(
      true, WebSocketFrameHeader::kOpCodeBinary,
      AsIOBuffer(std::string(kBinaryBlob, kBinaryBlob + kBinaryBlobSize)),
      kBinaryBlobSize);
  ASSERT_TRUE(frames != nullptr);
  ASSERT_EQ(1U, frames->size());
  const WebSocketFrame* out_frame = (*frames)[0].get();
  EXPECT_EQ(kBinaryBlobSize, out_frame->header.payload_length);
  ASSERT_TRUE(out_frame->payload);
  EXPECT_EQ(0, memcmp(kBinaryBlob, out_frame->payload, kBinaryBlobSize));
}

// Test the read path for 8-bit cleanliness as well.
TEST_F(WebSocketChannelEventInterfaceTest, ReadBinaryFramesAre8BitClean) {
  auto frame =
      std::make_unique<WebSocketFrame>(WebSocketFrameHeader::kOpCodeBinary);
  WebSocketFrameHeader& frame_header = frame->header;
  frame_header.final = true;
  frame_header.payload_length = kBinaryBlobSize;
  auto buffer = base::MakeRefCounted<IOBuffer>(kBinaryBlobSize);
  memcpy(buffer->data(), kBinaryBlob, kBinaryBlobSize);
  frame->payload = buffer->data();
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  frames.push_back(std::move(frame));
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  stream->PrepareRawReadFrames(ReadableFakeWebSocketStream::SYNC, OK,
                               std::move(frames));
  set_stream(std::move(stream));
  EXPECT_CALL(*event_interface_, OnAddChannelResponse(_, _, _));
  EXPECT_CALL(
      *event_interface_,
      OnDataFrameVector(
          true, WebSocketFrameHeader::kOpCodeBinary,
          std::vector<char>(kBinaryBlob, kBinaryBlob + kBinaryBlobSize)));

  CreateChannelAndConnectSuccessfully();
}

// Invalid UTF-8 is not permitted in Text frames.
TEST_F(WebSocketChannelSendUtf8Test, InvalidUtf8Rejected) {
  EXPECT_CALL(
      *event_interface_,
      OnFailChannel("Browser sent a text frame containing invalid UTF-8"));

  CreateChannelAndConnectSuccessfully();

  channel_->SendFrame(true, WebSocketFrameHeader::kOpCodeText,
                      AsIOBuffer("\xff"), 1U);
}

// A Text message cannot end with a partial UTF-8 character.
TEST_F(WebSocketChannelSendUtf8Test, IncompleteCharacterInFinalFrame) {
  EXPECT_CALL(
      *event_interface_,
      OnFailChannel("Browser sent a text frame containing invalid UTF-8"));

  CreateChannelAndConnectSuccessfully();

  channel_->SendFrame(true, WebSocketFrameHeader::kOpCodeText,
                      AsIOBuffer("\xc2"), 1U);
}

// A non-final Text frame may end with a partial UTF-8 character (compare to
// previous test).
TEST_F(WebSocketChannelSendUtf8Test, IncompleteCharacterInNonFinalFrame) {
  CreateChannelAndConnectSuccessfully();

  channel_->SendFrame(false, WebSocketFrameHeader::kOpCodeText,
                      AsIOBuffer("\xc2"), 1U);
}

// UTF-8 parsing context must be retained between frames.
TEST_F(WebSocketChannelSendUtf8Test, ValidCharacterSplitBetweenFrames) {
  CreateChannelAndConnectSuccessfully();

  channel_->SendFrame(false, WebSocketFrameHeader::kOpCodeText,
                      AsIOBuffer("\xf1"), 1U);
  channel_->SendFrame(true, WebSocketFrameHeader::kOpCodeContinuation,
                      AsIOBuffer("\x80\xa0\xbf"), 3U);
}

// Similarly, an invalid character should be detected even if split.
TEST_F(WebSocketChannelSendUtf8Test, InvalidCharacterSplit) {
  EXPECT_CALL(
      *event_interface_,
      OnFailChannel("Browser sent a text frame containing invalid UTF-8"));

  CreateChannelAndConnectSuccessfully();

  channel_->SendFrame(false, WebSocketFrameHeader::kOpCodeText,
                      AsIOBuffer("\xe1"), 1U);
  channel_->SendFrame(true, WebSocketFrameHeader::kOpCodeContinuation,
                      AsIOBuffer("\x80\xa0\xbf"), 3U);
}

// An invalid character must be detected in continuation frames.
TEST_F(WebSocketChannelSendUtf8Test, InvalidByteInContinuation) {
  EXPECT_CALL(
      *event_interface_,
      OnFailChannel("Browser sent a text frame containing invalid UTF-8"));

  CreateChannelAndConnectSuccessfully();

  channel_->SendFrame(false, WebSocketFrameHeader::kOpCodeText,
                      AsIOBuffer("foo"), 3U);
  channel_->SendFrame(false, WebSocketFrameHeader::kOpCodeContinuation,
                      AsIOBuffer("bar"), 3U);
  channel_->SendFrame(true, WebSocketFrameHeader::kOpCodeContinuation,
                      AsIOBuffer("\xff"), 1U);
}

// However, continuation frames of a Binary frame will not be tested for UTF-8
// validity.
TEST_F(WebSocketChannelSendUtf8Test, BinaryContinuationNotChecked) {
  CreateChannelAndConnectSuccessfully();

  channel_->SendFrame(false, WebSocketFrameHeader::kOpCodeBinary,
                      AsIOBuffer("foo"), 3U);
  channel_->SendFrame(false, WebSocketFrameHeader::kOpCodeContinuation,
                      AsIOBuffer("bar"), 3U);
  channel_->SendFrame(true, WebSocketFrameHeader::kOpCodeContinuation,
                      AsIOBuffer("\xff"), 1U);
}

// Multiple text messages can be validated without the validation state getting
// confused.
TEST_F(WebSocketChannelSendUtf8Test, ValidateMultipleTextMessages) {
  CreateChannelAndConnectSuccessfully();

  channel_->SendFrame(true, WebSocketFrameHeader::kOpCodeText,
                      AsIOBuffer("foo"), 3U);
  channel_->SendFrame(true, WebSocketFrameHeader::kOpCodeText,
                      AsIOBuffer("bar"), 3U);
}

// UTF-8 validation is enforced on received Text frames.
TEST_F(WebSocketChannelEventInterfaceTest, ReceivedInvalidUtf8) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, "\xff"}};
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::SYNC, OK, frames);
  set_stream(std::move(stream));

  EXPECT_CALL(*event_interface_,
              OnAddChannelResponse(_, _, kDefaultInitialQuota));
  EXPECT_CALL(*event_interface_,
              OnFailChannel("Could not decode a text frame as UTF-8."));

  CreateChannelAndConnectSuccessfully();
  base::RunLoop().RunUntilIdle();
}

// Invalid UTF-8 is not sent over the network.
TEST_F(WebSocketChannelStreamTest, InvalidUtf8TextFrameNotSent) {
  static const InitFrame expected[] = {{FINAL_FRAME,
                                        WebSocketFrameHeader::kOpCodeClose,
                                        MASKED, CLOSE_DATA(GOING_AWAY, "")}};
  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillRepeatedly(Return(ERR_IO_PENDING));
  EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected), _))
      .WillOnce(Return(OK));
  EXPECT_CALL(*mock_stream_, Close()).Times(1);

  CreateChannelAndConnectSuccessfully();

  channel_->SendFrame(true, WebSocketFrameHeader::kOpCodeText,
                      AsIOBuffer("\xff"), 1U);
}

// The rest of the tests for receiving invalid UTF-8 test the communication with
// the server. Since there is only one code path, it would be redundant to
// perform the same tests on the EventInterface as well.

// If invalid UTF-8 is received in a Text frame, the connection is failed.
TEST_F(WebSocketChannelReceiveUtf8Test, InvalidTextFrameRejected) {
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, "\xff"}};
  static const InitFrame expected[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose, MASKED,
       CLOSE_DATA(PROTOCOL_ERROR, "Invalid UTF-8 in text frame")}};
  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
        .WillOnce(ReturnFrames(&frames, &result_frame_data_))
        .WillRepeatedly(Return(ERR_IO_PENDING));
    EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected), _))
        .WillOnce(Return(OK));
    EXPECT_CALL(*mock_stream_, Close()).Times(1);
  }

  CreateChannelAndConnectSuccessfully();
}

// A received Text message is not permitted to end with a partial UTF-8
// character.
TEST_F(WebSocketChannelReceiveUtf8Test, IncompleteCharacterReceived) {
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, "\xc2"}};
  static const InitFrame expected[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose, MASKED,
       CLOSE_DATA(PROTOCOL_ERROR, "Invalid UTF-8 in text frame")}};
  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillOnce(ReturnFrames(&frames, &result_frame_data_))
      .WillRepeatedly(Return(ERR_IO_PENDING));
  EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected), _))
      .WillOnce(Return(OK));
  EXPECT_CALL(*mock_stream_, Close()).Times(1);

  CreateChannelAndConnectSuccessfully();
}

// However, a non-final Text frame may end with a partial UTF-8 character.
TEST_F(WebSocketChannelReceiveUtf8Test, IncompleteCharacterIncompleteMessage) {
  static const InitFrame frames[] = {
      {NOT_FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, "\xc2"}};
  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillOnce(ReturnFrames(&frames, &result_frame_data_))
      .WillRepeatedly(Return(ERR_IO_PENDING));

  CreateChannelAndConnectSuccessfully();
}

// However, it will become an error if it is followed by an empty final frame.
TEST_F(WebSocketChannelReceiveUtf8Test, TricksyIncompleteCharacter) {
  static const InitFrame frames[] = {
      {NOT_FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, "\xc2"},
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeContinuation, NOT_MASKED, ""}};
  static const InitFrame expected[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose, MASKED,
       CLOSE_DATA(PROTOCOL_ERROR, "Invalid UTF-8 in text frame")}};
  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillOnce(ReturnFrames(&frames, &result_frame_data_))
      .WillRepeatedly(Return(ERR_IO_PENDING));
  EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected), _))
      .WillOnce(Return(OK));
  EXPECT_CALL(*mock_stream_, Close()).Times(1);

  CreateChannelAndConnectSuccessfully();
}

// UTF-8 parsing context must be retained between received frames of the same
// message.
TEST_F(WebSocketChannelReceiveUtf8Test, ReceivedParsingContextRetained) {
  static const InitFrame frames[] = {
      {NOT_FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, "\xf1"},
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeContinuation,
       NOT_MASKED,  "\x80\xa0\xbf"}};
  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillOnce(ReturnFrames(&frames, &result_frame_data_))
      .WillRepeatedly(Return(ERR_IO_PENDING));

  CreateChannelAndConnectSuccessfully();
}

// An invalid character must be detected even if split between frames.
TEST_F(WebSocketChannelReceiveUtf8Test, SplitInvalidCharacterReceived) {
  static const InitFrame frames[] = {
      {NOT_FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, "\xe1"},
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeContinuation,
       NOT_MASKED,  "\x80\xa0\xbf"}};
  static const InitFrame expected[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose, MASKED,
       CLOSE_DATA(PROTOCOL_ERROR, "Invalid UTF-8 in text frame")}};
  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillOnce(ReturnFrames(&frames, &result_frame_data_))
      .WillRepeatedly(Return(ERR_IO_PENDING));
  EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected), _))
      .WillOnce(Return(OK));
  EXPECT_CALL(*mock_stream_, Close()).Times(1);

  CreateChannelAndConnectSuccessfully();
}

// An invalid character received in a continuation frame must be detected.
TEST_F(WebSocketChannelReceiveUtf8Test, InvalidReceivedIncontinuation) {
  static const InitFrame frames[] = {
      {NOT_FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, "foo"},
      {NOT_FINAL_FRAME, WebSocketFrameHeader::kOpCodeContinuation,
       NOT_MASKED,      "bar"},
      {FINAL_FRAME,     WebSocketFrameHeader::kOpCodeContinuation,
       NOT_MASKED,      "\xff"}};
  static const InitFrame expected[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose, MASKED,
       CLOSE_DATA(PROTOCOL_ERROR, "Invalid UTF-8 in text frame")}};
  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillOnce(ReturnFrames(&frames, &result_frame_data_))
      .WillRepeatedly(Return(ERR_IO_PENDING));
  EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected), _))
      .WillOnce(Return(OK));
  EXPECT_CALL(*mock_stream_, Close()).Times(1);

  CreateChannelAndConnectSuccessfully();
}

// Continuations of binary frames must not be tested for UTF-8 validity.
TEST_F(WebSocketChannelReceiveUtf8Test, ReceivedBinaryNotUtf8Tested) {
  static const InitFrame frames[] = {
      {NOT_FINAL_FRAME, WebSocketFrameHeader::kOpCodeBinary, NOT_MASKED, "foo"},
      {NOT_FINAL_FRAME, WebSocketFrameHeader::kOpCodeContinuation,
       NOT_MASKED,      "bar"},
      {FINAL_FRAME,     WebSocketFrameHeader::kOpCodeContinuation,
       NOT_MASKED,      "\xff"}};
  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillOnce(ReturnFrames(&frames, &result_frame_data_))
      .WillRepeatedly(Return(ERR_IO_PENDING));

  CreateChannelAndConnectSuccessfully();
}

// Multiple Text messages can be validated.
TEST_F(WebSocketChannelReceiveUtf8Test, ValidateMultipleReceived) {
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, "foo"},
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, "bar"}};
  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillOnce(ReturnFrames(&frames, &result_frame_data_))
      .WillRepeatedly(Return(ERR_IO_PENDING));

  CreateChannelAndConnectSuccessfully();
}

// A new data message cannot start in the middle of another data message.
TEST_F(WebSocketChannelEventInterfaceTest, BogusContinuation) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  static const InitFrame frames[] = {
      {NOT_FINAL_FRAME, WebSocketFrameHeader::kOpCodeBinary,
       NOT_MASKED, "frame1"},
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeText,
       NOT_MASKED, "frame2"}};
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::SYNC, OK, frames);
  set_stream(std::move(stream));

  EXPECT_CALL(*event_interface_,
              OnAddChannelResponse(_, _, kDefaultInitialQuota));
  EXPECT_CALL(*event_interface_,
              OnDataFrameVector(false, WebSocketFrameHeader::kOpCodeBinary,
                                AsVector("frame1")));
  EXPECT_CALL(
      *event_interface_,
      OnFailChannel(
          "Received start of new message but previous message is unfinished."));

  CreateChannelAndConnectSuccessfully();
}

// A new message cannot start with a Continuation frame.
TEST_F(WebSocketChannelEventInterfaceTest, MessageStartingWithContinuation) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeContinuation,
       NOT_MASKED, "continuation"}};
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::SYNC, OK, frames);
  set_stream(std::move(stream));

  EXPECT_CALL(*event_interface_,
              OnAddChannelResponse(_, _, kDefaultInitialQuota));
  EXPECT_CALL(*event_interface_,
              OnFailChannel("Received unexpected continuation frame."));

  CreateChannelAndConnectSuccessfully();
}

// A frame passed to the renderer must be either non-empty or have the final bit
// set.
TEST_F(WebSocketChannelEventInterfaceTest, DataFramesNonEmptyOrFinal) {
  auto stream = std::make_unique<ReadableFakeWebSocketStream>();
  static const InitFrame frames[] = {
      {NOT_FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, ""},
      {NOT_FINAL_FRAME, WebSocketFrameHeader::kOpCodeContinuation,
       NOT_MASKED, ""},
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeContinuation, NOT_MASKED, ""}};
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::SYNC, OK, frames);
  set_stream(std::move(stream));

  EXPECT_CALL(*event_interface_,
              OnAddChannelResponse(_, _, kDefaultInitialQuota));
  EXPECT_CALL(
      *event_interface_,
      OnDataFrameVector(true, WebSocketFrameHeader::kOpCodeText, AsVector("")));

  CreateChannelAndConnectSuccessfully();
}

// Calls to OnSSLCertificateError() must be passed through to the event
// interface with the correct URL attached.
TEST_F(WebSocketChannelEventInterfaceTest, OnSSLCertificateErrorCalled) {
  const GURL wss_url("wss://example.com/sslerror");
  connect_data_.socket_url = wss_url;
  const SSLInfo ssl_info;
  const bool fatal = true;
  auto fake_callbacks = std::make_unique<FakeSSLErrorCallbacks>();

  EXPECT_CALL(*event_interface_,
              OnSSLCertificateErrorCalled(NotNull(), wss_url, _, fatal));

  CreateChannelAndConnect();
  connect_data_.argument_saver.connect_delegate->OnSSLCertificateError(
      std::move(fake_callbacks), net::ERR_CERT_DATE_INVALID, ssl_info, fatal);
}

// Calls to OnAuthRequired() must be passed through to the event interface.
TEST_F(WebSocketChannelEventInterfaceTest, OnAuthRequiredCalled) {
  const GURL wss_url("wss://example.com/on_auth_required");
  connect_data_.socket_url = wss_url;
  AuthChallengeInfo auth_info;
  base::Optional<AuthCredentials> credentials;
  scoped_refptr<HttpResponseHeaders> response_headers =
      base::MakeRefCounted<HttpResponseHeaders>("HTTP/1.1 200 OK");
  IPEndPoint remote_endpoint(net::IPAddress(127, 0, 0, 1), 80);

  EXPECT_CALL(*event_interface_,
              OnAuthRequiredCalled(_, response_headers, _, &credentials))
      .WillOnce(Return(OK));

  CreateChannelAndConnect();
  connect_data_.argument_saver.connect_delegate->OnAuthRequired(
      auth_info, response_headers, remote_endpoint, {}, &credentials);
}

// If we receive another frame after Close, it is not valid. It is not
// completely clear what behaviour is required from the standard in this case,
// but the current implementation fails the connection. Since a Close has
// already been sent, this just means closing the connection.
TEST_F(WebSocketChannelStreamTest, PingAfterCloseIsRejected) {
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose,
       NOT_MASKED,  CLOSE_DATA(NORMAL_CLOSURE, "OK")},
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodePing,
       NOT_MASKED,  "Ping body"}};
  static const InitFrame expected[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose,
       MASKED,      CLOSE_DATA(NORMAL_CLOSURE, "OK")}};
  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillOnce(ReturnFrames(&frames, &result_frame_data_))
      .WillRepeatedly(Return(ERR_IO_PENDING));
  {
    // We only need to verify the relative order of WriteFrames() and
    // Close(). The current implementation calls WriteFrames() for the Close
    // frame before calling ReadFrames() again, but that is an implementation
    // detail and better not to consider required behaviour.
    InSequence s;
    EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected), _))
        .WillOnce(Return(OK));
    EXPECT_CALL(*mock_stream_, Close()).Times(1);
  }

  CreateChannelAndConnectSuccessfully();
}

// A protocol error from the remote server should result in a close frame with
// status 1002, followed by the connection closing.
TEST_F(WebSocketChannelStreamTest, ProtocolError) {
  static const InitFrame expected[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose,
       MASKED,      CLOSE_DATA(PROTOCOL_ERROR, "WebSocket Protocol Error")}};
  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillOnce(Return(ERR_WS_PROTOCOL_ERROR));
  EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected), _))
      .WillOnce(Return(OK));
  EXPECT_CALL(*mock_stream_, Close());

  CreateChannelAndConnectSuccessfully();
}

// Set the closing handshake timeout to a very tiny value before connecting.
class WebSocketChannelStreamTimeoutTest : public WebSocketChannelStreamTest {
 protected:
  WebSocketChannelStreamTimeoutTest() = default;

  void CreateChannelAndConnectSuccessfully() override {
    set_stream(std::move(mock_stream_));
    CreateChannelAndConnect();
    channel_->SetClosingHandshakeTimeoutForTesting(
        TimeDelta::FromMilliseconds(kVeryTinyTimeoutMillis));
    channel_->SetUnderlyingConnectionCloseTimeoutForTesting(
        TimeDelta::FromMilliseconds(kVeryTinyTimeoutMillis));
    connect_data_.argument_saver.connect_delegate->OnSuccess(
        std::move(stream_));
    ignore_result(channel_->ReadFrames());
  }
};

// In this case the server initiates the closing handshake with a Close
// message. WebSocketChannel responds with a matching Close message, and waits
// for the server to close the TCP/IP connection. The server never closes the
// connection, so the closing handshake times out and WebSocketChannel closes
// the connection itself.
TEST_F(WebSocketChannelStreamTimeoutTest, ServerInitiatedCloseTimesOut) {
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose,
       NOT_MASKED,  CLOSE_DATA(NORMAL_CLOSURE, "OK")}};
  static const InitFrame expected[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose,
       MASKED,      CLOSE_DATA(NORMAL_CLOSURE, "OK")}};
  EXPECT_CALL(*mock_stream_, GetSubProtocol()).Times(AnyNumber());
  EXPECT_CALL(*mock_stream_, GetExtensions()).Times(AnyNumber());
  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillOnce(ReturnFrames(&frames, &result_frame_data_))
      .WillRepeatedly(Return(ERR_IO_PENDING));
  Checkpoint checkpoint;
  TestClosure completion;
  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected), _))
        .WillOnce(Return(OK));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*mock_stream_, Close())
        .WillOnce(InvokeClosure(completion.closure()));
  }

  CreateChannelAndConnectSuccessfully();
  checkpoint.Call(1);
  completion.WaitForResult();
}

// In this case the client initiates the closing handshake by sending a Close
// message. WebSocketChannel waits for a Close message in response from the
// server. The server never responds to the Close message, so the closing
// handshake times out and WebSocketChannel closes the connection.
TEST_F(WebSocketChannelStreamTimeoutTest, ClientInitiatedCloseTimesOut) {
  static const InitFrame expected[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose,
       MASKED,      CLOSE_DATA(NORMAL_CLOSURE, "OK")}};
  EXPECT_CALL(*mock_stream_, GetSubProtocol()).Times(AnyNumber());
  EXPECT_CALL(*mock_stream_, GetExtensions()).Times(AnyNumber());
  EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
      .WillRepeatedly(Return(ERR_IO_PENDING));
  TestClosure completion;
  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected), _))
        .WillOnce(Return(OK));
    EXPECT_CALL(*mock_stream_, Close())
        .WillOnce(InvokeClosure(completion.closure()));
  }

  CreateChannelAndConnectSuccessfully();
  ASSERT_EQ(CHANNEL_ALIVE,
            channel_->StartClosingHandshake(kWebSocketNormalClosure, "OK"));
  completion.WaitForResult();
}

// In this case the client initiates the closing handshake and the server
// responds with a matching Close message. WebSocketChannel waits for the server
// to close the TCP/IP connection, but it never does. The closing handshake
// times out and WebSocketChannel closes the connection.
TEST_F(WebSocketChannelStreamTimeoutTest, ConnectionCloseTimesOut) {
  static const InitFrame expected[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose,
       MASKED,      CLOSE_DATA(NORMAL_CLOSURE, "OK")}};
  static const InitFrame frames[] = {
      {FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose,
       NOT_MASKED,  CLOSE_DATA(NORMAL_CLOSURE, "OK")}};
  EXPECT_CALL(*mock_stream_, GetSubProtocol()).Times(AnyNumber());
  EXPECT_CALL(*mock_stream_, GetExtensions()).Times(AnyNumber());
  TestClosure completion;
  std::vector<std::unique_ptr<WebSocketFrame>>* read_frames = nullptr;
  CompletionOnceCallback read_callback;
  {
    InSequence s;
    // Copy the arguments to ReadFrames so that the test can call the callback
    // after it has send the close message.
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
        .WillOnce(DoAll(SaveArg<0>(&read_frames), SaveArg<1>(&read_callback),
                        Return(ERR_IO_PENDING)));
    // The first real event that happens is the client sending the Close
    // message.
    EXPECT_CALL(*mock_stream_, WriteFramesInternal(EqualsFrames(expected), _))
        .WillOnce(Return(OK));
    // The |read_frames| callback is called (from this test case) at this
    // point. ReadFrames is called again by WebSocketChannel, waiting for
    // ERR_CONNECTION_CLOSED.
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(_, _))
        .WillOnce(Return(ERR_IO_PENDING));
    // The timeout happens and so WebSocketChannel closes the stream.
    EXPECT_CALL(*mock_stream_, Close())
        .WillOnce(InvokeClosure(completion.closure()));
  }

  CreateChannelAndConnectSuccessfully();
  ASSERT_EQ(CHANNEL_ALIVE,
            channel_->StartClosingHandshake(kWebSocketNormalClosure, "OK"));
  ASSERT_TRUE(read_frames);
  // Provide the "Close" message from the server.
  *read_frames = CreateFrameVector(frames, &result_frame_data_);
  std::move(read_callback).Run(OK);
  completion.WaitForResult();
}

// Verify that current_send_quota() returns a non-zero value for a newly
// connected channel.
TEST_F(WebSocketChannelTest, CurrentSendQuotaNonZero) {
  CreateChannelAndConnectSuccessfully();
  EXPECT_GT(channel_->current_send_quota(), 0);
}

// Verify that current_send_quota() is updated when SendFrame() is called.
TEST_F(WebSocketChannelTest, CurrentSendQuotaUpdated) {
  const int kMessageSize = 5;
  set_stream(std::make_unique<WriteableFakeWebSocketStream>());
  CreateChannelAndConnectSuccessfully();

  int initial_send_quota = channel_->current_send_quota();
  EXPECT_GE(initial_send_quota, kMessageSize);

  channel_->SendFrame(
      true, WebSocketFrameHeader::kOpCodeText,
      AsIOBuffer(std::string(static_cast<size_t>(kMessageSize), 'a')),
      static_cast<size_t>(kMessageSize));
  int new_send_quota = channel_->current_send_quota();
  EXPECT_EQ(kMessageSize, initial_send_quota - new_send_quota);
}

}  // namespace
}  // namespace net
