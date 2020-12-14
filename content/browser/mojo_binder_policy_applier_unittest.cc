// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo_binder_policy_applier.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "content/browser/mojo_binder_policy_map_impl.h"
#include "content/test/test_mojo_binder_policy_applier_unittest.mojom.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// A test class that implements test interfaces and provides verification
// methods.
class TestReceiverCollector : public mojom::TestInterfaceForDefer,
                              public mojom::TestInterfaceForGrant,
                              public mojom::TestInterfaceForCancel {
 public:
  TestReceiverCollector() = default;

  ~TestReceiverCollector() override = default;

  // Deletes copy and move operations.
  TestReceiverCollector(const TestReceiverCollector& other) = delete;
  TestReceiverCollector& operator=(const TestReceiverCollector& other) = delete;
  TestReceiverCollector(TestReceiverCollector&&) = delete;
  TestReceiverCollector& operator=(TestReceiverCollector&&) = delete;

  void BindDeferInterface(
      mojo::PendingReceiver<mojom::TestInterfaceForDefer> receiver) {
    ASSERT_FALSE(defer_receiver_.is_bound());
    defer_receiver_.Bind(std::move(receiver));
  }

  void BindGrantInterface(
      mojo::PendingReceiver<mojom::TestInterfaceForGrant> receiver) {
    ASSERT_FALSE(grant_receiver_.is_bound());
    grant_receiver_.Bind(std::move(receiver));
  }

  void BindCancelInterface(
      mojo::PendingReceiver<mojom::TestInterfaceForCancel> receiver) {
    ASSERT_FALSE(cancel_receiver_.is_bound());
    cancel_receiver_.Bind(std::move(receiver));
  }

  // Will be called when MojoBinderPolicyApplier::ApplyPolicyToBinder()
  // handles a kCancel binding request.
  void Cancel() { is_canceled_ = true; }

  // Used to check if the cancel_closure of MojoBinderPolicyApplier was
  // executed.
  bool IsCanceled() { return is_canceled_; }

  bool IsDeferReceiverBound() const { return defer_receiver_.is_bound(); }

  bool IsGrantReceiverBound() const { return grant_receiver_.is_bound(); }

  bool IsCancelReceiverBound() const { return cancel_receiver_.is_bound(); }

 private:
  mojo::Receiver<mojom::TestInterfaceForDefer> defer_receiver_{this};
  mojo::Receiver<mojom::TestInterfaceForGrant> grant_receiver_{this};
  mojo::Receiver<mojom::TestInterfaceForCancel> cancel_receiver_{this};
  bool is_canceled_ = false;
};

class MojoBinderPolicyApplierTest : public testing::Test {
 public:
  MojoBinderPolicyApplierTest() = default;

 protected:
  const MojoBinderPolicyMapImpl policy_map_{
      {{"content.mojom.TestInterfaceForDefer", MojoBinderPolicy::kDefer},
       {"content.mojom.TestInterfaceForGrant", MojoBinderPolicy::kGrant},
       {"content.mojom.TestInterfaceForCancel", MojoBinderPolicy::kCancel}}};
  TestReceiverCollector collector_{};
  MojoBinderPolicyApplier policy_applier_{
      &policy_map_, base::BindOnce(&TestReceiverCollector::Cancel,
                                   base::Unretained(&collector_))};

 private:
  base::test::TaskEnvironment task_environment_;
};

// Verifies that interfaces whose policies are kDefer cannot be bound until
// GrantAll() is called.
TEST_F(MojoBinderPolicyApplierTest, ApplyDeferPolicy) {
  // Initialize Mojo interfaces.
  mojo::Remote<mojom::TestInterfaceForDefer> defer_remote;
  mojo::GenericPendingReceiver defer_receiver(
      defer_remote.BindNewPipeAndPassReceiver());

  // Delay binding the interface until GrantAll() is called.
  const std::string interface_name = defer_receiver.interface_name().value();
  EXPECT_FALSE(collector_.IsCanceled());
  policy_applier_.ApplyPolicyToBinder(
      interface_name,
      base::BindOnce(&TestReceiverCollector::BindDeferInterface,
                     base::Unretained(&collector_),
                     defer_receiver.As<mojom::TestInterfaceForDefer>()));
  EXPECT_FALSE(collector_.IsDeferReceiverBound());
  policy_applier_.GrantAll();
  EXPECT_TRUE(collector_.IsDeferReceiverBound());
  EXPECT_FALSE(collector_.IsCanceled());
}

