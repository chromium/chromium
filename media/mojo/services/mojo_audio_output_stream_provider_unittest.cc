// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_audio_output_stream_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/audio/audio_output_delegate.h"
#include "media/base/audio_parameters.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Basic usage of MojoAudioOutputStreamProvider is tested in
// the RenderFrameAudioOutputStreamFactoryTest tests.
// These additional tests test some error conditions.

namespace media {

namespace {

using testing::DeleteArg;
using testing::Mock;
using testing::StrictMock;

using MockDeleter = base::MockCallback<
    base::OnceCallback<void(mojom::AudioOutputStreamProvider*)>>;

class FakeObserver : public mojom::AudioOutputStreamObserver {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  void DidStartPlaying() override {}
  void DidStopPlaying() override {}
  void DidChangeAudibleState(bool is_audible) override {}
};

class FakeDelegate : public AudioOutputDelegate {
 public:
  explicit FakeDelegate(
      mojo::PendingRemote<media::mojom::AudioOutputStreamObserver>
          pending_observer)
      : observer_(std::move(pending_observer)) {}
  ~FakeDelegate() override = default;

  int GetStreamId() override { return 0; }
  void OnPlayStream() override {}
  void OnPauseStream() override {}
  void OnFlushStream() override {}
  void OnSetVolume(double) override {}

 private:
  mojo::PendingRemote<media::mojom::AudioOutputStreamObserver> observer_;
};

std::unique_ptr<AudioOutputDelegate> CreateFakeDelegate(
    const AudioParameters& params,
    mojo::PendingRemote<media::mojom::AudioOutputStreamObserver>
        pending_observer,
    AudioOutputDelegate::EventHandler*) {
  return std::make_unique<FakeDelegate>(std::move(pending_observer));
}

}  // namespace

TEST(MojoAudioOutputStreamProviderTest, AcquireTwice_BadMessage) {
  base::test::SingleThreadTaskEnvironment task_environment;
  bool got_bad_message = false;
  mojo::core::SetDefaultProcessErrorCallback(
      base::BindRepeating([](bool* got_bad_message,
                             const std::string& s) { *got_bad_message = true; },
                          &got_bad_message));

  mojo::Remote<mojom::AudioOutputStreamProvider> provider_remote;
  StrictMock<MockDeleter> deleter;

  // Freed by deleter.
  auto* provider = new MojoAudioOutputStreamProvider(
      provider_remote.BindNewPipeAndPassReceiver(),
      base::BindOnce(&CreateFakeDelegate), deleter.Get(),
      std::make_unique<FakeObserver>());

  mojo::PendingRemote<mojom::AudioOutputStreamProviderClient> client_1;
  ignore_result(client_1.InitWithNewPipeAndPassReceiver());
  provider_remote->Acquire(media::AudioParameters::UnavailableDeviceParams(),
                           std::move(client_1), base::nullopt);

  mojo::PendingRemote<mojom::AudioOutputStreamProviderClient> client_2;
  ignore_result(client_2.InitWithNewPipeAndPassReceiver());
  provider_remote->Acquire(media::AudioParameters::UnavailableDeviceParams(),
                           std::move(client_2), base::nullopt);

  EXPECT_CALL(deleter, Run(provider)).WillOnce(DeleteArg<0>());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(got_bad_message);
  Mock::VerifyAndClear(&deleter);

  mojo::core::SetDefaultProcessErrorCallback(
      mojo::core::ProcessErrorCallback());
}

TEST(MojoAudioOutputStreamProviderTest,
     Bitstream_BadMessageOnNonAndoirdPlatforms) {
  base::test::SingleThreadTaskEnvironment task_environment;
  bool got_bad_message = false;
  mojo::core::SetDefaultProcessErrorCallback(
      base::BindRepeating([](bool* got_bad_message,
                             const std::string& s) { *got_bad_message = true; },
                          &got_bad_message));

  mojo::Remote<mojom::AudioOutputStreamProvider> provider_remote;
  StrictMock<MockDeleter> deleter;
  media::AudioParameters params =
      media::AudioParameters::UnavailableDeviceParams();
  params.set_format(AudioParameters::AUDIO_BITSTREAM_AC3);

  auto* provider = new MojoAudioOutputStreamProvider(
      provider_remote.BindNewPipeAndPassReceiver(),
      base::BindOnce(&CreateFakeDelegate), deleter.Get(),
      std::make_unique<FakeObserver>());

  mojo::PendingRemote<mojom::AudioOutputStreamProviderClient> client;
  ignore_result(client.InitWithNewPipeAndPassReceiver());
  provider_remote->Acquire(params, std::move(client), base::nullopt);

#if defined(OS_ANDROID)
  base::RunLoop().RunUntilIdle();
  // Creating bitstream streams is allowed on Android.
  EXPECT_FALSE(got_bad_message);
  // |deleter| shouldn't have been called, so delete manually.
  Mock::VerifyAndClear(&deleter);
  delete provider;
#else
  EXPECT_CALL(deleter, Run(provider)).WillOnce(DeleteArg<0>());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(got_bad_message);
  Mock::VerifyAndClear(&deleter);
#endif
  mojo::core::SetDefaultProcessErrorCallback(
      mojo::core::ProcessErrorCallback());
}

}  // namespace media
