// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/tests/feature_unittest.test-mojom-features.h"
#include "mojo/public/cpp/bindings/tests/feature_unittest.test-mojom-forward.h"
#include "mojo/public/cpp/bindings/tests/feature_unittest.test-mojom-shared.h"
#include "mojo/public/cpp/bindings/tests/feature_unittest.test-mojom.h"

#include <utility>

#include "base/dcheck_is_on.h"
#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/generic_pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/service_factory.h"
#include "mojo/public/cpp/bindings/shared_associated_remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "mojo/public/cpp/bindings/tests/bindings_test_base.h"
#include "mojo/public/cpp/system/functions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo::test::feature_unittest {

namespace {

//// Mojom impls ////

class DefaultDeniedImpl : public mojom::DefaultDenied {
 public:
  explicit DefaultDeniedImpl(
      mojo::PendingReceiver<mojom::DefaultDenied> receiver)
      : receiver_(this, std::move(receiver)) {}

  DefaultDeniedImpl() : receiver_(this) {}
  ~DefaultDeniedImpl() override = default;

  DefaultDeniedImpl(const DefaultDeniedImpl&) = delete;
  DefaultDeniedImpl& operator=(const DefaultDeniedImpl&) = delete;

  // mojom::DefaultDenied overrides.
  void GetInt(GetIntCallback callback) override { std::move(callback).Run(1); }

  mojo::Receiver<mojom::DefaultDenied>& receiver() { return receiver_; }

  // Helper for ServiceFactory test.
  static auto RunService(PendingReceiver<mojom::DefaultDenied> receiver) {
    return std::make_unique<DefaultDeniedImpl>(std::move(receiver));
  }

 private:
  mojo::Receiver<mojom::DefaultDenied> receiver_;
};

class OtherDeniedImpl : public mojom::OtherDenied {
 public:
  explicit OtherDeniedImpl(mojo::PendingReceiver<mojom::OtherDenied> receiver)
      : receiver_(this, std::move(receiver)) {}

  OtherDeniedImpl() : receiver_(this) {}
  ~OtherDeniedImpl() override = default;

  OtherDeniedImpl(const OtherDeniedImpl&) = delete;
  OtherDeniedImpl& operator=(const OtherDeniedImpl&) = delete;

  // mojom::OtherDenied overrides.
  void GetInt(GetIntCallback callback) override { std::move(callback).Run(1); }

  mojo::Receiver<mojom::OtherDenied>& receiver() { return receiver_; }

 private:
  mojo::Receiver<mojom::OtherDenied> receiver_;
};

class OtherAllowedImpl : public mojom::OtherAllowed {
 public:
  explicit OtherAllowedImpl(mojo::PendingReceiver<mojom::OtherAllowed> receiver)
      : receiver_(this, std::move(receiver)) {}

  OtherAllowedImpl() : receiver_(this) {}
  ~OtherAllowedImpl() override = default;

  OtherAllowedImpl(const OtherAllowedImpl&) = delete;
  OtherAllowedImpl& operator=(const OtherAllowedImpl&) = delete;

  // mojom::OtherAllowed overrides.
  void GetInt(GetIntCallback callback) override { std::move(callback).Run(1); }

  mojo::Receiver<mojom::OtherAllowed>& receiver() { return receiver_; }

 private:
  mojo::Receiver<mojom::OtherAllowed> receiver_;
};

class DefaultDeniedSelfOwnedImpl : public mojom::DefaultDenied {
 public:
  DefaultDeniedSelfOwnedImpl() = default;
  ~DefaultDeniedSelfOwnedImpl() override = default;

  DefaultDeniedSelfOwnedImpl(const DefaultDeniedSelfOwnedImpl&) = delete;
  DefaultDeniedSelfOwnedImpl& operator=(const DefaultDeniedSelfOwnedImpl&) =
      delete;

  // mojom::DefaultDenied overrides.
  void GetInt(GetIntCallback callback) override { std::move(callback).Run(1); }
};

class DefaultAllowedSelfOwnedImpl : public mojom::DefaultAllowed {
 public:
  DefaultAllowedSelfOwnedImpl() = default;
  ~DefaultAllowedSelfOwnedImpl() override = default;

  DefaultAllowedSelfOwnedImpl(const DefaultAllowedSelfOwnedImpl&) = delete;
  DefaultAllowedSelfOwnedImpl& operator=(const DefaultAllowedSelfOwnedImpl&) =
      delete;

  // mojom::DefaultAllowed overrides.
  void GetInt(GetIntCallback callback) override { std::move(callback).Run(1); }
};

class DefaultAllowedImpl : mojom::DefaultAllowed {
 public:
  explicit DefaultAllowedImpl(
      mojo::PendingReceiver<mojom::DefaultAllowed> receiver)
      : receiver_(this, std::move(receiver)) {}

