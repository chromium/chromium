// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_audio_fifo_sink_adapter.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "remoting/base/fifo_buffer.h"
#include "remoting/base/in_memory_fifo_buffer.h"
#include "remoting/protocol/audio_sample_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/media_stream_interface.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

namespace remoting::protocol {

namespace {

class FakeAudioTrackSource : public webrtc::AudioSourceInterface {
 public:
  FakeAudioTrackSource() = default;

  // webrtc::NotifierInterface implementation.
  void RegisterObserver(webrtc::ObserverInterface* observer) override {}
  void UnregisterObserver(webrtc::ObserverInterface* observer) override {}

  // webrtc::MediaSourceInterface implementation.
  SourceState state() const override { return kLive; }
  bool remote() const override { return false; }

  // webrtc::AudioSourceInterface implementation.
  void AddSink(webrtc::AudioTrackSinkInterface* sink) override {
    sinks_.push_back(sink);
  }
  void RemoveSink(webrtc::AudioTrackSinkInterface* sink) override {
    std::erase(sinks_, sink);
  }

  const std::vector<webrtc::AudioTrackSinkInterface*>& sinks() const {
    return sinks_;
  }

 protected:
  ~FakeAudioTrackSource() override = default;

 private:
  std::vector<webrtc::AudioTrackSinkInterface*> sinks_;
};

class FakeAudioTrack : public webrtc::AudioTrackInterface {
 public:
  explicit FakeAudioTrack(FakeAudioTrackSource* source) : source_(source) {}

  // webrtc::NotifierInterface implementation.
  void RegisterObserver(webrtc::ObserverInterface* observer) override {}
  void UnregisterObserver(webrtc::ObserverInterface* observer) override {}

  // webrtc::MediaStreamTrackInterface implementation.
  std::string kind() const override { return "audio"; }
  std::string id() const override { return "audio_track"; }
  bool enabled() const override { return true; }
  bool set_enabled(bool enable) override { return true; }
  TrackState state() const override { return kLive; }

  // webrtc::AudioTrackInterface implementation.
  void AddSink(webrtc::AudioTrackSinkInterface* sink) override {
    source_->AddSink(sink);
  }
  void RemoveSink(webrtc::AudioTrackSinkInterface* sink) override {
    source_->RemoveSink(sink);
  }
  webrtc::AudioSourceInterface* GetSource() const override {
    return source_.get();
  }

 protected:
  ~FakeAudioTrack() override = default;

 private:
  webrtc::scoped_refptr<FakeAudioTrackSource> source_;
};

}  // namespace

class WebrtcAudioFifoSinkAdapterTest : public testing::Test {
 public:
  void SetUp() override {
    source_ = new webrtc::RefCountedObject<FakeAudioTrackSource>();
    track_ = new webrtc::RefCountedObject<FakeAudioTrack>(source_.get());
  }

  void CreateAdapter(std::unique_ptr<FifoBufferWriter> writer) {
    adapter_ = std::make_unique<WebrtcAudioFifoSinkAdapter>(
        std::move(writer),
        base::BindRepeating(&WebrtcAudioFifoSinkAdapterTest::OnFormatChanged,
                            base::Unretained(this)));
    adapter_->SetTrack(track_);
  }

  void OnFormatChanged(const AudioSampleInfo& info,
                       base::OnceClosure acknowledgment_callback) {
    format_changed_calls_.push_back(info);
    pending_acknowledgments_.push_back(std::move(acknowledgment_callback));
    format_changed_future_.SetValue();
  }

  void AcknowledgeFormat(size_t index = 0) {
    ASSERT_LT(index, pending_acknowledgments_.size());
    ASSERT_TRUE(pending_acknowledgments_[index]);
    std::move(pending_acknowledgments_[index]).Run();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  webrtc::scoped_refptr<FakeAudioTrackSource> source_;
  webrtc::scoped_refptr<FakeAudioTrack> track_;

  std::unique_ptr<WebrtcAudioFifoSinkAdapter> adapter_;
  std::vector<AudioSampleInfo> format_changed_calls_;
  std::vector<base::OnceClosure> pending_acknowledgments_;
  base::test::TestFuture<void> format_changed_future_;
};

TEST_F(WebrtcAudioFifoSinkAdapterTest, PlayoutAndFormatHandshake) {
  std::unique_ptr<InMemoryFifoBufferWriter> writer;
  std::unique_ptr<InMemoryFifoBufferReader> reader;
  ASSERT_TRUE(CreateInMemoryFifoBuffer(1024, writer, reader));

  CreateAdapter(std::move(writer));

  // 1. Inject first frame (triggers format handshake).
  std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8};
  adapter_->OnData(data.data(), 16, 48000, 2, 2);

