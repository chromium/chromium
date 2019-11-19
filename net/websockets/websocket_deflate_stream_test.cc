// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_deflate_stream.h"

#include <stddef.h>
#include <stdint.h>

#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/mock_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/test/gtest_util.h"
#include "net/websockets/websocket_deflate_parameters.h"
#include "net/websockets/websocket_deflate_predictor.h"
#include "net/websockets/websocket_deflater.h"
#include "net/websockets/websocket_frame.h"
#include "net/websockets/websocket_inflater.h"
#include "net/websockets/websocket_stream.h"
#include "net/websockets/websocket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {
namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;

typedef uint32_t FrameFlag;
const FrameFlag kNoFlag = 0;
const FrameFlag kFinal = 1;
const FrameFlag kReserved1 = 2;
// We don't define values for other flags because we don't need them.

// The value must equal to the value of the corresponding
// constant in websocket_deflate_stream.cc
const size_t kChunkSize = 4 * 1024;
const int kWindowBits = 15;

std::string ToString(IOBufferWithSize* buffer) {
  return std::string(buffer->data(), buffer->size());
}

std::string ToString(const scoped_refptr<IOBufferWithSize>& buffer) {
  return ToString(buffer.get());
}

std::string ToString(const WebSocketFrame* frame) {
  return frame->payload
             ? std::string(frame->payload, frame->header.payload_length)
             : "";
}

std::string ToString(const std::unique_ptr<WebSocketFrame>& frame) {
  return ToString(frame.get());
}

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
};

// This mock class relies on some assumptions.
//  - RecordInputDataFrame is called after the corresponding WriteFrames
//    call.
//  - RecordWrittenDataFrame is called before writing the frame.
class WebSocketDeflatePredictorMock : public WebSocketDeflatePredictor {
 public:
  WebSocketDeflatePredictorMock() : result_(DEFLATE) {}
  ~WebSocketDeflatePredictorMock() override {
    // Verify whether all expectaions are consumed.
    if (!frames_to_be_input_.empty()) {
      ADD_FAILURE() << "There are missing frames to be input.";
      return;
    }
    if (!frames_written_.empty()) {
      ADD_FAILURE() << "There are extra written frames.";
      return;
    }
  }

  // WebSocketDeflatePredictor functions.
  Result Predict(const std::vector<std::unique_ptr<WebSocketFrame>>& frames,
                 size_t frame_index) override {
    return result_;
  }
  void RecordInputDataFrame(const WebSocketFrame* frame) override {
    if (!WebSocketFrameHeader::IsKnownDataOpCode(frame->header.opcode)) {
      ADD_FAILURE() << "Control frames should not be recorded.";
      return;
    }
    if (frame->header.reserved1) {
      ADD_FAILURE() << "Input frame may not be compressed.";
      return;
    }
    if (frames_to_be_input_.empty()) {
      ADD_FAILURE() << "Unexpected input data frame";
      return;
    }
    if (frame != frames_to_be_input_.front()) {
      ADD_FAILURE() << "Input data frame does not match the expectation.";
      return;
    }
    frames_to_be_input_.pop_front();
  }
  void RecordWrittenDataFrame(const WebSocketFrame* frame) override {
    if (!WebSocketFrameHeader::IsKnownDataOpCode(frame->header.opcode)) {
      ADD_FAILURE() << "Control frames should not be recorded.";
      return;
    }
    frames_written_.push_back(frame);
  }

  // Sets |result_| for the |Predict| return value.
  void set_result(Result result) { result_ = result; }

  // Adds |frame| as an expectation of future |RecordInputDataFrame| call.
  void AddFrameToBeInput(const WebSocketFrame* frame) {
    if (!WebSocketFrameHeader::IsKnownDataOpCode(frame->header.opcode))
      return;
    frames_to_be_input_.push_back(frame);
  }
  // Verifies that |frame| is recorded in order.
  void VerifySentFrame(const WebSocketFrame* frame) {
    if (!WebSocketFrameHeader::IsKnownDataOpCode(frame->header.opcode))
      return;
    if (frames_written_.empty()) {
      ADD_FAILURE() << "There are missing frames to be written.";
      return;
    }
    if (frame != frames_written_.front()) {
      ADD_FAILURE() << "Written data frame does not match the expectation.";
      return;
    }
    frames_written_.pop_front();
  }
  void AddFramesToBeInput(
      const std::vector<std::unique_ptr<WebSocketFrame>>& frames) {
    for (size_t i = 0; i < frames.size(); ++i)
      AddFrameToBeInput(frames[i].get());
  }
  void VerifySentFrames(
      const std::vector<std::unique_ptr<WebSocketFrame>>& frames) {
    for (size_t i = 0; i < frames.size(); ++i)
      VerifySentFrame(frames[i].get());
  }
  // Call this method in order to disable checks in the destructor when
  // WriteFrames fails.
  void Clear() {
    frames_to_be_input_.clear();
    frames_written_.clear();
  }

 private:
  Result result_;
  // Data frames which will be recorded by |RecordInputFrames|.
  // Pushed by |AddFrameToBeInput| and popped and verified by
  // |RecordInputFrames|.
  base::circular_deque<const WebSocketFrame*> frames_to_be_input_;
  // Data frames recorded by |RecordWrittenFrames|.
  // Pushed by |RecordWrittenFrames| and popped and verified by
  // |VerifySentFrame|.
  base::circular_deque<const WebSocketFrame*> frames_written_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketDeflatePredictorMock);
};

class WebSocketDeflateStreamTest : public ::testing::Test {
 public:
  WebSocketDeflateStreamTest() : mock_stream_(nullptr), predictor_(nullptr) {}
  ~WebSocketDeflateStreamTest() override = default;

  void SetUp() override {
    Initialize(WebSocketDeflater::TAKE_OVER_CONTEXT, kWindowBits);
  }

