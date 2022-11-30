// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/processing_audio_fifo.h"

#include <cstring>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "media/audio/simple_sources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace audio {

const int kSampleRate = 48000;
const int kFramesPerBuffer = kSampleRate / 200;  // 5ms, for testing.
const int kTestToneFrequency = 440;
const int kTestFifoSize = 4;
const double kTestVolume = 0.567;

struct TestCaptureData {
  TestCaptureData(std::unique_ptr<media::AudioBus> audio_bus,
                  base::TimeTicks capture_time,
                  double volume,
                  bool key_pressed)
      : audio_bus(std::move(audio_bus)),
        capture_time(capture_time),
        volume(volume),
        key_pressed(key_pressed) {}

  TestCaptureData(const TestCaptureData&) = delete;
  TestCaptureData& operator=(const TestCaptureData&) = delete;

  TestCaptureData(TestCaptureData&&) = default;
  TestCaptureData& operator=(TestCaptureData&& other) = default;

  ~TestCaptureData() = default;

  std::unique_ptr<media::AudioBus> audio_bus;
  base::TimeTicks capture_time;
  double volume;
  bool key_pressed;
};

void VerifyAudioDataEqual(const media::AudioBus& first,
                          const media::AudioBus& second) {
  DCHECK_EQ(first.channels(), second.channels());
  DCHECK_EQ(first.frames(), second.frames());
  for (int ch = 0; ch < first.channels(); ++ch) {
    EXPECT_EQ(0, memcmp(first.channel(ch), second.channel(ch),
                        sizeof(float) * first.frames()));
  }
}

void VerifyProcessingData(const TestCaptureData& expected_data,
                          const media::AudioBus& audio_bus,
                          base::TimeTicks capture_time,
                          double volume,
                          bool key_pressed) {
  VerifyAudioDataEqual(*expected_data.audio_bus, audio_bus);
  EXPECT_EQ(expected_data.capture_time, capture_time);
  EXPECT_DOUBLE_EQ(expected_data.volume, volume);
  EXPECT_EQ(expected_data.key_pressed, key_pressed);
}

class ProcessingAudioFifoTest : public testing::Test {
 public:
  ProcessingAudioFifoTest()
      : params_(media::AudioParameters::Format::AUDIO_PCM_LINEAR,
                media::ChannelLayoutConfig::Stereo(),
                kSampleRate,
                kFramesPerBuffer),
        audio_source_(params_.channels(),
                      kTestToneFrequency,
                      params_.sample_rate()) {}

  ProcessingAudioFifoTest(const ProcessingAudioFifoTest&) = delete;
  ProcessingAudioFifoTest& operator=(const ProcessingAudioFifoTest&) = delete;

  ~ProcessingAudioFifoTest() override {
    if (fifo_)
      TearDownFifo();
  }

 protected:
  const media::AudioParameters params_;

  ProcessingAudioFifo* fifo() { return fifo_.get(); }

  void SetupFifo(ProcessingAudioFifo::ProcessAudioCallback callback) {
    fifo_ = std::make_unique<ProcessingAudioFifo>(
        params_, kTestFifoSize, std::move(callback), base::DoNothing());
    fifo_->Start();
  }

  void SetupFifoWithFakeEvent(
      ProcessingAudioFifo::ProcessAudioCallback callback) {
    fifo_ = std::make_unique<ProcessingAudioFifo>(
        params_, kTestFifoSize, std::move(callback), base::DoNothing());
    using_fake_event_ = true;
    fifo_->StartForTesting(&fake_new_capture_event_);
  }

  void SetFifoStoppingFlag() { fifo_->fifo_stopping_.Set(); }

  void TearDownFifo() {
    DCHECK(fifo_);

    if (using_fake_event_) {
      // Manually set the FIFO stopping flag, and allow the processing thread to
      // exit, otherwise we will wait forever in StopProcessingLoop().
      SetFifoStoppingFlag();
      SignalFakeNewCaptureEvent();
    }

    fifo_.reset();
  }

  std::unique_ptr<media::AudioBus> CreateAudioData(
      base::TimeTicks timestamp = base::TimeTicks::Now()) {
    auto data = media::AudioBus::Create(params_);
    audio_source_.OnMoreData(base::TimeDelta(), timestamp, {}, data.get());
    return data;
  }

