// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_recognition_engine.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/big_endian.h"
#include "base/containers/queue.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_byteorder.h"
#include "base/test/task_environment.h"
#include "content/browser/speech/audio_buffer.h"
#include "content/browser/speech/proto/google_streaming_api.pb.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/speech/speech_recognition_error.mojom.h"
#include "third_party/blink/public/mojom/speech/speech_recognition_result.mojom.h"

using base::HostToNet32;
using base::checked_cast;

namespace content {

// Frame types for framed POST data.
static const uint32_t kFrameTypePreamble = 0;
static const uint32_t kFrameTypeRecognitionAudio = 1;

// Note: the terms upstream and downstream are from the point-of-view of the
// client (engine_under_test_).

class SpeechRecognitionEngineTest
    : public SpeechRecognitionEngine::Delegate,
      public testing::Test {
 public:
  SpeechRecognitionEngineTest()
      : last_number_of_upstream_chunks_seen_(0U),
        error_(blink::mojom::SpeechRecognitionErrorCode::kNone),
        end_of_utterance_counter_(0) {}

  // SpeechRecognitionRequestDelegate methods.
  void OnSpeechRecognitionEngineResults(
      const std::vector<blink::mojom::SpeechRecognitionResultPtr>& results)
      override {
    results_.push(mojo::Clone(results));
  }
  void OnSpeechRecognitionEngineEndOfUtterance() override {
    ++end_of_utterance_counter_;
  }
  void OnSpeechRecognitionEngineError(
      const blink::mojom::SpeechRecognitionError& error) override {
    error_ = error.code;
  }

  // testing::Test methods.
  void SetUp() override;
  void TearDown() override;

 protected:
  enum DownstreamError {
    DOWNSTREAM_ERROR_NONE,
    DOWNSTREAM_ERROR_HTTP500,
    DOWNSTREAM_ERROR_NETWORK,
    DOWNSTREAM_ERROR_WEBSERVICE_NO_MATCH
  };
  static bool ResultsAreEqual(
      const std::vector<blink::mojom::SpeechRecognitionResultPtr>& a,
      const std::vector<blink::mojom::SpeechRecognitionResultPtr>& b);
  static std::string SerializeProtobufResponse(
      const proto::SpeechRecognitionEvent& msg);

  const network::TestURLLoaderFactory::PendingRequest* GetUpstreamRequest();
  const network::TestURLLoaderFactory::PendingRequest* GetDownstreamRequest();
  void StartMockRecognition();
  void EndMockRecognition();
  void InjectDummyAudioChunk();
  void ProvideMockResponseStartDownstreamIfNeeded();
  void ProvideMockProtoResultDownstream(
      const proto::SpeechRecognitionEvent& result);
  void ProvideMockResultDownstream(
      const blink::mojom::SpeechRecognitionResultPtr& result);
  void ExpectResultsReceived(
      const std::vector<blink::mojom::SpeechRecognitionResultPtr>& result);
  void ExpectFramedChunk(const std::string& chunk, uint32_t type);
  // Reads and returns all pending upload data from |upstream_data_pipe_|,
  // initializing the pipe from |GetUpstreamRequest()|, if needed.
  std::string ConsumeChunkedUploadData();
  void CloseMockDownstream(DownstreamError error);

  base::test::SingleThreadTaskEnvironment task_environment_;

  network::TestURLLoaderFactory url_loader_factory_;
  mojo::ScopedDataPipeProducerHandle downstream_data_pipe_;
  mojo::Remote<network::mojom::ChunkedDataPipeGetter> chunked_data_pipe_getter_;
  mojo::ScopedDataPipeConsumerHandle upstream_data_pipe_;

  std::unique_ptr<SpeechRecognitionEngine> engine_under_test_;
  size_t last_number_of_upstream_chunks_seen_;
  std::string response_buffer_;
  blink::mojom::SpeechRecognitionErrorCode error_;
  int end_of_utterance_counter_;
  base::queue<std::vector<blink::mojom::SpeechRecognitionResultPtr>> results_;
};

TEST_F(SpeechRecognitionEngineTest, SingleDefinitiveResult) {
  StartMockRecognition();
  ASSERT_TRUE(GetUpstreamRequest());
  ASSERT_EQ("", ConsumeChunkedUploadData());

  // Inject some dummy audio chunks and check a corresponding chunked upload
  // is performed every time on the server.
  for (int i = 0; i < 3; ++i) {
    InjectDummyAudioChunk();
    ASSERT_FALSE(ConsumeChunkedUploadData().empty());
  }

  // Ensure that a final (empty) audio chunk is uploaded on chunks end.
  engine_under_test_->AudioChunksEnded();
  ASSERT_FALSE(ConsumeChunkedUploadData().empty());
  ASSERT_TRUE(engine_under_test_->IsRecognitionPending());

  // Simulate a protobuf message streamed from the server containing a single
  // result with two hypotheses.
  std::vector<blink::mojom::SpeechRecognitionResultPtr> results;
  results.push_back(blink::mojom::SpeechRecognitionResult::New());
  blink::mojom::SpeechRecognitionResultPtr& result = results.back();
  result->is_provisional = false;
  result->hypotheses.push_back(blink::mojom::SpeechRecognitionHypothesis::New(
      base::UTF8ToUTF16("hypothesis 1"), 0.1F));
  result->hypotheses.push_back(blink::mojom::SpeechRecognitionHypothesis::New(
      base::UTF8ToUTF16("hypothesis 2"), 0.2F));

  ProvideMockResultDownstream(result);
  ExpectResultsReceived(results);
  ASSERT_TRUE(engine_under_test_->IsRecognitionPending());

  // Ensure everything is closed cleanly after the downstream is closed.
  CloseMockDownstream(DOWNSTREAM_ERROR_NONE);
  ASSERT_FALSE(engine_under_test_->IsRecognitionPending());
  EndMockRecognition();
  ASSERT_EQ(blink::mojom::SpeechRecognitionErrorCode::kNone, error_);
  ASSERT_EQ(0U, results_.size());
}

TEST_F(SpeechRecognitionEngineTest, SeveralStreamingResults) {
  StartMockRecognition();
  ASSERT_TRUE(GetUpstreamRequest());
  ASSERT_EQ("", ConsumeChunkedUploadData());

  for (int i = 0; i < 4; ++i) {
    InjectDummyAudioChunk();
    ASSERT_NE("", ConsumeChunkedUploadData());

    std::vector<blink::mojom::SpeechRecognitionResultPtr> results;
    results.push_back(blink::mojom::SpeechRecognitionResult::New());
    blink::mojom::SpeechRecognitionResultPtr& result = results.back();
    result->is_provisional = (i % 2 == 0);  // Alternate result types.
    float confidence = result->is_provisional ? 0.0F : (i * 0.1F);
    result->hypotheses.push_back(blink::mojom::SpeechRecognitionHypothesis::New(
        base::UTF8ToUTF16("hypothesis"), confidence));

    ProvideMockResultDownstream(result);
    ExpectResultsReceived(results);
    ASSERT_TRUE(engine_under_test_->IsRecognitionPending());
  }

  // Ensure that a final (empty) audio chunk is uploaded on chunks end.
  engine_under_test_->AudioChunksEnded();
  ASSERT_NE("", ConsumeChunkedUploadData());
  ASSERT_TRUE(engine_under_test_->IsRecognitionPending());

  // Simulate a final definitive result.
  std::vector<blink::mojom::SpeechRecognitionResultPtr> results;
  results.push_back(blink::mojom::SpeechRecognitionResult::New());
  blink::mojom::SpeechRecognitionResultPtr& result = results.back();
  result->is_provisional = false;
  result->hypotheses.push_back(blink::mojom::SpeechRecognitionHypothesis::New(
      base::UTF8ToUTF16("The final result"), 1.0F));
  ProvideMockResultDownstream(result);
  ExpectResultsReceived(results);
  ASSERT_TRUE(engine_under_test_->IsRecognitionPending());

  // Ensure everything is closed cleanly after the downstream is closed.
  CloseMockDownstream(DOWNSTREAM_ERROR_NONE);
  ASSERT_FALSE(engine_under_test_->IsRecognitionPending());
  EndMockRecognition();
  ASSERT_EQ(blink::mojom::SpeechRecognitionErrorCode::kNone, error_);
  ASSERT_EQ(0U, results_.size());
}

TEST_F(SpeechRecognitionEngineTest, NoFinalResultAfterAudioChunksEnded) {
  StartMockRecognition();
  ASSERT_TRUE(GetUpstreamRequest());
  ASSERT_EQ("", ConsumeChunkedUploadData());

  // Simulate one pushed audio chunk.
  InjectDummyAudioChunk();
  ASSERT_NE("", ConsumeChunkedUploadData());

  // Simulate the corresponding definitive result.
  std::vector<blink::mojom::SpeechRecognitionResultPtr> results;
  results.push_back(blink::mojom::SpeechRecognitionResult::New());
  blink::mojom::SpeechRecognitionResultPtr& result = results.back();
  result->hypotheses.push_back(blink::mojom::SpeechRecognitionHypothesis::New(
      base::UTF8ToUTF16("hypothesis"), 1.0F));
  ProvideMockResultDownstream(result);
  ExpectResultsReceived(results);
  ASSERT_TRUE(engine_under_test_->IsRecognitionPending());

  // Simulate a silent downstream closure after |AudioChunksEnded|.
  engine_under_test_->AudioChunksEnded();
  ASSERT_NE("", ConsumeChunkedUploadData());
  ASSERT_TRUE(engine_under_test_->IsRecognitionPending());
  CloseMockDownstream(DOWNSTREAM_ERROR_NONE);

  // Expect an empty result, aimed at notifying recognition ended with no
  // actual results nor errors.
  std::vector<blink::mojom::SpeechRecognitionResultPtr> empty_results;
  ExpectResultsReceived(empty_results);

  // Ensure everything is closed cleanly after the downstream is closed.
  ASSERT_FALSE(engine_under_test_->IsRecognitionPending());
  EndMockRecognition();
  ASSERT_EQ(blink::mojom::SpeechRecognitionErrorCode::kNone, error_);
  ASSERT_EQ(0U, results_.size());
}

// Simulate the network service repeatedly re-requesting data (Possibly due to
// using a stale socket, for instance).
TEST_F(SpeechRecognitionEngineTest, ReRequestData) {
  StartMockRecognition();
  ASSERT_TRUE(GetUpstreamRequest());
  ASSERT_EQ("", ConsumeChunkedUploadData());

  // Simulate one pushed audio chunk.
  InjectDummyAudioChunk();
  std::string uploaded_data = ConsumeChunkedUploadData();
  ASSERT_NE(uploaded_data, ConsumeChunkedUploadData());

  // The network service closes the data pipe.
  upstream_data_pipe_.reset();

  // Re-opening the data pipe should result in the data being re-uploaded.
  ASSERT_EQ(uploaded_data, ConsumeChunkedUploadData());

  // Simulate the corresponding definitive result.
  std::vector<blink::mojom::SpeechRecognitionResultPtr> results;
  results.push_back(blink::mojom::SpeechRecognitionResult::New());
  blink::mojom::SpeechRecognitionResultPtr& result = results.back();
  result->hypotheses.push_back(blink::mojom::SpeechRecognitionHypothesis::New(
      base::UTF8ToUTF16("hypothesis"), 1.0F));
  ProvideMockResultDownstream(result);
  ExpectResultsReceived(results);
  ASSERT_TRUE(engine_under_test_->IsRecognitionPending());

  // Simulate a silent downstream closure after |AudioChunksEnded|.
  engine_under_test_->AudioChunksEnded();
  std::string new_uploaded_data = ConsumeChunkedUploadData();
  ASSERT_NE(new_uploaded_data, ConsumeChunkedUploadData());
  uploaded_data += new_uploaded_data;

  // The network service closes the data pipe.
  upstream_data_pipe_.reset();

  // Re-opening the data pipe should result in the data being re-uploaded.
  ASSERT_EQ(uploaded_data, ConsumeChunkedUploadData());

  ASSERT_TRUE(engine_under_test_->IsRecognitionPending());
  CloseMockDownstream(DOWNSTREAM_ERROR_NONE);

  // Expect an empty result, aimed at notifying recognition ended with no
  // actual results nor errors.
  std::vector<blink::mojom::SpeechRecognitionResultPtr> empty_results;
  ExpectResultsReceived(empty_results);

  // Ensure everything is closed cleanly after the downstream is closed.
  ASSERT_FALSE(engine_under_test_->IsRecognitionPending());
  EndMockRecognition();
  ASSERT_EQ(blink::mojom::SpeechRecognitionErrorCode::kNone, error_);
  ASSERT_EQ(0U, results_.size());
}

TEST_F(SpeechRecognitionEngineTest, NoMatchError) {
  StartMockRecognition();
  ASSERT_TRUE(GetUpstreamRequest());
  ASSERT_EQ("", ConsumeChunkedUploadData());

  for (int i = 0; i < 3; ++i) {
    InjectDummyAudioChunk();
    ASSERT_NE("", ConsumeChunkedUploadData());
  }
  engine_under_test_->AudioChunksEnded();
  ASSERT_NE("", ConsumeChunkedUploadData());
  ASSERT_TRUE(engine_under_test_->IsRecognitionPending());

  // Simulate only a provisional result.
  std::vector<blink::mojom::SpeechRecognitionResultPtr> results;
  results.push_back(blink::mojom::SpeechRecognitionResult::New());
  blink::mojom::SpeechRecognitionResultPtr& result = results.back();
  result->is_provisional = true;
  result->hypotheses.push_back(blink::mojom::SpeechRecognitionHypothesis::New(
      base::UTF8ToUTF16("The final result"), 0.0F));
  ProvideMockResultDownstream(result);
  ExpectResultsReceived(results);
  ASSERT_TRUE(engine_under_test_->IsRecognitionPending());

  CloseMockDownstream(DOWNSTREAM_ERROR_WEBSERVICE_NO_MATCH);

  // Expect an empty result.
  ASSERT_FALSE(engine_under_test_->IsRecognitionPending());
  EndMockRecognition();
  std::vector<blink::mojom::SpeechRecognitionResultPtr> empty_result;
  ExpectResultsReceived(empty_result);
}

TEST_F(SpeechRecognitionEngineTest, HTTPError) {
  StartMockRecognition();
  ASSERT_TRUE(GetUpstreamRequest());
  ASSERT_EQ("", ConsumeChunkedUploadData());

  InjectDummyAudioChunk();
  ASSERT_NE("", ConsumeChunkedUploadData());

  // Close the downstream with a HTTP 500 error.
  CloseMockDownstream(DOWNSTREAM_ERROR_HTTP500);

  // Expect a blink::mojom::SpeechRecognitionErrorCode::kNetwork error to be
  // raised.
  ASSERT_FALSE(engine_under_test_->IsRecognitionPending());
  EndMockRecognition();
  ASSERT_EQ(blink::mojom::SpeechRecognitionErrorCode::kNetwork, error_);
  ASSERT_EQ(0U, results_.size());
}

TEST_F(SpeechRecognitionEngineTest, NetworkError) {
  StartMockRecognition();
  ASSERT_TRUE(GetUpstreamRequest());
  ASSERT_EQ("", ConsumeChunkedUploadData());

  InjectDummyAudioChunk();
  ASSERT_NE("", ConsumeChunkedUploadData());

  // Close the downstream fetcher simulating a network failure.
  CloseMockDownstream(DOWNSTREAM_ERROR_NETWORK);

  // Expect a blink::mojom::SpeechRecognitionErrorCode::kNetwork error to be
  // raised.
  ASSERT_FALSE(engine_under_test_->IsRecognitionPending());
  EndMockRecognition();
  ASSERT_EQ(blink::mojom::SpeechRecognitionErrorCode::kNetwork, error_);
  ASSERT_EQ(0U, results_.size());
}

TEST_F(SpeechRecognitionEngineTest, Stability) {
  StartMockRecognition();
  ASSERT_TRUE(GetUpstreamRequest());
  ASSERT_EQ("", ConsumeChunkedUploadData());

  // Upload a dummy audio chunk.
  InjectDummyAudioChunk();
  ASSERT_NE("", ConsumeChunkedUploadData());
  engine_under_test_->AudioChunksEnded();

  // Simulate a protobuf message with an intermediate result without confidence,
  // but with stability.
  proto::SpeechRecognitionEvent proto_event;
  proto_event.set_status(proto::SpeechRecognitionEvent::STATUS_SUCCESS);
  proto::SpeechRecognitionResult* proto_result = proto_event.add_result();
  proto_result->set_stability(0.5);
  proto::SpeechRecognitionAlternative *proto_alternative =
      proto_result->add_alternative();
  proto_alternative->set_transcript("foo");
  ProvideMockProtoResultDownstream(proto_event);

  // Set up expectations.
  std::vector<blink::mojom::SpeechRecognitionResultPtr> results;
  results.push_back(blink::mojom::SpeechRecognitionResult::New());
  blink::mojom::SpeechRecognitionResultPtr& result = results.back();
  result->is_provisional = true;
  result->hypotheses.push_back(blink::mojom::SpeechRecognitionHypothesis::New(
      base::UTF8ToUTF16("foo"), 0.5));

  // Check that the protobuf generated the expected result.
  ExpectResultsReceived(results);

  // Since it was a provisional result, recognition is still pending.
  ASSERT_TRUE(engine_under_test_->IsRecognitionPending());

  // Shut down.
  CloseMockDownstream(DOWNSTREAM_ERROR_NONE);
  ASSERT_FALSE(engine_under_test_->IsRecognitionPending());
  EndMockRecognition();

  // Since there was no final result, we get an empty "no match" result.
  std::vector<blink::mojom::SpeechRecognitionResultPtr> empty_result;
  ExpectResultsReceived(empty_result);
  ASSERT_EQ(blink::mojom::SpeechRecognitionErrorCode::kNone, error_);
  ASSERT_EQ(0U, results_.size());
}

TEST_F(SpeechRecognitionEngineTest, EndOfUtterance) {
  StartMockRecognition();
  ASSERT_TRUE(GetUpstreamRequest());

  // Simulate a END_OF_UTTERANCE proto event with continuous true.
  SpeechRecognitionEngine::Config config;
  config.continuous = true;
  engine_under_test_->SetConfig(config);
  proto::SpeechRecognitionEvent proto_event;
  proto_event.set_endpoint(proto::SpeechRecognitionEvent::END_OF_UTTERANCE);
  ASSERT_EQ(0, end_of_utterance_counter_);
  ProvideMockProtoResultDownstream(proto_event);
  ASSERT_EQ(0, end_of_utterance_counter_);

  // Simulate a END_OF_UTTERANCE proto event with continuous false.
  config.continuous = false;
  engine_under_test_->SetConfig(config);
  ProvideMockProtoResultDownstream(proto_event);
  ASSERT_EQ(1, end_of_utterance_counter_);

  // Shut down.
  CloseMockDownstream(DOWNSTREAM_ERROR_NONE);
  EndMockRecognition();
}

TEST_F(SpeechRecognitionEngineTest, SendPreamble) {
  const size_t kPreambleLength = 100;
  scoped_refptr<SpeechRecognitionSessionPreamble> preamble =
      new SpeechRecognitionSessionPreamble();
  preamble->sample_rate = 16000;
  preamble->sample_depth = 2;
  preamble->sample_data.assign(kPreambleLength, 0);
  SpeechRecognitionEngine::Config config;
  config.auth_token = "foo";
  config.auth_scope = "bar";
  config.preamble = preamble;
  engine_under_test_->SetConfig(config);

  StartMockRecognition();
  ASSERT_TRUE(GetUpstreamRequest());
  // First chunk uploaded should be the preamble.
  std::string chunk = ConsumeChunkedUploadData();
  ASSERT_NE("", chunk);
  ExpectFramedChunk(chunk, kFrameTypePreamble);

  for (int i = 0; i < 3; ++i) {
    InjectDummyAudioChunk();
    chunk = ConsumeChunkedUploadData();
    ASSERT_NE("", chunk);
    ExpectFramedChunk(chunk, kFrameTypeRecognitionAudio);
  }
  engine_under_test_->AudioChunksEnded();
  ASSERT_TRUE(engine_under_test_->IsRecognitionPending());

  // Simulate a protobuf message streamed from the server containing a single
  // result with one hypotheses.
  std::vector<blink::mojom::SpeechRecognitionResultPtr> results;
  results.push_back(blink::mojom::SpeechRecognitionResult::New());
  blink::mojom::SpeechRecognitionResultPtr& result = results.back();
  result->is_provisional = false;
  result->hypotheses.push_back(blink::mojom::SpeechRecognitionHypothesis::New(
      base::UTF8ToUTF16("hypothesis 1"), 0.1F));

  ProvideMockResultDownstream(result);
  ExpectResultsReceived(results);
  ASSERT_TRUE(engine_under_test_->IsRecognitionPending());

  // Ensure everything is closed cleanly after the downstream is closed.
  CloseMockDownstream(DOWNSTREAM_ERROR_NONE);
  ASSERT_FALSE(engine_under_test_->IsRecognitionPending());
  EndMockRecognition();
  ASSERT_EQ(blink::mojom::SpeechRecognitionErrorCode::kNone, error_);
  ASSERT_EQ(0U, results_.size());
}

void SpeechRecognitionEngineTest::SetUp() {
  engine_under_test_.reset(new SpeechRecognitionEngine(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &url_loader_factory_),
      "" /* accept_language */));
  engine_under_test_->set_delegate(this);
}