 protected:
  // Initialize deflate_stream_ with the given parameters.
  void Initialize(WebSocketDeflater::ContextTakeOverMode mode,
                  int window_bits) {
    WebSocketDeflateParameters parameters;
    if (mode == WebSocketDeflater::DO_NOT_TAKE_OVER_CONTEXT) {
      parameters.SetClientNoContextTakeOver();
    }
    parameters.SetClientMaxWindowBits(window_bits);
    mock_stream_ = new testing::StrictMock<MockWebSocketStream>;
    predictor_ = new WebSocketDeflatePredictorMock;
    deflate_stream_ = std::make_unique<WebSocketDeflateStream>(
        base::WrapUnique(mock_stream_), parameters,
        base::WrapUnique(predictor_));
  }

  void AppendTo(std::vector<std::unique_ptr<WebSocketFrame>>* frames,
                WebSocketFrameHeader::OpCode opcode,
                FrameFlag flag) {
    auto frame = std::make_unique<WebSocketFrame>(opcode);
    frame->header.final = (flag & kFinal);
    frame->header.reserved1 = (flag & kReserved1);
    frames->push_back(std::move(frame));
  }

  void AppendTo(std::vector<std::unique_ptr<WebSocketFrame>>* frames,
                WebSocketFrameHeader::OpCode opcode,
                FrameFlag flag,
                const std::string& data) {
    auto frame = std::make_unique<WebSocketFrame>(opcode);
    frame->header.final = (flag & kFinal);
    frame->header.reserved1 = (flag & kReserved1);
    auto buffer = std::make_unique<char[]>(data.size());
    memcpy(buffer.get(), data.c_str(), data.size());
    frame->payload = buffer.get();
    data_buffers.push_back(std::move(buffer));
    frame->header.payload_length = data.size();
    frames->push_back(std::move(frame));
  }

  std::unique_ptr<WebSocketDeflateStream> deflate_stream_;
  // Owned by |deflate_stream_|.
  MockWebSocketStream* mock_stream_;
  // Owned by |deflate_stream_|.
  WebSocketDeflatePredictorMock* predictor_;

  // TODO(yoichio): Make this type std::vector<std::string>.
  std::vector<std::unique_ptr<const char[]>> data_buffers;
};

// Since WebSocketDeflater with DoNotTakeOverContext is well tested at
// websocket_deflater_test.cc, we have only a few tests for this configuration
// here.
class WebSocketDeflateStreamWithDoNotTakeOverContextTest
    : public WebSocketDeflateStreamTest {
 public:
  WebSocketDeflateStreamWithDoNotTakeOverContextTest() = default;
  ~WebSocketDeflateStreamWithDoNotTakeOverContextTest() override = default;

  void SetUp() override {
    Initialize(WebSocketDeflater::DO_NOT_TAKE_OVER_CONTEXT, kWindowBits);
  }
};

class WebSocketDeflateStreamWithClientWindowBitsTest
    : public WebSocketDeflateStreamTest {
 public:
  WebSocketDeflateStreamWithClientWindowBitsTest() = default;
  ~WebSocketDeflateStreamWithClientWindowBitsTest() override = default;

  // Overridden to postpone the call to Initialize().
  void SetUp() override {}

  // This needs to be called explicitly from the tests.
  void SetUpWithWindowBits(int window_bits) {
    Initialize(WebSocketDeflater::TAKE_OVER_CONTEXT, window_bits);
  }

  // Add a frame which will be compressed to a smaller size if the window
  // size is large enough.
  void AddCompressibleFrameString() {
    const std::string word = "Chromium";
    const std::string payload = word + std::string(256, 'a') + word;
    AppendTo(&frames_, WebSocketFrameHeader::kOpCodeText, kFinal, payload);
    predictor_->AddFramesToBeInput(frames_);
  }

 protected:
  std::vector<std::unique_ptr<WebSocketFrame>> frames_;
};

// ReadFrameStub is a stub for WebSocketStream::ReadFrames.
// It returns |result_| and |frames_to_output_| to the caller and
// saves parameters to |frames_passed_| and |callback_|.
class ReadFramesStub {
 public:
  explicit ReadFramesStub(int result) : result_(result) {}

  ReadFramesStub(int result,
                 std::vector<std::unique_ptr<WebSocketFrame>>* frames_to_output)
      : result_(result) {
    frames_to_output_.swap(*frames_to_output);
  }

  int Call(std::vector<std::unique_ptr<WebSocketFrame>>* frames,
           const CompletionRepeatingCallback& callback) {
    DCHECK(frames->empty());
    frames_passed_ = frames;
    callback_ = callback;
    frames->swap(frames_to_output_);
    return result_;
  }

  int result() const { return result_; }
  const CompletionRepeatingCallback& callback() const { return callback_; }
  std::vector<std::unique_ptr<WebSocketFrame>>* frames_passed() {
    return frames_passed_;
  }

 private:
  int result_;
  CompletionRepeatingCallback callback_;
  std::vector<std::unique_ptr<WebSocketFrame>> frames_to_output_;
  std::vector<std::unique_ptr<WebSocketFrame>>* frames_passed_;
};

// WriteFramesStub is a stub for WebSocketStream::WriteFrames.
// It returns |result_| and |frames_| to the caller and
// saves |callback| parameter to |callback_|.
class WriteFramesStub {
 public:
  explicit WriteFramesStub(WebSocketDeflatePredictorMock* predictor,
                           int result)
      : result_(result), predictor_(predictor) {}

  int Call(std::vector<std::unique_ptr<WebSocketFrame>>* frames,
           const CompletionRepeatingCallback& callback) {
    frames_.insert(frames_.end(), std::make_move_iterator(frames->begin()),
                   std::make_move_iterator(frames->end()));
    frames->clear();
    callback_ = callback;
    predictor_->VerifySentFrames(frames_);
    return result_;
  }

  int result() const { return result_; }
  const CompletionRepeatingCallback& callback() const { return callback_; }
  std::vector<std::unique_ptr<WebSocketFrame>>* frames() { return &frames_; }

 private:
  int result_;
  CompletionRepeatingCallback callback_;
  std::vector<std::unique_ptr<WebSocketFrame>> frames_;
  WebSocketDeflatePredictorMock* predictor_;
};

