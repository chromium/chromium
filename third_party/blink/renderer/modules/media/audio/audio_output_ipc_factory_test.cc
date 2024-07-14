// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/modules/media/audio/audio_output_ipc_factory.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/threading/thread.h"
#include "media/audio/audio_output_ipc.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/media/renderer_audio_output_stream_factory.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

using ::testing::_;

namespace blink {

namespace {

const int kRenderFrameId = 0;

blink::LocalFrameToken TokenFromInt(int i) {
  static base::UnguessableToken base_token = base::UnguessableToken::Create();
  return blink::LocalFrameToken(base::UnguessableToken::CreateForTesting(
      base_token.GetHighForSerialization() + i,
      base_token.GetLowForSerialization() + i));
}

std::unique_ptr<base::Thread> MakeIOThread() {
  auto io_thread = std::make_unique<base::Thread>("test IO thread");
  base::Thread::Options thread_options(base::MessagePumpType::IO, 0);
  CHECK(io_thread->StartWithOptions(std::move(thread_options)));
  return io_thread;
}

class FakeRemoteFactory
    : public mojom::blink::RendererAudioOutputStreamFactory {
 public:
  FakeRemoteFactory() = default;
  ~FakeRemoteFactory() override {}

  void RequestDeviceAuthorization(
      mojo::PendingReceiver<media::mojom::blink::AudioOutputStreamProvider>
          stream_provider,
      const std::optional<base::UnguessableToken>& session_id,
      const String& device_id,
      RequestDeviceAuthorizationCallback callback) override {
    std::move(callback).Run(
        static_cast<media::mojom::blink::OutputDeviceStatus>(
            media::OutputDeviceStatus::
                OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED),
        media::AudioParameters::UnavailableDeviceParams(), WTF::g_empty_string);
    EXPECT_FALSE(on_called_.is_null());
    std::move(on_called_).Run();
  }

  void SetOnCalledCallback(base::OnceClosure on_called) {
    on_called_ = std::move(on_called);
  }

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    EXPECT_FALSE(receiver_.is_bound());
    receiver_.Bind(
        mojo::PendingReceiver<mojom::blink::RendererAudioOutputStreamFactory>(
            std::move(handle)));
  }

 private:
  mojo::Receiver<mojom::blink::RendererAudioOutputStreamFactory> receiver_{
      this};
  base::OnceClosure on_called_;
};

class FakeAudioOutputIPCDelegate : public media::AudioOutputIPCDelegate {
  void OnError() override {}
  void OnDeviceAuthorized(media::OutputDeviceStatus device_status,
                          const media::AudioParameters& output_params,
                          const std::string& matched_device_id) override {}
  void OnStreamCreated(base::UnsafeSharedMemoryRegion region,
                       base::SyncSocket::ScopedHandle socket_handle,
                       bool playing_automatically) override {}
  void OnIPCClosed() override {}
};

}  // namespace

class AudioOutputIPCFactoryTest : public testing::Test {
 public:
  AudioOutputIPCFactoryTest() = default;
  ~AudioOutputIPCFactoryTest() override = default;

  void RequestAuthorizationOnIOThread(
      std::unique_ptr<media::AudioOutputIPC> output_ipc) {
    output_ipc->RequestDeviceAuthorization(&fake_delegate,
                                           base::UnguessableToken(), "");

    output_ipc->CloseStream();
  }

 private:
  FakeAudioOutputIPCDelegate fake_delegate;
  test::TaskEnvironment task_environment_;
};

