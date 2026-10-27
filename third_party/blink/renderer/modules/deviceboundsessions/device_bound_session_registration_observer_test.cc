// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/deviceboundsessions/device_bound_session_registration_observer.h"

#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/restricted_device_bound_sessions.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_device_bound_session.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_device_bound_session_registration_observer_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_device_bound_session_registration_observer_options.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/deviceboundsessions/device_bound_session_manager_supplement.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

using ::network::mojom::blink::RestrictedDeviceBoundSessionManager;
using ::network::mojom::blink::RestrictedDeviceBoundSessionObserver;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

constexpr std::string_view kHistogram =
    "Blink.DeviceBoundSession.AbortedStatus";
constexpr int kCompletedBucket = 0;
constexpr int kAbortedBucket = 1;

network::mojom::blink::RestrictedDeviceBoundSessionPtr MakeSession(
    const String& session_id) {
  return network::mojom::blink::RestrictedDeviceBoundSession::New(
      session_id, KURL("https://example.test/refresh"),
      base::Time::UnixEpoch() + base::Seconds(1));
}

Vector<network::mojom::blink::RestrictedDeviceBoundSessionPtr> MakeSessions(
    base::span<const String> session_ids) {
  Vector<network::mojom::blink::RestrictedDeviceBoundSessionPtr> sessions;
  for (const String& session_id : session_ids) {
    sessions.push_back(MakeSession(session_id));
  }
  return sessions;
}

// Records the session ids passed to each invocation of the JS callback.
class RecordingCallback final : public ScriptFunction {
 public:
  RecordingCallback() = default;
  RecordingCallback(const RecordingCallback&) = delete;
  RecordingCallback& operator=(const RecordingCallback&) = delete;

  ScriptValue Call(ScriptState* script_state, ScriptValue value) override {
    NonThrowableExceptionState exception_state;
    HeapVector<Member<DeviceBoundSession>> sessions =
        NativeValueTraits<IDLSequence<DeviceBoundSession>>::NativeValue(
            script_state->GetIsolate(), value.V8Value(), exception_state);
    Vector<String> ids;
    // `MakeSession()` gives every session this refresh URL and creation time.
    for (const Member<DeviceBoundSession>& session : sessions) {
      EXPECT_EQ(session->refreshURL(), "https://example.test/refresh");
      EXPECT_EQ(session->creationTime(), 1000.0);
      ids.push_back(session->sessionId());
    }
    calls_.push_back(std::move(ids));
    return ScriptValue();
  }

  const Vector<Vector<String>>& Calls() const { return calls_; }

 private:
  Vector<Vector<String>> calls_;
};

// Stands in for the Network Service manager. Each `SubscribeToRegistrations()`
// call is kept so a test decides when pushes and the reply are sent.
class FakeManager : public RestrictedDeviceBoundSessionManager {
 public:
  FakeManager() = default;
  FakeManager(const FakeManager&) = delete;
  FakeManager& operator=(const FakeManager&) = delete;

  // Tests leave some replies unrun. Mojo allows dropping them only after the
  // pipe they belong to is closed.
  ~FakeManager() override { receivers_.Clear(); }

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    ++bind_count_;
    receivers_.Add(this,
                   mojo::PendingReceiver<RestrictedDeviceBoundSessionManager>(
                       std::move(handle)));
  }

  // RestrictedDeviceBoundSessionManager:
  void SubscribeToRegistrations(
      mojo::PendingAssociatedRemote<RestrictedDeviceBoundSessionObserver>
          observer,
      SubscribeToRegistrationsCallback callback) override {
    observers_.emplace_back(std::move(observer));
    replies_.push_back(std::move(callback));
  }

  void Push(wtf_size_t subscription, const String& session_id) {
    observers_[subscription]->OnDeviceBoundSessionRegistered(
        MakeSession(session_id));
  }

  void Reply(wtf_size_t subscription, base::span<const String> session_ids) {
    std::move(replies_[subscription]).Run(MakeSessions(session_ids));
  }

  // Waits until every message sent before this call on an open pipe has been
  // handled: replies and pushes to Blink, and Blink's calls to this fake.
  void Flush() {
    receivers_.FlushForTesting();
    // By index: a subscription made during a flush appends to `observers_`.
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (wtf_size_t i = 0; i < observers_.size(); ++i) {
      observers_[i].FlushForTesting();
    }
  }

  // Closes every manager pipe, as the network service does when DBSC is off
  // for the profile.
  void CloseManagerPipes() { receivers_.Clear(); }

  // Closes one observer pipe. The network service closes them when the
  // manager goes away.
  void CloseObserverPipe(wtf_size_t subscription) {
    observers_[subscription].reset();
  }

  int BindCount() const { return bind_count_; }
  wtf_size_t SubscriptionCount() const { return replies_.size(); }

 private:
  int bind_count_ = 0;
  mojo::ReceiverSet<RestrictedDeviceBoundSessionManager> receivers_;
  Vector<mojo::AssociatedRemote<RestrictedDeviceBoundSessionObserver>>
      observers_;
  Vector<SubscribeToRegistrationsCallback> replies_;
};