TEST_F(WebSocketDeflateStreamTest, ReadFailedImmediately) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(&frames, _))
        .WillOnce(Return(ERR_FAILED));
  }
  EXPECT_THAT(deflate_stream_->ReadFrames(&frames, CompletionOnceCallback()),
              IsError(ERR_FAILED));
}

TEST_F(WebSocketDeflateStreamTest, ReadUncompressedFrameImmediately) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kFinal,
           "hello");
  ReadFramesStub stub(OK, &frames_to_output);
  std::vector<std::unique_ptr<WebSocketFrame>> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_THAT(deflate_stream_->ReadFrames(&frames, CompletionOnceCallback()),
              IsOk());
  ASSERT_EQ(1u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ("hello", ToString(frames[0]));
}

TEST_F(WebSocketDeflateStreamTest, ReadUncompressedFrameAsync) {
  ReadFramesStub stub(ERR_IO_PENDING);
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  base::MockCallback<CompletionOnceCallback> mock_callback;
  base::MockCallback<base::Closure> checkpoint;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
    EXPECT_CALL(checkpoint, Run());
    EXPECT_CALL(mock_callback, Run(OK));
  }
  ASSERT_THAT(deflate_stream_->ReadFrames(&frames, mock_callback.Get()),
              IsError(ERR_IO_PENDING));
  ASSERT_EQ(0u, frames.size());

  checkpoint.Run();

  AppendTo(stub.frames_passed(),
           WebSocketFrameHeader::kOpCodeText,
           kFinal,
           "hello");
  stub.callback().Run(OK);
  ASSERT_EQ(1u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ("hello", ToString(frames[0]));
}

TEST_F(WebSocketDeflateStreamTest, ReadFailedAsync) {
  ReadFramesStub stub(ERR_IO_PENDING);
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  base::MockCallback<CompletionOnceCallback> mock_callback;
  base::MockCallback<base::Closure> checkpoint;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
    EXPECT_CALL(checkpoint, Run());
    EXPECT_CALL(mock_callback, Run(ERR_FAILED));
  }
  ASSERT_THAT(deflate_stream_->ReadFrames(&frames, mock_callback.Get()),
              IsError(ERR_IO_PENDING));
  ASSERT_EQ(0u, frames.size());

  checkpoint.Run();

  AppendTo(stub.frames_passed(),
           WebSocketFrameHeader::kOpCodeText,
           kFinal,
           "hello");
  stub.callback().Run(ERR_FAILED);
  ASSERT_EQ(0u, frames.size());
}

TEST_F(WebSocketDeflateStreamTest, ReadCompressedFrameImmediately) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kFinal | kReserved1,
           std::string("\xf2\x48\xcd\xc9\xc9\x07\x00", 7));
  ReadFramesStub stub(OK, &frames_to_output);
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_THAT(deflate_stream_->ReadFrames(&frames, CompletionOnceCallback()),
              IsOk());
  ASSERT_EQ(1u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ("Hello", ToString(frames[0]));
}

TEST_F(WebSocketDeflateStreamTest, ReadCompressedFrameAsync) {
  ReadFramesStub stub(ERR_IO_PENDING);

  base::MockCallback<CompletionOnceCallback> mock_callback;
  base::MockCallback<base::Closure> checkpoint;
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
    EXPECT_CALL(checkpoint, Run());
    EXPECT_CALL(mock_callback, Run(OK));
  }
  ASSERT_THAT(deflate_stream_->ReadFrames(&frames, mock_callback.Get()),
              IsError(ERR_IO_PENDING));

  checkpoint.Run();

  AppendTo(stub.frames_passed(),
           WebSocketFrameHeader::kOpCodeText,
           kFinal | kReserved1,
           std::string("\xf2\x48\xcd\xc9\xc9\x07\x00", 7));
  stub.callback().Run(OK);

  ASSERT_EQ(1u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ("Hello", ToString(frames[0]));
}

TEST_F(WebSocketDeflateStreamTest,
       ReadCompressedFrameFragmentImmediatelyButInflaterReturnsPending) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames_to_output;
  const std::string data1("\xf2", 1);
  const std::string data2("\x48\xcd\xc9\xc9\x07\x00", 6);
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kReserved1,
           data1);
  ReadFramesStub stub1(OK, &frames_to_output), stub2(ERR_IO_PENDING);
  base::MockCallback<CompletionOnceCallback> mock_callback;
  base::MockCallback<base::Closure> checkpoint;
  std::vector<std::unique_ptr<WebSocketFrame>> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub1, &ReadFramesStub::Call))
        .WillOnce(Invoke(&stub2, &ReadFramesStub::Call));
    EXPECT_CALL(checkpoint, Run());
    EXPECT_CALL(mock_callback, Run(OK));
  }
  ASSERT_THAT(deflate_stream_->ReadFrames(&frames, mock_callback.Get()),
              IsError(ERR_IO_PENDING));
  ASSERT_EQ(0u, frames.size());

  AppendTo(stub2.frames_passed(),
           WebSocketFrameHeader::kOpCodeText,
           kFinal,
           data2);

  checkpoint.Run();
  stub2.callback().Run(OK);

  ASSERT_EQ(1u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ("Hello", ToString(frames[0]));
}

TEST_F(WebSocketDeflateStreamTest, ReadInvalidCompressedPayload) {
  const std::string data("\xf2\x48\xcdINVALID", 10);
  std::vector<std::unique_ptr<WebSocketFrame>> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kFinal | kReserved1,
           data);
  ReadFramesStub stub(OK, &frames_to_output);
  std::vector<std::unique_ptr<WebSocketFrame>> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_EQ(ERR_WS_PROTOCOL_ERROR,
            deflate_stream_->ReadFrames(&frames, CompletionOnceCallback()));
  ASSERT_EQ(0u, frames.size());
}

