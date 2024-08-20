// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/mojo/clients/mojo_audio_encoder.h"

#include <memory>

#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "media/base/audio_encoder.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/base/waiting.h"
#include "media/mojo/mojom/audio_encoder.mojom.h"
#include "media/mojo/services/mojo_audio_encoder_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::StrictMock;

namespace media {

// Tests MojoAudioEncoder (client) and MojoAudioEncoderService (service).
class MojoAudioEncoderTest : public ::testing::Test {
 public:
  MojoAudioEncoderTest() : service_thread_("Service Thread") {}

  void SetUp() override {
    service_thread_.Start();

    callback_runner_ = task_environment_.GetMainThreadTaskRunner();
    service_task_runner_ = service_thread_.task_runner();

    std::unique_ptr<StrictMock<MockAudioEncoder>> mock_audio_encoder(
        new StrictMock<MockAudioEncoder>());
    mock_audio_encoder_ = mock_audio_encoder.get();
    EXPECT_CALL(*mock_audio_encoder_, OnDestruct());

    // Setup the mojo connection.
    mojo::PendingRemote<mojom::AudioEncoder> remote_audio_encoder;
    audio_encoder_service_ = std::make_unique<MojoAudioEncoderService>(
        std::move(mock_audio_encoder));

    service_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MojoAudioEncoderTest::SetupMojoService,
                       base::Unretained(this),
                       remote_audio_encoder.InitWithNewPipeAndPassReceiver()));
    mojo_audio_encoder_ =
        std::make_unique<MojoAudioEncoder>(std::move(remote_audio_encoder));
  }

  void TearDown() override {
    mojo_audio_encoder_.reset();
    service_task_runner_->DeleteSoon(FROM_HERE, std::move(receiver_));
    service_task_runner_->DeleteSoon(FROM_HERE,
                                     std::move(audio_encoder_service_));
    service_thread_.Stop();
  }

  void SetupMojoService(mojo::PendingReceiver<mojom::AudioEncoder> receiver) {
    receiver_ = std::make_unique<mojo::Receiver<mojom::AudioEncoder>>(
        audio_encoder_service_.get(), std::move(receiver));
  }

  std::unique_ptr<AudioBus> MakeInput(float seed, int channels, int frames) {
    CHECK_EQ(std::clamp(seed, -1.0f, 1.0f), seed)
        << "Some AudioBuffer <-> AudioBus conversions involve clipping to the "
           "range [-1.0, 1.0], so test results will be unreliable";
    auto result = AudioBus::Create(channels, frames);
    for (int channel = 0; channel < channels; channel++) {
      for (int i = 0; i < frames; i++)
        result->channel(channel)[i] = seed;
    }
    return result;
  }

  AudioEncoder::Options MakeOptions() {
    AudioEncoder::Options options;
    options.codec = AudioCodec::kOpus;
    options.bitrate = 128000;
    options.channels = 2;
    options.sample_rate = 44000;
    return options;
  }

  base::TimeTicks FromMilliseconds(int ms) {
    return base::TimeTicks() + base::Milliseconds(ms);
  }

  int64_t ToMilliseconds(base::TimeTicks ticks) {
    return (ticks - base::TimeTicks()).InMilliseconds();
  }

  AudioEncoder::EncoderStatusCB ValidatingStatusCB(
      base::Location loc = FROM_HERE) {
    struct CallEnforcer {
      bool called = false;
      std::string location;
      ~CallEnforcer() {
        EXPECT_TRUE(called) << "Callback created: " << location;
      }
    };
    auto enforcer = std::make_unique<CallEnforcer>();
    enforcer->location = loc.ToString();
    return base::BindLambdaForTesting(
        [this, enforcer{std::move(enforcer)}](EncoderStatus s) {
          EXPECT_TRUE(callback_runner_->RunsTasksInCurrentSequence());
          EXPECT_TRUE(s.is_ok()) << " Callback created: " << enforcer->location
                                 << " Error: " << s.message();
          enforcer->called = true;
        });
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<base::RunLoop> run_loop_;

  // The thread where the service runs. This provides test coverage in an
  // environment similar to what we use in production.
  base::Thread service_thread_;
  scoped_refptr<base::SequencedTaskRunner> service_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> callback_runner_;

  // The MojoAudioEncoder that we are testing.
  std::unique_ptr<MojoAudioEncoder> mojo_audio_encoder_;

  // Mojo server-side
  std::unique_ptr<mojo::Receiver<mojom::AudioEncoder>> receiver_;
  std::unique_ptr<MojoAudioEncoderService> audio_encoder_service_;
  raw_ptr<StrictMock<MockAudioEncoder>, AcrossTasksDanglingUntriaged>
      mock_audio_encoder_ = nullptr;
};

