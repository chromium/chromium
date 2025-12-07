// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/pressure_manager_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/system_cpu/pressure_test_support.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/system/functions.h"
#include "services/device/compute_pressure/cpu_probe_manager.h"
#include "services/device/compute_pressure/probes_manager.h"
#include "services/device/device_service_test_base.h"
#include "services/device/public/mojom/pressure_manager.mojom-shared.h"
#include "services/device/public/mojom/pressure_manager.mojom.h"
#include "services/device/public/mojom/pressure_update.mojom-shared.h"
#include "services/device/public/mojom/pressure_update.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

class FakePressureClient : public mojom::PressureClient {
 public:
  FakePressureClient() : client_(this) {}
  ~FakePressureClient() override = default;

  FakePressureClient(const FakePressureClient&) = delete;
  FakePressureClient& operator=(const FakePressureClient&) = delete;

  // device::mojom::PressureClient implementation.
  void OnPressureUpdated(mojom::PressureUpdatePtr update) override {
    updates_.push_back(std::move(update));
    if (update_callback_) {
      std::move(update_callback_).Run();
      update_callback_.Reset();
    }
  }

  const std::vector<mojom::PressureUpdatePtr>& updates() const {
    return updates_;
  }

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

  mojo::PendingAssociatedRemote<mojom::PressureClient> get_remote() {
    return client_.BindNewEndpointAndPassRemote();
  }

  bool is_bound() const { return client_.is_bound(); }

 private:
  // Used to save pressure updates.
  std::vector<mojom::PressureUpdatePtr> updates_;

  // Used to implement WaitForUpdate().
  base::OnceClosure update_callback_;

  mojo::AssociatedReceiver<mojom::PressureClient> client_;
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

    manager_impl_ = PressureManagerImpl::Create(TestTimeouts::tiny_timeout());
    auto fake_cpu_probe = std::make_unique<system_cpu::FakeCpuProbe>();
    fake_cpu_probe->SetLastSample(system_cpu::CpuSample{0.63});
    auto* probes_manager = manager_impl_->GetProbesManagerForTesting();
    probes_manager->set_cpu_probe_manager(CpuProbeManager::CreateForTesting(
        probes_manager->sampling_interval_for_testing(),
        probes_manager->cpu_probe_sampling_callback(),
        std::move(fake_cpu_probe)));
    manager_.reset();
    manager_impl_->Bind(manager_.BindNewPipeAndPassReceiver());
  }

  mojom::PressureManagerAddClientResult AddPressureClient(
      FakePressureClient* client,
      mojom::PressureSource source) {
    return AddPressureClient(client, /*token=*/std::nullopt, source);
  }

  mojom::PressureManagerAddClientResult AddPressureClient(
      FakePressureClient* client,
      const std::optional<base::UnguessableToken>& token,
      mojom::PressureSource source) {
    base::test::TestFuture<mojom::PressureManagerAddClientResult> future;
    manager_->AddClient(source, token, client->get_remote(),
                        future.GetCallback());

    return future.Take();
  }

  bool AddVirtualPressureSource(
      const base::UnguessableToken& token,
      mojom::PressureSource source,
      mojom::VirtualPressureSourceMetadataPtr metadata =
          mojom::VirtualPressureSourceMetadata::New()) {
    base::test::TestFuture<void> future;
    manager_->AddVirtualPressureSource(token, source, std::move(metadata),
                                       future.GetCallback());
    return future.Wait();
  }

  bool UpdateVirtualPressureSource(const base::UnguessableToken& token,
                                   mojom::PressureSource source,
                                   mojom::PressureState state,
                                   double own_contribution_estimate) {
    base::test::TestFuture<void> future;
    manager_->UpdateVirtualPressureSourceData(
        token, source, state, own_contribution_estimate, future.GetCallback());
    return future.Wait();
  }

  bool RemoveVirtualPressureSource(const base::UnguessableToken& token,
                                   mojom::PressureSource source) {
    base::test::TestFuture<void> future;
    manager_->RemoveVirtualPressureSource(token, source, future.GetCallback());
    return future.Wait();
  }

 protected:
  std::unique_ptr<PressureManagerImpl> manager_impl_;
  mojo::Remote<mojom::PressureManager> manager_;
};