TEST_F(WebSocketDeflateStreamTest, MergeMultipleFramesInReadFrames) {
  const std::string data1("\xf2\x48\xcd", 3);
  const std::string data2("\xc9\xc9\x07\x00", 4);
  std::vector<std::unique_ptr<WebSocketFrame>> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kReserved1,
           data1);
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeContinuation,
           kFinal,
           data2);
  ReadFramesStub stub(OK, &frames_to_output);
  std::vector<std::unique_ptr<WebSocketFrame>> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_THAT(deflate_stream_->ReadFrames(&frames, CompletionOnceCallback()),
              IsOk());
  ASSERT_EQ(1u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ("Hello", ToString(frames[0]));
}

TEST_F(WebSocketDeflateStreamTest, ReadUncompressedEmptyFrames) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kNoFlag);
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeContinuation,
           kFinal);
  ReadFramesStub stub(OK, &frames_to_output);
  std::vector<std::unique_ptr<WebSocketFrame>> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_THAT(deflate_stream_->ReadFrames(&frames, CompletionOnceCallback()),
              IsOk());
  ASSERT_EQ(2u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_FALSE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ("", ToString(frames[0]));
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeContinuation,
            frames[1]->header.opcode);
  EXPECT_TRUE(frames[1]->header.final);
  EXPECT_FALSE(frames[1]->header.reserved1);
  EXPECT_EQ("", ToString(frames[1]));
}

TEST_F(WebSocketDeflateStreamTest, ReadCompressedEmptyFrames) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kReserved1,
           std::string("\x02\x00", 1));
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeContinuation,
           kFinal);
  ReadFramesStub stub(OK, &frames_to_output);
  std::vector<std::unique_ptr<WebSocketFrame>> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_THAT(deflate_stream_->ReadFrames(&frames, CompletionOnceCallback()),
              IsOk());
  ASSERT_EQ(1u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ("", ToString(frames[0]));
}

TEST_F(WebSocketDeflateStreamTest,
       ReadCompressedFrameFollowedByEmptyFrame) {
  const std::string data("\xf2\x48\xcd\xc9\xc9\x07\x00", 7);
  std::vector<std::unique_ptr<WebSocketFrame>> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kReserved1,
           data);
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeContinuation,
           kFinal);
  ReadFramesStub stub(OK, &frames_to_output);
  std::vector<std::unique_ptr<WebSocketFrame>> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_THAT(deflate_stream_->ReadFrames(&frames, CompletionOnceCallback()),
              IsOk());
  ASSERT_EQ(1u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ("Hello", ToString(frames[0]));
}

TEST_F(WebSocketDeflateStreamTest, ReadControlFrameBetweenDataFrames) {
  const std::string data1("\xf2\x48\xcd", 3);
  const std::string data2("\xc9\xc9\x07\x00", 4);
  std::vector<std::unique_ptr<WebSocketFrame>> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kReserved1,
           data1);
  AppendTo(&frames_to_output, WebSocketFrameHeader::kOpCodePing, kFinal);
  AppendTo(&frames_to_output, WebSocketFrameHeader::kOpCodeText, kFinal, data2);
  ReadFramesStub stub(OK, &frames_to_output);
  std::vector<std::unique_ptr<WebSocketFrame>> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_THAT(deflate_stream_->ReadFrames(&frames, CompletionOnceCallback()),
              IsOk());
  ASSERT_EQ(2u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodePing, frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[1]->header.opcode);
  EXPECT_TRUE(frames[1]->header.final);
  EXPECT_FALSE(frames[1]->header.reserved1);
  EXPECT_EQ("Hello", ToString(frames[1]));
}

TEST_F(WebSocketDeflateStreamTest, SplitToMultipleFramesInReadFrames) {
  WebSocketDeflater deflater(WebSocketDeflater::TAKE_OVER_CONTEXT);
  deflater.Initialize(kWindowBits);
  const size_t kSize = kChunkSize * 3;
  const std::string original_data(kSize, 'a');
  deflater.AddBytes(original_data.data(), original_data.size());
  deflater.Finish();

  std::vector<std::unique_ptr<WebSocketFrame>> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeBinary,
           kFinal | kReserved1,
           ToString(deflater.GetOutput(deflater.CurrentOutputSize())));

  ReadFramesStub stub(OK, &frames_to_output);
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }

  ASSERT_THAT(deflate_stream_->ReadFrames(&frames, CompletionOnceCallback()),
              IsOk());
  ASSERT_EQ(3u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeBinary, frames[0]->header.opcode);
  EXPECT_FALSE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ(kChunkSize, static_cast<size_t>(frames[0]->header.payload_length));
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeContinuation,
            frames[1]->header.opcode);
  EXPECT_FALSE(frames[1]->header.final);
  EXPECT_FALSE(frames[1]->header.reserved1);
  EXPECT_EQ(kChunkSize, static_cast<size_t>(frames[1]->header.payload_length));
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeContinuation,
            frames[2]->header.opcode);
  EXPECT_TRUE(frames[2]->header.final);
  EXPECT_FALSE(frames[2]->header.reserved1);
  EXPECT_EQ(kChunkSize, static_cast<size_t>(frames[2]->header.payload_length));
  EXPECT_EQ(original_data,
            ToString(frames[0]) + ToString(frames[1]) + ToString(frames[2]));
}

TEST_F(WebSocketDeflateStreamTest, InflaterInternalDataCanBeEmpty) {
  WebSocketDeflater deflater(WebSocketDeflater::TAKE_OVER_CONTEXT);
  deflater.Initialize(kWindowBits);
  const std::string original_data(kChunkSize, 'a');
  deflater.AddBytes(original_data.data(), original_data.size());
  deflater.Finish();

  std::vector<std::unique_ptr<WebSocketFrame>> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeBinary,
           kReserved1,
           ToString(deflater.GetOutput(deflater.CurrentOutputSize())));
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeBinary,
           kFinal,
           "");

  ReadFramesStub stub(OK, &frames_to_output);
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }

  ASSERT_THAT(deflate_stream_->ReadFrames(&frames, CompletionOnceCallback()),
              IsOk());
  ASSERT_EQ(2u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeBinary, frames[0]->header.opcode);
  EXPECT_FALSE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ(kChunkSize, static_cast<size_t>(frames[0]->header.payload_length));

  EXPECT_EQ(WebSocketFrameHeader::kOpCodeContinuation,
            frames[1]->header.opcode);
  EXPECT_TRUE(frames[1]->header.final);
  EXPECT_FALSE(frames[1]->header.reserved1);
  EXPECT_EQ(0u, static_cast<size_t>(frames[1]->header.payload_length));
  EXPECT_EQ(original_data, ToString(frames[0]) + ToString(frames[1]));
}