void SpeechRecognitionEngineTest::TearDown() {
  engine_under_test_.reset();
}

const network::TestURLLoaderFactory::PendingRequest*
SpeechRecognitionEngineTest::GetUpstreamRequest() {
  for (const auto& pending_request : *url_loader_factory_.pending_requests()) {
    if (pending_request.request.url.spec().find("/up") != std::string::npos)
      return &pending_request;
  }
  return nullptr;
}

const network::TestURLLoaderFactory::PendingRequest*
SpeechRecognitionEngineTest::GetDownstreamRequest() {
  for (const auto& pending_request : *url_loader_factory_.pending_requests()) {
    if (pending_request.request.url.spec().find("/down") != std::string::npos)
      return &pending_request;
  }
  return nullptr;
}

// Starts recognition on the engine, ensuring that both stream fetchers are
// created.
void SpeechRecognitionEngineTest::StartMockRecognition() {
  DCHECK(engine_under_test_.get());

  ASSERT_FALSE(engine_under_test_->IsRecognitionPending());

  engine_under_test_->StartRecognition();
  ASSERT_TRUE(engine_under_test_->IsRecognitionPending());

  ASSERT_TRUE(GetUpstreamRequest());
  ASSERT_TRUE(GetDownstreamRequest());
}

void SpeechRecognitionEngineTest::EndMockRecognition() {
  DCHECK(engine_under_test_.get());
  engine_under_test_->EndRecognition();
  ASSERT_FALSE(engine_under_test_->IsRecognitionPending());

  // TODO(primiano): In order to be very pedantic we should check that both the
  // upstream and downstream URL fetchers have been disposed at this time.
  // Unfortunately it seems that there is no direct way to detect (in tests)
  // if a url_fetcher has been freed or not, since they are not automatically
  // de-registered from the TestURLFetcherFactory on destruction.
}

