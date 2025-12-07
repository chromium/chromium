// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for WebSocketBasicStream. Note that we do not attempt to verify that
// frame parsing itself functions correctly, as that is covered by the
// WebSocketFrameParser tests.

#include "net/websockets/websocket_basic_stream.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>  // for memcpy() and memset().

#include <array>
#include <iterator>
#include <optional>
#include <utility>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/string_view_util.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/connect_job.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {
namespace {

#define WEBSOCKET_BASIC_STREAM_TEST_DEFINE_CONSTANT(name, value) \
  constexpr auto k##name = base::span_from_cstring(value)

WEBSOCKET_BASIC_STREAM_TEST_DEFINE_CONSTANT(SampleFrame, "\x81\x06Sample");
WEBSOCKET_BASIC_STREAM_TEST_DEFINE_CONSTANT(
    PartialLargeFrame,
    "\x81\x7F\x00\x00\x00\x00\x7F\xFF\xFF\xFF"
    "chromiunum ad pasco per loca insanis pullum manducat frumenti");
constexpr size_t kLargeFrameHeaderSize = 10;
WEBSOCKET_BASIC_STREAM_TEST_DEFINE_CONSTANT(MultipleFrames,
                                            "\x81\x01X\x81\x01Y\x81\x01Z");
WEBSOCKET_BASIC_STREAM_TEST_DEFINE_CONSTANT(EmptyFirstFrame, "\x01\x00");
WEBSOCKET_BASIC_STREAM_TEST_DEFINE_CONSTANT(EmptyMiddleFrame, "\x00\x00");
WEBSOCKET_BASIC_STREAM_TEST_DEFINE_CONSTANT(EmptyFinalTextFrame, "\x81\x00");
WEBSOCKET_BASIC_STREAM_TEST_DEFINE_CONSTANT(EmptyFinalContinuationFrame,
                                            "\x80\x00");
WEBSOCKET_BASIC_STREAM_TEST_DEFINE_CONSTANT(ValidPong, "\x8A\x00");
// This frame encodes a payload length of 7 in two bytes, which is always
// invalid.
WEBSOCKET_BASIC_STREAM_TEST_DEFINE_CONSTANT(InvalidFrame,
                                            "\x81\x7E\x00\x07Invalid");
// Control frames must have the FIN bit set. This one does not.
WEBSOCKET_BASIC_STREAM_TEST_DEFINE_CONSTANT(PingFrameWithoutFin, "\x09\x00");
// Control frames must have a payload of 125 bytes or less. This one has
// a payload of 126 bytes.
WEBSOCKET_BASIC_STREAM_TEST_DEFINE_CONSTANT(
    126BytePong,
    "\x8a\x7e\x00\x7eZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ"
    "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");
WEBSOCKET_BASIC_STREAM_TEST_DEFINE_CONSTANT(CloseFrame,
                                            "\x88\x09\x03\xe8occludo");
WEBSOCKET_BASIC_STREAM_TEST_DEFINE_CONSTANT(WriteFrame,
                                            "\x81\x85\x00\x00\x00\x00Write");
WEBSOCKET_BASIC_STREAM_TEST_DEFINE_CONSTANT(MaskedEmptyPong,
                                            "\x8A\x80\x00\x00\x00\x00");
constexpr WebSocketMaskingKey kNulMaskingKey = {{'\0', '\0', '\0', '\0'}};
constexpr WebSocketMaskingKey kNonNulMaskingKey = {
    {'\x0d', '\x1b', '\x06', '\x17'}};

// A masking key generator function which generates the identity mask,
// ie. "\0\0\0\0".
WebSocketMaskingKey GenerateNulMaskingKey() {
  return kNulMaskingKey;
}

// A masking key generation function which generates a fixed masking key with no
// nul characters.
WebSocketMaskingKey GenerateNonNulMaskingKey() {
  return kNonNulMaskingKey;
}

// A subclass of StaticSocketDataProvider modified to require that all data
// expected to be read or written actually is.
class StrictStaticSocketDataProvider : public StaticSocketDataProvider {
 public:
  StrictStaticSocketDataProvider(base::span<const MockRead> reads,
                                 base::span<const MockWrite> writes,
                                 bool strict_mode)
      : StaticSocketDataProvider(reads, writes), strict_mode_(strict_mode) {}

  ~StrictStaticSocketDataProvider() override {
    if (strict_mode_) {
      EXPECT_EQ(read_count(), read_index());
      EXPECT_EQ(write_count(), write_index());
    }
  }

 private:
  const bool strict_mode_;
};

// A fixture for tests which only perform normal socket operations.
class WebSocketBasicStreamSocketTest : public TestWithTaskEnvironment {
 protected:
  WebSocketBasicStreamSocketTest()
      : common_connect_job_params_(
            &factory_,
            /*host_resolver=*/nullptr,
            /*http_auth_cache=*/nullptr,
            /*http_auth_handler_factory=*/nullptr,
            /*spdy_session_pool=*/nullptr,
            /*quic_supported_versions=*/nullptr,
            /*quic_session_pool=*/nullptr,
            /*proxy_delegate=*/nullptr,
            /*http_user_agent_settings=*/nullptr,
            /*ssl_client_context=*/nullptr,
            /*socket_performance_watcher_factory=*/nullptr,
            /*network_quality_estimator=*/nullptr,
            /*net_log=*/nullptr,
            /*websocket_endpoint_lock_manager=*/nullptr,
            /*http_server_properties*/ nullptr,
            /*alpn_protos=*/nullptr,
            /*application_settings=*/nullptr,
            /*ignore_certificate_errors=*/nullptr,
            /*early_data_enabled=*/nullptr),
        pool_(1, 1, &common_connect_job_params_),
        generator_(&GenerateNulMaskingKey) {}

  ~WebSocketBasicStreamSocketTest() override {
    // stream_ has a reference to socket_data_ (via MockTCPClientSocket) and so
    // should be destroyed first.
    stream_.reset();
  }

