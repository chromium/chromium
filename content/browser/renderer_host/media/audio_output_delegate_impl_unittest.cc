// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_output_delegate_impl.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/sync_socket.h"
#include "base/task/post_task.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_observer.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/audio/audio_output_controller.h"
#include "media/audio/audio_sync_reader.h"
#include "media/audio/audio_thread_impl.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/media_switches.h"
#include "media/mojo/interfaces/audio_output_stream.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::NotNull;
using ::testing::StrictMock;

namespace content {

media::AudioParameters Params() {
  return media::AudioParameters::UnavailableDeviceParams();
}

namespace {

const int kRenderProcessId = 1;
const int kRenderFrameId = 5;
const int kStreamId = 50;
const char kDefaultDeviceId[] = "";

void NoLog(const std::string&) {}

class MockAudioMirroringManager : public AudioMirroringManager {
 public:
  MOCK_METHOD3(AddDiverter,
               void(int render_process_id,
                    int render_frame_id,
                    Diverter* diverter));
  MOCK_METHOD1(RemoveDiverter, void(Diverter* diverter));
};

class MockObserver : public content::MediaObserver {
 public:
  void OnAudioCaptureDevicesChanged() override {}
  void OnVideoCaptureDevicesChanged() override {}
  void OnMediaRequestStateChanged(int render_process_id,
                                  int render_frame_id,
                                  int page_request_id,
                                  const GURL& security_origin,
                                  MediaStreamType stream_type,
                                  MediaRequestState state) override {}
  void OnSetCapturingLinkSecured(int render_process_id,
                                 int render_frame_id,
                                 int page_request_id,
                                 MediaStreamType stream_type,
                                 bool is_secure) override {}

  MOCK_METHOD2(OnCreatingAudioStream,
               void(int render_process_id, int render_frame_id));
};

class MockEventHandler : public media::AudioOutputDelegate::EventHandler {
 public:
  void OnStreamCreated(
      int stream_id,
      base::UnsafeSharedMemoryRegion shared_memory_region,
      std::unique_ptr<base::CancelableSyncSocket> socket) override {
    EXPECT_EQ(stream_id, kStreamId);
    EXPECT_TRUE(shared_memory_region.IsValid());
    EXPECT_NE(socket.get(), nullptr);
    GotOnStreamCreated();
  }

  MOCK_METHOD0(GotOnStreamCreated, void());
  MOCK_METHOD1(OnStreamError, void(int stream_id));
};

class DummyAudioOutputStream : public media::AudioOutputStream {
 public:
  // AudioOutputSteam implementation:
  bool Open() override { return true; }
  void Start(AudioSourceCallback* cb) override {}
  void Stop() override {}
  void SetVolume(double volume) override {}
  void GetVolume(double* volume) override { *volume = 1; }
  void Close() override {}
};

class MockAudioOutputStreamObserver
    : public media::mojom::AudioOutputStreamObserver {
 public:
  ~MockAudioOutputStreamObserver() override = default;

  // media::mojom::AudioOutputStreamObserver implementation
  MOCK_METHOD0(DidStartPlaying, void());
  MOCK_METHOD0(DidStopPlaying, void());
  MOCK_METHOD1(DidChangeAudibleState, void(bool));
};

class DummyMojoAudioLogImpl : public media::mojom::AudioLog {
 public:
  DummyMojoAudioLogImpl() = default;
  ~DummyMojoAudioLogImpl() override = default;

  void OnCreated(const media::AudioParameters& params,
                 const std::string& device_id) override {}
  void OnStarted() override {}
  void OnStopped() override {}
  void OnClosed() override {}
  void OnError() override {}
  void OnSetVolume(double volume) override {}
  void OnProcessingStateChanged(const std::string& message) override {}
  void OnLogMessage(const std::string& message) override {}
};

media::mojom::AudioLogPtr CreateDummyMojoAudioLog() {
  media::mojom::AudioLogPtr audio_log_ptr;
  mojo::MakeStrongBinding(std::make_unique<DummyMojoAudioLogImpl>(),
                          mojo::MakeRequest(&audio_log_ptr));
  return audio_log_ptr;
}

}  // namespace

class AudioOutputDelegateTest : public testing::Test {
 public:
  AudioOutputDelegateTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);

