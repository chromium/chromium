// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/audio_system_to_service_adapter.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/audio/audio_system_test_util.h"
#include "media/audio/test_audio_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/audio/in_process_audio_manager_accessor.h"
#include "services/audio/system_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Exactly;
using testing::MockFunction;
using testing::NiceMock;

namespace audio {

// Base fixture for all the tests below.
class AudioSystemToServiceAdapterTestBase : public testing::Test {
 public:
  AudioSystemToServiceAdapterTestBase() {}

  AudioSystemToServiceAdapterTestBase(
      const AudioSystemToServiceAdapterTestBase&) = delete;
  AudioSystemToServiceAdapterTestBase& operator=(
      const AudioSystemToServiceAdapterTestBase&) = delete;

  ~AudioSystemToServiceAdapterTestBase() override {}

  void SetUp() override {
    audio_manager_ = std::make_unique<media::MockAudioManager>(
        std::make_unique<media::TestAudioThread>(
            false /* we do not use separate thread here */));
    system_info_impl_ =
        std::make_unique<audio::SystemInfo>(audio_manager_.get());
    system_info_receiver_ = std::make_unique<mojo::Receiver<mojom::SystemInfo>>(
        system_info_impl_.get());
    audio_system_ =
        std::make_unique<AudioSystemToServiceAdapter>(base::BindRepeating(
            &AudioSystemToServiceAdapterTestBase::BindSystemInfoReceiver,
            base::Unretained(this)));
  }

  void TearDown() override {
    audio_system_.reset();
    system_info_receiver_.reset();
    audio_manager_->Shutdown();
  }

 protected:
  // Required by AudioSystemTestTemplate:
  media::MockAudioManager* audio_manager() { return audio_manager_.get(); }
  media::AudioSystem* audio_system() { return audio_system_.get(); }

  // Called when SystemInfo bind request is received. Nice mock because
  // AudioSystem conformance tests won't set expecnations.
  NiceMock<MockFunction<void(void)>> system_info_bind_requested_;

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<media::MockAudioManager> audio_manager_;
  std::unique_ptr<mojom::SystemInfo> system_info_impl_;
  std::unique_ptr<mojo::Receiver<mojom::SystemInfo>> system_info_receiver_;
  std::unique_ptr<media::AudioSystem> audio_system_;