  // Wait for the format change callback.
  EXPECT_TRUE(format_changed_future_.Wait());

  // Handshake pending: data must be dropped, and format callback triggered.
  ASSERT_EQ(format_changed_calls_.size(), 1u);
  EXPECT_EQ(format_changed_calls_[0].sampling_rate, 48000u);
  EXPECT_EQ(format_changed_calls_[0].channels, 2u);
  EXPECT_EQ(reader->GetBufferedBytes(), 0u);

  // 2. Inject data during handshake (must be dropped).
  adapter_->OnData(data.data(), 16, 48000, 2, 2);
  EXPECT_EQ(reader->GetBufferedBytes(), 0u);

  // 3. Acknowledge format.
  AcknowledgeFormat();
  EXPECT_EQ(reader->GetBufferedBytes(), 0u);

  // 4. Inject data after handshake (single-copy write succeeds).
  adapter_->OnData(data.data(), 16, 48000, 2, 2);
  EXPECT_EQ(reader->GetBufferedBytes(), 8u);

  std::vector<uint8_t> read_data(8);
  EXPECT_EQ(reader->Read(read_data), 8u);
  EXPECT_EQ(read_data, data);
}

TEST_F(WebrtcAudioFifoSinkAdapterTest, PlayoutFormatOscillationHandshake) {
  std::unique_ptr<InMemoryFifoBufferWriter> writer;
  std::unique_ptr<InMemoryFifoBufferReader> reader;
  ASSERT_TRUE(CreateInMemoryFifoBuffer(1024, writer, reader));

  CreateAdapter(std::move(writer));

  // 1. Trigger first format change (Stereo 48kHz).
  std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8};
  adapter_->OnData(data.data(), 16, 48000, 2, 2);
  EXPECT_TRUE(format_changed_future_.Wait());
  format_changed_future_.Clear();

  // 2. Rapidly trigger second format change (Mono 44.1kHz) BEFORE acknowledging
  // the first.
  adapter_->OnData(data.data(), 16, 44100, 1, 4);
  EXPECT_TRUE(format_changed_future_.Wait());

  // We have two pending handshakes.
  ASSERT_EQ(format_changed_calls_.size(), 2u);
  EXPECT_EQ(format_changed_calls_[0].sampling_rate, 48000u);
  EXPECT_EQ(format_changed_calls_[1].sampling_rate, 44100u);
  EXPECT_EQ(reader->GetBufferedBytes(), 0u);

  // 3. Acknowledge the FIRST format handshake (sequence number 1).
  AcknowledgeFormat(0);
  EXPECT_EQ(reader->GetBufferedBytes(), 0u);

  // Inject data. Since the LATEST sequence is 2, but we only acknowledged
  // sequence 1, data must still be dropped.
  adapter_->OnData(data.data(), 16, 44100, 1, 4);
  EXPECT_EQ(reader->GetBufferedBytes(), 0u);

  // 4. Acknowledge the SECOND format handshake (sequence number 2).
  AcknowledgeFormat(1);
  EXPECT_EQ(reader->GetBufferedBytes(), 0u);

  // Now both sequences match (value 2). Direct playout succeeds.
  adapter_->OnData(data.data(), 16, 44100, 1, 4);
  EXPECT_EQ(reader->GetBufferedBytes(), 8u);
}

TEST_F(WebrtcAudioFifoSinkAdapterTest, SetTrackHotSwapsTrackOnTheFly) {
  std::unique_ptr<InMemoryFifoBufferWriter> writer;
  std::unique_ptr<InMemoryFifoBufferReader> reader;
  ASSERT_TRUE(CreateInMemoryFifoBuffer(1024, writer, reader));

  CreateAdapter(std::move(writer));

  // 1. Verify first track has the adapter in its sinks.
  EXPECT_FALSE(source_->sinks().empty());
  EXPECT_EQ(source_->sinks()[0], adapter_.get());

  // 2. Create a second track.
  webrtc::scoped_refptr<FakeAudioTrackSource> source2(
      new webrtc::RefCountedObject<FakeAudioTrackSource>());
  webrtc::scoped_refptr<FakeAudioTrack> track2(
      new webrtc::RefCountedObject<FakeAudioTrack>(source2.get()));

  // 3. Hot-swap the track.
  adapter_->SetTrack(track2);

  // 4. Verify the first track's sink is removed.
  EXPECT_TRUE(source_->sinks().empty());

  // 5. Verify the second track's sink is added.
  EXPECT_FALSE(source2->sinks().empty());
  EXPECT_EQ(source2->sinks()[0], adapter_.get());

  // 6. Verify we can cleanly unbind the track (nullptr).
  adapter_->SetTrack(nullptr);
  EXPECT_TRUE(source2->sinks().empty());
}

}  // namespace remoting::protocol