TEST_F(WebSocketDeflateStreamTest,
       Reserved1TurnsOnDuringReadingCompressedContinuationFrame) {
  const std::string data1("\xf2\x48\xcd", 3);
  const std::string data2("\xc9\xc9\x07\x00", 4);
  std::vector<std::unique_ptr<WebSocketFrame>> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kReserved1,
           data1);
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeContinuation,
           kFinal | kReserved1,
           data2);
  ReadFramesStub stub(OK, &frames_to_output);
  std::vector<std::unique_ptr<WebSocketFrame>> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_EQ(ERR_WS_PROTOCOL_ERROR,
            deflate_stream_->ReadFrames(&frames, CompletionOnceCallback()));
}

TEST_F(WebSocketDeflateStreamTest,
       Reserved1TurnsOnDuringReadingUncompressedContinuationFrame) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kNoFlag,
           "hello");
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeContinuation,
           kFinal | kReserved1,
           "world");
  ReadFramesStub stub(OK, &frames_to_output);
  std::vector<std::unique_ptr<WebSocketFrame>> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_EQ(ERR_WS_PROTOCOL_ERROR,
            deflate_stream_->ReadFrames(&frames, CompletionOnceCallback()));
}

TEST_F(WebSocketDeflateStreamTest, ReadCompressedMessages) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kFinal | kReserved1,
           std::string(
               "\x4a\xce\xcf\x2d\x28\x4a\x2d\x2e\x4e\x4d\x31\x04\x00", 13));
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kFinal | kReserved1,
           std::string("\x4a\x86\x33\x8d\x00\x00", 6));
  ReadFramesStub stub(OK, &frames_to_output);
  std::vector<std::unique_ptr<WebSocketFrame>> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_THAT(deflate_stream_->ReadFrames(&frames, CompletionOnceCallback()),
              IsOk());
  ASSERT_EQ(2u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ("compressed1", ToString(frames[0]));
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[1]->header.opcode);
  EXPECT_TRUE(frames[1]->header.final);
  EXPECT_FALSE(frames[1]->header.reserved1);
  EXPECT_EQ("compressed2", ToString(frames[1]));
}

TEST_F(WebSocketDeflateStreamTest, ReadUncompressedMessages) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kFinal,
           "uncompressed1");
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kFinal,
           "uncompressed2");
  ReadFramesStub stub(OK, &frames_to_output);
  std::vector<std::unique_ptr<WebSocketFrame>> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_THAT(deflate_stream_->ReadFrames(&frames, CompletionOnceCallback()),
              IsOk());
  ASSERT_EQ(2u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ("uncompressed1", ToString(frames[0]));
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[1]->header.opcode);
  EXPECT_TRUE(frames[1]->header.final);
  EXPECT_FALSE(frames[1]->header.reserved1);
  EXPECT_EQ("uncompressed2", ToString(frames[1]));
}

TEST_F(WebSocketDeflateStreamTest,
       ReadCompressedMessageThenUncompressedMessage) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kFinal | kReserved1,
           std::string(
               "\x4a\xce\xcf\x2d\x28\x4a\x2d\x2e\x4e\x4d\x01\x00", 12));
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kFinal,
           "uncompressed");
  ReadFramesStub stub(OK, &frames_to_output);
  std::vector<std::unique_ptr<WebSocketFrame>> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_THAT(deflate_stream_->ReadFrames(&frames, CompletionOnceCallback()),
              IsOk());
  ASSERT_EQ(2u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ("compressed", ToString(frames[0]));
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[1]->header.opcode);
  EXPECT_TRUE(frames[1]->header.final);
  EXPECT_FALSE(frames[1]->header.reserved1);
  EXPECT_EQ("uncompressed", ToString(frames[1]));
}

TEST_F(WebSocketDeflateStreamTest,
       ReadUncompressedMessageThenCompressedMessage) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kFinal,
           "uncompressed");
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kFinal | kReserved1,
           std::string(
               "\x4a\xce\xcf\x2d\x28\x4a\x2d\x2e\x4e\x4d\x01\x00", 12));
  ReadFramesStub stub(OK, &frames_to_output);
  std::vector<std::unique_ptr<WebSocketFrame>> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_THAT(deflate_stream_->ReadFrames(&frames, CompletionOnceCallback()),
              IsOk());
  ASSERT_EQ(2u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ("uncompressed", ToString(frames[0]));
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[1]->header.opcode);
  EXPECT_TRUE(frames[1]->header.final);
  EXPECT_FALSE(frames[1]->header.reserved1);
  EXPECT_EQ("compressed", ToString(frames[1]));
}