// Verifies that interfaces whose policies are kGrant can be bound immediately.
TEST_F(MojoBinderPolicyApplierTest, ApplyGrantPolicy) {
  // Initialize Mojo interfaces.
  mojo::Remote<mojom::TestInterfaceForGrant> grant_remote;
  mojo::GenericPendingReceiver grant_receiver(
      grant_remote.BindNewPipeAndPassReceiver());

  // Bind the interface immediately if the policy is kGrant.
  const std::string interface_name = grant_receiver.interface_name().value();
  EXPECT_FALSE(collector_.IsCanceled());
  EXPECT_FALSE(collector_.IsGrantReceiverBound());
  policy_applier_.ApplyPolicyToBinder(
      interface_name,
      base::BindOnce(&TestReceiverCollector::BindGrantInterface,
                     base::Unretained(&collector_),
                     grant_receiver.As<mojom::TestInterfaceForGrant>()));
  EXPECT_TRUE(collector_.IsGrantReceiverBound());
  EXPECT_FALSE(collector_.IsCanceled());
}

// Verifies that when receiving an interface whose policy is kCancel,
// cancel_closure_ can be invoked.
TEST_F(MojoBinderPolicyApplierTest, ApplyCancelPolicy) {
  // Initialize Mojo interfaces.
  mojo::Remote<mojom::TestInterfaceForCancel> cancel_remote;
  mojo::GenericPendingReceiver cancel_receiver(
      cancel_remote.BindNewPipeAndPassReceiver());

  const std::string interface_name = cancel_receiver.interface_name().value();
  EXPECT_FALSE(collector_.IsCanceled());
  EXPECT_FALSE(collector_.IsCancelReceiverBound());
  policy_applier_.ApplyPolicyToBinder(
      interface_name,
      base::BindOnce(&TestReceiverCollector::BindCancelInterface,
                     base::Unretained(&collector_),
                     cancel_receiver.As<mojom::TestInterfaceForCancel>()));
  EXPECT_TRUE(collector_.IsCanceled());
  EXPECT_FALSE(collector_.IsCancelReceiverBound());
}

// Verifies that all interfaces are bound immediately if GrantAll() is called,
// regardless of policies.
TEST_F(MojoBinderPolicyApplierTest, BindInterfacesAfterResolving) {
  // Initialize Mojo interfaces.
  mojo::Remote<mojom::TestInterfaceForDefer> defer_remote;
  mojo::GenericPendingReceiver defer_receiver(
      defer_remote.BindNewPipeAndPassReceiver());
  mojo::Remote<mojom::TestInterfaceForGrant> grant_remote;
  mojo::GenericPendingReceiver grant_receiver(
      grant_remote.BindNewPipeAndPassReceiver());
  mojo::Remote<mojom::TestInterfaceForCancel> cancel_remote;
  mojo::GenericPendingReceiver cancel_receiver(
      cancel_remote.BindNewPipeAndPassReceiver());

  policy_applier_.GrantAll();
  // All interfaces should be bound immediately.
  const std::string defer_interface_name =
      defer_receiver.interface_name().value();
  const std::string grant_interface_name =
      grant_receiver.interface_name().value();
  const std::string cancel_interface_name =
      cancel_receiver.interface_name().value();
  EXPECT_FALSE(collector_.IsCanceled());
  EXPECT_FALSE(collector_.IsGrantReceiverBound());
  EXPECT_FALSE(collector_.IsDeferReceiverBound());
  EXPECT_FALSE(collector_.IsCancelReceiverBound());
  policy_applier_.ApplyPolicyToBinder(
      defer_interface_name,
      base::BindOnce(&TestReceiverCollector::BindDeferInterface,
                     base::Unretained(&collector_),
                     defer_receiver.As<mojom::TestInterfaceForDefer>()));
  policy_applier_.ApplyPolicyToBinder(
      grant_interface_name,
      base::BindOnce(&TestReceiverCollector::BindGrantInterface,
                     base::Unretained(&collector_),
                     grant_receiver.As<mojom::TestInterfaceForGrant>()));
  policy_applier_.ApplyPolicyToBinder(
      cancel_interface_name,
      base::BindOnce(&TestReceiverCollector::BindCancelInterface,
                     base::Unretained(&collector_),
                     cancel_receiver.As<mojom::TestInterfaceForCancel>()));
  EXPECT_TRUE(collector_.IsGrantReceiverBound());
  EXPECT_TRUE(collector_.IsDeferReceiverBound());
  EXPECT_TRUE(collector_.IsCancelReceiverBound());
  EXPECT_FALSE(collector_.IsCanceled());
}

}  // namespace

}  // namespace content
