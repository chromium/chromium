// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo_binder_policy_applier.h"

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "content/browser/mojo_binder_policy_map_impl.h"
#include "content/public/test/mojo_capability_control_test_interfaces.mojom.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// A test class that implements test interfaces and provides verification
// methods.
class TestReceiverCollector : public mojom::TestInterfaceForDefer,
                              public mojom::TestInterfaceForGrant,
                              public mojom::TestInterfaceForCancel,
                              public mojom::TestInterfaceForUnexpected {
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

  void BindUnexpectedInterface(
      mojo::PendingReceiver<mojom::TestInterfaceForUnexpected> receiver) {
    ASSERT_FALSE(unexpected_receiver_.is_bound());
    unexpected_receiver_.Bind(std::move(receiver));
  }

  // mojom::TestInterfaceForDefer implementation.
  void Ping(PingCallback callback) override { NOTREACHED_IN_MIGRATION(); }

  // Will be called when MojoBinderPolicyApplier::ApplyPolicyToBinder()
  // handles a kCancel binding request.
  void Cancel(const std::string& interface_name) {
    is_cancelled_ = true;
    cancelled_interface_ = interface_name;
  }

  // Used to check if the cancel_closure of MojoBinderPolicyApplier was
  // executed.
  bool IsCancelled() { return is_cancelled_; }

  const std::string& cancelled_interface() const {
    return cancelled_interface_;
  }

  bool IsDeferReceiverBound() const { return defer_receiver_.is_bound(); }

  bool IsGrantReceiverBound() const { return grant_receiver_.is_bound(); }

  bool IsCancelReceiverBound() const { return cancel_receiver_.is_bound(); }

  bool IsUnexpectedReceiverBound() const {
    return unexpected_receiver_.is_bound();
  }

 private:
  mojo::Receiver<mojom::TestInterfaceForDefer> defer_receiver_{this};
  mojo::Receiver<mojom::TestInterfaceForGrant> grant_receiver_{this};
  mojo::Receiver<mojom::TestInterfaceForCancel> cancel_receiver_{this};
  mojo::Receiver<mojom::TestInterfaceForUnexpected> unexpected_receiver_{this};
  bool is_cancelled_ = false;
  std::string cancelled_interface_;
};

class MojoBinderPolicyApplierTest : public testing::Test,
                                    mojom::MojoContextProvider {
 public:
  MojoBinderPolicyApplierTest() = default;

  // mojom::MojoContextProvider
  void GrantAll() override { policy_applier_.GrantAll(); }

 protected:
  std::vector<base::OnceClosure>& deferred_binders() {
    return policy_applier_.deferred_binders_;
  }

  // Calls MojoBinderPolicyApplier::GrantAll() inside a Mojo message dispatch
  // stack.
  void RunGrantAll() {
    DCHECK(!receiver_.is_bound());
    receiver_.Bind(remote_.BindNewPipeAndPassReceiver());
    remote_->GrantAll();
    remote_.FlushForTesting();
  }

  const MojoBinderPolicyMapImpl policy_map_{
      {{"content.mojom.TestInterfaceForDefer",
        MojoBinderNonAssociatedPolicy::kDefer},
       {"content.mojom.TestInterfaceForGrant",
        MojoBinderNonAssociatedPolicy::kGrant},
       {"content.mojom.TestInterfaceForCancel",
        MojoBinderNonAssociatedPolicy::kCancel},
       {"content.mojom.TestInterfaceForUnexpected",
        MojoBinderNonAssociatedPolicy::kUnexpected}}};
  TestReceiverCollector collector_{};
  MojoBinderPolicyApplier policy_applier_{
      &policy_map_, base::BindOnce(&TestReceiverCollector::Cancel,
                                   base::Unretained(&collector_))};

 private:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::MojoContextProvider> remote_;
  mojo::Receiver<mojom::MojoContextProvider> receiver_{this};
};

// Verifies that interfaces whose policies are kGrant can be bound immediately.
TEST_F(MojoBinderPolicyApplierTest, GrantInEnforce) {
  // Initialize Mojo interfaces.
  mojo::Remote<mojom::TestInterfaceForGrant> grant_remote;
  mojo::GenericPendingReceiver grant_receiver(
      grant_remote.BindNewPipeAndPassReceiver());

  // Bind the interface immediately if the policy is kGrant.
  const std::string interface_name = grant_receiver.interface_name().value();
  EXPECT_FALSE(collector_.IsCancelled());
  EXPECT_FALSE(collector_.IsGrantReceiverBound());
  policy_applier_.ApplyPolicyToNonAssociatedBinder(
      interface_name,
      base::BindOnce(&TestReceiverCollector::BindGrantInterface,
                     base::Unretained(&collector_),
                     grant_receiver.As<mojom::TestInterfaceForGrant>()));
  EXPECT_TRUE(collector_.IsGrantReceiverBound());
  EXPECT_FALSE(collector_.IsCancelled());
}

