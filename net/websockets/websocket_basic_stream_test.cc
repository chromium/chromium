// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

// Tests for WebSocketBasicStream. Note that we do not attempt to verify that
// frame parsing itself functions correctly, as that is covered by the
// WebSocketFrameParser tests.

#include "net/websockets/websocket_basic_stream.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>  // for memcpy() and memset().

#include <iterator>
#include <optional>
#include <utility>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"
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
  const char k##name[] = value;                                  \
  const size_t k##name##Size = std::size(k##name) - 1

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
        CompletionOnceCallback(), ClientSocketPool::ProxyAuthCallback(), &pool_,
        NetLogWithSource());
    return transport_socket;
  }

  void SetHttpReadBuffer(const char* data, size_t size) {
    http_read_buffer_ = base::MakeRefCounted<GrowableIOBuffer>();
    http_read_buffer_->SetCapacity(size);
    memcpy(http_read_buffer_->data(), data, size);
    http_read_buffer_->set_offset(size);
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
  std::vector<std::unique_ptr<WebSocketFrame>> frames_;
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

  // Prepares a read from |data| of |data_size|, split into |number_of_chunks|,
  // each of |chunk_size| (except that the last chunk may be larger or
  // smaller). All reads must be either SYNCHRONOUS or ASYNC (not a mixture),
  // and errors cannot be simulated. Once data is exhausted, further reads will
  // return 0 (ie. connection closed).
  void CreateChunkedRead(IoMode mode,
                         const char data[],
                         size_t data_size,
                         int chunk_size,
                         size_t number_of_chunks,
                         LastFrameBehaviour last_frame_behaviour) {
    reads_.clear();
    const char* start = data;
    for (size_t i = 0; i < number_of_chunks; ++i) {
      int len = chunk_size;
      const bool is_last_chunk = (i == number_of_chunks - 1);
      if ((last_frame_behaviour == LAST_FRAME_BIG && is_last_chunk) ||
          static_cast<int>(data + data_size - start) < len) {
        len = static_cast<int>(data + data_size - start);
      }
      reads_.emplace_back(mode, start, len);
      start += len;
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
        kWriteFrameSize - (WebSocketFrameHeader::kBaseHeaderSize +
                           WebSocketFrameHeader::kMaskingKeyLength);
    auto buffer = base::MakeRefCounted<IOBufferWithSize>(payload_size);
    frame_buffers_.push_back(buffer);
    memcpy(buffer->data(), kWriteFrame + kWriteFrameSize - payload_size,
           payload_size);
    frame->payload = buffer->data();
    WebSocketFrameHeader& header = frame->header;
    header.final = true;
    header.masked = true;
    header.payload_length = payload_size;
    frames_.push_back(std::move(frame));
  }

  // TODO(yoichio): Make this type std::vector<std::string>.
  std::vector<scoped_refptr<IOBuffer>> frame_buffers_;
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
  CreateRead(MockRead(SYNCHRONOUS, kSampleFrame, kSampleFrameSize));
  int result = stream_->ReadFrames(&frames_, cb_.callback());
  EXPECT_THAT(result, IsOk());
  ASSERT_EQ(1U, frames_.size());
  EXPECT_EQ(UINT64_C(6), frames_[0]->header.payload_length);
  EXPECT_TRUE(frames_[0]->header.final);
}

TEST_F(WebSocketBasicStreamSocketSingleReadTest, AsyncReadWorks) {
  CreateRead(MockRead(ASYNC, kSampleFrame, kSampleFrameSize));
  int result = stream_->ReadFrames(&frames_, cb_.callback());
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames_.size());
  EXPECT_EQ(UINT64_C(6), frames_[0]->header.payload_length);
  // Don't repeat all the tests from SyncReadWorks; just enough to be sure the
  // frame was really read.
}

