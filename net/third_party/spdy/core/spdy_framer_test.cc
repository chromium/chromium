// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/spdy/core/spdy_framer.h"

#include <stdlib.h>

#include <algorithm>
#include <limits>
#include <tuple>
#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "net/third_party/spdy/core/array_output_buffer.h"
#include "net/third_party/spdy/core/hpack/hpack_constants.h"
#include "net/third_party/spdy/core/mock_spdy_framer_visitor.h"
#include "net/third_party/spdy/core/spdy_bitmasks.h"
#include "net/third_party/spdy/core/spdy_frame_builder.h"
#include "net/third_party/spdy/core/spdy_frame_reader.h"
#include "net/third_party/spdy/core/spdy_protocol.h"
#include "net/third_party/spdy/core/spdy_test_utils.h"
#include "net/third_party/spdy/platform/api/spdy_arraysize.h"
#include "net/third_party/spdy/platform/api/spdy_flags.h"
#include "net/third_party/spdy/platform/api/spdy_ptr_util.h"
#include "net/third_party/spdy/platform/api/spdy_string.h"
#include "net/third_party/spdy/platform/api/spdy_string_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using ::http2::Http2DecoderAdapter;
using ::testing::_;

namespace spdy {

namespace test {

namespace {

const int64_t kSize = 1024 * 1024;
char output_buffer[kSize] = "";

// frame_list_char is used to hold frames to be compared with output_buffer.
const int64_t buffer_size = 64 * 1024;
char frame_list_char[buffer_size] = "";
}  // namespace

class MockDebugVisitor : public SpdyFramerDebugVisitorInterface {
 public:
  MOCK_METHOD4(OnSendCompressedFrame,
               void(SpdyStreamId stream_id,
                    SpdyFrameType type,
                    size_t payload_len,
                    size_t frame_len));

  MOCK_METHOD3(OnReceiveCompressedFrame,
               void(SpdyStreamId stream_id,
                    SpdyFrameType type,
                    size_t frame_len));
};

MATCHER_P(IsFrameUnionOf, frame_list, "") {
  size_t size_verified = 0;
  for (const auto& frame : *frame_list) {
    if (arg.size() < size_verified + frame.size()) {
      LOG(FATAL) << "Incremental header serialization should not lead to a "
                 << "higher total frame length than non-incremental method.";
      return false;
    }
    if (memcmp(arg.data() + size_verified, frame.data(), frame.size())) {
      CompareCharArraysWithHexError(
          "Header serialization methods should be equivalent: ",
          reinterpret_cast<unsigned char*>(arg.data() + size_verified),
          frame.size(), reinterpret_cast<unsigned char*>(frame.data()),
          frame.size());
      return false;
    }
    size_verified += frame.size();
  }
  return size_verified == arg.size();
}

class SpdyFramerPeer {
 public:
  // TODO(dahollings): Remove these methods when deprecating non-incremental
  // header serialization path.
  static std::unique_ptr<SpdyHeadersIR> CloneSpdyHeadersIR(
      const SpdyHeadersIR& headers) {
    auto new_headers = SpdyMakeUnique<SpdyHeadersIR>(
        headers.stream_id(), headers.header_block().Clone());
    new_headers->set_fin(headers.fin());
    new_headers->set_has_priority(headers.has_priority());
    new_headers->set_weight(headers.weight());
    new_headers->set_parent_stream_id(headers.parent_stream_id());
    new_headers->set_exclusive(headers.exclusive());
    if (headers.padded()) {
      new_headers->set_padding_len(headers.padding_payload_len() + 1);
    }
    return new_headers;
  }

  static SpdySerializedFrame SerializeHeaders(SpdyFramer* framer,
                                              const SpdyHeadersIR& headers) {
    SpdySerializedFrame serialized_headers_old_version(
        framer->SerializeHeaders(headers));
    framer->hpack_encoder_.reset(nullptr);
    auto* saved_debug_visitor = framer->debug_visitor_;
    framer->debug_visitor_ = nullptr;

    std::vector<SpdySerializedFrame> frame_list;
    ArrayOutputBuffer frame_list_buffer(frame_list_char, buffer_size);
    SpdyFramer::SpdyHeaderFrameIterator it(framer, CloneSpdyHeadersIR(headers));
    while (it.HasNextFrame()) {
      size_t size_before = frame_list_buffer.Size();
      EXPECT_GT(it.NextFrame(&frame_list_buffer), 0u);
      frame_list.emplace_back(
          SpdySerializedFrame(frame_list_buffer.Begin() + size_before,
                              frame_list_buffer.Size() - size_before, false));
    }
    framer->debug_visitor_ = saved_debug_visitor;

    EXPECT_THAT(serialized_headers_old_version, IsFrameUnionOf(&frame_list));
    return serialized_headers_old_version;
  }

  static SpdySerializedFrame SerializeHeaders(SpdyFramer* framer,
                                              const SpdyHeadersIR& headers,
                                              ArrayOutputBuffer* output) {
    if (output == nullptr) {
      return SerializeHeaders(framer, headers);
    }
    output->Reset();
    EXPECT_TRUE(framer->SerializeHeaders(headers, output));
    SpdySerializedFrame serialized_headers_old_version(output->Begin(),
                                                       output->Size(), false);
    framer->hpack_encoder_.reset(nullptr);
    auto* saved_debug_visitor = framer->debug_visitor_;
    framer->debug_visitor_ = nullptr;

    std::vector<SpdySerializedFrame> frame_list;
    ArrayOutputBuffer frame_list_buffer(frame_list_char, buffer_size);
    SpdyFramer::SpdyHeaderFrameIterator it(framer, CloneSpdyHeadersIR(headers));
    while (it.HasNextFrame()) {
      size_t size_before = frame_list_buffer.Size();
      EXPECT_GT(it.NextFrame(&frame_list_buffer), 0u);
      frame_list.emplace_back(
          SpdySerializedFrame(frame_list_buffer.Begin() + size_before,
                              frame_list_buffer.Size() - size_before, false));
    }
    framer->debug_visitor_ = saved_debug_visitor;

    EXPECT_THAT(serialized_headers_old_version, IsFrameUnionOf(&frame_list));
    return serialized_headers_old_version;
  }

  static std::unique_ptr<SpdyPushPromiseIR> CloneSpdyPushPromiseIR(
      const SpdyPushPromiseIR& push_promise) {
    auto new_push_promise = SpdyMakeUnique<SpdyPushPromiseIR>(
        push_promise.stream_id(), push_promise.promised_stream_id(),
        push_promise.header_block().Clone());
    new_push_promise->set_fin(push_promise.fin());
    if (push_promise.padded()) {
      new_push_promise->set_padding_len(push_promise.padding_payload_len() + 1);
    }
    return new_push_promise;
  }

  static SpdySerializedFrame SerializePushPromise(
      SpdyFramer* framer,
      const SpdyPushPromiseIR& push_promise) {
    SpdySerializedFrame serialized_headers_old_version =
        framer->SerializePushPromise(push_promise);
    framer->hpack_encoder_.reset(nullptr);
    auto* saved_debug_visitor = framer->debug_visitor_;
    framer->debug_visitor_ = nullptr;

    std::vector<SpdySerializedFrame> frame_list;
    ArrayOutputBuffer frame_list_buffer(frame_list_char, buffer_size);
    frame_list_buffer.Reset();
    SpdyFramer::SpdyPushPromiseFrameIterator it(
        framer, CloneSpdyPushPromiseIR(push_promise));
    while (it.HasNextFrame()) {
      size_t size_before = frame_list_buffer.Size();
      EXPECT_GT(it.NextFrame(&frame_list_buffer), 0u);
      frame_list.emplace_back(
          SpdySerializedFrame(frame_list_buffer.Begin() + size_before,
                              frame_list_buffer.Size() - size_before, false));
    }
    framer->debug_visitor_ = saved_debug_visitor;

    EXPECT_THAT(serialized_headers_old_version, IsFrameUnionOf(&frame_list));
    return serialized_headers_old_version;
  }

  static SpdySerializedFrame SerializePushPromise(
      SpdyFramer* framer,
      const SpdyPushPromiseIR& push_promise,
      ArrayOutputBuffer* output) {
    if (output == nullptr) {
      return SerializePushPromise(framer, push_promise);
    }
    output->Reset();
    EXPECT_TRUE(framer->SerializePushPromise(push_promise, output));
    SpdySerializedFrame serialized_headers_old_version(output->Begin(),
                                                       output->Size(), false);
    framer->hpack_encoder_.reset(nullptr);
    auto* saved_debug_visitor = framer->debug_visitor_;
    framer->debug_visitor_ = nullptr;

    std::vector<SpdySerializedFrame> frame_list;
    ArrayOutputBuffer frame_list_buffer(frame_list_char, buffer_size);
    frame_list_buffer.Reset();
    SpdyFramer::SpdyPushPromiseFrameIterator it(
        framer, CloneSpdyPushPromiseIR(push_promise));
    while (it.HasNextFrame()) {
      size_t size_before = frame_list_buffer.Size();
      EXPECT_GT(it.NextFrame(&frame_list_buffer), 0u);
      frame_list.emplace_back(
          SpdySerializedFrame(frame_list_buffer.Begin() + size_before,
                              frame_list_buffer.Size() - size_before, false));
    }
    framer->debug_visitor_ = saved_debug_visitor;

    EXPECT_THAT(serialized_headers_old_version, IsFrameUnionOf(&frame_list));
    return serialized_headers_old_version;
  }
};

class TestSpdyVisitor : public SpdyFramerVisitorInterface,
                        public SpdyFramerDebugVisitorInterface {
 public:
  // This is larger than our max frame size because header blocks that
  // are too long can spill over into CONTINUATION frames.
  static const size_t kDefaultHeaderBufferSize = 16 * 1024 * 1024;

  explicit TestSpdyVisitor(SpdyFramer::CompressionOption option)
      : framer_(option),
        error_count_(0),
        headers_frame_count_(0),
        push_promise_frame_count_(0),
        goaway_count_(0),
        setting_count_(0),
        settings_ack_sent_(0),
        settings_ack_received_(0),
        continuation_count_(0),
        altsvc_count_(0),
        priority_count_(0),
        on_unknown_frame_result_(false),
        last_window_update_stream_(0),
        last_window_update_delta_(0),
        last_push_promise_stream_(0),
        last_push_promise_promised_stream_(0),
        data_bytes_(0),
        fin_frame_count_(0),
        fin_flag_count_(0),
        end_of_stream_count_(0),
        control_frame_header_data_count_(0),
        zero_length_control_frame_header_data_count_(0),
        data_frame_count_(0),
        last_payload_len_(0),
        last_frame_len_(0),
        header_buffer_(new char[kDefaultHeaderBufferSize]),
        header_buffer_length_(0),
        header_buffer_size_(kDefaultHeaderBufferSize),
        header_stream_id_(static_cast<SpdyStreamId>(-1)),
        header_control_type_(SpdyFrameType::DATA),
        header_buffer_valid_(false) {}

  void OnError(Http2DecoderAdapter::SpdyFramerError error) override {
    VLOG(1) << "SpdyFramer Error: "
            << Http2DecoderAdapter::SpdyFramerErrorToString(error);
    ++error_count_;
  }

  void OnDataFrameHeader(SpdyStreamId stream_id,
                         size_t length,
                         bool fin) override {
    VLOG(1) << "OnDataFrameHeader(" << stream_id << ", " << length << ", "
            << fin << ")";
    ++data_frame_count_;
    header_stream_id_ = stream_id;
  }

  void OnStreamFrameData(SpdyStreamId stream_id,
                         const char* data,
                         size_t len) override {
    VLOG(1) << "OnStreamFrameData(" << stream_id << ", data, " << len << ", "
            << ")   data:\n"
            << SpdyHexDump(SpdyStringPiece(data, len));
    EXPECT_EQ(header_stream_id_, stream_id);

    data_bytes_ += len;
  }

  void OnStreamEnd(SpdyStreamId stream_id) override {
    VLOG(1) << "OnStreamEnd(" << stream_id << ")";
    EXPECT_EQ(header_stream_id_, stream_id);
    ++end_of_stream_count_;
  }

  void OnStreamPadLength(SpdyStreamId stream_id, size_t value) override {
    VLOG(1) << "OnStreamPadding(" << stream_id << ", " << value << ")\n";
    EXPECT_EQ(header_stream_id_, stream_id);
    // Count the padding length field byte against total data bytes.
    data_bytes_ += 1;
  }

  void OnStreamPadding(SpdyStreamId stream_id, size_t len) override {
    VLOG(1) << "OnStreamPadding(" << stream_id << ", " << len << ")\n";
    EXPECT_EQ(header_stream_id_, stream_id);
    data_bytes_ += len;
  }

  SpdyHeadersHandlerInterface* OnHeaderFrameStart(
      SpdyStreamId stream_id) override {
    if (headers_handler_ == nullptr) {
      headers_handler_ = SpdyMakeUnique<TestHeadersHandler>();
    }
    return headers_handler_.get();
  }

  void OnHeaderFrameEnd(SpdyStreamId stream_id) override {
    CHECK(headers_handler_ != nullptr);
    headers_ = headers_handler_->decoded_block().Clone();
    header_bytes_received_ = headers_handler_->header_bytes_parsed();
    headers_handler_.reset();
  }

  void OnRstStream(SpdyStreamId stream_id, SpdyErrorCode error_code) override {
    VLOG(1) << "OnRstStream(" << stream_id << ", " << error_code << ")";
    ++fin_frame_count_;
  }

  void OnSetting(SpdySettingsId id, uint32_t value) override {
    VLOG(1) << "OnSetting(" << id << ", " << std::hex << value << ")";
    ++setting_count_;
  }

  void OnSettingsAck() override {
    VLOG(1) << "OnSettingsAck";
    ++settings_ack_received_;
  }

  void OnSettingsEnd() override {
    VLOG(1) << "OnSettingsEnd";
    ++settings_ack_sent_;
  }

  void OnPing(SpdyPingId unique_id, bool is_ack) override {
    LOG(DFATAL) << "OnPing(" << unique_id << ", " << (is_ack ? 1 : 0) << ")";
  }

  void OnGoAway(SpdyStreamId last_accepted_stream_id,
                SpdyErrorCode error_code) override {
    VLOG(1) << "OnGoAway(" << last_accepted_stream_id << ", " << error_code
            << ")";
    ++goaway_count_;
  }

  void OnHeaders(SpdyStreamId stream_id,
                 bool has_priority,
                 int weight,
                 SpdyStreamId parent_stream_id,
                 bool exclusive,
                 bool fin,
                 bool end) override {
    VLOG(1) << "OnHeaders(" << stream_id << ", " << has_priority << ", "
            << weight << ", " << parent_stream_id << ", " << exclusive << ", "
            << fin << ", " << end << ")";
    ++headers_frame_count_;
    InitHeaderStreaming(SpdyFrameType::HEADERS, stream_id);
    if (fin) {
      ++fin_flag_count_;
    }
    header_has_priority_ = has_priority;
    header_parent_stream_id_ = parent_stream_id;
    header_exclusive_ = exclusive;
  }

  void OnWindowUpdate(SpdyStreamId stream_id, int delta_window_size) override {
    VLOG(1) << "OnWindowUpdate(" << stream_id << ", " << delta_window_size
            << ")";
    last_window_update_stream_ = stream_id;
    last_window_update_delta_ = delta_window_size;
  }

  void OnPushPromise(SpdyStreamId stream_id,
                     SpdyStreamId promised_stream_id,
                     bool end) override {
    VLOG(1) << "OnPushPromise(" << stream_id << ", " << promised_stream_id
            << ", " << end << ")";
    ++push_promise_frame_count_;
    InitHeaderStreaming(SpdyFrameType::PUSH_PROMISE, stream_id);
    last_push_promise_stream_ = stream_id;
    last_push_promise_promised_stream_ = promised_stream_id;
  }

  void OnContinuation(SpdyStreamId stream_id, bool end) override {
    VLOG(1) << "OnContinuation(" << stream_id << ", " << end << ")";
    ++continuation_count_;
  }

  void OnAltSvc(SpdyStreamId stream_id,
                SpdyStringPiece origin,
                const SpdyAltSvcWireFormat::AlternativeServiceVector&
                    altsvc_vector) override {
    VLOG(1) << "OnAltSvc(" << stream_id << ", \"" << origin
            << "\", altsvc_vector)";
    test_altsvc_ir_ = SpdyMakeUnique<SpdyAltSvcIR>(stream_id);
    if (origin.length() > 0) {
      test_altsvc_ir_->set_origin(SpdyString(origin));
    }
    for (const auto& altsvc : altsvc_vector) {
      test_altsvc_ir_->add_altsvc(altsvc);
    }
    ++altsvc_count_;
  }

  void OnPriority(SpdyStreamId stream_id,
                  SpdyStreamId parent_stream_id,
                  int weight,
                  bool exclusive) override {
    VLOG(1) << "OnPriority(" << stream_id << ", " << parent_stream_id << ", "
            << weight << ", " << (exclusive ? 1 : 0) << ")";
    ++priority_count_;
  }

  bool OnUnknownFrame(SpdyStreamId stream_id, uint8_t frame_type) override {
    VLOG(1) << "OnUnknownFrame(" << stream_id << ", " << frame_type << ")";
    return on_unknown_frame_result_;
  }

  void OnSendCompressedFrame(SpdyStreamId stream_id,
                             SpdyFrameType type,
                             size_t payload_len,
                             size_t frame_len) override {
    VLOG(1) << "OnSendCompressedFrame(" << stream_id << ", " << type << ", "
            << payload_len << ", " << frame_len << ")";
    last_payload_len_ = payload_len;
    last_frame_len_ = frame_len;
  }

  void OnReceiveCompressedFrame(SpdyStreamId stream_id,
                                SpdyFrameType type,
                                size_t frame_len) override {
    VLOG(1) << "OnReceiveCompressedFrame(" << stream_id << ", " << type << ", "
            << frame_len << ")";
    last_frame_len_ = frame_len;
  }

  // Convenience function which runs a framer simulation with particular input.
  void SimulateInFramer(const unsigned char* input, size_t size) {
    deframer_.set_visitor(this);
    size_t input_remaining = size;
    const char* input_ptr = reinterpret_cast<const char*>(input);
    while (input_remaining > 0 && deframer_.spdy_framer_error() ==
                                      Http2DecoderAdapter::SPDY_NO_ERROR) {
      // To make the tests more interesting, we feed random (and small) chunks
      // into the framer.  This simulates getting strange-sized reads from
      // the socket.
      const size_t kMaxReadSize = 32;
      size_t bytes_read =
          (rand() % std::min(input_remaining, kMaxReadSize)) + 1;
      size_t bytes_processed = deframer_.ProcessInput(input_ptr, bytes_read);
      input_remaining -= bytes_processed;
      input_ptr += bytes_processed;
    }
  }

  void InitHeaderStreaming(SpdyFrameType header_control_type,
                           SpdyStreamId stream_id) {
    if (!IsDefinedFrameType(SerializeFrameType(header_control_type))) {
      DLOG(FATAL) << "Attempted to init header streaming with "
                  << "invalid control frame type: " << header_control_type;
    }
    memset(header_buffer_.get(), 0, header_buffer_size_);
    header_buffer_length_ = 0;
    header_stream_id_ = stream_id;
    header_control_type_ = header_control_type;
    header_buffer_valid_ = true;
  }

  void set_extension_visitor(ExtensionVisitorInterface* extension) {
    deframer_.set_extension_visitor(extension);
  }

  // Override the default buffer size (16K). Call before using the framer!
  void set_header_buffer_size(size_t header_buffer_size) {
    header_buffer_size_ = header_buffer_size;
    header_buffer_.reset(new char[header_buffer_size]);
  }

  SpdyFramer framer_;
  Http2DecoderAdapter deframer_;

  // Counters from the visitor callbacks.
  int error_count_;
  int headers_frame_count_;
  int push_promise_frame_count_;
  int goaway_count_;
  int setting_count_;
  int settings_ack_sent_;
  int settings_ack_received_;
  int continuation_count_;
  int altsvc_count_;
  int priority_count_;
  std::unique_ptr<SpdyAltSvcIR> test_altsvc_ir_;
  bool on_unknown_frame_result_;
  SpdyStreamId last_window_update_stream_;
  int last_window_update_delta_;
  SpdyStreamId last_push_promise_stream_;
  SpdyStreamId last_push_promise_promised_stream_;
  int data_bytes_;
  int fin_frame_count_;      // The count of RST_STREAM type frames received.
  int fin_flag_count_;       // The count of frames with the FIN flag set.
  int end_of_stream_count_;  // The count of zero-length data frames.
  int control_frame_header_data_count_;  // The count of chunks received.
  // The count of zero-length control frame header data chunks received.
  int zero_length_control_frame_header_data_count_;
  int data_frame_count_;
  size_t last_payload_len_;
  size_t last_frame_len_;

  // Header block streaming state:
  std::unique_ptr<char[]> header_buffer_;
  size_t header_buffer_length_;
  size_t header_buffer_size_;
  size_t header_bytes_received_;
  SpdyStreamId header_stream_id_;
  SpdyFrameType header_control_type_;
  bool header_buffer_valid_;
  std::unique_ptr<TestHeadersHandler> headers_handler_;
  SpdyHeaderBlock headers_;
  bool header_has_priority_;
  SpdyStreamId header_parent_stream_id_;
  bool header_exclusive_;
};

class TestExtension : public ExtensionVisitorInterface {
 public:
  void OnSetting(SpdySettingsId id, uint32_t value) override {
    settings_received_.push_back({id, value});
  }

  // Called when non-standard frames are received.
  bool OnFrameHeader(SpdyStreamId stream_id,
                     size_t length,
                     uint8_t type,
                     uint8_t flags) override {
    stream_id_ = stream_id;
    length_ = length;
    type_ = type;
    flags_ = flags;
    return true;
  }

  // The payload for a single frame may be delivered as multiple calls to
  // OnFramePayload.
  void OnFramePayload(const char* data, size_t len) override {
    payload_.append(data, len);
  }

  std::vector<std::pair<SpdySettingsId, uint32_t>> settings_received_;
  SpdyStreamId stream_id_ = 0;
  size_t length_ = 0;
  uint8_t type_ = 0;
  uint8_t flags_ = 0;
  SpdyString payload_;
};

// Exposes SpdyUnknownIR::set_length() for testing purposes.
class TestSpdyUnknownIR : public SpdyUnknownIR {
 public:
  using SpdyUnknownIR::SpdyUnknownIR;
  using SpdyUnknownIR::set_length;
};

enum Output { USE, NOT_USE };

class SpdyFramerTest : public ::testing::TestWithParam<Output> {
 public:
  SpdyFramerTest()
      : output_(output_buffer, kSize),
        framer_(SpdyFramer::ENABLE_COMPRESSION) {}

 protected:
  void SetUp() override {
    switch (GetParam()) {
      case USE:
        use_output_ = true;
        break;
      case NOT_USE:
        // TODO(yasong): remove this case after
        // FLAGS_chromium_http2_flag_remove_rewritelength deprecates.
        use_output_ = false;
        break;
    }
  }

  void CompareFrame(const SpdyString& description,
                    const SpdySerializedFrame& actual_frame,
                    const unsigned char* expected,
                    const int expected_len) {
    const unsigned char* actual =
        reinterpret_cast<const unsigned char*>(actual_frame.data());
    CompareCharArraysWithHexError(description, actual, actual_frame.size(),
                                  expected, expected_len);
  }