  void GenerateTestCaptureData(std::vector<TestCaptureData>* dest, int count) {
    double volume_step = 1.0 / count;
    base::TimeTicks capture_time = base::TimeTicks::Now();
    base::TimeTicks timestamp = base::TimeTicks();

    for (int i = 0; i < count; ++i) {
      dest->emplace_back(CreateAudioData(timestamp), capture_time,
                         i * volume_step, i % 2);

      timestamp += params_.GetBufferDuration();
      capture_time += base::Milliseconds(5);
    }
  }

  void SignalFakeNewCaptureEvent() {
    DCHECK(using_fake_event_);
    fake_new_capture_event_.Signal();
  }

 private:
  std::unique_ptr<ProcessingAudioFifo> fifo_;

  bool using_fake_event_ = false;
  base::WaitableEvent fake_new_capture_event_{
      base::WaitableEvent::ResetPolicy::AUTOMATIC};

  // The audio source used to generate data.
  media::SineWaveAudioSource audio_source_;
};

TEST_F(ProcessingAudioFifoTest, ConstructDestroy) {
  auto fifo = std::make_unique<ProcessingAudioFifo>(
      params_, kTestFifoSize, base::DoNothing(), base::DoNothing());
  fifo.reset();
}

TEST_F(ProcessingAudioFifoTest, ConstructStartDestroy) {
  auto fifo = std::make_unique<ProcessingAudioFifo>(
      params_, kTestFifoSize, base::DoNothing(), base::DoNothing());
  fifo->Start();
  fifo.reset();
}

TEST_F(ProcessingAudioFifoTest, PushData_OneBuffer) {
  TestCaptureData test_data(CreateAudioData(), base::TimeTicks(), kTestVolume,
                            true);

  base::WaitableEvent data_processed;

  auto verify_data = [&](const media::AudioBus& audio_bus,
                         base::TimeTicks capture_time, double volume,
                         bool key_pressed) {
    // The processing callback should receive the same data that was pushed into
    // the fifo.
    VerifyProcessingData(test_data, audio_bus, capture_time, volume,
                         key_pressed);
    data_processed.Signal();
  };

  SetupFifo(base::BindLambdaForTesting(verify_data));

  fifo()->PushData(test_data.audio_bus.get(), test_data.capture_time,
                   test_data.volume, test_data.key_pressed);

  data_processed.Wait();
}

TEST_F(ProcessingAudioFifoTest, PushData_MultipleBuffers_SingleBatch) {
  // Stay below the FIFO's size to avoid dropping buffers.
  const int kNumberOfBuffers = kTestFifoSize;

  std::vector<TestCaptureData> capture_buffers;
  GenerateTestCaptureData(&capture_buffers, kNumberOfBuffers);

  base::WaitableEvent all_data_processed;

  int buffer_number = 0;
  auto verify_sequential_data = [&](const media::AudioBus& process_data,
                                    base::TimeTicks capture_time, double volume,
                                    bool key_pressed) {
    // Callbacks should receive buffers from |capture_buffers| in the order in
    // which they are pushed.
    VerifyProcessingData(capture_buffers[buffer_number], process_data,
                         capture_time, volume, key_pressed);

    if (++buffer_number == kNumberOfBuffers)
      all_data_processed.Signal();
  };

  SetupFifo(base::BindLambdaForTesting(verify_sequential_data));

  // Push all data at once.
  for (int i = 0; i < kNumberOfBuffers; ++i) {
    TestCaptureData& data = capture_buffers[i];
    fifo()->PushData(data.audio_bus.get(), data.capture_time, data.volume,
                     data.key_pressed);
  }

  all_data_processed.Wait();
  EXPECT_EQ(buffer_number, kNumberOfBuffers);
}