TEST_F(MojoAudioEncoderTest, Initialize_Success) {
  base::RunLoop run_loop;
  AudioEncoder::Options options = MakeOptions();
  EXPECT_CALL(*mock_audio_encoder_, Initialize(_, _, _))
      .WillOnce(Invoke([this](const AudioEncoder::Options& options,
                              AudioEncoder::OutputCB output_cb,
                              AudioEncoder::EncoderStatusCB done_cb) {
        EXPECT_TRUE(service_task_runner_->RunsTasksInCurrentSequence());
        std::move(done_cb).Run(EncoderStatus::Codes::kOk);
      }));

  AudioEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](EncodedAudioBuffer output,
          std::optional<AudioEncoder::CodecDescription>) { FAIL(); });

  auto done_cb = base::BindLambdaForTesting([&, this](EncoderStatus s) {
    EXPECT_TRUE(callback_runner_->RunsTasksInCurrentSequence());
    EXPECT_TRUE(s.is_ok());
    run_loop.QuitWhenIdle();
  });

  mojo_audio_encoder_->Initialize(options, std::move(output_cb),
                                  std::move(done_cb));
  run_loop.Run();
}

TEST_F(MojoAudioEncoderTest, Initialize_Fail) {
  base::RunLoop run_loop;
  AudioEncoder::Options options = MakeOptions();
  EXPECT_CALL(*mock_audio_encoder_, Initialize(_, _, _))
      .WillOnce(Invoke([](const AudioEncoder::Options& options,
                          AudioEncoder::OutputCB output_cb,
                          AudioEncoder::EncoderStatusCB done_cb) {
        std::move(done_cb).Run(
            EncoderStatus::Codes::kEncoderInitializationError);
      }));

  AudioEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](EncodedAudioBuffer output,
          std::optional<AudioEncoder::CodecDescription>) { FAIL(); });

  auto done_cb = base::BindLambdaForTesting([&, this](EncoderStatus s) {
    EXPECT_TRUE(callback_runner_->RunsTasksInCurrentSequence());
    EXPECT_EQ(s.code(), EncoderStatus::Codes::kEncoderInitializationError);
    run_loop.QuitWhenIdle();
  });

  mojo_audio_encoder_->Initialize(options, std::move(output_cb),
                                  std::move(done_cb));
  run_loop.Run();
}

TEST_F(MojoAudioEncoderTest, Initialize_Twice) {
  base::RunLoop good_init_run_loop;
  base::RunLoop failed_initi_run_loop;
  AudioEncoder::Options options = MakeOptions();
  EXPECT_CALL(*mock_audio_encoder_, Initialize(_, _, _))
      .WillRepeatedly(Invoke([](const AudioEncoder::Options& options,
                                AudioEncoder::OutputCB output_cb,
                                AudioEncoder::EncoderStatusCB done_cb) {
        std::move(done_cb).Run(EncoderStatus::Codes::kOk);
      }));

  auto expect_ok = base::BindLambdaForTesting([&](EncoderStatus s) {
    EXPECT_TRUE(s.is_ok());
    good_init_run_loop.QuitWhenIdle();
  });

  auto expect_error = base::BindLambdaForTesting([&](EncoderStatus s) {
    EXPECT_EQ(s.code(), EncoderStatus::Codes::kEncoderInitializeTwice);
    failed_initi_run_loop.QuitWhenIdle();
  });

  mojo_audio_encoder_->Initialize(options, base::DoNothing(),
                                  std::move(expect_ok));
  good_init_run_loop.Run();

  mojo_audio_encoder_->Initialize(options, base::DoNothing(),
                                  std::move(expect_error));
  failed_initi_run_loop.Run();
}