  std::unique_ptr<ClientSocketHandle> MakeTransportSocket(
      base::span<const MockRead> reads,
      base::span<const MockWrite> writes) {
    socket_data_ = std::make_unique<StrictStaticSocketDataProvider>(
        reads, writes, expect_all_io_to_complete_);
    socket_data_->set_connect_data(MockConnect(SYNCHRONOUS, OK));
    factory_.AddSocketDataProvider(socket_data_.get());

    auto transport_socket = std::make_unique<ClientSocketHandle>();
    scoped_refptr<ClientSocketPool::SocketParams> null_params;
    ClientSocketPool::GroupId group_id(
        url::SchemeHostPort(url::kHttpScheme, "a", 80),
        PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
        SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);
    transport_socket->Init(
        group_id, null_params, std::nullopt /* proxy_annotation_tag */, MEDIUM,
        SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
        CompletionOnceCallback(), ClientSocketPool::ProxyAuthCallback(),
        /*fail_if_alias_requires_proxy_override=*/false, &pool_,
        NetLogWithSource());
    return transport_socket;
  }

  void SetHttpReadBuffer(base::span<const char> data) {
    http_read_buffer_ = base::MakeRefCounted<GrowableIOBuffer>();
    http_read_buffer_->SetCapacity(data.size());
    http_read_buffer_->span().copy_from(base::as_byte_span(data));
    http_read_buffer_->set_offset(data.size());
  }

  void CreateStream(base::span<const MockRead> reads,
                    base::span<const MockWrite> writes) {
    stream_ = WebSocketBasicStream::CreateWebSocketBasicStreamForTesting(
        MakeTransportSocket(reads, writes), http_read_buffer_, sub_protocol_,
        extensions_, net_log_, generator_);
  }

  std::unique_ptr<SocketDataProvider> socket_data_;
  MockClientSocketFactory factory_;
  const CommonConnectJobParams common_connect_job_params_;
  MockTransportClientSocketPool pool_;
  TestCompletionCallback cb_;
  scoped_refptr<GrowableIOBuffer> http_read_buffer_;
  std::string sub_protocol_;
  std::string extensions_;
  NetLogWithSource net_log_;
  WebSocketBasicStream::WebSocketMaskingKeyGeneratorFunction generator_;
  bool expect_all_io_to_complete_ = true;
  std::unique_ptr<WebSocketBasicStream> stream_;
};

// A test fixture for the common case of tests that only perform a single read.
class WebSocketBasicStreamSocketSingleReadTest
    : public WebSocketBasicStreamSocketTest {
 protected:
  void CreateRead(const MockRead& read) {
    reads_[0] = read;
    CreateStream(reads_, base::span<MockWrite>());
  }

  MockRead reads_[1];
};

// A test fixture for tests that perform chunked reads.
class WebSocketBasicStreamSocketChunkedReadTest
    : public WebSocketBasicStreamSocketTest {
 protected:
  // Specify the behaviour if there aren't enough chunks to use all the data. If
  // LAST_FRAME_BIG is specified, then the rest of the data will be
  // put in the last chunk. If LAST_FRAME_NOT_BIG is specified, then the last
  // frame will be no bigger than the rest of the frames (but it can be smaller,
  // if not enough data remains).
  enum LastFrameBehaviour { LAST_FRAME_BIG, LAST_FRAME_NOT_BIG };

  // Prepares a read from |data|, split into |number_of_chunks|,
  // each of |chunk_size| (except that the last chunk may be larger or
  // smaller). All reads must be either SYNCHRONOUS or ASYNC (not a mixture),
  // and errors cannot be simulated. Once data is exhausted, further reads will
  // return 0 (ie. connection closed).
  void CreateChunkedRead(IoMode mode,
                         base::span<const char> data,
                         size_t chunk_size,
                         size_t number_of_chunks,
                         LastFrameBehaviour last_frame_behaviour) {
    reads_.clear();
    for (size_t i = 0; i < number_of_chunks; ++i) {
      size_t len = chunk_size;
      const bool is_last_chunk = (i == number_of_chunks - 1);
      if ((last_frame_behaviour == LAST_FRAME_BIG && is_last_chunk) ||
          len > data.size()) {
        len = data.size();
      }
      reads_.emplace_back(mode, data.take_first(len));
    }
    CreateStream(reads_, base::span<MockWrite>());
  }

  std::vector<MockRead> reads_;
};

// Test fixture for write tests.
class WebSocketBasicStreamSocketWriteTest
    : public WebSocketBasicStreamSocketTest {
 protected:
  // All write tests use the same frame, so it is easiest to create it during
  // test creation.
  void SetUp() override { PrepareWriteFrame(); }

  // Creates a WebSocketFrame with a wire format matching kWriteFrame and adds
  // it to |frames_|.
  void PrepareWriteFrame() {
    auto frame =
        std::make_unique<WebSocketFrame>(WebSocketFrameHeader::kOpCodeText);
    const size_t payload_size =
        kWriteFrame.size() - (WebSocketFrameHeader::kBaseHeaderSize +
                              WebSocketFrameHeader::kMaskingKeyLength);
    auto buffer = base::MakeRefCounted<IOBufferWithSize>(payload_size);
    frame_buffers_.push_back(buffer);
    buffer->span().copy_from(base::as_bytes(kWriteFrame).last(payload_size));
    frame->payload = buffer->span();
    WebSocketFrameHeader& header = frame->header;
    header.final = true;
    header.masked = true;
    header.payload_length = payload_size;
    frames_.push_back(std::move(frame));
  }

  // TODO(yoichio): Make this type std::vector<std::string>.
  std::vector<scoped_refptr<IOBuffer>> frame_buffers_;
  std::vector<std::unique_ptr<WebSocketFrame>> frames_;
};

// A test fixture for tests that perform read buffer size switching.
class WebSocketBasicStreamSwitchTest : public WebSocketBasicStreamSocketTest {
 protected:
  // This is used to specify the read start/end time.
  base::TimeTicks MicrosecondsFromStart(int microseconds) {
    static const base::TimeTicks kStartPoint =
        base::TimeTicks::UnixEpoch() + base::Seconds(60);
    return kStartPoint + base::Microseconds(microseconds);
  }

