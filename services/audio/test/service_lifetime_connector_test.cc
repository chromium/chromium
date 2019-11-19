// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "services/audio/in_process_audio_manager_accessor.h"
#include "services/audio/public/mojom/constants.mojom.h"
#include "services/audio/service.h"
#include "services/audio/service_factory.h"
#include "services/audio/system_info.h"
#include "services/service_manager/public/cpp/binder_map.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Exactly;
using testing::StrictMock;

namespace {
constexpr base::TimeDelta kQuitTimeout = base::TimeDelta::FromMilliseconds(100);
}  // namespace

namespace audio {

// Connector-based tests for Audio service "quit with timeout" logic
class AudioServiceLifetimeConnectorTest : public testing::Test {
 public:
  AudioServiceLifetimeConnectorTest() {}
  ~AudioServiceLifetimeConnectorTest() override {}

  void SetUp() override {
    audio_manager_ = std::make_unique<media::MockAudioManager>(
        std::make_unique<media::TestAudioThread>(
            false /*not using a separate audio thread*/));
    service_ = std::make_unique<Service>(
        std::make_unique<InProcessAudioManagerAccessor>(audio_manager_.get()),
        kQuitTimeout, false /* device_notifications_enabled */,
        std::make_unique<service_manager::BinderMap>(),
        connector_factory_.RegisterInstance(mojom::kServiceName));
    service_->set_termination_closure(quit_request_.Get());
    connector_ = connector_factory_.CreateConnector();
  }

