// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_audio_input_stream.h"

#include <stdint.h>

#include <list>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "content/browser/media/capture/web_contents_tracker.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/audio/simple_sources.h"
#include "media/audio/virtual_audio_input_stream.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Assign;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;
using ::testing::SaveArg;
using ::testing::WithArgs;

using media::AudioBus;
using media::AudioInputStream;
using media::AudioOutputStream;
using media::AudioParameters;
using media::AudioPushSink;
using media::SineWaveAudioSource;
using media::VirtualAudioInputStream;

namespace content {

namespace {

const int kRenderProcessId = 123;
const int kRenderFrameId = 456;
const int kAnotherRenderProcessId = 789;
const int kAnotherRenderFrameId = 1;

const AudioParameters& TestAudioParameters() {
  static const AudioParameters params(
      AudioParameters::AUDIO_FAKE, media::CHANNEL_LAYOUT_STEREO,
      AudioParameters::kAudioCDSampleRate,
      AudioParameters::kAudioCDSampleRate / 100);
  return params;
}

class MockAudioMirroringManager : public AudioMirroringManager {
 public:
  MockAudioMirroringManager() : AudioMirroringManager() {}
  ~MockAudioMirroringManager() override {}

  MOCK_METHOD1(StartMirroring, void(MirroringDestination* destination));
  MOCK_METHOD1(StopMirroring, void(MirroringDestination* destination));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioMirroringManager);
};

class MockWebContentsTracker : public WebContentsTracker {
 public:
  MockWebContentsTracker() : WebContentsTracker(false) {}

  MOCK_METHOD3(Start,
               void(int render_process_id, int render_frame_id,
                    const ChangeCallback& callback));
  MOCK_METHOD0(Stop, void());

 private:
  ~MockWebContentsTracker() override {}

  DISALLOW_COPY_AND_ASSIGN(MockWebContentsTracker);
};

// A fully-functional VirtualAudioInputStream, but methods are mocked to allow
// tests to check how/when they are invoked.
class MockVirtualAudioInputStream : public VirtualAudioInputStream {
 public:
  explicit MockVirtualAudioInputStream(
      const scoped_refptr<base::SingleThreadTaskRunner>& worker_loop)
      : VirtualAudioInputStream(TestAudioParameters(), worker_loop,
                                VirtualAudioInputStream::AfterCloseCallback()),
        real_(TestAudioParameters(), worker_loop,
              base::Bind(&MockVirtualAudioInputStream::OnRealStreamHasClosed,
                         base::Unretained(this))),
        real_stream_is_closed_(false) {
    // Set default actions of mocked methods to delegate to the concrete
    // implementation.
    ON_CALL(*this, Open())
        .WillByDefault(Invoke(&real_, &VirtualAudioInputStream::Open));
    ON_CALL(*this, Start(_))
        .WillByDefault(Invoke(&real_, &VirtualAudioInputStream::Start));
    ON_CALL(*this, Stop())
        .WillByDefault(Invoke(&real_, &VirtualAudioInputStream::Stop));
    ON_CALL(*this, Close())
        .WillByDefault(Invoke(&real_, &VirtualAudioInputStream::Close));
    ON_CALL(*this, GetMaxVolume())
        .WillByDefault(Invoke(&real_, &VirtualAudioInputStream::GetMaxVolume));
    ON_CALL(*this, SetVolume(_))
        .WillByDefault(Invoke(&real_, &VirtualAudioInputStream::SetVolume));
    ON_CALL(*this, GetVolume())
        .WillByDefault(Invoke(&real_, &VirtualAudioInputStream::GetVolume));
    ON_CALL(*this, SetAutomaticGainControl(_))
        .WillByDefault(
            Invoke(&real_, &VirtualAudioInputStream::SetAutomaticGainControl));
    ON_CALL(*this, GetAutomaticGainControl())
        .WillByDefault(
            Invoke(&real_, &VirtualAudioInputStream::GetAutomaticGainControl));
    ON_CALL(*this, AddInputProvider(NotNull(), _))
        .WillByDefault(
            Invoke(&real_, &VirtualAudioInputStream::AddInputProvider));
    ON_CALL(*this, RemoveInputProvider(NotNull(), _))
        .WillByDefault(
            Invoke(&real_, &VirtualAudioInputStream::RemoveInputProvider));
  }

  ~MockVirtualAudioInputStream() override { DCHECK(real_stream_is_closed_); }

