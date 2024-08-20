// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <cstdint>
#include <list>
#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "remoting/client/audio/audio_jitter_buffer.h"
#include "remoting/client/audio/audio_stream_format.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

constexpr AudioPacket::BytesPerSample kBytesPerSample =
    AudioPacket::BYTES_PER_SAMPLE_2;
constexpr AudioPacket::Channels kChannels = AudioPacket::CHANNELS_STEREO;
constexpr uint32_t kAudioSampleBytes = uint32_t{kChannels} * kBytesPerSample;

constexpr uint32_t kNumConsumerBuffers = 3;
constexpr uint32_t kConsumerBufferMaxByteSize = 5000 * kAudioSampleBytes;

constexpr uint8_t kDefaultBufferData = 0x5A;
constexpr uint8_t kDummyAudioData = 0x8B;

std::unique_ptr<AudioPacket> CreateAudioPacketWithSamplingRate(
    AudioPacket::SamplingRate rate,
    size_t bytes) {
  std::unique_ptr<AudioPacket> packet = std::make_unique<AudioPacket>();
  packet->set_encoding(AudioPacket::ENCODING_RAW);
  packet->set_sampling_rate(rate);
  packet->set_bytes_per_sample(kBytesPerSample);
  packet->set_channels(kChannels);

  std::string data;
  data.resize(bytes, kDummyAudioData);
  packet->add_data(data);

  return packet;
}

// Check that the first |bytes_written| bytes are filled with audio data and
// the rest of the buffer is unchanged.
void CheckDataBytes(const uint8_t* buffer, size_t bytes_written) {
  uint32_t i = 0;
  for (; i < bytes_written; i++) {
    ASSERT_EQ(kDummyAudioData, *(buffer + i));
  }
  // Rest of audio frame must be unchanged.
  for (; i < kConsumerBufferMaxByteSize; i++) {
    ASSERT_EQ(kDefaultBufferData, *(buffer + i));
  }
}

}  // namespace

class AudioJitterBufferTest : public ::testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;

  void SetSampleRate(AudioPacket::SamplingRate sample_rate);
  std::unique_ptr<AudioPacket> CreatePacket(int time_ms);
  void AsyncConsumeData(size_t duration);
  void VerifyStreamFormat();
  void VerifyBuffersNotLost();
  size_t ByteFromTime(int time_ms) const;
  size_t GetNumQueuedPackets() const;
  int GetNumQueuedTime() const;
  size_t GetNumQueuedRequests() const;

  std::unique_ptr<AudioJitterBuffer> audio_;
  std::list<std::unique_ptr<uint8_t[]>> consumer_buffers_;

 private:
  class SimpleGetDataRequest;

  void OnFormatChanged(const AudioStreamFormat& format);

  AudioPacket::SamplingRate sample_rate_;
  std::unique_ptr<AudioStreamFormat> stream_format_;
};

class AudioJitterBufferTest::SimpleGetDataRequest
    : public AsyncAudioDataSupplier::GetDataRequest {
 public:
  SimpleGetDataRequest(AudioJitterBufferTest* test, size_t bytes_to_write);
  ~SimpleGetDataRequest() override;

  void OnDataFilled() override;

 private:
  raw_ptr<AudioJitterBufferTest> test_;
  std::unique_ptr<uint8_t[]> buffer_;
  size_t bytes_to_write_;
};

// Test fixture definitions

void AudioJitterBufferTest::SetUp() {
  audio_ = std::make_unique<AudioJitterBuffer>(base::BindRepeating(
      &AudioJitterBufferTest::OnFormatChanged, base::Unretained(this)));
  consumer_buffers_.clear();
  for (uint32_t i = 0u; i < kNumConsumerBuffers; i++) {
    consumer_buffers_.push_back(
        std::make_unique<uint8_t[]>(kConsumerBufferMaxByteSize));
  }
  SetSampleRate(AudioPacket::SAMPLING_RATE_48000);
}

void AudioJitterBufferTest::TearDown() {
  VerifyBuffersNotLost();
  audio_.reset();
  consumer_buffers_.clear();
}

void AudioJitterBufferTest::SetSampleRate(
    AudioPacket::SamplingRate sample_rate) {
  sample_rate_ = sample_rate;
}

std::unique_ptr<AudioPacket> AudioJitterBufferTest::CreatePacket(int time_ms) {
  return CreateAudioPacketWithSamplingRate(sample_rate_, ByteFromTime(time_ms));
}