  WebSocketBasicStream::BufferSizeManager buffer_size_manager_;
};

TEST_F(WebSocketBasicStreamSocketTest, ConstructionWorks) {
  CreateStream(base::span<MockRead>(), base::span<MockWrite>());
}

TEST_F(WebSocketBasicStreamSocketSingleReadTest, SyncReadWorks) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  CreateRead(MockRead(SYNCHRONOUS, kSampleFrame));
  int result = stream_->ReadFrames(&frames, cb_.callback());
  EXPECT_THAT(result, IsOk());
  ASSERT_EQ(1U, frames.size());
  EXPECT_EQ(UINT64_C(6), frames[0]->header.payload_length);
  EXPECT_TRUE(frames[0]->header.final);
}

TEST_F(WebSocketBasicStreamSocketSingleReadTest, AsyncReadWorks) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  CreateRead(MockRead(ASYNC, kSampleFrame));
  int result = stream_->ReadFrames(&frames, cb_.callback());
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames.size());
  EXPECT_EQ(UINT64_C(6), frames[0]->header.payload_length);
  // Don't repeat all the tests from SyncReadWorks; just enough to be sure the
  // frame was really read.
}

// ReadFrames will not return a frame whose header has not been wholly received.
TEST_F(WebSocketBasicStreamSocketChunkedReadTest, HeaderFragmentedSync) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  CreateChunkedRead(SYNCHRONOUS, kSampleFrame, 1, 2, LAST_FRAME_BIG);
  int result = stream_->ReadFrames(&frames, cb_.callback());
  EXPECT_THAT(result, IsOk());
  ASSERT_EQ(1U, frames.size());
  EXPECT_EQ(UINT64_C(6), frames[0]->header.payload_length);
}

// The same behaviour applies to asynchronous reads.
TEST_F(WebSocketBasicStreamSocketChunkedReadTest, HeaderFragmentedAsync) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  CreateChunkedRead(ASYNC, kSampleFrame, 1, 2, LAST_FRAME_BIG);
  int result = stream_->ReadFrames(&frames, cb_.callback());
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames.size());
  EXPECT_EQ(UINT64_C(6), frames[0]->header.payload_length);
}

// If it receives an incomplete header in a synchronous call, then has to wait
// for the rest of the frame, ReadFrames will return ERR_IO_PENDING.
TEST_F(WebSocketBasicStreamSocketTest, HeaderFragmentedSyncAsync) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  MockRead reads[] = {MockRead(SYNCHRONOUS, kSampleFrame.first(1u)),
                      MockRead(ASYNC, kSampleFrame.subspan(1u))};
  CreateStream(reads, base::span<MockWrite>());
  int result = stream_->ReadFrames(&frames, cb_.callback());
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames.size());
  EXPECT_EQ(UINT64_C(6), frames[0]->header.payload_length);
}

// An extended header should also return ERR_IO_PENDING if it is not completely
// received.
TEST_F(WebSocketBasicStreamSocketTest, FragmentedLargeHeader) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  MockRead reads[] = {MockRead(SYNCHRONOUS, kPartialLargeFrame.first(
                                                kLargeFrameHeaderSize - 1)),
                      MockRead(SYNCHRONOUS, ERR_IO_PENDING)};
  CreateStream(reads, base::span<MockWrite>());
  EXPECT_THAT(stream_->ReadFrames(&frames, cb_.callback()),
              IsError(ERR_IO_PENDING));
}

// A frame that does not arrive in a single read should be broken into separate
// frames.
TEST_F(WebSocketBasicStreamSocketSingleReadTest, LargeFrameFirstChunk) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  CreateRead(MockRead(SYNCHRONOUS, kPartialLargeFrame));
  EXPECT_THAT(stream_->ReadFrames(&frames, cb_.callback()), IsOk());
  ASSERT_EQ(1U, frames.size());
  EXPECT_FALSE(frames[0]->header.final);
  EXPECT_EQ(kPartialLargeFrame.size() - kLargeFrameHeaderSize,
            static_cast<size_t>(frames[0]->header.payload_length));
}

// If only the header of a data frame arrives, we should receive a frame with a
// zero-size payload.
TEST_F(WebSocketBasicStreamSocketSingleReadTest, HeaderOnlyChunk) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  CreateRead(
      MockRead(SYNCHRONOUS, kPartialLargeFrame.first(kLargeFrameHeaderSize)));

  EXPECT_THAT(stream_->ReadFrames(&frames, cb_.callback()), IsOk());
  ASSERT_EQ(1U, frames.size());
  ASSERT_TRUE(frames[0]->payload.empty());
  EXPECT_EQ(0U, frames[0]->header.payload_length);
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
}

// If the header and the body of a data frame arrive seperately, we should see
// them as separate frames.
TEST_F(WebSocketBasicStreamSocketTest, HeaderBodySeparated) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kPartialLargeFrame.first(kLargeFrameHeaderSize)),
      MockRead(ASYNC, kPartialLargeFrame.subspan(kLargeFrameHeaderSize))};
  CreateStream(reads, base::span<MockWrite>());
  EXPECT_THAT(stream_->ReadFrames(&frames, cb_.callback()), IsOk());
  ASSERT_EQ(1U, frames.size());
  ASSERT_TRUE(frames[0]->payload.empty());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  frames.clear();
  EXPECT_THAT(stream_->ReadFrames(&frames, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames.size());
  EXPECT_EQ(kPartialLargeFrame.size() - kLargeFrameHeaderSize,
            frames[0]->header.payload_length);
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeContinuation,
            frames[0]->header.opcode);
}

