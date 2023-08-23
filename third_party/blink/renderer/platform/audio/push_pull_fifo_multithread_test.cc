// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "third_party/blink/renderer/platform/audio/push_pull_fifo.h"

#include <memory>

#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

// Base FIFOClient with an extra thread for looping and jitter control. The
// child class must define a specific task to run on the thread.
class FIFOClient {
  USING_FAST_MALLOC(FIFOClient);

 public:
  FIFOClient(PushPullFIFO* fifo, size_t bus_length, size_t jitter_range_ms)
      : fifo_(fifo),
        bus_(AudioBus::Create(fifo->GetStateForTest().number_of_channels,
                              bus_length)),
        client_thread_(NonMainThread::CreateThread(
            ThreadCreationParams(ThreadType::kTestThread)
                .SetThreadNameForTest("FIFOClientThread"))),
        done_event_(std::make_unique<base::WaitableEvent>(
            base::WaitableEvent::ResetPolicy::AUTOMATIC,
            base::WaitableEvent::InitialState::NOT_SIGNALED)),
        jitter_range_ms_(jitter_range_ms) {}

  base::WaitableEvent* Start(double duration_ms, double interval_ms) {
    duration_ms_ = duration_ms;
    interval_ms_ = interval_ms;
    PostCrossThreadTask(*client_thread_->GetTaskRunner(), FROM_HERE,
                        CrossThreadBindOnce(&FIFOClient::RunTaskOnOwnThread,
                                            CrossThreadUnretained(this)));
    return done_event_.get();
  }

  virtual void Stop(int callback_counter) = 0;
  virtual void RunTask() = 0;

  void Pull(size_t frames_to_pull) { fifo_->Pull(bus_.get(), frames_to_pull); }

  void Push() { fifo_->Push(bus_.get()); }

 private:
  void RunTaskOnOwnThread() {
    double interval_with_jitter = interval_ms_
        + (static_cast<double>(std::rand()) / RAND_MAX) * jitter_range_ms_;
    elapsed_ms_ += interval_with_jitter;
    ++counter_;
    RunTask();
    if (elapsed_ms_ < duration_ms_) {
      PostDelayedCrossThreadTask(
          *client_thread_->GetTaskRunner(), FROM_HERE,
          CrossThreadBindOnce(&FIFOClient::RunTaskOnOwnThread,
                              CrossThreadUnretained(this)),
          base::Milliseconds(interval_with_jitter));
    } else {
      Stop(counter_);
      done_event_->Signal();
    }
  }

  // Should be instantiated before calling Thread::CreateThread().
  // Do not place this after the |client_thread_| below.
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;

  raw_ptr<PushPullFIFO> fifo_;
  scoped_refptr<AudioBus> bus_;
  std::unique_ptr<NonMainThread> client_thread_;
  std::unique_ptr<base::WaitableEvent> done_event_;

  // Test duration.
  double duration_ms_;

  // Interval between each callback.
  double interval_ms_;

  // Jitter added to the regular pushing/pulling interval.
  // (where j is 0 < j < jitter_range_ms)
  double jitter_range_ms_;

  // Elapsed test duration.
  double elapsed_ms_ = 0;

  // Counter variable for the total number of callbacks invoked.
  int counter_ = 0;
};

// FIFO-pulling client (consumer). This mimics the audio device thread.
// |frames_to_pull| is variable.
class PullClient final : public FIFOClient {
 public:
  PullClient(PushPullFIFO* fifo, size_t frames_to_pull, double jitter_range_ms)
      : FIFOClient(fifo, frames_to_pull, jitter_range_ms),
        frames_to_pull_(frames_to_pull) {
  }

  void RunTask() override {
    Pull(frames_to_pull_);
  }

  void Stop(int callback_counter) override {
    LOG(INFO) << "PullClient stopped. (" << callback_counter << " calls)";
  }

 private:
  size_t frames_to_pull_;
};

// FIFO-pushing client (producer). This mimics the WebAudio rendering thread.
// The frames to push are static as 128 frames.
class PushClient final : public FIFOClient {
 public:
  PushClient(PushPullFIFO* fifo, size_t frames_to_push, double jitter_range_ms)
      : FIFOClient(fifo, frames_to_push, jitter_range_ms) {}

  void RunTask() override {
    Push();
  }