void SpeechRecognitionEngineTest::InjectDummyAudioChunk() {
  // Enough data so that the encoder will output something, as can't read 0
  // bytes from a Mojo stream.
  unsigned char dummy_audio_buffer_data[2000 * 2] = {'\0'};
  scoped_refptr<AudioChunk> dummy_audio_chunk(
      new AudioChunk(&dummy_audio_buffer_data[0],
                     sizeof(dummy_audio_buffer_data),
                     2 /* bytes per sample */));
  DCHECK(engine_under_test_.get());
  engine_under_test_->TakeAudioChunk(*dummy_audio_chunk.get());
}

void SpeechRecognitionEngineTest::ProvideMockResponseStartDownstreamIfNeeded() {
  if (downstream_data_pipe_.get())
    return;
  const network::TestURLLoaderFactory::PendingRequest* downstream_request =
      GetDownstreamRequest();
  ASSERT_TRUE(downstream_request);

  network::ResourceResponseHead head;
  std::string headers("HTTP/1.1 200 OK\n\n");
  head.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  downstream_request->client->OnReceiveResponse(head);

  mojo::DataPipe data_pipe;
  downstream_request->client->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  downstream_data_pipe_ = std::move(data_pipe.producer_handle);
}

void SpeechRecognitionEngineTest::ProvideMockProtoResultDownstream(
    const proto::SpeechRecognitionEvent& result) {
  ProvideMockResponseStartDownstreamIfNeeded();
  ASSERT_TRUE(downstream_data_pipe_.get());
  ASSERT_TRUE(downstream_data_pipe_.is_valid());

  std::string response_string = SerializeProtobufResponse(result);
  response_buffer_.append(response_string);
  uint32_t written = 0;
  while (written < response_string.size()) {
    uint32_t write_bytes = response_string.size() - written;
    MojoResult result = downstream_data_pipe_->WriteData(
        response_string.data() + written, &write_bytes,
        MOJO_WRITE_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_OK) {
      written += write_bytes;
      continue;
    }
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      base::RunLoop().RunUntilIdle();
      continue;
    }

    FAIL() << "Mojo pipe unexpectedly closed";
  }
  base::RunLoop().RunUntilIdle();
}