// Every frame has a header with a correct payload_length field.
TEST_F(WebSocketBasicStreamSocketChunkedReadTest, LargeFrameTwoChunks) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  constexpr size_t kChunkSize = 16;
  CreateChunkedRead(ASYNC, kPartialLargeFrame, kChunkSize, 2,
                    LAST_FRAME_NOT_BIG);
  TestCompletionCallback cb[2];

  ASSERT_THAT(stream_->ReadFrames(&frames, cb[0].callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb[0].WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames.size());
  EXPECT_EQ(kChunkSize - kLargeFrameHeaderSize,
            frames[0]->header.payload_length);

  frames.clear();
  ASSERT_THAT(stream_->ReadFrames(&frames, cb[1].callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb[1].WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames.size());
  EXPECT_EQ(kChunkSize, frames[0]->header.payload_length);
}

// Only the final frame of a fragmented message has |final| bit set.
TEST_F(WebSocketBasicStreamSocketChunkedReadTest, OnlyFinalChunkIsFinal) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  static constexpr size_t kFirstChunkSize = 4;
  CreateChunkedRead(ASYNC, kSampleFrame, kFirstChunkSize, 2, LAST_FRAME_BIG);
  TestCompletionCallback cb[2];

  ASSERT_THAT(stream_->ReadFrames(&frames, cb[0].callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb[0].WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames.size());
  ASSERT_FALSE(frames[0]->header.final);

  frames.clear();
  ASSERT_THAT(stream_->ReadFrames(&frames, cb[1].callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb[1].WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames.size());
  ASSERT_TRUE(frames[0]->header.final);
}

// All frames after the first have their opcode changed to Continuation.
TEST_F(WebSocketBasicStreamSocketChunkedReadTest, ContinuationOpCodeUsed) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  constexpr size_t kFirstChunkSize = 3;
  constexpr int kChunkCount = 3;
  // The input data is one frame with opcode Text, which arrives in three
  // separate chunks.
  CreateChunkedRead(ASYNC, kSampleFrame, kFirstChunkSize, kChunkCount,
                    LAST_FRAME_BIG);
  std::array<TestCompletionCallback, kChunkCount> cb;

  ASSERT_THAT(stream_->ReadFrames(&frames, cb[0].callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb[0].WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);

  // This test uses a loop to verify that the opcode for every frames generated
  // after the first is converted to Continuation.
  for (int i = 1; i < kChunkCount; ++i) {
    frames.clear();
    ASSERT_THAT(stream_->ReadFrames(&frames, cb[i].callback()),
                IsError(ERR_IO_PENDING));
    EXPECT_THAT(cb[i].WaitForResult(), IsOk());
    ASSERT_EQ(1U, frames.size());
    EXPECT_EQ(WebSocketFrameHeader::kOpCodeContinuation,
              frames[0]->header.opcode);
  }
}

// Multiple frames that arrive together should be parsed correctly.
TEST_F(WebSocketBasicStreamSocketSingleReadTest, ThreeFramesTogether) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  CreateRead(MockRead(SYNCHRONOUS, kMultipleFrames));

  EXPECT_THAT(stream_->ReadFrames(&frames, cb_.callback()), IsOk());
  ASSERT_EQ(3U, frames.size());
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_TRUE(frames[1]->header.final);
  EXPECT_TRUE(frames[2]->header.final);
}

// ERR_CONNECTION_CLOSED must be returned on close.
TEST_F(WebSocketBasicStreamSocketSingleReadTest, SyncClose) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  CreateRead(MockRead(SYNCHRONOUS, std::string_view()));

  EXPECT_EQ(ERR_CONNECTION_CLOSED,
            stream_->ReadFrames(&frames, cb_.callback()));
}

TEST_F(WebSocketBasicStreamSocketSingleReadTest, AsyncClose) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  CreateRead(MockRead(ASYNC, std::string_view()));

  ASSERT_THAT(stream_->ReadFrames(&frames, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsError(ERR_CONNECTION_CLOSED));
}

// The result should be the same if the socket returns
// ERR_CONNECTION_CLOSED. This is not expected to happen on an established
// connection; a Read of size 0 is the expected behaviour. The key point of this
// test is to confirm that ReadFrames() behaviour is identical in both cases.
TEST_F(WebSocketBasicStreamSocketSingleReadTest, SyncCloseWithErr) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  CreateRead(MockRead(SYNCHRONOUS, ERR_CONNECTION_CLOSED));

  EXPECT_EQ(ERR_CONNECTION_CLOSED,
            stream_->ReadFrames(&frames, cb_.callback()));
}

TEST_F(WebSocketBasicStreamSocketSingleReadTest, AsyncCloseWithErr) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  CreateRead(MockRead(ASYNC, ERR_CONNECTION_CLOSED));

  ASSERT_THAT(stream_->ReadFrames(&frames, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsError(ERR_CONNECTION_CLOSED));
}

TEST_F(WebSocketBasicStreamSocketSingleReadTest, SyncErrorsPassedThrough) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  // ERR_INSUFFICIENT_RESOURCES here represents an arbitrary error that
  // WebSocketBasicStream gives no special handling to.
  CreateRead(MockRead(SYNCHRONOUS, ERR_INSUFFICIENT_RESOURCES));

  EXPECT_EQ(ERR_INSUFFICIENT_RESOURCES,
            stream_->ReadFrames(&frames, cb_.callback()));
}

TEST_F(WebSocketBasicStreamSocketSingleReadTest, AsyncErrorsPassedThrough) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  CreateRead(MockRead(ASYNC, ERR_INSUFFICIENT_RESOURCES));

  ASSERT_THAT(stream_->ReadFrames(&frames, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsError(ERR_INSUFFICIENT_RESOURCES));
}

// If we get a frame followed by a close, we should receive them separately.
TEST_F(WebSocketBasicStreamSocketChunkedReadTest, CloseAfterFrame) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  // The chunk size equals the data size, so the second chunk is 0 size, closing
  // the connection.
  CreateChunkedRead(SYNCHRONOUS, kSampleFrame, kSampleFrame.size(), 2,
                    LAST_FRAME_NOT_BIG);

  EXPECT_THAT(stream_->ReadFrames(&frames, cb_.callback()), IsOk());
  EXPECT_EQ(1U, frames.size());
  frames.clear();
  EXPECT_EQ(ERR_CONNECTION_CLOSED,
            stream_->ReadFrames(&frames, cb_.callback()));
}

// Synchronous close after an async frame header is handled by a different code
// path.
TEST_F(WebSocketBasicStreamSocketTest, AsyncCloseAfterIncompleteHeader) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  MockRead reads[] = {MockRead(ASYNC, kSampleFrame.first(1U)),
                      MockRead(SYNCHRONOUS, std::string_view())};
  CreateStream(reads, base::span<MockWrite>());

  ASSERT_THAT(stream_->ReadFrames(&frames, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsError(ERR_CONNECTION_CLOSED));
}

