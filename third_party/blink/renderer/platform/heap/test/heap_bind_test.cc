// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/heap_bind.h"

#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink::bindings {
namespace {

class MockReceiver : public GarbageCollected<MockReceiver> {
 public:
  MockReceiver() { EXPECT_CALL(*this, OnDestruction); }

  void Trace(Visitor* visitor) const { OnTrace(); }

  ~MockReceiver() { OnDestruction(); }

  MOCK_METHOD(void, OnDestruction, ());
  MOCK_METHOD(void, OnTrace, (), (const));

  MOCK_METHOD(void, Method0, ());
  MOCK_METHOD(int, Method0Ret, ());
  MOCK_METHOD(void, Method1, (int));
  MOCK_METHOD(void, Method2, (int, double));
  MOCK_METHOD(void, Method1Ptr, (MockReceiver*));
  MOCK_METHOD(void, Method1Str, (const String&));
  MOCK_METHOD(void, Method0Const, (), (const));
  MOCK_METHOD(void, Method1UPtr, (std::unique_ptr<int>));
  MOCK_METHOD(void, Method1VecVal, (std::vector<char>));
  MOCK_METHOD(void, Method1VecRef, (std::vector<char>&));
  MOCK_METHOD(void, Method1VecRValRef, (std::vector<char>&&));
  MOCK_METHOD(void,
              Method1HeapVec,
              (const GCedHeapVector<Member<MockReceiver>>*));
};

TEST(HeapBindTest, Empty) {
  HeapCallback<void()> cb;
  EXPECT_TRUE(!cb);
  EXPECT_TRUE(cb == HeapCallback<void()>());
  auto* receiver = MakeGarbageCollected<MockReceiver>();
  HeapCallback<void()> cb1 = HeapBind(&MockReceiver::Method0, receiver);
  EXPECT_TRUE(cb1);
  EXPECT_FALSE(cb1 == HeapCallback<void()>());
}

TEST(HeapBindTest, Equality) {
  auto* receiver = MakeGarbageCollected<MockReceiver>();
  HeapCallback<void()> cb1 = HeapBind(&MockReceiver::Method0, receiver);
  HeapCallback<void()> cb2 = HeapBind(&MockReceiver::Method0, receiver);
  HeapCallback<void()> cb3 = cb2;
  EXPECT_TRUE(cb1 != cb2);
  EXPECT_FALSE(cb1 == cb2);
  EXPECT_TRUE(cb2 == cb3);
}

TEST(HeapBindTest, BindNothing) {
  std::ignore = HeapBind([] {});
}

TEST(HeapBindTest, Method0) {
  auto* receiver = MakeGarbageCollected<MockReceiver>();
  HeapCallback<void()> cb = HeapBind(&MockReceiver::Method0, receiver);
  EXPECT_CALL(*receiver, Method0());
  cb.Run();
}

TEST(HeapBindTest, ConstMethod) {
  auto* receiver = MakeGarbageCollected<MockReceiver>();
  HeapCallback<void()> cb = HeapBind(&MockReceiver::Method0Const, receiver);
  EXPECT_CALL(*receiver, Method0Const());
  cb.Run();
}

TEST(HeapBindTest, ReturnValue) {
  auto* receiver = MakeGarbageCollected<MockReceiver>();
  HeapCallback<int()> cb = HeapBind(&MockReceiver::Method0Ret, receiver);
  EXPECT_CALL(*receiver, Method0Ret()).WillOnce(testing::Return(42));
  EXPECT_EQ(cb.Run(), 42);
}

TEST(HeapBindTest, Method1Argument) {
  auto* receiver = MakeGarbageCollected<MockReceiver>();
  {
    HeapCallback<void(int)> cb = HeapBind(&MockReceiver::Method1, receiver);
    EXPECT_CALL(*receiver, Method1(41));
    EXPECT_CALL(*receiver, Method1(42));
    cb.Run(41);
    cb.Run(42);
  }
  {
    HeapCallback<void()> cb = HeapBind(&MockReceiver::Method1, receiver, 43);
    EXPECT_CALL(*receiver, Method1(43));
    cb.Run();
  }
}

TEST(HeapBindTest, Method2Arguments) {
  auto* receiver = MakeGarbageCollected<MockReceiver>();
  {
    HeapCallback<void(int, double)> cb =
        HeapBind(&MockReceiver::Method2, receiver);
    EXPECT_CALL(*receiver, Method2(41, 4.1));
    cb.Run(41, 4.1);
  }
  {
    HeapCallback<void(double)> cb =
        HeapBind(&MockReceiver::Method2, receiver, 42);
    EXPECT_CALL(*receiver, Method2(42, 4.2));
    cb.Run(4.2);
  }
  {
    HeapCallback<void()> cb3 =
        HeapBind(&MockReceiver::Method2, receiver, 43, 4.3);
    EXPECT_CALL(*receiver, Method2(43, 4.3));
    cb3.Run();
  }
}

// Assure we can have callback as a member.
class CallbackWrapper : public GarbageCollected<CallbackWrapper> {
 public:
  explicit CallbackWrapper(HeapCallback<void()> cb) : callback_(cb) {}
  void Invoke() { callback_.Run(); }
  void Trace(Visitor* visitor) const { visitor->Trace(callback_); }

