// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/pressure_service_impl.h"

#include <vector>

#include "base/barrier_closure.h"
#include "base/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/device/public/cpp/test/scoped_pressure_manager_overrider.h"
#include "services/device/public/mojom/pressure_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/compute_pressure/pressure_service.mojom.h"
#include "url/gurl.h"

namespace content {

using device::mojom::PressureFactor;
using device::mojom::PressureState;
using device::mojom::PressureUpdate;

namespace {

constexpr base::TimeDelta kSampleInterval = base::Seconds(1);

// Synchronous proxy to a blink::mojom::PressureService.
class PressureServiceImplSync {
 public:
  explicit PressureServiceImplSync(blink::mojom::PressureService* service)
      : service_(*service) {
    DCHECK(service);
  }
  ~PressureServiceImplSync() = default;

  PressureServiceImplSync(const PressureServiceImplSync&) = delete;
  PressureServiceImplSync& operator=(const PressureServiceImplSync&) = delete;

  blink::mojom::PressureStatus BindObserver(
      mojo::PendingRemote<blink::mojom::PressureObserver> observer) {
    base::test::TestFuture<blink::mojom::PressureStatus> future;
    service_->BindObserver(std::move(observer), future.GetCallback());
    return future.Get();
  }

 private:
  // The reference is immutable, so accessing it is thread-safe. The referenced
  // blink::mojom::PressureService implementation is called synchronously,
  // so it's acceptable to rely on its own thread-safety checks.
  const raw_ref<blink::mojom::PressureService> service_;
};

// Test double for PressureObserver that records all updates.
class FakePressureObserver : public blink::mojom::PressureObserver {
 public:
  FakePressureObserver() : receiver_(this) {}
  ~FakePressureObserver() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  FakePressureObserver(const FakePressureObserver&) = delete;
  FakePressureObserver& operator=(const FakePressureObserver&) = delete;

  // blink::mojom::PressureObserver implementation.
  void OnUpdate(device::mojom::PressureUpdatePtr state) override {
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
      std::initializer_list<FakePressureObserver*> observers) {
    base::RunLoop run_loop;
    base::RepeatingClosure update_barrier =
        base::BarrierClosure(observers.size(), run_loop.QuitClosure());
    for (FakePressureObserver* observer : observers)
      observer->SetNextUpdateCallback(update_barrier);
    run_loop.Run();
  }

  mojo::PendingRemote<blink::mojom::PressureObserver>
  BindNewPipeAndPassRemote() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  std::vector<PressureUpdate> updates_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Used to implement WaitForUpdate().
  base::OnceClosure update_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::Receiver<blink::mojom::PressureObserver> receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace

class PressureServiceImplTest : public RenderViewHostImplTestHarness {
 public:
  PressureServiceImplTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kComputePressure);
  }

  ~PressureServiceImplTest() override = default;

  PressureServiceImplTest(const PressureServiceImplTest&) = delete;
  PressureServiceImplTest& operator=(const PressureServiceImplTest&) = delete;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    NavigateAndCommit(kTestUrl);

    SetPressureServiceImpl();
  }

  void TearDown() override {
    pressure_service_impl_sync_.reset();
    pressure_manager_overrider_.reset();
    task_environment()->RunUntilIdle();

    RenderViewHostImplTestHarness::TearDown();
  }

  void SetPressureServiceImpl() {
    pressure_manager_overrider_ =
        std::make_unique<device::ScopedPressureManagerOverrider>();
    pressure_service_.reset();
    PressureServiceImpl::Create(contents()->GetPrimaryMainFrame(),
                                pressure_service_.BindNewPipeAndPassReceiver());
    pressure_service_impl_sync_ =
        std::make_unique<PressureServiceImplSync>(pressure_service_.get());
  }

 protected:
  const GURL kTestUrl{"https://example.com/compute_pressure.html"};
  const GURL kInsecureUrl{"http://example.com/compute_pressure.html"};

  base::test::ScopedFeatureList scoped_feature_list_;