TEST_F(ProcessingAudioFifoTest, PushData_MultipleBuffers_WaitBetweenBuffers) {
  // Make sure push enough buffers to overwrite old ones.
  const int kNumberOfBuffers = kTestFifoSize * 3;

  std::vector<TestCaptureData> capture_buffers;
  GenerateTestCaptureData(&capture_buffers, kNumberOfBuffers);

  base::WaitableEvent single_buffer_processed(
      base::WaitableEvent::ResetPolicy::AUTOMATIC);

  int buffer_number = 0;
  auto verify_sequential_data = [&](const media::AudioBus& process_data,
                                    base::TimeTicks capture_time, double volume,
                                    bool key_pressed) {
    // Callbacks should receive buffers from |capture_buffers| in the order in
    // which they are pushed.
    VerifyProcessingData(capture_buffers[buffer_number++], process_data,
                         capture_time, volume, key_pressed);

    single_buffer_processed.Signal();
  };

  SetupFifo(base::BindLambdaForTesting(verify_sequential_data));

  // Wait for the previous buffer to be processed before pushing a new one.
  // This should guarantee we never drop buffers.
  for (int i = 0; i < kNumberOfBuffers; ++i) {
    TestCaptureData& data = capture_buffers[i];
    fifo()->PushData(data.audio_bus.get(), data.capture_time, data.volume,
                     data.key_pressed);
    single_buffer_processed.Wait();
  }
}

TEST_F(ProcessingAudioFifoTest, ProcessesAllAvailableData) {
  // Stay below the FIFO's size to avoid dropping buffers.
  const int kNumberOfBuffers = kTestFifoSize;

  std::vector<TestCaptureData> capture_buffers;
  GenerateTestCaptureData(&capture_buffers, kNumberOfBuffers);

  base::WaitableEvent any_buffer_processed;
  base::WaitableEvent all_buffers_processed;

  int buffer_number = 0;
  auto verify_sequential_data = [&](const media::AudioBus& process_data,
                                    base::TimeTicks capture_time, double volume,
                                    bool key_pressed) {
    VerifyProcessingData(capture_buffers[buffer_number++], process_data,
                         capture_time, volume, key_pressed);

    any_buffer_processed.Signal();
    if (buffer_number == kNumberOfBuffers)
      all_buffers_processed.Signal();
  };

  // The processing thread won't be woken up when new data is pushed.
  SetupFifoWithFakeEvent(base::BindLambdaForTesting(verify_sequential_data));

  // Push all data.
  for (int i = 0; i < kNumberOfBuffers; ++i) {
    TestCaptureData& data = capture_buffers[i];
    fifo()->PushData(data.audio_bus.get(), data.capture_time, data.volume,
                     data.key_pressed);
  }

  // No data should have been processed by now.
  EXPECT_FALSE(any_buffer_processed.TimedWait(base::Milliseconds(10)));
  EXPECT_EQ(buffer_number, 0);

  // The processing thread should process all pending data at once.
  SignalFakeNewCaptureEvent();

  all_buffers_processed.Wait();
}

TEST_F(ProcessingAudioFifoTest, NoDataToProcess) {
  base::WaitableEvent processing_callback_called;

  SetupFifoWithFakeEvent(base::BindLambdaForTesting(
      [&](const media::AudioBus& process_data, base::TimeTicks capture_time,
          double volume, bool key_pressed) {
        ADD_FAILURE() << "Processing callback unexpectedly called";
        processing_callback_called.Signal();
      }));

  // Wake up the processing thread without any queued data. This might not
  // happen in practice, but the processing thread should be resilient to race
  // conditions nonetheless.
  SignalFakeNewCaptureEvent();

  EXPECT_FALSE(processing_callback_called.TimedWait(base::Milliseconds(10)));
}

TEST_F(ProcessingAudioFifoTest, DontProcessPendingDataDuringStop) {
  base::WaitableEvent processing_callback_called;

  SetupFifoWithFakeEvent(base::BindLambdaForTesting(
      [&](const media::AudioBus& process_data, base::TimeTicks capture_time,
          double volume, bool key_pressed) {
        ADD_FAILURE() << "Processing callback unexpectedly called";
        processing_callback_called.Signal();
      }));

  // Push data into the FIFO, without calling SignalFakeNewCaptureEvent().
  fifo()->PushData(CreateAudioData().get(), base::TimeTicks(), kTestVolume,
                   true);

  // The pushed data should not be processed when we stop and destroy the FIFO.
  TearDownFifo();

  EXPECT_FALSE(processing_callback_called.TimedWait(base::Milliseconds(10)));
}