TEST_F(AudioOutputIPCFactoryTest, CallFactoryFromIOThread) {
  // This test makes sure that AudioOutputIPCFactory correctly binds the
  // RendererAudioOutputStreamFactory to the IO thread.
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform;
  base::RunLoop run_loop;
  auto io_thread = MakeIOThread();

  FakeRemoteFactory remote_factory;
  remote_factory.SetOnCalledCallback(run_loop.QuitWhenIdleClosure());

  auto& interface_broker = blink::GetEmptyBrowserInterfaceBroker();
  interface_broker.SetBinderForTesting(
      mojom::blink::RendererAudioOutputStreamFactory::Name_,
      base::BindRepeating(&FakeRemoteFactory::Bind,
                          base::Unretained(&remote_factory)));

  AudioOutputIPCFactory ipc_factory(io_thread->task_runner());

  ipc_factory.RegisterRemoteFactory(TokenFromInt(kRenderFrameId),
                                    interface_broker);

  // To make sure that the pointer stored in |ipc_factory| is connected to
  // |remote_factory|, and also that it's bound to |io_thread|, we create an
  // AudioOutputIPC object and request device authorization on the IO thread.
  // This is supposed to call |remote_factory| on the main thread.
  io_thread->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AudioOutputIPCFactoryTest::RequestAuthorizationOnIOThread,
          base::Unretained(this),
          ipc_factory.CreateAudioOutputIPC(TokenFromInt(kRenderFrameId))));

  // Wait for call to |remote_factory|:
  run_loop.Run();

  ipc_factory.MaybeDeregisterRemoteFactory(TokenFromInt(0));

  interface_broker.SetBinderForTesting(
      mojom::blink::RendererAudioOutputStreamFactory::Name_, {});

  io_thread.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioOutputIPCFactoryTest, SeveralFactories) {
  // This test simulates having several frames being created and destructed.
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform;
  auto io_thread = MakeIOThread();
  const int n_factories = 5;

  std::vector<FakeRemoteFactory> remote_factories(n_factories);

  auto& interface_broker = blink::GetEmptyBrowserInterfaceBroker();

  interface_broker.SetBinderForTesting(
      mojom::blink::RendererAudioOutputStreamFactory::Name_,
      base::BindLambdaForTesting([&](mojo::ScopedMessagePipeHandle handle) {
        static int factory_index = 0;
        DCHECK_LT(factory_index, n_factories);
        remote_factories[factory_index++].Bind(std::move(handle));
      }));

  base::RunLoop().RunUntilIdle();

  AudioOutputIPCFactory ipc_factory(io_thread->task_runner());

  for (int i = 0; i < n_factories; i++) {
    ipc_factory.RegisterRemoteFactory(TokenFromInt(kRenderFrameId + i),
                                      interface_broker);
  }

  base::RunLoop run_loop;
  remote_factories[0].SetOnCalledCallback(run_loop.QuitWhenIdleClosure());
  io_thread->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AudioOutputIPCFactoryTest::RequestAuthorizationOnIOThread,
          base::Unretained(this),
          ipc_factory.CreateAudioOutputIPC(TokenFromInt(kRenderFrameId))));
  run_loop.Run();

  // Do some operation and make sure the internal state isn't messed up:
  ipc_factory.MaybeDeregisterRemoteFactory(TokenFromInt(1));

  base::RunLoop run_loop2;
  remote_factories[2].SetOnCalledCallback(run_loop2.QuitWhenIdleClosure());
  io_thread->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AudioOutputIPCFactoryTest::RequestAuthorizationOnIOThread,
          base::Unretained(this),
          ipc_factory.CreateAudioOutputIPC(TokenFromInt(kRenderFrameId + 2))));
  run_loop2.Run();

  for (int i = 0; i < n_factories; i++) {
    if (i == 1)
      continue;
    ipc_factory.MaybeDeregisterRemoteFactory(TokenFromInt(i));
  }

  interface_broker.SetBinderForTesting(
      mojom::blink::RendererAudioOutputStreamFactory::Name_, {});

  io_thread.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioOutputIPCFactoryTest, RegisterDeregisterBackToBack_Deregisters) {
  // This test makes sure that calling Register... followed by Deregister...
  // correctly sequences the registration before the deregistration.
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform;

  auto io_thread = MakeIOThread();

  FakeRemoteFactory remote_factory;

  auto& interface_broker = blink::GetEmptyBrowserInterfaceBroker();
  interface_broker.SetBinderForTesting(
      mojom::blink::RendererAudioOutputStreamFactory::Name_,
      base::BindRepeating(&FakeRemoteFactory::Bind,
                          base::Unretained(&remote_factory)));

  AudioOutputIPCFactory ipc_factory(io_thread->task_runner());

  ipc_factory.RegisterRemoteFactory(TokenFromInt(kRenderFrameId),
                                    interface_broker);
  ipc_factory.MaybeDeregisterRemoteFactory(TokenFromInt(kRenderFrameId));
  // That there is no factory remaining at destruction is DCHECKed in the
  // AudioOutputIPCFactory destructor.

  base::RunLoop().RunUntilIdle();

  interface_broker.SetBinderForTesting(
      mojom::blink::RendererAudioOutputStreamFactory::Name_, {});
  io_thread.reset();
  base::RunLoop().RunUntilIdle();
}

}  // namespace blink