// When Stream::Read returns ERR_CONNECTION_CLOSED we get the same result via a
// slightly different code path.
TEST_F(WebSocketBasicStreamSocketTest, AsyncErrCloseAfterIncompleteHeader) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  MockRead reads[] = {MockRead(ASYNC, kSampleFrame.first(1U)),
                      MockRead(SYNCHRONOUS, ERR_CONNECTION_CLOSED)};
  CreateStream(reads, base::span<MockWrite>());

  ASSERT_THAT(stream_->ReadFrames(&frames, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsError(ERR_CONNECTION_CLOSED));
}

// An empty first frame is not ignored.
TEST_F(WebSocketBasicStreamSocketSingleReadTest, EmptyFirstFrame) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  CreateRead(MockRead(SYNCHRONOUS, kEmptyFirstFrame));

  EXPECT_THAT(stream_->ReadFrames(&frames, cb_.callback()), IsOk());
  ASSERT_EQ(1U, frames.size());
  ASSERT_TRUE(frames[0]->payload.empty());
  EXPECT_EQ(0U, frames[0]->header.payload_length);
}

// An empty frame in the middle of a message is processed as part of the
// message.
TEST_F(WebSocketBasicStreamSocketTest, EmptyMiddleFrame) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  MockRead reads[] = {MockRead(SYNCHRONOUS, kEmptyFirstFrame),
                      MockRead(SYNCHRONOUS, kEmptyMiddleFrame),
                      MockRead(SYNCHRONOUS, ERR_IO_PENDING)};
  CreateStream(reads, base::span<MockWrite>());

  EXPECT_THAT(stream_->ReadFrames(&frames, cb_.callback()), IsOk());
  EXPECT_EQ(1U, frames.size());
  frames.clear();
  EXPECT_THAT(stream_->ReadFrames(&frames, cb_.callback()), IsOk());
  EXPECT_EQ(1U, frames.size());
  frames.clear();
  EXPECT_THAT(stream_->ReadFrames(&frames, cb_.callback()),
              IsError(ERR_IO_PENDING));
}

// An empty frame in the middle of a message that arrives separately is
// processed.
TEST_F(WebSocketBasicStreamSocketTest, EmptyMiddleFrameAsync) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  MockRead reads[] = {MockRead(SYNCHRONOUS, kEmptyFirstFrame),
                      MockRead(ASYNC, kEmptyMiddleFrame),
                      // We include a pong message to verify the middle frame
                      // was actually processed.
                      MockRead(ASYNC, kValidPong)};
  CreateStream(reads, base::span<MockWrite>());

  EXPECT_THAT(stream_->ReadFrames(&frames, cb_.callback()), IsOk());
  EXPECT_EQ(1U, frames.size());
  frames.clear();
  ASSERT_THAT(stream_->ReadFrames(&frames, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeContinuation,
            frames[0]->header.opcode);
  frames.clear();
  ASSERT_THAT(stream_->ReadFrames(&frames, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodePong, frames[0]->header.opcode);
}

// An empty final frame is not ignored.
TEST_F(WebSocketBasicStreamSocketSingleReadTest, EmptyFinalFrame) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  CreateRead(MockRead(SYNCHRONOUS, kEmptyFinalTextFrame));

  EXPECT_THAT(stream_->ReadFrames(&frames, cb_.callback()), IsOk());
  ASSERT_EQ(1U, frames.size());
  ASSERT_TRUE(frames[0]->payload.empty());
  EXPECT_EQ(0U, frames[0]->header.payload_length);
}

// An empty middle frame is processed with a final frame present.
TEST_F(WebSocketBasicStreamSocketTest, ThreeFrameEmptyMessage) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  MockRead reads[] = {MockRead(SYNCHRONOUS, kEmptyFirstFrame),
                      MockRead(SYNCHRONOUS, kEmptyMiddleFrame),
                      MockRead(SYNCHRONOUS, kEmptyFinalContinuationFrame)};
  CreateStream(reads, base::span<MockWrite>());

  EXPECT_THAT(stream_->ReadFrames(&frames, cb_.callback()), IsOk());
  ASSERT_EQ(1U, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  frames.clear();
  EXPECT_THAT(stream_->ReadFrames(&frames, cb_.callback()), IsOk());
  ASSERT_EQ(1U, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeContinuation,
            frames[0]->header.opcode);
  frames.clear();
  EXPECT_THAT(stream_->ReadFrames(&frames, cb_.callback()), IsOk());
  ASSERT_EQ(1U, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeContinuation,
            frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
}

// If there was a frame read at the same time as the response headers (and the
// handshake succeeded), then we should parse it.
TEST_F(WebSocketBasicStreamSocketTest, HttpReadBufferIsUsed) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  SetHttpReadBuffer(kSampleFrame);
  CreateStream(base::span<MockRead>(), base::span<MockWrite>());

  EXPECT_THAT(stream_->ReadFrames(&frames, cb_.callback()), IsOk());
  ASSERT_EQ(1U, frames.size());
  ASSERT_FALSE(frames[0]->payload.empty());
  EXPECT_EQ(UINT64_C(6), frames[0]->header.payload_length);
}

// Check that a frame whose header partially arrived at the end of the response
// headers works correctly.
TEST_F(WebSocketBasicStreamSocketSingleReadTest,
       PartialFrameHeaderInHttpResponse) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  SetHttpReadBuffer(kSampleFrame.first(1u));
  CreateRead(MockRead(ASYNC, kSampleFrame.subspan(1u)));

  ASSERT_THAT(stream_->ReadFrames(&frames, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames.size());
  ASSERT_FALSE(frames[0]->payload.empty());
  EXPECT_EQ(UINT64_C(6), frames[0]->header.payload_length);
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
}

// Check that a control frame which partially arrives at the end of the response
// headers works correctly.
TEST_F(WebSocketBasicStreamSocketSingleReadTest,
       PartialControlFrameInHttpResponse) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  constexpr size_t kPartialFrameBytes = 3;
  SetHttpReadBuffer(kCloseFrame.first(kPartialFrameBytes));
  CreateRead(MockRead(ASYNC, kCloseFrame.subspan(kPartialFrameBytes)));

  ASSERT_THAT(stream_->ReadFrames(&frames, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeClose, frames[0]->header.opcode);
  EXPECT_EQ(kCloseFrame.size() - 2, frames[0]->header.payload_length);
  EXPECT_EQ(frames[0]->payload, base::as_bytes(kCloseFrame.subspan(2u)));
}

