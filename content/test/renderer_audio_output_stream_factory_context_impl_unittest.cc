// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/renderer_audio_output_stream_factory_context_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/shared_memory.h"
#include "base/memory/shared_memory_handle.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/sync_socket.h"
#include "base/task/post_task.h"
#include "cc/base/math_util.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/common/media/renderer_audio_output_stream_factory.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/renderer/media/audio/mojo_audio_output_ipc.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_output_controller.h"
#include "media/audio/audio_output_device.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/audio_thread_impl.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/simple_sources.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using testing::_;
using testing::StrictMock;
using testing::Return;
using testing::Test;

const int kRenderProcessId = 42;
const int kRenderFrameId = 24;
const float kWaveFrequency = 440.f;
const int kChannels = 1;
const int kBuffers = 1000;
const int kSampleFrequency = 44100;
const int kSamplesPerBuffer = kSampleFrequency / 100;

std::unique_ptr<media::AudioOutputStream::AudioSourceCallback>
GetTestAudioSource() {
  return std::make_unique<media::SineWaveAudioSource>(kChannels, kWaveFrequency,
                                                      kSampleFrequency);
}

media::AudioParameters GetTestAudioParameters() {
  return media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_MONO, kSampleFrequency,
                                kSamplesPerBuffer);
}

void SyncWith(scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  CHECK(task_runner);
  CHECK(!task_runner->BelongsToCurrentThread());
  base::WaitableEvent e = {base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED};
  task_runner->PostTask(FROM_HERE, base::BindOnce(&base::WaitableEvent::Signal,
                                                  base::Unretained(&e)));
  e.Wait();
}

void SyncWithAllThreads() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // New tasks might be posted while we are syncing, but in every iteration at
  // least one task will be run. 20 iterations should be enough for our code.
  for (int i = 0; i < 20; ++i) {
    base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
    SyncWith(base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));
    SyncWith(media::AudioManager::Get()->GetWorkerTaskRunner());
  }
}

class MockAudioOutputStream : public media::AudioOutputStream,
                              public base::PlatformThread::Delegate {
 public:
  MockAudioOutputStream()
      : done_(base::WaitableEvent::ResetPolicy::MANUAL,
              base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  ~MockAudioOutputStream() override {
    base::PlatformThread::Join(thread_handle_);
  }

  void Wait() { done_.Wait(); }

  void Start(AudioSourceCallback* callback) override {
    callback_ = callback;
    EXPECT_TRUE(base::PlatformThread::CreateWithPriority(
        0, this, &thread_handle_, base::ThreadPriority::REALTIME_AUDIO));
  }

  void Stop() override {
    done_.Wait();
    callback_ = nullptr;
  }

  bool Open() override { return true; }
  void SetVolume(double volume) override {}
  void GetVolume(double* volume) override { *volume = 1; }
  void Close() override {
    Stop();
    delete this;
  }

  void ThreadMain() override {
    std::unique_ptr<media::AudioOutputStream::AudioSourceCallback>
        expected_audio = GetTestAudioSource();
    media::AudioParameters params = GetTestAudioParameters();
    std::unique_ptr<media::AudioBus> dest = media::AudioBus::Create(params);
    std::unique_ptr<media::AudioBus> expected_buffer =
        media::AudioBus::Create(params);
    for (int i = 0; i < kBuffers; ++i) {
      expected_audio->OnMoreData(base::TimeDelta(), base::TimeTicks::Now(), 0,
                                 expected_buffer.get());
      callback_->OnMoreData(base::TimeDelta(), base::TimeTicks::Now(), 0,
                            dest.get());
      for (int frame = 0; frame < params.frames_per_buffer(); ++frame) {
        // Using EXPECT here causes massive log spam in case of a broken test,
        // and ASSERT causes it to hang, so we use CHECK.
        CHECK(cc::MathUtil::IsFloatNearlyTheSame(
            expected_buffer->channel(0)[frame], dest->channel(0)[frame]))
            << "Got " << dest->channel(0)[frame] << ", expected "
            << expected_buffer->channel(0)[frame];
      }
    }
    done_.Signal();
  }

 private:
  base::OnceClosure sync_closure_;
  base::PlatformThreadHandle thread_handle_;
  base::WaitableEvent done_;
  AudioSourceCallback* callback_;
};

class TestRenderCallback : public media::AudioRendererSink::RenderCallback {
 public:
  TestRenderCallback() : source_(GetTestAudioSource()) {}

  ~TestRenderCallback() override {}

  int Render(base::TimeDelta delay,
             base::TimeTicks delay_timestamp,
             int prior_frames_skipped,
             media::AudioBus* dest) override {
    return source_->OnMoreData(delay, delay_timestamp, prior_frames_skipped,
                               dest);
  }

  MOCK_METHOD0(OnRenderError, void());

 private:
  std::unique_ptr<media::AudioOutputStream::AudioSourceCallback> source_;
};

}  // namespace

