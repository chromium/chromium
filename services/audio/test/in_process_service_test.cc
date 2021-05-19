// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/task_environment.h"
#include "media/audio/audio_system_test_util.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "services/audio/in_process_audio_manager_accessor.h"
#include "services/audio/public/cpp/audio_system_to_service_adapter.h"
#include "services/audio/public/cpp/fake_system_info.h"
#include "services/audio/public/mojom/audio_service.mojom.h"
#include "services/audio/service.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Exactly;
using testing::Invoke;

namespace audio {

class ServiceTestHelper {
 public:
  class AudioThreadContext
      : public base::RefCountedThreadSafe<AudioThreadContext> {
   public:
    explicit AudioThreadContext(media::AudioManager* audio_manager)
        : audio_manager_(audio_manager) {}

    void CreateServiceOnAudioThread(
        mojo::PendingReceiver<mojom::AudioService> receiver) {
      if (!audio_manager_->GetTaskRunner()->BelongsToCurrentThread()) {
        audio_manager_->GetTaskRunner()->PostTask(
            FROM_HERE,
            base::BindOnce(&AudioThreadContext::CreateServiceOnAudioThread,
                           this, std::move(receiver)));
        return;
      }
      DCHECK(!service_);
      service_ = std::make_unique<audio::Service>(
          std::make_unique<InProcessAudioManagerAccessor>(audio_manager_),
          false /* device_notifications_enabled */, std::move(receiver));
    }

    void QuitOnAudioThread() {
      DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
      service_.reset();
    }

   private:
    friend class base::RefCountedThreadSafe<AudioThreadContext>;
    virtual ~AudioThreadContext() = default;

    media::AudioManager* const audio_manager_;
    std::unique_ptr<Service> service_;

    DISALLOW_COPY_AND_ASSIGN(AudioThreadContext);
  };

  explicit ServiceTestHelper(media::AudioManager* audio_manager)
      : audio_manager_(audio_manager),
        audio_thread_context_(new AudioThreadContext(audio_manager)) {
    audio_thread_context_->CreateServiceOnAudioThread(
        service_remote_.BindNewPipeAndPassReceiver());
  }

  ~ServiceTestHelper() {
    // Ensure that the AudioThreadContext is destroyed on the correct thread by
    // passing our only reference into a task posted there.
    audio_manager_->GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&AudioThreadContext::QuitOnAudioThread,
                                  std::move(audio_thread_context_)));
  }

  mojom::AudioService& service() { return *service_remote_.get(); }

 private:
  media::AudioManager* const audio_manager_;
  mojo::Remote<mojom::AudioService> service_remote_;
  scoped_refptr<AudioThreadContext> audio_thread_context_;

  DISALLOW_COPY_AND_ASSIGN(ServiceTestHelper);
};

// if |use_audio_thread| is true, AudioManager has a dedicated audio thread and
// Audio service lives on it; otherwise audio thread is the main thread of the
// test fixture, and that's where Service lives. So in the former case the
// service is accessed from another thread, and in the latter case - from the
// thread it lives on (which imitates access to Audio service from UI thread on
// Mac).
template <bool use_audio_thread>
class InProcessServiceTest : public testing::Test {
 public:
  InProcessServiceTest()
      : audio_manager_(
            std::make_unique<media::TestAudioThread>(use_audio_thread)),
        helper_(std::make_unique<ServiceTestHelper>(&audio_manager_)),
        audio_system_(std::make_unique<AudioSystemToServiceAdapter>(
            base::BindRepeating(&InProcessServiceTest::BindSystemInfo,
                                base::Unretained(this)))) {}
  ~InProcessServiceTest() override = default;

 protected:
  void TearDown() override {
    audio_system_.reset();

    // Deletes ServiceTestHelper, which will result in posting
    // AuioThreadContext::QuitOnAudioThread() to AudioManager thread, so that
    // Service is delete there.
    helper_.reset();

    // Joins AudioManager thread if it is used.
    audio_manager_.Shutdown();
  }

 protected:
  media::MockAudioManager* audio_manager() { return &audio_manager_; }
  media::AudioSystem* audio_system() { return audio_system_.get(); }

 private:
  void BindSystemInfo(mojo::PendingReceiver<mojom::SystemInfo> receiver) {
    helper_->service().BindSystemInfo(std::move(receiver));
  }

  base::test::TaskEnvironment task_environment_;
  media::MockAudioManager audio_manager_;
  std::unique_ptr<ServiceTestHelper> helper_;
  std::unique_ptr<media::AudioSystem> audio_system_;

  DISALLOW_COPY_AND_ASSIGN(InProcessServiceTest);
};

// Tests for FakeSystemInfo overriding the global binder.
class FakeSystemInfoTest : public InProcessServiceTest<false>,
                           public FakeSystemInfo {
 public:
  FakeSystemInfoTest() = default;
  ~FakeSystemInfoTest() override = default;

 protected:
  MOCK_METHOD0(MethodCalled, void());

 private:
  void HasInputDevices(HasInputDevicesCallback callback) override {
    std::move(callback).Run(true);
    MethodCalled();
  }

  DISALLOW_COPY_AND_ASSIGN(FakeSystemInfoTest);
};

TEST_F(FakeSystemInfoTest, HasInputDevicesCalledOnGlobalBinderOverride) {
  FakeSystemInfo::OverrideGlobalBinderForAudioService(this);
  base::RunLoop wait_loop;
  EXPECT_CALL(*this, MethodCalled())
      .WillOnce(testing::Invoke(&wait_loop, &base::RunLoop::Quit));
  audio_system()->HasInputDevices(base::BindOnce([](bool) {}));
  wait_loop.Run();
  FakeSystemInfo::ClearGlobalBinderForAudioService();
}

}  // namespace audio

// AudioSystem interface conformance tests.
// AudioSystemTestTemplate is defined in media, so should be its instantiations.
namespace media {

using AudioSystemTestVariations =
    testing::Types<audio::InProcessServiceTest<false>,
                   audio::InProcessServiceTest<true>>;

INSTANTIATE_TYPED_TEST_SUITE_P(InProcessAudioService,
                               AudioSystemTestTemplate,
                               AudioSystemTestVariations);

}  // namespace media