    // This test uses real UI, IO and audio threads.
    // AudioOutputDelegate mainly interacts with the IO and audio threads,
    // but interacts with UI for bad messages, so using these threads should
    // approximate the real conditions of AudioOutputDelegate well.
    thread_bundle_ = std::make_unique<TestBrowserThreadBundle>(
        TestBrowserThreadBundle::Options::REAL_IO_THREAD);

    audio_manager_.reset(new media::FakeAudioManager(
        std::make_unique<media::AudioThreadImpl>(), &log_factory_));
    audio_manager_->SetDiverterCallbacks(
        mirroring_manager_.GetAddDiverterCallback(),
        mirroring_manager_.GetRemoveDiverterCallback());
  }

  ~AudioOutputDelegateTest() override { audio_manager_->Shutdown(); }

  mojo::StrongBindingPtr<media::mojom::AudioOutputStreamObserver>
  CreateObserverBinding(
      media::mojom::AudioOutputStreamObserverPtr* observer_ptr) {
    return mojo::MakeStrongBinding(
        std::make_unique<MockAudioOutputStreamObserver>(),
        mojo::MakeRequest(observer_ptr));
  }

  MockAudioOutputStreamObserver& GetMockObserver(
      mojo::StrongBindingPtr<media::mojom::AudioOutputStreamObserver>*
          observer_binding) {
    return *static_cast<MockAudioOutputStreamObserver*>(
        (*observer_binding)->impl());
  }

  // Test bodies are here, so that we can run them on the IO thread.
  void CreateTest(base::Closure done) {
    EXPECT_CALL(media_observer_,
                OnCreatingAudioStream(kRenderProcessId, kRenderFrameId));
    EXPECT_CALL(event_handler_, GotOnStreamCreated());
    EXPECT_CALL(mirroring_manager_,
                AddDiverter(kRenderProcessId, kRenderFrameId, NotNull()));
    media::mojom::AudioOutputStreamObserverPtr observer_ptr;
    auto observer_binding = CreateObserverBinding(&observer_ptr);
    EXPECT_CALL(GetMockObserver(&observer_binding), DidStartPlaying()).Times(0);
    EXPECT_CALL(GetMockObserver(&observer_binding), DidStopPlaying()).Times(0);

    {
      auto socket = std::make_unique<base::CancelableSyncSocket>();
      auto reader = media::AudioSyncReader::Create(base::BindRepeating(&NoLog),
                                                   Params(), socket.get());
      AudioOutputDelegateImpl delegate(
          std::move(reader), std::move(socket), &event_handler_,
          audio_manager_.get(), CreateDummyMojoAudioLog(), &media_observer_,
          kStreamId, kRenderFrameId, kRenderProcessId, Params(),
          std::move(observer_ptr), kDefaultDeviceId);

      SyncWithAllThreads();

      EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()));
    }
    SyncWithAllThreads();
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, std::move(done));
  }

  void PlayTest(base::Closure done, bool use_bound_observer) {
    EXPECT_CALL(media_observer_,
                OnCreatingAudioStream(kRenderProcessId, kRenderFrameId));
    EXPECT_CALL(event_handler_, GotOnStreamCreated());
    EXPECT_CALL(mirroring_manager_,
                AddDiverter(kRenderProcessId, kRenderFrameId, NotNull()));
    media::mojom::AudioOutputStreamObserverPtr observer_ptr;
    auto observer_binding = CreateObserverBinding(&observer_ptr);
    if (use_bound_observer) {
      EXPECT_CALL(GetMockObserver(&observer_binding), DidStartPlaying())
          .Times(0);
      EXPECT_CALL(GetMockObserver(&observer_binding), DidStopPlaying())
          .Times(0);
    }

    {
      auto socket = std::make_unique<base::CancelableSyncSocket>();
      auto reader = media::AudioSyncReader::Create(base::BindRepeating(&NoLog),
                                                   Params(), socket.get());
      media::mojom::AudioOutputStreamObserverPtr observer_ptr;
      auto observer_binding = CreateObserverBinding(&observer_ptr);
      if (use_bound_observer) {
        InSequence s;
        EXPECT_CALL(GetMockObserver(&observer_binding), DidStartPlaying());
        EXPECT_CALL(GetMockObserver(&observer_binding), DidStopPlaying());
      }

      AudioOutputDelegateImpl delegate(
          std::move(reader), std::move(socket), &event_handler_,
          audio_manager_.get(), CreateDummyMojoAudioLog(), &media_observer_,
          kStreamId, kRenderFrameId, kRenderProcessId, Params(),
          std::move(observer_ptr), kDefaultDeviceId);

      if (!use_bound_observer)
        observer_binding->Close();

      delegate.OnPlayStream();

      SyncWithAllThreads();

      EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()));
    }
    SyncWithAllThreads();
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, std::move(done));
  }

  void PauseTest(base::Closure done) {
    EXPECT_CALL(media_observer_,
                OnCreatingAudioStream(kRenderProcessId, kRenderFrameId));
    EXPECT_CALL(event_handler_, GotOnStreamCreated());
    EXPECT_CALL(mirroring_manager_,
                AddDiverter(kRenderProcessId, kRenderFrameId, NotNull()));
    media::mojom::AudioOutputStreamObserverPtr observer_ptr;
    auto observer_binding = CreateObserverBinding(&observer_ptr);
    EXPECT_CALL(GetMockObserver(&observer_binding), DidStartPlaying()).Times(0);
    EXPECT_CALL(GetMockObserver(&observer_binding), DidStopPlaying()).Times(0);

    {
      auto socket = std::make_unique<base::CancelableSyncSocket>();
      auto reader = media::AudioSyncReader::Create(base::BindRepeating(&NoLog),
                                                   Params(), socket.get());
      AudioOutputDelegateImpl delegate(
          std::move(reader), std::move(socket), &event_handler_,
          audio_manager_.get(), CreateDummyMojoAudioLog(), &media_observer_,
          kStreamId, kRenderFrameId, kRenderProcessId, Params(),
          std::move(observer_ptr), kDefaultDeviceId);

      delegate.OnPauseStream();

      SyncWithAllThreads();

      EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()));
    }
    SyncWithAllThreads();
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, std::move(done));
  }

  void PlayPausePlayTest(base::Closure done) {
    EXPECT_CALL(media_observer_,
                OnCreatingAudioStream(kRenderProcessId, kRenderFrameId));
    EXPECT_CALL(event_handler_, GotOnStreamCreated());
    EXPECT_CALL(mirroring_manager_,
                AddDiverter(kRenderProcessId, kRenderFrameId, NotNull()));

    {
      auto socket = std::make_unique<base::CancelableSyncSocket>();
      auto reader = media::AudioSyncReader::Create(base::BindRepeating(&NoLog),
                                                   Params(), socket.get());
      media::mojom::AudioOutputStreamObserverPtr observer_ptr;
      auto observer_binding = CreateObserverBinding(&observer_ptr);
      InSequence s;
      EXPECT_CALL(GetMockObserver(&observer_binding), DidStartPlaying());
      EXPECT_CALL(GetMockObserver(&observer_binding), DidStopPlaying());
      EXPECT_CALL(GetMockObserver(&observer_binding), DidStartPlaying());
      EXPECT_CALL(GetMockObserver(&observer_binding), DidStopPlaying());

      AudioOutputDelegateImpl delegate(
          std::move(reader), std::move(socket), &event_handler_,
          audio_manager_.get(), CreateDummyMojoAudioLog(), &media_observer_,
          kStreamId, kRenderFrameId, kRenderProcessId, Params(),
          std::move(observer_ptr), kDefaultDeviceId);

      delegate.OnPlayStream();
      delegate.OnPauseStream();
      delegate.OnPlayStream();

      SyncWithAllThreads();

      EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()));
    }
    SyncWithAllThreads();
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, std::move(done));
  }

  void PlayPlayTest(base::Closure done) {
    EXPECT_CALL(media_observer_,
                OnCreatingAudioStream(kRenderProcessId, kRenderFrameId));
    EXPECT_CALL(event_handler_, GotOnStreamCreated());
    EXPECT_CALL(mirroring_manager_,
                AddDiverter(kRenderProcessId, kRenderFrameId, NotNull()));

    {
      auto socket = std::make_unique<base::CancelableSyncSocket>();
      auto reader = media::AudioSyncReader::Create(base::BindRepeating(&NoLog),
                                                   Params(), socket.get());
      media::mojom::AudioOutputStreamObserverPtr observer_ptr;
      auto observer_binding = CreateObserverBinding(&observer_ptr);
      InSequence s;
      EXPECT_CALL(GetMockObserver(&observer_binding), DidStartPlaying());
      EXPECT_CALL(GetMockObserver(&observer_binding), DidStopPlaying());

      AudioOutputDelegateImpl delegate(
          std::move(reader), std::move(socket), &event_handler_,
          audio_manager_.get(), CreateDummyMojoAudioLog(), &media_observer_,
          kStreamId, kRenderFrameId, kRenderProcessId, Params(),
          std::move(observer_ptr), kDefaultDeviceId);

      delegate.OnPlayStream();
      delegate.OnPlayStream();

      SyncWithAllThreads();

      EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()));
    }
    SyncWithAllThreads();
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, std::move(done));
  }

  void CreateDivertTest(base::Closure done) {
    EXPECT_CALL(media_observer_,
                OnCreatingAudioStream(kRenderProcessId, kRenderFrameId));
    EXPECT_CALL(event_handler_, GotOnStreamCreated());
    EXPECT_CALL(mirroring_manager_,
                AddDiverter(kRenderProcessId, kRenderFrameId, NotNull()));
    media::mojom::AudioOutputStreamObserverPtr observer_ptr;
    auto observer_binding = CreateObserverBinding(&observer_ptr);
    EXPECT_CALL(GetMockObserver(&observer_binding), DidStartPlaying()).Times(0);
    EXPECT_CALL(GetMockObserver(&observer_binding), DidStopPlaying()).Times(0);

    DummyAudioOutputStream stream;
    {
      auto socket = std::make_unique<base::CancelableSyncSocket>();
      auto reader = media::AudioSyncReader::Create(base::BindRepeating(&NoLog),
                                                   Params(), socket.get());
      AudioOutputDelegateImpl delegate(
          std::move(reader), std::move(socket), &event_handler_,
          audio_manager_.get(), CreateDummyMojoAudioLog(), &media_observer_,
          kStreamId, kRenderFrameId, kRenderProcessId, Params(),
          std::move(observer_ptr), kDefaultDeviceId);

      delegate.GetControllerForTesting()->StartDiverting(&stream);

      SyncWithAllThreads();

      EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()));
    }
    SyncWithAllThreads();
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, std::move(done));
  }

  void CreateDivertPauseTest(base::Closure done) {
    EXPECT_CALL(media_observer_,
                OnCreatingAudioStream(kRenderProcessId, kRenderFrameId));
    EXPECT_CALL(event_handler_, GotOnStreamCreated());
    EXPECT_CALL(mirroring_manager_,
                AddDiverter(kRenderProcessId, kRenderFrameId, NotNull()));

    DummyAudioOutputStream stream;
    {
      auto socket = std::make_unique<base::CancelableSyncSocket>();
      auto reader = media::AudioSyncReader::Create(base::BindRepeating(&NoLog),
                                                   Params(), socket.get());
      AudioOutputDelegateImpl delegate(
          std::move(reader), std::move(socket), &event_handler_,
          audio_manager_.get(), CreateDummyMojoAudioLog(), &media_observer_,
          kStreamId, kRenderFrameId, kRenderProcessId, Params(), nullptr,
          kDefaultDeviceId);

      delegate.GetControllerForTesting()->StartDiverting(&stream);

      SyncWithAllThreads();
      delegate.OnPauseStream();

      SyncWithAllThreads();

      EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()));
    }
    SyncWithAllThreads();
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, std::move(done));
  }

  void PlayDivertTest(base::Closure done) {
    EXPECT_CALL(media_observer_,
                OnCreatingAudioStream(kRenderProcessId, kRenderFrameId));
    EXPECT_CALL(event_handler_, GotOnStreamCreated());
    EXPECT_CALL(mirroring_manager_,
                AddDiverter(kRenderProcessId, kRenderFrameId, NotNull()));
    media::mojom::AudioOutputStreamObserverPtr observer_ptr;
    auto observer_binding = CreateObserverBinding(&observer_ptr);
    InSequence s;
    EXPECT_CALL(GetMockObserver(&observer_binding), DidStartPlaying());
    EXPECT_CALL(GetMockObserver(&observer_binding), DidStopPlaying());

    DummyAudioOutputStream stream;
    {
      auto socket = std::make_unique<base::CancelableSyncSocket>();
      auto reader = media::AudioSyncReader::Create(base::BindRepeating(&NoLog),
                                                   Params(), socket.get());
      AudioOutputDelegateImpl delegate(
          std::move(reader), std::move(socket), &event_handler_,
          audio_manager_.get(), CreateDummyMojoAudioLog(), &media_observer_,
          kStreamId, kRenderFrameId, kRenderProcessId, Params(),
          std::move(observer_ptr), kDefaultDeviceId);

      delegate.OnPlayStream();
      delegate.GetControllerForTesting()->StartDiverting(&stream);

      SyncWithAllThreads();

      EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()));
    }
    SyncWithAllThreads();
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, std::move(done));
  }

  void TrampolineToUI(base::Closure done,
                      std::unique_ptr<AudioOutputDelegateImpl> delegate) {
    // Destruct and then sync since destruction will post some tasks.
    delegate.reset();
    SyncWithAllThreads();
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, std::move(done));
  }

  void ErrorTest(base::Closure done) {
    EXPECT_CALL(media_observer_,
                OnCreatingAudioStream(kRenderProcessId, kRenderFrameId));
    EXPECT_CALL(event_handler_, GotOnStreamCreated());
    EXPECT_CALL(mirroring_manager_,
                AddDiverter(kRenderProcessId, kRenderFrameId, NotNull()));
    EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()));
    media::mojom::AudioOutputStreamObserverPtr observer_ptr;
    auto observer_binding = CreateObserverBinding(&observer_ptr);
    EXPECT_CALL(GetMockObserver(&observer_binding), DidStartPlaying());
    EXPECT_CALL(GetMockObserver(&observer_binding), DidStopPlaying());

    auto socket = std::make_unique<base::CancelableSyncSocket>();
    auto reader = media::AudioSyncReader::Create(base::BindRepeating(&NoLog),
                                                 Params(), socket.get());
    auto delegate = std::make_unique<AudioOutputDelegateImpl>(
        std::move(reader), std::move(socket), &event_handler_,
        audio_manager_.get(), CreateDummyMojoAudioLog(), &media_observer_,
        kStreamId, kRenderFrameId, kRenderProcessId, Params(),
        std::move(observer_ptr), kDefaultDeviceId);

    delegate->OnPlayStream();
    delegate->GetControllerForTesting()->OnError();

    // Errors are deferred by AudioOutputController, so wait for the error; pass
    // the delegate along since destructing it will close the stream and void
    // the purpose of this test.
    EXPECT_CALL(event_handler_, OnStreamError(kStreamId))
        .WillOnce(media::RunClosure(media::BindToCurrentLoop(base::Bind(
            &AudioOutputDelegateTest::TrampolineToUI, base::Unretained(this),
            std::move(done), base::Passed(&delegate)))));
  }

  void CreateAndDestroyTest(base::Closure done) {
    EXPECT_CALL(media_observer_,
                OnCreatingAudioStream(kRenderProcessId, kRenderFrameId));
    EXPECT_CALL(mirroring_manager_,
                AddDiverter(kRenderProcessId, kRenderFrameId, NotNull()));
    EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()));
    media::mojom::AudioOutputStreamObserverPtr observer_ptr;
    auto observer_binding = CreateObserverBinding(&observer_ptr);
    EXPECT_CALL(GetMockObserver(&observer_binding), DidStartPlaying()).Times(0);
    EXPECT_CALL(GetMockObserver(&observer_binding), DidStopPlaying()).Times(0);

    {
      auto socket = std::make_unique<base::CancelableSyncSocket>();
      auto reader = media::AudioSyncReader::Create(base::BindRepeating(&NoLog),
                                                   Params(), socket.get());
      AudioOutputDelegateImpl delegate(
          std::move(reader), std::move(socket), &event_handler_,
          audio_manager_.get(), CreateDummyMojoAudioLog(), &media_observer_,
          kStreamId, kRenderFrameId, kRenderProcessId, Params(),
          std::move(observer_ptr), kDefaultDeviceId);
    }
    SyncWithAllThreads();
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, std::move(done));
  }

  void PlayAndDestroyTest(base::Closure done) {
    EXPECT_CALL(media_observer_,
                OnCreatingAudioStream(kRenderProcessId, kRenderFrameId));
    EXPECT_CALL(event_handler_, GotOnStreamCreated());
    EXPECT_CALL(mirroring_manager_,
                AddDiverter(kRenderProcessId, kRenderFrameId, NotNull()));
    EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()));
    media::mojom::AudioOutputStreamObserverPtr observer_ptr;
    auto observer_binding = CreateObserverBinding(&observer_ptr);
    EXPECT_CALL(GetMockObserver(&observer_binding), DidStartPlaying()).Times(0);
    EXPECT_CALL(GetMockObserver(&observer_binding), DidStopPlaying()).Times(0);

    {
      auto socket = std::make_unique<base::CancelableSyncSocket>();
      auto reader = media::AudioSyncReader::Create(base::BindRepeating(&NoLog),
                                                   Params(), socket.get());
      AudioOutputDelegateImpl delegate(
          std::move(reader), std::move(socket), &event_handler_,
          audio_manager_.get(), CreateDummyMojoAudioLog(), &media_observer_,
          kStreamId, kRenderFrameId, kRenderProcessId, Params(),
          std::move(observer_ptr), kDefaultDeviceId);

      SyncWithAllThreads();

      delegate.OnPlayStream();
    }
    SyncWithAllThreads();
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, std::move(done));
  }

  void ErrorAndDestroyTest(base::Closure done) {
    EXPECT_CALL(media_observer_,
                OnCreatingAudioStream(kRenderProcessId, kRenderFrameId));
    EXPECT_CALL(event_handler_, GotOnStreamCreated());
    EXPECT_CALL(mirroring_manager_,
                AddDiverter(kRenderProcessId, kRenderFrameId, NotNull()));
    EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()));
    media::mojom::AudioOutputStreamObserverPtr observer_ptr;
    auto observer_binding = CreateObserverBinding(&observer_ptr);
    EXPECT_CALL(GetMockObserver(&observer_binding), DidStartPlaying()).Times(0);
    EXPECT_CALL(GetMockObserver(&observer_binding), DidStopPlaying()).Times(0);

    {
      auto socket = std::make_unique<base::CancelableSyncSocket>();
      auto reader = media::AudioSyncReader::Create(base::BindRepeating(&NoLog),
                                                   Params(), socket.get());
      AudioOutputDelegateImpl delegate(
          std::move(reader), std::move(socket), &event_handler_,
          audio_manager_.get(), CreateDummyMojoAudioLog(), &media_observer_,
          kStreamId, kRenderFrameId, kRenderProcessId, Params(),
          std::move(observer_ptr), kDefaultDeviceId);
      SyncWithAllThreads();

      delegate.GetControllerForTesting()->OnError();
    }
    SyncWithAllThreads();
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, std::move(done));
  }

 protected:
  std::unique_ptr<TestBrowserThreadBundle> thread_bundle_;
  std::unique_ptr<media::AudioManager> audio_manager_;
  StrictMock<MockAudioMirroringManager> mirroring_manager_;
  StrictMock<MockEventHandler> event_handler_;
  StrictMock<MockObserver> media_observer_;
  media::FakeAudioLogFactory log_factory_;

 private:
  void SyncWithAllThreads() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    // New tasks might be posted while we are syncing, but in every iteration at
    // least one task will be run. 20 iterations should be enough for our code.
    for (int i = 0; i < 20; ++i) {
      base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
      SyncWith(
          base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI}));
      SyncWith(audio_manager_->GetWorkerTaskRunner());
    }
  }

  void SyncWith(scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    CHECK(task_runner);
    CHECK(!task_runner->BelongsToCurrentThread());
    base::WaitableEvent e = {base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED};
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(&e)));
    e.Wait();
  }

  DISALLOW_COPY_AND_ASSIGN(AudioOutputDelegateTest);
};

