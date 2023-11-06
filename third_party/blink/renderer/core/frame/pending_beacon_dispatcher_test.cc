// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/frame/pending_beacon_dispatcher.h"

#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/pending_beacon.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/frame/pending_beacon.mojom-blink.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock-matchers.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock-more-matchers.h"

namespace blink {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

}  // namespace

class MockPendingBeacon : public GarbageCollected<MockPendingBeacon>,
                          public PendingBeaconDispatcher::PendingBeacon {
 public:
  using OnSendCallbackType = base::RepeatingCallback<void(int)>;

  MockPendingBeacon(ExecutionContext* ec,
                    int id,
                    base::TimeDelta background_timeout,
                    OnSendCallbackType on_send)
      : ec_(ec),
        remote_(ec),
        id_(id),
        background_timeout_(background_timeout),
        on_send_(on_send) {
    auto task_runner = ec->GetTaskRunner(PendingBeaconDispatcher::kTaskType);
    mojo::PendingReceiver<mojom::blink::PendingBeacon> receiver =
        remote_.BindNewPipeAndPassReceiver(task_runner);

    auto& dispatcher = PendingBeaconDispatcher::FromOrAttachTo(*ec);
    dispatcher.CreateHostBeacon(this, std::move(receiver), url_, method_);
  }

  MockPendingBeacon(ExecutionContext* ec, int id, OnSendCallbackType on_send)
      : MockPendingBeacon(ec, id, base::Milliseconds(-1), on_send) {}

  // Not copyable or movable
  MockPendingBeacon(const MockPendingBeacon&) = delete;
  MockPendingBeacon& operator=(const MockPendingBeacon&) = delete;
  virtual ~MockPendingBeacon() = default;

  void Trace(Visitor* visitor) const override {
    visitor->Trace(ec_);
    visitor->Trace(remote_);
  }

  // PendingBeaconDispatcher::Beacon Implementation.
  base::TimeDelta GetBackgroundTimeout() const override {
    return background_timeout_;
  }
  void Send() override {
    on_send_.Run(id_);
    PendingBeaconDispatcher::From(*ec_)->Unregister(this);
  }
  ExecutionContext* GetExecutionContext() override { return ec_.Get(); }
  bool IsPending() const override { return is_pending_; }
  void MarkNotPending() override { is_pending_ = false; }

 private:
  const KURL url_ = KURL("/");
  const mojom::blink::BeaconMethod method_ = mojom::blink::BeaconMethod::kGet;
  Member<ExecutionContext> ec_;
  HeapMojoRemote<mojom::blink::PendingBeacon> remote_;
  const int id_;
  const base::TimeDelta background_timeout_;
  base::RepeatingCallback<void(int)> on_send_;
  bool is_pending_ = true;
};

class PendingBeaconDispatcherTestBase : public ::testing::Test {
 protected:
  using IdToTimeouts = std::vector<std::pair<int, base::TimeDelta>>;

  void TriggerDispatchOnBackgroundTimeout(V8TestingScope& scope) {
    auto* ec = scope.GetExecutionContext();
    // Ensures that a dispatcher is attached to `ec`.
    PendingBeaconDispatcher::FromOrAttachTo(*ec);
    scope.GetPage().SetVisibilityState(
        blink::mojom::PageVisibilityState::kHidden, /*is_initial_state=*/false);
  }

  HeapVector<Member<MockPendingBeacon>> CreateBeacons(
      V8TestingScope& v8_scope,
      const IdToTimeouts& id_to_timeouts,
      MockPendingBeacon::OnSendCallbackType callback) {
    HeapVector<Member<MockPendingBeacon>> beacons;
    auto* ec = v8_scope.GetExecutionContext();
    for (const auto& id_to_timeout : id_to_timeouts) {
      beacons.push_back(MakeGarbageCollected<MockPendingBeacon>(
          ec, id_to_timeout.first, id_to_timeout.second, callback));
    }
    return beacons;
  }
};

struct BeaconIdToTimeoutsTestType {
  std::string test_case_name;
  std::vector<std::pair<int, base::TimeDelta>> id_to_timeouts;
  std::vector<int> expected;
};

// Tests to cover the basic sending order of beacons on backgroundTimeout or
// on timeout.
// Note that the beacons in the same test falls into different bundles such that
// the resulting order is deterministic.
class PendingBeaconDispatcherBasicBeaconsTest
    : public PendingBeaconDispatcherTestBase,
      public ::testing::WithParamInterface<BeaconIdToTimeoutsTestType> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    PendingBeaconDispatcherBasicBeaconsTest,
    testing::ValuesIn<std::vector<BeaconIdToTimeoutsTestType>>({
        {"OneBeacon", {{1, base::Milliseconds(0)}}, {1}},
        {"OrderedBeacons",
         {
             {1, base::Milliseconds(0)},
             {2, base::Milliseconds(100)},
             {3, base::Milliseconds(200)},
             {4, base::Milliseconds(300)},
             {5, base::Milliseconds(400)},
         },
         {1, 2, 3, 4, 5}},
        {"ReversedBeacons",
         {
             {1, base::Milliseconds(400)},
             {2, base::Milliseconds(300)},
             {3, base::Milliseconds(200)},
             {4, base::Milliseconds(100)},
             {5, base::Milliseconds(0)},
         },
         {5, 4, 3, 2, 1}},
        {"RandomOrderedBeacons",
         {
             {1, base::Milliseconds(300)},
             {2, base::Milliseconds(100)},
             {3, base::Milliseconds(0)},
             {4, base::Milliseconds(500)},
             {5, base::Milliseconds(200)},
         },
         {3, 2, 5, 1, 4}},

    }),
    [](const testing::TestParamInfo<BeaconIdToTimeoutsTestType>& info) {
      return info.param.test_case_name;
    });

