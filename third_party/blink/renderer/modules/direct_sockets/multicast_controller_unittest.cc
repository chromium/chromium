// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/multicast_controller.h"

#include "net/base/net_errors.h"
#include "services/network/public/mojom/restricted_udp_socket.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_multicast_group_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/direct_sockets/udp_socket_mojo_remote.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {
namespace {

constexpr char kIPv4MulticastAddress[] = "233.252.0.1";
constexpr char kIPv6MulticastAddress[] = "ff02::1234";
constexpr char kIPv4SourceAddress[] = "192.0.2.1";
constexpr char kIPv4SourceAddress2[] = "192.0.2.2";
constexpr char kIPv6SourceAddress[] = "2001:db8::1";

MulticastGroupOptions* MakeSourceOptions(const char* source_address) {
  auto* options = MulticastGroupOptions::Create();
  options->setSourceAddress(String::FromUTF8(source_address));
  return options;
}

// Helper function to extract string representations from joinedGroups()
std::vector<std::string> ExtractJoinedGroupStrings(
    const HeapVector<Member<V8UnionMulticastMembershipOrString>>& groups) {
  std::vector<std::string> result;
  for (const auto& group : groups) {
    if (group->IsString()) {
      result.push_back(group->GetAsString().Utf8());
    } else if (group->IsMulticastMembership()) {
      const auto* membership = group->GetAsMulticastMembership();
      result.push_back(membership->groupAddress().Utf8() + "@" +
                       membership->sourceAddress().Utf8());
    }
  }
  return result;
}

class FakeRestrictedUDPSocket final
    : public GarbageCollected<FakeRestrictedUDPSocket>,
      public network::mojom::blink::RestrictedUDPSocket {
 public:
  explicit FakeRestrictedUDPSocket() = default;

  void JoinGroup(const net::IPAddress& group_address,
                 const std::optional<net::IPAddress>& source_address,
                 JoinGroupCallback callback) override {
    last_join_group_address_ = group_address;
    last_join_source_address_ = source_address;
    join_group_callback_ = std::move(callback);
  }

  void LeaveGroup(const net::IPAddress& group_address,
                  const std::optional<net::IPAddress>& source_address,
                  LeaveGroupCallback callback) override {
    last_leave_group_address_ = group_address;
    last_leave_source_address_ = source_address;
    leave_group_callback_ = std::move(callback);
  }

  const std::optional<net::IPAddress>& last_join_source_address() const {
    return last_join_source_address_;
  }
  const std::optional<net::IPAddress>& last_leave_source_address() const {
    return last_leave_source_address_;
  }

  void Send(base::span<const uint8_t> data, SendCallback callback) override {
    NOTREACHED();
  }

  void SendTo(base::span<const uint8_t> data,
              const net::HostPortPair& dest_addr,
              net::DnsQueryType dns_query_type,
              SendToCallback callback) override {
    NOTREACHED();
  }

  void ReceiveMore(uint32_t num_additional_datagrams) override { NOTREACHED(); }

  void CompleteJoinGroup(net::Error result) {
    test::RunPendingTasks();
    std::move(join_group_callback_).Run(result);
  }

  void CompleteLeaveGroup(net::Error result) {
    test::RunPendingTasks();
    std::move(leave_group_callback_).Run(result);
  }
  void Trace(cppgc::Visitor* visitor) const {}

 private:
  net::IPAddress last_join_group_address_;
  std::optional<net::IPAddress> last_join_source_address_;
  net::IPAddress last_leave_group_address_;
  std::optional<net::IPAddress> last_leave_source_address_;
  JoinGroupCallback join_group_callback_;
  LeaveGroupCallback leave_group_callback_;
};

class MulticastCreator : public GarbageCollected<MulticastCreator> {
 public:
  explicit MulticastCreator(const V8TestingScope& scope)
      : MulticastCreator(scope,
                         MakeGarbageCollected<FakeRestrictedUDPSocket>()) {}

  MulticastCreator(const V8TestingScope& scope, FakeRestrictedUDPSocket* socket)
      : fake_udp_socket_(socket),
        receiver_{fake_udp_socket_.Get(), scope.GetExecutionContext()} {}