  mojo::Remote<blink::mojom::PressureService> pressure_service_;
  std::unique_ptr<PressureServiceImplSync> pressure_service_impl_sync_;
  std::unique_ptr<device::ScopedPressureManagerOverrider>
      pressure_manager_overrider_;
};

TEST_F(PressureServiceImplTest, BindObserver) {
  FakePressureObserver observer;
  ASSERT_EQ(pressure_service_impl_sync_->BindObserver(
                observer.BindNewPipeAndPassRemote()),
            blink::mojom::PressureStatus::kOk);

  const base::Time time = base::Time::Now();
  PressureUpdate update(PressureState::kNominal, {PressureFactor::kThermal},
                        time);
  pressure_manager_overrider_->UpdateClients(update);
  observer.WaitForUpdate();
  ASSERT_EQ(observer.updates().size(), 1u);
  EXPECT_EQ(observer.updates()[0], update);
}

TEST_F(PressureServiceImplTest, UpdatePressureFactors) {
  FakePressureObserver observer;
  ASSERT_EQ(pressure_service_impl_sync_->BindObserver(
                observer.BindNewPipeAndPassRemote()),
            blink::mojom::PressureStatus::kOk);

  const base::Time time = base::Time::Now();
  PressureUpdate update1(PressureState::kNominal,
                         {PressureFactor::kPowerSupply}, time);

  pressure_manager_overrider_->UpdateClients(update1);
  observer.WaitForUpdate();
  ASSERT_EQ(observer.updates().size(), 1u);
  EXPECT_EQ(observer.updates()[0], update1);
  observer.updates().clear();

  PressureUpdate update2(
      PressureState::kCritical,
      {PressureFactor::kThermal, PressureFactor::kPowerSupply},
      time + kSampleInterval);
  pressure_manager_overrider_->UpdateClients(update2);
  observer.WaitForUpdate();
  ASSERT_EQ(observer.updates().size(), 1u);
  EXPECT_EQ(observer.updates()[0], update2);
  observer.updates().clear();

  PressureUpdate update3(PressureState::kCritical, {PressureFactor::kThermal},
                         time + kSampleInterval * 2);
  pressure_manager_overrider_->UpdateClients(update3);
  observer.WaitForUpdate();
  ASSERT_EQ(observer.updates().size(), 1u);
  EXPECT_EQ(observer.updates()[0], update3);
  observer.updates().clear();
}

// TODO(crbug.com/1385588): Remove this when "passes privacy test" steps are
// implemented.
TEST_F(PressureServiceImplTest, NoVisibility) {
  FakePressureObserver observer;
  ASSERT_EQ(pressure_service_impl_sync_->BindObserver(
                observer.BindNewPipeAndPassRemote()),
            blink::mojom::PressureStatus::kOk);

  const base::Time time = base::Time::Now();

  test_rvh()->SimulateWasHidden();

  // The first two updates should be blocked due to invisibility.
  PressureUpdate update1(PressureState::kNominal, {}, time);
  pressure_manager_overrider_->UpdateClients(update1);
  PressureUpdate update2(PressureState::kCritical, {PressureFactor::kThermal},
                         time + kSampleInterval);
  pressure_manager_overrider_->UpdateClients(update2);
  task_environment()->RunUntilIdle();

  test_rvh()->SimulateWasShown();

  // The third update should be dispatched.
  PressureUpdate update3(PressureState::kFair, {PressureFactor::kThermal},
                         time + kSampleInterval * 2);
  pressure_manager_overrider_->UpdateClients(update3);
  observer.WaitForUpdate();

  ASSERT_EQ(observer.updates().size(), 1u);
  EXPECT_EQ(observer.updates()[0], update3);
}