// Check that a control frame which partially arrives at the end of the response
// headers works correctly. Synchronous version (unlikely in practice).
TEST_F(WebSocketBasicStreamSocketSingleReadTest,
       PartialControlFrameInHttpResponseSync) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  constexpr size_t kPartialFrameBytes = 3;
  SetHttpReadBuffer(kCloseFrame.first(kPartialFrameBytes));
  CreateRead(MockRead(SYNCHRONOUS, kCloseFrame.subspan(kPartialFrameBytes)));

  EXPECT_THAT(stream_->ReadFrames(&frames, cb_.callback()), IsOk());
  ASSERT_EQ(1U, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeClose, frames[0]->header.opcode);
}

// Check that an invalid frame results in an error.
TEST_F(WebSocketBasicStreamSocketSingleReadTest, SyncInvalidFrame) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  CreateRead(MockRead(SYNCHRONOUS, kInvalidFrame));

  EXPECT_EQ(ERR_WS_PROTOCOL_ERROR,
            stream_->ReadFrames(&frames, cb_.callback()));
}

TEST_F(WebSocketBasicStreamSocketSingleReadTest, AsyncInvalidFrame) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  CreateRead(MockRead(ASYNC, kInvalidFrame));

  ASSERT_THAT(stream_->ReadFrames(&frames, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsError(ERR_WS_PROTOCOL_ERROR));
}

// A control frame without a FIN flag is invalid and should not be passed
// through to higher layers. RFC6455 5.5 "All control frames ... MUST NOT be
// fragmented."
TEST_F(WebSocketBasicStreamSocketSingleReadTest, ControlFrameWithoutFin) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  CreateRead(MockRead(SYNCHRONOUS, kPingFrameWithoutFin));

  EXPECT_EQ(ERR_WS_PROTOCOL_ERROR,
            stream_->ReadFrames(&frames, cb_.callback()));
  EXPECT_TRUE(frames.empty());
}

// A control frame over 125 characters is invalid. RFC6455 5.5 "All control
// frames MUST have a payload length of 125 bytes or less". Since we use a
// 125-byte buffer to assemble fragmented control frames, we need to detect this
// error before attempting to assemble the fragments.
TEST_F(WebSocketBasicStreamSocketSingleReadTest, OverlongControlFrame) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  CreateRead(MockRead(SYNCHRONOUS, k126BytePong));

  EXPECT_EQ(ERR_WS_PROTOCOL_ERROR,
            stream_->ReadFrames(&frames, cb_.callback()));
  EXPECT_TRUE(frames.empty());
}

// A control frame over 125 characters should still be rejected if it is split
// into multiple chunks.
TEST_F(WebSocketBasicStreamSocketChunkedReadTest, SplitOverlongControlFrame) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  constexpr size_t kFirstChunkSize = 16;
  expect_all_io_to_complete_ = false;
  CreateChunkedRead(SYNCHRONOUS, k126BytePong, kFirstChunkSize, 2,
                    LAST_FRAME_BIG);

  EXPECT_EQ(ERR_WS_PROTOCOL_ERROR,
            stream_->ReadFrames(&frames, cb_.callback()));
  EXPECT_TRUE(frames.empty());
}

TEST_F(WebSocketBasicStreamSocketChunkedReadTest,
       AsyncSplitOverlongControlFrame) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  constexpr size_t kFirstChunkSize = 16;
  expect_all_io_to_complete_ = false;
  CreateChunkedRead(ASYNC, k126BytePong, kFirstChunkSize, 2, LAST_FRAME_BIG);

  ASSERT_THAT(stream_->ReadFrames(&frames, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsError(ERR_WS_PROTOCOL_ERROR));
  // The caller should not call ReadFrames() again after receiving an error
  // other than ERR_IO_PENDING.
  EXPECT_TRUE(frames.empty());
}

constexpr auto kMultiplePongFramesData = std::to_array({
    '\x8A', '\x05', 'P', 'o', 'n', 'g', '1',  // "Pong1".
    '\x8A', '\x05', 'P', 'o', 'n', 'g', '2'   // "Pong2".
});

constexpr auto kMultiplePongFrames =
    std::string_view(kMultiplePongFramesData.begin(),
                     kMultiplePongFramesData.end());

// Test to ensure multiple control frames with different payloads are handled
// properly.
TEST_F(WebSocketBasicStreamSocketTest, MultipleControlFramesInOneRead) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;

  MockRead reads[] = {MockRead(SYNCHRONOUS, kMultiplePongFrames)};
  CreateStream(reads, base::span<MockWrite>());

  EXPECT_THAT(stream_->ReadFrames(&frames, cb_.callback()), IsOk());
  ASSERT_EQ(2U, frames.size());

  EXPECT_EQ(WebSocketFrameHeader::kOpCodePong, frames[0]->header.opcode);
  EXPECT_EQ(5U, frames[0]->header.payload_length);
  EXPECT_EQ(base::as_string_view(frames[0]->payload), "Pong1");

  EXPECT_EQ(WebSocketFrameHeader::kOpCodePong, frames[1]->header.opcode);
  EXPECT_EQ(5U, frames[1]->header.payload_length);
  EXPECT_EQ(base::as_string_view(frames[1]->payload), "Pong2");
}

// This is a repro for https://crbug.com/issues/377318323
TEST_F(WebSocketBasicStreamSocketTest, SplitControlFrameAfterAnotherFrame) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;

  MockRead reads[] = {
      MockRead(ASYNC,
               kMultiplePongFrames.substr(0, kMultiplePongFrames.size() - 2u)),
      MockRead(SYNCHRONOUS,
               kMultiplePongFrames.substr(kMultiplePongFrames.size() - 2u))};
  CreateStream(reads, base::span<MockWrite>());

  TestCompletionCallback cb1;
  EXPECT_THAT(stream_->ReadFrames(&frames, cb1.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb1.WaitForResult(), IsOk());
  // ReadFrames() returns after the first read that returns at least 1 complete
  // frame, so this call only returns the first pong.
  ASSERT_EQ(1U, frames.size());
  EXPECT_EQ(base::as_string_view(frames[0]->payload), "Pong1");

  frames.clear();

  TestCompletionCallback cb2;
  EXPECT_THAT(stream_->ReadFrames(&frames, cb2.callback()), IsOk());
  ASSERT_EQ(1U, frames.size());
  EXPECT_EQ(base::as_string_view(frames[0]->payload), "Pong2");
}