 private:
  HeapCallback<void()> callback_;
};

TEST(HeapBindTest, String) {
  auto* receiver = MakeGarbageCollected<MockReceiver>();
  auto cb = HeapBind(&MockReceiver::Method1Str, receiver, "don\'t panic");
  EXPECT_CALL(*receiver, Method1Str(testing::Eq("don\'t panic")));
  cb.Run();
}

TEST(HeapBindTest, Wrapped) {
  base::test::TaskEnvironment task_environment;

  auto* receiver = MakeGarbageCollected<MockReceiver>();
  Persistent<CallbackWrapper> wrapper = MakeGarbageCollected<CallbackWrapper>(
      HeapBind(&MockReceiver::Method0, receiver));
  EXPECT_CALL(*receiver, Method0());
  EXPECT_CALL(*receiver, OnTrace()).Times(testing::AnyNumber());
  ThreadState::Current()->CollectAllGarbageForTesting();
  wrapper->Invoke();
}

TEST(HeapBindTest, HeapVector) {
  base::test::TaskEnvironment task_environment;

  auto* receiver = MakeGarbageCollected<MockReceiver>();
  auto* vec = MakeGarbageCollected<GCedHeapVector<Member<MockReceiver>>>();
  vec->push_back(MakeGarbageCollected<MockReceiver>());
  vec->push_back(MakeGarbageCollected<MockReceiver>());

  EXPECT_CALL(*receiver, OnTrace()).Times(testing::AnyNumber());
  for (auto& item : *vec) {
    EXPECT_CALL(*item, OnTrace()).Times(testing::AnyNumber());
  }
  auto cb = HeapBind(&MockReceiver::Method1HeapVec, receiver, vec);

  Persistent<CallbackWrapper> wrapper =
      MakeGarbageCollected<CallbackWrapper>(cb);
  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_CALL(*receiver, Method1HeapVec(testing::Pointee(testing::SizeIs(2))));
  cb.Run();
}

void StandaloneFunc(MockReceiver* receiver) {
  receiver->Method0();
}

TEST(HeapBindTest, Standalone) {
  auto* receiver = MakeGarbageCollected<MockReceiver>();
  auto cb = HeapBind(StandaloneFunc, receiver);
  EXPECT_CALL(*receiver, Method0());
  cb.Run();
}

TEST(HeapBindTest, Lambda) {
  auto* receiver = MakeGarbageCollected<MockReceiver>();
  HeapCallback<void()> cb =
      HeapBind([](MockReceiver* receiver) { receiver->Method0(); }, receiver);
  EXPECT_CALL(*receiver, Method0());
  cb.Run();
}

TEST(HeapBindTest, BindBound) {
  base::test::TaskEnvironment task_environment;
  auto* receiver = MakeGarbageCollected<MockReceiver>();
  auto cb1 = HeapBind(&MockReceiver::Method2, receiver);
  auto cb2 = HeapBind(cb1, 42);
  auto cb3 = HeapBind(cb2, 4.2);
  // We don't have a persistent counterpart for HeapCallback wrapper, so this
  // is our only way of retaining it beyond GC for now.
  Persistent<CallbackWrapper> wrapper =
      MakeGarbageCollected<CallbackWrapper>(cb3);
  EXPECT_CALL(*receiver, Method2(42, 4.1));
  EXPECT_CALL(*receiver, Method2(42, 4.2));
  cb2.Run(4.1);
  EXPECT_CALL(*receiver, OnTrace()).Times(testing::AnyNumber());
  ThreadState::Current()->CollectAllGarbageForTesting();
  cb3.Run();
}

TEST(HeapBindTest, MoveOnlyArg) {
  auto* receiver = MakeGarbageCollected<MockReceiver>();
  auto cb = HeapBind(&MockReceiver::Method1UPtr, receiver);
  EXPECT_CALL(*receiver, Method1UPtr(testing::Pointee(42)));
  cb.Run(std::make_unique<int>(42));
}

TEST(HeapBindTest, Moveable) {
  auto* receiver = MakeGarbageCollected<MockReceiver>();
  {
    auto cb = HeapBind(&MockReceiver::Method1VecVal, receiver);

    std::vector<char> v = {'a', 'b', 'c'};
    EXPECT_CALL(*receiver, Method1VecVal(testing::SizeIs(3)));
    EXPECT_CALL(*receiver, Method1VecVal(testing::SizeIs(0)));
    cb.Run(std::move(v));
    cb.Run(std::move(v));
  }
  {
    auto cb = HeapBind(&MockReceiver::Method1VecRValRef, receiver);

    std::vector<char> v = {'a', 'b', 'c'};
    EXPECT_CALL(*receiver, Method1VecRValRef(testing::SizeIs(3))).Times(2);
    cb.Run(std::move(v));
    cb.Run(std::move(v));
  }
  {
    std::vector<char> v = {'a', 'b', 'c'};
    auto cb = HeapBind(&MockReceiver::Method1VecVal, receiver, std::move(v));
    EXPECT_THAT(v, testing::IsEmpty());
    EXPECT_CALL(*receiver, Method1VecVal(testing::SizeIs(3))).Times(2);
    cb.Run();
    cb.Run();
  }
  {
    std::vector<char> v = {'a', 'b', 'c'};
    auto cb = HeapBind(&MockReceiver::Method1VecRef, receiver, std::move(v));
    EXPECT_THAT(v, testing::IsEmpty());
    EXPECT_CALL(*receiver, Method1VecRef(testing::SizeIs(3))).Times(2);
    cb.Run();
    cb.Run();
  }
}

TEST(HeapBindTest, NullReceiverCheck) {
  EXPECT_CHECK_DEATH(std::ignore =
                         HeapBind(&MockReceiver::Method1Ptr, nullptr));
}

TEST(HeapBindTest, NullReceiverCheckForBound) {
  HeapCallback<void(int)> cb;

  EXPECT_CHECK_DEATH(std::ignore = HeapBind(cb, 42));
}

int Func(MockReceiver* receiver) {
  CHECK(!receiver);
  return 42;
}

TEST(HeapBindTest, NullArgumentsOk) {
  {
    auto* receiver = MakeGarbageCollected<MockReceiver>();
    auto cb = HeapBind(&MockReceiver::Method1Ptr, receiver, nullptr);
    EXPECT_CALL(*receiver, Method1Ptr(testing::IsNull()));
    cb.Run();
  }
  {
    auto cb = HeapBind(Func, nullptr);
    EXPECT_THAT(cb.Run(), testing::Eq(42));
  }
}

TEST(HeapBindTest, BindNothingToHeapCallbackOptimization) {
  auto* receiver = MakeGarbageCollected<MockReceiver>();
  auto cb = HeapBind(&MockReceiver::Method1Ptr, receiver, nullptr);
  auto cb2 = HeapBind(cb);
  EXPECT_TRUE(cb == cb2);
}

}  // namespace

}  // namespace blink::bindings