TEST_F(AudioOutputDelegateTest, Create) {
  base::RunLoop l;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AudioOutputDelegateTest::CreateTest,
                     base::Unretained(this), l.QuitClosure()));
  l.Run();
}

TEST_F(AudioOutputDelegateTest, Play) {
  base::RunLoop l;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AudioOutputDelegateTest::PlayTest, base::Unretained(this),
                     l.QuitClosure(), true /* use_bound_observer */));
  l.Run();
}

TEST_F(AudioOutputDelegateTest, PlayWithUnboundObserver) {
  base::RunLoop l;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AudioOutputDelegateTest::PlayTest, base::Unretained(this),
                     l.QuitClosure(), false /* use_bound_observer */));
  l.Run();
}

TEST_F(AudioOutputDelegateTest, Pause) {
  base::RunLoop l;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AudioOutputDelegateTest::PauseTest,
                     base::Unretained(this), l.QuitClosure()));
  l.Run();
}

TEST_F(AudioOutputDelegateTest, PlayPausePlay) {
  base::RunLoop l;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AudioOutputDelegateTest::PlayPausePlayTest,
                     base::Unretained(this), l.QuitClosure()));
  l.Run();
}

TEST_F(AudioOutputDelegateTest, PlayPlay) {
  base::RunLoop l;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AudioOutputDelegateTest::PlayPlayTest,
                     base::Unretained(this), l.QuitClosure()));
  l.Run();
}