 private:
  void BindSystemInfoReceiver(
      mojo::PendingReceiver<mojom::SystemInfo> receiver) {
    EXPECT_TRUE(system_info_receiver_) << "AudioSystemToServiceAdapter should "
                                          "not request AudioSysteInfo during "
                                          "construction";
    ASSERT_FALSE(system_info_receiver_->is_bound());
    system_info_bind_requested_.Call();
    system_info_receiver_->Bind(std::move(receiver));
  }
};

// Base fixture for connection loss tests.
class AudioSystemToServiceAdapterConnectionLossTest
    : public AudioSystemToServiceAdapterTestBase {
 public:
  AudioSystemToServiceAdapterConnectionLossTest() {}

  AudioSystemToServiceAdapterConnectionLossTest(
      const AudioSystemToServiceAdapterConnectionLossTest&) = delete;
  AudioSystemToServiceAdapterConnectionLossTest& operator=(
      const AudioSystemToServiceAdapterConnectionLossTest&) = delete;

  ~AudioSystemToServiceAdapterConnectionLossTest() override {}

  void SetUp() override {
    AudioSystemToServiceAdapterTestBase::SetUp();
    params_ = media::AudioParameters::UnavailableDeviceParams();
    audio_manager()->SetInputStreamParameters(params_);
    audio_manager()->SetOutputStreamParameters(params_);
    audio_manager()->SetDefaultOutputStreamParameters(params_);

    auto get_device_descriptions =
        [](const media::AudioDeviceDescriptions* source,
           media::AudioDeviceDescriptions* destination) {
          destination->insert(destination->end(), source->begin(),
                              source->end());
        };
    device_descriptions_.emplace_back("device_name", "device_id", "group_id");
    audio_manager()->SetInputDeviceDescriptionsCallback(base::BindRepeating(
        get_device_descriptions, base::Unretained(&device_descriptions_)));
    audio_manager()->SetOutputDeviceDescriptionsCallback(base::BindRepeating(
        get_device_descriptions, base::Unretained(&device_descriptions_)));

    associated_id_ = "associated_id";
    audio_manager()->SetAssociatedOutputDeviceIDCallback(base::BindRepeating(
        [](const std::string* result, const std::string&) -> std::string {
          return *result;
        },
        base::Unretained(&associated_id_)));
  }

 protected:
  void GetDeviceDescriptionsTest(bool for_input) {
    EXPECT_FALSE(system_info_receiver_->is_bound());
    {  // Succeeds.
      base::RunLoop wait_loop;
      // Should bind:
      EXPECT_CALL(system_info_bind_requested_, Call()).Times(Exactly(1));
      audio_system_->GetDeviceDescriptions(
          for_input,
          expectations_.GetDeviceDescriptionsCallback(
              FROM_HERE, wait_loop.QuitClosure(), device_descriptions_));
      wait_loop.Run();
    }
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(system_info_receiver_->is_bound());
    {  // Fails correctly on connection loss.
      base::RunLoop wait_loop;
      // Should be already bound:
      EXPECT_CALL(system_info_bind_requested_, Call()).Times(Exactly(0));
      audio_system_->GetDeviceDescriptions(
          for_input, expectations_.GetDeviceDescriptionsCallback(
                         FROM_HERE, wait_loop.QuitClosure(),
                         media::AudioDeviceDescriptions()));
      system_info_receiver_->reset();  // Connection loss.
      base::RunLoop().RunUntilIdle();
      wait_loop.Run();
    }
    EXPECT_FALSE(system_info_receiver_->is_bound());
  }

  void HasDevicesTest(bool for_input) {
    auto has_devices = for_input ? &media::AudioSystem::HasInputDevices
                                 : &media::AudioSystem::HasOutputDevices;
    EXPECT_FALSE(system_info_receiver_->is_bound());
    {  // Succeeds.
      base::RunLoop wait_loop;
      // Should bind:
      EXPECT_CALL(system_info_bind_requested_, Call()).Times(Exactly(1));
      (audio_system()->*has_devices)(expectations_.GetBoolCallback(
          FROM_HERE, wait_loop.QuitClosure(), true));
      wait_loop.Run();
    }
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(system_info_receiver_->is_bound());
    {  // Fails correctly on connection loss.
      base::RunLoop wait_loop;
      // Should be already bound:
      EXPECT_CALL(system_info_bind_requested_, Call()).Times(Exactly(0));
      (audio_system()->*has_devices)(expectations_.GetBoolCallback(
          FROM_HERE, wait_loop.QuitClosure(), false));
      system_info_receiver_->reset();  // Connection loss.
      base::RunLoop().RunUntilIdle();
      wait_loop.Run();
    }
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(system_info_receiver_->is_bound());
  }

  void GetStreamParametersTest(bool for_input) {
    auto get_stream_parameters =
        for_input ? &media::AudioSystem::GetInputStreamParameters
                  : &media::AudioSystem::GetOutputStreamParameters;
    EXPECT_FALSE(system_info_receiver_->is_bound());
    {  // Succeeds.
      base::RunLoop wait_loop;
      // Should bind:
      EXPECT_CALL(system_info_bind_requested_, Call()).Times(Exactly(1));
      (audio_system()->*get_stream_parameters)(
          media::AudioDeviceDescription::kDefaultDeviceId,
          expectations_.GetAudioParamsCallback(
              FROM_HERE, wait_loop.QuitClosure(), params_));
      wait_loop.Run();
    }
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(system_info_receiver_->is_bound());
    {  // Fails correctly on connection loss.
      base::RunLoop wait_loop;
      // Should be already bound:
      EXPECT_CALL(system_info_bind_requested_, Call()).Times(Exactly(0));
      (audio_system()->*get_stream_parameters)(
          media::AudioDeviceDescription::kDefaultDeviceId,
          expectations_.GetAudioParamsCallback(
              FROM_HERE, wait_loop.QuitClosure(), std::nullopt));
      system_info_receiver_->reset();  // Connection loss.
      base::RunLoop().RunUntilIdle();
      wait_loop.Run();
    }
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(system_info_receiver_->is_bound());
  }

  std::string associated_id_;
  media::AudioParameters params_;
  media::AudioDeviceDescriptions device_descriptions_;
  media::AudioSystemCallbackExpectations expectations_;
};

// This test covers various scenarios of connection loss/restore, and the
// next tests only verify that we receive a correct callback if the connection
// is lost.
TEST_F(AudioSystemToServiceAdapterConnectionLossTest,
       GetAssociatedOutputDeviceIDFullConnectionTest) {
  EXPECT_FALSE(system_info_receiver_->is_bound());
  {  // Succeeds.
    base::RunLoop wait_loop;
    // Should bind:
    EXPECT_CALL(system_info_bind_requested_, Call()).Times(Exactly(1));
    audio_system_->GetAssociatedOutputDeviceID(
        std::string(), expectations_.GetDeviceIdCallback(
                           FROM_HERE, wait_loop.QuitClosure(), associated_id_));
    wait_loop.Run();
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(system_info_receiver_->is_bound());
  {  // Succeeds second time.
    base::RunLoop wait_loop;
    // Should be already bound:
    EXPECT_CALL(system_info_bind_requested_, Call()).Times(Exactly(0));
    audio_system_->GetAssociatedOutputDeviceID(
        std::string(), expectations_.GetDeviceIdCallback(
                           FROM_HERE, wait_loop.QuitClosure(), associated_id_));
    wait_loop.Run();
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(system_info_receiver_->is_bound());
  {  // Fails correctly on connection loss.
    base::RunLoop wait_loop;
    // Should be already bound:
    EXPECT_CALL(system_info_bind_requested_, Call()).Times(Exactly(0));
    audio_system_->GetAssociatedOutputDeviceID(
        std::string(), expectations_.GetDeviceIdCallback(
                           FROM_HERE, wait_loop.QuitClosure(), std::nullopt));
    system_info_receiver_->reset();  // Connection loss.
    wait_loop.Run();
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(system_info_receiver_->is_bound());
  {  // Fails correctly on connection loss if already unbound.
    base::RunLoop wait_loop;
    // Should re-bind after connection loss:
    EXPECT_CALL(system_info_bind_requested_, Call()).Times(Exactly(1));
    audio_system_->GetAssociatedOutputDeviceID(
        std::string(), expectations_.GetDeviceIdCallback(
                           FROM_HERE, wait_loop.QuitClosure(), std::nullopt));
    system_info_receiver_->reset();  // Connection loss.
    wait_loop.Run();
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(system_info_receiver_->is_bound());
  {  // Finally succeeds again!
    base::RunLoop wait_loop;
    // Should bind:
    EXPECT_CALL(system_info_bind_requested_, Call()).Times(Exactly(1));
    audio_system_->GetAssociatedOutputDeviceID(
        std::string(), expectations_.GetDeviceIdCallback(
                           FROM_HERE, wait_loop.QuitClosure(), associated_id_));
    wait_loop.Run();
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(system_info_receiver_->is_bound());
}

TEST_F(AudioSystemToServiceAdapterConnectionLossTest,
       GetInputStreamParameters) {
  GetStreamParametersTest(true);
}

TEST_F(AudioSystemToServiceAdapterConnectionLossTest,
       GetOutputStreamParameters) {
  GetStreamParametersTest(false);
}

TEST_F(AudioSystemToServiceAdapterConnectionLossTest, HasInputDevices) {
  HasDevicesTest(true);
}

TEST_F(AudioSystemToServiceAdapterConnectionLossTest, HasOutputDevices) {
  HasDevicesTest(false);
}

TEST_F(AudioSystemToServiceAdapterConnectionLossTest,
       GetInputDeviceDescriptions) {
  GetDeviceDescriptionsTest(true);
}

TEST_F(AudioSystemToServiceAdapterConnectionLossTest,
       GetOutputDeviceDescriptions) {
  GetDeviceDescriptionsTest(false);
}

TEST_F(AudioSystemToServiceAdapterConnectionLossTest, GetInputDeviceInfo) {
  EXPECT_FALSE(system_info_receiver_->is_bound());
  {  // Succeeds.
    base::RunLoop wait_loop;
    // Should bind:
    EXPECT_CALL(system_info_bind_requested_, Call()).Times(Exactly(1));
    audio_system_->GetInputDeviceInfo(
        "device-id",
        expectations_.GetInputDeviceInfoCallback(
            FROM_HERE, wait_loop.QuitClosure(), params_, associated_id_));
    wait_loop.Run();
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(system_info_receiver_->is_bound());
  {  // Fails correctly on connection loss.
    base::RunLoop wait_loop;
    // Should be already bound:
    EXPECT_CALL(system_info_bind_requested_, Call()).Times(Exactly(0));
    audio_system_->GetInputDeviceInfo(
        "device-id",
        expectations_.GetInputDeviceInfoCallback(
            FROM_HERE, wait_loop.QuitClosure(), std::nullopt, std::nullopt));
    system_info_receiver_->reset();  // Connection loss.
    wait_loop.Run();
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(system_info_receiver_->is_bound());
}

namespace {

static constexpr auto kResponseDelay = base::Milliseconds(10);
static constexpr char kSomeDeviceId[] = "Some device";
static constexpr char kValidReplyId[] =
    "If you can read it you received the reply";

}  // namespace

class AudioSystemToServiceAdapterDisconnectTest : public testing::Test {
 public:
  AudioSystemToServiceAdapterDisconnectTest() {}
  ~AudioSystemToServiceAdapterDisconnectTest() override {}

 protected:
  class MockSystemInfo : public mojom::SystemInfo {
   public:
    MockSystemInfo(base::TimeDelta response_delay) {}
    ~MockSystemInfo() override {}

   private:
    // audio::mojom::SystemInfo implementation.
    void GetInputStreamParameters(
        const std::string& device_id,
        GetInputStreamParametersCallback callback) override {
      NOTIMPLEMENTED();
    }
    void GetOutputStreamParameters(
        const std::string& device_id,
        GetOutputStreamParametersCallback callback) override {
      NOTIMPLEMENTED();
    }
    void HasInputDevices(HasInputDevicesCallback callback) override {
      NOTIMPLEMENTED();
    }
    void HasOutputDevices(HasOutputDevicesCallback callback) override {
      NOTIMPLEMENTED();
    }
    void GetInputDeviceDescriptions(
        GetInputDeviceDescriptionsCallback callback) override {
      NOTIMPLEMENTED();
    }
    void GetOutputDeviceDescriptions(
        GetOutputDeviceDescriptionsCallback callback) override {
      NOTIMPLEMENTED();
    }
    void GetAssociatedOutputDeviceID(
        const std::string& input_device_id,
        GetAssociatedOutputDeviceIDCallback callback) override {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, base::BindOnce(std::move(callback), kValidReplyId),
          kResponseDelay);
    }
    void GetInputDeviceInfo(const std::string& input_device_id,
                            GetInputDeviceInfoCallback callback) override {
      NOTIMPLEMENTED();
    }
  };  // class MockSystemInfo

  AudioSystemToServiceAdapter::SystemInfoBinder GetSystemInfoBinder() {
    return base::BindRepeating(
        &AudioSystemToServiceAdapterDisconnectTest::BindSystemInfoReceiver,
        base::Unretained(this));
  }

  void BindSystemInfoReceiver(
      mojo::PendingReceiver<mojom::SystemInfo> receiver) {
    ClientConnected();
    system_info_receiver_.Bind(std::move(receiver));
    system_info_receiver_.set_disconnect_handler(base::BindOnce(
        &AudioSystemToServiceAdapterDisconnectTest::OnConnectionError,
        base::Unretained(this)));
  }

  void OnConnectionError() {
    system_info_receiver_.reset();
    ClientDisconnected();
  }

  MOCK_METHOD0(ClientConnected, void(void));
  MOCK_METHOD0(ClientDisconnected, void(void));

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};

  const std::optional<std::string> valid_reply_{kValidReplyId};
  base::MockCallback<media::AudioSystem::OnDeviceIdCallback> response_received_;

  MockSystemInfo mock_system_info_{kResponseDelay};
  mojo::Receiver<mojom::SystemInfo> system_info_receiver_{&mock_system_info_};
};

TEST_F(AudioSystemToServiceAdapterDisconnectTest,
       ResponseDelayIsShorterThanDisconnectTimeout) {
  const base::TimeDelta kDisconnectTimeout = kResponseDelay * 2;
  AudioSystemToServiceAdapter audio_system(GetSystemInfoBinder(),
                                           kDisconnectTimeout);
  {
    EXPECT_CALL(*this, ClientConnected());
    EXPECT_CALL(*this, ClientDisconnected()).Times(0);
    EXPECT_CALL(response_received_, Run(valid_reply_));
    audio_system.GetAssociatedOutputDeviceID(kSomeDeviceId,
                                             response_received_.Get());
    task_environment_.FastForwardBy(kResponseDelay);
  }
  EXPECT_CALL(*this, ClientDisconnected());
  task_environment_.FastForwardBy(kDisconnectTimeout);
}

TEST_F(AudioSystemToServiceAdapterDisconnectTest,
       ResponseDelayIsLongerThanDisconnectTimeout) {
  const base::TimeDelta kDisconnectTimeout = kResponseDelay / 2;
  AudioSystemToServiceAdapter audio_system(GetSystemInfoBinder(),
                                           kDisconnectTimeout);
  {
    EXPECT_CALL(*this, ClientConnected());
    EXPECT_CALL(*this, ClientDisconnected()).Times(0);
    EXPECT_CALL(response_received_, Run(valid_reply_));
    audio_system.GetAssociatedOutputDeviceID(kSomeDeviceId,
                                             response_received_.Get());
    task_environment_.FastForwardBy(kResponseDelay);
  }
  EXPECT_CALL(*this, ClientDisconnected());
  task_environment_.FastForwardBy(kDisconnectTimeout);
}

TEST_F(AudioSystemToServiceAdapterDisconnectTest,
       DisconnectTimeoutIsResetOnSecondRequest) {
  const base::TimeDelta kDisconnectTimeout = kResponseDelay * 1.5;
  AudioSystemToServiceAdapter audio_system(GetSystemInfoBinder(),
                                           kDisconnectTimeout);
  {
    EXPECT_CALL(*this, ClientConnected());
    EXPECT_CALL(*this, ClientDisconnected()).Times(0);
    EXPECT_CALL(response_received_, Run(valid_reply_));
    audio_system.GetAssociatedOutputDeviceID(kSomeDeviceId,
                                             response_received_.Get());
    task_environment_.FastForwardBy(kResponseDelay);
  }
  {
    EXPECT_CALL(*this, ClientConnected()).Times(0);
    EXPECT_CALL(*this, ClientDisconnected()).Times(0);
    EXPECT_CALL(response_received_, Run(valid_reply_));
    audio_system.GetAssociatedOutputDeviceID(kSomeDeviceId,
                                             response_received_.Get());
    task_environment_.FastForwardBy(kResponseDelay);
  }
  EXPECT_CALL(*this, ClientDisconnected());
  task_environment_.FastForwardBy(kDisconnectTimeout);
}

TEST_F(AudioSystemToServiceAdapterDisconnectTest,
       DoesNotDisconnectIfNoTimeout) {
  AudioSystemToServiceAdapter audio_system(GetSystemInfoBinder(),
                                           base::TimeDelta());
  EXPECT_CALL(*this, ClientConnected());
  EXPECT_CALL(*this, ClientDisconnected()).Times(0);
  EXPECT_CALL(response_received_, Run(valid_reply_));
  audio_system.GetAssociatedOutputDeviceID(kSomeDeviceId,
                                           response_received_.Get());
  task_environment_.FastForwardUntilNoTasksRemain();
}

}  // namespace audio

// AudioSystem interface conformance tests.
// AudioSystemTestTemplate is defined in media, so should be its instantiations.
namespace media {

using AudioSystemToServiceAdapterTestVariations =
    testing::Types<audio::AudioSystemToServiceAdapterTestBase>;

INSTANTIATE_TYPED_TEST_SUITE_P(AudioSystemToServiceAdapter,
                               AudioSystemTestTemplate,
                               AudioSystemToServiceAdapterTestVariations);
}  // namespace media