// This is a regression test for crbug.com/343506.
TEST_F(WebSocketDeflateStreamTest, ReadEmptyAsyncFrame) {
  std::vector<std::unique_ptr<ReadFramesStub>> stub_vector;
  stub_vector.push_back(std::make_unique<ReadFramesStub>(ERR_IO_PENDING));
  stub_vector.push_back(std::make_unique<ReadFramesStub>(ERR_IO_PENDING));
  base::MockCallback<CompletionOnceCallback> mock_callback;
  std::vector<std::unique_ptr<WebSocketFrame>> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFramesInternal(&frames, _))
        .WillOnce(Invoke(stub_vector[0].get(), &ReadFramesStub::Call));

    EXPECT_CALL(*mock_stream_, ReadFramesInternal(&frames, _))
        .WillOnce(Invoke(stub_vector[1].get(), &ReadFramesStub::Call));

    EXPECT_CALL(mock_callback, Run(OK));
  }

  ASSERT_THAT(deflate_stream_->ReadFrames(&frames, mock_callback.Get()),
              IsError(ERR_IO_PENDING));
  AppendTo(stub_vector[0]->frames_passed(),
           WebSocketFrameHeader::kOpCodeText,
           kReserved1,
           std::string());
  stub_vector[0]->callback().Run(OK);
  AppendTo(stub_vector[1]->frames_passed(),
           WebSocketFrameHeader::kOpCodeContinuation,
           kFinal,
           std::string("\x02\x00"));
  stub_vector[1]->callback().Run(OK);
  ASSERT_EQ(1u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_EQ("", ToString(frames[0]));
}

TEST_F(WebSocketDeflateStreamTest, WriteEmpty) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, WriteFramesInternal(&frames, _)).Times(0);
  }
  EXPECT_THAT(deflate_stream_->WriteFrames(&frames, CompletionOnceCallback()),
              IsOk());
}

TEST_F(WebSocketDeflateStreamTest, WriteFailedImmediately) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, WriteFramesInternal(&frames, _))
        .WillOnce(Return(ERR_FAILED));
  }

  AppendTo(&frames, WebSocketFrameHeader::kOpCodeText, kFinal, "hello");
  predictor_->AddFramesToBeInput(frames);
  EXPECT_THAT(deflate_stream_->WriteFrames(&frames, CompletionOnceCallback()),
              IsError(ERR_FAILED));
  predictor_->Clear();
}

TEST_F(WebSocketDeflateStreamTest, WriteFrameImmediately) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  WriteFramesStub stub(predictor_, OK);
  AppendTo(&frames, WebSocketFrameHeader::kOpCodeText, kFinal, "Hello");
  predictor_->AddFramesToBeInput(frames);
  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, WriteFramesInternal(_, _))
        .WillOnce(Invoke(&stub, &WriteFramesStub::Call));
  }
  ASSERT_THAT(deflate_stream_->WriteFrames(&frames, CompletionOnceCallback()),
              IsOk());
  const std::vector<std::unique_ptr<WebSocketFrame>>& frames_passed =
      *stub.frames();
  ASSERT_EQ(1u, frames_passed.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames_passed[0]->header.opcode);
  EXPECT_TRUE(frames_passed[0]->header.final);
  EXPECT_TRUE(frames_passed[0]->header.reserved1);
  EXPECT_EQ(std::string("\xf2\x48\xcd\xc9\xc9\x07\x00", 7),
            ToString(frames_passed[0]));
}

TEST_F(WebSocketDeflateStreamTest, WriteFrameAsync) {
  WriteFramesStub stub(predictor_, ERR_IO_PENDING);
  base::MockCallback<CompletionOnceCallback> mock_callback;
  base::MockCallback<base::Closure> checkpoint;
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, WriteFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub, &WriteFramesStub::Call));
    EXPECT_CALL(checkpoint, Run());
    EXPECT_CALL(mock_callback, Run(OK));
  }
  AppendTo(&frames, WebSocketFrameHeader::kOpCodeText, kFinal, "Hello");
  predictor_->AddFramesToBeInput(frames);
  ASSERT_THAT(deflate_stream_->WriteFrames(&frames, mock_callback.Get()),
              IsError(ERR_IO_PENDING));

  checkpoint.Run();
  stub.callback().Run(OK);

  const std::vector<std::unique_ptr<WebSocketFrame>>& frames_passed =
      *stub.frames();
  ASSERT_EQ(1u, frames_passed.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames_passed[0]->header.opcode);
  EXPECT_TRUE(frames_passed[0]->header.final);
  EXPECT_TRUE(frames_passed[0]->header.reserved1);
  EXPECT_EQ(std::string("\xf2\x48\xcd\xc9\xc9\x07\x00", 7),
            ToString(frames_passed[0]));
}

TEST_F(WebSocketDeflateStreamTest, WriteControlFrameBetweenDataFrames) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  AppendTo(&frames, WebSocketFrameHeader::kOpCodeText, kNoFlag, "Hel");
  AppendTo(&frames, WebSocketFrameHeader::kOpCodePing, kFinal);
  AppendTo(&frames, WebSocketFrameHeader::kOpCodeContinuation, kFinal, "lo");
  predictor_->AddFramesToBeInput(frames);
  WriteFramesStub stub(predictor_, OK);

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, WriteFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub, &WriteFramesStub::Call));
  }
  ASSERT_THAT(deflate_stream_->WriteFrames(&frames, CompletionOnceCallback()),
              IsOk());
  const std::vector<std::unique_ptr<WebSocketFrame>>& frames_passed =
      *stub.frames();
  ASSERT_EQ(2u, frames_passed.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodePing, frames_passed[0]->header.opcode);
  EXPECT_TRUE(frames_passed[0]->header.final);
  EXPECT_FALSE(frames_passed[0]->header.reserved1);
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames_passed[1]->header.opcode);
  EXPECT_TRUE(frames_passed[1]->header.final);
  EXPECT_TRUE(frames_passed[1]->header.reserved1);
  EXPECT_EQ(std::string("\xf2\x48\xcd\xc9\xc9\x07\x00", 7),
            ToString(frames_passed[1]));
}

TEST_F(WebSocketDeflateStreamTest, WriteEmptyMessage) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  AppendTo(&frames, WebSocketFrameHeader::kOpCodeText, kFinal);
  predictor_->AddFramesToBeInput(frames);
  WriteFramesStub stub(predictor_, OK);

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, WriteFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub, &WriteFramesStub::Call));
  }
  ASSERT_THAT(deflate_stream_->WriteFrames(&frames, CompletionOnceCallback()),
              IsOk());
  const std::vector<std::unique_ptr<WebSocketFrame>>& frames_passed =
      *stub.frames();
  ASSERT_EQ(1u, frames_passed.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames_passed[0]->header.opcode);
  EXPECT_TRUE(frames_passed[0]->header.final);
  EXPECT_TRUE(frames_passed[0]->header.reserved1);
  EXPECT_EQ(std::string("\x00", 1), ToString(frames_passed[0]));
}