  bool use_output_ = false;
  ArrayOutputBuffer output_;
  SpdyFramer framer_;
  Http2DecoderAdapter deframer_;
};

INSTANTIATE_TEST_CASE_P(SpdyFramerTests,
                        SpdyFramerTest,
                        ::testing::Values(USE, NOT_USE));

// Test that we can encode and decode a SpdyHeaderBlock in serialized form.
TEST_P(SpdyFramerTest, HeaderBlockInBuffer) {
  SpdyFramer framer(SpdyFramer::DISABLE_COMPRESSION);

  // Encode the header block into a Headers frame.
  SpdyHeadersIR headers(/* stream_id = */ 1);
  headers.SetHeader("alpha", "beta");
  headers.SetHeader("gamma", "charlie");
  headers.SetHeader("cookie", "key1=value1; key2=value2");
  SpdySerializedFrame frame(
      SpdyFramerPeer::SerializeHeaders(&framer, headers, &output_));

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(reinterpret_cast<unsigned char*>(frame.data()),
                           frame.size());

  EXPECT_EQ(0, visitor.zero_length_control_frame_header_data_count_);
  EXPECT_EQ(headers.header_block(), visitor.headers_);
}

// Test that if there's not a full frame, we fail to parse it.
TEST_P(SpdyFramerTest, UndersizedHeaderBlockInBuffer) {
  SpdyFramer framer(SpdyFramer::DISABLE_COMPRESSION);

  // Encode the header block into a Headers frame.
  SpdyHeadersIR headers(/* stream_id = */ 1);
  headers.SetHeader("alpha", "beta");
  headers.SetHeader("gamma", "charlie");
  SpdySerializedFrame frame(
      SpdyFramerPeer::SerializeHeaders(&framer, headers, &output_));

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(reinterpret_cast<unsigned char*>(frame.data()),
                           frame.size() - 2);

  EXPECT_EQ(0, visitor.zero_length_control_frame_header_data_count_);
  EXPECT_THAT(visitor.headers_, testing::IsEmpty());
}

// Test that we can encode and decode stream dependency values in a header
// frame.
TEST_P(SpdyFramerTest, HeaderStreamDependencyValues) {
  SpdyFramer framer(SpdyFramer::DISABLE_COMPRESSION);

  const SpdyStreamId parent_stream_id_test_array[] = {0, 3};
  for (SpdyStreamId parent_stream_id : parent_stream_id_test_array) {
    const bool exclusive_test_array[] = {true, false};
    for (bool exclusive : exclusive_test_array) {
      SpdyHeadersIR headers(1);
      headers.set_has_priority(true);
      headers.set_parent_stream_id(parent_stream_id);
      headers.set_exclusive(exclusive);
      SpdySerializedFrame frame(
          SpdyFramerPeer::SerializeHeaders(&framer, headers, &output_));

      TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
      visitor.SimulateInFramer(reinterpret_cast<unsigned char*>(frame.data()),
                               frame.size());

      EXPECT_TRUE(visitor.header_has_priority_);
      EXPECT_EQ(parent_stream_id, visitor.header_parent_stream_id_);
      EXPECT_EQ(exclusive, visitor.header_exclusive_);
    }
  }
}

// Test that if we receive a frame with payload length field at the
// advertised max size, we do not set an error in ProcessInput.
TEST_P(SpdyFramerTest, AcceptMaxFrameSizeSetting) {
  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
  deframer_.set_visitor(&visitor);

  // DATA frame with maximum allowed payload length.
  unsigned char kH2FrameData[] = {
      0x00, 0x40, 0x00,        // Length: 2^14
      0x00,                    //   Type: HEADERS
      0x00,                    //  Flags: None
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0x00, 0x00, 0x00, 0x00,  // Junk payload
  };

  SpdySerializedFrame frame(reinterpret_cast<char*>(kH2FrameData),
                            sizeof(kH2FrameData), false);

  EXPECT_CALL(visitor, OnDataFrameHeader(1, 1 << 14, false));
  EXPECT_CALL(visitor, OnStreamFrameData(1, _, 4));
  deframer_.ProcessInput(frame.data(), frame.size());
  EXPECT_FALSE(deframer_.HasError());
}

// Test that if we receive a frame with payload length larger than the
// advertised max size, we set an error of SPDY_INVALID_CONTROL_FRAME_SIZE.
TEST_P(SpdyFramerTest, ExceedMaxFrameSizeSetting) {
  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
  deframer_.set_visitor(&visitor);

  // DATA frame with too large payload length.
  unsigned char kH2FrameData[] = {
      0x00, 0x40, 0x01,        // Length: 2^14 + 1
      0x00,                    //   Type: HEADERS
      0x00,                    //  Flags: None
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0x00, 0x00, 0x00, 0x00,  // Junk payload
  };

  SpdySerializedFrame frame(reinterpret_cast<char*>(kH2FrameData),
                            sizeof(kH2FrameData), false);

  EXPECT_CALL(visitor, OnError(Http2DecoderAdapter::SPDY_OVERSIZED_PAYLOAD));
  deframer_.ProcessInput(frame.data(), frame.size());
  EXPECT_TRUE(deframer_.HasError());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_OVERSIZED_PAYLOAD,
            deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             deframer_.spdy_framer_error());
}

// Test that if we receive a DATA frame with padding length larger than the
// payload length, we set an error of SPDY_INVALID_PADDING
TEST_P(SpdyFramerTest, OversizedDataPaddingError) {
  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
  deframer_.set_visitor(&visitor);

  // DATA frame with invalid padding length.
  // |kH2FrameData| has to be |unsigned char|, because Chromium on Windows uses
  // MSVC, where |char| is signed by default, which would not compile because of
  // the element exceeding 127.
  unsigned char kH2FrameData[] = {
      0x00, 0x00, 0x05,        // Length: 5
      0x00,                    //   Type: DATA
      0x09,                    //  Flags: END_STREAM|PADDED
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0xff,                    // PadLen: 255 trailing bytes (Too Long)
      0x00, 0x00, 0x00, 0x00,  // Padding
  };

  SpdySerializedFrame frame(reinterpret_cast<char*>(kH2FrameData),
                            sizeof(kH2FrameData), false);

  {
    testing::InSequence seq;
    EXPECT_CALL(visitor, OnDataFrameHeader(1, 5, 1));
    EXPECT_CALL(visitor, OnStreamPadding(1, 1));
    EXPECT_CALL(visitor, OnError(Http2DecoderAdapter::SPDY_INVALID_PADDING));
  }
  EXPECT_GT(frame.size(), deframer_.ProcessInput(frame.data(), frame.size()));
  EXPECT_TRUE(deframer_.HasError());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_INVALID_PADDING,
            deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             deframer_.spdy_framer_error());
}

// Test that if we receive a DATA frame with padding length not larger than the
// payload length, we do not set an error of SPDY_INVALID_PADDING
TEST_P(SpdyFramerTest, CorrectlySizedDataPaddingNoError) {
  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;

  deframer_.set_visitor(&visitor);

  // DATA frame with valid Padding length
  char kH2FrameData[] = {
      0x00, 0x00, 0x05,        // Length: 5
      0x00,                    //   Type: DATA
      0x08,                    //  Flags: PADDED
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0x04,                    // PadLen: 4 trailing bytes
      0x00, 0x00, 0x00, 0x00,  // Padding
  };

  SpdySerializedFrame frame(kH2FrameData, sizeof(kH2FrameData), false);

  {
    testing::InSequence seq;
    EXPECT_CALL(visitor, OnDataFrameHeader(1, 5, false));
    EXPECT_CALL(visitor, OnStreamPadLength(1, 4));
    EXPECT_CALL(visitor, OnError(_)).Times(0);
    // Note that OnStreamFrameData(1, _, 1)) is never called
    // since there is no data, only padding
    EXPECT_CALL(visitor, OnStreamPadding(1, 4));
  }

  EXPECT_EQ(frame.size(), deframer_.ProcessInput(frame.data(), frame.size()));
  EXPECT_FALSE(deframer_.HasError());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_NO_ERROR, deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             deframer_.spdy_framer_error());
}

// Test that if we receive a HEADERS frame with padding length larger than the
// payload length, we set an error of SPDY_INVALID_PADDING
TEST_P(SpdyFramerTest, OversizedHeadersPaddingError) {
  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;

  deframer_.set_visitor(&visitor);

  // HEADERS frame with invalid padding length.
  // |kH2FrameData| has to be |unsigned char|, because Chromium on Windows uses
  // MSVC, where |char| is signed by default, which would not compile because of
  // the element exceeding 127.
  unsigned char kH2FrameData[] = {
      0x00, 0x00, 0x05,        // Length: 5
      0x01,                    //   Type: HEADERS
      0x08,                    //  Flags: PADDED
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0xff,                    // PadLen: 255 trailing bytes (Too Long)
      0x00, 0x00, 0x00, 0x00,  // Padding
  };

  SpdySerializedFrame frame(reinterpret_cast<char*>(kH2FrameData),
                            sizeof(kH2FrameData), false);

  EXPECT_CALL(visitor, OnHeaders(1, false, 0, 0, false, false, false));
  EXPECT_CALL(visitor, OnHeaderFrameStart(1)).Times(1);
  EXPECT_CALL(visitor, OnError(Http2DecoderAdapter::SPDY_INVALID_PADDING));
  EXPECT_EQ(frame.size(), deframer_.ProcessInput(frame.data(), frame.size()));
  EXPECT_TRUE(deframer_.HasError());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_INVALID_PADDING,
            deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             deframer_.spdy_framer_error());
}

// Test that if we receive a HEADERS frame with padding length not larger
// than the payload length, we do not set an error of SPDY_INVALID_PADDING
TEST_P(SpdyFramerTest, CorrectlySizedHeadersPaddingNoError) {
  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;

  deframer_.set_visitor(&visitor);

  // HEADERS frame with invalid Padding length
  char kH2FrameData[] = {
      0x00, 0x00, 0x05,        // Length: 5
      0x01,                    //   Type: HEADERS
      0x08,                    //  Flags: PADDED
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0x04,                    // PadLen: 4 trailing bytes
      0x00, 0x00, 0x00, 0x00,  // Padding
  };

  SpdySerializedFrame frame(kH2FrameData, sizeof(kH2FrameData), false);

  EXPECT_CALL(visitor, OnHeaders(1, false, 0, 0, false, false, false));
  EXPECT_CALL(visitor, OnHeaderFrameStart(1)).Times(1);

  EXPECT_EQ(frame.size(), deframer_.ProcessInput(frame.data(), frame.size()));
  EXPECT_FALSE(deframer_.HasError());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_NO_ERROR, deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             deframer_.spdy_framer_error());
}

// Test that if we receive a DATA with stream ID zero, we signal an error
// (but don't crash).
TEST_P(SpdyFramerTest, DataWithStreamIdZero) {
  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;

  deframer_.set_visitor(&visitor);

  const char bytes[] = "hello";
  SpdyDataIR data_ir(/* stream_id = */ 0, bytes);
  SpdySerializedFrame frame(framer_.SerializeData(data_ir));

  // We shouldn't have to read the whole frame before we signal an error.
  EXPECT_CALL(visitor, OnError(Http2DecoderAdapter::SPDY_INVALID_STREAM_ID));
  EXPECT_GT(frame.size(), deframer_.ProcessInput(frame.data(), frame.size()));
  EXPECT_TRUE(deframer_.HasError());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_INVALID_STREAM_ID,
            deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             deframer_.spdy_framer_error());
}

// Test that if we receive a HEADERS with stream ID zero, we signal an error
// (but don't crash).
TEST_P(SpdyFramerTest, HeadersWithStreamIdZero) {
  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;

  deframer_.set_visitor(&visitor);

  SpdyHeadersIR headers(/* stream_id = */ 0);
  headers.SetHeader("alpha", "beta");
  SpdySerializedFrame frame(
      SpdyFramerPeer::SerializeHeaders(&framer_, headers, &output_));

  // We shouldn't have to read the whole frame before we signal an error.
  EXPECT_CALL(visitor, OnError(Http2DecoderAdapter::SPDY_INVALID_STREAM_ID));
  EXPECT_GT(frame.size(), deframer_.ProcessInput(frame.data(), frame.size()));
  EXPECT_TRUE(deframer_.HasError());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_INVALID_STREAM_ID,
            deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             deframer_.spdy_framer_error());
}

// Test that if we receive a PRIORITY with stream ID zero, we signal an error
// (but don't crash).
TEST_P(SpdyFramerTest, PriorityWithStreamIdZero) {
  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;

  deframer_.set_visitor(&visitor);

  SpdyPriorityIR priority_ir(/* stream_id = */ 0,
                             /* parent_stream_id = */ 1,
                             /* weight = */ 16,
                             /* exclusive = */ true);
  SpdySerializedFrame frame(framer_.SerializeFrame(priority_ir));
  if (use_output_) {
    EXPECT_EQ(framer_.SerializeFrame(priority_ir, &output_), frame.size());
    frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
  }

  // We shouldn't have to read the whole frame before we signal an error.
  EXPECT_CALL(visitor, OnError(Http2DecoderAdapter::SPDY_INVALID_STREAM_ID));
  EXPECT_GT(frame.size(), deframer_.ProcessInput(frame.data(), frame.size()));
  EXPECT_TRUE(deframer_.HasError());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_INVALID_STREAM_ID,
            deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             deframer_.spdy_framer_error());
}

// Test that if we receive a RST_STREAM with stream ID zero, we signal an error
// (but don't crash).
TEST_P(SpdyFramerTest, RstStreamWithStreamIdZero) {
  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;

  deframer_.set_visitor(&visitor);

  SpdyRstStreamIR rst_stream_ir(/* stream_id = */ 0, ERROR_CODE_PROTOCOL_ERROR);
  SpdySerializedFrame frame(framer_.SerializeRstStream(rst_stream_ir));
  if (use_output_) {
    EXPECT_TRUE(framer_.SerializeRstStream(rst_stream_ir, &output_));
    frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
  }

  // We shouldn't have to read the whole frame before we signal an error.
  EXPECT_CALL(visitor, OnError(Http2DecoderAdapter::SPDY_INVALID_STREAM_ID));
  EXPECT_GT(frame.size(), deframer_.ProcessInput(frame.data(), frame.size()));
  EXPECT_TRUE(deframer_.HasError());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_INVALID_STREAM_ID,
            deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             deframer_.spdy_framer_error());
}

// Test that if we receive a SETTINGS with stream ID other than zero,
// we signal an error (but don't crash).
TEST_P(SpdyFramerTest, SettingsWithStreamIdNotZero) {
  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;

  deframer_.set_visitor(&visitor);

  // Settings frame with invalid StreamID of 0x01
  char kH2FrameData[] = {
      0x00, 0x00, 0x06,        // Length: 6
      0x04,                    //   Type: SETTINGS
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0x00, 0x04,              //  Param: INITIAL_WINDOW_SIZE
      0x0a, 0x0b, 0x0c, 0x0d,  //  Value: 168496141
  };

  SpdySerializedFrame frame(kH2FrameData, sizeof(kH2FrameData), false);

  // We shouldn't have to read the whole frame before we signal an error.
  EXPECT_CALL(visitor, OnError(Http2DecoderAdapter::SPDY_INVALID_STREAM_ID));
  EXPECT_GT(frame.size(), deframer_.ProcessInput(frame.data(), frame.size()));
  EXPECT_TRUE(deframer_.HasError());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_INVALID_STREAM_ID,
            deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             deframer_.spdy_framer_error());
}

// Test that if we receive a GOAWAY with stream ID other than zero,
// we signal an error (but don't crash).
TEST_P(SpdyFramerTest, GoawayWithStreamIdNotZero) {
  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;

  deframer_.set_visitor(&visitor);

  // GOAWAY frame with invalid StreamID of 0x01
  char kH2FrameData[] = {
      0x00, 0x00, 0x0a,        // Length: 10
      0x07,                    //   Type: GOAWAY
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0x00, 0x00, 0x00, 0x00,  //   Last: 0
      0x00, 0x00, 0x00, 0x00,  //  Error: NO_ERROR
      0x47, 0x41,              // Description
  };

  SpdySerializedFrame frame(kH2FrameData, sizeof(kH2FrameData), false);

  // We shouldn't have to read the whole frame before we signal an error.
  EXPECT_CALL(visitor, OnError(Http2DecoderAdapter::SPDY_INVALID_STREAM_ID));
  EXPECT_GT(frame.size(), deframer_.ProcessInput(frame.data(), frame.size()));
  EXPECT_TRUE(deframer_.HasError());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_INVALID_STREAM_ID,
            deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             deframer_.spdy_framer_error());
}

// Test that if we receive a CONTINUATION with stream ID zero, we signal
// SPDY_INVALID_STREAM_ID.
TEST_P(SpdyFramerTest, ContinuationWithStreamIdZero) {
  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;

  deframer_.set_visitor(&visitor);

  SpdyContinuationIR continuation(/* stream_id = */ 0);
  auto some_nonsense_encoding =
      SpdyMakeUnique<SpdyString>("some nonsense encoding");
  continuation.take_encoding(std::move(some_nonsense_encoding));
  continuation.set_end_headers(true);
  SpdySerializedFrame frame(framer_.SerializeContinuation(continuation));
  if (use_output_) {
    ASSERT_TRUE(framer_.SerializeContinuation(continuation, &output_));
    frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
  }

  // We shouldn't have to read the whole frame before we signal an error.
  EXPECT_CALL(visitor, OnError(Http2DecoderAdapter::SPDY_INVALID_STREAM_ID));
  EXPECT_GT(frame.size(), deframer_.ProcessInput(frame.data(), frame.size()));
  EXPECT_TRUE(deframer_.HasError());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_INVALID_STREAM_ID,
            deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             deframer_.spdy_framer_error());
}

// Test that if we receive a PUSH_PROMISE with stream ID zero, we signal
// SPDY_INVALID_STREAM_ID.
TEST_P(SpdyFramerTest, PushPromiseWithStreamIdZero) {
  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;

  deframer_.set_visitor(&visitor);

  SpdyPushPromiseIR push_promise(/* stream_id = */ 0,
                                 /* promised_stream_id = */ 4);
  push_promise.SetHeader("alpha", "beta");
  SpdySerializedFrame frame(SpdyFramerPeer::SerializePushPromise(
      &framer_, push_promise, use_output_ ? &output_ : nullptr));

  // We shouldn't have to read the whole frame before we signal an error.
  EXPECT_CALL(visitor, OnError(Http2DecoderAdapter::SPDY_INVALID_STREAM_ID));
  EXPECT_GT(frame.size(), deframer_.ProcessInput(frame.data(), frame.size()));
  EXPECT_TRUE(deframer_.HasError());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_INVALID_STREAM_ID,
            deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             deframer_.spdy_framer_error());
}

// Test that if we receive a PUSH_PROMISE with promised stream ID zero, we
// signal SPDY_INVALID_CONTROL_FRAME.
TEST_P(SpdyFramerTest, PushPromiseWithPromisedStreamIdZero) {
  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;

  deframer_.set_visitor(&visitor);

  SpdyPushPromiseIR push_promise(/* stream_id = */ 3,
                                 /* promised_stream_id = */ 0);
  push_promise.SetHeader("alpha", "beta");
  SpdySerializedFrame frame(SpdyFramerPeer::SerializePushPromise(
      &framer_, push_promise, use_output_ ? &output_ : nullptr));

  EXPECT_CALL(visitor,
              OnError(Http2DecoderAdapter::SPDY_INVALID_CONTROL_FRAME));
  deframer_.ProcessInput(frame.data(), frame.size());
  EXPECT_TRUE(deframer_.HasError());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_INVALID_CONTROL_FRAME,
            deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             deframer_.spdy_framer_error());
}

TEST_P(SpdyFramerTest, MultiValueHeader) {
  SpdyFramer framer(SpdyFramer::DISABLE_COMPRESSION);
  SpdyString value("value1\0value2", 13);
  // TODO(jgraettinger): If this pattern appears again, move to test class.
  SpdyHeaderBlock header_set;
  header_set["name"] = value;
  SpdyString buffer;
  HpackEncoder encoder(ObtainHpackHuffmanTable());
  encoder.DisableCompression();
  encoder.EncodeHeaderSet(header_set, &buffer);
  // Frame builder with plentiful buffer size.
  SpdyFrameBuilder frame(1024);
  frame.BeginNewFrame(SpdyFrameType::HEADERS,
                      HEADERS_FLAG_PRIORITY | HEADERS_FLAG_END_HEADERS, 3,
                      buffer.size() + 5 /* priority */);
  frame.WriteUInt32(0);   // Priority exclusivity and dependent stream.
  frame.WriteUInt8(255);  // Priority weight.
  frame.WriteBytes(&buffer[0], buffer.size());

  SpdySerializedFrame control_frame(frame.take());

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame.data()),
      control_frame.size());

  EXPECT_THAT(visitor.headers_, testing::ElementsAre(testing::Pair(
                                    "name", SpdyStringPiece(value))));
}

TEST_P(SpdyFramerTest, CompressEmptyHeaders) {
  // See https://crbug.com/172383/
  SpdyHeadersIR headers(1);
  headers.SetHeader("server", "SpdyServer 1.0");
  headers.SetHeader("date", "Mon 12 Jan 2009 12:12:12 PST");
  headers.SetHeader("status", "200");
  headers.SetHeader("version", "HTTP/1.1");
  headers.SetHeader("content-type", "text/html");
  headers.SetHeader("content-length", "12");
  headers.SetHeader("x-empty-header", "");

  SpdyFramer framer(SpdyFramer::ENABLE_COMPRESSION);
  SpdySerializedFrame frame1(
      SpdyFramerPeer::SerializeHeaders(&framer, headers, &output_));
}

TEST_P(SpdyFramerTest, Basic) {
  // Send HEADERS frames with PRIORITY and END_HEADERS set.
  // frame-format off
  const unsigned char kH2Input[] = {
      0x00, 0x00, 0x05,        // Length: 5
      0x01,                    //   Type: HEADERS
      0x24,                    //  Flags: END_HEADERS|PRIORITY
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0x00, 0x00, 0x00, 0x00,  // Parent: 0
      0x82,                    // Weight: 131

      0x00, 0x00, 0x01,        // Length: 1
      0x01,                    //   Type: HEADERS
      0x04,                    //  Flags: END_HEADERS
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0x8c,                    // :status: 200

      0x00, 0x00, 0x0c,        // Length: 12
      0x00,                    //   Type: DATA
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0xde, 0xad, 0xbe, 0xef,  // Payload
      0xde, 0xad, 0xbe, 0xef,  //
      0xde, 0xad, 0xbe, 0xef,  //

      0x00, 0x00, 0x05,        // Length: 5
      0x01,                    //   Type: HEADERS
      0x24,                    //  Flags: END_HEADERS|PRIORITY
      0x00, 0x00, 0x00, 0x03,  // Stream: 3
      0x00, 0x00, 0x00, 0x00,  // Parent: 0
      0x82,                    // Weight: 131

      0x00, 0x00, 0x08,        // Length: 8
      0x00,                    //   Type: DATA
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x03,  // Stream: 3
      0xde, 0xad, 0xbe, 0xef,  // Payload
      0xde, 0xad, 0xbe, 0xef,  //

      0x00, 0x00, 0x04,        // Length: 4
      0x00,                    //   Type: DATA
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0xde, 0xad, 0xbe, 0xef,  // Payload

      0x00, 0x00, 0x04,        // Length: 4
      0x03,                    //   Type: RST_STREAM
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0x00, 0x00, 0x00, 0x08,  //  Error: CANCEL

      0x00, 0x00, 0x00,        // Length: 0
      0x00,                    //   Type: DATA
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x03,  // Stream: 3

      0x00, 0x00, 0x04,        // Length: 4
      0x03,                    //   Type: RST_STREAM
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x03,  // Stream: 3
      0x00, 0x00, 0x00, 0x08,  //  Error: CANCEL
  };
  // frame-format on

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(kH2Input, sizeof(kH2Input));

  EXPECT_EQ(24, visitor.data_bytes_);
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(2, visitor.fin_frame_count_);

  EXPECT_EQ(3, visitor.headers_frame_count_);

  EXPECT_EQ(0, visitor.fin_flag_count_);
  EXPECT_EQ(0, visitor.end_of_stream_count_);
  EXPECT_EQ(4, visitor.data_frame_count_);
}

// Test that the FIN flag on a data frame signifies EOF.
TEST_P(SpdyFramerTest, FinOnDataFrame) {
  // Send HEADERS frames with END_HEADERS set.
  // frame-format off
  const unsigned char kH2Input[] = {
      0x00, 0x00, 0x05,        // Length: 5
      0x01,                    //   Type: HEADERS
      0x24,                    //  Flags: END_HEADERS|PRIORITY
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0x00, 0x00, 0x00, 0x00,  // Parent: 0
      0x82,                    // Weight: 131

      0x00, 0x00, 0x01,        // Length: 1
      0x01,                    //   Type: HEADERS
      0x04,                    //  Flags: END_HEADERS
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0x8c,                    // :status: 200

      0x00, 0x00, 0x0c,        // Length: 12
      0x00,                    //   Type: DATA
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0xde, 0xad, 0xbe, 0xef,  // Payload
      0xde, 0xad, 0xbe, 0xef,  //
      0xde, 0xad, 0xbe, 0xef,  //

      0x00, 0x00, 0x04,        // Length: 4
      0x00,                    //   Type: DATA
      0x01,                    //  Flags: END_STREAM
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0xde, 0xad, 0xbe, 0xef,  // Payload
  };
  // frame-format on

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(kH2Input, sizeof(kH2Input));

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(2, visitor.headers_frame_count_);
  EXPECT_EQ(16, visitor.data_bytes_);
  EXPECT_EQ(0, visitor.fin_frame_count_);
  EXPECT_EQ(0, visitor.fin_flag_count_);
  EXPECT_EQ(1, visitor.end_of_stream_count_);
  EXPECT_EQ(2, visitor.data_frame_count_);
}

TEST_P(SpdyFramerTest, FinOnHeadersFrame) {
  // Send HEADERS frames with END_HEADERS set.
  // frame-format off
  const unsigned char kH2Input[] = {
      0x00, 0x00, 0x05,        // Length: 5
      0x01,                    //   Type: HEADERS
      0x24,                    //  Flags: END_HEADERS|PRIORITY
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0x00, 0x00, 0x00, 0x00,  // Parent: 0
      0x82,                    // Weight: 131

      0x00, 0x00, 0x01,        // Length: 1
      0x01,                    //   Type: HEADERS
      0x05,                    //  Flags: END_STREAM|END_HEADERS
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0x8c,                    // :status: 200
  };
  // frame-format on

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(kH2Input, sizeof(kH2Input));

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(2, visitor.headers_frame_count_);
  EXPECT_EQ(0, visitor.data_bytes_);
  EXPECT_EQ(0, visitor.fin_frame_count_);
  EXPECT_EQ(1, visitor.fin_flag_count_);
  EXPECT_EQ(1, visitor.end_of_stream_count_);
  EXPECT_EQ(0, visitor.data_frame_count_);
}

// Verify we can decompress the stream even if handed over to the
// framer 1 byte at a time.
TEST_P(SpdyFramerTest, UnclosedStreamDataCompressorsOneByteAtATime) {
  const char kHeader1[] = "header1";
  const char kHeader2[] = "header2";
  const char kValue1[] = "value1";
  const char kValue2[] = "value2";

  SpdyHeadersIR headers(/* stream_id = */ 1);
  headers.SetHeader(kHeader1, kValue1);
  headers.SetHeader(kHeader2, kValue2);
  SpdySerializedFrame headers_frame(SpdyFramerPeer::SerializeHeaders(
      &framer_, headers, use_output_ ? &output_ : nullptr));

  const char bytes[] = "this is a test test test test test!";
  SpdyDataIR data_ir(/* stream_id = */ 1,
                     SpdyStringPiece(bytes, SPDY_ARRAYSIZE(bytes)));
  data_ir.set_fin(true);
  SpdySerializedFrame send_frame(framer_.SerializeData(data_ir));

  // Run the inputs through the framer.
  TestSpdyVisitor visitor(SpdyFramer::ENABLE_COMPRESSION);
  const unsigned char* data;
  data = reinterpret_cast<const unsigned char*>(headers_frame.data());
  for (size_t idx = 0; idx < headers_frame.size(); ++idx) {
    visitor.SimulateInFramer(data + idx, 1);
    ASSERT_EQ(0, visitor.error_count_);
  }
  data = reinterpret_cast<const unsigned char*>(send_frame.data());
  for (size_t idx = 0; idx < send_frame.size(); ++idx) {
    visitor.SimulateInFramer(data + idx, 1);
    ASSERT_EQ(0, visitor.error_count_);
  }

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(SPDY_ARRAYSIZE(bytes), static_cast<unsigned>(visitor.data_bytes_));
  EXPECT_EQ(0, visitor.fin_frame_count_);
  EXPECT_EQ(0, visitor.fin_flag_count_);
  EXPECT_EQ(1, visitor.end_of_stream_count_);
  EXPECT_EQ(1, visitor.data_frame_count_);
}