// Verifies that interfaces whose policies are kDefer cannot be bound until
// GrantAll() is called.
TEST_F(MojoBinderPolicyApplierTest, DeferInEnforce) {
  // Initialize Mojo interfaces.
  mojo::Remote<mojom::TestInterfaceForDefer> defer_remote;
  mojo::GenericPendingReceiver defer_receiver(
      defer_remote.BindNewPipeAndPassReceiver());

  // Delay binding the interface until GrantAll() is called.
  const std::string interface_name = defer_receiver.interface_name().value();
  EXPECT_FALSE(collector_.IsCancelled());
  policy_applier_.ApplyPolicyToNonAssociatedBinder(
      interface_name,
      base::BindOnce(&TestReceiverCollector::BindDeferInterface,
                     base::Unretained(&collector_),
                     defer_receiver.As<mojom::TestInterfaceForDefer>()));
  EXPECT_FALSE(collector_.IsDeferReceiverBound());
  EXPECT_EQ(1U, deferred_binders().size());

  RunGrantAll();
  EXPECT_EQ(0U, deferred_binders().size());
  EXPECT_TRUE(collector_.IsDeferReceiverBound());
  EXPECT_FALSE(collector_.IsCancelled());
}

// Verifies that MojoBinderPolicyApplier will run `cancel_closure` when running
// in the kEnforce mode and receiving an interface whose policy is kCancel,
TEST_F(MojoBinderPolicyApplierTest, CancelInEnforce) {
  // Initialize Mojo interfaces.
  mojo::Remote<mojom::TestInterfaceForCancel> cancel_remote;
  mojo::GenericPendingReceiver cancel_receiver(
      cancel_remote.BindNewPipeAndPassReceiver());

  const std::string interface_name = cancel_receiver.interface_name().value();
  EXPECT_FALSE(collector_.IsCancelled());
  EXPECT_FALSE(collector_.IsCancelReceiverBound());
  policy_applier_.ApplyPolicyToNonAssociatedBinder(
      interface_name,
      base::BindOnce(&TestReceiverCollector::BindCancelInterface,
                     base::Unretained(&collector_),
                     cancel_receiver.As<mojom::TestInterfaceForCancel>()));
  EXPECT_TRUE(collector_.IsCancelled());
  EXPECT_EQ(collector_.cancelled_interface(),
            "content.mojom.TestInterfaceForCancel");
  EXPECT_FALSE(collector_.IsCancelReceiverBound());
}

// When MojoBinderPolicyApplier runs in the kPrepareToGrantAll mode, verifies it
// applies kGrant for kGrant interfaces.
TEST_F(MojoBinderPolicyApplierTest, GrantInPrepareToGrantAll) {
  // Initialize Mojo interfaces.
  mojo::Remote<mojom::TestInterfaceForGrant> grant_remote;
  mojo::GenericPendingReceiver grant_receiver(
      grant_remote.BindNewPipeAndPassReceiver());

  policy_applier_.PrepareToGrantAll();
  const std::string grant_interface_name =
      grant_receiver.interface_name().value();
  policy_applier_.ApplyPolicyToNonAssociatedBinder(
      grant_interface_name,
      base::BindOnce(&TestReceiverCollector::BindGrantInterface,
                     base::Unretained(&collector_),
                     grant_receiver.As<mojom::TestInterfaceForGrant>()));
  EXPECT_TRUE(collector_.IsGrantReceiverBound());
}

// When MojoBinderPolicyApplier runs in the kPrepareToGrantAll mode, verifies it
// applies kDefer for kDefer interfaces.
TEST_F(MojoBinderPolicyApplierTest, DeferInPrepareToGrantAll) {
  // Initialize Mojo interfaces.
  mojo::Remote<mojom::TestInterfaceForDefer> defer_remote;
  mojo::GenericPendingReceiver defer_receiver(
      defer_remote.BindNewPipeAndPassReceiver());

  policy_applier_.PrepareToGrantAll();
  const std::string defer_interface_name =
      defer_receiver.interface_name().value();
  policy_applier_.ApplyPolicyToNonAssociatedBinder(
      defer_interface_name,
      base::BindOnce(&TestReceiverCollector::BindDeferInterface,
                     base::Unretained(&collector_),
                     defer_receiver.As<mojom::TestInterfaceForDefer>()));
  EXPECT_FALSE(collector_.IsDeferReceiverBound());
  EXPECT_EQ(1U, deferred_binders().size());

  RunGrantAll();
  EXPECT_TRUE(collector_.IsDeferReceiverBound());
  EXPECT_EQ(0U, deferred_binders().size());
}

// When MojoBinderPolicyApplier runs in the kPrepareToGrantAll mode, verifies it
// applies kGrant rather than kCancel policy when receiving a kCancel interface
// binding request.
TEST_F(MojoBinderPolicyApplierTest, CancelInPrepareToGrantAll) {
  // Initialize Mojo interfaces.
  mojo::Remote<mojom::TestInterfaceForCancel> cancel_remote;
  mojo::GenericPendingReceiver cancel_receiver(
      cancel_remote.BindNewPipeAndPassReceiver());

  policy_applier_.PrepareToGrantAll();
  const std::string cancel_interface_name =
      cancel_receiver.interface_name().value();
  policy_applier_.ApplyPolicyToNonAssociatedBinder(
      cancel_interface_name,
      base::BindOnce(&TestReceiverCollector::BindCancelInterface,
                     base::Unretained(&collector_),
                     cancel_receiver.As<mojom::TestInterfaceForCancel>()));
  EXPECT_FALSE(collector_.IsCancelled());
  EXPECT_TRUE(collector_.IsCancelReceiverBound());
}