// ReadFrames will not return a frame whose header has not been wholly received.
TEST_F(WebSocketBasicStreamSocketChunkedReadTest, HeaderFragmentedSync) {
  CreateChunkedRead(SYNCHRONOUS, kSampleFrame, kSampleFrameSize, 1, 2,
                    LAST_FRAME_BIG);
  int result = stream_->ReadFrames(&frames_, cb_.callback());
  EXPECT_THAT(result, IsOk());
  ASSERT_EQ(1U, frames_.size());
  EXPECT_EQ(UINT64_C(6), frames_[0]->header.payload_length);
}

// The same behaviour applies to asynchronous reads.
TEST_F(WebSocketBasicStreamSocketChunkedReadTest, HeaderFragmentedAsync) {
  CreateChunkedRead(ASYNC, kSampleFrame, kSampleFrameSize, 1, 2,
                    LAST_FRAME_BIG);
  int result = stream_->ReadFrames(&frames_, cb_.callback());
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames_.size());
  EXPECT_EQ(UINT64_C(6), frames_[0]->header.payload_length);
}

// If it receives an incomplete header in a synchronous call, then has to wait
// for the rest of the frame, ReadFrames will return ERR_IO_PENDING.
TEST_F(WebSocketBasicStreamSocketTest, HeaderFragmentedSyncAsync) {
  MockRead reads[] = {MockRead(SYNCHRONOUS, kSampleFrame, 1),
                      MockRead(ASYNC, kSampleFrame + 1, kSampleFrameSize - 1)};
  CreateStream(reads, base::span<MockWrite>());
  int result = stream_->ReadFrames(&frames_, cb_.callback());
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames_.size());
  EXPECT_EQ(UINT64_C(6), frames_[0]->header.payload_length);
}

// An extended header should also return ERR_IO_PENDING if it is not completely
// received.
TEST_F(WebSocketBasicStreamSocketTest, FragmentedLargeHeader) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kPartialLargeFrame, kLargeFrameHeaderSize - 1),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING)};
  CreateStream(reads, base::span<MockWrite>());
  EXPECT_THAT(stream_->ReadFrames(&frames_, cb_.callback()),
              IsError(ERR_IO_PENDING));
}

// A frame that does not arrive in a single read should be broken into separate
// frames.
TEST_F(WebSocketBasicStreamSocketSingleReadTest, LargeFrameFirstChunk) {
  CreateRead(MockRead(SYNCHRONOUS, kPartialLargeFrame, kPartialLargeFrameSize));
  EXPECT_THAT(stream_->ReadFrames(&frames_, cb_.callback()), IsOk());
  ASSERT_EQ(1U, frames_.size());
  EXPECT_FALSE(frames_[0]->header.final);
  EXPECT_EQ(kPartialLargeFrameSize - kLargeFrameHeaderSize,
            static_cast<size_t>(frames_[0]->header.payload_length));
}

// If only the header of a data frame arrives, we should receive a frame with a
// zero-size payload.
TEST_F(WebSocketBasicStreamSocketSingleReadTest, HeaderOnlyChunk) {
  CreateRead(MockRead(SYNCHRONOUS, kPartialLargeFrame, kLargeFrameHeaderSize));

  EXPECT_THAT(stream_->ReadFrames(&frames_, cb_.callback()), IsOk());
  ASSERT_EQ(1U, frames_.size());
  EXPECT_EQ(nullptr, frames_[0]->payload);
  EXPECT_EQ(0U, frames_[0]->header.payload_length);
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames_[0]->header.opcode);
}

// If the header and the body of a data frame arrive seperately, we should see
// them as separate frames.
TEST_F(WebSocketBasicStreamSocketTest, HeaderBodySeparated) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kPartialLargeFrame, kLargeFrameHeaderSize),
      MockRead(ASYNC, kPartialLargeFrame + kLargeFrameHeaderSize,
               kPartialLargeFrameSize - kLargeFrameHeaderSize)};
  CreateStream(reads, base::span<MockWrite>());
  EXPECT_THAT(stream_->ReadFrames(&frames_, cb_.callback()), IsOk());
  ASSERT_EQ(1U, frames_.size());
  EXPECT_EQ(nullptr, frames_[0]->payload);
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames_[0]->header.opcode);
  frames_.clear();
  EXPECT_THAT(stream_->ReadFrames(&frames_, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames_.size());
  EXPECT_EQ(kPartialLargeFrameSize - kLargeFrameHeaderSize,
            frames_[0]->header.payload_length);
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeContinuation,
            frames_[0]->header.opcode);
}