TEST_F(AudioOutputDelegateTest, PlayDivert) {
  base::RunLoop l;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AudioOutputDelegateTest::PlayDivertTest,
                     base::Unretained(this), l.QuitClosure()));
  l.Run();
}

TEST_F(AudioOutputDelegateTest, CreateDivert) {
  base::RunLoop l;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AudioOutputDelegateTest::CreateDivertTest,
                     base::Unretained(this), l.QuitClosure()));
  l.Run();
}

TEST_F(AudioOutputDelegateTest, Error) {
  base::RunLoop l;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AudioOutputDelegateTest::ErrorTest,
                     base::Unretained(this), l.QuitClosure()));
  l.Run();
}

TEST_F(AudioOutputDelegateTest, CreateAndDestroy) {
  base::RunLoop l;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AudioOutputDelegateTest::CreateAndDestroyTest,
                     base::Unretained(this), l.QuitClosure()));
  l.Run();
}

TEST_F(AudioOutputDelegateTest, PlayAndDestroy) {
  base::RunLoop l;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AudioOutputDelegateTest::PlayAndDestroyTest,
                     base::Unretained(this), l.QuitClosure()));
  l.Run();
}

TEST_F(AudioOutputDelegateTest, ErrorAndDestroy) {
  base::RunLoop l;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AudioOutputDelegateTest::PlayAndDestroyTest,
                     base::Unretained(this), l.QuitClosure()));
  l.Run();
}

}  // namespace content