TEST_F(MojoAudioEncoderTest, Encode) {
  base::RunLoop run_loop;
  AudioEncoder::Options options = MakeOptions();
  constexpr int kInputCount = 20;
  constexpr size_t kFrameCount = 1024;
  int output_count = 0;
  AudioEncoder::OutputCB service_output_cb;
  EXPECT_CALL(*mock_audio_encoder_, Initialize(_, _, _))
      .WillOnce(Invoke([&, this](const AudioEncoder::Options& options,
                                 AudioEncoder::OutputCB output_cb,
                                 AudioEncoder::EncoderStatusCB done_cb) {
        EXPECT_TRUE(service_task_runner_->RunsTasksInCurrentSequence());
        service_output_cb = std::move(output_cb);
        std::move(done_cb).Run(EncoderStatus::Codes::kOk);
      }));

  EXPECT_CALL(*mock_audio_encoder_, Encode(_, _, _))
      .WillRepeatedly(Invoke([&, this](std::unique_ptr<AudioBus> audio_bus,
                                       base::TimeTicks capture_time,
                                       AudioEncoder::EncoderStatusCB done_cb) {
        EXPECT_TRUE(service_task_runner_->RunsTasksInCurrentSequence());
        std::move(done_cb).Run(EncoderStatus::Codes::kOk);

        int64_t input_number = ToMilliseconds(capture_time);
        EXPECT_LE(input_number, kInputCount);

        AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                               {CHANNEL_LAYOUT_DISCRETE, audio_bus->channels()},
                               options.sample_rate, audio_bus->frames());

        const auto channel_data = base::make_span(
            reinterpret_cast<const uint8_t*>(audio_bus->channel(0)),
            base::checked_cast<size_t>(AudioBus::CalculateMemorySize(
                /*channels=*/1, audio_bus->frames())));
        auto encoded_data = base::HeapArray<uint8_t>::CopiedFrom(channel_data);

        EncodedAudioBuffer output(params, std::move(encoded_data),
                                  capture_time);

        std::optional<AudioEncoder::CodecDescription> desc;
        if (input_number > 0)
          desc.emplace(AudioEncoder::CodecDescription{
              static_cast<uint8_t>(input_number)});
        service_output_cb.Run(std::move(output), desc);
      }));

  AudioEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](EncodedAudioBuffer output,
          std::optional<AudioEncoder::CodecDescription> desc) {
        const int64_t output_number = ToMilliseconds(output.timestamp);
        EXPECT_EQ(output_number, output_count);
        EXPECT_EQ(output.params.channels(), options.channels);
        EXPECT_EQ(output.params.sample_rate(), options.sample_rate);
        ASSERT_EQ(output.encoded_data.size(),
                  base::checked_cast<size_t>(AudioBus::CalculateMemorySize(
                      /*channels=*/1, kFrameCount)));
        const float seed = 1.0 / kInputCount * output_number;
        auto* const encoded_data =
            reinterpret_cast<const float*>(output.encoded_data.data());
        for (size_t i = 0; i < kFrameCount; i++) {
          EXPECT_EQ(encoded_data[i], seed)
              << " output_number: " << output_number << " i: " << i;
        }

        if (output_number == 0)
          EXPECT_FALSE(desc.has_value());
        else
          EXPECT_EQ(output_number, desc.value()[0]);

        output_count++;
        if (output_count == kInputCount) {
          run_loop.QuitWhenIdle();
        }
      });

  mojo_audio_encoder_->Initialize(options, std::move(output_cb),
                                  ValidatingStatusCB());

  for (int i = 0; i < kInputCount; i++) {
    auto ts = FromMilliseconds(i);
    mojo_audio_encoder_->Encode(
        MakeInput(1.0 / kInputCount * i, options.channels, kFrameCount), ts,
        ValidatingStatusCB());
  }

  run_loop.Run();
}  // namespace media