TEST_F(ProcessingAudioFifoTest, FifoFull_DroppedFrames) {
  // Make sure push enough buffers to overwrite old ones.
  const int kNumberOfBatches = 3;
  const int kBatchSize = kTestFifoSize;
  const int kNumberOfBuffers = kBatchSize * kNumberOfBatches;

  std::vector<TestCaptureData> capture_buffers;
  GenerateTestCaptureData(&capture_buffers, kNumberOfBuffers);

  // Get the buffer idx in |capture_buffers| from the batch and buffer number.
  auto get_idx = [](int buffer_number, int batch_number) {
    return buffer_number + batch_number * kBatchSize;
  };

  base::WaitableEvent buffer_batch_processed(
      base::WaitableEvent::ResetPolicy::AUTOMATIC);

  // This test pushes 3 batches of buffers of size kBatchSize into the FIFO.
  // The first batch will be queued, the second will be dropped; the third
  // batch will be queued, after the first batch is done processing.
  int buffer_number = 0;
  int batch_number = 0;
  auto verify_dropped_data = [&](const media::AudioBus& process_data,
                                 base::TimeTicks capture_time, double volume,
                                 bool key_pressed) {
    // Verify we don't get any buffers from the 2nd batch.
    VerifyProcessingData(capture_buffers[get_idx(buffer_number, batch_number)],
                         process_data, capture_time, volume, key_pressed);

    if (++buffer_number == kBatchSize) {
      buffer_number = 0;
      batch_number = 2;  // Skip from 1st to 3rd batch of buffers.
      buffer_batch_processed.Signal();
    }
  };

  SetupFifoWithFakeEvent(base::BindLambdaForTesting(verify_dropped_data));

  // Send 3 batches.
  for (int batch = 0; batch < kNumberOfBatches; ++batch) {
    // Queue a batch of buffers.
    for (int buffer = 0; buffer < kBatchSize; ++buffer) {
      TestCaptureData& data = capture_buffers[get_idx(buffer, batch)];
      fifo()->PushData(data.audio_bus.get(), data.capture_time, data.volume,
                       data.key_pressed);
    }

    // Skip processing the first batch, ensuring the second batch will be
    // dropped.
    if (batch == 0)
      continue;

    // Process queued buffers.
    SignalFakeNewCaptureEvent();
    buffer_batch_processed.Wait();
  }
}

TEST_F(ProcessingAudioFifoTest, StopDuringBatchProcess) {
  const int kNumberOfBuffers = kTestFifoSize;

  std::vector<TestCaptureData> capture_buffers;
  GenerateTestCaptureData(&capture_buffers, kNumberOfBuffers);

  base::WaitableEvent any_buffer_processed(
      base::WaitableEvent::ResetPolicy::AUTOMATIC);

  base::WaitableEvent stop_flag_set;

  int number_of_calls = 0;
  auto verify_stopping = [&](const media::AudioBus& process_data,
                             base::TimeTicks capture_time, double volume,
                             bool key_pressed) {
    // We should only get one processing callback call, since calling
    // SetFifoStoppingFlag() should prevent further data from being processed.
    EXPECT_LE(++number_of_calls, 1);

    any_buffer_processed.Signal();

    stop_flag_set.Wait();
  };

  SetupFifoWithFakeEvent(base::BindLambdaForTesting(verify_stopping));

  // Queue data but don't process it.
  for (int i = 0; i < kNumberOfBuffers; ++i) {
    TestCaptureData& data = capture_buffers[i];
    fifo()->PushData(data.audio_bus.get(), data.capture_time, data.volume,
                     data.key_pressed);
  }

  // Process a single buffer.
  SignalFakeNewCaptureEvent();
  any_buffer_processed.Wait();

  // Simulate StopProcessingLoop() being called (without stopping the processing
  // thread).
  SetFifoStoppingFlag();
  stop_flag_set.Signal();

  // The remaining buffers in the FIFO should not be processed.
  EXPECT_FALSE(any_buffer_processed.TimedWait(base::Milliseconds(10)));
  EXPECT_EQ(1, number_of_calls);

  // Explicitly destroy the FIFO, to avoid TSAN failures.
  TearDownFifo();
}

}  // namespace audio