  MOCK_METHOD0(Open, bool());
  MOCK_METHOD1(Start, void(AudioInputStream::AudioInputCallback*));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(GetMaxVolume, double());
  MOCK_METHOD1(SetVolume, void(double));
  MOCK_METHOD0(GetVolume, double());
  MOCK_METHOD1(SetAutomaticGainControl, bool(bool));
  MOCK_METHOD0(GetAutomaticGainControl, bool());
  MOCK_METHOD2(AddInputProvider,
               void(media::AudioConverter::InputCallback*,
                    const AudioParameters&));
  MOCK_METHOD2(RemoveInputProvider,
               void(media::AudioConverter::InputCallback*,
                    const AudioParameters&));

 private:
  void OnRealStreamHasClosed(VirtualAudioInputStream* stream) {
    DCHECK_EQ(&real_, stream);
    DCHECK(!real_stream_is_closed_);
    real_stream_is_closed_ = true;
  }

  VirtualAudioInputStream real_;
  bool real_stream_is_closed_;

  DISALLOW_COPY_AND_ASSIGN(MockVirtualAudioInputStream);
};

class MockAudioInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  MockAudioInputCallback() {}

  MOCK_METHOD3(OnData,
               void(const media::AudioBus* src,
                    base::TimeTicks capture_time,
                    double volume));
  MOCK_METHOD0(OnError, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioInputCallback);
};

}  // namespace