TEST_P(PendingBeaconDispatcherBasicBeaconsTest,
       DispatchBeaconsOnBackgroundTimeout) {
  const auto& id_to_timeouts = GetParam().id_to_timeouts;
  std::vector<int> beacons_sent_order;

  V8TestingScope scope;
  auto beacons =
      CreateBeacons(scope, id_to_timeouts,
                    base::BindLambdaForTesting([&beacons_sent_order](int id) {
                      beacons_sent_order.push_back(id);
                    }));

  TriggerDispatchOnBackgroundTimeout(scope);
  while (beacons_sent_order.size() < id_to_timeouts.size()) {
    test::RunPendingTasks();
  }

  EXPECT_THAT(beacons_sent_order, testing::ContainerEq(GetParam().expected));
  for (const auto& beacon : beacons) {
    EXPECT_FALSE(PendingBeaconDispatcher::From(*scope.GetExecutionContext())
                     ->HasPendingBeaconForTesting(beacon));
  }
}

// Tests to cover the beacon bundling behavior on backgroundTimeout.
using PendingBeaconDispatcherBackgroundTimeoutBundledTest =
    PendingBeaconDispatcherTestBase;

TEST_F(PendingBeaconDispatcherBackgroundTimeoutBundledTest,
       DispatchOrderedBeacons) {
  const std::vector<std::pair<int, base::TimeDelta>> id_to_timeouts = {
      {1, base::Milliseconds(0)},    {2, base::Milliseconds(1)},
      {3, base::Milliseconds(50)},   {4, base::Milliseconds(99)},
      {5, base::Milliseconds(100)},  {6, base::Milliseconds(101)},
      {7, base::Milliseconds(150)},  {8, base::Milliseconds(201)},
      {9, base::Milliseconds(202)},  {10, base::Milliseconds(250)},
      {11, base::Milliseconds(499)}, {12, base::Milliseconds(500)},
  };
  std::vector<int> beacons_sent_order;

  V8TestingScope scope;
  auto beacons =
      CreateBeacons(scope, id_to_timeouts,
                    base::BindLambdaForTesting([&beacons_sent_order](int id) {
                      beacons_sent_order.push_back(id);
                    }));

  TriggerDispatchOnBackgroundTimeout(scope);
  while (beacons_sent_order.size() < id_to_timeouts.size()) {
    test::RunPendingTasks();
  }

  // Bundle 1: {0, 1, 50, 99}
  EXPECT_THAT(std::vector<int>(beacons_sent_order.begin(),
                               beacons_sent_order.begin() + 4),
              UnorderedElementsAre(1, 2, 3, 4));
  // Bundle 2: {100, 101, 150}
  EXPECT_THAT(std::vector<int>(beacons_sent_order.begin() + 4,
                               beacons_sent_order.begin() + 7),
              UnorderedElementsAre(5, 6, 7));
  // Bundle 3: {201, 202, 250}
  EXPECT_THAT(std::vector<int>(beacons_sent_order.begin() + 7,
                               beacons_sent_order.begin() + 10),
              UnorderedElementsAre(8, 9, 10));
  // Bundle 4: {499, 500}
  EXPECT_THAT(std::vector<int>(beacons_sent_order.begin() + 10,
                               beacons_sent_order.begin() + 12),
              UnorderedElementsAre(11, 12));
  for (const auto& beacon : beacons) {
    EXPECT_FALSE(PendingBeaconDispatcher::From(*scope.GetExecutionContext())
                     ->HasPendingBeaconForTesting(beacon));
  }
}

