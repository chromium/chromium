// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/soda_speech_recognition_engine_impl.h"

#include <array>
#include <memory>
#include <vector>

#include "base/containers/queue.h"
#include "base/run_loop.h"
#include "content/browser/speech/fake_speech_recognition_manager_delegate.h"
#include "content/browser/speech/speech_recognition_engine.h"
#include "content/browser/speech/speech_recognizer_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kFirstSpeechResult[] = "the brown fox";
const char kSecondSpeechResult[] = "the brown fox jumped over the lazy dog";
}  // namespace

using testing::_;

namespace content {

class SodaSpeechRecognitionEngineImplTest
    : public testing::Test,
      public SpeechRecognitionEngine::Delegate {
 public:
  SodaSpeechRecognitionEngineImplTest() = default;

  // testing::Test methods.
  void SetUp() override;
  void TearDown() override;

  // SpeechRecognitionRequestDelegate methods.
  void OnSpeechRecognitionEngineResults(
      const std::vector<media::mojom::WebSpeechRecognitionResultPtr>& results)
      override;
  void OnSpeechRecognitionEngineEndOfUtterance() override;
  void OnSpeechRecognitionEngineError(
      const media::mojom::SpeechRecognitionError& error) override;

  // context.
  std::unique_ptr<SodaSpeechRecognitionEngineImpl> CreateSpeechRecognition(
      SpeechRecognitionSessionConfig config,
      SpeechRecognitionManagerDelegate* speech_recognition_mgr_delegate,
      bool set_ready_cb);
  void OnSpeechRecognitionReady() { recognition_ready_ = true; }

  // operations.
  void SendDummyAudioChunk();
  void FillRecognitionExpectResults(
      std::vector<media::mojom::WebSpeechRecognitionResultPtr>& results,
      const char* transcription,
      bool is_final);
  void SendSpeechResult(const char* result, bool is_final);
  void SendTranscriptionError();
  void ExpectResultsReceived(
      const std::vector<media::mojom::WebSpeechRecognitionResultPtr>& results);
  bool ResultsAreEqual(
      const std::vector<media::mojom::WebSpeechRecognitionResultPtr>& a,
      const std::vector<media::mojom::WebSpeechRecognitionResultPtr>& b);

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<content::TestBrowserContext> browser_context_;
  std::unique_ptr<MockOnDeviceWebSpeechRecognitionService> mock_service_;
  std::unique_ptr<FakeSpeechRecognitionManagerDelegate>
      fake_speech_recognition_mgr_delegate_;
  std::unique_ptr<SodaSpeechRecognitionEngineImpl> client_under_test_;

  base::queue<std::vector<media::mojom::WebSpeechRecognitionResultPtr>>
      results_;
  media::mojom::SpeechRecognitionErrorCode error_;
  int end_of_utterance_counter_ = 0;
  bool recognition_ready_ = false;

  base::WeakPtrFactory<SodaSpeechRecognitionEngineImplTest> weak_factory_{this};
};

void SodaSpeechRecognitionEngineImplTest::SetUp() {
  error_ = media::mojom::SpeechRecognitionErrorCode::kNone;
  end_of_utterance_counter_ = 0;
  recognition_ready_ = false;
  browser_context_ = std::make_unique<content::TestBrowserContext>();
  mock_service_ = std::make_unique<MockOnDeviceWebSpeechRecognitionService>(
      browser_context_.get());
  fake_speech_recognition_mgr_delegate_ =
      std::make_unique<FakeSpeechRecognitionManagerDelegate>(
          mock_service_.get());
}

void SodaSpeechRecognitionEngineImplTest::TearDown() {}

void SodaSpeechRecognitionEngineImplTest::OnSpeechRecognitionEngineResults(
    const std::vector<media::mojom::WebSpeechRecognitionResultPtr>& results) {
  results_.push(mojo::Clone(results));
}

void SodaSpeechRecognitionEngineImplTest::
    OnSpeechRecognitionEngineEndOfUtterance() {
  ++end_of_utterance_counter_;
}

void SodaSpeechRecognitionEngineImplTest::OnSpeechRecognitionEngineError(
    const media::mojom::SpeechRecognitionError& error) {
  error_ = error.code;
}

std::unique_ptr<SodaSpeechRecognitionEngineImpl>
SodaSpeechRecognitionEngineImplTest::CreateSpeechRecognition(
    SpeechRecognitionSessionConfig config,
    SpeechRecognitionManagerDelegate* speech_recognition_mgr_delegate,
    bool set_ready_cb) {
  std::unique_ptr<SodaSpeechRecognitionEngineImpl> client_under_test =
      std::make_unique<SodaSpeechRecognitionEngineImpl>(config);

  client_under_test->set_delegate(this);
  if (set_ready_cb) {
    recognition_ready_ = false;
    client_under_test->SetOnReadyCallback(base::BindOnce(
        &SodaSpeechRecognitionEngineImplTest::OnSpeechRecognitionReady,
        weak_factory_.GetWeakPtr()));
  }
  client_under_test->SetSpeechRecognitionManagerDelegateForTesting(
      speech_recognition_mgr_delegate);

  int chunk_duration_ms = client_under_test->GetDesiredAudioChunkDurationMs();
  // Audio converter shall provide audio based on these parameters as output.
  // Hard coded, WebSpeech specific parameters are utilized here.
  int frames_per_buffer =
      (SpeechRecognizerImpl::kAudioSampleRate * chunk_duration_ms) / 1000;
  media::AudioParameters audio_parameters = media::AudioParameters(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::FromLayout<
          SpeechRecognizerImpl::kChannelLayout>(),
      SpeechRecognizerImpl::kAudioSampleRate, frames_per_buffer);
  client_under_test->SetAudioParameters(audio_parameters);

  return client_under_test;
}

void SodaSpeechRecognitionEngineImplTest::SendDummyAudioChunk() {
  // Enough data so that the encoder will output something, as can't read 0
  // bytes from a Mojo stream.
  std::array<unsigned char, 2000 * 2> dummy_audio_buffer_data = {'\0'};
  scoped_refptr<AudioChunk> dummy_audio_chunk(new AudioChunk(
      &dummy_audio_buffer_data[0], sizeof(dummy_audio_buffer_data),
      2 /* bytes per sample */));
  DCHECK(client_under_test_.get());
  base::RunLoop loop;
  client_under_test_->TakeAudioChunk(*dummy_audio_chunk.get());
  loop.RunUntilIdle();
}

void SodaSpeechRecognitionEngineImplTest::FillRecognitionExpectResults(
    std::vector<media::mojom::WebSpeechRecognitionResultPtr>& results,
    const char* transcription,
    bool is_final) {
  results.push_back(media::mojom::WebSpeechRecognitionResult::New());
  media::mojom::WebSpeechRecognitionResultPtr& result = results.back();
  result->is_provisional = !is_final;

  media::mojom::SpeechRecognitionHypothesisPtr hypothesis =
      media::mojom::SpeechRecognitionHypothesis::New();
  hypothesis->confidence = 1.0;
  hypothesis->utterance = base::UTF8ToUTF16(transcription);
  result->hypotheses.push_back(std::move(hypothesis));
}

void SodaSpeechRecognitionEngineImplTest::SendSpeechResult(const char* result,
                                                           bool is_final) {
  base::RunLoop loop;
  mock_service_->SendSpeechRecognitionResult(
      media::SpeechRecognitionResult(result, is_final));
  loop.RunUntilIdle();
}

void SodaSpeechRecognitionEngineImplTest::SendTranscriptionError() {
  base::RunLoop loop;
  mock_service_->SendSpeechRecognitionError();
  loop.RunUntilIdle();
}

void SodaSpeechRecognitionEngineImplTest::ExpectResultsReceived(
    const std::vector<media::mojom::WebSpeechRecognitionResultPtr>& results) {
  ASSERT_GE(1U, results_.size());
  ASSERT_TRUE(ResultsAreEqual(results, results_.front()));
  results_.pop();
}

bool SodaSpeechRecognitionEngineImplTest::ResultsAreEqual(
    const std::vector<media::mojom::WebSpeechRecognitionResultPtr>& a,
    const std::vector<media::mojom::WebSpeechRecognitionResultPtr>& b) {
  if (a.size() != b.size()) {
    return false;
  }

  auto it_a = a.begin();
  auto it_b = b.begin();
  for (; it_a != a.end() && it_b != b.end(); ++it_a, ++it_b) {
    if ((*it_a)->is_provisional != (*it_b)->is_provisional ||
        (*it_a)->hypotheses.size() != (*it_b)->hypotheses.size()) {
      return false;
    }
    for (size_t i = 0; i < (*it_a)->hypotheses.size(); ++i) {
      const media::mojom::SpeechRecognitionHypothesisPtr& hyp_a =
          (*it_a)->hypotheses[i];
      const media::mojom::SpeechRecognitionHypothesisPtr& hyp_b =
          (*it_b)->hypotheses[i];
      if (hyp_a->utterance != hyp_b->utterance ||
          hyp_a->confidence != hyp_b->confidence) {
        return false;
      }
    }
  }

  return true;
}

TEST_F(SodaSpeechRecognitionEngineImplTest, SpeechRecognitionResults) {
  SpeechRecognitionSessionConfig config;
  client_under_test_ = CreateSpeechRecognition(
      config, fake_speech_recognition_mgr_delegate_.get(), true);
  ASSERT_TRUE(client_under_test_->Initialize());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(recognition_ready_);

  EXPECT_CALL(*mock_service_, SendAudioToSpeechRecognitionService(_)).Times(2);

  client_under_test_->StartRecognition();
  SendDummyAudioChunk();

  std::vector<media::mojom::WebSpeechRecognitionResultPtr> first_results;
  FillRecognitionExpectResults(first_results, kFirstSpeechResult, false);
  SendSpeechResult(kFirstSpeechResult, /*is_final=*/false);
  ExpectResultsReceived(first_results);

  SendDummyAudioChunk();
  std::vector<media::mojom::WebSpeechRecognitionResultPtr> second_results;
  FillRecognitionExpectResults(second_results, kSecondSpeechResult, false);
  SendSpeechResult(kSecondSpeechResult, /*is_final=*/false);
  ExpectResultsReceived(second_results);

  SendTranscriptionError();
  ASSERT_EQ(media::mojom::SpeechRecognitionErrorCode::kNoSpeech, error_);
}

TEST_F(SodaSpeechRecognitionEngineImplTest, SpeechRecognitionAudioChunksEnded) {
  SpeechRecognitionSessionConfig config;
  client_under_test_ = CreateSpeechRecognition(
      config, fake_speech_recognition_mgr_delegate_.get(), true);
  ASSERT_TRUE(client_under_test_->Initialize());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(recognition_ready_);

  EXPECT_CALL(*mock_service_, SendAudioToSpeechRecognitionService(_)).Times(1);

  client_under_test_->StartRecognition();
  SendDummyAudioChunk();

  std::vector<media::mojom::WebSpeechRecognitionResultPtr> first_results;
  FillRecognitionExpectResults(first_results, kFirstSpeechResult, false);
  SendSpeechResult(kFirstSpeechResult, /*is_final=*/false);
  ExpectResultsReceived(first_results);

  base::RunLoop loop;
  client_under_test_->AudioChunksEnded();
  client_under_test_->EndRecognition();
  loop.RunUntilIdle();
  ASSERT_EQ(media::mojom::SpeechRecognitionErrorCode::kAborted, error_);
}

TEST_F(SodaSpeechRecognitionEngineImplTest, SpeechRecognitionEndOfUtterance) {
  SpeechRecognitionSessionConfig config;
  config.continuous = false;
  client_under_test_ = CreateSpeechRecognition(
      config, fake_speech_recognition_mgr_delegate_.get(), true);
  ASSERT_TRUE(client_under_test_->Initialize());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(recognition_ready_);

  EXPECT_CALL(*mock_service_, SendAudioToSpeechRecognitionService(_)).Times(1);

  client_under_test_->StartRecognition();
  SendDummyAudioChunk();

  std::vector<media::mojom::WebSpeechRecognitionResultPtr> first_results;
  FillRecognitionExpectResults(first_results, kFirstSpeechResult, false);
  SendSpeechResult(kFirstSpeechResult, /*is_final=*/false);
  ExpectResultsReceived(first_results);

  std::vector<media::mojom::WebSpeechRecognitionResultPtr> second_results;
  FillRecognitionExpectResults(second_results, kSecondSpeechResult, true);
  SendSpeechResult(kSecondSpeechResult, /*is_final=*/true);
  ExpectResultsReceived(second_results);

  ASSERT_EQ(1, end_of_utterance_counter_);
}

TEST_F(SodaSpeechRecognitionEngineImplTest, SpeechRecognitionEnd) {
  SpeechRecognitionSessionConfig config;
  config.continuous = false;
  client_under_test_ = CreateSpeechRecognition(
      config, fake_speech_recognition_mgr_delegate_.get(), true);
  ASSERT_TRUE(client_under_test_->Initialize());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(recognition_ready_);

  EXPECT_CALL(*mock_service_, SendAudioToSpeechRecognitionService(_)).Times(1);

  client_under_test_->StartRecognition();
  SendDummyAudioChunk();

  std::vector<media::mojom::WebSpeechRecognitionResultPtr> first_results;
  FillRecognitionExpectResults(first_results, kFirstSpeechResult, false);
  SendSpeechResult(kFirstSpeechResult, /*is_final=*/false);
  ExpectResultsReceived(first_results);

  client_under_test_->EndRecognition();
  SendDummyAudioChunk();

  ASSERT_EQ(media::mojom::SpeechRecognitionErrorCode::kNotAllowed, error_);
}

TEST_F(SodaSpeechRecognitionEngineImplTest, SetOnReadyCallbackAfterBind) {
  SpeechRecognitionSessionConfig config;
  client_under_test_ = CreateSpeechRecognition(
      config, fake_speech_recognition_mgr_delegate_.get(), false);
  ASSERT_TRUE(client_under_test_->Initialize());
  base::RunLoop().RunUntilIdle();

  recognition_ready_ = false;
  client_under_test_->SetOnReadyCallback(base::BindOnce(
      &SodaSpeechRecognitionEngineImplTest::OnSpeechRecognitionReady,
      weak_factory_.GetWeakPtr()));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(recognition_ready_);
}

}  // namespace content