void AudioJitterBufferTest::AsyncConsumeData(size_t duration) {
  size_t bytes_to_write = ByteFromTime(duration);
  ASSERT_LE(bytes_to_write, kConsumerBufferMaxByteSize);
  ASSERT_FALSE(consumer_buffers_.empty());
  audio_->AsyncGetData(
      std::make_unique<SimpleGetDataRequest>(this, bytes_to_write));
}

void AudioJitterBufferTest::VerifyStreamFormat() {
  ASSERT_TRUE(stream_format_);
  ASSERT_EQ(kBytesPerSample, stream_format_->bytes_per_sample);
  ASSERT_EQ(kChannels, stream_format_->channels);
  ASSERT_EQ(sample_rate_, stream_format_->sample_rate);
}

void AudioJitterBufferTest::VerifyBuffersNotLost() {
  size_t queued_requests = GetNumQueuedRequests();
  ASSERT_EQ(kNumConsumerBuffers, queued_requests + consumer_buffers_.size());
}

size_t AudioJitterBufferTest::ByteFromTime(int time_ms) const {
  return time_ms * sample_rate_ * kAudioSampleBytes /
         base::Time::kMillisecondsPerSecond;
}

size_t AudioJitterBufferTest::GetNumQueuedPackets() const {
  return audio_->queued_packets_.size();
}

int AudioJitterBufferTest::GetNumQueuedTime() const {
  return audio_->queued_bytes_ * base::Time::kMillisecondsPerSecond /
         kAudioSampleBytes / sample_rate_;
}

size_t AudioJitterBufferTest::GetNumQueuedRequests() const {
  return audio_->queued_requests_.size();
}
void AudioJitterBufferTest::OnFormatChanged(const AudioStreamFormat& format) {
  stream_format_ = std::make_unique<AudioStreamFormat>(format);
}

// SimpleGetDataRequest definitions

AudioJitterBufferTest::SimpleGetDataRequest::SimpleGetDataRequest(
    AudioJitterBufferTest* test,
    size_t bytes_to_write)
    : GetDataRequest(test->consumer_buffers_.front().get(), bytes_to_write),
      test_(test),
      buffer_(std::move(test->consumer_buffers_.front())),
      bytes_to_write_(bytes_to_write) {
  test_->consumer_buffers_.pop_front();
  memset(buffer_.get(), kDefaultBufferData, kConsumerBufferMaxByteSize);
}

AudioJitterBufferTest::SimpleGetDataRequest::~SimpleGetDataRequest() {
  if (buffer_) {
    test_->consumer_buffers_.push_back(std::move(buffer_));
  }
}

void AudioJitterBufferTest::SimpleGetDataRequest::OnDataFilled() {
  CheckDataBytes(buffer_.get(), bytes_to_write_);
  test_->consumer_buffers_.push_back(std::move(buffer_));
}

// Test cases

TEST_F(AudioJitterBufferTest, Init) {
  ASSERT_EQ(0u, GetNumQueuedPackets());

  audio_->AddAudioPacket(CreatePacket(20));
  ASSERT_EQ(1u, GetNumQueuedPackets());
  VerifyStreamFormat();
}

TEST_F(AudioJitterBufferTest, MultipleSamples) {
  audio_->AddAudioPacket(CreatePacket(10));
  ASSERT_EQ(10, GetNumQueuedTime());
  ASSERT_EQ(1u, GetNumQueuedPackets());

  audio_->AddAudioPacket(CreatePacket(20));
  ASSERT_EQ(30, GetNumQueuedTime());
  ASSERT_EQ(2u, GetNumQueuedPackets());
}

TEST_F(AudioJitterBufferTest, ExceedLatency) {
  // Push about 4 seconds worth of samples.
  for (uint32_t i = 0; i < 100; ++i) {
    audio_->AddAudioPacket(CreatePacket(40));
  }

  // Verify that we don't have more than 0.5s.
  ASSERT_LT(GetNumQueuedTime(), 500);
}

TEST_F(AudioJitterBufferTest, SingleAsyncRequest_UnderrunProtection) {
  // Add samples that are enough to fulfill one request but still doesn't get
  // passed the underrun protection.
  audio_->AddAudioPacket(CreatePacket(10));

  // Create an Audio Request.
  AsyncConsumeData(10);

  // The request is not fulfilled.
  ASSERT_EQ(1u, GetNumQueuedPackets());
  ASSERT_EQ(1u, GetNumQueuedRequests());
}

TEST_F(AudioJitterBufferTest, SingleAsyncRequest_Fulfilled) {
  // Add samples that are enough to bypass underrun protection.
  audio_->AddAudioPacket(CreatePacket(80));

  // Create an Audio Request.
  AsyncConsumeData(10);

  // Request is fulfilled and buffer is returned.
  ASSERT_EQ(1u, GetNumQueuedPackets());
  ASSERT_EQ(0u, GetNumQueuedRequests());
}