// Routes the window's manager requests to `fake` for the life of the object.
class ScopedFakeManagerBinder {
  STACK_ALLOCATED();

 public:
  explicit ScopedFakeManagerBinder(V8TestingScope& scope, FakeManager& fake)
      : window_(scope.GetWindow()) {
    // `Unretained()` is safe: `fake` outlives this object, and the destructor
    // removes the binder.
    window_.GetBrowserInterfaceBroker().SetBinderForTesting(
        RestrictedDeviceBoundSessionManager::Name_,
        BindRepeating(&FakeManager::Bind, Unretained(&fake)));
  }

  ScopedFakeManagerBinder(const ScopedFakeManagerBinder&) = delete;
  ScopedFakeManagerBinder& operator=(const ScopedFakeManagerBinder&) = delete;

  ~ScopedFakeManagerBinder() {
    window_.GetBrowserInterfaceBroker().SetBinderForTesting(
        RestrictedDeviceBoundSessionManager::Name_, {});
  }

 private:
  LocalDOMWindow& window_;
};

// True while the window's supplement holds `observer` for a live
// subscription, which keeps an observing observer alive without a script
// reference.
bool IsKeptAlive(V8TestingScope& scope,
                 DeviceBoundSessionRegistrationObserver* observer) {
  return DeviceBoundSessionManagerSupplement::From(scope.GetWindow())
      .HasObserverForTesting(observer);
}

class DeviceBoundSessionRegistrationObserverTest : public testing::Test {
 protected:
  DeviceBoundSessionRegistrationObserver* CreateObserver(
      V8TestingScope& scope,
      RecordingCallback* recorder,
      AbortSignal* signal = nullptr) {
    scope.GetWindow().SetCanUseDeviceBoundSessionJsApi(can_use_js_api_);
    auto* options = DeviceBoundSessionRegistrationObserverOptions::Create();
    if (signal) {
      options->setSignal(signal);
    }
    auto* callback = V8DeviceBoundSessionRegistrationObserverCallback::Create(
        recorder->ToV8Function(scope.GetScriptState()));
    return DeviceBoundSessionRegistrationObserver::Create(
        scope.GetScriptState(), callback, options, scope.GetExceptionState());
  }

  test::TaskEnvironment task_environment_;
  FakeManager fake_;
  // The commit-time value `CreateObserver()` gives the window.
  bool can_use_js_api_ = true;
};

TEST_F(DeviceBoundSessionRegistrationObserverTest, CreateAndDisconnect) {
  V8TestingScope scope(KURL("https://example.test"));
  ScopedFakeManagerBinder binder(scope, fake_);
  auto* recorder = MakeGarbageCollected<RecordingCallback>();

  DeviceBoundSessionRegistrationObserver* observer =
      CreateObserver(scope, recorder);
  ASSERT_TRUE(observer);
  EXPECT_FALSE(IsKeptAlive(scope, observer));

  observer->observe();
  observer->observe();
  fake_.Flush();
  EXPECT_EQ(fake_.SubscriptionCount(), 1u);
  EXPECT_TRUE(IsKeptAlive(scope, observer));

  observer->disconnect();
  EXPECT_FALSE(IsKeptAlive(scope, observer));
}