class PressureServiceImplFencedFrameTest : public PressureServiceImplTest {
 public:
  PressureServiceImplFencedFrameTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
  }
  ~PressureServiceImplFencedFrameTest() override = default;

 protected:
  RenderFrameHost* CreateFencedFrame(RenderFrameHost* parent) {
    RenderFrameHost* fenced_frame =
        RenderFrameHostTester::For(parent)->AppendFencedFrame();
    return fenced_frame;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PressureServiceImplFencedFrameTest, BindObserverFromFencedFrame) {
  auto* fenced_frame_rfh = CreateFencedFrame(contents()->GetPrimaryMainFrame());
  // PressureServiceImpl::Create() will fail if the RenderFrameHost* passed to
  // it has not navigated to a secure origin, so we need to create a navigation
  // here.
  auto navigation_simulator = NavigationSimulator::CreateRendererInitiated(
      GURL("https://fencedframe.com"), fenced_frame_rfh);
  navigation_simulator->Commit();
  fenced_frame_rfh = static_cast<RenderFrameHostImpl*>(
      navigation_simulator->GetFinalRenderFrameHost());

  mojo::Remote<blink::mojom::PressureService> fenced_frame_pressure_service;
  PressureServiceImpl::Create(
      fenced_frame_rfh,
      fenced_frame_pressure_service.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(fenced_frame_pressure_service.is_bound());

  auto fenced_frame_sync_service = std::make_unique<PressureServiceImplSync>(
      fenced_frame_pressure_service.get());
  FakePressureObserver observer;
  EXPECT_EQ(fenced_frame_sync_service->BindObserver(
                observer.BindNewPipeAndPassRemote()),
            blink::mojom::PressureStatus::kNotSupported);
}

TEST_F(PressureServiceImplTest, BindObserver_NotSupported) {
  pressure_manager_overrider_->set_is_supported(false);

  FakePressureObserver observer;
  EXPECT_EQ(pressure_service_impl_sync_->BindObserver(
                observer.BindNewPipeAndPassRemote()),
            blink::mojom::PressureStatus::kNotSupported);
}

TEST_F(PressureServiceImplTest, InsecureOrigin) {
  NavigateAndCommit(kInsecureUrl);

  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  SetPressureServiceImpl();
  EXPECT_EQ("Compute Pressure access from an insecure origin",
            bad_message_observer.WaitForBadMessage());
}

// Allows callers to run a custom callback before running
// FakePressureManager::AddClient().
class InterceptingFakePressureManager : public device::FakePressureManager {
 public:
  explicit InterceptingFakePressureManager(
      base::OnceClosure interception_callback)
      : interception_callback_(std::move(interception_callback)) {}

  void AddClient(mojo::PendingRemote<device::mojom::PressureClient> client,
                 AddClientCallback callback) override {
    std::move(interception_callback_).Run();
    device::FakePressureManager::AddClient(std::move(client),
                                           std::move(callback));
  }

 private:
  base::OnceClosure interception_callback_;
};

// Test for https://crbug.com/1355662: destroying PressureServiceImplTest
// between calling PressureServiceImpl::BindObserver() and its |remote_|
// invoking the callback it receives does not crash.
TEST_F(PressureServiceImplTest, DestructionOrderWithOngoingCallback) {
  auto intercepting_fake_pressure_manager =
      std::make_unique<InterceptingFakePressureManager>(
          base::BindLambdaForTesting([&]() {
            // Delete the current WebContents and consequently trigger
            // PressureServiceImpl's destruction between calling
            // PressureServiceImpl::BindObserver() and its |remote_|
            // invoking the callback it receives.
            DeleteContents();
          }));
  pressure_manager_overrider_->set_fake_pressure_manager(
      std::move(intercepting_fake_pressure_manager));

  base::RunLoop run_loop;
  pressure_service_.set_disconnect_handler(run_loop.QuitClosure());
  FakePressureObserver observer;
  pressure_service_->BindObserver(
      observer.BindNewPipeAndPassRemote(),
      base::BindOnce([](blink::mojom::PressureStatus) {
        ADD_FAILURE() << "Reached BindObserver callback unexpectedly";
      }));
  run_loop.Run();
}

}  // namespace content