// TODO(maxmorin): Add test for play, pause and set volume.
class RendererAudioOutputStreamFactoryIntegrationTest : public Test {
 public:
  RendererAudioOutputStreamFactoryIntegrationTest()
      : media_stream_manager_(),
        thread_bundle_(TestBrowserThreadBundle::Options::REAL_IO_THREAD),
        audio_manager_(std::make_unique<media::AudioThreadImpl>()),
        audio_system_(&audio_manager_) {
    media_stream_manager_ = std::make_unique<MediaStreamManager>(
        &audio_system_, audio_manager_.GetTaskRunner());
  }

  ~RendererAudioOutputStreamFactoryIntegrationTest() override {
    audio_manager_.Shutdown();
  }

  UniqueAudioOutputStreamFactoryPtr CreateAndBindFactory(
      mojom::RendererAudioOutputStreamFactoryRequest request) {
    factory_context_.reset(new RendererAudioOutputStreamFactoryContextImpl(
        kRenderProcessId, &audio_system_, &audio_manager_,
        media_stream_manager_.get()));
    return RenderFrameAudioOutputStreamFactoryHandle::CreateFactory(
        factory_context_.get(), kRenderFrameId, std::move(request));
  }

  std::unique_ptr<MediaStreamManager> media_stream_manager_;
  TestBrowserThreadBundle thread_bundle_;
  media::MockAudioManager audio_manager_;
  media::AudioSystemImpl audio_system_;
  std::unique_ptr<RendererAudioOutputStreamFactoryContextImpl,
                  BrowserThread::DeleteOnIOThread>
      factory_context_;
};

TEST_F(RendererAudioOutputStreamFactoryIntegrationTest, StreamIntegrationTest) {
  // Sets up the factory on the IO thread and runs client code on the UI thread.
  // Send a sine wave from the client and makes sure it's received by the output
  // stream.
  MockAudioOutputStream* stream = new MockAudioOutputStream();

  // Make sure the mock audio manager uses our mock stream.
  bool create_stream_called = false;
  audio_manager_.SetMakeOutputStreamCB(base::BindRepeating(
      [](bool* create_stream_called, media::AudioOutputStream* stream,
         const media::AudioParameters& params,
         const std::string& name) -> media::AudioOutputStream* {
        DCHECK(!*create_stream_called);
        DCHECK(stream);
        DCHECK_EQ(name, "default");
        DCHECK_EQ(params.AsHumanReadableString(),
                  GetTestAudioParameters().AsHumanReadableString());
        *create_stream_called = true;
        return stream;
      },
      &create_stream_called, stream));

  mojom::RendererAudioOutputStreamFactoryPtr stream_factory;
  auto factory_handle =
      CreateAndBindFactory(mojo::MakeRequest(&stream_factory));

  base::Thread renderer_side_ipc_thread("Renderer IPC thread");
  ASSERT_TRUE(renderer_side_ipc_thread.Start());
  auto renderer_ipc_task_runner =
      renderer_side_ipc_thread.message_loop()->task_runner();

  // Bind |stream_factory| to |renderer_ipc_task_runner|.
  mojom::RendererAudioOutputStreamFactory* factory_ptr;
  renderer_ipc_task_runner->PostTask(
      FROM_HERE, base::BindOnce(
                     [](mojom::RendererAudioOutputStreamFactoryPtr* factory,
                        mojom::RendererAudioOutputStreamFactoryPtrInfo info,
                        mojom::RendererAudioOutputStreamFactory** ptr) {
                       factory->Bind(std::move(info));
                       *ptr = factory->get();
                     },
                     base::Unretained(&stream_factory),
                     stream_factory.PassInterface(), &factory_ptr));
  // Wait for factory_ptr to be set.
  SyncWith(renderer_ipc_task_runner);

  auto renderer_side_ipc = std::make_unique<MojoAudioOutputIPC>(
      base::BindRepeating(
          [](mojom::RendererAudioOutputStreamFactory* factory_ptr) {
            return factory_ptr;
          },
          factory_ptr),
      renderer_ipc_task_runner);

  auto device = base::MakeRefCounted<media::AudioOutputDevice>(
      std::move(renderer_side_ipc), renderer_ipc_task_runner,
      media::AudioSinkParameters(), base::TimeDelta());

  StrictMock<TestRenderCallback> source;

  device->Initialize(GetTestAudioParameters(), &source);
  device->Start();
  device->Play();
  // Wait for stream to start.
  SyncWithAllThreads();
  // Wait for stream to finish. Verifies data.
  stream->Wait();
  device->Stop();

  // |stream_factory| must be destroyed on the correct thread.
  renderer_ipc_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce([](mojom::RendererAudioOutputStreamFactoryPtr) {},
                     std::move(stream_factory)));
  SyncWith(renderer_ipc_task_runner);
  // Wait for any clean-up tasks.
  SyncWithAllThreads();
}

}  // namespace content