TEST_F(MojoBinderPolicyApplierTest, UnexpectedInPrepareToGrantAll) {
  // Initialize Mojo interfaces.
  mojo::Remote<mojom::TestInterfaceForUnexpected> unexpected_remote;
  mojo::GenericPendingReceiver unexpected_receiver(
      unexpected_remote.BindNewPipeAndPassReceiver());

  policy_applier_.PrepareToGrantAll();
  const std::string interface_name =
      unexpected_receiver.interface_name().value();
  policy_applier_.ApplyPolicyToNonAssociatedBinder(
      interface_name,
      base::BindOnce(
          &TestReceiverCollector::BindUnexpectedInterface,
          base::Unretained(&collector_),
          unexpected_receiver.As<mojom::TestInterfaceForUnexpected>()));
  EXPECT_FALSE(collector_.IsCancelled());
  EXPECT_TRUE(collector_.IsUnexpectedReceiverBound());
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
  mojo::Remote<mojom::TestInterfaceForUnexpected> unexpected_remote;
  mojo::GenericPendingReceiver unexpected_receiver(
      unexpected_remote.BindNewPipeAndPassReceiver());

  RunGrantAll();

  // All interfaces should be bound immediately.
  const std::string defer_interface_name =
      defer_receiver.interface_name().value();
  const std::string grant_interface_name =
      grant_receiver.interface_name().value();
  const std::string cancel_interface_name =
      cancel_receiver.interface_name().value();
  const std::string unexpected_interface_name =
      unexpected_receiver.interface_name().value();
  EXPECT_FALSE(collector_.IsCancelled());
  EXPECT_FALSE(collector_.IsGrantReceiverBound());
  EXPECT_FALSE(collector_.IsDeferReceiverBound());
  EXPECT_FALSE(collector_.IsCancelReceiverBound());
  EXPECT_FALSE(collector_.IsUnexpectedReceiverBound());
  policy_applier_.ApplyPolicyToNonAssociatedBinder(
      defer_interface_name,
      base::BindOnce(&TestReceiverCollector::BindDeferInterface,
                     base::Unretained(&collector_),
                     defer_receiver.As<mojom::TestInterfaceForDefer>()));
  policy_applier_.ApplyPolicyToNonAssociatedBinder(
      grant_interface_name,
      base::BindOnce(&TestReceiverCollector::BindGrantInterface,
                     base::Unretained(&collector_),
                     grant_receiver.As<mojom::TestInterfaceForGrant>()));
  policy_applier_.ApplyPolicyToNonAssociatedBinder(
      cancel_interface_name,
      base::BindOnce(&TestReceiverCollector::BindCancelInterface,
                     base::Unretained(&collector_),
                     cancel_receiver.As<mojom::TestInterfaceForCancel>()));
  policy_applier_.ApplyPolicyToNonAssociatedBinder(
      unexpected_interface_name,
      base::BindOnce(
          &TestReceiverCollector::BindUnexpectedInterface,
          base::Unretained(&collector_),
          unexpected_receiver.As<mojom::TestInterfaceForUnexpected>()));
  EXPECT_TRUE(collector_.IsGrantReceiverBound());
  EXPECT_TRUE(collector_.IsDeferReceiverBound());
  EXPECT_TRUE(collector_.IsCancelReceiverBound());
  EXPECT_TRUE(collector_.IsUnexpectedReceiverBound());
  EXPECT_FALSE(collector_.IsCancelled());
  EXPECT_EQ(0U, deferred_binders().size());
}

// Verifies that DropDeferredBinders() deletes all deferred binders.
TEST_F(MojoBinderPolicyApplierTest, DropDeferredBinders) {
  // Initialize Mojo interfaces.
  mojo::Remote<mojom::TestInterfaceForDefer> defer_remote;
  mojo::GenericPendingReceiver defer_receiver(
      defer_remote.BindNewPipeAndPassReceiver());

  const std::string interface_name = defer_receiver.interface_name().value();
  EXPECT_FALSE(collector_.IsCancelled());
  policy_applier_.ApplyPolicyToNonAssociatedBinder(
      interface_name,
      base::BindOnce(&TestReceiverCollector::BindDeferInterface,
                     base::Unretained(&collector_),
                     defer_receiver.As<mojom::TestInterfaceForDefer>()));
  EXPECT_FALSE(collector_.IsDeferReceiverBound());
  EXPECT_EQ(1U, deferred_binders().size());
  policy_applier_.DropDeferredBinders();
  EXPECT_EQ(0U, deferred_binders().size());
  RunGrantAll();
  EXPECT_FALSE(collector_.IsDeferReceiverBound());
}

}  // namespace content
