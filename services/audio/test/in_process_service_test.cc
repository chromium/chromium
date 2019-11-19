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
#include "services/audio/public/cpp/manifest.h"
#include "services/audio/public/mojom/constants.mojom.h"
#include "services/audio/service.h"
#include "services/audio/test/service_lifetime_test_template.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/test/test_service_manager.h"
#include "services/service_manager/public/mojom/constants.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Exactly;
using testing::Invoke;

namespace audio {

class ServiceTestHelper : public service_manager::Service {
 public:
  class AudioThreadContext
      : public base::RefCountedThreadSafe<AudioThreadContext> {
   public:
    AudioThreadContext(media::AudioManager* audio_manager,
                       base::TimeDelta service_quit_timeout)
        : audio_manager_(audio_manager),
          service_quit_timeout_(service_quit_timeout) {}

    void CreateServiceOnAudioThread(
        service_manager::mojom::ServiceRequest request) {
      if (!audio_manager_->GetTaskRunner()->BelongsToCurrentThread()) {
        audio_manager_->GetTaskRunner()->PostTask(
            FROM_HERE,
            base::BindOnce(&AudioThreadContext::CreateServiceOnAudioThread,
                           this, std::move(request)));
        return;
      }
      DCHECK(!service_);
      service_ = std::make_unique<audio::Service>(
          std::make_unique<InProcessAudioManagerAccessor>(audio_manager_),
          service_quit_timeout_, false /* device_notifications_enabled */,
          std::make_unique<service_manager::BinderMap>(), std::move(request));
      service_->set_termination_closure(base::BindOnce(
          &AudioThreadContext::QuitOnAudioThread, base::Unretained(this)));
    }

    void QuitOnAudioThread() {
      DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
      service_.reset();
    }

   private:
    friend class base::RefCountedThreadSafe<AudioThreadContext>;
    virtual ~AudioThreadContext() = default;

    media::AudioManager* const audio_manager_;
    const base::TimeDelta service_quit_timeout_;
    std::unique_ptr<Service> service_;

    DISALLOW_COPY_AND_ASSIGN(AudioThreadContext);
  };

  ServiceTestHelper(media::AudioManager* audio_manager,
                    base::TimeDelta service_quit_timeout,
                    service_manager::mojom::ServiceRequest request)
      : service_binding_(this, std::move(request)),
        audio_manager_(audio_manager),
        audio_thread_context_(
            new AudioThreadContext(audio_manager, service_quit_timeout)) {}

  ~ServiceTestHelper() override {
    // Ensure that the AudioThreadContext is destroyed on the correct thread by
    // passing our only reference into a task posted there.
    audio_manager_->GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&AudioThreadContext::QuitOnAudioThread,
                                  std::move(audio_thread_context_)));
  }

  service_manager::Connector* connector() {
    return service_binding_.GetConnector();
  }

 protected:
  // service_manager::Service:
  void CreatePackagedServiceInstance(
      const std::string& service_name,
      mojo::PendingReceiver<service_manager::mojom::Service> receiver,
      CreatePackagedServiceInstanceCallback callback) override {
    if (service_name == mojom::kServiceName) {
      audio_thread_context_->CreateServiceOnAudioThread(std::move(receiver));
      std::move(callback).Run(base::GetCurrentProcId());
    } else {
      std::move(callback).Run(base::nullopt);
    }
  }

 private:
  service_manager::ServiceBinding service_binding_;
  media::AudioManager* const audio_manager_;
  scoped_refptr<AudioThreadContext> audio_thread_context_;

  DISALLOW_COPY_AND_ASSIGN(ServiceTestHelper);
};

const char kTestServiceName[] = "audio_unittests";

// if |use_audio_thread| is true, AudioManager has a dedicated audio thread and
// Audio service lives on it; otherwise audio thread is the main thread of the
// test fixture, and that's where Service lives. So in the former case the
// service is accessed from another thread, and in the latter case - from the
// thread it lives on (which imitates access to Audio service from UI thread on
// Mac).
template <bool use_audio_thread>
class InProcessServiceTest : public testing::Test {
 public:
  explicit InProcessServiceTest(base::TimeDelta service_quit_timeout)
      : test_service_manager_(
            {service_manager::ManifestBuilder()
                 .WithServiceName(kTestServiceName)
                 .RequireCapability(mojom::kServiceName, "info")
                 .RequireCapability(service_manager::mojom::kServiceName,
                                    "service_manager:service_manager")
                 .PackageService(
                     GetManifest(service_manager::Manifest::ExecutionMode ::
                                     kInProcessBuiltin))
                 .Build()}),
        audio_manager_(
            std::make_unique<media::TestAudioThread>(use_audio_thread)),
        helper_(std::make_unique<ServiceTestHelper>(
            &audio_manager_,
            service_quit_timeout,
            test_service_manager_.RegisterTestInstance(kTestServiceName))),
        audio_system_(std::make_unique<AudioSystemToServiceAdapter>(
            connector()->Clone())) {}

  InProcessServiceTest()
      : InProcessServiceTest(base::TimeDelta() /* not timeout */) {}

  ~InProcessServiceTest() override {}

 protected:
  service_manager::Connector* connector() {
    DCHECK(helper_);
    return helper_->connector();
  }

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
  base::test::TaskEnvironment task_environment_;
  service_manager::TestServiceManager test_service_manager_;
  media::MockAudioManager audio_manager_;
  std::unique_ptr<ServiceTestHelper> helper_;
  std::unique_ptr<media::AudioSystem> audio_system_;

  DISALLOW_COPY_AND_ASSIGN(InProcessServiceTest);
};

// Tests for FakeSystemInfo overriding the global binder.
class FakeSystemInfoTest : public InProcessServiceTest<false>,
                           public FakeSystemInfo {
 public:
  FakeSystemInfoTest() {}
  ~FakeSystemInfoTest() override {}

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

// Service lifetime tests.
class InProcessServiceLifetimeTestBase : public InProcessServiceTest<false> {
 public:
  using TestBase = InProcessServiceTest<false>;

  InProcessServiceLifetimeTestBase()
      : TestBase(base::TimeDelta::FromMilliseconds(1)) {}
  ~InProcessServiceLifetimeTestBase() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InProcessServiceLifetimeTestBase);
};

INSTANTIATE_TYPED_TEST_SUITE_P(InProcessAudioService,
                               ServiceLifetimeTestTemplate,
                               InProcessServiceLifetimeTestBase);

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