// This is a repro for https://crbug.com/issues/393000981
TEST_F(WebSocketBasicStreamSocketTest, SplitControlFrameBetweenTextFrames) {
  static constexpr auto kFirstReadBuffer =
      std::to_array<char>({// Text frame, size 5.
                           '\x81', '\x05', 't', 'e', 'x', 't', '1',
                           // Ping frame, size 4, truncated.
                           '\x89', '\x04', 'p', 'i'});
  static constexpr auto kSecondReadBuffer =
      std::to_array<char>({// Last two bytes of ping frame.
                           'n', 'g',
                           // Text frame, size 5.
                           '\x81', '\x05', 't', 'e', 'x', 't', '2'});

  std::vector<std::unique_ptr<WebSocketFrame>> frames;

  MockRead reads[] = {
      MockRead(SYNCHRONOUS, base::as_byte_span(kFirstReadBuffer)),
      MockRead(SYNCHRONOUS, base::as_byte_span(kSecondReadBuffer))};
  CreateStream(reads, base::span<MockWrite>());

  EXPECT_THAT(stream_->ReadFrames(&frames, CompletionOnceCallback()), IsOk());
  // ReadFrames() returns after the first read that returns at least 1 complete
  // frame, so this call only returns the first text frame.
  ASSERT_EQ(1U, frames.size());
  const auto& frame = *frames.front();
  EXPECT_EQ(frame.header.opcode, WebSocketFrameHeader::kOpCodeText);
  EXPECT_EQ(base::as_string_view(frame.payload), "text1");

  frames.clear();

  EXPECT_THAT(stream_->ReadFrames(&frames, CompletionOnceCallback()), IsOk());
  ASSERT_EQ(2U, frames.size());
  const auto& ping_frame = *frames[0];
  const auto& text_frame = *frames[1];
  EXPECT_EQ(ping_frame.header.opcode, WebSocketFrameHeader::kOpCodePing);
  EXPECT_EQ(base::as_string_view(ping_frame.payload), "ping");
  EXPECT_EQ(text_frame.header.opcode, WebSocketFrameHeader::kOpCodeText);
  EXPECT_EQ(base::as_string_view(text_frame.payload), "text2");
}

// In the synchronous case, ReadFrames assembles the whole control frame before
// returning.
TEST_F(WebSocketBasicStreamSocketChunkedReadTest, SyncControlFrameAssembly) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  constexpr size_t kChunkSize = 3;
  CreateChunkedRead(SYNCHRONOUS, kCloseFrame, kChunkSize, 3, LAST_FRAME_BIG);

  EXPECT_THAT(stream_->ReadFrames(&frames, cb_.callback()), IsOk());
  ASSERT_EQ(1U, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeClose, frames[0]->header.opcode);
}

// In the asynchronous case, the callback is not called until the control frame
// has been completely assembled.
TEST_F(WebSocketBasicStreamSocketChunkedReadTest, AsyncControlFrameAssembly) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  constexpr size_t kChunkSize = 3;
  CreateChunkedRead(ASYNC, kCloseFrame, kChunkSize, 3, LAST_FRAME_BIG);

  ASSERT_THAT(stream_->ReadFrames(&frames, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeClose, frames[0]->header.opcode);
}

// A frame with a 1MB payload that has to be read in chunks.
TEST_F(WebSocketBasicStreamSocketChunkedReadTest, OneMegFrame) {
  // This should be equal to the definition of kSmallReadBufferFrame in
  // websocket_basic_stream.cc.
  constexpr int kReadBufferSize = 1000;
  constexpr uint64_t kPayloadSize = 1 << 20;
  constexpr size_t kWireSize = kPayloadSize + kLargeFrameHeaderSize;
  constexpr size_t kExpectedFrameCount =
      (kWireSize + kReadBufferSize - 1) / kReadBufferSize;
  std::vector<std::unique_ptr<WebSocketFrame>> frames;

  auto big_frame = base::HeapArray<uint8_t>::WithSize(kWireSize);
  auto [extended_header, payload] =
      big_frame.as_span().split_at(kLargeFrameHeaderSize);

  {
    auto [header, extended_payload_length] = extended_header.split_at<2u>();
    header.copy_from(base::as_byte_span({'\x81', '\x7F'}));
    extended_payload_length.copy_from(base::U64ToBigEndian(kPayloadSize));
  }

  std::ranges::fill(payload, 'A');

  CreateChunkedRead(ASYNC, base::as_chars(big_frame.as_span()), kReadBufferSize,
                    kExpectedFrameCount, LAST_FRAME_BIG);

  for (size_t frame = 0; frame < kExpectedFrameCount; ++frame) {
    frames.clear();
    ASSERT_THAT(stream_->ReadFrames(&frames, cb_.callback()),
                IsError(ERR_IO_PENDING));
    EXPECT_THAT(cb_.WaitForResult(), IsOk());
    ASSERT_EQ(1U, frames.size());
    size_t expected_payload_size = kReadBufferSize;
    if (frame == 0) {
      expected_payload_size = kReadBufferSize - kLargeFrameHeaderSize;
    } else if (frame == kExpectedFrameCount - 1) {
      expected_payload_size =
          kWireSize - kReadBufferSize * (kExpectedFrameCount - 1);
    }
    EXPECT_EQ(expected_payload_size, frames[0]->header.payload_length);
  }
}

// A frame with reserved flag(s) set that arrives in chunks should only have the
// reserved flag(s) set on the first chunk when split.
TEST_F(WebSocketBasicStreamSocketChunkedReadTest, ReservedFlagCleared) {
  static constexpr char kReservedFlagFrameData[] = "\x41\x05Hello";
  auto const kReservedFlagFrame =
      base::span_from_cstring(kReservedFlagFrameData);
  constexpr size_t kChunkSize = 5;
  std::vector<std::unique_ptr<WebSocketFrame>> frames;

  CreateChunkedRead(ASYNC, kReservedFlagFrame, kChunkSize, 2, LAST_FRAME_BIG);

  TestCompletionCallback cb[2];
  ASSERT_THAT(stream_->ReadFrames(&frames, cb[0].callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb[0].WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames.size());
  EXPECT_TRUE(frames[0]->header.reserved1);

  frames.clear();
  ASSERT_THAT(stream_->ReadFrames(&frames, cb[1].callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb[1].WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames.size());
  EXPECT_FALSE(frames[0]->header.reserved1);
}

// Check that writing a frame all at once works.
TEST_F(WebSocketBasicStreamSocketWriteTest, WriteAtOnce) {
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, kWriteFrame)};
  CreateStream(base::span<MockRead>(), writes);

  EXPECT_THAT(stream_->WriteFrames(&frames_, cb_.callback()), IsOk());
}