TEST_F(WebSocketDeflateStreamTest, WriteUncompressedMessage) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  AppendTo(&frames, WebSocketFrameHeader::kOpCodeText, kNoFlag, "AAAA");
  AppendTo(&frames, WebSocketFrameHeader::kOpCodeContinuation, kFinal, "AAA");
  predictor_->AddFramesToBeInput(frames);
  WriteFramesStub stub(predictor_, OK);

  predictor_->set_result(WebSocketDeflatePredictor::DO_NOT_DEFLATE);

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, WriteFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub, &WriteFramesStub::Call));
  }
  ASSERT_THAT(deflate_stream_->WriteFrames(&frames, CompletionOnceCallback()),
              IsOk());
  const std::vector<std::unique_ptr<WebSocketFrame>>& frames_passed =
      *stub.frames();
  ASSERT_EQ(2u, frames_passed.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames_passed[0]->header.opcode);
  EXPECT_FALSE(frames_passed[0]->header.final);
  EXPECT_FALSE(frames_passed[0]->header.reserved1);
  EXPECT_EQ("AAAA", ToString(frames_passed[0]));
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeContinuation,
            frames_passed[1]->header.opcode);
  EXPECT_TRUE(frames_passed[1]->header.final);
  EXPECT_FALSE(frames_passed[1]->header.reserved1);
  EXPECT_EQ("AAA", ToString(frames_passed[1]));
}

TEST_F(WebSocketDeflateStreamTest, LargeDeflatedFramesShouldBeSplit) {
  WebSocketDeflater deflater(WebSocketDeflater::TAKE_OVER_CONTEXT);
  LinearCongruentialGenerator lcg(133);
  WriteFramesStub stub(predictor_, OK);
  const size_t size = 1024;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, WriteFramesInternal(_, _))
        .WillRepeatedly(Invoke(&stub, &WriteFramesStub::Call));
  }
  std::vector<std::unique_ptr<WebSocketFrame>> total_compressed_frames;
  std::vector<std::string> buffers;

  deflater.Initialize(kWindowBits);
  while (true) {
    bool is_final = (total_compressed_frames.size() >= 2);
    std::vector<std::unique_ptr<WebSocketFrame>> frames;
    std::string data;
    for (size_t i = 0; i < size; ++i)
      data += static_cast<char>(lcg.Generate());
    deflater.AddBytes(data.data(), data.size());
    FrameFlag flag = is_final ? kFinal : kNoFlag;
    AppendTo(&frames, WebSocketFrameHeader::kOpCodeBinary, flag, data);
    predictor_->AddFramesToBeInput(frames);
    ASSERT_THAT(deflate_stream_->WriteFrames(&frames, CompletionOnceCallback()),
                IsOk());
    for (auto& frame : *stub.frames()) {
      buffers.push_back(
          std::string(frame->payload, frame->header.payload_length));
      frame->payload = (buffers.end() - 1)->data();
    }
    total_compressed_frames.insert(
        total_compressed_frames.end(),
        std::make_move_iterator(stub.frames()->begin()),
        std::make_move_iterator(stub.frames()->end()));
    stub.frames()->clear();
    if (is_final)
      break;
  }
  deflater.Finish();
  std::string total_deflated;
  for (size_t i = 0; i < total_compressed_frames.size(); ++i) {
    WebSocketFrame* frame = total_compressed_frames[i].get();
    const WebSocketFrameHeader& header = frame->header;
    if (i > 0) {
      EXPECT_EQ(header.kOpCodeContinuation, header.opcode);
      EXPECT_FALSE(header.reserved1);
    } else {
      EXPECT_EQ(header.kOpCodeBinary, header.opcode);
      EXPECT_TRUE(header.reserved1);
    }
    const bool is_final_frame = (i + 1 == total_compressed_frames.size());
    EXPECT_EQ(is_final_frame, header.final);
    if (!is_final_frame)
      EXPECT_GT(header.payload_length, 0ul);
    total_deflated += ToString(frame);
  }
  EXPECT_EQ(total_deflated,
            ToString(deflater.GetOutput(deflater.CurrentOutputSize())));
}

TEST_F(WebSocketDeflateStreamTest, WriteMultipleMessages) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  AppendTo(&frames, WebSocketFrameHeader::kOpCodeText, kFinal, "Hello");
  AppendTo(&frames, WebSocketFrameHeader::kOpCodeText, kFinal, "Hello");
  predictor_->AddFramesToBeInput(frames);
  WriteFramesStub stub(predictor_, OK);

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, WriteFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub, &WriteFramesStub::Call));
  }
  ASSERT_THAT(deflate_stream_->WriteFrames(&frames, CompletionOnceCallback()),
              IsOk());
  const std::vector<std::unique_ptr<WebSocketFrame>>& frames_passed =
      *stub.frames();
  ASSERT_EQ(2u, frames_passed.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames_passed[0]->header.opcode);
  EXPECT_TRUE(frames_passed[0]->header.final);
  EXPECT_TRUE(frames_passed[0]->header.reserved1);
  EXPECT_EQ(std::string("\xf2\x48\xcd\xc9\xc9\x07\x00", 7),
            ToString(frames_passed[0]));
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames_passed[1]->header.opcode);
  EXPECT_TRUE(frames_passed[1]->header.final);
  EXPECT_TRUE(frames_passed[1]->header.reserved1);
  EXPECT_EQ(std::string("\xf2\x00\x11\x00\x00", 5), ToString(frames_passed[1]));
}