  ~DefaultAllowedImpl() override = default;

  DefaultAllowedImpl(const DefaultAllowedImpl&) = delete;
  DefaultAllowedImpl& operator=(const DefaultAllowedImpl&) = delete;

  // mojom::DefaultAllowed overrides.
  void GetInt(GetIntCallback callback) override { std::move(callback).Run(1); }

  mojo::Receiver<mojom::DefaultAllowed>& receiver() { return receiver_; }

  // Helper for ServiceFactory test.
  static auto RunService(PendingReceiver<mojom::DefaultAllowed> receiver) {
    return std::make_unique<DefaultAllowedImpl>(std::move(receiver));
  }

 private:
  mojo::Receiver<mojom::DefaultAllowed> receiver_;
};

class FeaturesOnMethodsImpl : public mojom::FeaturesOnMethods {
 public:
  explicit FeaturesOnMethodsImpl(
      mojo::PendingReceiver<mojom::FeaturesOnMethods> receiver)
      : receiver_(this, std::move(receiver)), assoc_receiver_(nullptr) {}
  explicit FeaturesOnMethodsImpl(
      mojo::PendingAssociatedReceiver<mojom::FeaturesOnMethods> receiver)
      : receiver_(nullptr), assoc_receiver_(this, std::move(receiver)) {}

  ~FeaturesOnMethodsImpl() override = default;

  FeaturesOnMethodsImpl(const FeaturesOnMethodsImpl&) = delete;
  FeaturesOnMethodsImpl& operator=(const FeaturesOnMethodsImpl&) = delete;

  // mojom::FeaturesOnMethods overrides.
  void DefaultDenied(DefaultDeniedCallback callback) override {
    std::move(callback).Run(1);
  }
  void DefaultDeniedSync(DefaultDeniedCallback callback) override {
    std::move(callback).Run(1);
  }
  void DefaultAllowed(DefaultAllowedCallback callback) override {
    std::move(callback).Run(1);
  }
  void Normal(NormalCallback callback) override { std::move(callback).Run(1); }

 private:
  mojo::Receiver<mojom::FeaturesOnMethods> receiver_;
  mojo::AssociatedReceiver<mojom::FeaturesOnMethods> assoc_receiver_;
};

class PassesInterfacesImpl : public mojom::PassesInterfaces {
 public:
  explicit PassesInterfacesImpl(
      mojo::PendingReceiver<mojom::PassesInterfaces> receiver)
      : receiver_(this, std::move(receiver)) {}

  ~PassesInterfacesImpl() override = default;

  PassesInterfacesImpl(const PassesInterfacesImpl&) = delete;
  PassesInterfacesImpl& operator=(const PassesInterfacesImpl&) = delete;

  // Callback to service responses from a mojom::DefaultAllowed remote.
  void OnGetInt(PassPendingOptionalRemoteAllowedCallback callback, int val) {
    std::move(callback).Run(val == 1);
  }

  // mojom::PassesInterfaces overrides.
  void PassPendingOptionalRemoteAllowed(
      mojo::PendingRemote<mojom::DefaultAllowed> iface,
      PassPendingOptionalRemoteAllowedCallback callback) override {
    allowed_remote_.Bind(std::move(iface));
    ASSERT_TRUE(allowed_remote_);
    allowed_remote_->GetInt(base::BindOnce(&PassesInterfacesImpl::OnGetInt,
                                           base::Unretained(this),
                                           std::move(callback)));
  }

  void PassPendingOptionalRemoteDisabled(
      mojo::PendingRemote<mojom::DefaultDenied> iface,
      PassPendingOptionalRemoteDisabledCallback callback) override {
    // As disabled interfaces are serialized as if an unbound pipe was
    // passed this method is called but it does not receive a pipe to bind.
    ASSERT_FALSE(iface.is_valid());
    std::move(callback).Run(true);
  }

  void PassPendingOptionalReceiverAllowed(
      mojo::PendingReceiver<mojom::DefaultAllowed> iface) override {
    allowed_impl_ = std::make_unique<DefaultAllowedImpl>(std::move(iface));
    ASSERT_TRUE(allowed_impl_->receiver().is_bound());
  }

  void PassPendingOptionalReceiverDisabled(
      mojo::PendingReceiver<mojom::DefaultDenied> iface,
      PassPendingOptionalReceiverDisabledCallback callback) override {
    denied_impl_ = std::make_unique<DefaultDeniedImpl>(std::move(iface));
    ASSERT_FALSE(denied_impl_->receiver().is_bound());
    std::move(callback).Run(true);
  }