TEST_P(SpdyFramerTest, WindowUpdateFrame) {
  SpdyWindowUpdateIR window_update(/* stream_id = */ 1,
                                   /* delta = */ 0x12345678);
  SpdySerializedFrame frame(framer_.SerializeWindowUpdate(window_update));
  if (use_output_) {
    ASSERT_TRUE(framer_.SerializeWindowUpdate(window_update, &output_));
    frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
  }

  const char kDescription[] = "WINDOW_UPDATE frame, stream 1, delta 0x12345678";
  const unsigned char kH2FrameData[] = {
      0x00, 0x00, 0x04,        // Length: 4
      0x08,                    //   Type: WINDOW_UPDATE
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0x12, 0x34, 0x56, 0x78,  // Increment: 305419896
  };

  CompareFrame(kDescription, frame, kH2FrameData, SPDY_ARRAYSIZE(kH2FrameData));
}

TEST_P(SpdyFramerTest, CreateDataFrame) {
  {
    const char kDescription[] = "'hello' data frame, no FIN";
    // frame-format off
    const unsigned char kH2FrameData[] = {
        0x00, 0x00, 0x05,        // Length: 5
        0x00,                    //   Type: DATA
        0x00,                    //  Flags: none
        0x00, 0x00, 0x00, 0x01,  // Stream: 1
        'h',  'e',  'l',  'l',   // Payload
        'o',                     //
    };
    // frame-format on
    const char bytes[] = "hello";

    SpdyDataIR data_ir(/* stream_id = */ 1, bytes);
    SpdySerializedFrame frame(framer_.SerializeData(data_ir));
    CompareFrame(kDescription, frame, kH2FrameData,
                 SPDY_ARRAYSIZE(kH2FrameData));

    SpdyDataIR data_header_ir(/* stream_id = */ 1);
    data_header_ir.SetDataShallow(bytes);
    frame =
        framer_.SerializeDataFrameHeaderWithPaddingLengthField(data_header_ir);
    CompareCharArraysWithHexError(
        kDescription, reinterpret_cast<const unsigned char*>(frame.data()),
        kDataFrameMinimumSize, kH2FrameData, kDataFrameMinimumSize);
  }

  {
    const char kDescription[] = "'hello' data frame with more padding, no FIN";
    // clang-format off
    // frame-format off
    const unsigned char kH2FrameData[] = {
        0x00, 0x00, 0xfd,        // Length: 253
        0x00,                    //   Type: DATA
        0x08,                    //  Flags: PADDED
        0x00, 0x00, 0x00, 0x01,  // Stream: 1
        0xf7,                    // PadLen: 247 trailing bytes
        'h', 'e', 'l', 'l',      // Payload
        'o',                     //
        // Padding of 247 0x00(s).
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    // frame-format on
    // clang-format on
    const char bytes[] = "hello";

    SpdyDataIR data_ir(/* stream_id = */ 1, bytes);
    // 247 zeros and the pad length field make the overall padding to be 248
    // bytes.
    data_ir.set_padding_len(248);
    SpdySerializedFrame frame(framer_.SerializeData(data_ir));
    CompareFrame(kDescription, frame, kH2FrameData,
                 SPDY_ARRAYSIZE(kH2FrameData));

    frame = framer_.SerializeDataFrameHeaderWithPaddingLengthField(data_ir);
    CompareCharArraysWithHexError(
        kDescription, reinterpret_cast<const unsigned char*>(frame.data()),
        kDataFrameMinimumSize, kH2FrameData, kDataFrameMinimumSize);
  }

  {
    const char kDescription[] = "'hello' data frame with few padding, no FIN";
    // frame-format off
    const unsigned char kH2FrameData[] = {
        0x00, 0x00, 0x0d,        // Length: 13
        0x00,                    //   Type: DATA
        0x08,                    //  Flags: PADDED
        0x00, 0x00, 0x00, 0x01,  // Stream: 1
        0x07,                    // PadLen: 7 trailing bytes
        'h',  'e',  'l',  'l',   // Payload
        'o',                     //
        0x00, 0x00, 0x00, 0x00,  // Padding
        0x00, 0x00, 0x00,        // Padding
    };
    // frame-format on
    const char bytes[] = "hello";

    SpdyDataIR data_ir(/* stream_id = */ 1, bytes);
    // 7 zeros and the pad length field make the overall padding to be 8 bytes.
    data_ir.set_padding_len(8);
    SpdySerializedFrame frame(framer_.SerializeData(data_ir));
    CompareFrame(kDescription, frame, kH2FrameData,
                 SPDY_ARRAYSIZE(kH2FrameData));
  }

  {
    const char kDescription[] =
        "'hello' data frame with 1 byte padding, no FIN";
    // frame-format off
    const unsigned char kH2FrameData[] = {
        0x00, 0x00, 0x06,        // Length: 6
        0x00,                    //   Type: DATA
        0x08,                    //  Flags: PADDED
        0x00, 0x00, 0x00, 0x01,  // Stream: 1
        0x00,                    // PadLen: 0 trailing bytes
        'h',  'e',  'l',  'l',   // Payload
        'o',                     //
    };
    // frame-format on
    const char bytes[] = "hello";

    SpdyDataIR data_ir(/* stream_id = */ 1, bytes);
    // The pad length field itself is used for the 1-byte padding and no padding
    // payload is needed.
    data_ir.set_padding_len(1);
    SpdySerializedFrame frame(framer_.SerializeData(data_ir));
    CompareFrame(kDescription, frame, kH2FrameData,
                 SPDY_ARRAYSIZE(kH2FrameData));

    frame = framer_.SerializeDataFrameHeaderWithPaddingLengthField(data_ir);
    CompareCharArraysWithHexError(
        kDescription, reinterpret_cast<const unsigned char*>(frame.data()),
        kDataFrameMinimumSize, kH2FrameData, kDataFrameMinimumSize);
  }

  {
    const char kDescription[] = "Data frame with negative data byte, no FIN";
    const unsigned char kH2FrameData[] = {
        0x00, 0x00, 0x01,        // Length: 1
        0x00,                    //   Type: DATA
        0x00,                    //  Flags: none
        0x00, 0x00, 0x00, 0x01,  // Stream: 1
        0xff,                    // Payload
    };
    SpdyDataIR data_ir(/* stream_id = */ 1, "\xff");
    SpdySerializedFrame frame(framer_.SerializeData(data_ir));
    CompareFrame(kDescription, frame, kH2FrameData,
                 SPDY_ARRAYSIZE(kH2FrameData));
  }

  {
    const char kDescription[] = "'hello' data frame, with FIN";
    const unsigned char kH2FrameData[] = {
        0x00, 0x00, 0x05,        // Length: 5
        0x00,                    //   Type: DATA
        0x01,                    //  Flags: END_STREAM
        0x00, 0x00, 0x00, 0x01,  // Stream: 1
        0x68, 0x65, 0x6c, 0x6c,  // Payload
        0x6f,                    //
    };
    SpdyDataIR data_ir(/* stream_id = */ 1, "hello");
    data_ir.set_fin(true);
    SpdySerializedFrame frame(framer_.SerializeData(data_ir));
    CompareFrame(kDescription, frame, kH2FrameData,
                 SPDY_ARRAYSIZE(kH2FrameData));
  }

  {
    const char kDescription[] = "Empty data frame";
    const unsigned char kH2FrameData[] = {
        0x00, 0x00, 0x00,        // Length: 0
        0x00,                    //   Type: DATA
        0x00,                    //  Flags: none
        0x00, 0x00, 0x00, 0x01,  // Stream: 1
    };
    SpdyDataIR data_ir(/* stream_id = */ 1, "");
    SpdySerializedFrame frame(framer_.SerializeData(data_ir));
    CompareFrame(kDescription, frame, kH2FrameData,
                 SPDY_ARRAYSIZE(kH2FrameData));

    frame = framer_.SerializeDataFrameHeaderWithPaddingLengthField(data_ir);
    CompareCharArraysWithHexError(
        kDescription, reinterpret_cast<const unsigned char*>(frame.data()),
        kDataFrameMinimumSize, kH2FrameData, kDataFrameMinimumSize);
  }

  {
    const char kDescription[] = "Data frame with max stream ID";
    const unsigned char kH2FrameData[] = {
        0x00, 0x00, 0x05,        // Length: 5
        0x00,                    //   Type: DATA
        0x01,                    //  Flags: END_STREAM
        0x7f, 0xff, 0xff, 0xff,  // Stream: 0x7fffffff
        0x68, 0x65, 0x6c, 0x6c,  // Payload
        0x6f,                    //
    };
    SpdyDataIR data_ir(/* stream_id = */ 0x7fffffff, "hello");
    data_ir.set_fin(true);
    SpdySerializedFrame frame(framer_.SerializeData(data_ir));
    CompareFrame(kDescription, frame, kH2FrameData,
                 SPDY_ARRAYSIZE(kH2FrameData));
  }
}

TEST_P(SpdyFramerTest, CreateRstStream) {
  SpdyFramer framer(SpdyFramer::ENABLE_COMPRESSION);

  {
    const char kDescription[] = "RST_STREAM frame";
    const unsigned char kH2FrameData[] = {
        0x00, 0x00, 0x04,        // Length: 4
        0x03,                    //   Type: RST_STREAM
        0x00,                    //  Flags: none
        0x00, 0x00, 0x00, 0x01,  // Stream: 1
        0x00, 0x00, 0x00, 0x01,  //  Error: PROTOCOL_ERROR
    };
    SpdyRstStreamIR rst_stream(/* stream_id = */ 1, ERROR_CODE_PROTOCOL_ERROR);
    SpdySerializedFrame frame(framer_.SerializeRstStream(rst_stream));
    if (use_output_) {
      ASSERT_TRUE(framer_.SerializeRstStream(rst_stream, &output_));
      frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
    }
    CompareFrame(kDescription, frame, kH2FrameData,
                 SPDY_ARRAYSIZE(kH2FrameData));
  }

  {
    const char kDescription[] = "RST_STREAM frame with max stream ID";
    // clang-format off
    const unsigned char kH2FrameData[] = {
        0x00, 0x00, 0x04,        // Length: 4
        0x03,                    //   Type: RST_STREAM
        0x00,                    //  Flags: none
        0x7f, 0xff, 0xff, 0xff,  // Stream: 0x7fffffff
        0x00, 0x00, 0x00, 0x01,  //  Error: PROTOCOL_ERROR
    };
    SpdyRstStreamIR rst_stream(/* stream_id = */ 0x7FFFFFFF,
                               ERROR_CODE_PROTOCOL_ERROR);
    SpdySerializedFrame frame(framer_.SerializeRstStream(rst_stream));
    if (use_output_) {
      output_.Reset();
      ASSERT_TRUE(framer_.SerializeRstStream(rst_stream, &output_));
      frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
    }
    CompareFrame(kDescription, frame, kH2FrameData,
                 SPDY_ARRAYSIZE(kH2FrameData));
  }

  {
    const char kDescription[] = "RST_STREAM frame with max status code";
    const unsigned char kH2FrameData[] = {
        0x00, 0x00, 0x04,        // Length: 4
        0x03,                    //   Type: RST_STREAM
        0x00,                    //  Flags: none
        0x7f, 0xff, 0xff, 0xff,  // Stream: 0x7fffffff
        0x00, 0x00, 0x00, 0x02,  //  Error: INTERNAL_ERROR
    };
    SpdyRstStreamIR rst_stream(/* stream_id = */ 0x7FFFFFFF,
                               ERROR_CODE_INTERNAL_ERROR);
    SpdySerializedFrame frame(framer_.SerializeRstStream(rst_stream));
    if (use_output_) {
      output_.Reset();
      ASSERT_TRUE(framer_.SerializeRstStream(rst_stream, &output_));
      frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
    }
    CompareFrame(kDescription, frame, kH2FrameData,
                 SPDY_ARRAYSIZE(kH2FrameData));
  }
}

TEST_P(SpdyFramerTest, CreateSettings) {
  {
    const char kDescription[] = "Network byte order SETTINGS frame";
    const unsigned char kH2FrameData[] = {
        0x00, 0x00, 0x06,        // Length: 6
        0x04,                    //   Type: SETTINGS
        0x00,                    //  Flags: none
        0x00, 0x00, 0x00, 0x00,  // Stream: 0
        0x00, 0x04,              //  Param: INITIAL_WINDOW_SIZE
        0x0a, 0x0b, 0x0c, 0x0d,  //  Value: 168496141
    };

    uint32_t kValue = 0x0a0b0c0d;
    SpdySettingsIR settings_ir;

    SpdyKnownSettingsId kId = SETTINGS_INITIAL_WINDOW_SIZE;
    settings_ir.AddSetting(kId, kValue);

    SpdySerializedFrame frame(framer_.SerializeSettings(settings_ir));
    if (use_output_) {
      ASSERT_TRUE(framer_.SerializeSettings(settings_ir, &output_));
      frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
    }
    CompareFrame(kDescription, frame, kH2FrameData,
                 SPDY_ARRAYSIZE(kH2FrameData));
  }

  {
    const char kDescription[] = "Basic SETTINGS frame";
    // These end up seemingly out of order because of the way that our internal
    // ordering for settings_ir works. HTTP2 has no requirement on ordering on
    // the wire.
    const unsigned char kH2FrameData[] = {
        0x00, 0x00, 0x18,        // Length: 24
        0x04,                    //   Type: SETTINGS
        0x00,                    //  Flags: none
        0x00, 0x00, 0x00, 0x00,  // Stream: 0
        0x00, 0x01,              //  Param: HEADER_TABLE_SIZE
        0x00, 0x00, 0x00, 0x05,  //  Value: 5
        0x00, 0x02,              //  Param: ENABLE_PUSH
        0x00, 0x00, 0x00, 0x06,  //  Value: 6
        0x00, 0x03,              //  Param: MAX_CONCURRENT_STREAMS
        0x00, 0x00, 0x00, 0x07,  //  Value: 7
        0x00, 0x04,              //  Param: INITIAL_WINDOW_SIZE
        0x00, 0x00, 0x00, 0x08,  //  Value: 8
    };

    SpdySettingsIR settings_ir;
    settings_ir.AddSetting(SETTINGS_HEADER_TABLE_SIZE, 5);
    settings_ir.AddSetting(SETTINGS_ENABLE_PUSH, 6);
    settings_ir.AddSetting(SETTINGS_MAX_CONCURRENT_STREAMS, 7);
    settings_ir.AddSetting(SETTINGS_INITIAL_WINDOW_SIZE, 8);
    SpdySerializedFrame frame(framer_.SerializeSettings(settings_ir));
    if (use_output_) {
      output_.Reset();
      ASSERT_TRUE(framer_.SerializeSettings(settings_ir, &output_));
      frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
    }

    CompareFrame(kDescription, frame, kH2FrameData,
                 SPDY_ARRAYSIZE(kH2FrameData));
  }

  {
    const char kDescription[] = "Empty SETTINGS frame";
    const unsigned char kH2FrameData[] = {
        0x00, 0x00, 0x00,        // Length: 0
        0x04,                    //   Type: SETTINGS
        0x00,                    //  Flags: none
        0x00, 0x00, 0x00, 0x00,  // Stream: 0
    };
    SpdySettingsIR settings_ir;
    SpdySerializedFrame frame(framer_.SerializeSettings(settings_ir));
    if (use_output_) {
      output_.Reset();
      ASSERT_TRUE(framer_.SerializeSettings(settings_ir, &output_));
      frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
    }

    CompareFrame(kDescription, frame, kH2FrameData,
                 SPDY_ARRAYSIZE(kH2FrameData));
  }
}

TEST_P(SpdyFramerTest, CreatePingFrame) {
  {
    const char kDescription[] = "PING frame";
    const unsigned char kH2FrameData[] = {
        0x00, 0x00, 0x08,        // Length: 8
        0x06,                    //   Type: PING
        0x00,                    //  Flags: none
        0x00, 0x00, 0x00, 0x00,  // Stream: 0
        0x12, 0x34, 0x56, 0x78,  // Opaque
        0x9a, 0xbc, 0xde, 0xff,  //     Data
    };
    const unsigned char kH2FrameDataWithAck[] = {
        0x00, 0x00, 0x08,        // Length: 8
        0x06,                    //   Type: PING
        0x01,                    //  Flags: ACK
        0x00, 0x00, 0x00, 0x00,  // Stream: 0
        0x12, 0x34, 0x56, 0x78,  // Opaque
        0x9a, 0xbc, 0xde, 0xff,  //     Data
    };
    const SpdyPingId kPingId = 0x123456789abcdeffULL;
    SpdyPingIR ping_ir(kPingId);
    // Tests SpdyPingIR when the ping is not an ack.
    ASSERT_FALSE(ping_ir.is_ack());
    SpdySerializedFrame frame(framer_.SerializePing(ping_ir));
    if (use_output_) {
      ASSERT_TRUE(framer_.SerializePing(ping_ir, &output_));
      frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
    }
    CompareFrame(kDescription, frame, kH2FrameData,
                 SPDY_ARRAYSIZE(kH2FrameData));

    // Tests SpdyPingIR when the ping is an ack.
    ping_ir.set_is_ack(true);
    frame = framer_.SerializePing(ping_ir);
    if (use_output_) {
      output_.Reset();
      ASSERT_TRUE(framer_.SerializePing(ping_ir, &output_));
      frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
    }
    CompareFrame(kDescription, frame, kH2FrameDataWithAck,
                 SPDY_ARRAYSIZE(kH2FrameDataWithAck));
  }
}

TEST_P(SpdyFramerTest, CreateGoAway) {
  {
    const char kDescription[] = "GOAWAY frame";
    const unsigned char kH2FrameData[] = {
        0x00, 0x00, 0x0a,        // Length: 10
        0x07,                    //   Type: GOAWAY
        0x00,                    //  Flags: none
        0x00, 0x00, 0x00, 0x00,  // Stream: 0
        0x00, 0x00, 0x00, 0x00,  //   Last: 0
        0x00, 0x00, 0x00, 0x00,  //  Error: NO_ERROR
        0x47, 0x41,              // Description
    };
    SpdyGoAwayIR goaway_ir(/* last_good_stream_id = */ 0, ERROR_CODE_NO_ERROR,
                           "GA");
    SpdySerializedFrame frame(framer_.SerializeGoAway(goaway_ir));
    if (use_output_) {
      ASSERT_TRUE(framer_.SerializeGoAway(goaway_ir, &output_));
      frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
    }
    CompareFrame(kDescription, frame, kH2FrameData,
                 SPDY_ARRAYSIZE(kH2FrameData));
  }

  {
    const char kDescription[] = "GOAWAY frame with max stream ID, status";
    const unsigned char kH2FrameData[] = {
        0x00, 0x00, 0x0a,        // Length: 10
        0x07,                    //   Type: GOAWAY
        0x00,                    //  Flags: none
        0x00, 0x00, 0x00, 0x00,  // Stream: 0
        0x7f, 0xff, 0xff, 0xff,  //   Last: 0x7fffffff
        0x00, 0x00, 0x00, 0x02,  //  Error: INTERNAL_ERROR
        0x47, 0x41,              // Description
    };
    SpdyGoAwayIR goaway_ir(/* last_good_stream_id = */ 0x7FFFFFFF,
                           ERROR_CODE_INTERNAL_ERROR, "GA");
    SpdySerializedFrame frame(framer_.SerializeGoAway(goaway_ir));
    if (use_output_) {
      output_.Reset();
      ASSERT_TRUE(framer_.SerializeGoAway(goaway_ir, &output_));
      frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
    }
    CompareFrame(kDescription, frame, kH2FrameData,
                 SPDY_ARRAYSIZE(kH2FrameData));
  }
}

TEST_P(SpdyFramerTest, CreateHeadersUncompressed) {
  SpdyFramer framer(SpdyFramer::DISABLE_COMPRESSION);

  {
    const char kDescription[] = "HEADERS frame, no FIN";
    // frame-format off
    const unsigned char kH2FrameData[] = {
        0x00, 0x00, 0x12,        // Length: 18
        0x01,                    //   Type: HEADERS
        0x04,                    //  Flags: END_HEADERS
        0x00, 0x00, 0x00, 0x01,  // Stream: 1

        0x00,              // Unindexed Entry
        0x03,              // Name Len: 3
        0x62, 0x61, 0x72,  // bar
        0x03,              // Value Len: 3
        0x66, 0x6f, 0x6f,  // foo

        0x00,              // Unindexed Entry
        0x03,              // Name Len: 3
        0x66, 0x6f, 0x6f,  // foo
        0x03,              // Value Len: 3
        0x62, 0x61, 0x72,  // bar
    };
    // frame-format on
    SpdyHeadersIR headers(/* stream_id = */ 1);
    headers.SetHeader("bar", "foo");
    headers.SetHeader("foo", "bar");
    SpdySerializedFrame frame(SpdyFramerPeer::SerializeHeaders(
        &framer, headers, use_output_ ? &output_ : nullptr));
    CompareFrame(kDescription, frame, kH2FrameData,
                 SPDY_ARRAYSIZE(kH2FrameData));
  }

  {
    const char kDescription[] =
        "HEADERS frame with a 0-length header name, FIN, max stream ID";
    // frame-format off
    const unsigned char kH2FrameData[] = {
        0x00, 0x00, 0x0f,        // Length: 15
        0x01,                    //   Type: HEADERS
        0x05,                    //  Flags: END_STREAM|END_HEADERS
        0x7f, 0xff, 0xff, 0xff,  // Stream: 2147483647

        0x00,              // Unindexed Entry
        0x00,              // Name Len: 0
        0x03,              // Value Len: 3
        0x66, 0x6f, 0x6f,  // foo

        0x00,              // Unindexed Entry
        0x03,              // Name Len: 3
        0x66, 0x6f, 0x6f,  // foo
        0x03,              // Value Len: 3
        0x62, 0x61, 0x72,  // bar
    };
    // frame-format on
    SpdyHeadersIR headers(/* stream_id = */ 0x7fffffff);
    headers.set_fin(true);
    headers.SetHeader("", "foo");
    headers.SetHeader("foo", "bar");
    SpdySerializedFrame frame(SpdyFramerPeer::SerializeHeaders(
        &framer, headers, use_output_ ? &output_ : nullptr));
    CompareFrame(kDescription, frame, kH2FrameData,
                 SPDY_ARRAYSIZE(kH2FrameData));
  }

  {
    const char kDescription[] =
        "HEADERS frame with a 0-length header val, FIN, max stream ID";
    // frame-format off
    const unsigned char kH2FrameData[] = {
        0x00, 0x00, 0x0f,        // Length: 15
        0x01,                    //   Type: HEADERS
        0x05,                    //  Flags: END_STREAM|END_HEADERS
        0x7f, 0xff, 0xff, 0xff,  // Stream: 2147483647

        0x00,              // Unindexed Entry
        0x03,              // Name Len: 3
        0x62, 0x61, 0x72,  // bar
        0x03,              // Value Len: 3
        0x66, 0x6f, 0x6f,  // foo

        0x00,              // Unindexed Entry
        0x03,              // Name Len: 3
        0x66, 0x6f, 0x6f,  // foo
        0x00,              // Value Len: 0
    };
    // frame-format on
    SpdyHeadersIR headers_ir(/* stream_id = */ 0x7fffffff);
    headers_ir.set_fin(true);
    headers_ir.SetHeader("bar", "foo");
    headers_ir.SetHeader("foo", "");
    SpdySerializedFrame frame(SpdyFramerPeer::SerializeHeaders(
        &framer, headers_ir, use_output_ ? &output_ : nullptr));
    CompareFrame(kDescription, frame, kH2FrameData,
                 SPDY_ARRAYSIZE(kH2FrameData));
  }

  {
    const char kDescription[] =
        "HEADERS frame with a 0-length header val, FIN, max stream ID, pri";

    // frame-format off
    const unsigned char kH2FrameData[] = {
        0x00, 0x00, 0x14,        // Length: 20
        0x01,                    //   Type: HEADERS
        0x25,                    //  Flags: END_STREAM|END_HEADERS|PRIORITY
        0x7f, 0xff, 0xff, 0xff,  // Stream: 2147483647
        0x00, 0x00, 0x00, 0x00,  // Parent: 0
        0xdb,                    // Weight: 220

        0x00,              // Unindexed Entry
        0x03,              // Name Len: 3
        0x62, 0x61, 0x72,  // bar
        0x03,              // Value Len: 3
        0x66, 0x6f, 0x6f,  // foo

        0x00,              // Unindexed Entry
        0x03,              // Name Len: 3
        0x66, 0x6f, 0x6f,  // foo
        0x00,              // Value Len: 0
    };
    // frame-format on
    SpdyHeadersIR headers_ir(/* stream_id = */ 0x7fffffff);
    headers_ir.set_fin(true);
    headers_ir.set_has_priority(true);
    headers_ir.set_weight(220);
    headers_ir.SetHeader("bar", "foo");
    headers_ir.SetHeader("foo", "");
    SpdySerializedFrame frame(SpdyFramerPeer::SerializeHeaders(
        &framer, headers_ir, use_output_ ? &output_ : nullptr));
    CompareFrame(kDescription, frame, kH2FrameData,
                 SPDY_ARRAYSIZE(kH2FrameData));
  }

  {
    const char kDescription[] =
        "HEADERS frame with a 0-length header val, FIN, max stream ID, pri, "
        "exclusive=true, parent_stream=0";

    // frame-format off
    const unsigned char kV4FrameData[] = {
        0x00, 0x00, 0x14,        // Length: 20
        0x01,                    //   Type: HEADERS
        0x25,                    //  Flags: END_STREAM|END_HEADERS|PRIORITY
        0x7f, 0xff, 0xff, 0xff,  // Stream: 2147483647
        0x80, 0x00, 0x00, 0x00,  // Parent: 0 (Exclusive)
        0xdb,                    // Weight: 220

        0x00,              // Unindexed Entry
        0x03,              // Name Len: 3
        0x62, 0x61, 0x72,  // bar
        0x03,              // Value Len: 3
        0x66, 0x6f, 0x6f,  // foo

        0x00,              // Unindexed Entry
        0x03,              // Name Len: 3
        0x66, 0x6f, 0x6f,  // foo
        0x00,              // Value Len: 0
    };
    // frame-format on
    SpdyHeadersIR headers_ir(/* stream_id = */ 0x7fffffff);
    headers_ir.set_fin(true);
    headers_ir.set_has_priority(true);
    headers_ir.set_weight(220);
    headers_ir.set_exclusive(true);
    headers_ir.set_parent_stream_id(0);
    headers_ir.SetHeader("bar", "foo");
    headers_ir.SetHeader("foo", "");
    SpdySerializedFrame frame(SpdyFramerPeer::SerializeHeaders(
        &framer, headers_ir, use_output_ ? &output_ : nullptr));
    CompareFrame(kDescription, frame, kV4FrameData,
                 SPDY_ARRAYSIZE(kV4FrameData));
  }

  {
    const char kDescription[] =
        "HEADERS frame with a 0-length header val, FIN, max stream ID, pri, "
        "exclusive=false, parent_stream=max stream ID";

    // frame-format off
    const unsigned char kV4FrameData[] = {
        0x00, 0x00, 0x14,        // Length: 20
        0x01,                    //   Type: HEADERS
        0x25,                    //  Flags: END_STREAM|END_HEADERS|PRIORITY
        0x7f, 0xff, 0xff, 0xff,  // Stream: 2147483647
        0x7f, 0xff, 0xff, 0xff,  // Parent: 2147483647
        0xdb,                    // Weight: 220

        0x00,              // Unindexed Entry
        0x03,              // Name Len: 3
        0x62, 0x61, 0x72,  // bar
        0x03,              // Value Len: 3
        0x66, 0x6f, 0x6f,  // foo

        0x00,              // Unindexed Entry
        0x03,              // Name Len: 3
        0x66, 0x6f, 0x6f,  // foo
        0x00,              // Value Len: 0
    };
    // frame-format on
    SpdyHeadersIR headers_ir(/* stream_id = */ 0x7fffffff);
    headers_ir.set_fin(true);
    headers_ir.set_has_priority(true);
    headers_ir.set_weight(220);
    headers_ir.set_exclusive(false);
    headers_ir.set_parent_stream_id(0x7fffffff);
    headers_ir.SetHeader("bar", "foo");
    headers_ir.SetHeader("foo", "");
    SpdySerializedFrame frame(SpdyFramerPeer::SerializeHeaders(
        &framer, headers_ir, use_output_ ? &output_ : nullptr));
    CompareFrame(kDescription, frame, kV4FrameData,
                 SPDY_ARRAYSIZE(kV4FrameData));
  }

  {
    const char kDescription[] =
        "HEADERS frame with a 0-length header name, FIN, max stream ID, padded";

    // frame-format off
    const unsigned char kH2FrameData[] = {
        0x00, 0x00, 0x15,        // Length: 21
        0x01,                    //   Type: HEADERS
        0x0d,                    //  Flags: END_STREAM|END_HEADERS|PADDED
        0x7f, 0xff, 0xff, 0xff,  // Stream: 2147483647
        0x05,                    // PadLen: 5 trailing bytes

        0x00,              // Unindexed Entry
        0x00,              // Name Len: 0
        0x03,              // Value Len: 3
        0x66, 0x6f, 0x6f,  // foo

        0x00,              // Unindexed Entry
        0x03,              // Name Len: 3
        0x66, 0x6f, 0x6f,  // foo
        0x03,              // Value Len: 3
        0x62, 0x61, 0x72,  // bar

        0x00, 0x00, 0x00, 0x00,  // Padding
        0x00,                    // Padding
    };
    // frame-format on
    SpdyHeadersIR headers_ir(/* stream_id = */ 0x7fffffff);
    headers_ir.set_fin(true);
    headers_ir.SetHeader("", "foo");
    headers_ir.SetHeader("foo", "bar");
    headers_ir.set_padding_len(6);
    SpdySerializedFrame frame(SpdyFramerPeer::SerializeHeaders(
        &framer, headers_ir, use_output_ ? &output_ : nullptr));
    CompareFrame(kDescription, frame, kH2FrameData,
                 SPDY_ARRAYSIZE(kH2FrameData));
  }
}

TEST_P(SpdyFramerTest, CreateWindowUpdate) {
  {
    const char kDescription[] = "WINDOW_UPDATE frame";
    const unsigned char kH2FrameData[] = {
        0x00, 0x00, 0x04,        // Length: 4
        0x08,                    //   Type: WINDOW_UPDATE
        0x00,                    //  Flags: none
        0x00, 0x00, 0x00, 0x01,  // Stream: 1
        0x00, 0x00, 0x00, 0x01,  // Increment: 1
    };
    SpdySerializedFrame frame(framer_.SerializeWindowUpdate(
        SpdyWindowUpdateIR(/* stream_id = */ 1, /* delta = */ 1)));
    if (use_output_) {
      output_.Reset();
      ASSERT_TRUE(framer_.SerializeWindowUpdate(
          SpdyWindowUpdateIR(/* stream_id = */ 1, /* delta = */ 1), &output_));
      frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
    }
    CompareFrame(kDescription, frame, kH2FrameData,
                 SPDY_ARRAYSIZE(kH2FrameData));
  }

  {
    const char kDescription[] = "WINDOW_UPDATE frame with max stream ID";
    const unsigned char kH2FrameData[] = {
        0x00, 0x00, 0x04,        // Length: 4
        0x08,                    //   Type: WINDOW_UPDATE
        0x00,                    //  Flags: none
        0x7f, 0xff, 0xff, 0xff,  // Stream: 0x7fffffff
        0x00, 0x00, 0x00, 0x01,  // Increment: 1
    };
    SpdySerializedFrame frame(framer_.SerializeWindowUpdate(
        SpdyWindowUpdateIR(/* stream_id = */ 0x7FFFFFFF, /* delta = */ 1)));
    if (use_output_) {
      output_.Reset();
      ASSERT_TRUE(framer_.SerializeWindowUpdate(
          SpdyWindowUpdateIR(/* stream_id = */ 0x7FFFFFFF, /* delta = */ 1),
          &output_));
      frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
    }
    CompareFrame(kDescription, frame, kH2FrameData,
                 SPDY_ARRAYSIZE(kH2FrameData));
  }

  {
    const char kDescription[] = "WINDOW_UPDATE frame with max window delta";
    const unsigned char kH2FrameData[] = {
        0x00, 0x00, 0x04,        // Length: 4
        0x08,                    //   Type: WINDOW_UPDATE
        0x00,                    //  Flags: none
        0x00, 0x00, 0x00, 0x01,  // Stream: 1
        0x7f, 0xff, 0xff, 0xff,  // Increment: 0x7fffffff
    };
    SpdySerializedFrame frame(framer_.SerializeWindowUpdate(
        SpdyWindowUpdateIR(/* stream_id = */ 1, /* delta = */ 0x7FFFFFFF)));
    if (use_output_) {
      output_.Reset();
      ASSERT_TRUE(framer_.SerializeWindowUpdate(
          SpdyWindowUpdateIR(/* stream_id = */ 1, /* delta = */ 0x7FFFFFFF),
          &output_));
      frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
    }
    CompareFrame(kDescription, frame, kH2FrameData,
                 SPDY_ARRAYSIZE(kH2FrameData));
  }
}

TEST_P(SpdyFramerTest, CreatePushPromiseUncompressed) {
  {
    // Test framing PUSH_PROMISE without padding.
    SpdyFramer framer(SpdyFramer::DISABLE_COMPRESSION);
    const char kDescription[] = "PUSH_PROMISE frame without padding";

    // frame-format off
    const unsigned char kFrameData[] = {
        0x00, 0x00, 0x16,        // Length: 22
        0x05,                    //   Type: PUSH_PROMISE
        0x04,                    //  Flags: END_HEADERS
        0x00, 0x00, 0x00, 0x29,  // Stream: 41
        0x00, 0x00, 0x00, 0x3a,  // Promise: 58

        0x00,              // Unindexed Entry
        0x03,              // Name Len: 3
        0x62, 0x61, 0x72,  // bar
        0x03,              // Value Len: 3
        0x66, 0x6f, 0x6f,  // foo

        0x00,              // Unindexed Entry
        0x03,              // Name Len: 3
        0x66, 0x6f, 0x6f,  // foo
        0x03,              // Value Len: 3
        0x62, 0x61, 0x72,  // bar
    };
    // frame-format on

    SpdyPushPromiseIR push_promise(/* stream_id = */ 41,
                                   /* promised_stream_id = */ 58);
    push_promise.SetHeader("bar", "foo");
    push_promise.SetHeader("foo", "bar");
    SpdySerializedFrame frame(SpdyFramerPeer::SerializePushPromise(
        &framer, push_promise, use_output_ ? &output_ : nullptr));
    CompareFrame(kDescription, frame, kFrameData, SPDY_ARRAYSIZE(kFrameData));
  }

  {
    // Test framing PUSH_PROMISE with one byte of padding.
    SpdyFramer framer(SpdyFramer::DISABLE_COMPRESSION);
    const char kDescription[] = "PUSH_PROMISE frame with one byte of padding";

    // frame-format off
    const unsigned char kFrameData[] = {
        0x00, 0x00, 0x17,        // Length: 23
        0x05,                    //   Type: PUSH_PROMISE
        0x0c,                    //  Flags: END_HEADERS|PADDED
        0x00, 0x00, 0x00, 0x29,  // Stream: 41
        0x00,                    // PadLen: 0 trailing bytes
        0x00, 0x00, 0x00, 0x3a,  // Promise: 58

        0x00,              // Unindexed Entry
        0x03,              // Name Len: 3
        0x62, 0x61, 0x72,  // bar
        0x03,              // Value Len: 3
        0x66, 0x6f, 0x6f,  // foo

        0x00,              // Unindexed Entry
        0x03,              // Name Len: 3
        0x66, 0x6f, 0x6f,  // foo
        0x03,              // Value Len: 3
        0x62, 0x61, 0x72,  // bar
    };
    // frame-format on

    SpdyPushPromiseIR push_promise(/* stream_id = */ 41,
                                   /* promised_stream_id = */ 58);
    push_promise.set_padding_len(1);
    push_promise.SetHeader("bar", "foo");
    push_promise.SetHeader("foo", "bar");
    output_.Reset();
    SpdySerializedFrame frame(SpdyFramerPeer::SerializePushPromise(
        &framer, push_promise, use_output_ ? &output_ : nullptr));

    CompareFrame(kDescription, frame, kFrameData, SPDY_ARRAYSIZE(kFrameData));
  }

  {
    // Test framing PUSH_PROMISE with 177 bytes of padding.
    SpdyFramer framer(SpdyFramer::DISABLE_COMPRESSION);
    const char kDescription[] = "PUSH_PROMISE frame with 177 bytes of padding";

    // frame-format off
    // clang-format off
    const unsigned char kFrameData[] = {
        0x00, 0x00, 0xc7,        // Length: 199
        0x05,                    //   Type: PUSH_PROMISE
        0x0c,                    //  Flags: END_HEADERS|PADDED
        0x00, 0x00, 0x00, 0x2a,  // Stream: 42
        0xb0,                    // PadLen: 176 trailing bytes
        0x00, 0x00, 0x00, 0x39,  // Promise: 57

        0x00,                    // Unindexed Entry
        0x03,                    // Name Len: 3
        0x62, 0x61, 0x72,        // bar
        0x03,                    // Value Len: 3
        0x66, 0x6f, 0x6f,        // foo

        0x00,                    // Unindexed Entry
        0x03,                    // Name Len: 3
        0x66, 0x6f, 0x6f,        // foo
        0x03,                    // Value Len: 3
        0x62, 0x61, 0x72,        // bar

      // Padding of 176 0x00(s).
      0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00, 0x00,  0x00,  0x00,
      0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00, 0x00,  0x00,  0x00,
      0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00, 0x00,  0x00,  0x00,
      0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00, 0x00,  0x00,  0x00,
      0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00, 0x00,  0x00,  0x00,
      0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00, 0x00,  0x00,  0x00,
      0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00, 0x00,  0x00,  0x00,
      0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00, 0x00,  0x00,  0x00,
      0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00, 0x00,  0x00,  0x00,
      0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00, 0x00,  0x00,  0x00,
      0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00, 0x00,  0x00,  0x00,
      0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00, 0x00,  0x00,  0x00,
      0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00, 0x00,  0x00,  0x00,
      0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00, 0x00,  0x00,  0x00,
      0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00, 0x00,  0x00,  0x00,
      0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00, 0x00,  0x00,  0x00,
    };
    // clang-format on
    // frame-format on

    SpdyPushPromiseIR push_promise(/* stream_id = */ 42,
                                   /* promised_stream_id = */ 57);
    push_promise.set_padding_len(177);
    push_promise.SetHeader("bar", "foo");
    push_promise.SetHeader("foo", "bar");
    output_.Reset();
    SpdySerializedFrame frame(SpdyFramerPeer::SerializePushPromise(
        &framer, push_promise, use_output_ ? &output_ : nullptr));

    CompareFrame(kDescription, frame, kFrameData, SPDY_ARRAYSIZE(kFrameData));
  }
}

// Regression test for https://crbug.com/464748.
TEST_P(SpdyFramerTest, GetNumberRequiredContinuationFrames) {
  EXPECT_EQ(1u, GetNumberRequiredContinuationFrames(16383 + 16374));
  EXPECT_EQ(2u, GetNumberRequiredContinuationFrames(16383 + 16374 + 1));
  EXPECT_EQ(2u, GetNumberRequiredContinuationFrames(16383 + 2 * 16374));
  EXPECT_EQ(3u, GetNumberRequiredContinuationFrames(16383 + 2 * 16374 + 1));
}

TEST_P(SpdyFramerTest, CreateContinuationUncompressed) {
  SpdyFramer framer(SpdyFramer::DISABLE_COMPRESSION);
  const char kDescription[] = "CONTINUATION frame";

  // frame-format off
  const unsigned char kFrameData[] = {
      0x00, 0x00, 0x12,        // Length: 18
      0x09,                    //   Type: CONTINUATION
      0x04,                    //  Flags: END_HEADERS
      0x00, 0x00, 0x00, 0x2a,  // Stream: 42

      0x00,              // Unindexed Entry
      0x03,              // Name Len: 3
      0x62, 0x61, 0x72,  // bar
      0x03,              // Value Len: 3
      0x66, 0x6f, 0x6f,  // foo

      0x00,              // Unindexed Entry
      0x03,              // Name Len: 3
      0x66, 0x6f, 0x6f,  // foo
      0x03,              // Value Len: 3
      0x62, 0x61, 0x72,  // bar
  };
  // frame-format on

  SpdyHeaderBlock header_block;
  header_block["bar"] = "foo";
  header_block["foo"] = "bar";
  auto buffer = SpdyMakeUnique<SpdyString>();
  HpackEncoder encoder(ObtainHpackHuffmanTable());
  encoder.DisableCompression();
  encoder.EncodeHeaderSet(header_block, buffer.get());

  SpdyContinuationIR continuation(/* stream_id = */ 42);
  continuation.take_encoding(std::move(buffer));
  continuation.set_end_headers(true);

  SpdySerializedFrame frame(framer.SerializeContinuation(continuation));
  if (use_output_) {
    ASSERT_TRUE(framer.SerializeContinuation(continuation, &output_));
    frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
  }
  CompareFrame(kDescription, frame, kFrameData, SPDY_ARRAYSIZE(kFrameData));
}

// Test that if we send an unexpected CONTINUATION
// we signal an error (but don't crash).
TEST_P(SpdyFramerTest, SendUnexpectedContinuation) {
  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;

  deframer_.set_visitor(&visitor);

  // frame-format off
  char kH2FrameData[] = {
      0x00, 0x00, 0x12,        // Length: 18
      0x09,                    //   Type: CONTINUATION
      0x04,                    //  Flags: END_HEADERS
      0x00, 0x00, 0x00, 0x2a,  // Stream: 42

      0x00,              // Unindexed Entry
      0x03,              // Name Len: 3
      0x62, 0x61, 0x72,  // bar
      0x03,              // Value Len: 3
      0x66, 0x6f, 0x6f,  // foo

      0x00,              // Unindexed Entry
      0x03,              // Name Len: 3
      0x66, 0x6f, 0x6f,  // foo
      0x03,              // Value Len: 3
      0x62, 0x61, 0x72,  // bar
  };
  // frame-format on

  SpdySerializedFrame frame(kH2FrameData, sizeof(kH2FrameData), false);

  // We shouldn't have to read the whole frame before we signal an error.
  EXPECT_CALL(visitor, OnError(Http2DecoderAdapter::SPDY_UNEXPECTED_FRAME));
  EXPECT_GT(frame.size(), deframer_.ProcessInput(frame.data(), frame.size()));
  EXPECT_TRUE(deframer_.HasError());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_UNEXPECTED_FRAME,
            deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             deframer_.spdy_framer_error());
}