TEST_F(PressureManagerImplTest, OneClient) {
  FakePressureClient client;
  ASSERT_EQ(AddPressureClient(&client, mojom::PressureSource::kCpu),
            mojom::PressureManagerAddClientResult::kOk);

  client.WaitForUpdate();
  ASSERT_EQ(client.updates().size(), 1u);
  EXPECT_EQ(client.updates()[0]->source, mojom::PressureSource::kCpu);
  // In SetUp() CpuSample = 0.63.
  EXPECT_EQ(client.updates()[0]->data->cpu_utilization, 0.63);
}

TEST_F(PressureManagerImplTest, ThreeClients) {
  FakePressureClient client1;
  ASSERT_EQ(AddPressureClient(&client1, mojom::PressureSource::kCpu),
            mojom::PressureManagerAddClientResult::kOk);
  FakePressureClient client2;
  ASSERT_EQ(AddPressureClient(&client2, mojom::PressureSource::kCpu),
            mojom::PressureManagerAddClientResult::kOk);
  FakePressureClient client3;
  ASSERT_EQ(AddPressureClient(&client3, mojom::PressureSource::kCpu),
            mojom::PressureManagerAddClientResult::kOk);

  // In SetUp() CpuSample = 0.63.
  FakePressureClient::WaitForUpdates({&client1, &client2, &client3});
  ASSERT_EQ(client1.updates().size(), 1u);
  EXPECT_EQ(client1.updates()[0]->source, mojom::PressureSource::kCpu);
  EXPECT_EQ(client1.updates()[0]->data->cpu_utilization, 0.63);
  ASSERT_EQ(client2.updates().size(), 1u);
  EXPECT_EQ(client2.updates()[0]->source, mojom::PressureSource::kCpu);
  EXPECT_EQ(client2.updates()[0]->data->cpu_utilization, 0.63);
  ASSERT_EQ(client3.updates().size(), 1u);
  EXPECT_EQ(client3.updates()[0]->source, mojom::PressureSource::kCpu);
  EXPECT_EQ(client3.updates()[0]->data->cpu_utilization, 0.63);
}

TEST_F(PressureManagerImplTest, AddClientNoProbe) {
  manager_impl_->GetProbesManagerForTesting()->set_cpu_probe_manager(nullptr);

  FakePressureClient client;
  auto result = AddPressureClient(&client, mojom::PressureSource::kCpu);
  ASSERT_EQ(result, mojom::PressureManagerAddClientResult::kNotSupported);
}

TEST_F(PressureManagerImplTest, AddClientInvalidToken) {
  const base::UnguessableToken token1 = base::UnguessableToken::Create();
  const base::UnguessableToken token2 = base::UnguessableToken::Create();

  {
    FakePressureClient client;
    auto result =
        AddPressureClient(&client, token1, mojom::PressureSource::kCpu);
    ASSERT_EQ(result, mojom::PressureManagerAddClientResult::kNotSupported);
  }

  {
    ASSERT_TRUE(
        AddVirtualPressureSource(token2, mojom::PressureSource::kCpu,
                                 mojom::VirtualPressureSourceMetadata::New()));
  }

  {
    FakePressureClient client;
    auto result =
        AddPressureClient(&client, token1, mojom::PressureSource::kCpu);
    ASSERT_EQ(result, mojom::PressureManagerAddClientResult::kNotSupported);
  }
}