  ~MulticastCreator() = default;

  MulticastController* Create(V8TestingScope& scope) {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        scope.GetExecutionContext()->GetTaskRunner(TaskType::kNetworking);
    auto* udp_socket =
        MakeGarbageCollected<UDPSocketMojoRemote>(scope.GetExecutionContext());

    udp_socket->get().Bind(receiver_.BindNewPipeAndPassRemote(task_runner),
                           task_runner);

    multicast_controller_ = MakeGarbageCollected<MulticastController>(
        scope.GetExecutionContext(), udp_socket, /*inspector_id=*/0);

    return multicast_controller_.Get();
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(fake_udp_socket_);
    visitor->Trace(receiver_);
    visitor->Trace(multicast_controller_);
  }

  FakeRestrictedUDPSocket* fake_udp_socket() { return fake_udp_socket_.Get(); }

  void Cleanup() { receiver_.reset(); }

 private:
  Member<FakeRestrictedUDPSocket> fake_udp_socket_;
  HeapMojoReceiver<network::mojom::blink::RestrictedUDPSocket,
                   FakeRestrictedUDPSocket>
      receiver_;
  Member<MulticastController> multicast_controller_;
};

class ScopedMulticastCreator {
 public:
  explicit ScopedMulticastCreator(MulticastCreator* multicast_creator)
      : multicast_creator_(multicast_creator) {}

  ~ScopedMulticastCreator() { multicast_creator_->Cleanup(); }

  MulticastCreator* operator->() const { return multicast_creator_; }

 private:
  Persistent<MulticastCreator> multicast_creator_;
};

TEST(MulticastControllerTest, JoinAndLeaveGroup) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  ScriptState* script_state = scope.GetScriptState();
  NonThrowableExceptionState exception_state;

  // Join a group (ASM - no source address).
  ScriptPromise<IDLUndefined> join_promise = multicast_controller->joinGroup(
      script_state, kIPv4MulticastAddress, nullptr, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester join_tester(script_state, join_promise);

  creator->fake_udp_socket()->CompleteJoinGroup(net::OK);
  join_tester.WaitUntilSettled();
  ASSERT_TRUE(join_tester.IsFulfilled());
  EXPECT_THAT(ExtractJoinedGroupStrings(multicast_controller->joinedGroups()),
              testing::ElementsAre(kIPv4MulticastAddress));

  // Leave the group (ASM).
  ScriptPromise leave_promise = multicast_controller->leaveGroup(
      script_state, kIPv4MulticastAddress, nullptr, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester leave_tester(script_state, leave_promise);

  creator->fake_udp_socket()->CompleteLeaveGroup(net::OK);
  leave_tester.WaitUntilSettled();
  ASSERT_TRUE(leave_tester.IsFulfilled());
  EXPECT_TRUE(multicast_controller->joinedGroups().empty());
}

TEST(MulticastControllerTest, JoinGroupFailure) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  ScriptState* script_state = scope.GetScriptState();
  NonThrowableExceptionState exception_state;

  ScriptPromise promise = multicast_controller->joinGroup(
      script_state, kIPv4MulticastAddress, nullptr, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester tester(script_state, promise);

  creator->fake_udp_socket()->CompleteJoinGroup(net::ERR_FAILED);
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsRejected());
  EXPECT_TRUE(multicast_controller->joinedGroups().empty());
}

TEST(MulticastControllerTest, LeaveGroupFailure) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  ScriptState* script_state = scope.GetScriptState();
  NonThrowableExceptionState exception_state;

