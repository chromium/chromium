// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/barrier_closure.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "services/device/public/cpp/test/scoped_pressure_manager_overrider.h"
#include "services/device/public/mojom/pressure_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_declaration.h"
#include "url/gurl.h"

namespace content {

using device::mojom::PressureFactor;
using device::mojom::PressureState;
using device::mojom::PressureStatus;
using device::mojom::PressureUpdate;

namespace {

constexpr base::TimeDelta kSampleInterval = base::Seconds(1);

// Synchronous proxy to a device::mojom::PressureManager.
class PressureManagerSync {
 public:
  explicit PressureManagerSync(device::mojom::PressureManager* manager)
      : manager_(*manager) {
    DCHECK(manager);
  }
  ~PressureManagerSync() = default;

  PressureManagerSync(const PressureManagerSync&) = delete;
  PressureManagerSync& operator=(const PressureManagerSync&) = delete;

  PressureStatus AddClient(
      mojo::PendingRemote<device::mojom::PressureClient> client) {
    base::test::TestFuture<PressureStatus> future;
    manager_->AddClient(std::move(client), future.GetCallback());
    return future.Get();
  }

 private:
  // The reference is immutable, so accessing it is thread-safe. The referenced
  // device::mojom::PressureManager implementation is called synchronously,
  // so it's acceptable to rely on its own thread-safety checks.
  const raw_ref<device::mojom::PressureManager> manager_;
};

// Test double for PressureClient that records all updates.
class FakePressureClient : public device::mojom::PressureClient {
 public:
  FakePressureClient() : receiver_(this) {}
  ~FakePressureClient() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  FakePressureClient(const FakePressureClient&) = delete;
  FakePressureClient& operator=(const FakePressureClient&) = delete;

  // device::mojom::PressureClient implementation.
  void OnPressureUpdated(device::mojom::PressureUpdatePtr state) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    updates_.push_back(*state);
    if (update_callback_) {
      std::move(update_callback_).Run();
      update_callback_.Reset();
    }
  }

  std::vector<PressureUpdate>& updates() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return updates_;
  }

  void SetNextUpdateCallback(base::OnceClosure callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!update_callback_) << " already called before update received";

    update_callback_ = std::move(callback);
  }

  void WaitForUpdate() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    base::RunLoop run_loop;
    SetNextUpdateCallback(run_loop.QuitClosure());
    run_loop.Run();
  }

  static void WaitForUpdates(
      std::initializer_list<FakePressureClient*> clients) {
    base::RunLoop run_loop;
    base::RepeatingClosure update_barrier =
        base::BarrierClosure(clients.size(), run_loop.QuitClosure());
    for (FakePressureClient* client : clients) {
      client->SetNextUpdateCallback(update_barrier);
    }
    run_loop.Run();
  }

  mojo::PendingRemote<device::mojom::PressureClient>
  BindNewPipeAndPassRemote() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  std::vector<PressureUpdate> updates_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Used to implement WaitForUpdate().
  base::OnceClosure update_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::Receiver<device::mojom::PressureClient> receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace

class ComputePressureTest : public RenderViewHostImplTestHarness {
 public:
  ComputePressureTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kComputePressure);
  }

  ~ComputePressureTest() override = default;

  ComputePressureTest(const ComputePressureTest&) = delete;
  ComputePressureTest& operator=(const ComputePressureTest&) = delete;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    NavigateAndCommit(kTestUrl);

    SetComputePressureTest();
  }

  void TearDown() override {
    pressure_manager_sync_.reset();
    pressure_manager_overrider_.reset();
    task_environment()->RunUntilIdle();

    RenderViewHostImplTestHarness::TearDown();
  }

  void SetComputePressureTest() {
    pressure_manager_overrider_ =
        std::make_unique<device::ScopedPressureManagerOverrider>();
    pressure_manager_.reset();
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(contents()->GetPrimaryMainFrame());
    mojo::Receiver<blink::mojom::BrowserInterfaceBroker>& bib =
        rfh->browser_interface_broker_receiver_for_testing();
    blink::mojom::BrowserInterfaceBroker* broker = bib.internal_state()->impl();
    broker->GetInterface(pressure_manager_.BindNewPipeAndPassReceiver());
    pressure_manager_sync_ =
        std::make_unique<PressureManagerSync>(pressure_manager_.get());
    task_environment()->RunUntilIdle();
  }

 protected:
  const GURL kTestUrl{"https://example.com/compute_pressure.html"};
  const GURL kInsecureUrl{"http://example.com/compute_pressure.html"};

  base::test::ScopedFeatureList scoped_feature_list_;

  mojo::Remote<device::mojom::PressureManager> pressure_manager_;
  std::unique_ptr<PressureManagerSync> pressure_manager_sync_;
  std::unique_ptr<device::ScopedPressureManagerOverrider>
      pressure_manager_overrider_;
};