// Every frame has a header with a correct payload_length field.
TEST_F(WebSocketBasicStreamSocketChunkedReadTest, LargeFrameTwoChunks) {
  constexpr size_t kChunkSize = 16;
  CreateChunkedRead(ASYNC, kPartialLargeFrame, kPartialLargeFrameSize,
                    kChunkSize, 2, LAST_FRAME_NOT_BIG);
  TestCompletionCallback cb[2];

  ASSERT_THAT(stream_->ReadFrames(&frames_, cb[0].callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb[0].WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames_.size());
  EXPECT_EQ(kChunkSize - kLargeFrameHeaderSize,
            frames_[0]->header.payload_length);

  frames_.clear();
  ASSERT_THAT(stream_->ReadFrames(&frames_, cb[1].callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb[1].WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames_.size());
  EXPECT_EQ(kChunkSize, frames_[0]->header.payload_length);
}

// Only the final frame of a fragmented message has |final| bit set.
TEST_F(WebSocketBasicStreamSocketChunkedReadTest, OnlyFinalChunkIsFinal) {
  static constexpr size_t kFirstChunkSize = 4;
  CreateChunkedRead(ASYNC, kSampleFrame, kSampleFrameSize, kFirstChunkSize, 2,
                    LAST_FRAME_BIG);
  TestCompletionCallback cb[2];

  ASSERT_THAT(stream_->ReadFrames(&frames_, cb[0].callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb[0].WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames_.size());
  ASSERT_FALSE(frames_[0]->header.final);

  frames_.clear();
  ASSERT_THAT(stream_->ReadFrames(&frames_, cb[1].callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb[1].WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames_.size());
  ASSERT_TRUE(frames_[0]->header.final);
}

// All frames after the first have their opcode changed to Continuation.
TEST_F(WebSocketBasicStreamSocketChunkedReadTest, ContinuationOpCodeUsed) {
  constexpr size_t kFirstChunkSize = 3;
  constexpr int kChunkCount = 3;
  // The input data is one frame with opcode Text, which arrives in three
  // separate chunks.
  CreateChunkedRead(ASYNC, kSampleFrame, kSampleFrameSize, kFirstChunkSize,
                    kChunkCount, LAST_FRAME_BIG);
  TestCompletionCallback cb[kChunkCount];

  ASSERT_THAT(stream_->ReadFrames(&frames_, cb[0].callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb[0].WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames_.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames_[0]->header.opcode);

  // This test uses a loop to verify that the opcode for every frames generated
  // after the first is converted to Continuation.
  for (int i = 1; i < kChunkCount; ++i) {
    frames_.clear();
    ASSERT_THAT(stream_->ReadFrames(&frames_, cb[i].callback()),
                IsError(ERR_IO_PENDING));
    EXPECT_THAT(cb[i].WaitForResult(), IsOk());
    ASSERT_EQ(1U, frames_.size());
    EXPECT_EQ(WebSocketFrameHeader::kOpCodeContinuation,
              frames_[0]->header.opcode);
  }
}

// Multiple frames that arrive together should be parsed correctly.
TEST_F(WebSocketBasicStreamSocketSingleReadTest, ThreeFramesTogether) {
  CreateRead(MockRead(SYNCHRONOUS, kMultipleFrames, kMultipleFramesSize));

  EXPECT_THAT(stream_->ReadFrames(&frames_, cb_.callback()), IsOk());
  ASSERT_EQ(3U, frames_.size());
  EXPECT_TRUE(frames_[0]->header.final);
  EXPECT_TRUE(frames_[1]->header.final);
  EXPECT_TRUE(frames_[2]->header.final);
}

// ERR_CONNECTION_CLOSED must be returned on close.
TEST_F(WebSocketBasicStreamSocketSingleReadTest, SyncClose) {
  CreateRead(MockRead(SYNCHRONOUS, "", 0));

  EXPECT_EQ(ERR_CONNECTION_CLOSED,
            stream_->ReadFrames(&frames_, cb_.callback()));
}

TEST_F(WebSocketBasicStreamSocketSingleReadTest, AsyncClose) {
  CreateRead(MockRead(ASYNC, "", 0));

  ASSERT_THAT(stream_->ReadFrames(&frames_, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsError(ERR_CONNECTION_CLOSED));
}

// The result should be the same if the socket returns
// ERR_CONNECTION_CLOSED. This is not expected to happen on an established
// connection; a Read of size 0 is the expected behaviour. The key point of this
// test is to confirm that ReadFrames() behaviour is identical in both cases.
TEST_F(WebSocketBasicStreamSocketSingleReadTest, SyncCloseWithErr) {
  CreateRead(MockRead(SYNCHRONOUS, ERR_CONNECTION_CLOSED));

  EXPECT_EQ(ERR_CONNECTION_CLOSED,
            stream_->ReadFrames(&frames_, cb_.callback()));
}

TEST_F(WebSocketBasicStreamSocketSingleReadTest, AsyncCloseWithErr) {
  CreateRead(MockRead(ASYNC, ERR_CONNECTION_CLOSED));

  ASSERT_THAT(stream_->ReadFrames(&frames_, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsError(ERR_CONNECTION_CLOSED));
}

TEST_F(WebSocketBasicStreamSocketSingleReadTest, SyncErrorsPassedThrough) {
  // ERR_INSUFFICIENT_RESOURCES here represents an arbitrary error that
  // WebSocketBasicStream gives no special handling to.
  CreateRead(MockRead(SYNCHRONOUS, ERR_INSUFFICIENT_RESOURCES));

  EXPECT_EQ(ERR_INSUFFICIENT_RESOURCES,
            stream_->ReadFrames(&frames_, cb_.callback()));
}

TEST_F(WebSocketBasicStreamSocketSingleReadTest, AsyncErrorsPassedThrough) {
  CreateRead(MockRead(ASYNC, ERR_INSUFFICIENT_RESOURCES));

  ASSERT_THAT(stream_->ReadFrames(&frames_, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsError(ERR_INSUFFICIENT_RESOURCES));
}

// If we get a frame followed by a close, we should receive them separately.
TEST_F(WebSocketBasicStreamSocketChunkedReadTest, CloseAfterFrame) {
  // The chunk size equals the data size, so the second chunk is 0 size, closing
  // the connection.
  CreateChunkedRead(SYNCHRONOUS, kSampleFrame, kSampleFrameSize,
                    kSampleFrameSize, 2, LAST_FRAME_NOT_BIG);

  EXPECT_THAT(stream_->ReadFrames(&frames_, cb_.callback()), IsOk());
  EXPECT_EQ(1U, frames_.size());
  frames_.clear();
  EXPECT_EQ(ERR_CONNECTION_CLOSED,
            stream_->ReadFrames(&frames_, cb_.callback()));
}

// Synchronous close after an async frame header is handled by a different code
// path.
TEST_F(WebSocketBasicStreamSocketTest, AsyncCloseAfterIncompleteHeader) {
  MockRead reads[] = {MockRead(ASYNC, kSampleFrame, 1U),
                      MockRead(SYNCHRONOUS, "", 0)};
  CreateStream(reads, base::span<MockWrite>());

  ASSERT_THAT(stream_->ReadFrames(&frames_, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsError(ERR_CONNECTION_CLOSED));
}

// When Stream::Read returns ERR_CONNECTION_CLOSED we get the same result via a
// slightly different code path.
TEST_F(WebSocketBasicStreamSocketTest, AsyncErrCloseAfterIncompleteHeader) {
  MockRead reads[] = {MockRead(ASYNC, kSampleFrame, 1U),
                      MockRead(SYNCHRONOUS, ERR_CONNECTION_CLOSED)};
  CreateStream(reads, base::span<MockWrite>());

  ASSERT_THAT(stream_->ReadFrames(&frames_, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsError(ERR_CONNECTION_CLOSED));
}

// An empty first frame is not ignored.
TEST_F(WebSocketBasicStreamSocketSingleReadTest, EmptyFirstFrame) {
  CreateRead(MockRead(SYNCHRONOUS, kEmptyFirstFrame, kEmptyFirstFrameSize));

  EXPECT_THAT(stream_->ReadFrames(&frames_, cb_.callback()), IsOk());
  ASSERT_EQ(1U, frames_.size());
  EXPECT_EQ(nullptr, frames_[0]->payload);
  EXPECT_EQ(0U, frames_[0]->header.payload_length);
}

// An empty frame in the middle of a message is ignored.
TEST_F(WebSocketBasicStreamSocketTest, EmptyMiddleFrame) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kEmptyFirstFrame, kEmptyFirstFrameSize),
      MockRead(SYNCHRONOUS, kEmptyMiddleFrame, kEmptyMiddleFrameSize),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING)};
  CreateStream(reads, base::span<MockWrite>());

  EXPECT_THAT(stream_->ReadFrames(&frames_, cb_.callback()), IsOk());
  EXPECT_EQ(1U, frames_.size());
  frames_.clear();
  EXPECT_THAT(stream_->ReadFrames(&frames_, cb_.callback()),
              IsError(ERR_IO_PENDING));
}

// An empty frame in the middle of a message that arrives separately is still
// ignored.
TEST_F(WebSocketBasicStreamSocketTest, EmptyMiddleFrameAsync) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kEmptyFirstFrame, kEmptyFirstFrameSize),
      MockRead(ASYNC, kEmptyMiddleFrame, kEmptyMiddleFrameSize),
      // We include a pong message to verify the middle frame was actually
      // processed.
      MockRead(ASYNC, kValidPong, kValidPongSize)};
  CreateStream(reads, base::span<MockWrite>());

  EXPECT_THAT(stream_->ReadFrames(&frames_, cb_.callback()), IsOk());
  EXPECT_EQ(1U, frames_.size());
  frames_.clear();
  ASSERT_THAT(stream_->ReadFrames(&frames_, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames_.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodePong, frames_[0]->header.opcode);
}

// An empty final frame is not ignored.
TEST_F(WebSocketBasicStreamSocketSingleReadTest, EmptyFinalFrame) {
  CreateRead(
      MockRead(SYNCHRONOUS, kEmptyFinalTextFrame, kEmptyFinalTextFrameSize));

  EXPECT_THAT(stream_->ReadFrames(&frames_, cb_.callback()), IsOk());
  ASSERT_EQ(1U, frames_.size());
  EXPECT_EQ(nullptr, frames_[0]->payload);
  EXPECT_EQ(0U, frames_[0]->header.payload_length);
}

// An empty middle frame is ignored with a final frame present.
TEST_F(WebSocketBasicStreamSocketTest, ThreeFrameEmptyMessage) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kEmptyFirstFrame, kEmptyFirstFrameSize),
      MockRead(SYNCHRONOUS, kEmptyMiddleFrame, kEmptyMiddleFrameSize),
      MockRead(SYNCHRONOUS, kEmptyFinalContinuationFrame,
               kEmptyFinalContinuationFrameSize)};
  CreateStream(reads, base::span<MockWrite>());

  EXPECT_THAT(stream_->ReadFrames(&frames_, cb_.callback()), IsOk());
  ASSERT_EQ(1U, frames_.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames_[0]->header.opcode);
  frames_.clear();
  EXPECT_THAT(stream_->ReadFrames(&frames_, cb_.callback()), IsOk());
  ASSERT_EQ(1U, frames_.size());
  EXPECT_TRUE(frames_[0]->header.final);
}

// If there was a frame read at the same time as the response headers (and the
// handshake succeeded), then we should parse it.
TEST_F(WebSocketBasicStreamSocketTest, HttpReadBufferIsUsed) {
  SetHttpReadBuffer(kSampleFrame, kSampleFrameSize);
  CreateStream(base::span<MockRead>(), base::span<MockWrite>());

  EXPECT_THAT(stream_->ReadFrames(&frames_, cb_.callback()), IsOk());
  ASSERT_EQ(1U, frames_.size());
  ASSERT_TRUE(frames_[0]->payload);
  EXPECT_EQ(UINT64_C(6), frames_[0]->header.payload_length);
}

// Check that a frame whose header partially arrived at the end of the response
// headers works correctly.
TEST_F(WebSocketBasicStreamSocketSingleReadTest,
       PartialFrameHeaderInHttpResponse) {
  SetHttpReadBuffer(kSampleFrame, 1);
  CreateRead(MockRead(ASYNC, kSampleFrame + 1, kSampleFrameSize - 1));

  ASSERT_THAT(stream_->ReadFrames(&frames_, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames_.size());
  ASSERT_TRUE(frames_[0]->payload);
  EXPECT_EQ(UINT64_C(6), frames_[0]->header.payload_length);
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames_[0]->header.opcode);
}

// Check that a control frame which partially arrives at the end of the response
// headers works correctly.
TEST_F(WebSocketBasicStreamSocketSingleReadTest,
       PartialControlFrameInHttpResponse) {
  constexpr size_t kPartialFrameBytes = 3;
  SetHttpReadBuffer(kCloseFrame, kPartialFrameBytes);
  CreateRead(MockRead(ASYNC, kCloseFrame + kPartialFrameBytes,
                      kCloseFrameSize - kPartialFrameBytes));

  ASSERT_THAT(stream_->ReadFrames(&frames_, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames_.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeClose, frames_[0]->header.opcode);
  EXPECT_EQ(kCloseFrameSize - 2, frames_[0]->header.payload_length);
  EXPECT_EQ(std::string(frames_[0]->payload, kCloseFrameSize - 2),
            std::string(kCloseFrame + 2, kCloseFrameSize - 2));
}

// Check that a control frame which partially arrives at the end of the response
// headers works correctly. Synchronous version (unlikely in practice).
TEST_F(WebSocketBasicStreamSocketSingleReadTest,
       PartialControlFrameInHttpResponseSync) {
  constexpr size_t kPartialFrameBytes = 3;
  SetHttpReadBuffer(kCloseFrame, kPartialFrameBytes);
  CreateRead(MockRead(SYNCHRONOUS, kCloseFrame + kPartialFrameBytes,
                      kCloseFrameSize - kPartialFrameBytes));

  EXPECT_THAT(stream_->ReadFrames(&frames_, cb_.callback()), IsOk());
  ASSERT_EQ(1U, frames_.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeClose, frames_[0]->header.opcode);
}

// Check that an invalid frame results in an error.
TEST_F(WebSocketBasicStreamSocketSingleReadTest, SyncInvalidFrame) {
  CreateRead(MockRead(SYNCHRONOUS, kInvalidFrame, kInvalidFrameSize));

  EXPECT_EQ(ERR_WS_PROTOCOL_ERROR,
            stream_->ReadFrames(&frames_, cb_.callback()));
}

TEST_F(WebSocketBasicStreamSocketSingleReadTest, AsyncInvalidFrame) {
  CreateRead(MockRead(ASYNC, kInvalidFrame, kInvalidFrameSize));

  ASSERT_THAT(stream_->ReadFrames(&frames_, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsError(ERR_WS_PROTOCOL_ERROR));
}

// A control frame without a FIN flag is invalid and should not be passed
// through to higher layers. RFC6455 5.5 "All control frames ... MUST NOT be
// fragmented."
TEST_F(WebSocketBasicStreamSocketSingleReadTest, ControlFrameWithoutFin) {
  CreateRead(
      MockRead(SYNCHRONOUS, kPingFrameWithoutFin, kPingFrameWithoutFinSize));

  EXPECT_EQ(ERR_WS_PROTOCOL_ERROR,
            stream_->ReadFrames(&frames_, cb_.callback()));
  EXPECT_TRUE(frames_.empty());
}

// A control frame over 125 characters is invalid. RFC6455 5.5 "All control
// frames MUST have a payload length of 125 bytes or less". Since we use a
// 125-byte buffer to assemble fragmented control frames, we need to detect this
// error before attempting to assemble the fragments.
TEST_F(WebSocketBasicStreamSocketSingleReadTest, OverlongControlFrame) {
  CreateRead(MockRead(SYNCHRONOUS, k126BytePong, k126BytePongSize));

  EXPECT_EQ(ERR_WS_PROTOCOL_ERROR,
            stream_->ReadFrames(&frames_, cb_.callback()));
  EXPECT_TRUE(frames_.empty());
}

// A control frame over 125 characters should still be rejected if it is split
// into multiple chunks.
TEST_F(WebSocketBasicStreamSocketChunkedReadTest, SplitOverlongControlFrame) {
  constexpr size_t kFirstChunkSize = 16;
  expect_all_io_to_complete_ = false;
  CreateChunkedRead(SYNCHRONOUS, k126BytePong, k126BytePongSize,
                    kFirstChunkSize, 2, LAST_FRAME_BIG);

  EXPECT_EQ(ERR_WS_PROTOCOL_ERROR,
            stream_->ReadFrames(&frames_, cb_.callback()));
  EXPECT_TRUE(frames_.empty());
}

TEST_F(WebSocketBasicStreamSocketChunkedReadTest,
       AsyncSplitOverlongControlFrame) {
  constexpr size_t kFirstChunkSize = 16;
  expect_all_io_to_complete_ = false;
  CreateChunkedRead(ASYNC, k126BytePong, k126BytePongSize, kFirstChunkSize, 2,
                    LAST_FRAME_BIG);

  ASSERT_THAT(stream_->ReadFrames(&frames_, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsError(ERR_WS_PROTOCOL_ERROR));
  // The caller should not call ReadFrames() again after receiving an error
  // other than ERR_IO_PENDING.
  EXPECT_TRUE(frames_.empty());
}

// In the synchronous case, ReadFrames assembles the whole control frame before
// returning.
TEST_F(WebSocketBasicStreamSocketChunkedReadTest, SyncControlFrameAssembly) {
  constexpr size_t kChunkSize = 3;
  CreateChunkedRead(SYNCHRONOUS, kCloseFrame, kCloseFrameSize, kChunkSize, 3,
                    LAST_FRAME_BIG);

  EXPECT_THAT(stream_->ReadFrames(&frames_, cb_.callback()), IsOk());
  ASSERT_EQ(1U, frames_.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeClose, frames_[0]->header.opcode);
}

// In the asynchronous case, the callback is not called until the control frame
// has been completely assembled.
TEST_F(WebSocketBasicStreamSocketChunkedReadTest, AsyncControlFrameAssembly) {
  constexpr size_t kChunkSize = 3;
  CreateChunkedRead(ASYNC, kCloseFrame, kCloseFrameSize, kChunkSize, 3,
                    LAST_FRAME_BIG);

  ASSERT_THAT(stream_->ReadFrames(&frames_, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames_.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeClose, frames_[0]->header.opcode);
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

  auto big_frame = base::HeapArray<uint8_t>::WithSize(kWireSize);
  auto [extended_header, payload] =
      big_frame.as_span().split_at(kLargeFrameHeaderSize);

  {
    auto [header, extended_payload_length] = extended_header.split_at<2u>();
    header.copy_from(base::as_byte_span({'\x81', '\x7F'}));
    extended_payload_length.copy_from(base::U64ToBigEndian(kPayloadSize));
  }

  std::ranges::fill(payload, 'A');

  CreateChunkedRead(ASYNC, reinterpret_cast<char*>(big_frame.data()),
                    big_frame.size(), kReadBufferSize, kExpectedFrameCount,
                    LAST_FRAME_BIG);

  for (size_t frame = 0; frame < kExpectedFrameCount; ++frame) {
    frames_.clear();
    ASSERT_THAT(stream_->ReadFrames(&frames_, cb_.callback()),
                IsError(ERR_IO_PENDING));
    EXPECT_THAT(cb_.WaitForResult(), IsOk());
    ASSERT_EQ(1U, frames_.size());
    size_t expected_payload_size = kReadBufferSize;
    if (frame == 0) {
      expected_payload_size = kReadBufferSize - kLargeFrameHeaderSize;
    } else if (frame == kExpectedFrameCount - 1) {
      expected_payload_size =
          kWireSize - kReadBufferSize * (kExpectedFrameCount - 1);
    }
    EXPECT_EQ(expected_payload_size, frames_[0]->header.payload_length);
  }
}

// A frame with reserved flag(s) set that arrives in chunks should only have the
// reserved flag(s) set on the first chunk when split.
TEST_F(WebSocketBasicStreamSocketChunkedReadTest, ReservedFlagCleared) {
  static constexpr char kReservedFlagFrame[] = "\x41\x05Hello";
  constexpr size_t kReservedFlagFrameSize = std::size(kReservedFlagFrame) - 1;
  constexpr size_t kChunkSize = 5;

  CreateChunkedRead(ASYNC, kReservedFlagFrame, kReservedFlagFrameSize,
                    kChunkSize, 2, LAST_FRAME_BIG);

  TestCompletionCallback cb[2];
  ASSERT_THAT(stream_->ReadFrames(&frames_, cb[0].callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb[0].WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames_.size());
  EXPECT_TRUE(frames_[0]->header.reserved1);

  frames_.clear();
  ASSERT_THAT(stream_->ReadFrames(&frames_, cb[1].callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb[1].WaitForResult(), IsOk());
  ASSERT_EQ(1U, frames_.size());
  EXPECT_FALSE(frames_[0]->header.reserved1);
}

// Check that writing a frame all at once works.
TEST_F(WebSocketBasicStreamSocketWriteTest, WriteAtOnce) {
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, kWriteFrame, kWriteFrameSize)};
  CreateStream(base::span<MockRead>(), writes);

  EXPECT_THAT(stream_->WriteFrames(&frames_, cb_.callback()), IsOk());
}

// Check that completely async writing works.
TEST_F(WebSocketBasicStreamSocketWriteTest, AsyncWriteAtOnce) {
  MockWrite writes[] = {MockWrite(ASYNC, kWriteFrame, kWriteFrameSize)};
  CreateStream(base::span<MockRead>(), writes);

  ASSERT_THAT(stream_->WriteFrames(&frames_, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsOk());
}

// Check that writing a frame to an extremely full kernel buffer (so that it
// ends up being sent in bits) works. The WriteFrames() callback should not be
// called until all parts have been written.
TEST_F(WebSocketBasicStreamSocketWriteTest, WriteInBits) {
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, kWriteFrame, 4),
                        MockWrite(ASYNC, kWriteFrame + 4, 4),
                        MockWrite(ASYNC, kWriteFrame + 8, kWriteFrameSize - 8)};
  CreateStream(base::span<MockRead>(), writes);

  ASSERT_THAT(stream_->WriteFrames(&frames_, cb_.callback()),
              IsError(ERR_IO_PENDING));
  EXPECT_THAT(cb_.WaitForResult(), IsOk());
}

// Check that writing a Pong frame with a nullptr body works.
TEST_F(WebSocketBasicStreamSocketWriteTest, WriteNullptrPong) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, kMaskedEmptyPong, kMaskedEmptyPongSize)};
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
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, masked_frame.data(), masked_frame.size())};
  generator_ = &GenerateNonNulMaskingKey;
  CreateStream(base::span<MockRead>(), writes);

  auto frame =
      std::make_unique<WebSocketFrame>(WebSocketFrameHeader::kOpCodeText);
  const std::string unmasked_payload = "graphics";
  const size_t payload_size = unmasked_payload.size();
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(payload_size);
  memcpy(buffer->data(), unmasked_payload.data(), payload_size);
  frame->payload = buffer->data();
  WebSocketFrameHeader& header = frame->header;
  header.final = true;
  header.masked = true;
  header.payload_length = payload_size;
  frames_.push_back(std::move(frame));

  EXPECT_THAT(stream_->WriteFrames(&frames_, cb_.callback()), IsOk());
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