class WebContentsAudioInputStreamTest : public testing::TestWithParam<bool> {
 public:
  WebContentsAudioInputStreamTest()
      : thread_bundle_(new TestBrowserThreadBundle(
            TestBrowserThreadBundle::REAL_IO_THREAD)),
        audio_thread_("Audio thread"),
        mock_mirroring_manager_(new MockAudioMirroringManager()),
        mock_tracker_(new MockWebContentsTracker()),
        mock_vais_(nullptr),
        wcais_(nullptr),
        destination_(nullptr),
        current_render_process_id_(kRenderProcessId),
        current_render_frame_id_(kRenderFrameId),
        on_data_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                       base::WaitableEvent::InitialState::NOT_SIGNALED) {
    audio_thread_.Start();
  }

  ~WebContentsAudioInputStreamTest() override {
    audio_thread_.Stop();
    thread_bundle_.reset();

    DCHECK(!mock_vais_);
    DCHECK(!wcais_);
    EXPECT_FALSE(destination_);
    DCHECK(streams_.empty());
    DCHECK(sources_.empty());
  }

  // If this value is true, we are testing a WebContentsAudioInputStream
  // instance, which requests duplicate audio.
  // Otherwise, we are testing a WebContentsAudioInputStream instance, which
  // requests diverting audio.
  bool is_duplication() const { return GetParam(); }

  void Open() {
    mock_vais_ = new MockVirtualAudioInputStream(audio_thread_.task_runner());
    EXPECT_CALL(*mock_vais_, Open());
    EXPECT_CALL(*mock_vais_, Close());  // At Close() time.

    ASSERT_EQ(kRenderProcessId, current_render_process_id_);
    ASSERT_EQ(kRenderFrameId, current_render_frame_id_);
    EXPECT_CALL(*mock_tracker_.get(),
                Start(kRenderProcessId, kRenderFrameId, _))
        .WillOnce(DoAll(
             SaveArg<2>(&change_callback_),
             WithArgs<0, 1>(Invoke(this,
                                   &WebContentsAudioInputStreamTest::
                                       SimulateChangeCallback))));

    EXPECT_CALL(*mock_tracker_.get(), Stop());  // At Close() time.

    wcais_ = new WebContentsAudioInputStream(
        current_render_process_id_, current_render_frame_id_,
        mock_mirroring_manager_.get(), mock_tracker_, mock_vais_,
        is_duplication());
    wcais_->Open();
  }

  void Start() {
    EXPECT_CALL(*mock_vais_, Start(&mock_input_callback_));
    EXPECT_CALL(*mock_vais_, Stop());  // At Stop() time.

    EXPECT_CALL(*mock_mirroring_manager_, StartMirroring(NotNull()))
        .WillOnce(SaveArg<0>(&destination_))
        .RetiresOnSaturation();
    // At Stop() time, or when the mirroring target changes:
    EXPECT_CALL(*mock_mirroring_manager_, StopMirroring(NotNull()))
        .WillOnce(Assign(
            &destination_,
            static_cast<AudioMirroringManager::MirroringDestination*>(nullptr)))
        .RetiresOnSaturation();

    EXPECT_CALL(mock_input_callback_, OnData(NotNull(), _, _))
        .WillRepeatedly(
            InvokeWithoutArgs(&on_data_event_, &base::WaitableEvent::Signal));

    wcais_->Start(&mock_input_callback_);

    // Test plumbing of volume controls and automatic gain controls.  Calls to
    // wcais_ methods should delegate directly to mock_vais_.
    EXPECT_CALL(*mock_vais_, GetVolume());
    double volume = wcais_->GetVolume();
    EXPECT_CALL(*mock_vais_, GetMaxVolume());
    const double max_volume = wcais_->GetMaxVolume();
    volume *= 2.0;
    if (volume < max_volume) {
      volume = max_volume;
    }
    EXPECT_CALL(*mock_vais_, SetVolume(volume));
    wcais_->SetVolume(volume);
    EXPECT_CALL(*mock_vais_, GetAutomaticGainControl());
    bool auto_gain = wcais_->GetAutomaticGainControl();
    auto_gain = !auto_gain;
    EXPECT_CALL(*mock_vais_, SetAutomaticGainControl(auto_gain));
    wcais_->SetAutomaticGainControl(auto_gain);
  }

  void AddAnotherInput() {
    // Note: WCAIS posts a task to invoke
    // MockAudioMirroringManager::StartMirroring() on the IO thread, which
    // causes our mock to set |destination_|.  Block until that has happened.
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(&done)));
    done.Wait();
    ASSERT_TRUE(destination_);

    EXPECT_CALL(*mock_vais_, AddInputProvider(NotNull(), _))
        .RetiresOnSaturation();
    // Later, when stream is closed:
    EXPECT_CALL(*mock_vais_, RemoveInputProvider(NotNull(), _))
        .RetiresOnSaturation();

    const AudioParameters& params = TestAudioParameters();
    SineWaveAudioSource* const source = new SineWaveAudioSource(
        params.channel_layout(), 200.0, params.sample_rate());
    sources_.push_back(source);
    if (is_duplication()) {
      media::AudioPushSink* out = destination_->AddPushInput(params);
      ASSERT_TRUE(out);
      sinks_.push_back(out);
      std::unique_ptr<media::AudioBus> audio_data = AudioBus::Create(params);
      base::TimeTicks now = base::TimeTicks::Now();
      // 20 Audio buses are enough for all test cases.
      const int kAudioBusesNumber = 20;
      for (int i = 0; i < kAudioBusesNumber; i++) {
        int frames = source->OnMoreData(
            base::TimeDelta(), base::TimeTicks::Now(), 0, audio_data.get());
        std::unique_ptr<media::AudioBus> copy = AudioBus::Create(params);
        audio_data->CopyTo(copy.get());
        out->OnData(std::move(copy), now);
        now += base::TimeDelta::FromMillisecondsD(
            frames * params.GetMicrosecondsPerFrame());
      }
    } else {
      AudioOutputStream* const out = destination_->AddInput(params);
      ASSERT_TRUE(out);
      streams_.push_back(out);
      EXPECT_TRUE(out->Open());
      out->Start(source);
    }
  }

  void RemoveOneInputInFIFOOrder() {
    if (is_duplication()) {
      ASSERT_FALSE(sinks_.empty());
      AudioPushSink* const out = sinks_.front();
      sinks_.pop_front();
      out->Close();  // Self-deletes.
    } else {
      ASSERT_FALSE(streams_.empty());
      AudioOutputStream* const out = streams_.front();
      streams_.pop_front();
      out->Stop();
      out->Close();  // Self-deletes.
    }
    ASSERT_TRUE(!sources_.empty());
    delete sources_.front();
    sources_.pop_front();
  }

  void ChangeMirroringTarget() {
    const int next_render_process_id =
        current_render_process_id_ == kRenderProcessId ?
            kAnotherRenderProcessId : kRenderProcessId;
    const int next_render_frame_id =
        current_render_frame_id_ == kRenderFrameId ?
            kAnotherRenderFrameId : kRenderFrameId;

    EXPECT_CALL(*mock_mirroring_manager_, StartMirroring(NotNull()))
        .WillOnce(SaveArg<0>(&destination_))
        .RetiresOnSaturation();

    SimulateChangeCallback(next_render_process_id, next_render_frame_id);

    current_render_process_id_ = next_render_process_id;
    current_render_frame_id_ = next_render_frame_id;
  }

  void LoseMirroringTarget() {
    EXPECT_CALL(mock_input_callback_, OnError());

    SimulateChangeCallback(-1, -1);
  }

  void Stop() {
    wcais_->Stop();
  }

  void Close() {
    // WebContentsAudioInputStream self-destructs on Close().  Its internal
    // objects hang around until they are no longer referred to (e.g., as tasks
    // on other threads shut things down).
    wcais_->Close();
    wcais_ = nullptr;
    mock_vais_ = nullptr;
  }

  void RunOnAudioThread(const base::Closure& closure) {
    audio_thread_.task_runner()->PostTask(FROM_HERE, closure);
  }

  // Block the calling thread until OnData() callbacks are being made.
  void WaitForData() {
    // Note: Arbitrarily chosen, but more iterations causes tests to take
    // significantly more time.
    static const int kNumIterations = 3;
    for (int i = 0; i < kNumIterations; ++i)
      on_data_event_.Wait();
  }

 private:
  void SimulateChangeCallback(int render_process_id, int render_frame_id) {
    ASSERT_FALSE(change_callback_.is_null());
    change_callback_.Run(render_process_id != -1 && render_frame_id != -1);
  }

  std::unique_ptr<TestBrowserThreadBundle> thread_bundle_;
  base::Thread audio_thread_;

  std::unique_ptr<MockAudioMirroringManager> mock_mirroring_manager_;
  scoped_refptr<MockWebContentsTracker> mock_tracker_;

  MockVirtualAudioInputStream* mock_vais_;  // Owned by wcais_.
  WebContentsAudioInputStream* wcais_;  // Self-destructs on Close().

  // Mock consumer of audio data.
  MockAudioInputCallback mock_input_callback_;

  // Provided by WebContentsAudioInputStream to the mock WebContentsTracker.
  // This callback is saved here, and test code will invoke it to simulate
  // target change events.
  WebContentsTracker::ChangeCallback change_callback_;

  // Provided by WebContentsAudioInputStream to the mock AudioMirroringManager.
  // A pointer to the implementation is saved here, and test code will invoke it
  // to simulate: 1) calls to AddInput(); and 2) diverting audio data.
  AudioMirroringManager::MirroringDestination* destination_;

  // Current target RenderFrame.  These get flipped in ChangedMirroringTarget().
  int current_render_process_id_;
  int current_render_frame_id_;

  // Streams provided by calls to WebContentsAudioInputStream::AddInput().  Each
  // is started with a simulated source of audio data.
  std::list<AudioOutputStream*> streams_;
  std::list<media::AudioPushSink*> sinks_;
  std::list<SineWaveAudioSource*> sources_;  // 1:1 with elements in streams_.

  base::WaitableEvent on_data_event_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsAudioInputStreamTest);
};