void SpeechRecognitionEngineTest::ProvideMockResultDownstream(
    const blink::mojom::SpeechRecognitionResultPtr& result) {
  proto::SpeechRecognitionEvent proto_event;
  proto_event.set_status(proto::SpeechRecognitionEvent::STATUS_SUCCESS);
  proto::SpeechRecognitionResult* proto_result = proto_event.add_result();
  proto_result->set_final(!result->is_provisional);
  for (size_t i = 0; i < result->hypotheses.size(); ++i) {
    proto::SpeechRecognitionAlternative* proto_alternative =
        proto_result->add_alternative();
    const blink::mojom::SpeechRecognitionHypothesisPtr& hypothesis =
        result->hypotheses[i];
    proto_alternative->set_confidence(hypothesis->confidence);
    proto_alternative->set_transcript(base::UTF16ToUTF8(hypothesis->utterance));
  }
  ProvideMockProtoResultDownstream(proto_event);
  base::RunLoop().RunUntilIdle();
}

void SpeechRecognitionEngineTest::CloseMockDownstream(
    DownstreamError error) {
  if (error == DOWNSTREAM_ERROR_HTTP500) {
    // Can't provide a network error if already gave the consumer a 200
    // response.
    ASSERT_FALSE(downstream_data_pipe_.get());

    const network::TestURLLoaderFactory::PendingRequest* downstream_request =
        GetDownstreamRequest();
    ASSERT_TRUE(downstream_request);
    network::ResourceResponseHead head;
    std::string headers("HTTP/1.1 500 Server Sad\n\n");
    head.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(headers));
    downstream_request->client->OnReceiveResponse(head);
    // Wait for the response to be handled.
    base::RunLoop().RunUntilIdle();
    return;
  }

  ProvideMockResponseStartDownstreamIfNeeded();
  const network::TestURLLoaderFactory::PendingRequest* downstream_request =
      GetDownstreamRequest();
  ASSERT_TRUE(downstream_request);

  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = response_buffer_.size();
  status.error_code =
      (error == DOWNSTREAM_ERROR_NETWORK) ? net::ERR_FAILED : net::OK;
  downstream_request->client->OnComplete(status);
  downstream_data_pipe_.reset();
  // Wait for the completion events to be handled.
  base::RunLoop().RunUntilIdle();
}