  void Stop(int callback_counter) override {
    LOG(INFO) << "PushClient stopped. (" << callback_counter << " calls)";
  }
};

struct FIFOSmokeTestParam {
  const double sample_rate;
  const unsigned number_of_channels;
  const size_t fifo_length;
  const double test_duration_ms;
  // Buffer size for pulling. Equivalent of |callback_buffer_size|.
  const size_t pull_buffer_size;
  // Jitter range for the pulling interval.
  const double pull_jitter_range_ms;
  // Buffer size for pushing. Equivalent of WebAudio render quantum.
  const size_t push_buffer_size;
  // Jitter range for the pushing interval.
  const double push_jitter_range_ms;
};

class PushPullFIFOSmokeTest
    : public testing::TestWithParam<FIFOSmokeTestParam> {};

TEST_P(PushPullFIFOSmokeTest, SmokeTests) {
  const FIFOSmokeTestParam param = GetParam();
  const double sample_rate = param.sample_rate * 4;

  const double pull_interval_ms =
      param.pull_buffer_size / sample_rate * 1000;
  const double push_interval_ms =
      param.push_buffer_size / sample_rate * 1000;

  std::unique_ptr<PushPullFIFO> test_fifo = std::make_unique<PushPullFIFO>(
      param.number_of_channels, param.fifo_length);
  std::unique_ptr<PullClient> pull_client = std::make_unique<PullClient>(
      test_fifo.get(), param.pull_buffer_size, param.pull_jitter_range_ms);
  std::unique_ptr<PushClient> push_client = std::make_unique<PushClient>(
      test_fifo.get(), param.push_buffer_size, param.push_jitter_range_ms);

  Vector<base::WaitableEvent*> done_events;
  done_events.push_back(
      pull_client->Start(param.test_duration_ms, pull_interval_ms));
  done_events.push_back(
      push_client->Start(param.test_duration_ms, push_interval_ms));

  LOG(INFO) << "PushPullFIFOSmokeTest - Started";

  // We have to wait both of events to be signaled.
  base::WaitableEvent::WaitMany(done_events.data(), done_events.size());
  base::WaitableEvent::WaitMany(done_events.data(), done_events.size());
}

FIFOSmokeTestParam smoke_test_params[] = {
    // Test case 0 (OSX): 256 Pull, 128 Push, Minimal jitter.
    // Thread's priority is lower than the device thread, so its jitter range
    // is slightly bigger than the other.
    {48000, 2, 8192, 250, 256, 1, 128, 2},

    // Test case 1 (Windows): 441 Pull, 128 Push. Moderate Jitter.
    // Windows' audio callback is known to be ~10ms and UMA data shows the
    // evidence for it. The jitter range was determined speculatively.
    {44100, 2, 8192, 250, 441, 2, 128, 3},

    // Test case 2 (Ubuntu/Linux): 512 Pull, 128 Push. Unstable callback, but
    // fast CPU. A typical configuration for Ubuntu + PulseAudio setup.
    // PulseAudio's callback is known to be rather unstable.
    {48000, 2, 8192, 250, 512, 8, 128, 1},

    // Test case 3 (Android-Reference): 512 Pull, 128 Push. Similar to Linux,
    // but
    // low profile CPU.
    {44100, 2, 8192, 250, 512, 8, 128, 3},

    // Test case 4 (Android-ExternalA): 441 Pull, 128 Push. Extreme jitter with
    // low profile CPU.
    {44100, 2, 8192, 250, 441, 24, 128, 8},

    // Test case 5 (Android-ExternalB): 5768 Pull, 128 Push. Huge callback with
    // large jitter. Low profile CPU.
    {44100, 2, 8192, 250, 5768, 120, 128, 12},

    // Test case 6 (User-specified buffer size): 960 Pull, 128 Push. Minimal
    // Jitter. 960 frames = 20ms at 48KHz.
    {48000, 2, 8192, 250, 960, 1, 128, 1},

    // Test case 7 (Longer test duration): 256 Pull, 128 Push. 2.5 seconds.
    {48000, 2, 8192, 2500, 256, 0, 128, 1}};

INSTANTIATE_TEST_SUITE_P(PushPullFIFOSmokeTest,
                         PushPullFIFOSmokeTest,
                         testing::ValuesIn(smoke_test_params));

}  // namespace

}  // namespace blink