#define RUN_ON_AUDIO_THREAD(method) \
  RunOnAudioThread(base::Bind(&WebContentsAudioInputStreamTest::method,  \
                              base::Unretained(this)))

TEST_P(WebContentsAudioInputStreamTest, OpenedButNeverStarted) {
  RUN_ON_AUDIO_THREAD(Open);
  RUN_ON_AUDIO_THREAD(Close);
}

TEST_P(WebContentsAudioInputStreamTest, MirroringNothing) {
  RUN_ON_AUDIO_THREAD(Open);
  RUN_ON_AUDIO_THREAD(Start);
  WaitForData();
  RUN_ON_AUDIO_THREAD(Stop);
  RUN_ON_AUDIO_THREAD(Close);
}

TEST_P(WebContentsAudioInputStreamTest, MirroringOutputOutlivesSession) {
  RUN_ON_AUDIO_THREAD(Open);
  RUN_ON_AUDIO_THREAD(Start);
  RUN_ON_AUDIO_THREAD(AddAnotherInput);
  WaitForData();
  RUN_ON_AUDIO_THREAD(Stop);
  RUN_ON_AUDIO_THREAD(Close);
  RUN_ON_AUDIO_THREAD(RemoveOneInputInFIFOOrder);
}

TEST_P(WebContentsAudioInputStreamTest, MirroringOutputWithinSession) {
  RUN_ON_AUDIO_THREAD(Open);
  RUN_ON_AUDIO_THREAD(Start);
  RUN_ON_AUDIO_THREAD(AddAnotherInput);
  WaitForData();
  RUN_ON_AUDIO_THREAD(RemoveOneInputInFIFOOrder);
  RUN_ON_AUDIO_THREAD(Stop);
  RUN_ON_AUDIO_THREAD(Close);
}