void SpeechRecognitionEngineTest::ExpectResultsReceived(
    const std::vector<blink::mojom::SpeechRecognitionResultPtr>& results) {
  ASSERT_GE(1U, results_.size());
  ASSERT_TRUE(ResultsAreEqual(results, results_.front()));
  results_.pop();
}

bool SpeechRecognitionEngineTest::ResultsAreEqual(
    const std::vector<blink::mojom::SpeechRecognitionResultPtr>& a,
    const std::vector<blink::mojom::SpeechRecognitionResultPtr>& b) {
  if (a.size() != b.size())
    return false;

  auto it_a = a.begin();
  auto it_b = b.begin();
  for (; it_a != a.end() && it_b != b.end(); ++it_a, ++it_b) {
    if ((*it_a)->is_provisional != (*it_b)->is_provisional ||
        (*it_a)->hypotheses.size() != (*it_b)->hypotheses.size()) {
      return false;
    }
    for (size_t i = 0; i < (*it_a)->hypotheses.size(); ++i) {
      const blink::mojom::SpeechRecognitionHypothesisPtr& hyp_a =
          (*it_a)->hypotheses[i];
      const blink::mojom::SpeechRecognitionHypothesisPtr& hyp_b =
          (*it_b)->hypotheses[i];
      if (hyp_a->utterance != hyp_b->utterance ||
          hyp_a->confidence != hyp_b->confidence) {
        return false;
      }
    }
  }

  return true;
}