TEST_P(SpdyFramerTest, CreatePushPromiseThenContinuationUncompressed) {
  {
    // Test framing in a case such that a PUSH_PROMISE frame, with one byte of
    // padding, cannot hold all the data payload, which is overflowed to the
    // consecutive CONTINUATION frame.
    SpdyFramer framer(SpdyFramer::DISABLE_COMPRESSION);
    const char kDescription[] =
        "PUSH_PROMISE and CONTINUATION frames with one byte of padding";

    // frame-format off
    const unsigned char kPartialPushPromiseFrameData[] = {
        0x00, 0x3f, 0xf6,        // Length: 16374
        0x05,                    //   Type: PUSH_PROMISE
        0x08,                    //  Flags: PADDED
        0x00, 0x00, 0x00, 0x2a,  // Stream: 42
        0x00,                    // PadLen: 0 trailing bytes
        0x00, 0x00, 0x00, 0x39,  // Promise: 57

        0x00,                    // Unindexed Entry
        0x03,                    // Name Len: 3
        0x78, 0x78, 0x78,        // xxx
        0x7f, 0x80, 0x7f,        // Value Len: 16361
        0x78, 0x78, 0x78, 0x78,  // xxxx
        0x78, 0x78, 0x78, 0x78,  // xxxx
        0x78, 0x78, 0x78, 0x78,  // xxxx
        0x78, 0x78, 0x78, 0x78,  // xxxx
        0x78, 0x78, 0x78, 0x78,  // xxxx
        0x78, 0x78, 0x78, 0x78,  // xxxx
        0x78, 0x78, 0x78, 0x78,  // xxxx
        0x78, 0x78, 0x78, 0x78,  // xxxx
        0x78, 0x78, 0x78, 0x78,  // xxxx
        0x78, 0x78, 0x78, 0x78,  // xxxx
        0x78, 0x78, 0x78, 0x78,  // xxxx
        0x78, 0x78, 0x78, 0x78,  // xxxx
        0x78, 0x78, 0x78, 0x78,  // xxxx
        0x78, 0x78, 0x78, 0x78,  // xxxx
        0x78, 0x78, 0x78, 0x78,  // xxxx
        0x78, 0x78, 0x78, 0x78,  // xxxx
        0x78, 0x78, 0x78, 0x78,  // xxxx
        0x78, 0x78, 0x78, 0x78,  // xxxx
        0x78, 0x78, 0x78, 0x78,  // xxxx
        0x78, 0x78, 0x78, 0x78,  // xxxx
        0x78, 0x78, 0x78, 0x78,  // xxxx
    };
    const unsigned char kContinuationFrameData[] = {
        0x00, 0x00, 0x16,        // Length: 22
        0x09,                    //   Type: CONTINUATION
        0x04,                    //  Flags: END_HEADERS
        0x00, 0x00, 0x00, 0x2a,  // Stream: 42
        0x78, 0x78, 0x78, 0x78,  // xxxx
        0x78, 0x78, 0x78, 0x78,  // xxxx
        0x78, 0x78, 0x78, 0x78,  // xxxx
        0x78, 0x78, 0x78, 0x78,  // xxxx
        0x78, 0x78, 0x78, 0x78,  // xxxx
        0x78,                    // x
    };
    // frame-format on

    SpdyPushPromiseIR push_promise(/* stream_id = */ 42,
                                   /* promised_stream_id = */ 57);
    push_promise.set_padding_len(1);
    SpdyString big_value(kHttp2MaxControlFrameSendSize, 'x');
    push_promise.SetHeader("xxx", big_value);
    SpdySerializedFrame frame(SpdyFramerPeer::SerializePushPromise(
        &framer, push_promise, use_output_ ? &output_ : nullptr));

    // The entire frame should look like below:
    // Name                     Length in Byte
    // ------------------------------------------- Begin of PUSH_PROMISE frame
    // PUSH_PROMISE header      9
    // Pad length field         1
    // Promised stream          4
    // Length field of key      2
    // Content of key           3
    // Length field of value    3
    // Part of big_value        16361
    // ------------------------------------------- Begin of CONTINUATION frame
    // CONTINUATION header      9
    // Remaining of big_value   22
    // ------------------------------------------- End

    // Length of everything listed above except big_value.
    int len_non_data_payload = 31;
    EXPECT_EQ(kHttp2MaxControlFrameSendSize + len_non_data_payload,
              frame.size());

    // Partially compare the PUSH_PROMISE frame against the template.
    const unsigned char* frame_data =
        reinterpret_cast<const unsigned char*>(frame.data());
    CompareCharArraysWithHexError(kDescription, frame_data,
                                  SPDY_ARRAYSIZE(kPartialPushPromiseFrameData),
                                  kPartialPushPromiseFrameData,
                                  SPDY_ARRAYSIZE(kPartialPushPromiseFrameData));

    // Compare the CONTINUATION frame against the template.
    frame_data += kHttp2MaxControlFrameSendSize;
    CompareCharArraysWithHexError(
        kDescription, frame_data, SPDY_ARRAYSIZE(kContinuationFrameData),
        kContinuationFrameData, SPDY_ARRAYSIZE(kContinuationFrameData));
  }
}

TEST_P(SpdyFramerTest, CreateAltSvc) {
  const char kDescription[] = "ALTSVC frame";
  const unsigned char kType = SerializeFrameType(SpdyFrameType::ALTSVC);
  const unsigned char kFrameData[] = {
      0x00, 0x00, 0x49, kType, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x06, 'o',
      'r',  'i',  'g',  'i',   'n',  'p',  'i',  'd',  '1',  '=',  '"',  'h',
      'o',  's',  't',  ':',   '4',  '4',  '3',  '"',  ';',  ' ',  'm',  'a',
      '=',  '5',  ',',  'p',   '%',  '2',  '2',  '%',  '3',  'D',  'i',  '%',
      '3',  'A',  'd',  '=',   '"',  'h',  '_',  '\\', '\\', 'o',  '\\', '"',
      's',  't',  ':',  '1',   '2',  '3',  '"',  ';',  ' ',  'm',  'a',  '=',
      '4',  '2',  ';',  ' ',   'v',  '=',  '"',  '2',  '4',  '"'};
  SpdyAltSvcIR altsvc_ir(/* stream_id = */ 3);
  altsvc_ir.set_origin("origin");
  altsvc_ir.add_altsvc(SpdyAltSvcWireFormat::AlternativeService(
      "pid1", "host", 443, 5, SpdyAltSvcWireFormat::VersionVector()));
  altsvc_ir.add_altsvc(SpdyAltSvcWireFormat::AlternativeService(
      "p\"=i:d", "h_\\o\"st", 123, 42,
      SpdyAltSvcWireFormat::VersionVector{24}));
  SpdySerializedFrame frame(framer_.SerializeFrame(altsvc_ir));
  if (use_output_) {
    EXPECT_EQ(framer_.SerializeFrame(altsvc_ir, &output_), frame.size());
    frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
  }
  CompareFrame(kDescription, frame, kFrameData, SPDY_ARRAYSIZE(kFrameData));
}

TEST_P(SpdyFramerTest, CreatePriority) {
  const char kDescription[] = "PRIORITY frame";
  const unsigned char kFrameData[] = {
      0x00, 0x00, 0x05,        // Length: 5
      0x02,                    //   Type: PRIORITY
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x02,  // Stream: 2
      0x80, 0x00, 0x00, 0x01,  // Parent: 1 (Exclusive)
      0x10,                    // Weight: 17
  };
  SpdyPriorityIR priority_ir(/* stream_id = */ 2,
                             /* parent_stream_id = */ 1,
                             /* weight = */ 17,
                             /* exclusive = */ true);
  SpdySerializedFrame frame(framer_.SerializeFrame(priority_ir));
  if (use_output_) {
    EXPECT_EQ(framer_.SerializeFrame(priority_ir, &output_), frame.size());
    frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
  }
  CompareFrame(kDescription, frame, kFrameData, SPDY_ARRAYSIZE(kFrameData));
}

TEST_P(SpdyFramerTest, CreateUnknown) {
  const char kDescription[] = "Unknown frame";
  const uint8_t kType = 0xaf;
  const uint8_t kFlags = 0x11;
  const uint8_t kLength = strlen(kDescription);
  const unsigned char kFrameData[] = {
      0x00,   0x00, kLength,        // Length: 13
      kType,                        //   Type: undefined
      kFlags,                       //  Flags: arbitrary, undefined
      0x00,   0x00, 0x00,    0x02,  // Stream: 2
      0x55,   0x6e, 0x6b,    0x6e,  // "Unkn"
      0x6f,   0x77, 0x6e,    0x20,  // "own "
      0x66,   0x72, 0x61,    0x6d,  // "fram"
      0x65,                         // "e"
  };
  SpdyUnknownIR unknown_ir(/* stream_id = */ 2,
                           /* type = */ kType,
                           /* flags = */ kFlags,
                           /* payload = */ kDescription);
  SpdySerializedFrame frame(framer_.SerializeFrame(unknown_ir));
  if (use_output_) {
    EXPECT_EQ(framer_.SerializeFrame(unknown_ir, &output_), frame.size());
    frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
  }
  CompareFrame(kDescription, frame, kFrameData, SPDY_ARRAYSIZE(kFrameData));
}

