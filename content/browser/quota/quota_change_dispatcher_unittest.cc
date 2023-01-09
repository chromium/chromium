// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/quota/quota_change_dispatcher.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/test/task_environment.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/quota/quota_manager_host.mojom.h"

namespace content {

class MockQuotaChangeListener : public blink::mojom::QuotaChangeListener {
 public:
  explicit MockQuotaChangeListener(
      mojo::PendingReceiver<blink::mojom::QuotaChangeListener> receiver)
      : receiver_(this, std::move(receiver)) {}

  ~MockQuotaChangeListener() override = default;

  void SetQuotaChangeCallback(base::OnceClosure callback) {
    callback_ = std::move(callback);
  }

  void OnQuotaChange() override {
    DCHECK(callback_);
    ++quota_change_call_count_;
    std::move(callback_).Run();
  }

  void RemoveReceivers() { receiver_.reset(); }

  int quota_change_call_count() const { return quota_change_call_count_; }

 private:
  mojo::Receiver<blink::mojom::QuotaChangeListener> receiver_;
  base::OnceClosure callback_;
  int quota_change_call_count_ = 0;
};

class QuotaChangeDispatcherTest : public testing::Test {
 public:
  QuotaChangeDispatcherTest() = default;
  ~QuotaChangeDispatcherTest() override = default;

  void SetUp() override {
    quota_change_dispatcher_ = base::MakeRefCounted<QuotaChangeDispatcher>(
        task_environment_.GetMainThreadTaskRunner());
  }

  std::map<blink::StorageKey, QuotaChangeDispatcher::DelayedStorageKeyListener>*
  listeners_by_storage_key() {
    return &(quota_change_dispatcher_->listeners_by_storage_key_);
  }

  mojo::RemoteSet<blink::mojom::QuotaChangeListener>* GetListeners(
      const blink::StorageKey& storage_key) {
    DCHECK_GT(listeners_by_storage_key()->count(storage_key), 0U);
    return &(listeners_by_storage_key()->find(storage_key)->second.listeners);
  }

  base::TimeDelta GetDelay(const blink::StorageKey& storage_key) {
    DCHECK_GT(listeners_by_storage_key()->count(storage_key), 0U);
    return listeners_by_storage_key()->find(storage_key)->second.delay;
  }

  void DispatchCompleted() {
    if (run_loop_)  // Set in WaitForChange().
      run_loop_->Quit();
  }

  void WaitForChange() {
    base::RunLoop loop;
    run_loop_ = &loop;
    loop.Run();
    run_loop_ = nullptr;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<QuotaChangeDispatcher> quota_change_dispatcher_;

 private:
  // If not null, will be stopped when a quota change notification is received.
  raw_ptr<base::RunLoop> run_loop_ = nullptr;
};

TEST_F(QuotaChangeDispatcherTest, AddChangeListener) {
  const blink::StorageKey& storage_key_foo_ =
      blink::StorageKey::CreateFromStringForTesting("http://foo.com/");
  mojo::PendingRemote<blink::mojom::QuotaChangeListener> mojo_listener;
  mojo::PendingReceiver<blink::mojom::QuotaChangeListener> receiver =
      mojo_listener.InitWithNewPipeAndPassReceiver();
  MockQuotaChangeListener listener(std::move(receiver));

  EXPECT_EQ(0U, listeners_by_storage_key()->size());

  quota_change_dispatcher_->AddChangeListener(storage_key_foo_,
                                              std::move(mojo_listener));

  EXPECT_EQ(1U, listeners_by_storage_key()->size());
  EXPECT_TRUE(base::Contains(*(listeners_by_storage_key()), storage_key_foo_));
}

TEST_F(QuotaChangeDispatcherTest, AddChangeListener_DuplicateStorageKeys) {
  const blink::StorageKey& storage_key_foo_ =
      blink::StorageKey::CreateFromStringForTesting("http://foo.com/");
  constexpr double kMinDelay = 0;
  constexpr double kMaxDelay = 2;
  mojo::PendingRemote<blink::mojom::QuotaChangeListener> mojo_listener_1;
  mojo::PendingReceiver<blink::mojom::QuotaChangeListener> receiver_1 =
      mojo_listener_1.InitWithNewPipeAndPassReceiver();
  MockQuotaChangeListener listener_1(std::move(receiver_1));

  mojo::PendingRemote<blink::mojom::QuotaChangeListener> mojo_listener_2;
  mojo::PendingReceiver<blink::mojom::QuotaChangeListener> receiver_2 =
      mojo_listener_2.InitWithNewPipeAndPassReceiver();
  MockQuotaChangeListener listener_2(std::move(receiver_2));

  EXPECT_EQ(0U, listeners_by_storage_key()->size());

  quota_change_dispatcher_->AddChangeListener(storage_key_foo_,
                                              std::move(mojo_listener_1));
  quota_change_dispatcher_->AddChangeListener(storage_key_foo_,
                                              std::move(mojo_listener_2));

  EXPECT_EQ(1U, listeners_by_storage_key()->size());

  EXPECT_EQ(2U, GetListeners(storage_key_foo_)->size());
  EXPECT_LE(kMinDelay, GetDelay(storage_key_foo_).InSecondsF());
  EXPECT_GE(kMaxDelay, GetDelay(storage_key_foo_).InSecondsF());
}

TEST_F(QuotaChangeDispatcherTest, DispatchEvents) {
  const blink::StorageKey& storage_key_foo_ =
      blink::StorageKey::CreateFromStringForTesting("http://foo.com/");

  mojo::PendingRemote<blink::mojom::QuotaChangeListener> mojo_listener;
  mojo::PendingReceiver<blink::mojom::QuotaChangeListener> receiver =
      mojo_listener.InitWithNewPipeAndPassReceiver();

  quota_change_dispatcher_->AddChangeListener(storage_key_foo_,
                                              std::move(mojo_listener));
  MockQuotaChangeListener listener(std::move(receiver));

  EXPECT_EQ(0, listener.quota_change_call_count());

  base::RepeatingClosure barrier = base::BarrierClosure(
      1, base::BindOnce(&QuotaChangeDispatcherTest::DispatchCompleted,
                        base::Unretained(this)));
  listener.SetQuotaChangeCallback(barrier);

  quota_change_dispatcher_->MaybeDispatchEvents();
  WaitForChange();

  EXPECT_EQ(1, listener.quota_change_call_count());
}

TEST_F(QuotaChangeDispatcherTest, DispatchEvents_Multiple) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kQuotaChangeEventInterval, "0");

