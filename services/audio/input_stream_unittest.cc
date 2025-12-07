// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/input_stream.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "media/audio/audio_io.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/mojo/mojom/audio_processing.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "services/audio/ml_model_manager.h"
#include "services/audio/stream_factory.h"
#include "services/audio/test/mock_log.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::NotNull;
using testing::Return;
using testing::StrictMock;

namespace audio {

namespace {

const uint32_t kDefaultSharedMemoryCount = 10;
const bool kDoEnableAGC = true;
const bool kDoNotEnableAGC = false;
const bool kValidStream = true;
const bool kInvalidStream = false;
const bool kMuted = true;
const bool kNotMuted = false;
const char* kDefaultDeviceId = "default";

class MockMlModelManager : public MlModelManager {
 public:
  std::unique_ptr<MlModelHandle> GetResidualEchoEstimationModel() override {
    model_requested_ = true;
    return nullptr;
  }
  bool model_requested() { return model_requested_; }

 private:
  bool model_requested_ = false;
};

class MockStreamClient : public media::mojom::AudioInputStreamClient {
 public:
  MockStreamClient() = default;

  MockStreamClient(const MockStreamClient&) = delete;
  MockStreamClient& operator=(const MockStreamClient&) = delete;

  mojo::PendingRemote<media::mojom::AudioInputStreamClient> MakeRemote() {
    DCHECK(!receiver_.is_bound());
    mojo::PendingRemote<media::mojom::AudioInputStreamClient> remote;
    receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
    receiver_.set_disconnect_handler(base::BindOnce(
        &MockStreamClient::BindingConnectionError, base::Unretained(this)));
    return remote;
  }

  void CloseBinding() { receiver_.reset(); }

  MOCK_METHOD1(OnError, void(media::mojom::InputStreamErrorCode));
  MOCK_METHOD1(OnMutedStateChanged, void(bool));
  MOCK_METHOD0(BindingConnectionError, void());

 private:
  mojo::Receiver<media::mojom::AudioInputStreamClient> receiver_{this};
};

class MockStreamObserver : public media::mojom::AudioInputStreamObserver {
 public:
  MockStreamObserver() = default;

  MockStreamObserver(const MockStreamObserver&) = delete;
  MockStreamObserver& operator=(const MockStreamObserver&) = delete;

  mojo::PendingRemote<media::mojom::AudioInputStreamObserver> MakeRemote() {
    DCHECK(!receiver_.is_bound());
    mojo::PendingRemote<media::mojom::AudioInputStreamObserver> remote;
    receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
    receiver_.set_disconnect_handler(base::BindOnce(
        &MockStreamObserver::BindingConnectionError, base::Unretained(this)));
    return remote;
  }

  void CloseBinding() { receiver_.reset(); }

  MOCK_METHOD0(DidStartRecording, void());

  MOCK_METHOD0(BindingConnectionError, void());

 private:
  mojo::Receiver<media::mojom::AudioInputStreamObserver> receiver_{this};
};

class MockStream : public media::AudioInputStream {
 public:
  MockStream() {}

  MockStream(const MockStream&) = delete;
  MockStream& operator=(const MockStream&) = delete;

  double GetMaxVolume() override { return 1; }

  MOCK_METHOD0(Open, media::AudioInputStream::OpenOutcome());
  MOCK_METHOD1(Start, void(AudioInputCallback*));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(Close, void());
  MOCK_METHOD1(SetVolume, void(double));
  MOCK_METHOD0(GetVolume, double());
  MOCK_METHOD1(SetAutomaticGainControl, bool(bool));
  MOCK_METHOD0(GetAutomaticGainControl, bool());
  MOCK_METHOD0(IsMuted, bool());
  MOCK_METHOD1(SetOutputDeviceForAec, void(const std::string&));
};

}  // namespace

class AudioServiceInputStreamTest : public testing::Test {
 public:
  AudioServiceInputStreamTest()
      : audio_manager_(std::make_unique<media::TestAudioThread>(false)),
        stream_factory_(&audio_manager_,
                        /*aecdump_recording_manager=*/nullptr,
                        &mock_ml_model_manager_),
        stream_factory_receiver_(
            &stream_factory_,
            remote_stream_factory_.BindNewPipeAndPassReceiver()) {}

  AudioServiceInputStreamTest(const AudioServiceInputStreamTest&) = delete;
  AudioServiceInputStreamTest& operator=(const AudioServiceInputStreamTest&) =
      delete;