 private:
  mojo::Receiver<mojom::PassesInterfaces> receiver_;

  mojo::Remote<mojom::DefaultAllowed> allowed_remote_;
  mojo::Remote<mojom::DefaultDenied> denied_remote_;
  std::unique_ptr<DefaultAllowedImpl> allowed_impl_;
  std::unique_ptr<DefaultDeniedImpl> denied_impl_;
};

}  // namespace

//// Test runners ////

class FeatureBindingsTest : public BindingsTestBase {
 public:
  void SetUp() override {
    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
        &FeatureBindingsTest::OnProcessError, base::Unretained(this)));
  }

  void TearDown() override {
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  void SetErrorHandler(base::OnceClosure handler) {
    error_handler_ = std::move(handler);
  }

 private:
  void OnProcessError(const std::string& error) {
    if (error_handler_) {
      std::move(error_handler_).Run();
    }
  }

  base::OnceClosure error_handler_;
};

//// Tests /////

TEST(FeatureTest, FeatureBasics) {
  EXPECT_TRUE(base::FeatureList::IsEnabled(
      mojo::test::feature_unittest::mojom::TestFeatureOn));
  EXPECT_FALSE(base::FeatureList::IsEnabled(
      mojo::test::feature_unittest::mojom::TestFeatureOff));
}

TEST(FeatureTest, ScopedFeatures) {
  base::test::ScopedFeatureList feature_list1;
  // --enable-features=TestFeatureOff --disable-features=TestFeatureOn.
  feature_list1.InitFromCommandLine("TestFeatureOff", "TestFeatureOn");

  // Check state is affected by the command line.
  EXPECT_FALSE(base::FeatureList::IsEnabled(
      mojo::test::feature_unittest::mojom::TestFeatureOn));
  EXPECT_TRUE(base::FeatureList::IsEnabled(
      mojo::test::feature_unittest::mojom::TestFeatureOff));
}

//// Tests that enabled features allow normal behavior. ////