TEST_F(ComputePressureTest, AddClient) {
  FakePressureClient client;
  ASSERT_EQ(
      pressure_manager_sync_->AddClient(client.BindNewPipeAndPassRemote()),
      PressureStatus::kOk);

  const base::Time time = base::Time::Now();
  PressureUpdate update(PressureState::kNominal, {PressureFactor::kThermal},
                        time);
  pressure_manager_overrider_->UpdateClients(update);
  client.WaitForUpdate();
  ASSERT_EQ(client.updates().size(), 1u);
  EXPECT_EQ(client.updates()[0], update);
}

TEST_F(ComputePressureTest, UpdatePressureFactors) {
  FakePressureClient client;
  ASSERT_EQ(
      pressure_manager_sync_->AddClient(client.BindNewPipeAndPassRemote()),
      PressureStatus::kOk);

  const base::Time time = base::Time::Now();
  PressureUpdate update1(PressureState::kNominal,
                         {PressureFactor::kPowerSupply}, time);

  pressure_manager_overrider_->UpdateClients(update1);
  client.WaitForUpdate();
  ASSERT_EQ(client.updates().size(), 1u);
  EXPECT_EQ(client.updates()[0], update1);
  client.updates().clear();

  PressureUpdate update2(
      PressureState::kCritical,
      {PressureFactor::kThermal, PressureFactor::kPowerSupply},
      time + kSampleInterval);
  pressure_manager_overrider_->UpdateClients(update2);
  client.WaitForUpdate();
  ASSERT_EQ(client.updates().size(), 1u);
  EXPECT_EQ(client.updates()[0], update2);
  client.updates().clear();

  PressureUpdate update3(PressureState::kCritical, {PressureFactor::kThermal},
                         time + kSampleInterval * 2);
  pressure_manager_overrider_->UpdateClients(update3);
  client.WaitForUpdate();
  ASSERT_EQ(client.updates().size(), 1u);
  EXPECT_EQ(client.updates()[0], update3);
  client.updates().clear();
}

TEST_F(ComputePressureTest, AddClientNotSupported) {
  pressure_manager_overrider_->set_is_supported(false);

  FakePressureClient client;
  EXPECT_EQ(
      pressure_manager_sync_->AddClient(client.BindNewPipeAndPassRemote()),
      PressureStatus::kNotSupported);
}

TEST_F(ComputePressureTest, InsecureOrigin) {
  NavigateAndCommit(kInsecureUrl);

  SetComputePressureTest();
  EXPECT_EQ(1, process()->bad_msg_count());
}

TEST_F(ComputePressureTest, PermissionsPolicyBlock) {
  // Make compute pressure blocked by permissions policy and it can only be
  // made once on page load, so we refresh the page to simulate that.
  RenderFrameHost* rfh =
      static_cast<RenderFrameHost*>(contents()->GetPrimaryMainFrame());
  blink::ParsedPermissionsPolicy permissions_policy(1);
  permissions_policy[0].feature =
      blink::mojom::PermissionsPolicyFeature::kComputePressure;
  auto navigation_simulator = NavigationSimulator::CreateRendererInitiated(
      rfh->GetLastCommittedURL(), rfh);
  navigation_simulator->SetPermissionsPolicyHeader(permissions_policy);
  navigation_simulator->Commit();

  SetComputePressureTest();
  EXPECT_EQ(1, process()->bad_msg_count());
}

class ComputePressureFencedFrameTest : public ComputePressureTest {
 public:
  ComputePressureFencedFrameTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
  }
  ~ComputePressureFencedFrameTest() override = default;

 protected:
  RenderFrameHost* CreateFencedFrame(RenderFrameHost* parent) {
    RenderFrameHost* fenced_frame =
        RenderFrameHostTester::For(parent)->AppendFencedFrame();
    return fenced_frame;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ComputePressureFencedFrameTest, AccessFromFencedFrame) {
  auto* fenced_frame_rfh = CreateFencedFrame(contents()->GetPrimaryMainFrame());
  // Secure origin check will fail if the RenderFrameHost* passed to it has
  // not navigated to a secure origin, so we need to create a navigation here.
  auto navigation_simulator = NavigationSimulator::CreateRendererInitiated(
      GURL("https://fencedframe.com"), fenced_frame_rfh);
  navigation_simulator->Commit();
  fenced_frame_rfh = static_cast<RenderFrameHostImpl*>(
      navigation_simulator->GetFinalRenderFrameHost());

  mojo::Remote<device::mojom::PressureManager> fenced_frame_pressure_manager;
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker>& bib =
      static_cast<RenderFrameHostImpl*>(fenced_frame_rfh)
          ->browser_interface_broker_receiver_for_testing();
  blink::mojom::BrowserInterfaceBroker* broker = bib.internal_state()->impl();
  broker->GetInterface(
      fenced_frame_pressure_manager.BindNewPipeAndPassReceiver());
  EXPECT_EQ(1, static_cast<TestRenderFrameHost*>(fenced_frame_rfh)
                   ->GetProcess()
                   ->bad_msg_count());
}

}  // namespace content