TEST_F(PressureManagerImplTest, AddClientExistingToken) {
  const base::UnguessableToken token1 = base::UnguessableToken::Create();

  ASSERT_TRUE(AddVirtualPressureSource(token1, mojom::PressureSource::kCpu));

  EXPECT_TRUE(manager_.is_connected());

  std::string last_received_error;
  mojo::SetDefaultProcessErrorHandler(
      base::BindRepeating([](std::string* out_error,
                             const std::string& error) { *out_error = error; },
                          &last_received_error));

  manager_->AddVirtualPressureSource(
      token1, mojom::PressureSource::kCpu,
      mojom::VirtualPressureSourceMetadata::New(), base::BindOnce([]() {
        FAIL() << "The AddVirtualPressureSource() callback should not have "
                  "been called";
      }));
  manager_.FlushForTesting();
  EXPECT_FALSE(manager_.is_connected());
  EXPECT_EQ("The provided pressure source is already being overridden",
            last_received_error);
}

TEST_F(PressureManagerImplTest, OneClientOneVirtual) {
  FakePressureClient client;
  FakePressureClient virtual_client;

  const base::UnguessableToken token = base::UnguessableToken::Create();

  ASSERT_EQ(AddPressureClient(&client, mojom::PressureSource::kCpu),
            mojom::PressureManagerAddClientResult::kOk);

  ASSERT_TRUE(AddVirtualPressureSource(token, mojom::PressureSource::kCpu));

  ASSERT_EQ(
      AddPressureClient(&virtual_client, token, mojom::PressureSource::kCpu),
      mojom::PressureManagerAddClientResult::kOk);

  EXPECT_TRUE(UpdateVirtualPressureSource(token, mojom::PressureSource::kCpu,
                                          mojom::PressureState::kCritical,
                                          /*own_pressure_estimate=*/0.5));

  FakePressureClient::WaitForUpdates({&client, &virtual_client});

  ASSERT_EQ(client.updates().size(), 1u);
  EXPECT_EQ(client.updates()[0]->source, mojom::PressureSource::kCpu);
  // In SetUp() system_cpu::CpuSample is set to 0.63.
  EXPECT_EQ(client.updates()[0]->data->cpu_utilization, 0.63);

  // Virtual probes run faster than real ones, so we might have more than one
  // update.

  ASSERT_FALSE(virtual_client.updates().empty());

  for (const auto& update : virtual_client.updates()) {
    EXPECT_EQ(update->source, mojom::PressureSource::kCpu);
    // Critical = 1.0 - kThreshold.
    EXPECT_EQ(update->data->cpu_utilization, 0.97);
    EXPECT_NE(client.updates()[0]->timestamp, update->timestamp);
  }
}

TEST_F(PressureManagerImplTest, UpdateVirtualClientWithNoVirtualClient) {
  FakePressureClient client;

  const base::UnguessableToken token = base::UnguessableToken::Create();

  ASSERT_EQ(AddPressureClient(&client, mojom::PressureSource::kCpu),
            mojom::PressureManagerAddClientResult::kOk);

  client.SetNextUpdateCallback(base::BindOnce(
      []() { FAIL() << "The update callback should not have been called"; }));
  EXPECT_TRUE(UpdateVirtualPressureSource(token, mojom::PressureSource::kCpu,
                                          mojom::PressureState::kCritical,
                                          /*own_pressure_estimate=*/0.5));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(client.updates().empty());
}

TEST_F(PressureManagerImplTest, OneVirtualClient) {
  FakePressureClient virtual_client;

  const base::UnguessableToken token = base::UnguessableToken::Create();

  ASSERT_TRUE(AddVirtualPressureSource(token, mojom::PressureSource::kCpu));

  ASSERT_EQ(
      AddPressureClient(&virtual_client, token, mojom::PressureSource::kCpu),
      mojom::PressureManagerAddClientResult::kOk);

  // Values - hysteresis.
  const std::array<double,
                   static_cast<size_t>(mojom::PressureState::kMaxValue) + 1>
      base_thresholds = {0.57,   // kNominal
                         0.72,   // kFair
                         0.87,   // kSerious
                         0.97};  // kCritical

  // Test that all PressureState values are reported correctly.
  for (size_t i = 0; i < static_cast<size_t>(mojom::PressureState::kMaxValue);
       ++i) {
    mojom::PressureState state = static_cast<mojom::PressureState>(i);
    EXPECT_TRUE(UpdateVirtualPressureSource(token, mojom::PressureSource::kCpu,
                                            state,
                                            /*own_pressure_estimate=*/0.5));

    virtual_client.WaitForUpdate();

    ASSERT_EQ(virtual_client.updates().size(), i + 1);
    EXPECT_EQ(virtual_client.updates()[i]->source, mojom::PressureSource::kCpu);
    EXPECT_EQ(virtual_client.updates()[i]->data->cpu_utilization,
              base_thresholds[i]);
  }

  const size_t update_count = virtual_client.updates().size();

  EXPECT_TRUE(RemoveVirtualPressureSource(token, mojom::PressureSource::kCpu));

  // Pressure source was removed.
  EXPECT_TRUE(UpdateVirtualPressureSource(token, mojom::PressureSource::kCpu,
                                          mojom::PressureState::kCritical,
                                          /*own_pressure_estimate=*/0.5));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(virtual_client.updates().size(), update_count);
}

