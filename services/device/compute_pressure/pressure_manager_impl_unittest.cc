// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/pressure_manager_impl.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "services/device/compute_pressure/cpu_probe.h"
#include "services/device/compute_pressure/pressure_test_support.h"
#include "services/device/device_service_test_base.h"
#include "services/device/public/mojom/pressure_manager.mojom.h"
#include "services/device/public/mojom/pressure_state.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

constexpr base::TimeDelta kDefaultSamplingIntervalForTesting = base::Seconds(1);

// Synchronous proxy to a device::mojom::PressureManager.
class PressureManagerImplSync {
 public:
  explicit PressureManagerImplSync(mojom::PressureManager* manager)
      : manager_(*manager) {
    DCHECK(manager);
  }
  ~PressureManagerImplSync() = default;

  PressureManagerImplSync(const PressureManagerImplSync&) = delete;
  PressureManagerImplSync& operator=(const PressureManagerImplSync&) = delete;

  bool AddClient(mojo::PendingRemote<mojom::PressureClient> client) {
    base::test::TestFuture<bool> future;
    manager_.AddClient(std::move(client), future.GetCallback());
    return future.Get();
  }

 private:
  // The reference is immutable, so accessing it is thread-safe. The referenced
  // device::mojom::PressureManager implementation is called synchronously,
  // so it's acceptable to rely on its own thread-safety checks.
  mojom::PressureManager& manager_;
};

class FakePressureClient : public mojom::PressureClient {
 public:
  FakePressureClient() : client_(this) {}
  ~FakePressureClient() override = default;

  FakePressureClient(const FakePressureClient&) = delete;
  FakePressureClient& operator=(const FakePressureClient&) = delete;

  // device::mojom::PressureClient implementation.
  void PressureStateChanged(device::mojom::PressureStatePtr state,
                            base::Time timestamp) override {
    updates_.emplace_back(*state, timestamp);
    if (update_callback_) {
      std::move(update_callback_).Run();
      update_callback_.Reset();
    }
  }

  const std::vector<std::pair<mojom::PressureState, base::Time>>& updates()
      const {
    return updates_;
  }

  void SetNextUpdateCallback(base::OnceClosure callback) {
    DCHECK(!update_callback_) << " already called before update received";
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
  // Used to save pairs of PressureState and its timestamp.
  std::vector<std::pair<mojom::PressureState, base::Time>> updates_;

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
    CreateConnection(std::make_unique<FakeCpuProbe>(),
                     kDefaultSamplingIntervalForTesting);
  }

  void CreateConnection(std::unique_ptr<CpuProbe> cpu_probe,
                        base::TimeDelta sampling_interval) {
    manager_impl_ = PressureManagerImpl::CreateForTesting(std::move(cpu_probe),
                                                          sampling_interval);
    manager_.reset();
    manager_impl_->Bind(manager_.BindNewPipeAndPassReceiver());
    manager_impl_sync_ =
        std::make_unique<PressureManagerImplSync>(manager_.get());
  }

 protected:
  std::unique_ptr<PressureManagerImpl> manager_impl_;
  mojo::Remote<mojom::PressureManager> manager_;
  std::unique_ptr<PressureManagerImplSync> manager_impl_sync_;
};

// Disabled on Fuchsia arm64 debug builds: https://crbug.com/1250654
#if BUILDFLAG(IS_FUCHSIA) && defined(_DEBUG) && defined(ARCH_CPU_ARM64)
#define MAYBE_OneClient DISABLED_OneClient
#elif BUILDFLAG(IS_LINUX) && defined(USE_OZONE)  // https://crbug.com/1226086
#define MAYBE_OneClient DISABLED_OneClient
#else
#define MAYBE_OneClient OneClient
#endif
TEST_F(PressureManagerImplTest, MAYBE_OneClient) {
  FakePressureClient client;
  ASSERT_TRUE(manager_impl_sync_->AddClient(client.BindNewPipeAndPassRemote()));

  client.WaitForUpdate();
  ASSERT_EQ(client.updates().size(), 1u);
  EXPECT_EQ(client.updates()[0].first, mojom::PressureState{0.42});
}

// Disabled on Fuchsia arm64 debug builds: https://crbug.com/1250654
#if BUILDFLAG(IS_FUCHSIA) && defined(_DEBUG) && defined(ARCH_CPU_ARM64)
#define MAYBE_ThreeClients DISABLED_ThreeClients
#elif BUILDFLAG(IS_LINUX) && defined(USE_OZONE)  // https://crbug.com/1226086
#define MAYBE_ThreeClients DISABLED_ThreeClients
#else
#define MAYBE_ThreeClients ThreeClients
#endif
TEST_F(PressureManagerImplTest, MAYBE_ThreeClients) {
  FakePressureClient client1;
  ASSERT_TRUE(
      manager_impl_sync_->AddClient(client1.BindNewPipeAndPassRemote()));
  FakePressureClient client2;
  ASSERT_TRUE(
      manager_impl_sync_->AddClient(client2.BindNewPipeAndPassRemote()));
  FakePressureClient client3;
  ASSERT_TRUE(
      manager_impl_sync_->AddClient(client3.BindNewPipeAndPassRemote()));

  FakePressureClient::WaitForUpdates({&client1, &client2, &client3});
  ASSERT_EQ(client1.updates().size(), 1u);
  EXPECT_EQ(client1.updates()[0].first, mojom::PressureState{0.42});
  ASSERT_EQ(client2.updates().size(), 1u);
  EXPECT_EQ(client2.updates()[0].first, mojom::PressureState{0.42});
  ASSERT_EQ(client3.updates().size(), 1u);
  EXPECT_EQ(client3.updates()[0].first, mojom::PressureState{0.42});
}

TEST_F(PressureManagerImplTest, AddClient_NoProbe) {
  CreateConnection(nullptr, kDefaultSamplingIntervalForTesting);

  FakePressureClient client;
  ASSERT_FALSE(
      manager_impl_sync_->AddClient(client.BindNewPipeAndPassRemote()));
}

}  // namespace device