TEST_F(DeviceBoundSessionRegistrationObserverTest,
       KeptAliveOnlyWhileObserving) {
  V8TestingScope scope(KURL("https://example.test"));
  ScopedFakeManagerBinder binder(scope, fake_);
  auto* recorder = MakeGarbageCollected<RecordingCallback>();
  DeviceBoundSessionRegistrationObserver* observer =
      CreateObserver(scope, recorder);
  ASSERT_TRUE(observer);
  EXPECT_FALSE(IsKeptAlive(scope, observer));

  observer->observe();
  fake_.Flush();
  ASSERT_EQ(fake_.SubscriptionCount(), 1u);
  EXPECT_TRUE(IsKeptAlive(scope, observer));

  // Still observing after the snapshot reply.
  fake_.Reply(0, {"existing"});
  fake_.Flush();
  EXPECT_TRUE(IsKeptAlive(scope, observer));
  EXPECT_THAT(recorder->Calls(), ElementsAre(ElementsAre("existing")));

  observer->disconnect();
  EXPECT_FALSE(IsKeptAlive(scope, observer));
}

TEST_F(DeviceBoundSessionRegistrationObserverTest,
       AbortStopsObservingWithoutCallback) {
  base::HistogramTester histograms;
  V8TestingScope scope(KURL("https://example.test"));
  ScopedFakeManagerBinder binder(scope, fake_);
  auto* recorder = MakeGarbageCollected<RecordingCallback>();
  auto* controller = AbortController::Create(scope.GetScriptState());
  DeviceBoundSessionRegistrationObserver* observer =
      CreateObserver(scope, recorder, controller->signal());
  ASSERT_TRUE(observer);

  observer->observe();
  fake_.Flush();
  ASSERT_EQ(fake_.SubscriptionCount(), 1u);

  controller->abort(scope.GetScriptState());
  EXPECT_FALSE(IsKeptAlive(scope, observer));
  histograms.ExpectUniqueSample(kHistogram, kAbortedBucket, 1);

  fake_.Push(0, "after_abort");
  fake_.Reply(0, {"existing"});
  fake_.Flush();
  EXPECT_THAT(recorder->Calls(), IsEmpty());

  // An aborted observer stays stopped, and a later `disconnect()` is not
  // counted as a completion.
  observer->observe();
  fake_.Flush();
  EXPECT_EQ(fake_.SubscriptionCount(), 1u);
  observer->disconnect();
  histograms.ExpectUniqueSample(kHistogram, kAbortedBucket, 1);
}

TEST_F(DeviceBoundSessionRegistrationObserverTest,
       ObserversShareOneManagerRemote) {
  V8TestingScope scope(KURL("https://example.test"));
  ScopedFakeManagerBinder binder(scope, fake_);
  auto* recorder = MakeGarbageCollected<RecordingCallback>();
  HeapVector<Member<DeviceBoundSessionRegistrationObserver>> observers;
  for (int i = 0; i < 3; ++i) {
    DeviceBoundSessionRegistrationObserver* observer =
        CreateObserver(scope, recorder);
    ASSERT_TRUE(observer);
    observer->observe();
    observers.push_back(observer);
  }
  fake_.Flush();

  EXPECT_EQ(fake_.BindCount(), 1);
  EXPECT_EQ(fake_.SubscriptionCount(), 3u);
}

TEST_F(DeviceBoundSessionRegistrationObserverTest,
       AlreadyAbortedSignalStartsDisconnected) {
  base::HistogramTester histograms;
  V8TestingScope scope(KURL("https://example.test"));
  ScopedFakeManagerBinder binder(scope, fake_);
  auto* recorder = MakeGarbageCollected<RecordingCallback>();
  auto* controller = AbortController::Create(scope.GetScriptState());
  controller->abort(scope.GetScriptState());

  DeviceBoundSessionRegistrationObserver* observer =
      CreateObserver(scope, recorder, controller->signal());
  ASSERT_TRUE(observer);
  histograms.ExpectUniqueSample(kHistogram, kAbortedBucket, 1);
  EXPECT_FALSE(IsKeptAlive(scope, observer));

  observer->observe();
  fake_.Flush();
  EXPECT_FALSE(IsKeptAlive(scope, observer));
  EXPECT_EQ(fake_.SubscriptionCount(), 0u);
  EXPECT_THAT(recorder->Calls(), IsEmpty());
}