TEST_F(PressureManagerImplTest, ContinuousUpdateReports) {
  FakePressureClient virtual_client;

  const base::UnguessableToken token = base::UnguessableToken::Create();

  ASSERT_TRUE(AddVirtualPressureSource(token, mojom::PressureSource::kCpu));

  ASSERT_EQ(
      AddPressureClient(&virtual_client, token, mojom::PressureSource::kCpu),
      mojom::PressureManagerAddClientResult::kOk);

  const mojom::PressureState state = mojom::PressureState::kSerious;

  EXPECT_TRUE(UpdateVirtualPressureSource(token, mojom::PressureSource::kCpu,
                                          state,
                                          /*own_pressure_estimate=*/0.5));

  virtual_client.WaitForUpdate();
  virtual_client.WaitForUpdate();
  ASSERT_EQ(virtual_client.updates().size(), 2U);

  EXPECT_EQ(virtual_client.updates()[1]->data->cpu_utilization,
            virtual_client.updates()[0]->data->cpu_utilization);
  EXPECT_GT(virtual_client.updates()[1]->timestamp,
            virtual_client.updates()[0]->timestamp);
}

TEST_F(PressureManagerImplTest, SameStateUpdatesAreNotDropped) {
  FakePressureClient virtual_client;

  const base::UnguessableToken token = base::UnguessableToken::Create();

  ASSERT_TRUE(AddVirtualPressureSource(token, mojom::PressureSource::kCpu));

  ASSERT_EQ(
      AddPressureClient(&virtual_client, token, mojom::PressureSource::kCpu),
      mojom::PressureManagerAddClientResult::kOk);

  const mojom::PressureState state = mojom::PressureState::kSerious;

  EXPECT_TRUE(UpdateVirtualPressureSource(token, mojom::PressureSource::kCpu,
                                          state,
                                          /*own_pressure_estimate=*/0.5));

  virtual_client.WaitForUpdate();
  ASSERT_EQ(virtual_client.updates().size(), 1U);

  EXPECT_TRUE(UpdateVirtualPressureSource(token, mojom::PressureSource::kCpu,
                                          state,
                                          /*own_pressure_estimate=*/0.5));

  virtual_client.WaitForUpdate();
  ASSERT_EQ(virtual_client.updates().size(), 2U);

  EXPECT_EQ(virtual_client.updates()[1]->data->cpu_utilization,
            virtual_client.updates()[0]->data->cpu_utilization);
  EXPECT_GT(virtual_client.updates()[1]->timestamp,
            virtual_client.updates()[0]->timestamp);
}

TEST_F(PressureManagerImplTest, VirtualPressureSourceNotAvailable) {
  const base::UnguessableToken token = base::UnguessableToken::Create();

  ASSERT_TRUE(AddVirtualPressureSource(
      token, mojom::PressureSource::kCpu,
      mojom::VirtualPressureSourceMetadata::New(/*available=*/false)));

  FakePressureClient client;
  auto result = AddPressureClient(&client, token, mojom::PressureSource::kCpu);
  ASSERT_EQ(result, mojom::PressureManagerAddClientResult::kNotSupported);
}

}  // namespace device