// Check that completely async writing works.
TEST_F(WebSocketBasicStreamSocketWriteTest, AsyncWriteAtOnce) {
  MockWrite writes[] = {MockWrite(ASYNC, kWriteFrame)};
  CreateStream(base::span<MockRead>(), writes);

  ASSERT_THAT(stream_->WriteFrames(&frames_, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsOk());
}

// Check that writing a frame to an extremely full kernel buffer (so that it
// ends up being sent in bits) works. The WriteFrames() callback should not be
// called until all parts have been written.
TEST_F(WebSocketBasicStreamSocketWriteTest, WriteInBits) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, kWriteFrame.first(4u)),
      MockWrite(ASYNC, kWriteFrame.subspan(4u, 4u)),
      MockWrite(ASYNC, kWriteFrame.subspan(8u, kWriteFrame.size() - 8u))};
  CreateStream(base::span<MockRead>(), writes);

  ASSERT_THAT(stream_->WriteFrames(&frames_, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsOk());
}

// Check that writing a Pong frame with a nullptr body works.
TEST_F(WebSocketBasicStreamSocketWriteTest, WriteNullptrPong) {
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, kMaskedEmptyPong)};
  CreateStream(base::span<MockRead>(), writes);

  auto frame =
      std::make_unique<WebSocketFrame>(WebSocketFrameHeader::kOpCodePong);
  WebSocketFrameHeader& header = frame->header;
  header.final = true;
  header.masked = true;
  header.payload_length = 0;
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  frames.push_back(std::move(frame));
  EXPECT_THAT(stream_->WriteFrames(&frames, cb_.callback()), IsOk());
}

// Check that writing with a non-nullptr mask works correctly.
TEST_F(WebSocketBasicStreamSocketTest, WriteNonNulMask) {
  std::string masked_frame = std::string("\x81\x88");
  masked_frame += std::string(std::begin(kNonNulMaskingKey.key),
                              std::end(kNonNulMaskingKey.key));
  masked_frame += "jiggered";
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, std::string_view(masked_frame))};
  generator_ = &GenerateNonNulMaskingKey;
  CreateStream(base::span<MockRead>(), writes);

  auto frame =
      std::make_unique<WebSocketFrame>(WebSocketFrameHeader::kOpCodeText);
  const std::string unmasked_payload = "graphics";
  const size_t payload_size = unmasked_payload.size();
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(payload_size);
  buffer->span().copy_from(base::as_byte_span(unmasked_payload));
  frame->payload = buffer->span();
  WebSocketFrameHeader& header = frame->header;
  header.final = true;
  header.masked = true;
  header.payload_length = payload_size;

  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  frames.push_back(std::move(frame));

  EXPECT_THAT(stream_->WriteFrames(&frames, cb_.callback()), IsOk());
}

TEST_F(WebSocketBasicStreamSocketTest, GetExtensionsWorks) {
  extensions_ = "inflate-uuencode";
  CreateStream(base::span<MockRead>(), base::span<MockWrite>());

  EXPECT_EQ("inflate-uuencode", stream_->GetExtensions());
}

TEST_F(WebSocketBasicStreamSocketTest, GetSubProtocolWorks) {
  sub_protocol_ = "cyberchat";
  CreateStream(base::span<MockRead>(), base::span<MockWrite>());

  EXPECT_EQ("cyberchat", stream_->GetSubProtocol());
}

// Check that the read buffer size initialization works correctly.
TEST_F(WebSocketBasicStreamSwitchTest, GetInitialReadBufferSize) {
  EXPECT_EQ(buffer_size_manager_.buffer_size(),
            WebSocketBasicStream::BufferSize::kSmall);
  buffer_size_manager_.OnRead(MicrosecondsFromStart(0));
  EXPECT_EQ(buffer_size_manager_.buffer_size(),
            WebSocketBasicStream::BufferSize::kSmall);
}

// Check that the case where the start time and the end time are the same.
TEST_F(WebSocketBasicStreamSwitchTest, ZeroSecondRead) {
  buffer_size_manager_.set_window_for_test(1);
  buffer_size_manager_.OnRead(MicrosecondsFromStart(0));
  buffer_size_manager_.OnReadComplete(MicrosecondsFromStart(0), 1000);
  EXPECT_EQ(buffer_size_manager_.buffer_size(),
            WebSocketBasicStream::BufferSize::kLarge);
}

// Check that the read buffer size is switched for high throughput connection.
TEST_F(WebSocketBasicStreamSwitchTest, CheckSwitch) {
  buffer_size_manager_.set_window_for_test(4);
  // It tests the case where 4000 bytes data is read in 2000 ms. In this case,
  // the read buffer size should be switched to the large one.
  buffer_size_manager_.OnRead(MicrosecondsFromStart(0));
  buffer_size_manager_.OnReadComplete(MicrosecondsFromStart(200), 1000);
  buffer_size_manager_.OnRead(MicrosecondsFromStart(800));
  buffer_size_manager_.OnReadComplete(MicrosecondsFromStart(1000), 1000);
  buffer_size_manager_.OnRead(MicrosecondsFromStart(1300));
  buffer_size_manager_.OnReadComplete(MicrosecondsFromStart(1500), 1000);
  buffer_size_manager_.OnRead(MicrosecondsFromStart(1800));
  buffer_size_manager_.OnReadComplete(MicrosecondsFromStart(2000), 1000);
  EXPECT_EQ(buffer_size_manager_.buffer_size(),
            WebSocketBasicStream::BufferSize::kLarge);
}

}  // namespace
}  // namespace net