TEST_F(WebSocketDeflateStreamWithDoNotTakeOverContextTest,
       WriteMultipleMessages) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  AppendTo(&frames, WebSocketFrameHeader::kOpCodeText, kFinal, "Hello");
  AppendTo(&frames, WebSocketFrameHeader::kOpCodeText, kFinal, "Hello");
  predictor_->AddFramesToBeInput(frames);
  WriteFramesStub stub(predictor_, OK);

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, WriteFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub, &WriteFramesStub::Call));
  }
  ASSERT_THAT(deflate_stream_->WriteFrames(&frames, CompletionOnceCallback()),
              IsOk());
  const std::vector<std::unique_ptr<WebSocketFrame>>& frames_passed =
      *stub.frames();
  ASSERT_EQ(2u, frames_passed.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames_passed[0]->header.opcode);
  EXPECT_TRUE(frames_passed[0]->header.final);
  EXPECT_TRUE(frames_passed[0]->header.reserved1);
  EXPECT_EQ(std::string("\xf2\x48\xcd\xc9\xc9\x07\x00", 7),
            ToString(frames_passed[0]));
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames_passed[1]->header.opcode);
  EXPECT_TRUE(frames_passed[1]->header.final);
  EXPECT_TRUE(frames_passed[1]->header.reserved1);
  EXPECT_EQ(std::string("\xf2\x48\xcd\xc9\xc9\x07\x00", 7),
            ToString(frames_passed[1]));
}

// In order to check the stream works correctly for multiple
// "PossiblyCompressedMessage"s, we test various messages at one test case.
TEST_F(WebSocketDeflateStreamWithDoNotTakeOverContextTest,
       WritePossiblyCompressMessages) {
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  AppendTo(&frames, WebSocketFrameHeader::kOpCodeText, kNoFlag, "He");
  AppendTo(&frames, WebSocketFrameHeader::kOpCodeContinuation, kFinal, "llo");
  AppendTo(&frames, WebSocketFrameHeader::kOpCodeText, kNoFlag, "AAAAAAAAAA");
  AppendTo(&frames, WebSocketFrameHeader::kOpCodeContinuation, kFinal, "AA");
  AppendTo(&frames, WebSocketFrameHeader::kOpCodeText, kNoFlag, "XX");
  AppendTo(&frames, WebSocketFrameHeader::kOpCodeContinuation, kFinal, "YY");
  predictor_->AddFramesToBeInput(frames);
  WriteFramesStub stub(predictor_, OK);
  predictor_->set_result(WebSocketDeflatePredictor::TRY_DEFLATE);

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, WriteFramesInternal(&frames, _))
        .WillOnce(Invoke(&stub, &WriteFramesStub::Call));
  }
  ASSERT_THAT(deflate_stream_->WriteFrames(&frames, CompletionOnceCallback()),
              IsOk());
  const std::vector<std::unique_ptr<WebSocketFrame>>& frames_passed =
      *stub.frames();
  ASSERT_EQ(5u, frames_passed.size());

  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames_passed[0]->header.opcode);
  EXPECT_FALSE(frames_passed[0]->header.final);
  EXPECT_FALSE(frames_passed[0]->header.reserved1);
  EXPECT_EQ("He", ToString(frames_passed[0]));
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeContinuation,
            frames_passed[1]->header.opcode);
  EXPECT_TRUE(frames_passed[1]->header.final);
  EXPECT_FALSE(frames_passed[1]->header.reserved1);
  EXPECT_EQ("llo", ToString(frames_passed[1]));

  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames_passed[2]->header.opcode);
  EXPECT_TRUE(frames_passed[2]->header.final);
  EXPECT_TRUE(frames_passed[2]->header.reserved1);
  EXPECT_EQ(std::string("\x72\x74\x44\x00\x00\x00", 6),
            ToString(frames_passed[2]));

  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames_passed[3]->header.opcode);
  EXPECT_FALSE(frames_passed[3]->header.final);
  EXPECT_FALSE(frames_passed[3]->header.reserved1);
  EXPECT_EQ("XX", ToString(frames_passed[3]));
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeContinuation,
            frames_passed[4]->header.opcode);
  EXPECT_TRUE(frames_passed[4]->header.final);
  EXPECT_FALSE(frames_passed[4]->header.reserved1);
  EXPECT_EQ("YY", ToString(frames_passed[4]));
}

// This is based on the similar test from websocket_deflater_test.cc
TEST_F(WebSocketDeflateStreamWithClientWindowBitsTest, WindowBits8) {
  SetUpWithWindowBits(8);
  AddCompressibleFrameString();
  WriteFramesStub stub(predictor_, OK);
  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, WriteFramesInternal(_, _))
        .WillOnce(Invoke(&stub, &WriteFramesStub::Call));
  }
  ASSERT_THAT(deflate_stream_->WriteFrames(&frames_, CompletionOnceCallback()),
              IsOk());
  const std::vector<std::unique_ptr<WebSocketFrame>>& frames_passed =
      *stub.frames();
  ASSERT_EQ(1u, frames_passed.size());
  EXPECT_EQ(std::string("r\xce(\xca\xcf\xcd,\xcdM\x1c\xe1\xc0\x39\xa3"
                        "(?7\xb3\x34\x17\x00", 21),
            ToString(frames_passed[0]));
}

// The same input with window_bits=10 returns smaller output.
TEST_F(WebSocketDeflateStreamWithClientWindowBitsTest, WindowBits10) {
  SetUpWithWindowBits(10);
  AddCompressibleFrameString();
  WriteFramesStub stub(predictor_, OK);
  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, WriteFramesInternal(_, _))
        .WillOnce(Invoke(&stub, &WriteFramesStub::Call));
  }
  ASSERT_THAT(deflate_stream_->WriteFrames(&frames_, CompletionOnceCallback()),
              IsOk());
  const std::vector<std::unique_ptr<WebSocketFrame>>& frames_passed =
      *stub.frames();
  ASSERT_EQ(1u, frames_passed.size());
  EXPECT_EQ(
      std::string("r\xce(\xca\xcf\xcd,\xcdM\x1c\xe1\xc0\x19\x1a\x0e\0\0", 17),
      ToString(frames_passed[0]));
}

}  // namespace

}  // namespace net