TEST_F(MojoAudioEncoderTest, EncodeWithEmptyResult) {
  base::RunLoop run_loop;
  AudioEncoder::Options options = MakeOptions();
  AudioEncoder::OutputCB service_output_cb;
  EXPECT_CALL(*mock_audio_encoder_, Initialize(_, _, _))
      .WillOnce(Invoke([&](const AudioEncoder::Options& options,
                           AudioEncoder::OutputCB output_cb,
                           AudioEncoder::EncoderStatusCB done_cb) {
        service_output_cb = std::move(output_cb);
        std::move(done_cb).Run(EncoderStatus::Codes::kOk);
      }));

  EXPECT_CALL(*mock_audio_encoder_, Encode(_, _, _))
      .WillRepeatedly(Invoke([&, this](std::unique_ptr<AudioBus> audio_bus,
                                       base::TimeTicks capture_time,
                                       AudioEncoder::EncoderStatusCB done_cb) {
        EXPECT_TRUE(service_task_runner_->RunsTasksInCurrentSequence());
        std::move(done_cb).Run(EncoderStatus::Codes::kOk);

        AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                               {CHANNEL_LAYOUT_DISCRETE, 1}, 8000, 1);

        EncodedAudioBuffer output(params, base::HeapArray<uint8_t>(),
                                  capture_time);

        service_output_cb.Run(std::move(output), {});
      }));

  AudioEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](EncodedAudioBuffer output,
          std::optional<AudioEncoder::CodecDescription> desc) {
        EXPECT_TRUE(output.encoded_data.empty());
        run_loop.QuitWhenIdle();
      });

  mojo_audio_encoder_->Initialize(options, std::move(output_cb),
                                  ValidatingStatusCB());

  auto ts = FromMilliseconds(1);
  mojo_audio_encoder_->Encode(
      MakeInput(1, options.channels, options.sample_rate), ts,
      ValidatingStatusCB());

  run_loop.Run();
}  // namespace media