// Test serialization of a SpdyUnknownIR with a defined type, a length field
// that does not match the payload size and in fact exceeds framer limits, and a
// stream ID that effectively flips the reserved bit.
TEST_P(SpdyFramerTest, CreateUnknownUnchecked) {
  const char kDescription[] = "Unknown frame";
  const uint8_t kType = 0x00;
  const uint8_t kFlags = 0x11;
  const uint8_t kLength = std::numeric_limits<uint8_t>::max();
  const unsigned int kStreamId = kStreamIdMask + 42;
  const unsigned char kFrameData[] = {
      0x00,   0x00, kLength,        // Length: 16426
      kType,                        //   Type: DATA, defined
      kFlags,                       //  Flags: arbitrary, undefined
      0x80,   0x00, 0x00,    0x29,  // Stream: 2147483689
      0x55,   0x6e, 0x6b,    0x6e,  // "Unkn"
      0x6f,   0x77, 0x6e,    0x20,  // "own "
      0x66,   0x72, 0x61,    0x6d,  // "fram"
      0x65,                         // "e"
  };
  TestSpdyUnknownIR unknown_ir(/* stream_id = */ kStreamId,
                               /* type = */ kType,
                               /* flags = */ kFlags,
                               /* payload = */ kDescription);
  unknown_ir.set_length(kLength);
  SpdySerializedFrame frame(framer_.SerializeFrame(unknown_ir));
  if (use_output_) {
    EXPECT_EQ(framer_.SerializeFrame(unknown_ir, &output_), frame.size());
    frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
  }
  CompareFrame(kDescription, frame, kFrameData, SPDY_ARRAYSIZE(kFrameData));
}

TEST_P(SpdyFramerTest, ReadCompressedHeadersHeaderBlock) {
  SpdyHeadersIR headers_ir(/* stream_id = */ 1);
  headers_ir.SetHeader("alpha", "beta");
  headers_ir.SetHeader("gamma", "delta");
  SpdySerializedFrame control_frame(SpdyFramerPeer::SerializeHeaders(
      &framer_, headers_ir, use_output_ ? &output_ : nullptr));
  TestSpdyVisitor visitor(SpdyFramer::ENABLE_COMPRESSION);
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame.data()),
      control_frame.size());
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(0, visitor.control_frame_header_data_count_);
  EXPECT_EQ(0, visitor.zero_length_control_frame_header_data_count_);
  EXPECT_EQ(0, visitor.end_of_stream_count_);
  EXPECT_EQ(headers_ir.header_block(), visitor.headers_);
}

TEST_P(SpdyFramerTest, ReadCompressedHeadersHeaderBlockWithHalfClose) {
  SpdyHeadersIR headers_ir(/* stream_id = */ 1);
  headers_ir.set_fin(true);
  headers_ir.SetHeader("alpha", "beta");
  headers_ir.SetHeader("gamma", "delta");
  SpdySerializedFrame control_frame(SpdyFramerPeer::SerializeHeaders(
      &framer_, headers_ir, use_output_ ? &output_ : nullptr));
  TestSpdyVisitor visitor(SpdyFramer::ENABLE_COMPRESSION);
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame.data()),
      control_frame.size());
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(0, visitor.control_frame_header_data_count_);
  EXPECT_EQ(0, visitor.zero_length_control_frame_header_data_count_);
  EXPECT_EQ(1, visitor.end_of_stream_count_);
  EXPECT_EQ(headers_ir.header_block(), visitor.headers_);
}

TEST_P(SpdyFramerTest, TooLargeHeadersFrameUsesContinuation) {
  SpdyFramer framer(SpdyFramer::DISABLE_COMPRESSION);
  SpdyHeadersIR headers(/* stream_id = */ 1);
  headers.set_padding_len(256);

  // Exact payload length will change with HPACK, but this should be long
  // enough to cause an overflow.
  const size_t kBigValueSize = kHttp2MaxControlFrameSendSize;
  SpdyString big_value(kBigValueSize, 'x');
  headers.SetHeader("aa", big_value);
  SpdySerializedFrame control_frame(SpdyFramerPeer::SerializeHeaders(
      &framer, headers, use_output_ ? &output_ : nullptr));
  EXPECT_GT(control_frame.size(), kHttp2MaxControlFrameSendSize);

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame.data()),
      control_frame.size());
  EXPECT_TRUE(visitor.header_buffer_valid_);
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(1, visitor.continuation_count_);
  EXPECT_EQ(0, visitor.zero_length_control_frame_header_data_count_);
}

TEST_P(SpdyFramerTest, MultipleContinuationFramesWithIterator) {
  SpdyFramer framer(SpdyFramer::DISABLE_COMPRESSION);
  auto headers = SpdyMakeUnique<SpdyHeadersIR>(/* stream_id = */ 1);
  headers->set_padding_len(256);

  // Exact payload length will change with HPACK, but this should be long
  // enough to cause an overflow.
  const size_t kBigValueSize = kHttp2MaxControlFrameSendSize;
  SpdyString big_valuex(kBigValueSize, 'x');
  headers->SetHeader("aa", big_valuex);
  SpdyString big_valuez(kBigValueSize, 'z');
  headers->SetHeader("bb", big_valuez);

  SpdyFramer::SpdyHeaderFrameIterator frame_it(&framer, std::move(headers));

  EXPECT_TRUE(frame_it.HasNextFrame());
  EXPECT_GT(frame_it.NextFrame(&output_), 0u);
  SpdySerializedFrame headers_frame(output_.Begin(), output_.Size(), false);
  EXPECT_EQ(headers_frame.size(), kHttp2MaxControlFrameSendSize);

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(headers_frame.data()),
      headers_frame.size());
  EXPECT_TRUE(visitor.header_buffer_valid_);
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(0, visitor.continuation_count_);
  EXPECT_EQ(0, visitor.zero_length_control_frame_header_data_count_);

  output_.Reset();
  EXPECT_TRUE(frame_it.HasNextFrame());
  EXPECT_GT(frame_it.NextFrame(&output_), 0u);
  SpdySerializedFrame first_cont_frame(output_.Begin(), output_.Size(), false);
  EXPECT_EQ(first_cont_frame.size(), kHttp2MaxControlFrameSendSize);

  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(first_cont_frame.data()),
      first_cont_frame.size());
  EXPECT_TRUE(visitor.header_buffer_valid_);
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(1, visitor.continuation_count_);
  EXPECT_EQ(0, visitor.zero_length_control_frame_header_data_count_);

  output_.Reset();
  EXPECT_TRUE(frame_it.HasNextFrame());
  EXPECT_GT(frame_it.NextFrame(&output_), 0u);
  SpdySerializedFrame second_cont_frame(output_.Begin(), output_.Size(), false);
  EXPECT_LT(second_cont_frame.size(), kHttp2MaxControlFrameSendSize);

  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(second_cont_frame.data()),
      second_cont_frame.size());
  EXPECT_TRUE(visitor.header_buffer_valid_);
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(2, visitor.continuation_count_);
  EXPECT_EQ(0, visitor.zero_length_control_frame_header_data_count_);

  EXPECT_FALSE(frame_it.HasNextFrame());
}

TEST_P(SpdyFramerTest, PushPromiseFramesWithIterator) {
  SpdyFramer framer(SpdyFramer::DISABLE_COMPRESSION);
  auto push_promise =
      SpdyMakeUnique<SpdyPushPromiseIR>(/* stream_id = */ 1,
                                        /* promised_stream_id = */ 2);
  push_promise->set_padding_len(256);

  // Exact payload length will change with HPACK, but this should be long
  // enough to cause an overflow.
  const size_t kBigValueSize = kHttp2MaxControlFrameSendSize;
  SpdyString big_valuex(kBigValueSize, 'x');
  push_promise->SetHeader("aa", big_valuex);
  SpdyString big_valuez(kBigValueSize, 'z');
  push_promise->SetHeader("bb", big_valuez);

  SpdyFramer::SpdyPushPromiseFrameIterator frame_it(&framer,
                                                    std::move(push_promise));

  EXPECT_TRUE(frame_it.HasNextFrame());
  EXPECT_GT(frame_it.NextFrame(&output_), 0u);
  SpdySerializedFrame push_promise_frame(output_.Begin(), output_.Size(),
                                         false);
  EXPECT_EQ(push_promise_frame.size(), kHttp2MaxControlFrameSendSize);

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(push_promise_frame.data()),
      push_promise_frame.size());
  EXPECT_TRUE(visitor.header_buffer_valid_);
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.push_promise_frame_count_);
  EXPECT_EQ(0, visitor.continuation_count_);
  EXPECT_EQ(0, visitor.zero_length_control_frame_header_data_count_);

  EXPECT_TRUE(frame_it.HasNextFrame());
  output_.Reset();
  EXPECT_GT(frame_it.NextFrame(&output_), 0u);
  SpdySerializedFrame first_cont_frame(output_.Begin(), output_.Size(), false);

  EXPECT_EQ(first_cont_frame.size(), kHttp2MaxControlFrameSendSize);
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(first_cont_frame.data()),
      first_cont_frame.size());
  EXPECT_TRUE(visitor.header_buffer_valid_);
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.push_promise_frame_count_);
  EXPECT_EQ(1, visitor.continuation_count_);
  EXPECT_EQ(0, visitor.zero_length_control_frame_header_data_count_);

  EXPECT_TRUE(frame_it.HasNextFrame());
  output_.Reset();
  EXPECT_GT(frame_it.NextFrame(&output_), 0u);
  SpdySerializedFrame second_cont_frame(output_.Begin(), output_.Size(), false);
  EXPECT_LT(second_cont_frame.size(), kHttp2MaxControlFrameSendSize);

  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(second_cont_frame.data()),
      second_cont_frame.size());
  EXPECT_TRUE(visitor.header_buffer_valid_);
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.push_promise_frame_count_);
  EXPECT_EQ(2, visitor.continuation_count_);
  EXPECT_EQ(0, visitor.zero_length_control_frame_header_data_count_);

  EXPECT_FALSE(frame_it.HasNextFrame());
}

class SpdyControlFrameIteratorTest : public ::testing::Test {
 public:
  SpdyControlFrameIteratorTest() : output_(output_buffer, kSize) {}

  void RunTest(std::unique_ptr<SpdyFrameIR> ir) {
    SpdyFramer framer(SpdyFramer::DISABLE_COMPRESSION);
    SpdySerializedFrame frame(framer.SerializeFrame(*ir));
    std::unique_ptr<SpdyFrameSequence> it =
        SpdyFramer::CreateIterator(&framer, std::move(ir));
    EXPECT_TRUE(it->HasNextFrame());
    EXPECT_EQ(it->NextFrame(&output_), frame.size());
    EXPECT_FALSE(it->HasNextFrame());
  }

 private:
  ArrayOutputBuffer output_;
};

TEST_F(SpdyControlFrameIteratorTest, RstStreamFrameWithIterator) {
  auto ir = SpdyMakeUnique<SpdyRstStreamIR>(0, ERROR_CODE_PROTOCOL_ERROR);
  RunTest(std::move(ir));
}

TEST_F(SpdyControlFrameIteratorTest, SettingsFrameWithIterator) {
  auto ir = SpdyMakeUnique<SpdySettingsIR>();
  uint32_t kValue = 0x0a0b0c0d;
  SpdyKnownSettingsId kId = SETTINGS_INITIAL_WINDOW_SIZE;
  ir->AddSetting(kId, kValue);
  RunTest(std::move(ir));
}

TEST_F(SpdyControlFrameIteratorTest, PingFrameWithIterator) {
  const SpdyPingId kPingId = 0x123456789abcdeffULL;
  auto ir = SpdyMakeUnique<SpdyPingIR>(kPingId);
  RunTest(std::move(ir));
}

TEST_F(SpdyControlFrameIteratorTest, GoAwayFrameWithIterator) {
  auto ir = SpdyMakeUnique<SpdyGoAwayIR>(0, ERROR_CODE_NO_ERROR, "GA");
  RunTest(std::move(ir));
}

TEST_F(SpdyControlFrameIteratorTest, WindowUpdateFrameWithIterator) {
  auto ir = SpdyMakeUnique<SpdyWindowUpdateIR>(1, 1);
  RunTest(std::move(ir));
}

TEST_F(SpdyControlFrameIteratorTest, AtlSvcFrameWithIterator) {
  auto ir = SpdyMakeUnique<SpdyAltSvcIR>(3);
  ir->set_origin("origin");
  ir->add_altsvc(SpdyAltSvcWireFormat::AlternativeService(
      "pid1", "host", 443, 5, SpdyAltSvcWireFormat::VersionVector()));
  ir->add_altsvc(SpdyAltSvcWireFormat::AlternativeService(
      "p\"=i:d", "h_\\o\"st", 123, 42,
      SpdyAltSvcWireFormat::VersionVector{24}));
  RunTest(std::move(ir));
}

TEST_F(SpdyControlFrameIteratorTest, PriorityFrameWithIterator) {
  auto ir = SpdyMakeUnique<SpdyPriorityIR>(2, 1, 17, true);
  RunTest(std::move(ir));
}

TEST_P(SpdyFramerTest, TooLargePushPromiseFrameUsesContinuation) {
  SpdyFramer framer(SpdyFramer::DISABLE_COMPRESSION);
  SpdyPushPromiseIR push_promise(/* stream_id = */ 1,
                                 /* promised_stream_id = */ 2);
  push_promise.set_padding_len(256);

  // Exact payload length will change with HPACK, but this should be long
  // enough to cause an overflow.
  const size_t kBigValueSize = kHttp2MaxControlFrameSendSize;
  SpdyString big_value(kBigValueSize, 'x');
  push_promise.SetHeader("aa", big_value);
  SpdySerializedFrame control_frame(SpdyFramerPeer::SerializePushPromise(
      &framer, push_promise, use_output_ ? &output_ : nullptr));
  EXPECT_GT(control_frame.size(), kHttp2MaxControlFrameSendSize);

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame.data()),
      control_frame.size());
  EXPECT_TRUE(visitor.header_buffer_valid_);
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.push_promise_frame_count_);
  EXPECT_EQ(1, visitor.continuation_count_);
  EXPECT_EQ(0, visitor.zero_length_control_frame_header_data_count_);
}

// Check that the framer stops delivering header data chunks once the visitor
// declares it doesn't want any more. This is important to guard against
// "zip bomb" types of attacks.
TEST_P(SpdyFramerTest, ControlFrameMuchTooLarge) {
  const size_t kHeaderBufferChunks = 4;
  const size_t kHeaderBufferSize =
      kHttp2DefaultFramePayloadLimit / kHeaderBufferChunks;
  const size_t kBigValueSize = kHeaderBufferSize * 2;
  SpdyString big_value(kBigValueSize, 'x');
  SpdyFramer framer(SpdyFramer::ENABLE_COMPRESSION);
  SpdyHeadersIR headers(1);
  headers.set_fin(true);
  headers.SetHeader("aa", big_value);
  SpdySerializedFrame control_frame(SpdyFramerPeer::SerializeHeaders(
      &framer_, headers, use_output_ ? &output_ : nullptr));
  TestSpdyVisitor visitor(SpdyFramer::ENABLE_COMPRESSION);
  visitor.set_header_buffer_size(kHeaderBufferSize);
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame.data()),
      control_frame.size());
  // It's up to the visitor to ignore extraneous header data; the framer
  // won't throw an error.
  EXPECT_GT(visitor.header_bytes_received_, visitor.header_buffer_size_);
  EXPECT_EQ(1, visitor.end_of_stream_count_);
}

TEST_P(SpdyFramerTest, ControlFrameSizesAreValidated) {
  // Create a GoAway frame that has a few extra bytes at the end.
  const size_t length = 20;

  // HTTP/2 GOAWAY frames are only bound by a minimal length, since they may
  // carry opaque data. Verify that minimal length is tested.
  ASSERT_GT(kGoawayFrameMinimumSize, kFrameHeaderSize);
  const size_t less_than_min_length =
      kGoawayFrameMinimumSize - kFrameHeaderSize - 1;
  ASSERT_LE(less_than_min_length, std::numeric_limits<unsigned char>::max());
  const unsigned char kH2Len = static_cast<unsigned char>(less_than_min_length);
  const unsigned char kH2FrameData[] = {
      0x00, 0x00, kH2Len,        // Length: min length - 1
      0x07,                      //   Type: GOAWAY
      0x00,                      //  Flags: none
      0x00, 0x00, 0x00,   0x00,  // Stream: 0
      0x00, 0x00, 0x00,   0x00,  //   Last: 0
      0x00, 0x00, 0x00,          // Truncated Status Field
  };
  const size_t pad_length = length + kFrameHeaderSize - sizeof(kH2FrameData);
  SpdyString pad(pad_length, 'A');
  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);

  visitor.SimulateInFramer(kH2FrameData, sizeof(kH2FrameData));
  visitor.SimulateInFramer(reinterpret_cast<const unsigned char*>(pad.c_str()),
                           pad.length());

  EXPECT_EQ(1, visitor.error_count_);  // This generated an error.
  EXPECT_EQ(Http2DecoderAdapter::SPDY_INVALID_CONTROL_FRAME,
            visitor.deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             visitor.deframer_.spdy_framer_error());
  EXPECT_EQ(0, visitor.goaway_count_);  // Frame not parsed.
}

TEST_P(SpdyFramerTest, ReadZeroLenSettingsFrame) {
  SpdySettingsIR settings_ir;
  SpdySerializedFrame control_frame(framer_.SerializeSettings(settings_ir));
  if (use_output_) {
    ASSERT_TRUE(framer_.SerializeSettings(settings_ir, &output_));
    control_frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
  }
  SetFrameLength(&control_frame, 0);
  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame.data()), kFrameHeaderSize);
  // Zero-len settings frames are permitted as of HTTP/2.
  EXPECT_EQ(0, visitor.error_count_);
}

// Tests handling of SETTINGS frames with invalid length.
TEST_P(SpdyFramerTest, ReadBogusLenSettingsFrame) {
  SpdySettingsIR settings_ir;

  // Add settings to more than fill the frame so that we don't get a buffer
  // overflow when calling SimulateInFramer() below. These settings must be
  // distinct parameters because SpdySettingsIR has a map for settings, and
  // will collapse multiple copies of the same parameter.
  settings_ir.AddSetting(SETTINGS_INITIAL_WINDOW_SIZE, 0x00000002);
  settings_ir.AddSetting(SETTINGS_MAX_CONCURRENT_STREAMS, 0x00000002);
  SpdySerializedFrame control_frame(framer_.SerializeSettings(settings_ir));
  if (use_output_) {
    ASSERT_TRUE(framer_.SerializeSettings(settings_ir, &output_));
    control_frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
  }
  const size_t kNewLength = 8;
  SetFrameLength(&control_frame, kNewLength);
  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame.data()),
      kFrameHeaderSize + kNewLength);
  // Should generate an error, since its not possible to have a
  // settings frame of length kNewLength.
  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(Http2DecoderAdapter::SPDY_INVALID_CONTROL_FRAME_SIZE,
            visitor.deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             visitor.deframer_.spdy_framer_error());
}

// Tests handling of larger SETTINGS frames.
TEST_P(SpdyFramerTest, ReadLargeSettingsFrame) {
  SpdySettingsIR settings_ir;
  settings_ir.AddSetting(SETTINGS_HEADER_TABLE_SIZE, 5);
  settings_ir.AddSetting(SETTINGS_ENABLE_PUSH, 6);
  settings_ir.AddSetting(SETTINGS_MAX_CONCURRENT_STREAMS, 7);

  SpdySerializedFrame control_frame(framer_.SerializeSettings(settings_ir));
  if (use_output_) {
    ASSERT_TRUE(framer_.SerializeSettings(settings_ir, &output_));
    control_frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
  }

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);

  // Read all at once.
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame.data()),
      control_frame.size());
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(3, visitor.setting_count_);
  EXPECT_EQ(1, visitor.settings_ack_sent_);

  // Read data in small chunks.
  size_t framed_data = 0;
  size_t unframed_data = control_frame.size();
  size_t kReadChunkSize = 5;  // Read five bytes at a time.
  while (unframed_data > 0) {
    size_t to_read = std::min(kReadChunkSize, unframed_data);
    visitor.SimulateInFramer(
        reinterpret_cast<unsigned char*>(control_frame.data() + framed_data),
        to_read);
    unframed_data -= to_read;
    framed_data += to_read;
  }
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(3 * 2, visitor.setting_count_);
  EXPECT_EQ(2, visitor.settings_ack_sent_);
}

// Tests handling of SETTINGS frame with duplicate entries.
TEST_P(SpdyFramerTest, ReadDuplicateSettings) {
  const unsigned char kH2FrameData[] = {
      0x00, 0x00, 0x12,        // Length: 18
      0x04,                    //   Type: SETTINGS
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x00,  // Stream: 0
      0x00, 0x01,              //  Param: HEADER_TABLE_SIZE
      0x00, 0x00, 0x00, 0x02,  //  Value: 2
      0x00, 0x01,              //  Param: HEADER_TABLE_SIZE
      0x00, 0x00, 0x00, 0x03,  //  Value: 3
      0x00, 0x03,              //  Param: MAX_CONCURRENT_STREAMS
      0x00, 0x00, 0x00, 0x03,  //  Value: 3
  };

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(kH2FrameData, sizeof(kH2FrameData));

  // In HTTP/2, duplicate settings are allowed;
  // each setting replaces the previous value for that setting.
  EXPECT_EQ(3, visitor.setting_count_);
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.settings_ack_sent_);
}

// Tests handling of SETTINGS frame with a setting we don't recognize.
TEST_P(SpdyFramerTest, ReadUnknownSettingsId) {
  const unsigned char kH2FrameData[] = {
      0x00, 0x00, 0x06,        // Length: 6
      0x04,                    //   Type: SETTINGS
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x00,  // Stream: 0
      0x00, 0x10,              //  Param: 16
      0x00, 0x00, 0x00, 0x02,  //  Value: 2
  };

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(kH2FrameData, sizeof(kH2FrameData));

  // In HTTP/2, we ignore unknown settings because of extensions. However, we
  // pass the SETTINGS to the visitor, which can decide how to handle them.
  EXPECT_EQ(1, visitor.setting_count_);
  EXPECT_EQ(0, visitor.error_count_);
}

TEST_P(SpdyFramerTest, ReadKnownAndUnknownSettingsWithExtension) {
  const unsigned char kH2FrameData[] = {
      0x00, 0x00, 0x12,        // Length: 18
      0x04,                    //   Type: SETTINGS
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x00,  // Stream: 0
      0x00, 0x10,              //  Param: 16
      0x00, 0x00, 0x00, 0x02,  //  Value: 2
      0x00, 0x5f,              //  Param: 95
      0x00, 0x01, 0x00, 0x02,  //  Value: 65538
      0x00, 0x02,              //  Param: ENABLE_PUSH
      0x00, 0x00, 0x00, 0x01,  //  Value: 1
  };

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  TestExtension extension;
  visitor.set_extension_visitor(&extension);
  visitor.SimulateInFramer(kH2FrameData, sizeof(kH2FrameData));

  // In HTTP/2, we ignore unknown settings because of extensions. However, we
  // pass the SETTINGS to the visitor, which can decide how to handle them.
  EXPECT_EQ(3, visitor.setting_count_);
  EXPECT_EQ(0, visitor.error_count_);

  // The extension receives all SETTINGS, including the non-standard SETTINGS.
  EXPECT_THAT(
      extension.settings_received_,
      testing::ElementsAre(testing::Pair(16, 2), testing::Pair(95, 65538),
                           testing::Pair(2, 1)));
}

// Tests handling of SETTINGS frame with entries out of order.
TEST_P(SpdyFramerTest, ReadOutOfOrderSettings) {
  const unsigned char kH2FrameData[] = {
      0x00, 0x00, 0x12,        // Length: 18
      0x04,                    //   Type: SETTINGS
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x00,  // Stream: 0
      0x00, 0x02,              //  Param: ENABLE_PUSH
      0x00, 0x00, 0x00, 0x02,  //  Value: 2
      0x00, 0x01,              //  Param: HEADER_TABLE_SIZE
      0x00, 0x00, 0x00, 0x03,  //  Value: 3
      0x00, 0x03,              //  Param: MAX_CONCURRENT_STREAMS
      0x00, 0x00, 0x00, 0x03,  //  Value: 3
  };

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(kH2FrameData, sizeof(kH2FrameData));

  // In HTTP/2, settings are allowed in any order.
  EXPECT_EQ(3, visitor.setting_count_);
  EXPECT_EQ(0, visitor.error_count_);
}

TEST_P(SpdyFramerTest, ProcessSettingsAckFrame) {
  const unsigned char kFrameData[] = {
      0x00, 0x00, 0x00,        // Length: 0
      0x04,                    //   Type: SETTINGS
      0x01,                    //  Flags: ACK
      0x00, 0x00, 0x00, 0x00,  // Stream: 0
  };

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(kFrameData, sizeof(kFrameData));

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(0, visitor.setting_count_);
  EXPECT_EQ(1, visitor.settings_ack_received_);
}