void SpeechRecognitionEngineTest::ExpectFramedChunk(
    const std::string& chunk, uint32_t type) {
  uint32_t value;
  base::ReadBigEndian(&chunk[0], &value);
  EXPECT_EQ(chunk.size() - 8, value);
  base::ReadBigEndian(&chunk[4], &value);
  EXPECT_EQ(type, value);
}

std::string SpeechRecognitionEngineTest::ConsumeChunkedUploadData() {
  std::string result;
  base::RunLoop().RunUntilIdle();

  if (!upstream_data_pipe_.get()) {
    if (!chunked_data_pipe_getter_) {
      const network::TestURLLoaderFactory::PendingRequest* upstream_request =
          GetUpstreamRequest();
      EXPECT_TRUE(upstream_request);
      EXPECT_TRUE(upstream_request->request.request_body);
      EXPECT_EQ(1u, upstream_request->request.request_body->elements()->size());
      EXPECT_EQ(
          network::mojom::DataElementType::kChunkedDataPipe,
          (*upstream_request->request.request_body->elements())[0].type());
      network::TestURLLoaderFactory::PendingRequest* mutable_upstream_request =
          const_cast<network::TestURLLoaderFactory::PendingRequest*>(
              upstream_request);
      chunked_data_pipe_getter_.Bind((*mutable_upstream_request->request
                                           .request_body->elements_mutable())[0]
                                         .ReleaseChunkedDataPipeGetter());
    }
    mojo::DataPipe data_pipe;
    chunked_data_pipe_getter_->StartReading(
        std::move(data_pipe.producer_handle));
    upstream_data_pipe_ = std::move(data_pipe.consumer_handle);
  }
  EXPECT_TRUE(upstream_data_pipe_.is_valid());

  std::string out;
  while (true) {
    base::RunLoop().RunUntilIdle();

    const void* data;
    uint32_t num_bytes;
    MojoResult result = upstream_data_pipe_->BeginReadData(
        &data, &num_bytes, MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_OK) {
      out.append(static_cast<const char*>(data), num_bytes);
      upstream_data_pipe_->EndReadData(num_bytes);
      continue;
    }
    if (result == MOJO_RESULT_SHOULD_WAIT)
      break;

    ADD_FAILURE() << "Mojo pipe unexpectedly closed";
    break;
  }
  return out;
}

std::string SpeechRecognitionEngineTest::SerializeProtobufResponse(
    const proto::SpeechRecognitionEvent& msg) {
  std::string msg_string;
  msg.SerializeToString(&msg_string);

  // Prepend 4 byte prefix length indication to the protobuf message as
  // envisaged by the google streaming recognition webservice protocol.
  uint32_t prefix = HostToNet32(checked_cast<uint32_t>(msg_string.size()));
  msg_string.insert(0, reinterpret_cast<char*>(&prefix), sizeof(prefix));

  return msg_string;
}

}  // namespace content