TEST_F(MojoAudioEncoderTest, Flush) {
  base::RunLoop run_loop;
  AudioEncoder::Options options = MakeOptions();
  const int input_count = 5;
  int output_count = 0;
  AudioEncoder::OutputCB service_output_cb;
  EXPECT_CALL(*mock_audio_encoder_, Initialize(_, _, _))
      .WillOnce(Invoke([&](const AudioEncoder::Options& options,
                           AudioEncoder::OutputCB output_cb,
                           AudioEncoder::EncoderStatusCB done_cb) {
        service_output_cb = std::move(output_cb);
        std::move(done_cb).Run(EncoderStatus::Codes::kOk);
      }));

  EXPECT_CALL(*mock_audio_encoder_, Encode(_, _, _))
      .WillRepeatedly(Invoke([&](std::unique_ptr<AudioBus> audio_bus,
                                 base::TimeTicks capture_time,
                                 AudioEncoder::EncoderStatusCB done_cb) {
        std::move(done_cb).Run(EncoderStatus::Codes::kOk);

        AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                               {CHANNEL_LAYOUT_DISCRETE, audio_bus->channels()},
                               options.sample_rate, audio_bus->frames());
        EncodedAudioBuffer output(params, base::HeapArray<uint8_t>(),
                                  capture_time);
        service_output_cb.Run(std::move(output), {});
      }));

  EXPECT_CALL(*mock_audio_encoder_, Flush(_))
      .WillRepeatedly(Invoke([&, this](AudioEncoder::EncoderStatusCB done_cb) {
        EXPECT_TRUE(service_task_runner_->RunsTasksInCurrentSequence());
        std::move(done_cb).Run(EncoderStatus::Codes::kOk);
      }));

  AudioEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](EncodedAudioBuffer output,
          std::optional<AudioEncoder::CodecDescription>) { output_count++; });

  mojo_audio_encoder_->Initialize(options, std::move(output_cb),
                                  ValidatingStatusCB());

  for (int i = 0; i < input_count; i++) {
    auto ts = FromMilliseconds(i);
    mojo_audio_encoder_->Encode(
        MakeInput(1.0 / input_count * i, options.channels, options.sample_rate),
        ts, ValidatingStatusCB());
  }

  auto flush_cb = base::BindLambdaForTesting([&](EncoderStatus s) {
    EXPECT_TRUE(callback_runner_->RunsTasksInCurrentSequence());
    EXPECT_TRUE(s.is_ok());
    EXPECT_EQ(output_count, input_count);
    run_loop.QuitWhenIdle();
  });

  mojo_audio_encoder_->Flush(std::move(flush_cb));
  run_loop.Run();
}

// Test that in case of an mojo error all status callbacks report correct
// error status.
TEST_F(MojoAudioEncoderTest, MojoErrorCallsAllDoneCallbacks) {
  base::RunLoop run_loop;
  AudioEncoder::Options options = MakeOptions();
  std::vector<AudioEncoder::EncoderStatusCB> done_callbacks;
  const int input_count = 5;
  int error_count = 0;
  EXPECT_CALL(*mock_audio_encoder_, Initialize(_, _, _))
      .WillOnce(Invoke([&](const AudioEncoder::Options& options,
                           AudioEncoder::OutputCB output_cb,
                           AudioEncoder::EncoderStatusCB done_cb) {
        std::move(done_cb).Run(EncoderStatus::Codes::kOk);
      }));
  EXPECT_CALL(*mock_audio_encoder_, Encode(_, _, _))
      .WillRepeatedly(Invoke([&](std::unique_ptr<AudioBus> audio_bus,
                                 base::TimeTicks capture_time,
                                 AudioEncoder::EncoderStatusCB done_cb) {
        done_callbacks.push_back(std::move(done_cb));
      }));

  EXPECT_CALL(*mock_audio_encoder_, Flush(_))
      .WillOnce(Invoke([&](AudioEncoder::EncoderStatusCB done_cb) {
        done_callbacks.push_back(std::move(done_cb));
        service_task_runner_->DeleteSoon(FROM_HERE, std::move(receiver_));
      }));

  mojo_audio_encoder_->Initialize(options, base::DoNothing(),
                                  ValidatingStatusCB());

  for (int i = 0; i < input_count; i++) {
    auto ts = FromMilliseconds(i);
    auto done_cb = base::BindLambdaForTesting([&](EncoderStatus s) {
      EXPECT_EQ(s.code(), EncoderStatus::Codes::kEncoderMojoConnectionError);
      error_count++;
    });
    mojo_audio_encoder_->Encode(
        MakeInput(1.0 / input_count * i, options.channels, options.sample_rate),
        ts, std::move(done_cb));
  }
  auto flush_cb = base::BindLambdaForTesting([&](EncoderStatus s) {
    EXPECT_EQ(s.code(), EncoderStatus::Codes::kEncoderMojoConnectionError);
    run_loop.QuitWhenIdle();
  });

  mojo_audio_encoder_->Flush(std::move(flush_cb));

  run_loop.Run();
  EXPECT_EQ(error_count, input_count);
}

}  // namespace media