TEST_P(SpdyFramerTest, ProcessDataFrameWithPadding) {
  const int kPaddingLen = 119;
  const char data_payload[] = "hello";

  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
  deframer_.set_visitor(&visitor);

  SpdyDataIR data_ir(/* stream_id = */ 1, data_payload);
  data_ir.set_padding_len(kPaddingLen);
  SpdySerializedFrame frame(framer_.SerializeData(data_ir));

  int bytes_consumed = 0;

  // Send the frame header.
  EXPECT_CALL(visitor,
              OnDataFrameHeader(1, kPaddingLen + strlen(data_payload), false));
  CHECK_EQ(kDataFrameMinimumSize,
           deframer_.ProcessInput(frame.data(), kDataFrameMinimumSize));
  CHECK_EQ(deframer_.state(),
           Http2DecoderAdapter::SPDY_READ_DATA_FRAME_PADDING_LENGTH);
  CHECK_EQ(deframer_.spdy_framer_error(), Http2DecoderAdapter::SPDY_NO_ERROR);
  bytes_consumed += kDataFrameMinimumSize;

  // Send the padding length field.
  EXPECT_CALL(visitor, OnStreamPadLength(1, kPaddingLen - 1));
  CHECK_EQ(1u, deframer_.ProcessInput(frame.data() + bytes_consumed, 1));
  CHECK_EQ(deframer_.state(), Http2DecoderAdapter::SPDY_FORWARD_STREAM_FRAME);
  CHECK_EQ(deframer_.spdy_framer_error(), Http2DecoderAdapter::SPDY_NO_ERROR);
  bytes_consumed += 1;

  // Send the first two bytes of the data payload, i.e., "he".
  EXPECT_CALL(visitor, OnStreamFrameData(1, _, 2));
  CHECK_EQ(2u, deframer_.ProcessInput(frame.data() + bytes_consumed, 2));
  CHECK_EQ(deframer_.state(), Http2DecoderAdapter::SPDY_FORWARD_STREAM_FRAME);
  CHECK_EQ(deframer_.spdy_framer_error(), Http2DecoderAdapter::SPDY_NO_ERROR);
  bytes_consumed += 2;

  // Send the rest three bytes of the data payload, i.e., "llo".
  EXPECT_CALL(visitor, OnStreamFrameData(1, _, 3));
  CHECK_EQ(3u, deframer_.ProcessInput(frame.data() + bytes_consumed, 3));
  CHECK_EQ(deframer_.state(), Http2DecoderAdapter::SPDY_CONSUME_PADDING);
  CHECK_EQ(deframer_.spdy_framer_error(), Http2DecoderAdapter::SPDY_NO_ERROR);
  bytes_consumed += 3;

  // Send the first 100 bytes of the padding payload.
  EXPECT_CALL(visitor, OnStreamPadding(1, 100));
  CHECK_EQ(100u, deframer_.ProcessInput(frame.data() + bytes_consumed, 100));
  CHECK_EQ(deframer_.state(), Http2DecoderAdapter::SPDY_CONSUME_PADDING);
  CHECK_EQ(deframer_.spdy_framer_error(), Http2DecoderAdapter::SPDY_NO_ERROR);
  bytes_consumed += 100;

  // Send rest of the padding payload.
  EXPECT_CALL(visitor, OnStreamPadding(1, 18));
  CHECK_EQ(18u, deframer_.ProcessInput(frame.data() + bytes_consumed, 18));
  CHECK_EQ(deframer_.state(), Http2DecoderAdapter::SPDY_READY_FOR_FRAME);
  CHECK_EQ(deframer_.spdy_framer_error(), Http2DecoderAdapter::SPDY_NO_ERROR);
}

TEST_P(SpdyFramerTest, ReadWindowUpdate) {
  SpdySerializedFrame control_frame(framer_.SerializeWindowUpdate(
      SpdyWindowUpdateIR(/* stream_id = */ 1, /* delta = */ 2)));
  if (use_output_) {
    ASSERT_TRUE(framer_.SerializeWindowUpdate(
        SpdyWindowUpdateIR(/* stream_id = */ 1, /* delta = */ 2), &output_));
    control_frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
  }
  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame.data()),
      control_frame.size());
  EXPECT_EQ(1u, visitor.last_window_update_stream_);
  EXPECT_EQ(2, visitor.last_window_update_delta_);
}

TEST_P(SpdyFramerTest, ReadCompressedPushPromise) {
  SpdyPushPromiseIR push_promise(/* stream_id = */ 42,
                                 /* promised_stream_id = */ 57);
  push_promise.SetHeader("foo", "bar");
  push_promise.SetHeader("bar", "foofoo");
  SpdySerializedFrame frame(SpdyFramerPeer::SerializePushPromise(
      &framer_, push_promise, use_output_ ? &output_ : nullptr));
  TestSpdyVisitor visitor(SpdyFramer::ENABLE_COMPRESSION);
  visitor.SimulateInFramer(reinterpret_cast<unsigned char*>(frame.data()),
                           frame.size());
  EXPECT_EQ(42u, visitor.last_push_promise_stream_);
  EXPECT_EQ(57u, visitor.last_push_promise_promised_stream_);
  EXPECT_EQ(push_promise.header_block(), visitor.headers_);
}

TEST_P(SpdyFramerTest, ReadHeadersWithContinuation) {
  // frame-format off
  const unsigned char kInput[] = {
      0x00, 0x00, 0x14,                       // Length: 20
      0x01,                                   //   Type: HEADERS
      0x08,                                   //  Flags: PADDED
      0x00, 0x00, 0x00, 0x01,                 // Stream: 1
      0x03,                                   // PadLen: 3 trailing bytes
      0x00,                                   // Unindexed Entry
      0x06,                                   // Name Len: 6
      'c',  'o',  'o',  'k',  'i', 'e',       // Name
      0x07,                                   // Value Len: 7
      'f',  'o',  'o',  '=',  'b', 'a', 'r',  // Value
      0x00, 0x00, 0x00,                       // Padding

      0x00, 0x00, 0x14,                            // Length: 20
      0x09,                                        //   Type: CONTINUATION
      0x00,                                        //  Flags: none
      0x00, 0x00, 0x00, 0x01,                      // Stream: 1
      0x00,                                        // Unindexed Entry
      0x06,                                        // Name Len: 6
      'c',  'o',  'o',  'k',  'i', 'e',            // Name
      0x08,                                        // Value Len: 7
      'b',  'a',  'z',  '=',  'b', 'i', 'n', 'g',  // Value
      0x00,                                        // Unindexed Entry
      0x06,                                        // Name Len: 6
      'c',                                         // Name (split)

      0x00, 0x00, 0x12,             // Length: 18
      0x09,                         //   Type: CONTINUATION
      0x04,                         //  Flags: END_HEADERS
      0x00, 0x00, 0x00, 0x01,       // Stream: 1
      'o',  'o',  'k',  'i',  'e',  // Name (continued)
      0x00,                         // Value Len: 0
      0x00,                         // Unindexed Entry
      0x04,                         // Name Len: 4
      'n',  'a',  'm',  'e',        // Name
      0x05,                         // Value Len: 5
      'v',  'a',  'l',  'u',  'e',  // Value
  };
  // frame-format on

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(kInput, sizeof(kInput));

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(2, visitor.continuation_count_);
  EXPECT_EQ(0, visitor.zero_length_control_frame_header_data_count_);
  EXPECT_EQ(0, visitor.end_of_stream_count_);

  EXPECT_THAT(
      visitor.headers_,
      testing::ElementsAre(testing::Pair("cookie", "foo=bar; baz=bing; "),
                           testing::Pair("name", "value")));
}

TEST_P(SpdyFramerTest, ReadHeadersWithContinuationAndFin) {
  // frame-format off
  const unsigned char kInput[] = {
      0x00, 0x00, 0x10,                       // Length: 20
      0x01,                                   //   Type: HEADERS
      0x01,                                   //  Flags: END_STREAM
      0x00, 0x00, 0x00, 0x01,                 // Stream: 1
      0x00,                                   // Unindexed Entry
      0x06,                                   // Name Len: 6
      'c',  'o',  'o',  'k',  'i', 'e',       // Name
      0x07,                                   // Value Len: 7
      'f',  'o',  'o',  '=',  'b', 'a', 'r',  // Value

      0x00, 0x00, 0x14,                            // Length: 20
      0x09,                                        //   Type: CONTINUATION
      0x00,                                        //  Flags: none
      0x00, 0x00, 0x00, 0x01,                      // Stream: 1
      0x00,                                        // Unindexed Entry
      0x06,                                        // Name Len: 6
      'c',  'o',  'o',  'k',  'i', 'e',            // Name
      0x08,                                        // Value Len: 7
      'b',  'a',  'z',  '=',  'b', 'i', 'n', 'g',  // Value
      0x00,                                        // Unindexed Entry
      0x06,                                        // Name Len: 6
      'c',                                         // Name (split)

      0x00, 0x00, 0x12,             // Length: 18
      0x09,                         //   Type: CONTINUATION
      0x04,                         //  Flags: END_HEADERS
      0x00, 0x00, 0x00, 0x01,       // Stream: 1
      'o',  'o',  'k',  'i',  'e',  // Name (continued)
      0x00,                         // Value Len: 0
      0x00,                         // Unindexed Entry
      0x04,                         // Name Len: 4
      'n',  'a',  'm',  'e',        // Name
      0x05,                         // Value Len: 5
      'v',  'a',  'l',  'u',  'e',  // Value
  };
  // frame-format on

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(kInput, sizeof(kInput));

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(2, visitor.continuation_count_);
  EXPECT_EQ(1, visitor.fin_flag_count_);
  EXPECT_EQ(0, visitor.zero_length_control_frame_header_data_count_);
  EXPECT_EQ(1, visitor.end_of_stream_count_);

  EXPECT_THAT(
      visitor.headers_,
      testing::ElementsAre(testing::Pair("cookie", "foo=bar; baz=bing; "),
                           testing::Pair("name", "value")));
}

TEST_P(SpdyFramerTest, ReadPushPromiseWithContinuation) {
  // frame-format off
  const unsigned char kInput[] = {
      0x00, 0x00, 0x17,                       // Length: 23
      0x05,                                   //   Type: PUSH_PROMISE
      0x08,                                   //  Flags: PADDED
      0x00, 0x00, 0x00, 0x01,                 // Stream: 1
      0x02,                                   // PadLen: 2 trailing bytes
      0x00, 0x00, 0x00, 0x2a,                 // Promise: 42
      0x00,                                   // Unindexed Entry
      0x06,                                   // Name Len: 6
      'c',  'o',  'o',  'k',  'i', 'e',       // Name
      0x07,                                   // Value Len: 7
      'f',  'o',  'o',  '=',  'b', 'a', 'r',  // Value
      0x00, 0x00,                             // Padding

      0x00, 0x00, 0x14,                            // Length: 20
      0x09,                                        //   Type: CONTINUATION
      0x00,                                        //  Flags: none
      0x00, 0x00, 0x00, 0x01,                      // Stream: 1
      0x00,                                        // Unindexed Entry
      0x06,                                        // Name Len: 6
      'c',  'o',  'o',  'k',  'i', 'e',            // Name
      0x08,                                        // Value Len: 7
      'b',  'a',  'z',  '=',  'b', 'i', 'n', 'g',  // Value
      0x00,                                        // Unindexed Entry
      0x06,                                        // Name Len: 6
      'c',                                         // Name (split)

      0x00, 0x00, 0x12,             // Length: 18
      0x09,                         //   Type: CONTINUATION
      0x04,                         //  Flags: END_HEADERS
      0x00, 0x00, 0x00, 0x01,       // Stream: 1
      'o',  'o',  'k',  'i',  'e',  // Name (continued)
      0x00,                         // Value Len: 0
      0x00,                         // Unindexed Entry
      0x04,                         // Name Len: 4
      'n',  'a',  'm',  'e',        // Name
      0x05,                         // Value Len: 5
      'v',  'a',  'l',  'u',  'e',  // Value
  };
  // frame-format on

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(kInput, sizeof(kInput));

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1u, visitor.last_push_promise_stream_);
  EXPECT_EQ(42u, visitor.last_push_promise_promised_stream_);
  EXPECT_EQ(2, visitor.continuation_count_);
  EXPECT_EQ(0, visitor.zero_length_control_frame_header_data_count_);
  EXPECT_EQ(0, visitor.end_of_stream_count_);

  EXPECT_THAT(
      visitor.headers_,
      testing::ElementsAre(testing::Pair("cookie", "foo=bar; baz=bing; "),
                           testing::Pair("name", "value")));
}

// Receiving an unknown frame when a continuation is expected should
// result in a SPDY_UNEXPECTED_FRAME error
TEST_P(SpdyFramerTest, ReceiveUnknownMidContinuation) {
  const unsigned char kInput[] = {
      0x00, 0x00, 0x10,        // Length: 16
      0x01,                    //   Type: HEADERS
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0x00, 0x06, 0x63, 0x6f,  // HPACK
      0x6f, 0x6b, 0x69, 0x65,  //
      0x07, 0x66, 0x6f, 0x6f,  //
      0x3d, 0x62, 0x61, 0x72,  //

      0x00, 0x00, 0x14,        // Length: 20
      0xa9,                    //   Type: UnknownFrameType(169)
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0x00, 0x06, 0x63, 0x6f,  // Payload
      0x6f, 0x6b, 0x69, 0x65,  //
      0x08, 0x62, 0x61, 0x7a,  //
      0x3d, 0x62, 0x69, 0x6e,  //
      0x67, 0x00, 0x06, 0x63,  //
  };

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  // Assume the unknown frame is allowed
  visitor.on_unknown_frame_result_ = true;
  deframer_.set_visitor(&visitor);
  visitor.SimulateInFramer(kInput, sizeof(kInput));

  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(Http2DecoderAdapter::SPDY_UNEXPECTED_FRAME,
            visitor.deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             visitor.deframer_.spdy_framer_error());
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(0, visitor.continuation_count_);
  EXPECT_EQ(0u, visitor.header_buffer_length_);
}

// Receiving an unknown frame when a continuation is expected should
// result in a SPDY_UNEXPECTED_FRAME error
TEST_P(SpdyFramerTest, ReceiveUnknownMidContinuationWithExtension) {
  const unsigned char kInput[] = {
      0x00, 0x00, 0x10,        // Length: 16
      0x01,                    //   Type: HEADERS
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0x00, 0x06, 0x63, 0x6f,  // HPACK
      0x6f, 0x6b, 0x69, 0x65,  //
      0x07, 0x66, 0x6f, 0x6f,  //
      0x3d, 0x62, 0x61, 0x72,  //

      0x00, 0x00, 0x14,        // Length: 20
      0xa9,                    //   Type: UnknownFrameType(169)
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0x00, 0x06, 0x63, 0x6f,  // Payload
      0x6f, 0x6b, 0x69, 0x65,  //
      0x08, 0x62, 0x61, 0x7a,  //
      0x3d, 0x62, 0x69, 0x6e,  //
      0x67, 0x00, 0x06, 0x63,  //
  };

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  TestExtension extension;
  visitor.set_extension_visitor(&extension);
  deframer_.set_visitor(&visitor);
  visitor.SimulateInFramer(kInput, sizeof(kInput));

  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(Http2DecoderAdapter::SPDY_UNEXPECTED_FRAME,
            visitor.deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             visitor.deframer_.spdy_framer_error());
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(0, visitor.continuation_count_);
  EXPECT_EQ(0u, visitor.header_buffer_length_);
}

TEST_P(SpdyFramerTest, ReceiveContinuationOnWrongStream) {
  const unsigned char kInput[] = {
      0x00, 0x00, 0x10,        // Length: 16
      0x01,                    //   Type: HEADERS
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0x00, 0x06, 0x63, 0x6f,  // HPACK
      0x6f, 0x6b, 0x69, 0x65,  //
      0x07, 0x66, 0x6f, 0x6f,  //
      0x3d, 0x62, 0x61, 0x72,  //

      0x00, 0x00, 0x14,        // Length: 20
      0x09,                    //   Type: CONTINUATION
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x02,  // Stream: 2
      0x00, 0x06, 0x63, 0x6f,  // HPACK
      0x6f, 0x6b, 0x69, 0x65,  //
      0x08, 0x62, 0x61, 0x7a,  //
      0x3d, 0x62, 0x69, 0x6e,  //
      0x67, 0x00, 0x06, 0x63,  //
  };

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  deframer_.set_visitor(&visitor);
  visitor.SimulateInFramer(kInput, sizeof(kInput));

  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(Http2DecoderAdapter::SPDY_UNEXPECTED_FRAME,
            visitor.deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             visitor.deframer_.spdy_framer_error());
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(0, visitor.continuation_count_);
  EXPECT_EQ(0u, visitor.header_buffer_length_);
}

TEST_P(SpdyFramerTest, ReadContinuationOutOfOrder) {
  const unsigned char kInput[] = {
      0x00, 0x00, 0x18,        // Length: 24
      0x09,                    //   Type: CONTINUATION
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0x00, 0x06, 0x63, 0x6f,  // HPACK
      0x6f, 0x6b, 0x69, 0x65,  //
      0x07, 0x66, 0x6f, 0x6f,  //
      0x3d, 0x62, 0x61, 0x72,  //
  };

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  deframer_.set_visitor(&visitor);
  visitor.SimulateInFramer(kInput, sizeof(kInput));

  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(Http2DecoderAdapter::SPDY_UNEXPECTED_FRAME,
            visitor.deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             visitor.deframer_.spdy_framer_error());
  EXPECT_EQ(0, visitor.continuation_count_);
  EXPECT_EQ(0u, visitor.header_buffer_length_);
}

TEST_P(SpdyFramerTest, ExpectContinuationReceiveData) {
  const unsigned char kInput[] = {
      0x00, 0x00, 0x10,        // Length: 16
      0x01,                    //   Type: HEADERS
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0x00, 0x06, 0x63, 0x6f,  // HPACK
      0x6f, 0x6b, 0x69, 0x65,  //
      0x07, 0x66, 0x6f, 0x6f,  //
      0x3d, 0x62, 0x61, 0x72,  //

      0x00, 0x00, 0x00,        // Length: 0
      0x00,                    //   Type: DATA
      0x01,                    //  Flags: END_STREAM
      0x00, 0x00, 0x00, 0x04,  // Stream: 4

      0xde, 0xad, 0xbe, 0xef,  // Truncated Frame Header
  };

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  deframer_.set_visitor(&visitor);
  visitor.SimulateInFramer(kInput, sizeof(kInput));

  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(Http2DecoderAdapter::SPDY_UNEXPECTED_FRAME,
            visitor.deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             visitor.deframer_.spdy_framer_error());
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(0, visitor.continuation_count_);
  EXPECT_EQ(0u, visitor.header_buffer_length_);
  EXPECT_EQ(0, visitor.data_frame_count_);
}

TEST_P(SpdyFramerTest, ExpectContinuationReceiveControlFrame) {
  const unsigned char kInput[] = {
      0x00, 0x00, 0x10,        // Length: 16
      0x01,                    //   Type: HEADERS
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0x00, 0x06, 0x63, 0x6f,  // HPACK
      0x6f, 0x6b, 0x69, 0x65,  //
      0x07, 0x66, 0x6f, 0x6f,  //
      0x3d, 0x62, 0x61, 0x72,  //

      0x00, 0x00, 0x10,        // Length: 16
      0x01,                    //   Type: HEADERS
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0x00, 0x06, 0x63, 0x6f,  // HPACK
      0x6f, 0x6b, 0x69, 0x65,  //
      0x07, 0x66, 0x6f, 0x6f,  //
      0x3d, 0x62, 0x61, 0x72,  //
  };

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  deframer_.set_visitor(&visitor);
  visitor.SimulateInFramer(kInput, sizeof(kInput));

  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(Http2DecoderAdapter::SPDY_UNEXPECTED_FRAME,
            visitor.deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             visitor.deframer_.spdy_framer_error());
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(0, visitor.continuation_count_);
  EXPECT_EQ(0u, visitor.header_buffer_length_);
  EXPECT_EQ(0, visitor.data_frame_count_);
}

TEST_P(SpdyFramerTest, ReadGarbage) {
  unsigned char garbage_frame[256];
  memset(garbage_frame, ~0, sizeof(garbage_frame));
  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(garbage_frame, sizeof(garbage_frame));
  EXPECT_EQ(1, visitor.error_count_);
}

TEST_P(SpdyFramerTest, ReadUnknownExtensionFrame) {
  // The unrecognized frame type should still have a valid length.
  const unsigned char unknown_frame[] = {
      0x00, 0x00, 0x08,        // Length: 8
      0xff,                    //   Type: UnknownFrameType(255)
      0xff,                    //  Flags: 0xff
      0xff, 0xff, 0xff, 0xff,  // Stream: 0x7fffffff (R-bit set)
      0xff, 0xff, 0xff, 0xff,  // Payload
      0xff, 0xff, 0xff, 0xff,  //
  };
  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);

  // Simulate the case where the stream id validation checks out.
  visitor.on_unknown_frame_result_ = true;
  visitor.SimulateInFramer(unknown_frame, SPDY_ARRAYSIZE(unknown_frame));
  EXPECT_EQ(0, visitor.error_count_);

  // Follow it up with a valid control frame to make sure we handle
  // subsequent frames correctly.
  SpdySettingsIR settings_ir;
  settings_ir.AddSetting(SETTINGS_HEADER_TABLE_SIZE, 10);
  SpdySerializedFrame control_frame(framer_.SerializeSettings(settings_ir));
  if (use_output_) {
    ASSERT_TRUE(framer_.SerializeSettings(settings_ir, &output_));
    control_frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
  }
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame.data()),
      control_frame.size());
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.setting_count_);
  EXPECT_EQ(1, visitor.settings_ack_sent_);
}

TEST_P(SpdyFramerTest, ReadUnknownExtensionFrameWithExtension) {
  // The unrecognized frame type should still have a valid length.
  const unsigned char unknown_frame[] = {
      0x00, 0x00, 0x14,        // Length: 20
      0xff,                    //   Type: UnknownFrameType(255)
      0xff,                    //  Flags: 0xff
      0xff, 0xff, 0xff, 0xff,  // Stream: 0x7fffffff (R-bit set)
      0xff, 0xff, 0xff, 0xff,  // Payload
      0xff, 0xff, 0xff, 0xff,  //
      0xff, 0xff, 0xff, 0xff,  //
      0xff, 0xff, 0xff, 0xff,  //
      0xff, 0xff, 0xff, 0xff,  //
  };
  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  TestExtension extension;
  visitor.set_extension_visitor(&extension);
  visitor.SimulateInFramer(unknown_frame, SPDY_ARRAYSIZE(unknown_frame));
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(0x7fffffffu, extension.stream_id_);
  EXPECT_EQ(20u, extension.length_);
  EXPECT_EQ(255, extension.type_);
  EXPECT_EQ(0xff, extension.flags_);
  EXPECT_EQ(SpdyString(20, '\xff'), extension.payload_);

  // Follow it up with a valid control frame to make sure we handle
  // subsequent frames correctly.
  SpdySettingsIR settings_ir;
  settings_ir.AddSetting(SETTINGS_HEADER_TABLE_SIZE, 10);
  SpdySerializedFrame control_frame(framer_.SerializeSettings(settings_ir));
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame.data()),
      control_frame.size());
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.setting_count_);
  EXPECT_EQ(1, visitor.settings_ack_sent_);
}

TEST_P(SpdyFramerTest, ReadGarbageWithValidLength) {
  const unsigned char kFrameData[] = {
      0x00, 0x00, 0x08,        // Length: 8
      0xff,                    //   Type: UnknownFrameType(255)
      0xff,                    //  Flags: 0xff
      0xff, 0xff, 0xff, 0xff,  // Stream: 0x7fffffff (R-bit set)
      0xff, 0xff, 0xff, 0xff,  // Payload
      0xff, 0xff, 0xff, 0xff,  //
  };
  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(kFrameData, SPDY_ARRAYSIZE(kFrameData));
  EXPECT_EQ(1, visitor.error_count_);
}

TEST_P(SpdyFramerTest, ReadGarbageHPACKEncoding) {
  const unsigned char kInput[] = {
      0x00, 0x12, 0x01,        // Length: 4609
      0x04,                    //   Type: SETTINGS
      0x00,                    //  Flags: none
      0x00, 0x00, 0x01, 0xef,  // Stream: 495
      0xef, 0xff,              //  Param: 61439
      0xff, 0xff, 0xff, 0xff,  //  Value: 4294967295
      0xff, 0xff,              //  Param: 0xffff
      0xff, 0xff, 0xff, 0xff,  //  Value: 4294967295
      0xff, 0xff, 0xff, 0xff,  // Settings (Truncated)
      0xff,                    //
  };

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(kInput, SPDY_ARRAYSIZE(kInput));
  EXPECT_EQ(1, visitor.error_count_);
}

TEST_P(SpdyFramerTest, SizesTest) {
  EXPECT_EQ(9u, kFrameHeaderSize);
  EXPECT_EQ(9u, kDataFrameMinimumSize);
  EXPECT_EQ(9u, kHeadersFrameMinimumSize);
  EXPECT_EQ(14u, kPriorityFrameSize);
  EXPECT_EQ(13u, kRstStreamFrameSize);
  EXPECT_EQ(9u, kSettingsFrameMinimumSize);
  EXPECT_EQ(13u, kPushPromiseFrameMinimumSize);
  EXPECT_EQ(17u, kPingFrameSize);
  EXPECT_EQ(17u, kGoawayFrameMinimumSize);
  EXPECT_EQ(13u, kWindowUpdateFrameSize);
  EXPECT_EQ(9u, kContinuationFrameMinimumSize);
  EXPECT_EQ(11u, kGetAltSvcFrameMinimumSize);
  EXPECT_EQ(9u, kFrameMinimumSize);

  EXPECT_EQ(16384u, kHttp2DefaultFramePayloadLimit);
  EXPECT_EQ(16393u, kHttp2DefaultFrameSizeLimit);
}