  const blink::StorageKey& storage_key_foo_ =
      blink::StorageKey::CreateFromStringForTesting("http://foo.com/");

  mojo::PendingRemote<blink::mojom::QuotaChangeListener> mojo_listener_1;
  mojo::PendingReceiver<blink::mojom::QuotaChangeListener> receiver_1 =
      mojo_listener_1.InitWithNewPipeAndPassReceiver();
  MockQuotaChangeListener listener_1(std::move(receiver_1));

  mojo::PendingRemote<blink::mojom::QuotaChangeListener> mojo_listener_2;
  mojo::PendingReceiver<blink::mojom::QuotaChangeListener> receiver_2 =
      mojo_listener_2.InitWithNewPipeAndPassReceiver();
  MockQuotaChangeListener listener_2(std::move(receiver_2));

  quota_change_dispatcher_->AddChangeListener(storage_key_foo_,
                                              std::move(mojo_listener_1));
  quota_change_dispatcher_->AddChangeListener(storage_key_foo_,
                                              std::move(mojo_listener_2));

  EXPECT_EQ(0, listener_1.quota_change_call_count());
  EXPECT_EQ(0, listener_2.quota_change_call_count());

  base::RepeatingClosure barrier = base::BarrierClosure(
      2, base::BindOnce(&QuotaChangeDispatcherTest::DispatchCompleted,
                        base::Unretained(this)));
  listener_1.SetQuotaChangeCallback(barrier);
  listener_2.SetQuotaChangeCallback(barrier);

  quota_change_dispatcher_->MaybeDispatchEvents();
  WaitForChange();

  EXPECT_EQ(1, listener_1.quota_change_call_count());
  EXPECT_EQ(1, listener_2.quota_change_call_count());

  barrier = base::BarrierClosure(
      2, base::BindOnce(&QuotaChangeDispatcherTest::DispatchCompleted,
                        base::Unretained(this)));
  listener_1.SetQuotaChangeCallback(barrier);
  listener_2.SetQuotaChangeCallback(barrier);

  quota_change_dispatcher_->MaybeDispatchEvents();
  WaitForChange();

  EXPECT_EQ(2, listener_1.quota_change_call_count());
  EXPECT_EQ(2, listener_2.quota_change_call_count());
}

TEST_F(QuotaChangeDispatcherTest, RemoveThenDispatch) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kQuotaChangeEventInterval, "0");

  const blink::StorageKey& storage_key_foo_ =
      blink::StorageKey::CreateFromStringForTesting("http://foo.com/");
  const blink::StorageKey& storage_key_bar_ =
      blink::StorageKey::CreateFromStringForTesting("http://bar.com/");
  mojo::PendingRemote<blink::mojom::QuotaChangeListener> mojo_listener_1;
  mojo::PendingReceiver<blink::mojom::QuotaChangeListener> receiver_1 =
      mojo_listener_1.InitWithNewPipeAndPassReceiver();
  MockQuotaChangeListener listener_1(std::move(receiver_1));

  mojo::PendingRemote<blink::mojom::QuotaChangeListener> mojo_listener_2;
  mojo::PendingReceiver<blink::mojom::QuotaChangeListener> receiver_2 =
      mojo_listener_2.InitWithNewPipeAndPassReceiver();
  MockQuotaChangeListener listener_2(std::move(receiver_2));

  quota_change_dispatcher_->AddChangeListener(storage_key_foo_,
                                              std::move(mojo_listener_1));
  quota_change_dispatcher_->AddChangeListener(storage_key_bar_,
                                              std::move(mojo_listener_2));

  EXPECT_EQ(0, listener_1.quota_change_call_count());
  EXPECT_EQ(0, listener_2.quota_change_call_count());

  base::RepeatingClosure barrier = base::BarrierClosure(
      2, base::BindOnce(&QuotaChangeDispatcherTest::DispatchCompleted,
                        base::Unretained(this)));
  listener_1.SetQuotaChangeCallback(barrier);
  listener_2.SetQuotaChangeCallback(barrier);

  quota_change_dispatcher_->MaybeDispatchEvents();
  WaitForChange();

  EXPECT_EQ(1, listener_1.quota_change_call_count());
  EXPECT_EQ(1, listener_2.quota_change_call_count());

  listener_2.RemoveReceivers();

  barrier = base::BarrierClosure(
      1, base::BindOnce(&QuotaChangeDispatcherTest::DispatchCompleted,
                        base::Unretained(this)));
  listener_1.SetQuotaChangeCallback(barrier);

  quota_change_dispatcher_->MaybeDispatchEvents();
  WaitForChange();

  EXPECT_EQ(2, listener_1.quota_change_call_count());
  EXPECT_EQ(1, listener_2.quota_change_call_count());
}

}  // namespace content