// TODO(https://crbug.com/872340): Test appears to have timing-dependent flake.
TEST_P(WebContentsAudioInputStreamTest,
       DISABLED_MirroringNothingWithTargetChange) {
  RUN_ON_AUDIO_THREAD(Open);
  RUN_ON_AUDIO_THREAD(Start);
  RUN_ON_AUDIO_THREAD(ChangeMirroringTarget);
  RUN_ON_AUDIO_THREAD(Stop);
  RUN_ON_AUDIO_THREAD(Close);
}

TEST_P(WebContentsAudioInputStreamTest, MirroringOneStreamAfterTargetChange) {
  RUN_ON_AUDIO_THREAD(Open);
  RUN_ON_AUDIO_THREAD(Start);
  RUN_ON_AUDIO_THREAD(ChangeMirroringTarget);
  RUN_ON_AUDIO_THREAD(AddAnotherInput);
  WaitForData();
  RUN_ON_AUDIO_THREAD(Stop);
  RUN_ON_AUDIO_THREAD(Close);
  RUN_ON_AUDIO_THREAD(RemoveOneInputInFIFOOrder);
}

TEST_P(WebContentsAudioInputStreamTest, MirroringOneStreamWithTargetChange) {
  RUN_ON_AUDIO_THREAD(Open);
  RUN_ON_AUDIO_THREAD(Start);
  RUN_ON_AUDIO_THREAD(AddAnotherInput);
  WaitForData();
  RUN_ON_AUDIO_THREAD(ChangeMirroringTarget);
  RUN_ON_AUDIO_THREAD(RemoveOneInputInFIFOOrder);
  RUN_ON_AUDIO_THREAD(AddAnotherInput);
  WaitForData();
  RUN_ON_AUDIO_THREAD(Stop);
  RUN_ON_AUDIO_THREAD(Close);
  RUN_ON_AUDIO_THREAD(RemoveOneInputInFIFOOrder);
}

TEST_P(WebContentsAudioInputStreamTest, MirroringLostTarget) {
  RUN_ON_AUDIO_THREAD(Open);
  RUN_ON_AUDIO_THREAD(Start);
  RUN_ON_AUDIO_THREAD(AddAnotherInput);
  WaitForData();
  RUN_ON_AUDIO_THREAD(LoseMirroringTarget);
  RUN_ON_AUDIO_THREAD(RemoveOneInputInFIFOOrder);
  RUN_ON_AUDIO_THREAD(Stop);
  RUN_ON_AUDIO_THREAD(Close);
}

TEST_P(WebContentsAudioInputStreamTest, MirroringMultipleStreamsAndTargets) {
  RUN_ON_AUDIO_THREAD(Open);
  RUN_ON_AUDIO_THREAD(Start);
  RUN_ON_AUDIO_THREAD(AddAnotherInput);
  WaitForData();
  RUN_ON_AUDIO_THREAD(AddAnotherInput);
  RUN_ON_AUDIO_THREAD(AddAnotherInput);
  RUN_ON_AUDIO_THREAD(AddAnotherInput);
  WaitForData();
  RUN_ON_AUDIO_THREAD(ChangeMirroringTarget);
  RUN_ON_AUDIO_THREAD(RemoveOneInputInFIFOOrder);
  WaitForData();
  RUN_ON_AUDIO_THREAD(RemoveOneInputInFIFOOrder);
  RUN_ON_AUDIO_THREAD(RemoveOneInputInFIFOOrder);
  RUN_ON_AUDIO_THREAD(AddAnotherInput);
  WaitForData();
  RUN_ON_AUDIO_THREAD(RemoveOneInputInFIFOOrder);
  WaitForData();
  RUN_ON_AUDIO_THREAD(ChangeMirroringTarget);
  RUN_ON_AUDIO_THREAD(RemoveOneInputInFIFOOrder);
  RUN_ON_AUDIO_THREAD(Stop);
  RUN_ON_AUDIO_THREAD(Close);
}

INSTANTIATE_TEST_CASE_P(, WebContentsAudioInputStreamTest, ::testing::Bool());

}  // namespace content