TEST_P(SpdyFramerTest, StateToStringTest) {
  EXPECT_STREQ("ERROR", Http2DecoderAdapter::StateToString(
                            Http2DecoderAdapter::SPDY_ERROR));
  EXPECT_STREQ("FRAME_COMPLETE", Http2DecoderAdapter::StateToString(
                                     Http2DecoderAdapter::SPDY_FRAME_COMPLETE));
  EXPECT_STREQ("READY_FOR_FRAME",
               Http2DecoderAdapter::StateToString(
                   Http2DecoderAdapter::SPDY_READY_FOR_FRAME));
  EXPECT_STREQ("READING_COMMON_HEADER",
               Http2DecoderAdapter::StateToString(
                   Http2DecoderAdapter::SPDY_READING_COMMON_HEADER));
  EXPECT_STREQ("CONTROL_FRAME_PAYLOAD",
               Http2DecoderAdapter::StateToString(
                   Http2DecoderAdapter::SPDY_CONTROL_FRAME_PAYLOAD));
  EXPECT_STREQ("IGNORE_REMAINING_PAYLOAD",
               Http2DecoderAdapter::StateToString(
                   Http2DecoderAdapter::SPDY_IGNORE_REMAINING_PAYLOAD));
  EXPECT_STREQ("FORWARD_STREAM_FRAME",
               Http2DecoderAdapter::StateToString(
                   Http2DecoderAdapter::SPDY_FORWARD_STREAM_FRAME));
  EXPECT_STREQ(
      "SPDY_CONTROL_FRAME_BEFORE_HEADER_BLOCK",
      Http2DecoderAdapter::StateToString(
          Http2DecoderAdapter::SPDY_CONTROL_FRAME_BEFORE_HEADER_BLOCK));
  EXPECT_STREQ("SPDY_CONTROL_FRAME_HEADER_BLOCK",
               Http2DecoderAdapter::StateToString(
                   Http2DecoderAdapter::SPDY_CONTROL_FRAME_HEADER_BLOCK));
  EXPECT_STREQ("SPDY_SETTINGS_FRAME_PAYLOAD",
               Http2DecoderAdapter::StateToString(
                   Http2DecoderAdapter::SPDY_SETTINGS_FRAME_PAYLOAD));
  EXPECT_STREQ("SPDY_ALTSVC_FRAME_PAYLOAD",
               Http2DecoderAdapter::StateToString(
                   Http2DecoderAdapter::SPDY_ALTSVC_FRAME_PAYLOAD));
  EXPECT_STREQ("UNKNOWN_STATE",
               Http2DecoderAdapter::StateToString(
                   Http2DecoderAdapter::SPDY_ALTSVC_FRAME_PAYLOAD + 1));
}

TEST_P(SpdyFramerTest, SpdyFramerErrorToStringTest) {
  EXPECT_STREQ("NO_ERROR", Http2DecoderAdapter::SpdyFramerErrorToString(
                               Http2DecoderAdapter::SPDY_NO_ERROR));
  EXPECT_STREQ("INVALID_STREAM_ID",
               Http2DecoderAdapter::SpdyFramerErrorToString(
                   Http2DecoderAdapter::SPDY_INVALID_STREAM_ID));
  EXPECT_STREQ("INVALID_CONTROL_FRAME",
               Http2DecoderAdapter::SpdyFramerErrorToString(
                   Http2DecoderAdapter::SPDY_INVALID_CONTROL_FRAME));
  EXPECT_STREQ("CONTROL_PAYLOAD_TOO_LARGE",
               Http2DecoderAdapter::SpdyFramerErrorToString(
                   Http2DecoderAdapter::SPDY_CONTROL_PAYLOAD_TOO_LARGE));
  EXPECT_STREQ("ZLIB_INIT_FAILURE",
               Http2DecoderAdapter::SpdyFramerErrorToString(
                   Http2DecoderAdapter::SPDY_ZLIB_INIT_FAILURE));
  EXPECT_STREQ("UNSUPPORTED_VERSION",
               Http2DecoderAdapter::SpdyFramerErrorToString(
                   Http2DecoderAdapter::SPDY_UNSUPPORTED_VERSION));
  EXPECT_STREQ("DECOMPRESS_FAILURE",
               Http2DecoderAdapter::SpdyFramerErrorToString(
                   Http2DecoderAdapter::SPDY_DECOMPRESS_FAILURE));
  EXPECT_STREQ("COMPRESS_FAILURE",
               Http2DecoderAdapter::SpdyFramerErrorToString(
                   Http2DecoderAdapter::SPDY_COMPRESS_FAILURE));
  EXPECT_STREQ("GOAWAY_FRAME_CORRUPT",
               Http2DecoderAdapter::SpdyFramerErrorToString(
                   Http2DecoderAdapter::SPDY_GOAWAY_FRAME_CORRUPT));
  EXPECT_STREQ("RST_STREAM_FRAME_CORRUPT",
               Http2DecoderAdapter::SpdyFramerErrorToString(
                   Http2DecoderAdapter::SPDY_RST_STREAM_FRAME_CORRUPT));
  EXPECT_STREQ("INVALID_PADDING",
               Http2DecoderAdapter::SpdyFramerErrorToString(
                   Http2DecoderAdapter::SPDY_INVALID_PADDING));
  EXPECT_STREQ("INVALID_DATA_FRAME_FLAGS",
               Http2DecoderAdapter::SpdyFramerErrorToString(
                   Http2DecoderAdapter::SPDY_INVALID_DATA_FRAME_FLAGS));
  EXPECT_STREQ("INVALID_CONTROL_FRAME_FLAGS",
               Http2DecoderAdapter::SpdyFramerErrorToString(
                   Http2DecoderAdapter::SPDY_INVALID_CONTROL_FRAME_FLAGS));
  EXPECT_STREQ("UNEXPECTED_FRAME",
               Http2DecoderAdapter::SpdyFramerErrorToString(
                   Http2DecoderAdapter::SPDY_UNEXPECTED_FRAME));
  EXPECT_STREQ("INTERNAL_FRAMER_ERROR",
               Http2DecoderAdapter::SpdyFramerErrorToString(
                   Http2DecoderAdapter::SPDY_INTERNAL_FRAMER_ERROR));
  EXPECT_STREQ("INVALID_CONTROL_FRAME_SIZE",
               Http2DecoderAdapter::SpdyFramerErrorToString(
                   Http2DecoderAdapter::SPDY_INVALID_CONTROL_FRAME_SIZE));
  EXPECT_STREQ("OVERSIZED_PAYLOAD",
               Http2DecoderAdapter::SpdyFramerErrorToString(
                   Http2DecoderAdapter::SPDY_OVERSIZED_PAYLOAD));
  EXPECT_STREQ("UNKNOWN_ERROR", Http2DecoderAdapter::SpdyFramerErrorToString(
                                    Http2DecoderAdapter::LAST_ERROR));
  EXPECT_STREQ("UNKNOWN_ERROR",
               Http2DecoderAdapter::SpdyFramerErrorToString(
                   static_cast<Http2DecoderAdapter::SpdyFramerError>(
                       Http2DecoderAdapter::LAST_ERROR + 1)));
}

TEST_P(SpdyFramerTest, DataFrameFlagsV4) {
  uint8_t valid_data_flags = DATA_FLAG_FIN | DATA_FLAG_PADDED;

  uint8_t flags = 0;
  do {
    SCOPED_TRACE(testing::Message()
                 << "Flags " << flags << std::hex << static_cast<int>(flags));

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;

    deframer_.set_visitor(&visitor);

    SpdyDataIR data_ir(/* stream_id = */ 1, "hello");
    SpdySerializedFrame frame(framer_.SerializeData(data_ir));
    SetFrameFlags(&frame, flags);

    if (flags & ~valid_data_flags) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnDataFrameHeader(1, 5, flags & DATA_FLAG_FIN));
      if (flags & DATA_FLAG_PADDED) {
        // The first byte of payload is parsed as padding length, but 'h'
        // (0x68) is too large a padding length for a 5 byte payload.
        EXPECT_CALL(visitor, OnStreamPadding(_, 1));
        // Expect Error since the frame ends prematurely.
        EXPECT_CALL(visitor, OnError(_));
      } else {
        EXPECT_CALL(visitor, OnStreamFrameData(_, _, 5));
        if (flags & DATA_FLAG_FIN) {
          EXPECT_CALL(visitor, OnStreamEnd(_));
        }
      }
    }

    deframer_.ProcessInput(frame.data(), frame.size());
    if (flags & ~valid_data_flags) {
      EXPECT_EQ(Http2DecoderAdapter::SPDY_ERROR, deframer_.state());
      EXPECT_EQ(Http2DecoderAdapter::SPDY_INVALID_DATA_FRAME_FLAGS,
                deframer_.spdy_framer_error())
          << Http2DecoderAdapter::SpdyFramerErrorToString(
                 deframer_.spdy_framer_error());
    } else if (flags & DATA_FLAG_PADDED) {
      EXPECT_EQ(Http2DecoderAdapter::SPDY_ERROR, deframer_.state());
      EXPECT_EQ(Http2DecoderAdapter::SPDY_INVALID_PADDING,
                deframer_.spdy_framer_error())
          << Http2DecoderAdapter::SpdyFramerErrorToString(
                 deframer_.spdy_framer_error());
    } else {
      EXPECT_EQ(Http2DecoderAdapter::SPDY_READY_FOR_FRAME, deframer_.state());
      EXPECT_EQ(Http2DecoderAdapter::SPDY_NO_ERROR,
                deframer_.spdy_framer_error())
          << Http2DecoderAdapter::SpdyFramerErrorToString(
                 deframer_.spdy_framer_error());
    }
    deframer_.Reset();
  } while (++flags != 0);
}

TEST_P(SpdyFramerTest, RstStreamFrameFlags) {
  uint8_t flags = 0;
  do {
    SCOPED_TRACE(testing::Message()
                 << "Flags " << flags << std::hex << static_cast<int>(flags));

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    deframer_.set_visitor(&visitor);

    SpdyRstStreamIR rst_stream(/* stream_id = */ 13, ERROR_CODE_CANCEL);
    SpdySerializedFrame frame(framer_.SerializeRstStream(rst_stream));
    if (use_output_) {
      output_.Reset();
      ASSERT_TRUE(framer_.SerializeRstStream(rst_stream, &output_));
      frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
    }
    SetFrameFlags(&frame, flags);

    EXPECT_CALL(visitor, OnRstStream(13, ERROR_CODE_CANCEL));

    deframer_.ProcessInput(frame.data(), frame.size());
    EXPECT_EQ(Http2DecoderAdapter::SPDY_READY_FOR_FRAME, deframer_.state());
    EXPECT_EQ(Http2DecoderAdapter::SPDY_NO_ERROR, deframer_.spdy_framer_error())
        << Http2DecoderAdapter::SpdyFramerErrorToString(
               deframer_.spdy_framer_error());
    deframer_.Reset();
  } while (++flags != 0);
}

TEST_P(SpdyFramerTest, SettingsFrameFlags) {
  uint8_t flags = 0;
  do {
    SCOPED_TRACE(testing::Message()
                 << "Flags " << flags << std::hex << static_cast<int>(flags));

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    deframer_.set_visitor(&visitor);

    SpdySettingsIR settings_ir;
    settings_ir.AddSetting(SETTINGS_INITIAL_WINDOW_SIZE, 16);
    SpdySerializedFrame frame(framer_.SerializeSettings(settings_ir));
    if (use_output_) {
      output_.Reset();
      ASSERT_TRUE(framer_.SerializeSettings(settings_ir, &output_));
      frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
    }
    SetFrameFlags(&frame, flags);

    if (flags & SETTINGS_FLAG_ACK) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnSettings());
      EXPECT_CALL(visitor, OnSetting(SETTINGS_INITIAL_WINDOW_SIZE, 16));
      EXPECT_CALL(visitor, OnSettingsEnd());
    }

    deframer_.ProcessInput(frame.data(), frame.size());
    if (flags & SETTINGS_FLAG_ACK) {
      // The frame is invalid because ACK frames should have no payload.
      EXPECT_EQ(Http2DecoderAdapter::SPDY_ERROR, deframer_.state());
      EXPECT_EQ(Http2DecoderAdapter::SPDY_INVALID_CONTROL_FRAME_SIZE,
                deframer_.spdy_framer_error())
          << Http2DecoderAdapter::SpdyFramerErrorToString(
                 deframer_.spdy_framer_error());
    } else {
      EXPECT_EQ(Http2DecoderAdapter::SPDY_READY_FOR_FRAME, deframer_.state());
      EXPECT_EQ(Http2DecoderAdapter::SPDY_NO_ERROR,
                deframer_.spdy_framer_error())
          << Http2DecoderAdapter::SpdyFramerErrorToString(
                 deframer_.spdy_framer_error());
    }
    deframer_.Reset();
  } while (++flags != 0);
}

TEST_P(SpdyFramerTest, GoawayFrameFlags) {
  uint8_t flags = 0;
  do {
    SCOPED_TRACE(testing::Message()
                 << "Flags " << flags << std::hex << static_cast<int>(flags));

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;

    deframer_.set_visitor(&visitor);

    SpdyGoAwayIR goaway_ir(/* last_good_stream_id = */ 97, ERROR_CODE_NO_ERROR,
                           "test");
    SpdySerializedFrame frame(framer_.SerializeGoAway(goaway_ir));
    if (use_output_) {
      output_.Reset();
      ASSERT_TRUE(framer_.SerializeGoAway(goaway_ir, &output_));
      frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
    }
    SetFrameFlags(&frame, flags);

    EXPECT_CALL(visitor, OnGoAway(97, ERROR_CODE_NO_ERROR));

    deframer_.ProcessInput(frame.data(), frame.size());
    EXPECT_EQ(Http2DecoderAdapter::SPDY_READY_FOR_FRAME, deframer_.state());
    EXPECT_EQ(Http2DecoderAdapter::SPDY_NO_ERROR, deframer_.spdy_framer_error())
        << Http2DecoderAdapter::SpdyFramerErrorToString(
               deframer_.spdy_framer_error());
    deframer_.Reset();
  } while (++flags != 0);
}

TEST_P(SpdyFramerTest, HeadersFrameFlags) {
  uint8_t flags = 0;
  do {
    SCOPED_TRACE(testing::Message()
                 << "Flags " << flags << std::hex << static_cast<int>(flags));

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    SpdyFramer framer(SpdyFramer::ENABLE_COMPRESSION);
    Http2DecoderAdapter deframer;
    deframer.set_visitor(&visitor);

    SpdyHeadersIR headers_ir(/* stream_id = */ 57);
    if (flags & HEADERS_FLAG_PRIORITY) {
      headers_ir.set_weight(3);
      headers_ir.set_has_priority(true);
      headers_ir.set_parent_stream_id(5);
      headers_ir.set_exclusive(true);
    }
    headers_ir.SetHeader("foo", "bar");
    SpdySerializedFrame frame(SpdyFramerPeer::SerializeHeaders(
        &framer, headers_ir, use_output_ ? &output_ : nullptr));
    uint8_t set_flags = flags & ~HEADERS_FLAG_PADDED;
    SetFrameFlags(&frame, set_flags);

    // Expected callback values
    SpdyStreamId stream_id = 57;
    bool has_priority = false;
    int weight = 0;
    SpdyStreamId parent_stream_id = 0;
    bool exclusive = false;
    bool fin = flags & CONTROL_FLAG_FIN;
    bool end = flags & HEADERS_FLAG_END_HEADERS;
    if (flags & HEADERS_FLAG_PRIORITY) {
      has_priority = true;
      weight = 3;
      parent_stream_id = 5;
      exclusive = true;
    }
    EXPECT_CALL(visitor, OnHeaders(stream_id, has_priority, weight,
                                   parent_stream_id, exclusive, fin, end));
    EXPECT_CALL(visitor, OnHeaderFrameStart(57)).Times(1);
    if (end) {
      EXPECT_CALL(visitor, OnHeaderFrameEnd(57)).Times(1);
    }
    if (flags & DATA_FLAG_FIN && end) {
      EXPECT_CALL(visitor, OnStreamEnd(_));
    } else {
      // Do not close the stream if we are expecting a CONTINUATION frame.
      EXPECT_CALL(visitor, OnStreamEnd(_)).Times(0);
    }

    deframer.ProcessInput(frame.data(), frame.size());
    EXPECT_EQ(Http2DecoderAdapter::SPDY_READY_FOR_FRAME, deframer.state());
    EXPECT_EQ(Http2DecoderAdapter::SPDY_NO_ERROR, deframer.spdy_framer_error())
        << Http2DecoderAdapter::SpdyFramerErrorToString(
               deframer.spdy_framer_error());
    deframer.Reset();
  } while (++flags != 0);
}

TEST_P(SpdyFramerTest, PingFrameFlags) {
  uint8_t flags = 0;
  do {
    SCOPED_TRACE(testing::Message()
                 << "Flags " << flags << std::hex << static_cast<int>(flags));

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    deframer_.set_visitor(&visitor);

    SpdySerializedFrame frame(framer_.SerializePing(SpdyPingIR(42)));
    SetFrameFlags(&frame, flags);

    EXPECT_CALL(visitor, OnPing(42, flags & PING_FLAG_ACK));

    deframer_.ProcessInput(frame.data(), frame.size());
    EXPECT_EQ(Http2DecoderAdapter::SPDY_READY_FOR_FRAME, deframer_.state());
    EXPECT_EQ(Http2DecoderAdapter::SPDY_NO_ERROR, deframer_.spdy_framer_error())
        << Http2DecoderAdapter::SpdyFramerErrorToString(
               deframer_.spdy_framer_error());
    deframer_.Reset();
  } while (++flags != 0);
}

TEST_P(SpdyFramerTest, WindowUpdateFrameFlags) {
  uint8_t flags = 0;
  do {
    SCOPED_TRACE(testing::Message()
                 << "Flags " << flags << std::hex << static_cast<int>(flags));

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;

    deframer_.set_visitor(&visitor);

    SpdySerializedFrame frame(framer_.SerializeWindowUpdate(
        SpdyWindowUpdateIR(/* stream_id = */ 4, /* delta = */ 1024)));
    SetFrameFlags(&frame, flags);

    EXPECT_CALL(visitor, OnWindowUpdate(4, 1024));

    deframer_.ProcessInput(frame.data(), frame.size());
    EXPECT_EQ(Http2DecoderAdapter::SPDY_READY_FOR_FRAME, deframer_.state());
    EXPECT_EQ(Http2DecoderAdapter::SPDY_NO_ERROR, deframer_.spdy_framer_error())
        << Http2DecoderAdapter::SpdyFramerErrorToString(
               deframer_.spdy_framer_error());
    deframer_.Reset();
  } while (++flags != 0);
}

TEST_P(SpdyFramerTest, PushPromiseFrameFlags) {
  const SpdyStreamId client_id = 123;   // Must be odd.
  const SpdyStreamId promised_id = 22;  // Must be even.
  uint8_t flags = 0;
  do {
    SCOPED_TRACE(testing::Message()
                 << "Flags " << flags << std::hex << static_cast<int>(flags));

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    testing::StrictMock<test::MockDebugVisitor> debug_visitor;
    SpdyFramer framer(SpdyFramer::ENABLE_COMPRESSION);
    Http2DecoderAdapter deframer;
    deframer.set_visitor(&visitor);
    deframer.set_debug_visitor(&debug_visitor);
    framer.set_debug_visitor(&debug_visitor);

    EXPECT_CALL(
        debug_visitor,
        OnSendCompressedFrame(client_id, SpdyFrameType::PUSH_PROMISE, _, _));

    SpdyPushPromiseIR push_promise(client_id, promised_id);
    push_promise.SetHeader("foo", "bar");
    SpdySerializedFrame frame(SpdyFramerPeer::SerializePushPromise(
        &framer, push_promise, use_output_ ? &output_ : nullptr));
    // TODO(jgraettinger): Add padding to SpdyPushPromiseIR,
    // and implement framing.
    SetFrameFlags(&frame, flags & ~HEADERS_FLAG_PADDED);

    bool end = flags & PUSH_PROMISE_FLAG_END_PUSH_PROMISE;
    EXPECT_CALL(debug_visitor, OnReceiveCompressedFrame(
                                   client_id, SpdyFrameType::PUSH_PROMISE, _));
    EXPECT_CALL(visitor, OnPushPromise(client_id, promised_id, end));
    EXPECT_CALL(visitor, OnHeaderFrameStart(client_id)).Times(1);
    if (end) {
      EXPECT_CALL(visitor, OnHeaderFrameEnd(client_id)).Times(1);
    }

    deframer.ProcessInput(frame.data(), frame.size());
    EXPECT_EQ(Http2DecoderAdapter::SPDY_READY_FOR_FRAME, deframer.state());
    EXPECT_EQ(Http2DecoderAdapter::SPDY_NO_ERROR, deframer.spdy_framer_error())
        << Http2DecoderAdapter::SpdyFramerErrorToString(
               deframer.spdy_framer_error());
  } while (++flags != 0);
}

TEST_P(SpdyFramerTest, ContinuationFrameFlags) {
  uint8_t flags = 0;
  do {
    if (use_output_) {
      output_.Reset();
    }
    SCOPED_TRACE(testing::Message()
                 << "Flags " << flags << std::hex << static_cast<int>(flags));

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    testing::StrictMock<test::MockDebugVisitor> debug_visitor;
    SpdyFramer framer(SpdyFramer::ENABLE_COMPRESSION);
    Http2DecoderAdapter deframer;
    deframer.set_visitor(&visitor);
    deframer.set_debug_visitor(&debug_visitor);
    framer.set_debug_visitor(&debug_visitor);

    EXPECT_CALL(debug_visitor,
                OnSendCompressedFrame(42, SpdyFrameType::HEADERS, _, _));
    EXPECT_CALL(debug_visitor,
                OnReceiveCompressedFrame(42, SpdyFrameType::HEADERS, _));
    EXPECT_CALL(visitor, OnHeaders(42, false, 0, 0, false, false, false));
    EXPECT_CALL(visitor, OnHeaderFrameStart(42)).Times(1);

    SpdyHeadersIR headers_ir(/* stream_id = */ 42);
    headers_ir.SetHeader("foo", "bar");
    SpdySerializedFrame frame0;
    if (use_output_) {
      EXPECT_TRUE(framer.SerializeHeaders(headers_ir, &output_));
      frame0 = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
    } else {
      frame0 = framer.SerializeHeaders(headers_ir);
    }
    SetFrameFlags(&frame0, 0);

    SpdyContinuationIR continuation(/* stream_id = */ 42);
    SpdySerializedFrame frame1;
    if (use_output_) {
      char* begin = output_.Begin() + output_.Size();
      ASSERT_TRUE(framer.SerializeContinuation(continuation, &output_));
      frame1 =
          SpdySerializedFrame(begin, output_.Size() - frame0.size(), false);
    } else {
      frame1 = framer.SerializeContinuation(continuation);
    }
    SetFrameFlags(&frame1, flags);

    EXPECT_CALL(debug_visitor,
                OnReceiveCompressedFrame(42, SpdyFrameType::CONTINUATION, _));
    EXPECT_CALL(visitor, OnContinuation(42, flags & HEADERS_FLAG_END_HEADERS));
    bool end = flags & HEADERS_FLAG_END_HEADERS;
    if (end) {
      EXPECT_CALL(visitor, OnHeaderFrameEnd(42)).Times(1);
    }

    deframer.ProcessInput(frame0.data(), frame0.size());
    deframer.ProcessInput(frame1.data(), frame1.size());
    EXPECT_EQ(Http2DecoderAdapter::SPDY_READY_FOR_FRAME, deframer.state());
    EXPECT_EQ(Http2DecoderAdapter::SPDY_NO_ERROR, deframer.spdy_framer_error())
        << Http2DecoderAdapter::SpdyFramerErrorToString(
               deframer.spdy_framer_error());
  } while (++flags != 0);
}

// TODO(mlavan): Add TEST_P(SpdyFramerTest, AltSvcFrameFlags)

// Test handling of a RST_STREAM with out-of-bounds status codes.
TEST_P(SpdyFramerTest, RstStreamStatusBounds) {
  const unsigned char kH2RstStreamInvalid[] = {
      0x00, 0x00, 0x04,        // Length: 4
      0x03,                    //   Type: RST_STREAM
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0x00, 0x00, 0x00, 0x00,  //  Error: NO_ERROR
  };
  const unsigned char kH2RstStreamNumStatusCodes[] = {
      0x00, 0x00, 0x04,        // Length: 4
      0x03,                    //   Type: RST_STREAM
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x01,  // Stream: 1
      0x00, 0x00, 0x00, 0xff,  //  Error: 255
  };

  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
  deframer_.set_visitor(&visitor);

  EXPECT_CALL(visitor, OnRstStream(1, ERROR_CODE_NO_ERROR));
  deframer_.ProcessInput(reinterpret_cast<const char*>(kH2RstStreamInvalid),
                         SPDY_ARRAYSIZE(kH2RstStreamInvalid));
  EXPECT_EQ(Http2DecoderAdapter::SPDY_READY_FOR_FRAME, deframer_.state());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_NO_ERROR, deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             deframer_.spdy_framer_error());
  deframer_.Reset();

  EXPECT_CALL(visitor, OnRstStream(1, ERROR_CODE_INTERNAL_ERROR));
  deframer_.ProcessInput(
      reinterpret_cast<const char*>(kH2RstStreamNumStatusCodes),
      SPDY_ARRAYSIZE(kH2RstStreamNumStatusCodes));
  EXPECT_EQ(Http2DecoderAdapter::SPDY_READY_FOR_FRAME, deframer_.state());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_NO_ERROR, deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             deframer_.spdy_framer_error());
}

// Test handling of GOAWAY frames with out-of-bounds status code.
TEST_P(SpdyFramerTest, GoAwayStatusBounds) {
  const unsigned char kH2FrameData[] = {
      0x00, 0x00, 0x0a,        // Length: 10
      0x07,                    //   Type: GOAWAY
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x00,  // Stream: 0
      0x00, 0x00, 0x00, 0x01,  //   Last: 1
      0xff, 0xff, 0xff, 0xff,  //  Error: 0xffffffff
      0x47, 0x41,              // Description
  };
  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
  deframer_.set_visitor(&visitor);

  EXPECT_CALL(visitor, OnGoAway(1, ERROR_CODE_INTERNAL_ERROR));
  deframer_.ProcessInput(reinterpret_cast<const char*>(kH2FrameData),
                         SPDY_ARRAYSIZE(kH2FrameData));
  EXPECT_EQ(Http2DecoderAdapter::SPDY_READY_FOR_FRAME, deframer_.state());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_NO_ERROR, deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             deframer_.spdy_framer_error());
}