TEST_P(FeatureBindingsTest, DefaultAllowed) {
  // Validate that allowed interfaces on `DefaultAllowed` can be called.
  bool error = false;
  SetErrorHandler(base::BindLambdaForTesting([&] { error = true; }));

  Remote<mojom::DefaultAllowed> remote;
  DefaultAllowedImpl impl(remote.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(remote);

  base::RunLoop run_loop;
  remote->GetInt(base::BindLambdaForTesting([&](int) { run_loop.Quit(); }));
  run_loop.Run();
  EXPECT_FALSE(error);
}

TEST_P(FeatureBindingsTest, OtherAllowed) {
  // Validate that allowed interfaces on `OtherAllowed` can be called. This
  // interface imports its features from another file.
  bool error = false;
  SetErrorHandler(base::BindLambdaForTesting([&] { error = true; }));

  Remote<mojom::OtherAllowed> remote;
  OtherAllowedImpl impl(remote.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(remote);

  base::RunLoop run_loop;
  remote->GetInt(base::BindLambdaForTesting([&](int) { run_loop.Quit(); }));
  run_loop.Run();
  EXPECT_FALSE(error);
}

TEST_P(FeatureBindingsTest, FeaturesOnMethodsAllowed) {
  // Validate that allowed interfaces on `FeaturesOnMethods` can be called.
  bool error = false;
  SetErrorHandler(base::BindLambdaForTesting([&] { error = true; }));

  Remote<mojom::FeaturesOnMethods> remote;
  EXPECT_FALSE(remote);
  FeaturesOnMethodsImpl impl(remote.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(remote);

  base::RunLoop run_loop;
  remote->DefaultAllowed(
      base::BindLambdaForTesting([&](int) { run_loop.Quit(); }));
  run_loop.Run();

  base::RunLoop run_loop2;
  remote->Normal(base::BindLambdaForTesting([&](int) { run_loop2.Quit(); }));
  run_loop2.Run();
  EXPECT_FALSE(error);
}

TEST_P(FeatureBindingsTest, FeaturesOnReceiverDenied) {
  if (GetParam() == mojo::BindingsTestSerializationMode::kNeverSerialize) {
    return;
  }
  // Validate that disabled methods on `FeaturesOnMethods` cannot be called.
  bool called = false;
  bool called_disconnect = false;
  base::RunLoop run_loop;

  Remote<mojom::FeaturesOnMethods> remote;
  auto pending_receiver = remote.BindNewPipeAndPassReceiver();

  remote.set_disconnect_handler(base::BindLambdaForTesting([&] {
    called_disconnect = true;
    run_loop.Quit();
  }));

  {
    // Queue to the pending_receiver with the feature enabled.
    // --enable-features=TestFeatureOff.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitFromCommandLine("TestFeatureOff", "");
    remote->DefaultDenied(
        base::BindLambdaForTesting([&](int) { called = true; }));
  }
  // Receiver can now process messages and TestFeatureOff is now disabled.
  FeaturesOnMethodsImpl impl(std::move(pending_receiver));
  run_loop.Run();

  EXPECT_FALSE(called);
  EXPECT_TRUE(called_disconnect);
}

TEST_P(FeatureBindingsTest, PassesInterfacesAllowed) {
  // Validate that feature-protected interfaces can be passed over mojo methods.
  bool error = false;
  SetErrorHandler(base::BindLambdaForTesting([&] { error = true; }));

  Remote<mojom::PassesInterfaces> remote;
  EXPECT_FALSE(remote);
  PassesInterfacesImpl impl(remote.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(remote);

  {
    // Can call methods via a passed allowed pending remote.
    base::RunLoop run_loop;
    MessagePipe pipe;
    PendingRemote<mojom::DefaultAllowed> allowed_pending_remote(
        std::move(pipe.handle0), 0);
    PendingReceiver<mojom::DefaultAllowed> allowed_pending_receiver(
        std::move(pipe.handle1));
    bool cb_val = false;
    remote->PassPendingOptionalRemoteAllowed(
        std::move(allowed_pending_remote),
        base::BindLambdaForTesting([&](bool val) {
          cb_val = val;
          run_loop.Quit();
        }));
    DefaultAllowedImpl allowed_impl(std::move(allowed_pending_receiver));
    run_loop.Run();
    ASSERT_TRUE(cb_val);
  }

  {
    // Can call methods via a passed allowed pending receiver.
    base::RunLoop run_loop;
    MessagePipe pipe;
    PendingRemote<mojom::DefaultAllowed> allowed_pending_remote(
        std::move(pipe.handle0), 0);
    PendingReceiver<mojom::DefaultAllowed> allowed_pending_receiver(
        std::move(pipe.handle1));
    bool cb_val = false;
    remote->PassPendingOptionalReceiverAllowed(
        std::move(allowed_pending_receiver));
    Remote<mojom::DefaultAllowed> allowed(std::move(allowed_pending_remote));
    allowed->GetInt(base::BindLambdaForTesting([&](int val) {
      cb_val = val == 1;
      run_loop.Quit();
    }));
    run_loop.Run();
    ASSERT_TRUE(cb_val);
  }
}

TEST_P(FeatureBindingsTest, SetsAllowed) {
  // Features intervene at the set.Add() stage so validate that these work.
  Remote<mojom::DefaultAllowed> remote_a;
  Remote<mojom::DefaultAllowed> remote_b;
  DefaultAllowedImpl impl_a(remote_a.BindNewPipeAndPassReceiver());
  DefaultAllowedImpl impl_b(remote_b.BindNewPipeAndPassReceiver());

  PendingRemote<mojom::DefaultAllowed> pending_remote_c;
  auto pending_receiver_c = pending_remote_c.InitWithNewPipeAndPassReceiver();
  PendingRemote<mojom::DefaultAllowed> pending_remote_d;
  auto pending_receiver_d = pending_remote_d.InitWithNewPipeAndPassReceiver();
  auto impl_c = std::make_unique<DefaultAllowedSelfOwnedImpl>();
  auto impl_d = std::make_unique<DefaultAllowedSelfOwnedImpl>();

  ReceiverSet<mojom::DefaultAllowed> receiver_set;
  // Every .Add() variant routes to AddImpl in an internal class.
  receiver_set.Add(impl_c.get(), std::move(pending_receiver_c));
  receiver_set.Add(impl_d.get(), std::move(pending_receiver_d));

  RemoteSet<mojom::DefaultAllowed> remote_set;
  remote_set.Add(std::move(remote_a));
  remote_set.Add(std::move(remote_b));
  remote_set.Add(std::move(pending_remote_c));
  remote_set.Add(std::move(pending_remote_d));
}

TEST_P(FeatureBindingsTest, ServiceFactory) {
  // Service factory quietly skips Adding disabled helpers.
  ServiceFactory services;
  MessagePipe pipe_allowed;
  MessagePipe pipe_denied;

  services.Add(DefaultAllowedImpl::RunService);
  services.Add(DefaultDeniedImpl::RunService);

  GenericPendingReceiver gpr_allowed(mojom::DefaultAllowed::Name_,
                                     std::move(pipe_allowed.handle0));
  GenericPendingReceiver gpr_denied(mojom::DefaultDenied::Name_,
                                    std::move(pipe_denied.handle0));

  EXPECT_TRUE(services.CanRunService(gpr_allowed));
  EXPECT_FALSE(services.CanRunService(gpr_denied));
}

////
// Test that disabled features prevent binding or sending messages.
//
// These tests DCHECK pretty early (in the same helper that inspects feature
// state) so we cannot meaningfully run them in DCHECK builds.
////
#if !DCHECK_IS_ON()

//// mojo::(Associated)?Remote<> ////

TEST_P(FeatureBindingsTest, InertPendingRemoteWontInit) {
  // Inert pipes still cannot be initialized.
  PendingRemote<mojom::DefaultDenied> pending_remote;
  EXPECT_FALSE(pending_remote);
  PendingReceiver<mojom::DefaultDenied> pending_receiver =
      pending_remote.InitWithNewPipeAndPassReceiver();
  EXPECT_FALSE(pending_receiver);
  EXPECT_FALSE(pending_remote);
}

TEST_P(FeatureBindingsTest, InertPendingReceiverWontInit) {
  PendingReceiver<mojom::DefaultDenied> pending_receiver;
  EXPECT_FALSE(pending_receiver);
  PendingRemote<mojom::DefaultDenied> pending_remote =
      pending_receiver.InitWithNewPipeAndPassRemote();
  EXPECT_FALSE(pending_receiver);
  EXPECT_FALSE(pending_remote);
}

TEST_P(FeatureBindingsTest, InertPendingAssociatedRemoteWontInit) {
  // Inert pipes still cannot be initialized.
  PendingAssociatedRemote<mojom::DefaultDenied> pending_remote;
  EXPECT_FALSE(pending_remote);
  PendingAssociatedReceiver<mojom::DefaultDenied> pending_receiver =
      pending_remote.InitWithNewEndpointAndPassReceiver();
  EXPECT_FALSE(pending_receiver);
  EXPECT_FALSE(pending_remote);
}

TEST_P(FeatureBindingsTest, InertPendingAssociatedReceiverWontInit) {
  PendingAssociatedReceiver<mojom::DefaultDenied> pending_receiver;
  EXPECT_FALSE(pending_receiver);
  PendingAssociatedRemote<mojom::DefaultDenied> pending_remote =
      pending_receiver.InitWithNewEndpointAndPassRemote();
  EXPECT_FALSE(pending_receiver);
  EXPECT_FALSE(pending_remote);
}

TEST_P(FeatureBindingsTest, RemoteWontBindNewPipe) {
  Remote<mojom::DefaultDenied> remote;
  EXPECT_FALSE(remote);
  PendingReceiver<mojom::DefaultDenied> pending_receiver =
      remote.BindNewPipeAndPassReceiver();
  EXPECT_FALSE(pending_receiver);
}

TEST_P(FeatureBindingsTest, RemoteWontBindPendingRemote) {
  MessagePipe pipe;
  PendingRemote<mojom::DefaultDenied> pending_remote(std::move(pipe.handle0),
                                                     0);
  Remote<mojom::DefaultDenied> remote;
  EXPECT_FALSE(remote);
  remote.Bind(std::move(pending_remote));
  EXPECT_FALSE(remote);
}

TEST_P(FeatureBindingsTest, AssociatedRemoteWontBind) {
  PendingAssociatedRemote<mojom::DefaultDenied> pa_remote;
  PendingAssociatedReceiver<mojom::DefaultDenied> pa_receiver;
  {
    base::test::ScopedFeatureList feature_list1;
    // --enable-features=TestFeatureOff.
    feature_list1.InitFromCommandLine("TestFeatureOff", "");
    pa_receiver = pa_remote.InitWithNewEndpointAndPassReceiver();
  }
  EXPECT_TRUE(pa_remote);
  AssociatedRemote<mojom::DefaultDenied> a_remote;
  a_remote.Bind(std::move(pa_remote));
  EXPECT_FALSE(a_remote);
}

TEST_P(FeatureBindingsTest, AssociatedRemoteWontBindNewEndpoint) {
  AssociatedRemote<mojom::DefaultDenied> a_remote;
  PendingAssociatedReceiver<mojom::DefaultDenied> pa_receiver =
      a_remote.BindNewEndpointAndPassReceiver();
  EXPECT_FALSE(a_remote);
  EXPECT_FALSE(pa_receiver);
}

TEST_P(FeatureBindingsTest, AssociatedRemoteWontBindDedicated) {
  AssociatedRemote<mojom::DefaultDenied> a_remote;
  PendingAssociatedReceiver<mojom::DefaultDenied> pa_receiver =
      a_remote.BindNewEndpointAndPassDedicatedReceiver();
  EXPECT_FALSE(a_remote);
  EXPECT_FALSE(pa_receiver);
}

//// mojo::(Associated)?Receiver<> ////

TEST_P(FeatureBindingsTest, ReceiverWontBindNewPipe) {
  DefaultDeniedImpl impl;
  PendingRemote<mojom::DefaultDenied> pending_remote =
      impl.receiver().BindNewPipeAndPassRemote();
  EXPECT_FALSE(pending_remote);
}

TEST_P(FeatureBindingsTest, ReceiverWontBindPendingReceiver) {
  MessagePipe pipe;
  PendingReceiver<mojom::DefaultDenied> pending_receiver(
      std::move(pipe.handle0));
  EXPECT_TRUE(pending_receiver);
  DefaultDeniedImpl impl(std::move(pending_receiver));
  EXPECT_FALSE(impl.receiver().is_bound());
}

TEST_P(FeatureBindingsTest, AssociatedReceiverWontBind) {
  PendingAssociatedRemote<mojom::DefaultDenied> pa_remote;
  PendingAssociatedReceiver<mojom::DefaultDenied> pa_receiver =
      pa_remote.InitWithNewEndpointAndPassReceiver();
  DefaultDeniedSelfOwnedImpl impl;
  AssociatedReceiver<mojom::DefaultDenied> a_receiver(&impl);
  a_receiver.Bind(std::move(pa_receiver), nullptr);
  EXPECT_FALSE(a_receiver.is_bound());
}

TEST_P(FeatureBindingsTest, AssociatedReceiverWontBindNewEndpoint) {
  DefaultDeniedSelfOwnedImpl impl;
  AssociatedReceiver<mojom::DefaultDenied> a_receiver(&impl);
  auto a_remote = a_receiver.BindNewEndpointAndPassRemote(nullptr);
  EXPECT_FALSE(a_receiver);
  EXPECT_FALSE(a_remote);
}

TEST_P(FeatureBindingsTest, AssociatedReceiverWontBindDedicated) {
  DefaultDeniedSelfOwnedImpl impl;
  AssociatedReceiver<mojom::DefaultDenied> a_receiver(&impl);
  auto a_remote = a_receiver.BindNewEndpointAndPassDedicatedRemote();
  EXPECT_FALSE(a_receiver);
  EXPECT_FALSE(a_remote);
}

////
// mojo::Selfowned(Associated)?Receiver bail out without binding the underlying
// receivers.
////

TEST_P(FeatureBindingsTest, SelfOwnedReceiver) {
  MessagePipe pipe;
  PendingReceiver<mojom::DefaultDenied> pending_receiver(
      std::move(pipe.handle0));
  auto impl = std::make_unique<DefaultDeniedSelfOwnedImpl>();
  auto weak_ref =
      MakeSelfOwnedReceiver(std::move(impl), std::move(pending_receiver));
  EXPECT_FALSE(weak_ref);
}

TEST_P(FeatureBindingsTest, SelfOwnedAssociatedReceiver) {
  PendingAssociatedRemote<mojom::DefaultDenied> pa_remote;
  PendingAssociatedReceiver<mojom::DefaultDenied> pa_receiver =
      pa_remote.InitWithNewEndpointAndPassReceiver();
  auto impl = std::make_unique<DefaultDeniedSelfOwnedImpl>();
  auto weak_ref =
      MakeSelfOwnedAssociatedReceiver(std::move(impl), std::move(pa_receiver));
  EXPECT_FALSE(weak_ref);
}

////
//  mojo::Shared* bail out appropriately.
////
TEST_P(FeatureBindingsTest, SharedRemoteWontBindNewPipe) {
  SharedRemote<mojom::DefaultDenied> s_remote;
  EXPECT_FALSE(s_remote);
  PendingReceiver<mojom::DefaultDenied> p_receiver =
      s_remote.BindNewPipeAndPassReceiver();
  EXPECT_FALSE(s_remote);
  EXPECT_FALSE(p_receiver);
}

TEST_P(FeatureBindingsTest, SharedRemoteWontBind) {
  PendingRemote<mojom::DefaultDenied> pa_remote;
  PendingReceiver<mojom::DefaultDenied> pa_receiver;
  {
    base::test::ScopedFeatureList feature_list1;
    // --enable-features=TestFeatureOff.
    feature_list1.InitFromCommandLine("TestFeatureOff", "");
    pa_receiver = pa_remote.InitWithNewPipeAndPassReceiver();
  }
  EXPECT_TRUE(pa_remote);
  SharedRemote<mojom::DefaultDenied> s_remote;
  s_remote.Bind(std::move(pa_remote), nullptr);
  EXPECT_FALSE(s_remote);
}

TEST_P(FeatureBindingsTest, SharedAssociatedRemoteWontBindNewEndpoint) {
  SharedAssociatedRemote<mojom::DefaultDenied> sa_remote;
  EXPECT_FALSE(sa_remote);
  PendingAssociatedReceiver<mojom::DefaultDenied> pa_receiver =
      sa_remote.BindNewEndpointAndPassReceiver();
  EXPECT_FALSE(sa_remote);
  EXPECT_FALSE(pa_receiver);
}

TEST_P(FeatureBindingsTest, SharedAssociatedRemoteWontBind) {
  PendingAssociatedRemote<mojom::DefaultDenied> pa_remote;
  PendingAssociatedReceiver<mojom::DefaultDenied> pa_receiver;
  {
    base::test::ScopedFeatureList feature_list1;
    // --enable-features=TestFeatureOff.
    feature_list1.InitFromCommandLine("TestFeatureOff", "");
    pa_receiver = pa_remote.InitWithNewEndpointAndPassReceiver();
  }
  EXPECT_TRUE(pa_remote);
  SharedAssociatedRemote<mojom::DefaultDenied> sa_remote;
  sa_remote.Bind(std::move(pa_remote), nullptr);
  EXPECT_FALSE(sa_remote);
}

////
//  Generic*Receiver As<disabled> won't "cast" but allowed interfaces can.
////
TEST_P(FeatureBindingsTest, GenericPendingReceivers) {
  MessagePipe pipe_a;
  ScopedInterfaceEndpointHandle end_a;
  ScopedInterfaceEndpointHandle end_b;
  ScopedInterfaceEndpointHandle::CreatePairPendingAssociation(&end_a, &end_b);

  GenericPendingReceiver gpr_allowed(mojom::DefaultAllowed::Name_,
                                     std::move(pipe_a.handle0));
  GenericPendingReceiver gpr_denied(mojom::DefaultDenied::Name_,
                                    std::move(pipe_a.handle1));
  GenericPendingAssociatedReceiver gpar_allowed(mojom::DefaultAllowed::Name_,
                                                std::move(end_a));
  GenericPendingAssociatedReceiver gpar_denied(mojom::DefaultDenied::Name_,
                                               std::move(end_b));

  auto p_allowed = gpr_allowed.As<mojom::DefaultAllowed>();
  auto p_denied = gpr_denied.As<mojom::DefaultDenied>();
  auto pa_allowed = gpar_allowed.As<mojom::DefaultAllowed>();
  auto pa_denied = gpar_denied.As<mojom::DefaultDenied>();

  EXPECT_TRUE(p_allowed);
  EXPECT_TRUE(pa_allowed);
  EXPECT_FALSE(p_denied);
  EXPECT_FALSE(pa_denied);
}

////
// Test that pending_* cannot be transmitted by methods. This prevents a
// process from receiving an endpoint from a compromised process. Optional
// remotes and receivers are treated as being empty.
////

TEST_P(FeatureBindingsTest, PassesOptionalInterfacesRemoteDenied) {
  if (GetParam() == BindingsTestSerializationMode::kNeverSerialize) {
    return;
  }
  Remote<mojom::PassesInterfaces> remote;
  PassesInterfacesImpl impl(remote.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  MessagePipe pipe;
  PendingRemote<mojom::DefaultDenied> denied_pending_remote(
      std::move(pipe.handle0), 0);

  bool error = false;
  SetErrorHandler(base::BindLambdaForTesting([&] {
    error = true;
    run_loop.Quit();
  }));

  bool called_empty = false;
  remote->PassPendingOptionalRemoteDisabled(
      std::move(denied_pending_remote),
      base::BindLambdaForTesting([&](bool val) {
        called_empty = val;
        run_loop.Quit();
      }));
  run_loop.Run();
  ASSERT_FALSE(error);
  ASSERT_TRUE(called_empty);
}

TEST_P(FeatureBindingsTest, PassesOptionalInterfacesReceiverDenied) {
  if (GetParam() == mojo::BindingsTestSerializationMode::kNeverSerialize) {
    return;
  }
  Remote<mojom::PassesInterfaces> remote;
  PassesInterfacesImpl impl(remote.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  MessagePipe pipe;
  PendingReceiver<mojom::DefaultDenied> denied_pending_receiver(
      std::move(pipe.handle1));

  bool error = false;
  SetErrorHandler(base::BindLambdaForTesting([&] {
    error = true;
    run_loop.Quit();
  }));

  bool called_empty = false;
  remote->PassPendingOptionalReceiverDisabled(
      std::move(denied_pending_receiver),
      base::BindLambdaForTesting([&](bool val) {
        called_empty = val;
        run_loop.Quit();
      }));
  run_loop.Run();
  ASSERT_FALSE(error);
  ASSERT_TRUE(called_empty);
}

////
// Tests that features from a different mojom file work (not exhaustive) - as
// features are implemented by templates this demonstrates that the generated
// headers are correctly imported in the main interface's files.
////

TEST_P(FeatureBindingsTest, RemoteWontBindNewPipeImported) {
  // Interface is runtime disabled - so cannot bind.
  Remote<mojom::OtherDenied> remote;
  PendingReceiver<mojom::OtherDenied> pending_receiver =
      remote.BindNewPipeAndPassReceiver();
  EXPECT_FALSE(pending_receiver);
}

TEST_P(FeatureBindingsTest, ReceiverWontBindPendingReceiverImported) {
  // Interface is runtime disabled - so cannot bind.
  MessagePipe pipe;
  PendingReceiver<mojom::OtherDenied> pending_receiver(std::move(pipe.handle0));
  OtherDeniedImpl impl(std::move(pending_receiver));
  EXPECT_FALSE(impl.receiver().is_bound());
}

// Validate that the various Sets return nullopt if a disabled interface is
// provided.

TEST_P(FeatureBindingsTest, ReceiverSetDenied) {
  MessagePipe pipe_a;
  PendingReceiver<mojom::DefaultDenied> pending_receiver_a(
      std::move(pipe_a.handle0));
  auto impl = std::make_unique<DefaultDeniedSelfOwnedImpl>();

  ReceiverSet<mojom::DefaultDenied> receiver_set;
  EXPECT_EQ(receiver_set.Add(impl.get(), std::move(pending_receiver_a)),
            std::nullopt);
}

TEST_P(FeatureBindingsTest, RemoteSetDenied) {
  PendingRemote<mojom::DefaultDenied> pending_remote;
  auto pending_receiver = pending_remote.InitWithNewPipeAndPassReceiver();

  RemoteSet<mojom::DefaultDenied> remote_set;
  // Do not test the non-pending case as disabled remote<> cannot be bound.
  EXPECT_EQ(remote_set.Add(std::move(pending_remote)), std::nullopt);
}

#endif  // !DCHECK_IS_ON()

////
//  Death tests - these are flaky on Android.
////
#if defined(GTEST_HAS_DEATH_TEST) && !BUILDFLAG(IS_ANDROID)
using FeatureBindingsDeathTest = FeatureBindingsTest;

TEST_P(FeatureBindingsDeathTest, MethodsOnRemoteDenied) {
  // Validate that disabled methods on `FeaturesOnMethods` cannot be called.
  bool called = false;
  Remote<mojom::FeaturesOnMethods> remote;
  FeaturesOnMethodsImpl impl(remote.BindNewPipeAndPassReceiver());
  EXPECT_DEATH(remote->DefaultDenied(
                   base::BindLambdaForTesting([&](int) { called = true; })),
               "");
  EXPECT_FALSE(called);
}

TEST_P(FeatureBindingsDeathTest, MethodsOnRemoteDeniedSync) {
  // Validate that disabled sync methods on remotes cannot be called.
  int result = 0;
  Remote<mojom::FeaturesOnMethods> remote;
  FeaturesOnMethodsImpl impl(remote.BindNewPipeAndPassReceiver());
  EXPECT_DEATH(remote->DefaultDeniedSync(&result), "");
  EXPECT_EQ(result, 0);
}

TEST_P(FeatureBindingsDeathTest, MethodsOnAssociatedRemoteDenied) {
  bool called = false;
  // Validate that disabled methods on associated remotes cannot be called.
  AssociatedRemote<mojom::FeaturesOnMethods> remote;
  FeaturesOnMethodsImpl impl(remote.BindNewEndpointAndPassDedicatedReceiver());
  EXPECT_DEATH(remote->DefaultDenied(
                   base::BindLambdaForTesting([&](int) { called = true; })),
               "");
  EXPECT_FALSE(called);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    FeatureBindingsDeathTest,
    testing::Values(mojo::BindingsTestSerializationMode::kNeverSerialize));

#endif  // defined(GTEST_HAS_DEATH_TEST) && !BUILDFLAG(IS_ANDROID)

INSTANTIATE_TEST_SUITE_P(
    ,
    FeatureBindingsTest,
    testing::Values(
        mojo::BindingsTestSerializationMode::kSerializeBeforeSend,
        mojo::BindingsTestSerializationMode::kSerializeBeforeDispatch,
        mojo::BindingsTestSerializationMode::kNeverSerialize));

}  // namespace mojo::test::feature_unittest