  // First, join a group successfully.
  ScriptPromise join_promise = multicast_controller->joinGroup(
      script_state, kIPv4MulticastAddress, nullptr, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester join_tester(script_state, join_promise);

  creator->fake_udp_socket()->CompleteJoinGroup(net::OK);
  join_tester.WaitUntilSettled();
  ASSERT_TRUE(join_tester.IsFulfilled());

  // Then, fail to leave it.
  ScriptPromise leave_promise = multicast_controller->leaveGroup(
      script_state, kIPv4MulticastAddress, nullptr, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester leave_tester(script_state, leave_promise);

  creator->fake_udp_socket()->CompleteLeaveGroup(net::ERR_FAILED);
  leave_tester.WaitUntilSettled();
  ASSERT_TRUE(leave_tester.IsRejected());
  EXPECT_THAT(ExtractJoinedGroupStrings(multicast_controller->joinedGroups()),
              testing::ElementsAre(kIPv4MulticastAddress));
}

TEST(MulticastControllerTest, JoinInvalidAddress) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  DummyExceptionStateForTesting exception_state;

  multicast_controller->joinGroup(scope.GetScriptState(), "invalid-address",
                                  nullptr, exception_state);

  EXPECT_TRUE(exception_state.HadException());
}

TEST(MulticastControllerTest, JoinGroupTwice) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  NonThrowableExceptionState exception_state;