// Tests handling of a GOAWAY frame with out-of-bounds stream ID.
TEST_P(SpdyFramerTest, GoAwayStreamIdBounds) {
  const unsigned char kH2FrameData[] = {
      0x00, 0x00, 0x08,        // Length: 8
      0x07,                    //   Type: GOAWAY
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x00,  // Stream: 0
      0xff, 0xff, 0xff, 0xff,  //   Last: 0x7fffffff (R-bit set)
      0x00, 0x00, 0x00, 0x00,  //  Error: NO_ERROR
  };

  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;

  deframer_.set_visitor(&visitor);

  EXPECT_CALL(visitor, OnGoAway(0x7fffffff, ERROR_CODE_NO_ERROR));
  deframer_.ProcessInput(reinterpret_cast<const char*>(kH2FrameData),
                         SPDY_ARRAYSIZE(kH2FrameData));
  EXPECT_EQ(Http2DecoderAdapter::SPDY_READY_FOR_FRAME, deframer_.state());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_NO_ERROR, deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             deframer_.spdy_framer_error());
}

TEST_P(SpdyFramerTest, OnAltSvcWithOrigin) {
  const SpdyStreamId kStreamId = 0;  // Stream id must be zero if origin given.

  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;

  deframer_.set_visitor(&visitor);

  SpdyAltSvcWireFormat::AlternativeService altsvc1(
      "pid1", "host", 443, 5, SpdyAltSvcWireFormat::VersionVector());
  SpdyAltSvcWireFormat::AlternativeService altsvc2(
      "p\"=i:d", "h_\\o\"st", 123, 42, SpdyAltSvcWireFormat::VersionVector{24});
  SpdyAltSvcWireFormat::AlternativeServiceVector altsvc_vector;
  altsvc_vector.push_back(altsvc1);
  altsvc_vector.push_back(altsvc2);
  EXPECT_CALL(visitor,
              OnAltSvc(kStreamId, SpdyStringPiece("o_r|g!n"), altsvc_vector));

  SpdyAltSvcIR altsvc_ir(kStreamId);
  altsvc_ir.set_origin("o_r|g!n");
  altsvc_ir.add_altsvc(altsvc1);
  altsvc_ir.add_altsvc(altsvc2);
  SpdySerializedFrame frame(framer_.SerializeFrame(altsvc_ir));
  if (use_output_) {
    output_.Reset();
    EXPECT_GT(framer_.SerializeFrame(altsvc_ir, &output_), 0u);
    frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
  }
  deframer_.ProcessInput(frame.data(), frame.size());

  EXPECT_EQ(Http2DecoderAdapter::SPDY_READY_FOR_FRAME, deframer_.state());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_NO_ERROR, deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             deframer_.spdy_framer_error());
}

TEST_P(SpdyFramerTest, OnAltSvcNoOrigin) {
  const SpdyStreamId kStreamId = 1;

  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;

  deframer_.set_visitor(&visitor);

  SpdyAltSvcWireFormat::AlternativeService altsvc1(
      "pid1", "host", 443, 5, SpdyAltSvcWireFormat::VersionVector());
  SpdyAltSvcWireFormat::AlternativeService altsvc2(
      "p\"=i:d", "h_\\o\"st", 123, 42, SpdyAltSvcWireFormat::VersionVector{24});
  SpdyAltSvcWireFormat::AlternativeServiceVector altsvc_vector;
  altsvc_vector.push_back(altsvc1);
  altsvc_vector.push_back(altsvc2);
  EXPECT_CALL(visitor, OnAltSvc(kStreamId, SpdyStringPiece(""), altsvc_vector));

  SpdyAltSvcIR altsvc_ir(kStreamId);
  altsvc_ir.add_altsvc(altsvc1);
  altsvc_ir.add_altsvc(altsvc2);
  SpdySerializedFrame frame(framer_.SerializeFrame(altsvc_ir));
  deframer_.ProcessInput(frame.data(), frame.size());

  EXPECT_EQ(Http2DecoderAdapter::SPDY_READY_FOR_FRAME, deframer_.state());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_NO_ERROR, deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             deframer_.spdy_framer_error());
}

TEST_P(SpdyFramerTest, OnAltSvcEmptyProtocolId) {
  const SpdyStreamId kStreamId = 0;  // Stream id must be zero if origin given.

  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;

  deframer_.set_visitor(&visitor);

  EXPECT_CALL(visitor,
              OnError(Http2DecoderAdapter::SPDY_INVALID_CONTROL_FRAME));

  SpdyAltSvcIR altsvc_ir(kStreamId);
  altsvc_ir.set_origin("o1");
  altsvc_ir.add_altsvc(SpdyAltSvcWireFormat::AlternativeService(
      "pid1", "host", 443, 5, SpdyAltSvcWireFormat::VersionVector()));
  altsvc_ir.add_altsvc(SpdyAltSvcWireFormat::AlternativeService(
      "", "h1", 443, 10, SpdyAltSvcWireFormat::VersionVector()));
  SpdySerializedFrame frame(framer_.SerializeFrame(altsvc_ir));
  if (use_output_) {
    output_.Reset();
    EXPECT_GT(framer_.SerializeFrame(altsvc_ir, &output_), 0u);
    frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
  }
  deframer_.ProcessInput(frame.data(), frame.size());

  EXPECT_EQ(Http2DecoderAdapter::SPDY_ERROR, deframer_.state());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_INVALID_CONTROL_FRAME,
            deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             deframer_.spdy_framer_error());
}

TEST_P(SpdyFramerTest, OnAltSvcBadLengths) {
  const unsigned char kType = SerializeFrameType(SpdyFrameType::ALTSVC);
  const unsigned char kFrameDataOriginLenLargerThanFrame[] = {
      0x00, 0x00, 0x05, kType, 0x00, 0x00, 0x00,
      0x00, 0x03, 0x42, 0x42,  'f',  'o',  'o',
  };

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);

  deframer_.set_visitor(&visitor);
  visitor.SimulateInFramer(kFrameDataOriginLenLargerThanFrame,
                           sizeof(kFrameDataOriginLenLargerThanFrame));

  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(Http2DecoderAdapter::SPDY_INVALID_CONTROL_FRAME,
            visitor.deframer_.spdy_framer_error());
}

// Tests handling of ALTSVC frames delivered in small chunks.
TEST_P(SpdyFramerTest, ReadChunkedAltSvcFrame) {
  SpdyAltSvcIR altsvc_ir(/* stream_id = */ 1);
  SpdyAltSvcWireFormat::AlternativeService altsvc1(
      "pid1", "host", 443, 5, SpdyAltSvcWireFormat::VersionVector());
  SpdyAltSvcWireFormat::AlternativeService altsvc2(
      "p\"=i:d", "h_\\o\"st", 123, 42, SpdyAltSvcWireFormat::VersionVector{24});
  altsvc_ir.add_altsvc(altsvc1);
  altsvc_ir.add_altsvc(altsvc2);

  SpdySerializedFrame control_frame(framer_.SerializeAltSvc(altsvc_ir));
  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);

  // Read data in small chunks.
  size_t framed_data = 0;
  size_t unframed_data = control_frame.size();
  size_t kReadChunkSize = 5;  // Read five bytes at a time.
  while (unframed_data > 0) {
    size_t to_read = std::min(kReadChunkSize, unframed_data);
    visitor.SimulateInFramer(
        reinterpret_cast<unsigned char*>(control_frame.data() + framed_data),
        to_read);
    unframed_data -= to_read;
    framed_data += to_read;
  }
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.altsvc_count_);
  ASSERT_NE(nullptr, visitor.test_altsvc_ir_);
  ASSERT_EQ(2u, visitor.test_altsvc_ir_->altsvc_vector().size());
  EXPECT_TRUE(visitor.test_altsvc_ir_->altsvc_vector()[0] == altsvc1);
  EXPECT_TRUE(visitor.test_altsvc_ir_->altsvc_vector()[1] == altsvc2);
}

// While RFC7838 Section 4 says that an ALTSVC frame on stream 0 with empty
// origin MUST be ignored, it is not implemented at the framer level: instead,
// such frames are passed on to the consumer.
TEST_P(SpdyFramerTest, ReadAltSvcFrame) {
  constexpr struct {
    uint32_t stream_id;
    const char* origin;
  } test_cases[] = {{0, ""},
                    {1, ""},
                    {0, "https://www.example.com"},
                    {1, "https://www.example.com"}};
  for (const auto& test_case : test_cases) {
    SpdyAltSvcIR altsvc_ir(test_case.stream_id);
    SpdyAltSvcWireFormat::AlternativeService altsvc(
        "pid1", "host", 443, 5, SpdyAltSvcWireFormat::VersionVector());
    altsvc_ir.add_altsvc(altsvc);
    altsvc_ir.set_origin(test_case.origin);
    SpdySerializedFrame frame(framer_.SerializeAltSvc(altsvc_ir));

    TestSpdyVisitor visitor(SpdyFramer::ENABLE_COMPRESSION);
    deframer_.set_visitor(&visitor);
    deframer_.ProcessInput(frame.data(), frame.size());

    EXPECT_EQ(0, visitor.error_count_);
    EXPECT_EQ(1, visitor.altsvc_count_);
    EXPECT_EQ(Http2DecoderAdapter::SPDY_READY_FOR_FRAME, deframer_.state());
    EXPECT_EQ(Http2DecoderAdapter::SPDY_NO_ERROR, deframer_.spdy_framer_error())
        << Http2DecoderAdapter::SpdyFramerErrorToString(
               deframer_.spdy_framer_error());
  }
}

// An ALTSVC frame with invalid Alt-Svc-Field-Value results in an error.
TEST_P(SpdyFramerTest, ErrorOnAltSvcFrameWithInvalidValue) {
  // Alt-Svc-Field-Value must be "clear" or must contain an "=" character
  // per RFC7838 Section 3.
  const char kFrameData[] = {
      0x00, 0x00, 0x16,        //     Length: 22
      0x0a,                    //       Type: ALTSVC
      0x00,                    //      Flags: none
      0x00, 0x00, 0x00, 0x01,  //     Stream: 1
      0x00, 0x00,              // Origin-Len: 0
      0x74, 0x68, 0x69, 0x73,  // thisisnotavalidvalue
      0x69, 0x73, 0x6e, 0x6f, 0x74, 0x61, 0x76, 0x61,
      0x6c, 0x69, 0x64, 0x76, 0x61, 0x6c, 0x75, 0x65,
  };

  TestSpdyVisitor visitor(SpdyFramer::ENABLE_COMPRESSION);
  deframer_.set_visitor(&visitor);
  deframer_.ProcessInput(kFrameData, sizeof(kFrameData));

  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(0, visitor.altsvc_count_);
  EXPECT_EQ(Http2DecoderAdapter::SPDY_ERROR, deframer_.state());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_INVALID_CONTROL_FRAME,
            deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             deframer_.spdy_framer_error());
}

// Tests handling of PRIORITY frames.
TEST_P(SpdyFramerTest, ReadPriority) {
  SpdyPriorityIR priority(/* stream_id = */ 3,
                          /* parent_stream_id = */ 1,
                          /* weight = */ 256,
                          /* exclusive = */ false);
  SpdySerializedFrame frame(framer_.SerializePriority(priority));
  if (use_output_) {
    output_.Reset();
    ASSERT_TRUE(framer_.SerializePriority(priority, &output_));
    frame = SpdySerializedFrame(output_.Begin(), output_.Size(), false);
  }
  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
  deframer_.set_visitor(&visitor);
  EXPECT_CALL(visitor, OnPriority(3, 1, 256, false));
  deframer_.ProcessInput(frame.data(), frame.size());

  EXPECT_EQ(Http2DecoderAdapter::SPDY_READY_FOR_FRAME, deframer_.state());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_NO_ERROR, deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             deframer_.spdy_framer_error());
}

// Tests handling of PRIORITY frame with incorrect size.
TEST_P(SpdyFramerTest, ReadIncorrectlySizedPriority) {
  // PRIORITY frame of size 4, which isn't correct.
  const unsigned char kFrameData[] = {
      0x00, 0x00, 0x04,        // Length: 4
      0x02,                    //   Type: PRIORITY
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x03,  // Stream: 3
      0x00, 0x00, 0x00, 0x01,  // Priority (Truncated)
  };

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(kFrameData, sizeof(kFrameData));

  EXPECT_EQ(Http2DecoderAdapter::SPDY_ERROR, visitor.deframer_.state());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_INVALID_CONTROL_FRAME_SIZE,
            visitor.deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             visitor.deframer_.spdy_framer_error());
}

// Tests handling of PING frame with incorrect size.
TEST_P(SpdyFramerTest, ReadIncorrectlySizedPing) {
  // PING frame of size 4, which isn't correct.
  const unsigned char kFrameData[] = {
      0x00, 0x00, 0x04,        // Length: 4
      0x06,                    //   Type: PING
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x00,  // Stream: 0
      0x00, 0x00, 0x00, 0x01,  // Ping (Truncated)
  };

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(kFrameData, sizeof(kFrameData));

  EXPECT_EQ(Http2DecoderAdapter::SPDY_ERROR, visitor.deframer_.state());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_INVALID_CONTROL_FRAME_SIZE,
            visitor.deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             visitor.deframer_.spdy_framer_error());
}

// Tests handling of WINDOW_UPDATE frame with incorrect size.
TEST_P(SpdyFramerTest, ReadIncorrectlySizedWindowUpdate) {
  // WINDOW_UPDATE frame of size 3, which isn't correct.
  const unsigned char kFrameData[] = {
      0x00, 0x00, 0x03,        // Length: 3
      0x08,                    //   Type: WINDOW_UPDATE
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x03,  // Stream: 3
      0x00, 0x00, 0x01,        // WindowUpdate (Truncated)
  };

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(kFrameData, sizeof(kFrameData));

  EXPECT_EQ(Http2DecoderAdapter::SPDY_ERROR, visitor.deframer_.state());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_INVALID_CONTROL_FRAME_SIZE,
            visitor.deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             visitor.deframer_.spdy_framer_error());
}

// Tests handling of RST_STREAM frame with incorrect size.
TEST_P(SpdyFramerTest, ReadIncorrectlySizedRstStream) {
  // RST_STREAM frame of size 3, which isn't correct.
  const unsigned char kFrameData[] = {
      0x00, 0x00, 0x03,        // Length: 3
      0x03,                    //   Type: RST_STREAM
      0x00,                    //  Flags: none
      0x00, 0x00, 0x00, 0x03,  // Stream: 3
      0x00, 0x00, 0x01,        // RstStream (Truncated)
  };

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(kFrameData, sizeof(kFrameData));

  EXPECT_EQ(Http2DecoderAdapter::SPDY_ERROR, visitor.deframer_.state());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_INVALID_CONTROL_FRAME_SIZE,
            visitor.deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             visitor.deframer_.spdy_framer_error());
}

// Regression test for https://crbug.com/548674:
// RST_STREAM with payload must not be accepted.
TEST_P(SpdyFramerTest, ReadInvalidRstStreamWithPayload) {
  const unsigned char kFrameData[] = {
      0x00, 0x00, 0x07,        //  Length: 7
      0x03,                    //    Type: RST_STREAM
      0x00,                    //   Flags: none
      0x00, 0x00, 0x00, 0x01,  //  Stream: 1
      0x00, 0x00, 0x00, 0x00,  //   Error: NO_ERROR
      'f',  'o',  'o'          // Payload: "foo"
  };

  TestSpdyVisitor visitor(SpdyFramer::DISABLE_COMPRESSION);
  visitor.SimulateInFramer(kFrameData, sizeof(kFrameData));

  EXPECT_EQ(Http2DecoderAdapter::SPDY_ERROR, visitor.deframer_.state());
  EXPECT_EQ(Http2DecoderAdapter::SPDY_INVALID_CONTROL_FRAME_SIZE,
            visitor.deframer_.spdy_framer_error())
      << Http2DecoderAdapter::SpdyFramerErrorToString(
             visitor.deframer_.spdy_framer_error());
}

// Test that SpdyFramer processes, by default, all passed input in one call
// to ProcessInput (i.e. will not be calling set_process_single_input_frame()).
TEST_P(SpdyFramerTest, ProcessAllInput) {
  auto visitor =
      SpdyMakeUnique<TestSpdyVisitor>(SpdyFramer::DISABLE_COMPRESSION);
  deframer_.set_visitor(visitor.get());

  // Create two input frames.
  SpdyHeadersIR headers(/* stream_id = */ 1);
  headers.SetHeader("alpha", "beta");
  headers.SetHeader("gamma", "charlie");
  headers.SetHeader("cookie", "key1=value1; key2=value2");
  SpdySerializedFrame headers_frame(SpdyFramerPeer::SerializeHeaders(
      &framer_, headers, use_output_ ? &output_ : nullptr));

  const char four_score[] = "Four score and seven years ago";
  SpdyDataIR four_score_ir(/* stream_id = */ 1, four_score);
  SpdySerializedFrame four_score_frame(framer_.SerializeData(four_score_ir));

  // Put them in a single buffer (new variables here to make it easy to
  // change the order and type of frames).
  SpdySerializedFrame frame1 = std::move(headers_frame);
  SpdySerializedFrame frame2 = std::move(four_score_frame);

  const size_t frame1_size = frame1.size();
  const size_t frame2_size = frame2.size();

  VLOG(1) << "frame1_size = " << frame1_size;
  VLOG(1) << "frame2_size = " << frame2_size;

  SpdyString input_buffer;
  input_buffer.append(frame1.data(), frame1_size);
  input_buffer.append(frame2.data(), frame2_size);

  const char* buf = input_buffer.data();
  const size_t buf_size = input_buffer.size();

  VLOG(1) << "buf_size = " << buf_size;

  size_t processed = deframer_.ProcessInput(buf, buf_size);
  EXPECT_EQ(buf_size, processed);
  EXPECT_EQ(Http2DecoderAdapter::SPDY_READY_FOR_FRAME, deframer_.state());
  EXPECT_EQ(1, visitor->headers_frame_count_);
  EXPECT_EQ(1, visitor->data_frame_count_);
  EXPECT_EQ(strlen(four_score), static_cast<unsigned>(visitor->data_bytes_));
}

// Test that SpdyFramer stops after processing a full frame if
// process_single_input_frame is set. Input to ProcessInput has two frames, but
// only processes the first when we give it the first frame split at any point,
// or give it more than one frame in the input buffer.
TEST_P(SpdyFramerTest, ProcessAtMostOneFrame) {
  deframer_.set_process_single_input_frame(true);

  // Create two input frames.
  const char four_score[] = "Four score and ...";
  SpdyDataIR four_score_ir(/* stream_id = */ 1, four_score);
  SpdySerializedFrame four_score_frame(framer_.SerializeData(four_score_ir));

  SpdyHeadersIR headers(/* stream_id = */ 2);
  headers.SetHeader("alpha", "beta");
  headers.SetHeader("gamma", "charlie");
  headers.SetHeader("cookie", "key1=value1; key2=value2");
  SpdySerializedFrame headers_frame(SpdyFramerPeer::SerializeHeaders(
      &framer_, headers, use_output_ ? &output_ : nullptr));

  // Put them in a single buffer (new variables here to make it easy to
  // change the order and type of frames).
  SpdySerializedFrame frame1 = std::move(four_score_frame);
  SpdySerializedFrame frame2 = std::move(headers_frame);

  const size_t frame1_size = frame1.size();
  const size_t frame2_size = frame2.size();

  VLOG(1) << "frame1_size = " << frame1_size;
  VLOG(1) << "frame2_size = " << frame2_size;

  SpdyString input_buffer;
  input_buffer.append(frame1.data(), frame1_size);
  input_buffer.append(frame2.data(), frame2_size);

  const char* buf = input_buffer.data();
  const size_t buf_size = input_buffer.size();

  VLOG(1) << "buf_size = " << buf_size;

  for (size_t first_size = 0; first_size <= buf_size; ++first_size) {
    VLOG(1) << "first_size = " << first_size;
    auto visitor =
        SpdyMakeUnique<TestSpdyVisitor>(SpdyFramer::DISABLE_COMPRESSION);
    deframer_.set_visitor(visitor.get());

    EXPECT_EQ(Http2DecoderAdapter::SPDY_READY_FOR_FRAME, deframer_.state());

    size_t processed_first = deframer_.ProcessInput(buf, first_size);
    if (first_size < frame1_size) {
      EXPECT_EQ(first_size, processed_first);

      if (first_size == 0) {
        EXPECT_EQ(Http2DecoderAdapter::SPDY_READY_FOR_FRAME, deframer_.state());
      } else {
        EXPECT_NE(Http2DecoderAdapter::SPDY_READY_FOR_FRAME, deframer_.state());
      }

      const char* rest = buf + processed_first;
      const size_t remaining = buf_size - processed_first;
      VLOG(1) << "remaining = " << remaining;

      size_t processed_second = deframer_.ProcessInput(rest, remaining);

      // Redundant tests just to make it easier to think about.
      EXPECT_EQ(frame1_size - processed_first, processed_second);
      size_t processed_total = processed_first + processed_second;
      EXPECT_EQ(frame1_size, processed_total);
    } else {
      EXPECT_EQ(frame1_size, processed_first);
    }

    EXPECT_EQ(Http2DecoderAdapter::SPDY_READY_FOR_FRAME, deframer_.state());

    // At this point should have processed the entirety of the first frame,
    // and none of the second frame.

    EXPECT_EQ(1, visitor->data_frame_count_);
    EXPECT_EQ(strlen(four_score), static_cast<unsigned>(visitor->data_bytes_));
    EXPECT_EQ(0, visitor->headers_frame_count_);
  }
}

namespace {
void CheckFrameAndIRSize(SpdyFrameIR* ir,
                         SpdyFramer* framer,
                         ArrayOutputBuffer* output_buffer) {
  output_buffer->Reset();
  SpdyFrameType type = ir->frame_type();
  size_t ir_size = ir->size();
  framer->SerializeFrame(*ir, output_buffer);
  if (type == SpdyFrameType::HEADERS || type == SpdyFrameType::PUSH_PROMISE) {
    // For HEADERS and PUSH_PROMISE, the size is an estimate.
    EXPECT_GE(ir_size, output_buffer->Size() * 9 / 10);
    EXPECT_LT(ir_size, output_buffer->Size() * 11 / 10);
  } else {
    EXPECT_EQ(ir_size, output_buffer->Size());
  }
}
}  // namespace

TEST_P(SpdyFramerTest, SpdyFrameIRSize) {
  SpdyFramer framer(SpdyFramer::DISABLE_COMPRESSION);

  const char bytes[] = "this is a very short data frame";
  SpdyDataIR data_ir(1, SpdyStringPiece(bytes, SPDY_ARRAYSIZE(bytes)));
  CheckFrameAndIRSize(&data_ir, &framer, &output_);

  SpdyRstStreamIR rst_ir(/* stream_id = */ 1, ERROR_CODE_PROTOCOL_ERROR);
  CheckFrameAndIRSize(&rst_ir, &framer, &output_);

  SpdySettingsIR settings_ir;
  settings_ir.AddSetting(SETTINGS_HEADER_TABLE_SIZE, 5);
  settings_ir.AddSetting(SETTINGS_ENABLE_PUSH, 6);
  settings_ir.AddSetting(SETTINGS_MAX_CONCURRENT_STREAMS, 7);
  CheckFrameAndIRSize(&settings_ir, &framer, &output_);

  SpdyPingIR ping_ir(42);
  CheckFrameAndIRSize(&ping_ir, &framer, &output_);

  SpdyGoAwayIR goaway_ir(97, ERROR_CODE_NO_ERROR, "Goaway description");
  CheckFrameAndIRSize(&goaway_ir, &framer, &output_);

  SpdyHeadersIR headers_ir(1);
  headers_ir.SetHeader("alpha", "beta");
  headers_ir.SetHeader("gamma", "charlie");
  headers_ir.SetHeader("cookie", "key1=value1; key2=value2");
  CheckFrameAndIRSize(&headers_ir, &framer, &output_);

  SpdyHeadersIR headers_ir_with_continuation(1);
  headers_ir_with_continuation.SetHeader("alpha", SpdyString(100000, 'x'));
  headers_ir_with_continuation.SetHeader("beta", SpdyString(100000, 'x'));
  headers_ir_with_continuation.SetHeader("cookie", "key1=value1; key2=value2");
  CheckFrameAndIRSize(&headers_ir_with_continuation, &framer, &output_);

  SpdyWindowUpdateIR window_update_ir(4, 1024);
  CheckFrameAndIRSize(&window_update_ir, &framer, &output_);

  SpdyPushPromiseIR push_promise_ir(3, 8);
  push_promise_ir.SetHeader("alpha", SpdyString(100000, 'x'));
  push_promise_ir.SetHeader("beta", SpdyString(100000, 'x'));
  push_promise_ir.SetHeader("cookie", "key1=value1; key2=value2");
  CheckFrameAndIRSize(&push_promise_ir, &framer, &output_);

  SpdyAltSvcWireFormat::AlternativeService altsvc1(
      "pid1", "host", 443, 5, SpdyAltSvcWireFormat::VersionVector());
  SpdyAltSvcWireFormat::AlternativeService altsvc2(
      "p\"=i:d", "h_\\o\"st", 123, 42, SpdyAltSvcWireFormat::VersionVector{24});
  SpdyAltSvcWireFormat::AlternativeServiceVector altsvc_vector;
  altsvc_vector.push_back(altsvc1);
  altsvc_vector.push_back(altsvc2);
  SpdyAltSvcIR altsvc_ir(0);
  altsvc_ir.set_origin("o_r|g!n");
  altsvc_ir.add_altsvc(altsvc1);
  altsvc_ir.add_altsvc(altsvc2);
  CheckFrameAndIRSize(&altsvc_ir, &framer, &output_);

  SpdyPriorityIR priority_ir(3, 1, 256, false);
  CheckFrameAndIRSize(&priority_ir, &framer, &output_);

  const char kDescription[] = "Unknown frame";
  const uint8_t kType = 0xaf;
  const uint8_t kFlags = 0x11;
  SpdyUnknownIR unknown_ir(2, kType, kFlags, kDescription);
  CheckFrameAndIRSize(&unknown_ir, &framer, &output_);
}

}  // namespace test

}  // namespace spdy