TEST_F(DeviceBoundSessionRegistrationObserverTest,
       SnapshotDroppedAfterDisconnect) {
  V8TestingScope scope(KURL("https://example.test"));
  ScopedFakeManagerBinder binder(scope, fake_);
  auto* recorder = MakeGarbageCollected<RecordingCallback>();
  DeviceBoundSessionRegistrationObserver* observer =
      CreateObserver(scope, recorder);
  ASSERT_TRUE(observer);

  observer->observe();
  fake_.Flush();
  ASSERT_EQ(fake_.SubscriptionCount(), 1u);
  observer->disconnect();

  fake_.Reply(0, {"existing"});
  fake_.Flush();
  EXPECT_THAT(recorder->Calls(), IsEmpty());
}

TEST_F(DeviceBoundSessionRegistrationObserverTest,
       StaleSnapshotAfterReobserveIsDropped) {
  V8TestingScope scope(KURL("https://example.test"));
  ScopedFakeManagerBinder binder(scope, fake_);
  auto* recorder = MakeGarbageCollected<RecordingCallback>();
  DeviceBoundSessionRegistrationObserver* observer =
      CreateObserver(scope, recorder);
  ASSERT_TRUE(observer);

  observer->observe();
  fake_.Flush();
  observer->disconnect();
  observer->observe();
  fake_.Flush();
  ASSERT_EQ(fake_.SubscriptionCount(), 2u);

  // The first reply arrives while the second subscription is active. It
  // describes the earlier subscription, so it must not count as the snapshot.
  fake_.Reply(0, {"stale"});
  fake_.Flush();
  EXPECT_THAT(recorder->Calls(), IsEmpty());

  fake_.Push(1, "pushed");
  fake_.Flush();
  EXPECT_THAT(recorder->Calls(), IsEmpty());

  fake_.Reply(1, {"current"});
  fake_.Flush();
  EXPECT_THAT(recorder->Calls(), ElementsAre(ElementsAre("current", "pushed")));
}

TEST_F(DeviceBoundSessionRegistrationObserverTest,
       ReobserveAfterSnapshotHoldsPushesAgain) {
  V8TestingScope scope(KURL("https://example.test"));
  ScopedFakeManagerBinder binder(scope, fake_);
  auto* recorder = MakeGarbageCollected<RecordingCallback>();
  DeviceBoundSessionRegistrationObserver* observer =
      CreateObserver(scope, recorder);
  ASSERT_TRUE(observer);

  observer->observe();
  fake_.Flush();
  fake_.Reply(0, {"first"});
  fake_.Flush();
  observer->disconnect();

  observer->observe();
  fake_.Flush();
  ASSERT_EQ(fake_.SubscriptionCount(), 2u);
  fake_.Push(1, "pushed");
  fake_.Flush();
  fake_.Reply(1, {"second"});
  fake_.Flush();
  EXPECT_THAT(recorder->Calls(), ElementsAre(ElementsAre("first"),
                                             ElementsAre("second", "pushed")));
}

TEST_F(DeviceBoundSessionRegistrationObserverTest,
       OpaqueOriginThrowsSecurityError) {
  V8TestingScope scope(KURL("https://example.test"));
  ScopedFakeManagerBinder binder(scope, fake_);
  scope.GetWindow().GetSecurityContext().SetSecurityOriginForTesting(
      scope.GetWindow().GetSecurityOrigin()->DeriveNewOpaqueOrigin());
  ASSERT_TRUE(scope.GetWindow().GetSecurityOrigin()->IsOpaque());

  auto* recorder = MakeGarbageCollected<RecordingCallback>();
  auto* callback = V8DeviceBoundSessionRegistrationObserverCallback::Create(
      recorder->ToV8Function(scope.GetScriptState()));
  DummyExceptionStateForTesting exception_state;
  DeviceBoundSessionRegistrationObserver* observer =
      DeviceBoundSessionRegistrationObserver::Create(
          scope.GetScriptState(), callback,
          DeviceBoundSessionRegistrationObserverOptions::Create(),
          exception_state);

  EXPECT_FALSE(observer);
  ASSERT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kSecurityError);
  EXPECT_EQ(fake_.BindCount(), 0);
}