TEST_F(PendingBeaconDispatcherBackgroundTimeoutBundledTest,
       DispatchReversedBeacons) {
  const std::vector<std::pair<int, base::TimeDelta>> id_to_timeouts = {
      {1, base::Milliseconds(500)}, {2, base::Milliseconds(499)},
      {3, base::Milliseconds(250)}, {4, base::Milliseconds(202)},
      {5, base::Milliseconds(201)}, {6, base::Milliseconds(150)},
      {7, base::Milliseconds(101)}, {8, base::Milliseconds(100)},
      {9, base::Milliseconds(99)},  {10, base::Milliseconds(50)},
      {11, base::Milliseconds(1)},  {12, base::Milliseconds(0)},
  };
  std::vector<int> beacons_sent_order;

  V8TestingScope scope;
  auto beacons =
      CreateBeacons(scope, id_to_timeouts,
                    base::BindLambdaForTesting([&beacons_sent_order](int id) {
                      beacons_sent_order.push_back(id);
                    }));

  TriggerDispatchOnBackgroundTimeout(scope);
  while (beacons_sent_order.size() < id_to_timeouts.size()) {
    test::RunPendingTasks();
  }

  // Bundle 1: {0, 1, 50, 99}
  EXPECT_THAT(std::vector<int>(beacons_sent_order.begin(),
                               beacons_sent_order.begin() + 4),
              UnorderedElementsAre(9, 10, 11, 12));
  // Bundle 2: {100, 101, 150}
  EXPECT_THAT(std::vector<int>(beacons_sent_order.begin() + 4,
                               beacons_sent_order.begin() + 7),
              UnorderedElementsAre(6, 7, 8));
  // Bundle 3: {201, 202, 250}
  EXPECT_THAT(std::vector<int>(beacons_sent_order.begin() + 7,
                               beacons_sent_order.begin() + 10),
              UnorderedElementsAre(3, 4, 5));
  // Bundle 4: {499, 500}
  EXPECT_THAT(std::vector<int>(beacons_sent_order.begin() + 10,
                               beacons_sent_order.begin() + 12),
              UnorderedElementsAre(1, 2));
  for (const auto& beacon : beacons) {
    EXPECT_FALSE(PendingBeaconDispatcher::From(*scope.GetExecutionContext())
                     ->HasPendingBeaconForTesting(beacon));
  }
}

TEST_F(PendingBeaconDispatcherBackgroundTimeoutBundledTest,
       DispatchDuplicatedBeacons) {
  const std::vector<std::pair<int, base::TimeDelta>> id_to_timeouts = {
      {1, base::Milliseconds(0)},   {2, base::Milliseconds(0)},
      {3, base::Milliseconds(100)}, {4, base::Milliseconds(100)},
      {5, base::Milliseconds(100)}, {6, base::Milliseconds(101)},
      {7, base::Milliseconds(101)},
  };
  std::vector<int> beacons_sent_order;

  V8TestingScope scope;
  auto beacons =
      CreateBeacons(scope, id_to_timeouts,
                    base::BindLambdaForTesting([&beacons_sent_order](int id) {
                      beacons_sent_order.push_back(id);
                    }));

  TriggerDispatchOnBackgroundTimeout(scope);
  while (beacons_sent_order.size() < id_to_timeouts.size()) {
    test::RunPendingTasks();
  }

  // Bundle 1: {0, 0}
  EXPECT_THAT(std::vector<int>(beacons_sent_order.begin(),
                               beacons_sent_order.begin() + 2),
              UnorderedElementsAre(1, 2));
  // Bundle 2: {100, 100, 100, 101, 101}
  EXPECT_THAT(std::vector<int>(beacons_sent_order.begin() + 2,
                               beacons_sent_order.begin() + 7),
              UnorderedElementsAre(3, 4, 5, 6, 7));
  for (const auto& beacon : beacons) {
    EXPECT_FALSE(PendingBeaconDispatcher::From(*scope.GetExecutionContext())
                     ->HasPendingBeaconForTesting(beacon));
  }
}

class PendingBeaconDispatcherOnPagehideTest
    : public PendingBeaconDispatcherTestBase {
  void SetUp() override {
    const std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {blink::features::kPendingBeaconAPI, {{"send_on_navigation", "true"}}}};
    feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
    PendingBeaconDispatcherTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PendingBeaconDispatcherOnPagehideTest,
       OnPagehideUpdateAndUnregisterAllBeacons) {
  const std::vector<std::pair<int, base::TimeDelta>> id_to_timeouts = {
      {1, base::Milliseconds(0)},   {2, base::Milliseconds(0)},
      {3, base::Milliseconds(100)}, {4, base::Milliseconds(100)},
      {5, base::Milliseconds(100)}, {6, base::Milliseconds(101)},
      {7, base::Milliseconds(101)},
  };
  std::vector<int> beacons_sent_order;

  V8TestingScope scope;
  auto beacons =
      CreateBeacons(scope, id_to_timeouts,
                    base::BindLambdaForTesting([&beacons_sent_order](int id) {
                      beacons_sent_order.push_back(id);
                    }));
  for (const auto& beacon : beacons) {
    EXPECT_TRUE(beacon->IsPending());
  }

  PendingBeaconDispatcher::From(*scope.GetExecutionContext())
      ->OnDispatchPagehide();
  test::RunPendingTasks();

  // On page hide, all beacons should be marked as non-pending. However, none
  // should be sent directly by the renderer; the browser is responsible for
  // this.
  EXPECT_THAT(beacons_sent_order, IsEmpty());
  for (const auto& beacon : beacons) {
    EXPECT_FALSE(beacon->IsPending());
    EXPECT_FALSE(PendingBeaconDispatcher::From(*scope.GetExecutionContext())
                     ->HasPendingBeaconForTesting(beacon));
  }
}

}  // namespace blink