  // Join a group.
  ScriptPromise join_promise = multicast_controller->joinGroup(
      scope.GetScriptState(), kIPv4MulticastAddress, nullptr, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester join_tester(scope.GetScriptState(), join_promise);
  creator->fake_udp_socket()->CompleteJoinGroup(net::OK);
  join_tester.WaitUntilSettled();
  ASSERT_TRUE(join_tester.IsFulfilled());

  // Try to join again.
  DummyExceptionStateForTesting exception_state2;
  multicast_controller->joinGroup(scope.GetScriptState(), kIPv4MulticastAddress,
                                  nullptr, exception_state2);
  EXPECT_TRUE(exception_state2.HadException());
  EXPECT_EQ(exception_state2.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

TEST(MulticastControllerTest, LeaveUnjoinedGroup) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  DummyExceptionStateForTesting exception_state;
  multicast_controller->leaveGroup(
      scope.GetScriptState(), kIPv4MulticastAddress, nullptr, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

TEST(MulticastControllerTest, OnCloseOrAbort) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  NonThrowableExceptionState exception_state;

  // Join a group.
  ScriptPromise join_promise = multicast_controller->joinGroup(
      scope.GetScriptState(), kIPv4MulticastAddress, nullptr, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester join_tester(scope.GetScriptState(), join_promise);
  creator->fake_udp_socket()->CompleteJoinGroup(net::OK);
  join_tester.WaitUntilSettled();
  ASSERT_TRUE(join_tester.IsFulfilled());
  EXPECT_FALSE(multicast_controller->joinedGroups().empty());

  multicast_controller->OnCloseOrAbort();

  EXPECT_TRUE(multicast_controller->joinedGroups().empty());

  // Cannot join after close.
  DummyExceptionStateForTesting exception_state2;
  multicast_controller->joinGroup(scope.GetScriptState(), kIPv6MulticastAddress,
                                  nullptr, exception_state2);
  EXPECT_TRUE(exception_state2.HadException());
  EXPECT_EQ(exception_state2.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

TEST(MulticastControllerTest, OnCloseOrAbortRejectsPendingPromises) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  NonThrowableExceptionState exception_state;

  // Start a join but do NOT complete it - the promise remains pending.
  ScriptPromise<IDLUndefined> join_promise = multicast_controller->joinGroup(
      scope.GetScriptState(), kIPv4MulticastAddress, nullptr, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester join_tester(scope.GetScriptState(), join_promise);

  EXPECT_TRUE(multicast_controller->HasPendingActivity());

  // Close the socket while the join is still pending.
  multicast_controller->OnCloseOrAbort();

  // The pending promise should be rejected (not left unsettled).
  join_tester.WaitUntilSettled();
  EXPECT_TRUE(join_tester.IsRejected());
  EXPECT_FALSE(multicast_controller->HasPendingActivity());
}

TEST(MulticastControllerTest, HasPendingActivity) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  ScriptState* script_state = scope.GetScriptState();
  NonThrowableExceptionState exception_state;

  // Initially, there should be no pending activity.
  EXPECT_FALSE(multicast_controller->HasPendingActivity());

  // When a joinGroup request is made, pending activity should be true.
  ScriptPromise<IDLUndefined> join_promise = multicast_controller->joinGroup(
      script_state, kIPv4MulticastAddress, nullptr, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester join_tester(script_state, join_promise);

  EXPECT_TRUE(multicast_controller->HasPendingActivity());

  // Once the promise settles (fulfillment), pending activity should be false.
  creator->fake_udp_socket()->CompleteJoinGroup(net::OK);
  join_tester.WaitUntilSettled();
  ASSERT_TRUE(join_tester.IsFulfilled());
  EXPECT_FALSE(multicast_controller->HasPendingActivity());

  // When a leaveGroup request is made, pending activity should be true.
  ScriptPromise leave_promise = multicast_controller->leaveGroup(
      script_state, kIPv4MulticastAddress, nullptr, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester leave_tester(script_state, leave_promise);

  EXPECT_TRUE(multicast_controller->HasPendingActivity());

  // Once the promise settles (rejection), pending activity should be false.
  creator->fake_udp_socket()->CompleteLeaveGroup(net::ERR_FAILED);
  leave_tester.WaitUntilSettled();
  ASSERT_TRUE(leave_tester.IsRejected());
  EXPECT_FALSE(multicast_controller->HasPendingActivity());
}

// SSM (Source-Specific Multicast) Tests
//
// SSM tests use TEST_F to enable SourceSpecificMulticastInDirectSockets via
// ScopedSourceSpecificMulticastInDirectSocketsForTest, since the controller
// CHECKs the feature is enabled when a source address is provided. Unit tests
// call C++ directly, bypassing the IDL-level feature gate.
class MulticastControllerSSMTest : public testing::Test {
 private:
  ScopedSourceSpecificMulticastInDirectSocketsForTest ssm_enabled_{true};
};

TEST_F(MulticastControllerSSMTest, JoinAndLeaveGroup) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  ScriptState* script_state = scope.GetScriptState();
  NonThrowableExceptionState exception_state;

  // Join a group with source filtering (SSM).
  auto* join_options = MakeSourceOptions(kIPv4SourceAddress);
  ScriptPromise<IDLUndefined> join_promise = multicast_controller->joinGroup(
      script_state, kIPv4MulticastAddress, join_options, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester join_tester(script_state, join_promise);

  creator->fake_udp_socket()->CompleteJoinGroup(net::OK);
  join_tester.WaitUntilSettled();
  ASSERT_TRUE(join_tester.IsFulfilled());
  EXPECT_THAT(ExtractJoinedGroupStrings(multicast_controller->joinedGroups()),
              testing::ElementsAre("233.252.0.1@192.0.2.1"));

  // Leave the SSM group.
  auto* leave_options = MakeSourceOptions(kIPv4SourceAddress);
  ScriptPromise leave_promise = multicast_controller->leaveGroup(
      script_state, kIPv4MulticastAddress, leave_options, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester leave_tester(script_state, leave_promise);

  creator->fake_udp_socket()->CompleteLeaveGroup(net::OK);
  leave_tester.WaitUntilSettled();
  ASSERT_TRUE(leave_tester.IsFulfilled());
  EXPECT_TRUE(multicast_controller->joinedGroups().empty());
}

TEST_F(MulticastControllerSSMTest,JoinSameGroupDifferentSources) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  ScriptState* script_state = scope.GetScriptState();
  NonThrowableExceptionState exception_state;

  // Join the same multicast group with two different sources.
  auto* options_source1 = MakeSourceOptions(kIPv4SourceAddress);
  ScriptPromise join_promise1 = multicast_controller->joinGroup(
      script_state, kIPv4MulticastAddress, options_source1, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester join_tester1(script_state, join_promise1);
  creator->fake_udp_socket()->CompleteJoinGroup(net::OK);
  join_tester1.WaitUntilSettled();
  ASSERT_TRUE(join_tester1.IsFulfilled());

  auto* options_source2 = MakeSourceOptions(kIPv4SourceAddress2);
  ScriptPromise join_promise2 = multicast_controller->joinGroup(
      script_state, kIPv4MulticastAddress, options_source2, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester join_tester2(script_state, join_promise2);
  creator->fake_udp_socket()->CompleteJoinGroup(net::OK);
  join_tester2.WaitUntilSettled();
  ASSERT_TRUE(join_tester2.IsFulfilled());

  // Both source-specific memberships should be tracked.
  EXPECT_THAT(ExtractJoinedGroupStrings(multicast_controller->joinedGroups()),
              testing::UnorderedElementsAre("233.252.0.1@192.0.2.1",
                                            "233.252.0.1@192.0.2.2"));
}

TEST_F(MulticastControllerSSMTest,CannotJoinASMAndSSMForSameGroup) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  ScriptState* script_state = scope.GetScriptState();
  NonThrowableExceptionState exception_state;

  // Join group with ASM (no source).
  ScriptPromise join_asm = multicast_controller->joinGroup(
      script_state, kIPv4MulticastAddress, nullptr, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester asm_tester(script_state, join_asm);
  creator->fake_udp_socket()->CompleteJoinGroup(net::OK);
  asm_tester.WaitUntilSettled();
  ASSERT_TRUE(asm_tester.IsFulfilled());

  // Try to join same group with SSM - should fail.
  DummyExceptionStateForTesting exception_state2;
  auto* ssm_options = MakeSourceOptions(kIPv4SourceAddress);
  multicast_controller->joinGroup(script_state, kIPv4MulticastAddress,
                                  ssm_options, exception_state2);
  EXPECT_TRUE(exception_state2.HadException());
  EXPECT_EQ(exception_state2.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

TEST_F(MulticastControllerSSMTest,CannotJoinSSMThenASMForSameGroup) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  ScriptState* script_state = scope.GetScriptState();
  NonThrowableExceptionState exception_state;

  // Join group with SSM first.
  auto* ssm_options = MakeSourceOptions(kIPv4SourceAddress);
  ScriptPromise join_ssm = multicast_controller->joinGroup(
      script_state, kIPv4MulticastAddress, ssm_options, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester ssm_tester(script_state, join_ssm);
  creator->fake_udp_socket()->CompleteJoinGroup(net::OK);
  ssm_tester.WaitUntilSettled();
  ASSERT_TRUE(ssm_tester.IsFulfilled());

  // Try to join same group with ASM - should fail.
  DummyExceptionStateForTesting exception_state2;
  multicast_controller->joinGroup(script_state, kIPv4MulticastAddress, nullptr,
                                  exception_state2);
  EXPECT_TRUE(exception_state2.HadException());
  EXPECT_EQ(exception_state2.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

TEST_F(MulticastControllerSSMTest,LeaveMustMatchSource) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  ScriptState* script_state = scope.GetScriptState();
  NonThrowableExceptionState exception_state;

  // Join with source1.
  auto* join_options = MakeSourceOptions(kIPv4SourceAddress);
  ScriptPromise join_promise = multicast_controller->joinGroup(
      script_state, kIPv4MulticastAddress, join_options, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester join_tester(script_state, join_promise);
  creator->fake_udp_socket()->CompleteJoinGroup(net::OK);
  join_tester.WaitUntilSettled();
  ASSERT_TRUE(join_tester.IsFulfilled());

  // Try to leave with a different source - should fail.
  DummyExceptionStateForTesting exception_state2;
  auto* leave_options = MakeSourceOptions(kIPv4SourceAddress2);
  multicast_controller->leaveGroup(script_state, kIPv4MulticastAddress,
                                   leave_options, exception_state2);
  EXPECT_TRUE(exception_state2.HadException());
  EXPECT_EQ(exception_state2.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);

  // Group should still be joined.
  EXPECT_THAT(ExtractJoinedGroupStrings(multicast_controller->joinedGroups()),
              testing::ElementsAre("233.252.0.1@192.0.2.1"));
}

TEST_F(MulticastControllerSSMTest,InvalidSourceAddress) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  DummyExceptionStateForTesting exception_state;
  auto* options = MakeSourceOptions("invalid-source");
  multicast_controller->joinGroup(scope.GetScriptState(), kIPv4MulticastAddress,
                                  options, exception_state);

  EXPECT_TRUE(exception_state.HadException());
}

TEST_F(MulticastControllerSSMTest,IPv6WithSource) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  ScriptState* script_state = scope.GetScriptState();
  NonThrowableExceptionState exception_state;

  // Join IPv6 multicast group with source filtering.
  auto* options = MakeSourceOptions(kIPv6SourceAddress);
  ScriptPromise join_promise = multicast_controller->joinGroup(
      script_state, kIPv6MulticastAddress, options, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester join_tester(script_state, join_promise);

  creator->fake_udp_socket()->CompleteJoinGroup(net::OK);
  join_tester.WaitUntilSettled();
  ASSERT_TRUE(join_tester.IsFulfilled());
  EXPECT_THAT(ExtractJoinedGroupStrings(multicast_controller->joinedGroups()),
              testing::ElementsAre("ff02::1234@2001:db8::1"));
}

TEST_F(MulticastControllerSSMTest,JoinGroupTwiceWithSameSource) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  NonThrowableExceptionState exception_state;

  // Join a group with source.
  auto* join_options = MakeSourceOptions(kIPv4SourceAddress);
  ScriptPromise join_promise = multicast_controller->joinGroup(
      scope.GetScriptState(), kIPv4MulticastAddress, join_options,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester join_tester(scope.GetScriptState(), join_promise);
  creator->fake_udp_socket()->CompleteJoinGroup(net::OK);
  join_tester.WaitUntilSettled();
  ASSERT_TRUE(join_tester.IsFulfilled());

  // Try to join again with the same group and source.
  DummyExceptionStateForTesting exception_state2;
  auto* duplicate_options = MakeSourceOptions(kIPv4SourceAddress);
  multicast_controller->joinGroup(scope.GetScriptState(), kIPv4MulticastAddress,
                                  duplicate_options, exception_state2);
  EXPECT_TRUE(exception_state2.HadException());
  EXPECT_EQ(exception_state2.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

TEST_F(MulticastControllerSSMTest,IPv4MulticastWithIPv6SourceFails) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  DummyExceptionStateForTesting exception_state;

  // Try to join IPv4 multicast group with IPv6 source - should fail.
  auto* options = MakeSourceOptions(kIPv6SourceAddress);
  multicast_controller->joinGroup(scope.GetScriptState(), kIPv4MulticastAddress,
                                  options, exception_state);

  EXPECT_TRUE(exception_state.HadException());
}

TEST_F(MulticastControllerSSMTest,IPv6MulticastWithIPv4SourceFails) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  DummyExceptionStateForTesting exception_state;

  // Try to join IPv6 multicast group with IPv4 source - should fail.
  auto* options = MakeSourceOptions(kIPv4SourceAddress);
  multicast_controller->joinGroup(scope.GetScriptState(), kIPv6MulticastAddress,
                                  options, exception_state);

  EXPECT_TRUE(exception_state.HadException());
}

TEST_F(MulticastControllerSSMTest,MulticastAddressAsSourceFails) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  DummyExceptionStateForTesting exception_state;
  // Multicast address as source must be rejected.
  auto* options = MakeSourceOptions("233.252.0.2");
  multicast_controller->joinGroup(scope.GetScriptState(), kIPv4MulticastAddress,
                                  options, exception_state);

  EXPECT_TRUE(exception_state.HadException());
}

TEST_F(MulticastControllerSSMTest,ZeroAddressAsSourceFails) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  DummyExceptionStateForTesting exception_state;
  // Zero (0.0.0.0) as source must be rejected.
  auto* options = MakeSourceOptions("0.0.0.0");
  multicast_controller->joinGroup(scope.GetScriptState(), kIPv4MulticastAddress,
                                  options, exception_state);

  EXPECT_TRUE(exception_state.HadException());
}

TEST_F(MulticastControllerSSMTest,CannotLeaveASMGroupWithSourceAddress) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  ScriptState* script_state = scope.GetScriptState();
  NonThrowableExceptionState exception_state;

  // Join as ASM.
  ScriptPromise join_promise = multicast_controller->joinGroup(
      script_state, kIPv4MulticastAddress, nullptr, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester join_tester(script_state, join_promise);
  creator->fake_udp_socket()->CompleteJoinGroup(net::OK);
  join_tester.WaitUntilSettled();
  ASSERT_TRUE(join_tester.IsFulfilled());

  // Try to leave with a source address - key mismatch, should throw.
  DummyExceptionStateForTesting exception_state2;
  auto* leave_options = MakeSourceOptions(kIPv4SourceAddress);
  multicast_controller->leaveGroup(script_state, kIPv4MulticastAddress,
                                   leave_options, exception_state2);
  EXPECT_TRUE(exception_state2.HadException());
  EXPECT_EQ(exception_state2.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

TEST(MulticastControllerTest, OnCloseOrAbortRejectsPendingLeavePromise) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  ScriptState* script_state = scope.GetScriptState();
  NonThrowableExceptionState exception_state;

  // Join successfully first.
  ScriptPromise join_promise = multicast_controller->joinGroup(
      script_state, kIPv4MulticastAddress, nullptr, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester join_tester(script_state, join_promise);
  creator->fake_udp_socket()->CompleteJoinGroup(net::OK);
  join_tester.WaitUntilSettled();
  ASSERT_TRUE(join_tester.IsFulfilled());

  // Start a leave but do NOT complete it.
  ScriptPromise<IDLUndefined> leave_promise = multicast_controller->leaveGroup(
      script_state, kIPv4MulticastAddress, nullptr, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester leave_tester(script_state, leave_promise);

  EXPECT_TRUE(multicast_controller->HasPendingActivity());

  // Close while leave is pending - should reject, not leave unsettled.
  multicast_controller->OnCloseOrAbort();

  leave_tester.WaitUntilSettled();
  EXPECT_TRUE(leave_tester.IsRejected());
  EXPECT_FALSE(multicast_controller->HasPendingActivity());
}

TEST_F(MulticastControllerSSMTest,SourceAddressPassedToSocket) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  ScriptState* script_state = scope.GetScriptState();
  NonThrowableExceptionState exception_state;

  auto* options = MakeSourceOptions(kIPv4SourceAddress);
  ScriptPromise join_promise = multicast_controller->joinGroup(
      script_state, kIPv4MulticastAddress, options, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester join_tester(script_state, join_promise);

  // Run pending tasks to deliver the JoinGroup Mojo message to the fake socket.
  test::RunPendingTasks();

  // Verify the source address was forwarded over the Mojo boundary.
  net::IPAddress expected_source;
  ASSERT_TRUE(expected_source.AssignFromIPLiteral(kIPv4SourceAddress));
  ASSERT_TRUE(
      creator->fake_udp_socket()->last_join_source_address().has_value());
  EXPECT_EQ(creator->fake_udp_socket()->last_join_source_address().value(),
            expected_source);

  creator->fake_udp_socket()->CompleteJoinGroup(net::OK);
  join_tester.WaitUntilSettled();
}

TEST_F(MulticastControllerSSMTest,TOCTOU_PendingASMBlocksSSMJoin) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedMulticastCreator creator(MakeGarbageCollected<MulticastCreator>(scope));
  MulticastController* multicast_controller = creator->Create(scope);

  ScriptState* script_state = scope.GetScriptState();
  NonThrowableExceptionState exception_state;

  // Start an ASM join but don't complete it yet.
  ScriptPromise asm_promise = multicast_controller->joinGroup(
      script_state, kIPv4MulticastAddress, nullptr, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  // While ASM join is in-flight, try SSM for same group — should throw.
  DummyExceptionStateForTesting exception_state2;
  auto* ssm_options = MakeSourceOptions(kIPv4SourceAddress);
  multicast_controller->joinGroup(script_state, kIPv4MulticastAddress,
                                  ssm_options, exception_state2);
  EXPECT_TRUE(exception_state2.HadException());
  EXPECT_EQ(exception_state2.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);

  // Cleanup: complete the pending ASM join.
  creator->fake_udp_socket()->CompleteJoinGroup(net::OK);
  ScriptPromiseTester asm_tester(script_state, asm_promise);
  asm_tester.WaitUntilSettled();
}

}  // namespace

}  // namespace blink