  ~AudioServiceInputStreamTest() override { audio_manager_.Shutdown(); }

  void SetUp() override {
    mojo::SetDefaultProcessErrorHandler(
        base::BindRepeating(&AudioServiceInputStreamTest::BadMessageCallback,
                            base::Unretained(this)));
  }

  void TearDown() override {
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  mojo::PendingRemote<media::mojom::AudioInputStream> CreateStream(
      bool enable_agc) {
    mojo::PendingRemote<media::mojom::AudioInputStream> remote_stream;
    remote_stream_factory_->CreateInputStream(
        remote_stream.InitWithNewPipeAndPassReceiver(), client_.MakeRemote(),
        observer_.MakeRemote(), log_.MakeRemote(), kDefaultDeviceId,
        media::AudioParameters::UnavailableDeviceParams(),
        base::UnguessableToken::Create(), kDefaultSharedMemoryCount, enable_agc,
        std::move(processing_config_),
        base::BindOnce(&AudioServiceInputStreamTest::OnCreated,
                       base::Unretained(this)));
    return remote_stream;
  }

  mojo::PendingRemote<media::mojom::AudioInputStream>
  CreateStreamWithNullptrLog() {
    mojo::PendingRemote<media::mojom::AudioInputStream> remote_stream;
    remote_stream_factory_->CreateInputStream(
        remote_stream.InitWithNewPipeAndPassReceiver(), client_.MakeRemote(),
        observer_.MakeRemote(), mojo::NullRemote(), kDefaultDeviceId,
        media::AudioParameters::UnavailableDeviceParams(),
        base::UnguessableToken::Create(), kDefaultSharedMemoryCount, false,
        std::move(processing_config_),
        base::BindOnce(&AudioServiceInputStreamTest::OnCreated,
                       base::Unretained(this)));
    return remote_stream;
  }

  mojo::PendingRemote<media::mojom::AudioInputStream>
  CreateStreamWithNullptrObserver() {
    mojo::PendingRemote<media::mojom::AudioInputStream> remote_stream;
    remote_stream_factory_->CreateInputStream(
        remote_stream.InitWithNewPipeAndPassReceiver(), client_.MakeRemote(),
        mojo::NullRemote(), log_.MakeRemote(), kDefaultDeviceId,
        media::AudioParameters::UnavailableDeviceParams(),
        base::UnguessableToken::Create(), kDefaultSharedMemoryCount, false,
        std::move(processing_config_),
        base::BindOnce(&AudioServiceInputStreamTest::OnCreated,
                       base::Unretained(this)));
    return remote_stream;
  }

  media::MockAudioManager& audio_manager() { return audio_manager_; }

  MockStreamClient& client() { return client_; }

  MockStreamObserver& observer() { return observer_; }

  MockLog& log() { return log_; }

  void OnCreated(media::mojom::ReadWriteAudioDataPipePtr ptr,
                 bool initially_muted,
                 const std::optional<base::UnguessableToken>& stream_id) {
    EXPECT_EQ(stream_id.has_value(), !!ptr);
    CreatedCallback(!!ptr, initially_muted);
  }

  MOCK_METHOD2(CreatedCallback, void(bool /*valid*/, bool /*initially_muted*/));
  MOCK_METHOD1(BadMessageCallback, void(const std::string&));

 protected:
  base::test::TaskEnvironment scoped_task_env_;
  media::MockAudioManager audio_manager_;
  MockMlModelManager mock_ml_model_manager_;
  media::mojom::AudioProcessingConfigPtr processing_config_ = nullptr;
  StreamFactory stream_factory_;
  mojo::Remote<media::mojom::AudioStreamFactory> remote_stream_factory_;
  mojo::Receiver<media::mojom::AudioStreamFactory> stream_factory_receiver_;
  StrictMock<MockStreamClient> client_;
  StrictMock<MockStreamObserver> observer_;
  NiceMock<MockLog> log_;
};

TEST_F(AudioServiceInputStreamTest, ConstructDestruct) {
  NiceMock<MockStream> mock_stream;
  audio_manager().SetMakeInputStreamCB(base::BindRepeating(
      [](media::AudioInputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open())
      .WillOnce(Return(MockStream::OpenOutcome::kSuccess));
  EXPECT_CALL(mock_stream, IsMuted()).WillOnce(Return(kNotMuted));
  EXPECT_CALL(log(), OnCreated(_, _));
  EXPECT_CALL(*this, CreatedCallback(kValidStream, kNotMuted));
  mojo::Remote<media::mojom::AudioInputStream> remote_stream(
      CreateStream(kDoNotEnableAGC));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(log(), OnClosed());
  EXPECT_CALL(client(), BindingConnectionError());
  EXPECT_CALL(observer(), BindingConnectionError());
  remote_stream.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioServiceInputStreamTest, ConstructDestructNullptrLog) {
  NiceMock<MockStream> mock_stream;
  audio_manager().SetMakeInputStreamCB(base::BindRepeating(
      [](media::AudioInputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open())
      .WillOnce(Return(MockStream::OpenOutcome::kSuccess));
  EXPECT_CALL(mock_stream, IsMuted()).WillOnce(Return(kNotMuted));
  EXPECT_CALL(*this, CreatedCallback(kValidStream, kNotMuted));
  mojo::Remote<media::mojom::AudioInputStream> remote_stream(
      CreateStreamWithNullptrLog());
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(client(), BindingConnectionError());
  EXPECT_CALL(observer(), BindingConnectionError());
  remote_stream.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioServiceInputStreamTest, ConstructDestructNullptrObserver) {
  NiceMock<MockStream> mock_stream;
  audio_manager().SetMakeInputStreamCB(base::BindRepeating(
      [](media::AudioInputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open())
      .WillOnce(Return(MockStream::OpenOutcome::kSuccess));
  EXPECT_CALL(mock_stream, IsMuted()).WillOnce(Return(kNotMuted));
  EXPECT_CALL(log(), OnCreated(_, _));
  EXPECT_CALL(*this, CreatedCallback(kValidStream, kNotMuted));
  mojo::Remote<media::mojom::AudioInputStream> remote_stream(
      CreateStreamWithNullptrObserver());
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(log(), OnClosed());
  EXPECT_CALL(client(), BindingConnectionError());
  remote_stream.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioServiceInputStreamTest,
       ConstructStreamAndCloseClientBinding_DestructsStream) {
  NiceMock<MockStream> mock_stream;
  audio_manager().SetMakeInputStreamCB(base::BindRepeating(
      [](media::AudioInputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open())
      .WillOnce(Return(MockStream::OpenOutcome::kSuccess));
  EXPECT_CALL(mock_stream, IsMuted()).WillOnce(Return(kNotMuted));
  EXPECT_CALL(log(), OnCreated(_, _));
  EXPECT_CALL(*this, CreatedCallback(kValidStream, kNotMuted));
  mojo::Remote<media::mojom::AudioInputStream> remote_stream(
      CreateStream(kDoNotEnableAGC));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(log(), OnClosed());
  EXPECT_CALL(observer(), BindingConnectionError());
  client().CloseBinding();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioServiceInputStreamTest,
       ConstructStreamAndCloseObserverBinding_DestructsStream) {
  NiceMock<MockStream> mock_stream;
  audio_manager().SetMakeInputStreamCB(base::BindRepeating(
      [](media::AudioInputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open())
      .WillOnce(Return(MockStream::OpenOutcome::kSuccess));
  EXPECT_CALL(mock_stream, IsMuted()).WillOnce(Return(kNotMuted));
  EXPECT_CALL(log(), OnCreated(_, _));
  EXPECT_CALL(*this, CreatedCallback(kValidStream, kNotMuted));
  mojo::Remote<media::mojom::AudioInputStream> remote_stream(
      CreateStream(kDoNotEnableAGC));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(log(), OnClosed());
  EXPECT_CALL(client(), BindingConnectionError());
  observer().CloseBinding();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioServiceInputStreamTest,
       ConstructStreamAndResetStreamPtr_DestructsStream) {
  NiceMock<MockStream> mock_stream;
  audio_manager().SetMakeInputStreamCB(base::BindRepeating(
      [](media::AudioInputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open())
      .WillOnce(Return(MockStream::OpenOutcome::kSuccess));
  EXPECT_CALL(mock_stream, IsMuted()).WillOnce(Return(kNotMuted));
  EXPECT_CALL(log(), OnCreated(_, _));
  EXPECT_CALL(*this, CreatedCallback(kValidStream, kNotMuted));

  mojo::Remote<media::mojom::AudioInputStream> remote_stream(
      CreateStream(kDoNotEnableAGC));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(log(), OnClosed());
  EXPECT_CALL(client(), BindingConnectionError());
  EXPECT_CALL(observer(), BindingConnectionError());
  remote_stream.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioServiceInputStreamTest, Record) {
  NiceMock<MockStream> mock_stream;
  audio_manager().SetMakeInputStreamCB(base::BindRepeating(
      [](media::AudioInputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open())
      .WillOnce(Return(MockStream::OpenOutcome::kSuccess));
  EXPECT_CALL(mock_stream, IsMuted()).WillOnce(Return(kNotMuted));
  EXPECT_CALL(log(), OnCreated(_, _));
  EXPECT_CALL(*this, CreatedCallback(kValidStream, kNotMuted));
  mojo::Remote<media::mojom::AudioInputStream> remote_stream(
      CreateStream(kDoNotEnableAGC));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_stream, Start(NotNull()));
  EXPECT_CALL(log(), OnStarted());
  EXPECT_CALL(observer(), DidStartRecording());
  remote_stream->Record();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_stream, Stop());
  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(log(), OnClosed());
  EXPECT_CALL(client(), BindingConnectionError());
  EXPECT_CALL(observer(), BindingConnectionError());
  remote_stream.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioServiceInputStreamTest, SetVolume) {
  NiceMock<MockStream> mock_stream;
  audio_manager().SetMakeInputStreamCB(base::BindRepeating(
      [](media::AudioInputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open())
      .WillOnce(Return(MockStream::OpenOutcome::kSuccess));
  EXPECT_CALL(mock_stream, IsMuted()).WillOnce(Return(kNotMuted));
  EXPECT_CALL(log(), OnCreated(_, _));
  EXPECT_CALL(*this, CreatedCallback(kValidStream, kNotMuted));
  mojo::Remote<media::mojom::AudioInputStream> remote_stream(
      CreateStream(kDoNotEnableAGC));
  base::RunLoop().RunUntilIdle();

  const double new_volume = 0.618;
  EXPECT_CALL(mock_stream, SetVolume(new_volume));
  EXPECT_CALL(log(), OnSetVolume(new_volume));
  remote_stream->SetVolume(new_volume);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(log(), OnClosed());
  EXPECT_CALL(client(), BindingConnectionError());
  EXPECT_CALL(observer(), BindingConnectionError());
  remote_stream.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioServiceInputStreamTest, SetNegativeVolume_BadMessage) {
  NiceMock<MockStream> mock_stream;
  audio_manager().SetMakeInputStreamCB(base::BindRepeating(
      [](media::AudioInputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open())
      .WillOnce(Return(MockStream::OpenOutcome::kSuccess));
  EXPECT_CALL(mock_stream, IsMuted()).WillOnce(Return(kNotMuted));
  EXPECT_CALL(log(), OnCreated(_, _));
  EXPECT_CALL(*this, CreatedCallback(kValidStream, kNotMuted));
  mojo::Remote<media::mojom::AudioInputStream> remote_stream(
      CreateStream(kDoNotEnableAGC));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*this, BadMessageCallback(_));
  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(log(), OnClosed());
  EXPECT_CALL(client(), BindingConnectionError());
  EXPECT_CALL(observer(), BindingConnectionError());
  remote_stream->SetVolume(-0.618);
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioServiceInputStreamTest, SetVolumeGreaterThanOne_BadMessage) {
  NiceMock<MockStream> mock_stream;
  audio_manager().SetMakeInputStreamCB(base::BindRepeating(
      [](media::AudioInputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open())
      .WillOnce(Return(MockStream::OpenOutcome::kSuccess));
  EXPECT_CALL(mock_stream, IsMuted()).WillOnce(Return(kNotMuted));
  EXPECT_CALL(log(), OnCreated(_, _));
  EXPECT_CALL(*this, CreatedCallback(kValidStream, kNotMuted));
  mojo::Remote<media::mojom::AudioInputStream> remote_stream(
      CreateStream(kDoNotEnableAGC));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*this, BadMessageCallback(_));
  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(log(), OnClosed());
  EXPECT_CALL(client(), BindingConnectionError());
  EXPECT_CALL(observer(), BindingConnectionError());
  remote_stream->SetVolume(1.618);
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioServiceInputStreamTest, CreateStreamWithAGCEnable_PropagateAGC) {
  NiceMock<MockStream> mock_stream;
  audio_manager().SetMakeInputStreamCB(base::BindRepeating(
      [](media::AudioInputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open())
      .WillOnce(Return(MockStream::OpenOutcome::kSuccess));
  EXPECT_CALL(mock_stream, IsMuted()).WillOnce(Return(kNotMuted));
  EXPECT_CALL(mock_stream, SetAutomaticGainControl(kDoEnableAGC));
  EXPECT_CALL(log(), OnCreated(_, _));
  EXPECT_CALL(*this, CreatedCallback(kValidStream, kNotMuted));
  mojo::Remote<media::mojom::AudioInputStream> remote_stream(
      CreateStream(kDoEnableAGC));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(log(), OnClosed());
  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(client(), BindingConnectionError());
  EXPECT_CALL(observer(), BindingConnectionError());
  remote_stream.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioServiceInputStreamTest,
       CreateInitiallyMutedStream_PropagateInitiallyMuted) {
  NiceMock<MockStream> mock_stream;
  audio_manager().SetMakeInputStreamCB(base::BindRepeating(
      [](media::AudioInputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open())
      .WillOnce(Return(MockStream::OpenOutcome::kSuccess));
  EXPECT_CALL(mock_stream, IsMuted()).WillOnce(Return(kMuted));
  EXPECT_CALL(log(), OnCreated(_, _));
  EXPECT_CALL(*this, CreatedCallback(kValidStream, kMuted));
  mojo::Remote<media::mojom::AudioInputStream> remote_stream(
      CreateStream(kDoEnableAGC));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(log(), OnClosed());
  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(client(), BindingConnectionError());
  EXPECT_CALL(observer(), BindingConnectionError());
  remote_stream.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioServiceInputStreamTest,
       ConstructWithStreamCreationFailure_SignalsError) {
  // By default, MockAudioManager fails to create a stream.

  mojo::Remote<media::mojom::AudioInputStream> remote_stream(
      CreateStream(kDoNotEnableAGC));

  EXPECT_CALL(*this, CreatedCallback(kInvalidStream, kNotMuted));
  EXPECT_CALL(log(), OnError());
  EXPECT_CALL(client(), OnError(media::mojom::InputStreamErrorCode::kUnknown));
  EXPECT_CALL(client(), BindingConnectionError());
  EXPECT_CALL(observer(), BindingConnectionError());
  base::RunLoop().RunUntilIdle();
}

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
TEST_F(AudioServiceInputStreamTest, ResidualEchoEstimationModelRequested) {
  media::AudioProcessingSettings settings;
  settings.echo_cancellation = true;  // Enable some processing
  auto processing_config = media::mojom::AudioProcessingConfig::New();
  processing_config->settings = settings;
  mojo::Remote<media::mojom::AudioProcessorControls> controls_remote;
  processing_config->controls_receiver =
      controls_remote.BindNewPipeAndPassReceiver();

  // We don't strictly need to check the CreatedCallback for this test's purpose
  // but it avoids the print of a warning.
  EXPECT_CALL(*this, CreatedCallback(kValidStream, kNotMuted));

  // Setup MockAudioManager expectations
  NiceMock<MockStream> mock_stream;
  media::AudioParameters expected_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), 48000, 480);

  audio_manager().SetInputStreamParameters(expected_params);

  audio_manager().SetMakeInputStreamCB(base::BindRepeating(
      [](media::AudioInputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open())
      .WillOnce(Return(MockStream::OpenOutcome::kSuccess));
  EXPECT_CALL(mock_stream, IsMuted()).WillOnce(Return(kNotMuted));
  EXPECT_CALL(log(), OnCreated(_, _));

  // Call CreateInputStream directly
  mojo::PendingRemote<media::mojom::AudioInputStream> remote_stream;
  remote_stream_factory_->CreateInputStream(
      remote_stream.InitWithNewPipeAndPassReceiver(), client_.MakeRemote(),
      observer_.MakeRemote(), log_.MakeRemote(), kDefaultDeviceId,
      expected_params, base::UnguessableToken::Create(),
      kDefaultSharedMemoryCount, /*enable_agc=*/false,
      std::move(processing_config),
      base::BindOnce(&AudioServiceInputStreamTest::OnCreated,
                     base::Unretained(this)));

  EXPECT_CALL(client(), BindingConnectionError());
  EXPECT_CALL(observer(), BindingConnectionError());
  remote_stream.reset();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return mock_ml_model_manager_.model_requested(); }));
}
#endif

}  // namespace audio
