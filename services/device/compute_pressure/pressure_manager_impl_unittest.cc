// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/pressure_manager_impl.h"

#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/system_cpu/pressure_test_support.h"
#include "services/device/device_service_test_base.h"
#include "services/device/public/mojom/pressure_manager.mojom.h"
#include "services/device/public/mojom/pressure_update.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

constexpr base::TimeDelta kDefaultSamplingIntervalForTesting =
    base::Milliseconds(10);

class FakePressureClient : public mojom::PressureClient {
 public:
  FakePressureClient() : client_(this) {}
  ~FakePressureClient() override = default;

  FakePressureClient(const FakePressureClient&) = delete;
  FakePressureClient& operator=(const FakePressureClient&) = delete;

  // device::mojom::PressureClient implementation.
  void OnPressureUpdated(device::mojom::PressureUpdatePtr update) override {
    updates_.emplace_back(*update);
    if (update_callback_) {
      std::move(update_callback_).Run();
      update_callback_.Reset();
    }
  }

  const std::vector<mojom::PressureUpdate>& updates() const { return updates_; }

  void SetNextUpdateCallback(base::OnceClosure callback) {
    CHECK(!update_callback_) << " already called before update received";
    update_callback_ = std::move(callback);
  }

  void WaitForUpdate() {
    base::RunLoop run_loop;
    SetNextUpdateCallback(run_loop.QuitClosure());
    run_loop.Run();
  }

  static void WaitForUpdates(
      std::initializer_list<FakePressureClient*> clients) {
    base::RunLoop run_loop;
    base::RepeatingClosure update_barrier =
        base::BarrierClosure(clients.size(), run_loop.QuitClosure());
    for (FakePressureClient* client : clients)
      client->SetNextUpdateCallback(update_barrier);
    run_loop.Run();
  }

  mojo::PendingRemote<mojom::PressureClient> BindNewPipeAndPassRemote() {
    return client_.BindNewPipeAndPassRemote();
  }

 private:
  // Used to save PressureState.
  std::vector<mojom::PressureUpdate> updates_;

  // Used to implement WaitForUpdate().
  base::OnceClosure update_callback_;

  mojo::Receiver<mojom::PressureClient> client_;
};

}  // namespace

class PressureManagerImplTest : public DeviceServiceTestBase {
 public:
  PressureManagerImplTest() = default;
  ~PressureManagerImplTest() override = default;

  PressureManagerImplTest(const PressureManagerImplTest&) = delete;
  PressureManagerImplTest& operator=(const PressureManagerImplTest&) = delete;

  void SetUp() override {
    DeviceServiceTestBase::SetUp();

    manager_impl_ = PressureManagerImpl::Create();
    auto fake_cpu_probe = std::make_unique<system_cpu::FakeCpuProbe>();
    fake_cpu_probe->SetLastSample(system_cpu::CpuSample{0.42});
    std::unique_ptr<CpuProbeManager> cpu_probe_manager =
        CpuProbeManager::CreateForTesting(
            std::move(fake_cpu_probe), kDefaultSamplingIntervalForTesting,
            base::BindRepeating(&PressureManagerImpl::UpdateClients,
                                base::Unretained(manager_impl_.get()),
                                mojom::PressureSource::kCpu));
    manager_impl_->SetCpuProbeManagerForTesting(std::move(cpu_probe_manager));
    manager_.reset();
    manager_impl_->Bind(manager_.BindNewPipeAndPassReceiver());
  }

  mojom::PressureStatus AddPressureClient(
      mojo::PendingRemote<mojom::PressureClient> client,
      mojom::PressureSource source) {
    base::test::TestFuture<mojom::PressureStatus> future;
    manager_->AddClient(std::move(client), source, future.GetCallback());
    return future.Get();
  }

 protected:
  std::unique_ptr<PressureManagerImpl> manager_impl_;
  mojo::Remote<mojom::PressureManager> manager_;
};

TEST_F(PressureManagerImplTest, OneClient) {
  FakePressureClient client;
  ASSERT_EQ(AddPressureClient(client.BindNewPipeAndPassRemote(),
                              mojom::PressureSource::kCpu),
            mojom::PressureStatus::kOk);

  client.WaitForUpdate();
  ASSERT_EQ(client.updates().size(), 1u);
  EXPECT_EQ(client.updates()[0].source, mojom::PressureSource::kCpu);
  EXPECT_EQ(client.updates()[0].state, mojom::PressureState::kFair);
}

TEST_F(PressureManagerImplTest, ThreeClients) {
  FakePressureClient client1;
  ASSERT_EQ(AddPressureClient(client1.BindNewPipeAndPassRemote(),
                              mojom::PressureSource::kCpu),
            mojom::PressureStatus::kOk);
  FakePressureClient client2;
  ASSERT_EQ(AddPressureClient(client2.BindNewPipeAndPassRemote(),
                              mojom::PressureSource::kCpu),
            mojom::PressureStatus::kOk);
  FakePressureClient client3;
  ASSERT_EQ(AddPressureClient(client3.BindNewPipeAndPassRemote(),
                              mojom::PressureSource::kCpu),
            mojom::PressureStatus::kOk);

  FakePressureClient::WaitForUpdates({&client1, &client2, &client3});
  ASSERT_EQ(client1.updates().size(), 1u);
  EXPECT_EQ(client1.updates()[0].source, mojom::PressureSource::kCpu);
  EXPECT_EQ(client1.updates()[0].state, mojom::PressureState::kFair);
  ASSERT_EQ(client2.updates().size(), 1u);
  EXPECT_EQ(client2.updates()[0].source, mojom::PressureSource::kCpu);
  EXPECT_EQ(client2.updates()[0].state, mojom::PressureState::kFair);
  ASSERT_EQ(client3.updates().size(), 1u);
  EXPECT_EQ(client3.updates()[0].source, mojom::PressureSource::kCpu);
  EXPECT_EQ(client3.updates()[0].state, mojom::PressureState::kFair);
}

TEST_F(PressureManagerImplTest, AddClientNoProbe) {
  manager_impl_->SetCpuProbeManagerForTesting(nullptr);

  FakePressureClient client;
  ASSERT_EQ(AddPressureClient(client.BindNewPipeAndPassRemote(),
                              mojom::PressureSource::kCpu),
            mojom::PressureStatus::kNotSupported);
}

}  // namespace device