  void TearDown() override {
    if (audio_manager_)
      audio_manager_->Shutdown();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  StrictMock<base::MockCallback<base::RepeatingClosure>> quit_request_;
  std::unique_ptr<media::MockAudioManager> audio_manager_;
  service_manager::TestConnectorFactory connector_factory_;
  std::unique_ptr<Service> service_;
  std::unique_ptr<service_manager::Connector> connector_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioServiceLifetimeConnectorTest);
};

#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
// For platforms where the standalone audio service is supported, the
// service should terminate after a default timeout if no specific timeout has
// been set.
// Disabled due to flakiness.
// TODO(crbug.com/976841): Fix the flakiness and re-enable this.
TEST_F(AudioServiceLifetimeConnectorTest,
       DISABLED_StandaloneServiceTerminatesWhenNoTimeoutIsSet) {
  service_.reset();
  audio_manager_->Shutdown();
  audio_manager_.reset();
  service_manager::TestConnectorFactory connector_factory;
  service_ = audio::CreateStandaloneService(
      std::make_unique<service_manager::BinderMap>(),
      connector_factory.RegisterInstance(mojom::kServiceName));
  service_->set_termination_closure(quit_request_.Get());
  connector_ = connector_factory.CreateConnector();
  task_environment_.RunUntilIdle();

  mojom::SystemInfoPtr info;
  connector_->BindInterface(mojom::kServiceName, &info);

  // Make sure |info| is connected.
  base::RunLoop loop;
  info->HasOutputDevices(
      base::BindOnce([](base::OnceClosure cl, bool) { std::move(cl).Run(); },
                     loop.QuitClosure()));
  loop.Run();

  const base::TimeDelta default_timeout = base::TimeDelta::FromMinutes(15);
  {
    // Make sure the service does not disconnect before a timeout.
    EXPECT_CALL(quit_request_, Run()).Times(Exactly(0));
    info.reset();
    task_environment_.FastForwardBy(default_timeout / 2);
  }

  // Now wait for what is left from of the timeout: the service should
  // disconnect.
  EXPECT_CALL(quit_request_, Run()).Times(Exactly(1));
  task_environment_.FastForwardBy(default_timeout / 2);

  service_.reset();
}
#else
// For platforms where the standalone audio service has not been launched, the
// service should never terminate if no specific timeout has been set.
TEST_F(AudioServiceLifetimeConnectorTest,
       StandaloneServiceNeverTerminatesWhenNoTimeoutIsSet) {
  service_.reset();
  audio_manager_->Shutdown();
  audio_manager_.reset();
  service_manager::TestConnectorFactory connector_factory;
  service_ = audio::CreateStandaloneService(
      std::make_unique<service_manager::BinderMap>(),
      connector_factory.RegisterInstance(mojom::kServiceName));
  service_->set_termination_closure(quit_request_.Get());
  connector_ = connector_factory.CreateConnector();
  task_environment_.RunUntilIdle();

  mojom::SystemInfoPtr info;
  connector_->BindInterface(mojom::kServiceName, &info);

  // Make sure |info| is connected.
  base::RunLoop loop;
  info->HasOutputDevices(
      base::BindOnce([](base::OnceClosure cl, bool) { std::move(cl).Run(); },
                     loop.QuitClosure()));
  loop.Run();

  info.reset();

  task_environment_.RunUntilIdle();

  service_.reset();
}
#endif

TEST_F(AudioServiceLifetimeConnectorTest,
       EmbeddedServiceNeverTerminatesWhenNoTimeoutIsSet) {
  service_.reset();
  service_manager::TestConnectorFactory connector_factory;
  service_ = audio::CreateEmbeddedService(
      audio_manager_.get(),
      connector_factory.RegisterInstance(mojom::kServiceName));
  service_->set_termination_closure(quit_request_.Get());
  connector_ = connector_factory.CreateConnector();
  task_environment_.RunUntilIdle();

  mojom::SystemInfoPtr info;
  connector_->BindInterface(mojom::kServiceName, &info);

  // Make sure |info| is connected.
  base::RunLoop loop;
  info->HasOutputDevices(
      base::BindOnce([](base::OnceClosure cl, bool) { std::move(cl).Run(); },
                     loop.QuitClosure()));
  loop.Run();

  info.reset();

  task_environment_.RunUntilIdle();

  service_.reset();
}

TEST_F(AudioServiceLifetimeConnectorTest, ServiceNotQuitWhenClientConnected) {
  EXPECT_CALL(quit_request_, Run()).Times(Exactly(0));

  mojom::SystemInfoPtr info;
  connector_->BindInterface(mojom::kServiceName, &info);
  EXPECT_TRUE(info.is_bound());

  task_environment_.FastForwardBy(kQuitTimeout * 2);
  EXPECT_TRUE(info.is_bound());
}

TEST_F(AudioServiceLifetimeConnectorTest,
       ServiceQuitAfterTimeoutWhenClientDisconnected) {
  mojom::SystemInfoPtr info;
  connector_->BindInterface(mojom::kServiceName, &info);

  {
    // Make sure the service does not disconnect before a timeout.
    EXPECT_CALL(quit_request_, Run()).Times(Exactly(0));
    info.reset();
    task_environment_.FastForwardBy(kQuitTimeout / 2);
  }
  // Now wait for what is left from of the timeout: the service should
  // disconnect.
  EXPECT_CALL(quit_request_, Run()).Times(Exactly(1));
  task_environment_.FastForwardBy(kQuitTimeout / 2);
}

TEST_F(AudioServiceLifetimeConnectorTest,
       ServiceNotQuitWhenAnotherClientQuicklyConnects) {
  EXPECT_CALL(quit_request_, Run()).Times(Exactly(0));

  mojom::SystemInfoPtr info1;
  connector_->BindInterface(mojom::kServiceName, &info1);
  EXPECT_TRUE(info1.is_bound());

  info1.reset();

  mojom::SystemInfoPtr info2;
  connector_->BindInterface(mojom::kServiceName, &info2);
  EXPECT_TRUE(info2.is_bound());

  task_environment_.FastForwardBy(kQuitTimeout);
  EXPECT_TRUE(info2.is_bound());
}

TEST_F(AudioServiceLifetimeConnectorTest,
       ServiceNotQuitWhenOneClientRemainsConnected) {
  mojom::SystemInfoPtr info1;
  mojom::SystemInfoPtr info2;
  {
    EXPECT_CALL(quit_request_, Run()).Times(Exactly(0));

    connector_->BindInterface(mojom::kServiceName, &info1);
    EXPECT_TRUE(info1.is_bound());
    connector_->BindInterface(mojom::kServiceName, &info2);
    EXPECT_TRUE(info2.is_bound());

    task_environment_.FastForwardBy(kQuitTimeout);
    EXPECT_TRUE(info1.is_bound());
    EXPECT_TRUE(info2.is_bound());

    info1.reset();
    EXPECT_TRUE(info2.is_bound());

    task_environment_.FastForwardBy(kQuitTimeout);
    EXPECT_FALSE(info1.is_bound());
    EXPECT_TRUE(info2.is_bound());
  }
  // Now disconnect the last client and wait for service quit request.
  EXPECT_CALL(quit_request_, Run()).Times(Exactly(1));
  info2.reset();
  task_environment_.FastForwardBy(kQuitTimeout);
}

TEST_F(AudioServiceLifetimeConnectorTest,
       QuitTimeoutIsNotShortenedAfterDelayedReconnect) {
  mojom::SystemInfoPtr info1;
  mojom::SystemInfoPtr info2;
  {
    EXPECT_CALL(quit_request_, Run()).Times(Exactly(0));

    connector_->BindInterface(mojom::kServiceName, &info1);
    EXPECT_TRUE(info1.is_bound());
    info1.reset();
    task_environment_.FastForwardBy(kQuitTimeout * 0.75);

    connector_->BindInterface(mojom::kServiceName, &info2);
    EXPECT_TRUE(info2.is_bound());
    info2.reset();
    task_environment_.FastForwardBy(kQuitTimeout * 0.75);
  }
  EXPECT_CALL(quit_request_, Run()).Times(Exactly(1));
  task_environment_.FastForwardBy(kQuitTimeout * 0.25);
}

}  // namespace audio