TEST_F(AudioJitterBufferTest, TwoAsyncRequest_FulfillOneByOne) {
  // Add just enough samples to fulfill one request.
  audio_->AddAudioPacket(CreatePacket(80));
  ASSERT_EQ(1u, GetNumQueuedPackets());

  AsyncConsumeData(80);
  // Request is immediately fulfilled.
  ASSERT_EQ(0u, GetNumQueuedPackets());
  ASSERT_EQ(0u, GetNumQueuedRequests());
  VerifyBuffersNotLost();

  // Add another request.
  AsyncConsumeData(80);
  ASSERT_EQ(0u, GetNumQueuedPackets());
  ASSERT_EQ(1u, GetNumQueuedRequests());
  VerifyBuffersNotLost();

  // Add packet fulfill the request.
  audio_->AddAudioPacket(CreatePacket(80));
  ASSERT_EQ(0u, GetNumQueuedPackets());
  ASSERT_EQ(0u, GetNumQueuedRequests());
}

TEST_F(AudioJitterBufferTest, TwoAsyncRequest_OnePacketFulfillsTwoRequests) {
  // Add packet big enough to fulfill two requests.
  audio_->AddAudioPacket(CreatePacket(100));
  ASSERT_EQ(1u, GetNumQueuedPackets());

  AsyncConsumeData(50);
  // Request is immediately fulfilled.
  ASSERT_EQ(1u, GetNumQueuedPackets());
  ASSERT_EQ(0u, GetNumQueuedRequests());
  VerifyBuffersNotLost();

  // Add another request.
  AsyncConsumeData(50);
  ASSERT_EQ(0u, GetNumQueuedPackets());
  ASSERT_EQ(0u, GetNumQueuedRequests());
}

TEST_F(AudioJitterBufferTest, TwoAsyncRequest_UnderrunProtectionKicksIn) {
  audio_->AddAudioPacket(CreatePacket(80));
  ASSERT_EQ(1u, GetNumQueuedPackets());

  // Consumes all packets while still waiting for 20ms of more data.
  AsyncConsumeData(100);
  ASSERT_EQ(0u, GetNumQueuedPackets());
  ASSERT_EQ(1u, GetNumQueuedRequests());
  VerifyBuffersNotLost();

  // The package does not get pass underrun protection.
  audio_->AddAudioPacket(CreatePacket(20));
  ASSERT_EQ(1u, GetNumQueuedPackets());
  ASSERT_EQ(1u, GetNumQueuedRequests());

  // Add a bigger packet, which bypasses underrun protection.
  audio_->AddAudioPacket(CreatePacket(100));
  ASSERT_EQ(1u, GetNumQueuedPackets());
  ASSERT_EQ(0u, GetNumQueuedRequests());
}

TEST_F(AudioJitterBufferTest, TwoAsyncRequest_TwoPacketsFulfillTwoRequests) {
  // Add sample that doesn't fulfill the first request.
  audio_->AddAudioPacket(CreatePacket(70));

  // Create two requests.
  AsyncConsumeData(80);
  AsyncConsumeData(80);

  // The first packet has been used to fill the first request.
  ASSERT_EQ(0u, GetNumQueuedPackets());
  ASSERT_EQ(2u, GetNumQueuedRequests());
  VerifyBuffersNotLost();

  // Add the rest to fulfill both requests.
  audio_->AddAudioPacket(CreatePacket(90));
  ASSERT_EQ(0u, GetNumQueuedPackets());
  ASSERT_EQ(0u, GetNumQueuedRequests());
}

TEST_F(AudioJitterBufferTest, ChangeSampleRate) {
  ASSERT_EQ(0u, GetNumQueuedPackets());

  audio_->AddAudioPacket(CreatePacket(20));
  AsyncConsumeData(80);
  ASSERT_EQ(1u, GetNumQueuedPackets());
  ASSERT_EQ(1u, GetNumQueuedRequests());
  VerifyBuffersNotLost();
  VerifyStreamFormat();

  SetSampleRate(AudioPacket::SAMPLING_RATE_44100);
  audio_->AddAudioPacket(CreatePacket(20));
  // Previous packet has been removed.
  ASSERT_EQ(1u, GetNumQueuedPackets());

  // Previous pending requests are cleared and callbacks has been run.
  ASSERT_EQ(0u, GetNumQueuedRequests());
  VerifyStreamFormat();
}

}  // namespace remoting