// Script can still call the constructor through the window of a removed
// iframe, after that window's context is destroyed.
TEST_F(DeviceBoundSessionRegistrationObserverTest,
       DestroyedContextThrowsInvalidStateError) {
  V8TestingScope scope(KURL("https://example.test"));
  auto* recorder = MakeGarbageCollected<RecordingCallback>();
  auto* callback = V8DeviceBoundSessionRegistrationObserverCallback::Create(
      recorder->ToV8Function(scope.GetScriptState()));
  scope.GetExecutionContext()->NotifyContextDestroyed();

  DummyExceptionStateForTesting exception_state;
  DeviceBoundSessionRegistrationObserver* observer =
      DeviceBoundSessionRegistrationObserver::Create(
          scope.GetScriptState(), callback,
          DeviceBoundSessionRegistrationObserverOptions::Create(),
          exception_state);

  EXPECT_FALSE(observer);
  ASSERT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

TEST_F(DeviceBoundSessionRegistrationObserverTest,
       PushBeforeSnapshotIsHeldUntilSnapshot) {
  V8TestingScope scope(KURL("https://example.test"));
  ScopedFakeManagerBinder binder(scope, fake_);
  auto* recorder = MakeGarbageCollected<RecordingCallback>();
  DeviceBoundSessionRegistrationObserver* observer =
      CreateObserver(scope, recorder);
  ASSERT_TRUE(observer);

  observer->observe();
  fake_.Flush();
  ASSERT_EQ(fake_.SubscriptionCount(), 1u);

  fake_.Push(0, "pushed");
  fake_.Flush();
  EXPECT_THAT(recorder->Calls(), IsEmpty());

  fake_.Reply(0, {"existing"});
  fake_.Flush();
  EXPECT_THAT(recorder->Calls(),
              ElementsAre(ElementsAre("existing", "pushed")));

  // Pushes after the reply go straight to the callback.
  fake_.Push(0, "later");
  fake_.Flush();
  EXPECT_THAT(recorder->Calls(), ElementsAre(ElementsAre("existing", "pushed"),
                                             ElementsAre("later")));
}

TEST_F(DeviceBoundSessionRegistrationObserverTest,
       PushAlsoInSnapshotIsDeliveredOnce) {
  V8TestingScope scope(KURL("https://example.test"));
  ScopedFakeManagerBinder binder(scope, fake_);
  auto* recorder = MakeGarbageCollected<RecordingCallback>();
  DeviceBoundSessionRegistrationObserver* observer =
      CreateObserver(scope, recorder);
  ASSERT_TRUE(observer);

  observer->observe();
  fake_.Flush();
  ASSERT_EQ(fake_.SubscriptionCount(), 1u);

  fake_.Push(0, "same");
  fake_.Flush();
  fake_.Reply(0, {"same"});
  fake_.Flush();
  EXPECT_THAT(recorder->Calls(), ElementsAre(ElementsAre("same")));
}

TEST_F(DeviceBoundSessionRegistrationObserverTest,
       PushSentBeforeReplyArrivesFirst) {
  V8TestingScope scope(KURL("https://example.test"));
  ScopedFakeManagerBinder binder(scope, fake_);
  auto* recorder = MakeGarbageCollected<RecordingCallback>();
  DeviceBoundSessionRegistrationObserver* observer =
      CreateObserver(scope, recorder);
  ASSERT_TRUE(observer);

  observer->observe();
  fake_.Flush();
  ASSERT_EQ(fake_.SubscriptionCount(), 1u);

  // The push and the reply share the manager pipe, so the push is
  // dispatched first even with no flush between them, and the copy in the
  // reply is dropped.
  fake_.Push(0, "same");
  fake_.Reply(0, {"same"});
  fake_.Flush();
  EXPECT_THAT(recorder->Calls(), ElementsAre(ElementsAre("same")));
}

TEST_F(DeviceBoundSessionRegistrationObserverTest, EmptySnapshotSkipsCallback) {
  V8TestingScope scope(KURL("https://example.test"));
  ScopedFakeManagerBinder binder(scope, fake_);
  auto* recorder = MakeGarbageCollected<RecordingCallback>();
  DeviceBoundSessionRegistrationObserver* observer =
      CreateObserver(scope, recorder);
  ASSERT_TRUE(observer);

  observer->observe();
  fake_.Flush();
  ASSERT_EQ(fake_.SubscriptionCount(), 1u);

  // An empty reply calls nothing, and the observer keeps observing.
  fake_.Reply(0, {});
  fake_.Flush();
  EXPECT_THAT(recorder->Calls(), IsEmpty());
  EXPECT_TRUE(IsKeptAlive(scope, observer));

  fake_.Push(0, "later");
  fake_.Flush();
  EXPECT_THAT(recorder->Calls(), ElementsAre(ElementsAre("later")));
}

TEST_F(DeviceBoundSessionRegistrationObserverTest, DisconnectClearsHeldPushes) {
  V8TestingScope scope(KURL("https://example.test"));
  ScopedFakeManagerBinder binder(scope, fake_);
  auto* recorder = MakeGarbageCollected<RecordingCallback>();
  DeviceBoundSessionRegistrationObserver* observer =
      CreateObserver(scope, recorder);
  ASSERT_TRUE(observer);

  observer->observe();
  fake_.Flush();
  ASSERT_EQ(fake_.SubscriptionCount(), 1u);

  fake_.Push(0, "held");
  fake_.Flush();
  observer->disconnect();
  fake_.Reply(0, {});
  fake_.Flush();
  EXPECT_THAT(recorder->Calls(), IsEmpty());

  // The held push must not reach the reply to the next subscription.
  observer->observe();
  fake_.Flush();
  ASSERT_EQ(fake_.SubscriptionCount(), 2u);
  fake_.Reply(1, {"current"});
  fake_.Flush();
  EXPECT_THAT(recorder->Calls(), ElementsAre(ElementsAre("current")));
}

TEST_F(DeviceBoundSessionRegistrationObserverTest, DisconnectRecordsCompleted) {
  base::HistogramTester histograms;
  V8TestingScope scope(KURL("https://example.test"));
  ScopedFakeManagerBinder binder(scope, fake_);
  auto* recorder = MakeGarbageCollected<RecordingCallback>();
  DeviceBoundSessionRegistrationObserver* observer =
      CreateObserver(scope, recorder);
  ASSERT_TRUE(observer);

  // Not observing yet, so there is nothing to complete.
  observer->disconnect();
  histograms.ExpectTotalCount(kHistogram, 0);

  observer->observe();
  observer->disconnect();
  histograms.ExpectUniqueSample(kHistogram, kCompletedBucket, 1);

  // A second `disconnect()` ends nothing.
  observer->disconnect();
  histograms.ExpectUniqueSample(kHistogram, kCompletedBucket, 1);
}

TEST_F(DeviceBoundSessionRegistrationObserverTest,
       ManagerPipeCloseStopsObserving) {
  V8TestingScope scope(KURL("https://example.test"));
  ScopedFakeManagerBinder binder(scope, fake_);
  auto* recorder = MakeGarbageCollected<RecordingCallback>();
  DeviceBoundSessionRegistrationObserver* observer =
      CreateObserver(scope, recorder);
  ASSERT_TRUE(observer);

  observer->observe();
  fake_.Flush();
  ASSERT_EQ(fake_.SubscriptionCount(), 1u);
  ASSERT_TRUE(IsKeptAlive(scope, observer));

  fake_.CloseManagerPipes();
  test::RunPendingTasks();
  EXPECT_FALSE(IsKeptAlive(scope, observer));

  // A closed manager pipe is not reused: the next `observe()` binds a new one.
  observer->observe();
  test::RunPendingTasks();
  EXPECT_EQ(fake_.BindCount(), 2);
  EXPECT_EQ(fake_.SubscriptionCount(), 2u);
  EXPECT_TRUE(IsKeptAlive(scope, observer));
}

TEST_F(DeviceBoundSessionRegistrationObserverTest,
       AbortAfterReobserveStopsObserver) {
  base::HistogramTester histograms;
  V8TestingScope scope(KURL("https://example.test"));
  ScopedFakeManagerBinder binder(scope, fake_);
  auto* recorder = MakeGarbageCollected<RecordingCallback>();
  Persistent<AbortController> controller =
      AbortController::Create(scope.GetScriptState());
  Persistent<DeviceBoundSessionRegistrationObserver> observer =
      CreateObserver(scope, recorder, controller->signal());
  ASSERT_TRUE(observer);

  observer->observe();
  fake_.Flush();
  observer->disconnect();

  // `AbortSignal` drops an abort algorithm once a GC collects its handle.
  ThreadState::Current()->CollectAllGarbageForTesting();

  observer->observe();
  fake_.Flush();
  ASSERT_EQ(fake_.SubscriptionCount(), 2u);

  controller->abort(scope.GetScriptState());
  EXPECT_FALSE(IsKeptAlive(scope, observer));
  histograms.ExpectBucketCount(kHistogram, kCompletedBucket, 1);
  histograms.ExpectBucketCount(kHistogram, kAbortedBucket, 1);
}

TEST_F(DeviceBoundSessionRegistrationObserverTest,
       ObserverPipeCloseStopsObserving) {
  V8TestingScope scope(KURL("https://example.test"));
  ScopedFakeManagerBinder binder(scope, fake_);
  auto* recorder = MakeGarbageCollected<RecordingCallback>();
  DeviceBoundSessionRegistrationObserver* observer =
      CreateObserver(scope, recorder);
  ASSERT_TRUE(observer);

  observer->observe();
  fake_.Flush();
  ASSERT_EQ(fake_.SubscriptionCount(), 1u);
  ASSERT_TRUE(IsKeptAlive(scope, observer));

  fake_.CloseObserverPipe(0);
  test::RunPendingTasks();
  EXPECT_FALSE(IsKeptAlive(scope, observer));
}

// Follows `USB::getDevices()`: a document whose commit params said no never
// asks the browser for the manager.
TEST_F(DeviceBoundSessionRegistrationObserverTest,
       IneligibleDocumentNeverBinds) {
  V8TestingScope scope(KURL("https://example.test"));
  ScopedFakeManagerBinder binder(scope, fake_);
  auto* recorder = MakeGarbageCollected<RecordingCallback>();
  can_use_js_api_ = false;
  DeviceBoundSessionRegistrationObserver* observer =
      CreateObserver(scope, recorder);
  ASSERT_TRUE(observer);

  observer->observe();
  fake_.Flush();
  test::RunPendingTasks();

  EXPECT_EQ(fake_.BindCount(), 0);
  EXPECT_EQ(fake_.SubscriptionCount(), 0u);
  EXPECT_FALSE(IsKeptAlive(scope, observer));
  EXPECT_THAT(recorder->Calls(), IsEmpty());
}

// After `observe()`, only the supplement refers to the observer.
TEST_F(DeviceBoundSessionRegistrationObserverTest,
       ObservingObserverStaysAliveWithoutReferences) {
  V8TestingScope scope(KURL("https://example.test"));
  ScopedFakeManagerBinder binder(scope, fake_);
  Persistent<RecordingCallback> recorder =
      MakeGarbageCollected<RecordingCallback>();
  WeakPersistent<DeviceBoundSessionRegistrationObserver> observer =
      CreateObserver(scope, recorder);
  ASSERT_TRUE(observer);
  observer->observe();
  fake_.Flush();
  ASSERT_EQ(fake_.SubscriptionCount(), 1u);
  fake_.Reply(0, {});
  fake_.Flush();

  ThreadState::Current()->CollectAllGarbageForTesting();
  ASSERT_TRUE(observer);

  fake_.Push(0, "later");
  fake_.Flush();
  EXPECT_THAT(recorder->Calls(), ElementsAre(ElementsAre("later")));
}

TEST_F(DeviceBoundSessionRegistrationObserverTest,
       DisconnectedObserverIsCollected) {
  V8TestingScope scope(KURL("https://example.test"));
  ScopedFakeManagerBinder binder(scope, fake_);
  auto* recorder = MakeGarbageCollected<RecordingCallback>();
  WeakPersistent<DeviceBoundSessionRegistrationObserver> observer =
      CreateObserver(scope, recorder);
  ASSERT_TRUE(observer);
  observer->observe();
  fake_.Flush();
  observer->disconnect();

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(observer);
}

TEST_F(DeviceBoundSessionRegistrationObserverTest, AbortedObserverIsCollected) {
  V8TestingScope scope(KURL("https://example.test"));
  ScopedFakeManagerBinder binder(scope, fake_);
  auto* recorder = MakeGarbageCollected<RecordingCallback>();
  Persistent<AbortController> controller =
      AbortController::Create(scope.GetScriptState());
  WeakPersistent<DeviceBoundSessionRegistrationObserver> observer =
      CreateObserver(scope, recorder, controller->signal());
  ASSERT_TRUE(observer);
  observer->observe();
  fake_.Flush();
  controller->abort(scope.GetScriptState());

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(observer);
}

TEST_F(DeviceBoundSessionRegistrationObserverTest,
       ContextDestroyedReleasesObservers) {
  V8TestingScope scope(KURL("https://example.test"));
  ScopedFakeManagerBinder binder(scope, fake_);
  auto* recorder = MakeGarbageCollected<RecordingCallback>();
  WeakPersistent<DeviceBoundSessionRegistrationObserver> observer =
      CreateObserver(scope, recorder);
  ASSERT_TRUE(observer);
  observer->observe();
  fake_.Flush();

  scope.GetExecutionContext()->NotifyContextDestroyed();
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(observer);
}

TEST_F(DeviceBoundSessionRegistrationObserverTest,
       ObserveAfterContextDestroyedDoesNothing) {
  V8TestingScope scope(KURL("https://example.test"));
  ScopedFakeManagerBinder binder(scope, fake_);
  auto* recorder = MakeGarbageCollected<RecordingCallback>();
  DeviceBoundSessionRegistrationObserver* observer =
      CreateObserver(scope, recorder);
  ASSERT_TRUE(observer);

  scope.GetExecutionContext()->NotifyContextDestroyed();
  observer->observe();
  fake_.Flush();

  EXPECT_EQ(fake_.BindCount(), 0);
  EXPECT_FALSE(IsKeptAlive(scope, observer));
}

// The supplement ends a subscription whose endpoint closes without calling
// the observer, so the next `observe()` drops what it held for it.
TEST_F(DeviceBoundSessionRegistrationObserverTest,
       ClosedObserverPipeDropsHeldPushes) {
  V8TestingScope scope(KURL("https://example.test"));
  ScopedFakeManagerBinder binder(scope, fake_);
  auto* recorder = MakeGarbageCollected<RecordingCallback>();
  DeviceBoundSessionRegistrationObserver* observer =
      CreateObserver(scope, recorder);
  ASSERT_TRUE(observer);

  observer->observe();
  fake_.Flush();
  ASSERT_EQ(fake_.SubscriptionCount(), 1u);
  fake_.Push(0, "held");
  fake_.Flush();
  fake_.CloseObserverPipe(0);
  test::RunPendingTasks();
  ASSERT_FALSE(IsKeptAlive(scope, observer));

  // `Flush()` would flush the closed remote too, so run the tasks instead.
  observer->observe();
  test::RunPendingTasks();
  ASSERT_EQ(fake_.SubscriptionCount(), 2u);
  fake_.Reply(1, {"current"});
  test::RunPendingTasks();
  EXPECT_THAT(recorder->Calls(), ElementsAre(ElementsAre("current")));
}

}  // namespace
}  // namespace blink
